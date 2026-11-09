#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

constexpr int kSubBlockOffset[16] = {
    0, 64, 16, 80, 128, 192, 144, 208,
    32, 96, 48, 112, 160, 224, 176, 240,
};
constexpr int kCBPFixedLength[6] = {0, 2, 1, 2, 2, 0};
constexpr int kCBPOffset[6] = {0, 4, 2, 8, 12, 1};
constexpr int kCBPOutput[16] = {
    0, 15, 3, 12, 1, 2, 4, 8, 5, 6, 9, 10, 7, 11, 13, 14,
};
constexpr int kCBPExtended[4] = {6, 9, 10, 12};

// These tables were verified byte-for-byte at their retail image RVAs.
constexpr int kSignificantAbsLevelRemap[6] = {2, 3, 4, 6, 10, 14};
constexpr int kSignificantAbsLevelFixedLength[6] = {0, 0, 1, 2, 2, 2};
constexpr int kSignificantRunBin[16] = {
    -1, -1, -1, -1, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0, 0,
};
constexpr int kSignificantRunFixedLength[15] = {
    0, 0, 1, 1, 3, 0, 0, 1, 1, 2, 0, 0, 0, 0, 1,
};
constexpr int kSignificantRunRemap[15] = {
    1, 2, 3, 5, 7, 1, 2, 3, 5, 7, 1, 2, 3, 4, 5,
};

void EnsureBits(hdpBitIO_t* bitIO, unsigned count) {
  if (bitIO->bitCnt >= count)
    return;
  std::uint64_t buffer = bitIO->bitBuff;
  std::uint64_t available = bitIO->bitCnt;
  FillBitBuffer(bitIO, &buffer, &available);
  bitIO->bitBuff = buffer;
  bitIO->bitCnt = available;
}

std::uint64_t PeekBits(hdpBitIO_t* bitIO, unsigned count) {
  EnsureBits(bitIO, count);
  if (count == 0)
    return 0;
  const std::uint64_t mask = count == 64 ? ~0ULL : (1ULL << count) - 1;
  return (bitIO->bitBuff >> (bitIO->bitCnt - count)) & mask;
}

void FlushBits(hdpBitIO_t* bitIO, unsigned count) {
  EnsureBits(bitIO, count);
  bitIO->bitCnt -= count;
}

std::uint64_t ReadBits(hdpBitIO_t* bitIO, unsigned count) {
  const std::uint64_t value = PeekBits(bitIO, count);
  bitIO->bitCnt -= count;
  return value;
}

int ReadSignedBits(hdpBitIO_t* bitIO, unsigned magnitudeBits) {
  const int magnitude = static_cast<int>(ReadBits(bitIO, magnitudeBits));
  if (magnitude == 0)
    return 0;
  return ReadBits(bitIO, 1) != 0 ? -magnitude : magnitude;
}

int DecodeHuffman(const std::int16_t* table, hdpBitIO_t* bitIO) {
  int symbol = table[PeekBits(bitIO, 5)];
  if (symbol >= 0) {
    FlushBits(bitIO, static_cast<unsigned>(symbol & 7));
    return symbol >> 3;
  }

  FlushBits(bitIO, 5);
  do {
    symbol = table[0x8000 + symbol + ReadBits(bitIO, 1)];
  } while (symbol < 0);
  return symbol;
}

int DecodeShortHuffman(const std::int16_t* table, hdpBitIO_t* bitIO) {
  const int packed = table[PeekBits(bitIO, 5)];
  FlushBits(bitIO, static_cast<unsigned>(packed & 7));
  return packed >> 3;
}

void UpdateHuffman(hdpAdaptiveHuffman_t* huffman, int symbol,
                   bool secondDiscriminant) {
  huffman->discriminant += huffman->deltaTable[symbol];
  if (secondDiscriminant)
    huffman->discriminant1 += huffman->deltaTable1[symbol];
}

int DecodeSignificantAbsLevel(hdpAdaptiveHuffman_t* huffman,
                              hdpBitIO_t* bitIO) {
  const int index = DecodeHuffman(huffman->decTable, bitIO);
  UpdateHuffman(huffman, index, false);
  if (index < 2)
    return index + 2;
  if (index < 6) {
    const unsigned fixed =
        static_cast<unsigned>(kSignificantAbsLevelFixedLength[index]);
    return kSignificantAbsLevelRemap[index] +
           static_cast<int>(ReadBits(bitIO, fixed));
  }

  unsigned exponent = static_cast<unsigned>(ReadBits(bitIO, 4)) + 4;
  if (exponent == 19) {
    exponent += static_cast<unsigned>(ReadBits(bitIO, 2));
    if (exponent == 22)
      exponent += static_cast<unsigned>(ReadBits(bitIO, 3));
  }
  return static_cast<int>((1U << exponent) + 2U +
                          static_cast<std::uint32_t>(ReadBits(bitIO, exponent)));
}

int DecodeSignificantRun(int maxRun, hdpAdaptiveHuffman_t* huffman,
                         hdpBitIO_t* bitIO) {
  if (maxRun < 5) {
    if (maxRun == 1 || ReadBits(bitIO, 1) != 0)
      return 1;
    if (maxRun == 2 || ReadBits(bitIO, 1) != 0)
      return 2;
    if (maxRun == 3 || ReadBits(bitIO, 1) != 0)
      return 3;
    return 4;
  }

  const int index = DecodeShortHuffman(huffman->decTable, bitIO) +
                    5 * kSignificantRunBin[maxRun];
  return kSignificantRunRemap[index] +
         static_cast<int>(ReadBits(
             bitIO, static_cast<unsigned>(kSignificantRunFixedLength[index])));
}

int DecodeFirstIndex(hdpAdaptiveHuffman_t* huffman, hdpBitIO_t* bitIO) {
  const int index = DecodeHuffman(huffman->decTable, bitIO);
  UpdateHuffman(huffman, index, true);
  return index;
}

int DecodeFollowingIndex(int location, hdpAdaptiveHuffman_t* huffman,
                         hdpBitIO_t* bitIO) {
  if (location < 15) {
    const int index = DecodeShortHuffman(huffman->decTable, bitIO);
    UpdateHuffman(huffman, index, true);
    return index;
  }
  if (location == 15) {
    if (ReadBits(bitIO, 1) == 0)
      return 0;
    if (ReadBits(bitIO, 1) == 0)
      return 2;
    return 1 + 2 * static_cast<int>(ReadBits(bitIO, 1));
  }
  return static_cast<int>(ReadBits(bitIO, 1));
}

int DecodeCBPCode(hdpBitIO_t* bitIO, hdpAdaptiveHuffman_t* huffman,
                  bool updateModel) {
  int code = DecodeShortHuffman(huffman->decTable, bitIO);
  if (updateModel)
    UpdateHuffman(huffman, code, false);
  switch (code) {
    case 0:
      return 1 << ReadBits(bitIO, 2);
    case 1: {
      code = static_cast<int>(ReadBits(bitIO, 2));
      if (code == 0)
        return 3;
      if (code == 1)
        return 5;
      return kCBPExtended[2 * code + ReadBits(bitIO, 1) - 4];
    }
    case 2:
      return 15 ^ (1 << ReadBits(bitIO, 2));
    case 3:
      return 15;
    default:
      return code;
  }
}

void UpdateAdaptiveScan(hdpAdaptiveScan_t* scan, int location) {
  ++scan[location].scanTotal;
  if (location != 0 &&
      scan[location].scanTotal > scan[location - 1].scanTotal)
    std::swap(scan[location], scan[location - 1]);
}

bool IsYUV(hdpColorFormat_t format) {
  return format == CF_YUV_420 || format == CF_YUV_422 ||
         format == CF_YUV_444;
}

int DecodeDCValue(bool significant, int modelBits,
                  hdpAdaptiveHuffman_t* huffman, hdpBitIO_t* bitIO,
                  int* laplacianMean) {
  int value = 0;
  if (significant) {
    value = DecodeSignificantAbsLevel(huffman, bitIO) - 1;
    ++*laplacianMean;
  }
  if (modelBits != 0)
    value = (value << modelBits) |
            static_cast<int>(ReadBits(bitIO, modelBits));
  if (value != 0 && ReadBits(bitIO, 1) != 0)
    value = -value;
  return value;
}

}  // namespace

std::uint8_t DecodeQuantizerIndex(hdpBitIO_t* bitIO,
                                  std::uint8_t indexBitCount) {
  if (ReadBits(bitIO, 1) == 0)
    return 0;
  return static_cast<std::uint8_t>(ReadBits(bitIO, indexBitCount) + 1);
}

void DecodeHPCBP(hdpCodec_t* codec, hdpContext_t* context) {
  hdpBitIO_t* bitIO = context->bitIOHP;
  const hdpColorFormat_t format = codec->codecParms.colorFormat;
  const int planeCount =
      format == CF_N_CHANNEL || format == CF_CMYK
          ? static_cast<int>(codec->codecParms.numChannels)
          : 1;

  for (int plane = 0; plane < planeCount; ++plane) {
    int cbpY = 0;
    int cbpU = 0;
    int cbpV = 0;
    int groupMask = DecodeShortHuffman(context->adaptHuffCBP[1].decTable,
                                       bitIO);
    UpdateHuffman(&context->adaptHuffCBP[1], groupMask, false);
    switch (groupMask) {
      case 1:
        groupMask = 1 << ReadBits(bitIO, 2);
        break;
      case 2: {
        const int selector = static_cast<int>(ReadBits(bitIO, 2));
        if (selector == 0)
          groupMask = 3;
        else if (selector == 1)
          groupMask = 5;
        else
          groupMask = kCBPExtended[2 * selector + ReadBits(bitIO, 1) - 4];
        break;
      }
      case 3:
        groupMask = 15 ^ (1 << ReadBits(bitIO, 2));
        break;
      case 4:
        groupMask = 15;
        break;
      default:
        break;
    }

    for (int block = 0; block < 4; ++block) {
      if ((groupMask & (1 << block)) == 0)
        continue;
      int symbol = DecodeHuffman(context->adaptHuffCBP[0].decTable, bitIO);
      UpdateHuffman(&context->adaptHuffCBP[0], symbol, false);
      unsigned value = static_cast<unsigned>(symbol + 1);
      int blockCBP = 0;
      if (value >= 6) {
        if (ReadBits(bitIO, 1) != 0)
          blockCBP = 0x10;
        else if (ReadBits(bitIO, 1) != 0)
          blockCBP = 0x20;
        else
          blockCBP = 0x30;
        if (value == 9 && ReadBits(bitIO, 1) == 0)
          value = ReadBits(bitIO, 1) != 0 ? 10 : 11;
        value -= 6;
      }
      int code = kCBPOffset[value];
      code += static_cast<int>(ReadBits(
          bitIO, static_cast<unsigned>(kCBPFixedLength[value])));
      blockCBP += kCBPOutput[code];

      cbpY |= (blockCBP & 15) << (4 * block);
      if (format == CF_YUV_444) {
        if ((blockCBP & 0x10) != 0)
          cbpU |= DecodeCBPCode(bitIO, &context->adaptHuffCBP[2], false)
                  << (4 * block);
        if ((blockCBP & 0x20) != 0)
          cbpV |= DecodeCBPCode(bitIO, &context->adaptHuffCBP[2], false)
                  << (4 * block);
      } else if (format == CF_YUV_420) {
        cbpU |= ((blockCBP >> 4) & 1) << block;
        cbpV |= ((blockCBP >> 5) & 1) << block;
      } else if (format == CF_YUV_422) {
        constexpr int shift[4] = {0, 1, 4, 5};
        for (int chroma = 0; chroma < 2; ++chroma) {
          if ((blockCBP & (0x10 << chroma)) == 0)
            continue;
          int chromaCode = 5;
          if (ReadBits(bitIO, 1) != 0)
            chromaCode = 1;
          else if (ReadBits(bitIO, 1) != 0)
            chromaCode = 4;
          if (chroma == 0)
            cbpU |= chromaCode << shift[block];
          else
            cbpV |= chromaCode << shift[block];
        }
      }
    }

    codec->MBInfo.residualCBP[plane] = cbpY;
    if (IsYUV(format)) {
      codec->MBInfo.residualCBP[1] = cbpU;
      codec->MBInfo.residualCBP[2] = cbpV;
    }
  }
}

int DecodeLPCBP(hdpCodec_t* codec, hdpContext_t* context) {
  hdpBitIO_t* bitIO = context->bitIOLP;
  const int channelCount = static_cast<int>(codec->codecParms.numChannels);
  if (!IsYUV(codec->codecParms.colorFormat)) {
    int cbp = 0;
    for (int channel = 0; channel < channelCount; ++channel)
      cbp |= static_cast<int>(ReadBits(bitIO, 1)) << channel;
    return cbp;
  }

  int& maxCount = context->adaptCBPModelLP.maxCount;
  int& zeroCount = context->adaptCBPModelLP.zeroCount;
  const int maximum = 4 * channelCount - 5;
  int cbp = 0;
  if (zeroCount <= 0 || maxCount < 0) {
    if (ReadBits(bitIO, 1) != 0) {
      cbp = 1;
      const int suffix = static_cast<int>(ReadBits(bitIO, channelCount - 1));
      if (suffix != 0)
        cbp = 2 * suffix + static_cast<int>(ReadBits(bitIO, 1));
    }
    if (maxCount < zeroCount)
      cbp = maximum - cbp;
  } else {
    cbp = static_cast<int>(ReadBits(bitIO, channelCount));
  }
  maxCount = std::clamp(maxCount + 1 - 4 * (cbp == maximum), -8, 7);
  zeroCount = std::clamp(zeroCount + 1 - 4 * (cbp == 0), -8, 7);
  return cbp;
}

void DecodeBlockFlexBits(hdpBitIO_t* bitIO, int flexBitCount,
                         int trimFlexBits, std::int16_t* coefficients) {
  for (int index = 1; index < 16; ++index) {
    int coefficient = coefficients[index] >> trimFlexBits;
    if (coefficient > 0)
      coefficient += static_cast<int>(ReadBits(bitIO, flexBitCount));
    else if (coefficient < 0)
      coefficient -= static_cast<int>(ReadBits(bitIO, flexBitCount));
    else
      coefficient = ReadSignedBits(bitIO, flexBitCount);
    coefficients[index] =
        static_cast<std::int16_t>(coefficient << trimFlexBits);
  }
}

void DecodeBlockModelBits(hdpBitIO_t* bitIO, int modelBitCount,
                          std::int16_t* coefficients) {
  for (int index = 1; index < 16; ++index) {
    int coefficient = coefficients[index];
    if (coefficient > 0)
      coefficient = (coefficient << modelBitCount) +
                    static_cast<int>(ReadBits(bitIO, modelBitCount));
    else if (coefficient < 0)
      coefficient = (coefficient << modelBitCount) -
                    static_cast<int>(ReadBits(bitIO, modelBitCount));
    else
      coefficient = ReadSignedBits(bitIO, modelBitCount);
    coefficients[index] = static_cast<std::int16_t>(coefficient);
  }
}

hdpError_t DecodeMacroblockDC(hdpCodec_t* codec, hdpContext_t* context,
                              int, int) {
  const int channelCount = static_cast<int>(codec->codecParms.numChannels);
  const hdpColorFormat_t format = codec->codecParms.colorFormat;
  int laplacianMean[16]{};

  if (IsYUV(format)) {
    const int significance =
        DecodeShortHuffman(context->adaptHuffDC[0].decTable,
                           context->bitIODC);
    codec->MBInfo.compactDCLP[0][0] = static_cast<std::int16_t>(
        DecodeDCValue((significance & 4) != 0,
                      context->adaptCNModelDC.fixedLengthCodeBits[0],
                      &context->adaptHuffDC[1], context->bitIODC,
                      &laplacianMean[0]));
    codec->MBInfo.compactDCLP[1][0] = static_cast<std::int16_t>(
        DecodeDCValue((significance & 2) != 0,
                      context->adaptCNModelDC.fixedLengthCodeBits[1],
                      &context->adaptHuffDC[2], context->bitIODC,
                      &laplacianMean[1]));
    const int vModel =
        context->adaptCNModelDC.fixedLengthCodeBits[
            format == CF_YUV_444 ? 2 : 1];
    codec->MBInfo.compactDCLP[2][0] = static_cast<std::int16_t>(
        DecodeDCValue((significance & 1) != 0, vModel,
                      &context->adaptHuffDC[2], context->bitIODC,
                      &laplacianMean[format == CF_YUV_444 ? 2 : 1]));
  } else {
    for (int channel = 0; channel < channelCount; ++channel) {
      codec->MBInfo.compactDCLP[channel][0] = static_cast<std::int16_t>(
          DecodeDCValue(ReadBits(context->bitIODC, 1) != 0,
                        context->adaptCNModelDC.fixedLengthCodeBits[channel],
                        &context->adaptHuffDC[1], context->bitIODC,
                        &laplacianMean[channel]));
    }
  }

  UpdateAdaptiveCoefficientNormalization(format, channelCount, laplacianMean,
                                         &context->adaptCNModelDC);
  if (codec->adaptContextHuffman)
    return AdaptContextHuffmanDC(context);
  return HDP_ERROR_NONE;
}

hdpError_t DecodeQuantizerIndexLPHP(hdpCodec_t* codec,
                                    hdpContext_t* context) {
  codec->MBInfo.quantizerIndexLP = 0;
  codec->MBInfo.quantizerIndexHP = 0;
  hdpTileQuantization_t& tile =
      codec->tileQuantization[codec->currentTileColumn];
  if (codec->codecParms.streamLayout == SL_SPATIAL) {
    if (codec->codecParms.subBands < SB_NO_LOWPASS &&
        tile.numQuantizerIndexBitsLP != 0) {
      codec->MBInfo.quantizerIndexLP = DecodeQuantizerIndex(
          context->bitIODC, tile.numQuantizerIndexBitsLP);
    }
    if (codec->codecParms.subBands < SB_NO_HIGHPASS &&
        tile.numQuantizerIndexBitsHP != 0) {
      codec->MBInfo.quantizerIndexHP = DecodeQuantizerIndex(
          context->bitIODC, tile.numQuantizerIndexBitsHP);
    }
  }
  if (tile.numQuantizerIndexBitsHP == 0 && tile.numQuantizersHP > 1)
    codec->MBInfo.quantizerIndexHP = codec->MBInfo.quantizerIndexLP;
  if (codec->MBInfo.quantizerIndexLP >= tile.numQuantizersLP ||
      codec->MBInfo.quantizerIndexHP >= tile.numQuantizersHP)
    return HDP_ERROR_DECODE;
  return HDP_ERROR_NONE;
}

int DecodeBlockInverseAdaptiveScan(
    std::int16_t* coefficients, hdpAdaptiveHuffman_t* adaptiveHuffman,
    hdpAdaptiveScan_t* adaptiveScan, hdpBitIO_t* bitIO,
    std::int8_t modelBitCount, bool chroma) {
  int location = 1;
  int index = DecodeFirstIndex(&adaptiveHuffman[chroma ? 3 : 0], bitIO);
  int significantRun = index & 1;
  int remaining = index >> 2;
  int continuation = significantRun & remaining;
  const int sign = ReadBits(bitIO, 1) != 0 ? -1 : 0;
  int level = (1 << modelBitCount);
  if ((index & 2) != 0)
    level *= DecodeSignificantAbsLevel(
        &adaptiveHuffman[6 + continuation], bitIO);
  level = (level ^ sign) - sign;
  if (significantRun == 0)
    location += DecodeSignificantRun(15 - location, adaptiveHuffman, bitIO);
  location &= 15;
  coefficients[adaptiveScan[location].scanIndex] =
      static_cast<std::int16_t>(level);
  UpdateAdaptiveScan(adaptiveScan, location);
  location = (location + 1) & 15;

  int nonzeroCount = 1;
  while (remaining != 0) {
    significantRun = remaining & 1;
    if (significantRun == 0) {
      location += DecodeSignificantRun(15 - location,
                                       adaptiveHuffman, bitIO);
      if (location >= 16)
        return 16;
    }
    index = DecodeFollowingIndex(
        location + 1,
        &adaptiveHuffman[(chroma ? 3 : 0) + continuation + 1], bitIO);
    remaining = index >> 1;
    continuation &= remaining;
    const int nextSign = ReadBits(bitIO, 1) != 0 ? -1 : 0;
    level = 1 << modelBitCount;
    if ((index & 1) != 0)
      level *= DecodeSignificantAbsLevel(
          &adaptiveHuffman[6 + continuation], bitIO);
    level = (level ^ nextSign) - nextSign;
    coefficients[adaptiveScan[location].scanIndex] =
        static_cast<std::int16_t>(level);
    UpdateAdaptiveScan(adaptiveScan, location);
    location = (location + 1) & 15;
    ++nonzeroCount;
  }
  return nonzeroCount;
}

hdpError_t DecodeMacroblockHighPass(hdpCodec_t* codec,
                                    hdpContext_t* context,
                                    int macroblockX, int macroblockY) {
  if (codec->resetAdaptiveScanTotals)
    ResetAdaptiveScanTotalsHP(context);
  hdpTileQuantization_t& tile =
      codec->tileQuantization[codec->currentTileColumn];
  if (codec->codecParms.streamLayout == SL_FREQUENCY) {
    if (tile.numQuantizerIndexBitsHP != 0) {
      codec->MBInfo.quantizerIndexHP = DecodeQuantizerIndex(
          context->bitIOHP, tile.numQuantizerIndexBitsHP);
      if (codec->MBInfo.quantizerIndexHP >= tile.numQuantizersHP)
        return HDP_ERROR_DECODE;
    } else if (tile.numQuantizersHP > 1) {
      codec->MBInfo.quantizerIndexHP = codec->MBInfo.quantizerIndexLP;
    }
  }

  DecodeHPCBP(codec, context);
  PredictHPCBPDec(codec, context, macroblockX, macroblockY);
  int laplacianMean[16]{};
  const int channelCount = static_cast<int>(codec->codecParms.numChannels);
  const hdpColorFormat_t format = codec->codecParms.colorFormat;
  for (int channel = 0; channel < channelCount; ++channel) {
    hdpAdaptiveScan_t* scan =
        codec->MBInfo.predOrientationHP[channel] == 1
            ? context->adaptScanVerHP[0]
            : context->adaptScanHorHP[0];
    int cbp = codec->MBInfo.actualCBP[channel];
    const int modelBits =
        context->adaptCNModelHP.fixedLengthCodeBits[channel];
    const bool chroma = channel > 0 && IsYUV(format);
    for (int block = 0; block < 16; ++block, cbp >>= 1) {
      std::int16_t* coefficients =
          codec->macroBlockColumnPtr1[channel] + kSubBlockOffset[block];
      if ((cbp & 1) != 0) {
        const int decoded = DecodeBlockInverseAdaptiveScan(
            coefficients, context->adaptHuffHP, scan, context->bitIOHP,
            static_cast<std::int8_t>(modelBits), chroma);
        laplacianMean[channel] += decoded;
        if (decoded >= 16)
          return HDP_ERROR_DECODE;
      }
      if (modelBits - tile.trimFlexBits > 0) {
        DecodeBlockFlexBits(context->bitIOFL,
                            modelBits - tile.trimFlexBits,
                            tile.trimFlexBits, coefficients);
      }
    }
  }
  UpdateAdaptiveCoefficientNormalization(format, channelCount, laplacianMean,
                                         &context->adaptCNModelHP);
  if (codec->adaptContextHuffman)
    return AdaptContextHuffmanHP(context);
  return HDP_ERROR_NONE;
}

hdpError_t DecodeMacroblockLowPass(hdpCodec_t* codec,
                                   hdpContext_t* context,
                                   int, int) {
  if (codec->resetAdaptiveScanTotals)
    ResetAdaptiveScanTotalsLP(context);
  hdpTileQuantization_t& tile =
      codec->tileQuantization[codec->currentTileColumn];
  if (codec->codecParms.streamLayout == SL_FREQUENCY &&
      tile.numQuantizerIndexBitsLP != 0) {
    codec->MBInfo.quantizerIndexLP = DecodeQuantizerIndex(
        context->bitIOLP, tile.numQuantizerIndexBitsLP);
    if (codec->MBInfo.quantizerIndexLP >= tile.numQuantizersLP)
      return HDP_ERROR_DECODE;
  }

  int cbp = DecodeLPCBP(codec, context);
  int laplacianMean[16]{};
  const int channelCount = static_cast<int>(codec->codecParms.numChannels);
  const hdpColorFormat_t format = codec->codecParms.colorFormat;
  for (int channel = 0; channel < channelCount; ++channel, cbp >>= 1) {
    const int modelBits =
        context->adaptCNModelLP.fixedLengthCodeBits[channel];
    if ((cbp & 1) != 0) {
      const int decoded = DecodeBlockInverseAdaptiveScan(
          codec->MBInfo.compactDCLP[channel], context->adaptHuffLP,
          context->adaptScanLP[0], context->bitIOLP,
          static_cast<std::int8_t>(modelBits),
          channel > 0 && format == CF_YUV_444);
      laplacianMean[channel] += decoded;
      if (decoded >= 16)
        return HDP_ERROR_DECODE;
    }
    if (modelBits > 0)
      DecodeBlockModelBits(context->bitIOLP, modelBits,
                           codec->MBInfo.compactDCLP[channel]);
  }
  UpdateAdaptiveCoefficientNormalization(format, channelCount, laplacianMean,
                                         &context->adaptCNModelLP);
  if (codec->adaptContextHuffman)
    return AdaptContextHuffmanLP(context);
  return HDP_ERROR_NONE;
}

hdpError_t DecodeMacroblock(hdpCodec_t* codec, hdpContext_t* context,
                            int macroblockX, int macroblockY) {
  hdpError_t error = DecodeQuantizerIndexLPHP(codec, context);
  if (error != HDP_ERROR_NONE)
    return error;
  const std::size_t clearBytes = static_cast<std::size_t>(
      (32 * codec->codecParms.numChannels + 127) & ~127ULL);
  std::memset(codec->MBInfo.compactDCLP, 0, clearBytes);
  error = DecodeMacroblockDC(codec, context, macroblockX, macroblockY);
  if (error != HDP_ERROR_NONE)
    return error;
  if (codec->codecParms.decodeLP) {
    error = DecodeMacroblockLowPass(codec, context, macroblockX, macroblockY);
    if (error != HDP_ERROR_NONE)
      return error;
  }
  PredictDCLPDec(codec);
  if (codec->codecParms.decodeHP) {
    error = DecodeMacroblockHighPass(codec, context,
                                     macroblockX, macroblockY);
    if (error != HDP_ERROR_NONE)
      return error;
    PredictHPDec(codec);
  }
  UpdatePredictionInfo(codec, macroblockX);
  return HDP_ERROR_NONE;
}
