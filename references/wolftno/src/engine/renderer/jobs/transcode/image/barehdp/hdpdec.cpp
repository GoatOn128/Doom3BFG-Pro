#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

constexpr std::uint8_t kChromaBlocks[16] = {
    0, 4, 8, 16, 16, 16, 16, 0, 0, 0, 0, 0, 0, 0, 0, 0,
};

std::uint64_t ReadBits(hdpBitIO_t* bitIO, unsigned count) {
  std::uint64_t buffer = bitIO->bitBuff;
  std::uint64_t available = bitIO->bitCnt;
  if (available < count)
    FillBitBuffer(bitIO, &buffer, &available);
  available -= count;
  bitIO->bitBuff = buffer;
  bitIO->bitCnt = available;
  const std::uint64_t mask = count == 64 ? ~0ULL : ((1ULL << count) - 1);
  return (buffer >> available) & mask;
}

void CopyImageInfo(hdpImageInfo_t* destination,
                   const hdpImageInfo_t* source) {
  std::memcpy(destination, source, sizeof(*destination));
}

std::uintptr_t AlignDown(std::uintptr_t value, std::uintptr_t alignment) {
  return value & ~(alignment - 1);
}

}  // namespace

std::uint8_t ReadQuantizerIndex(std::uint8_t* quantizerIndex,
                                hdpBitIO_t* bitIO,
                                std::uint64_t numChannels) {
  std::uint8_t mode = 0;
  std::uint64_t encodedChannels = 1;
  if (numChannels > 1) {
    mode = static_cast<std::uint8_t>(ReadBits(bitIO, 2));
    encodedChannels = mode == 0 ? 1 : (mode == 1 ? 2 : numChannels);
  }
  for (std::uint64_t channel = 0; channel < encodedChannels; ++channel)
    quantizerIndex[channel] = static_cast<std::uint8_t>(ReadBits(bitIO, 8));
  return mode;
}

std::uint8_t ReadQuantizer(hdpQuantizer_t** quantizers, hdpBitIO_t* bitIO,
                           std::uint64_t numChannels,
                           std::uint64_t position, bool transcode) {
  std::uint8_t mode = 0;
  std::uint64_t encodedChannels = 1;
  if (numChannels > 1) {
    mode = static_cast<std::uint8_t>(ReadBits(bitIO, 2));
    encodedChannels = mode == 0 ? 1 : (mode == 1 ? 2 : numChannels);
  }
  for (std::uint64_t channel = 0; channel < encodedChannels; ++channel)
    quantizers[channel][position].quantization =
        static_cast<std::uint8_t>(ReadBits(bitIO, 8));
  if (transcode) {
    for (std::uint64_t channel = 0; channel < encodedChannels; ++channel)
      quantizers[channel][position].quantization = 0;
  }
  return mode;
}

hdpError_t ReadPacketHeader(hdpBitIO_t* bitIO, std::uint8_t packetType,
                            std::uint8_t packetID) {
  const std::uint8_t first = static_cast<std::uint8_t>(ReadBits(bitIO, 8));
  const std::uint8_t second = static_cast<std::uint8_t>(ReadBits(bitIO, 8));
  const std::uint8_t third = static_cast<std::uint8_t>(ReadBits(bitIO, 8));
  const std::uint8_t fourth = static_cast<std::uint8_t>(ReadBits(bitIO, 8));
  return first == 0 && second == 0 && third == 1 &&
                 (fourth >> 3) == packetID && (fourth & 7) == packetType
             ? HDP_ERROR_NONE
             : HDP_ERROR_DECODE;
}

hdpError_t ReadTileHeaderQuantizerDC(hdpCodec_t* codec, hdpBitIO_t* bitIO) {
  auto& tile = codec->tileQuantization[codec->currentTileColumn];
  if (tile.quantizerDC[0] == nullptr) {
    const hdpError_t error = AllocateQuantizers(
        &codec->codecParms, tile.quantizerDC,
        codec->codecParms.numChannels, 1);
    if (error != HDP_ERROR_NONE)
      return error;
  }
  const std::uint8_t mode = ReadQuantizer(
      tile.quantizerDC, bitIO, codec->codecParms.numChannels, 0,
      codec->codecParms.transcode);
  FormatQuantizers(tile.quantizerDC, mode, codec->codecParms.numChannels, 0,
                   1, codec->codecParms.useScaledArith, true);
  return HDP_ERROR_NONE;
}

hdpError_t ReadTileHeaderQuantizerLP(hdpCodec_t* codec, hdpBitIO_t* bitIO) {
  auto& tile = codec->tileQuantization[codec->currentTileColumn];
  if (tile.quantizerLP[0] == nullptr) {
    const hdpError_t error = AllocateQuantizers(
        &codec->codecParms, tile.quantizerLP,
        codec->codecParms.numChannels, 16);
    if (error != HDP_ERROR_NONE)
      return error;
  }
  if (ReadBits(bitIO, 1) != 0) {
    tile.numQuantizersLP = 1;
    tile.numQuantizerIndexBitsLP = 0;
    UseDCQuantizerForLP(codec, codec->currentTileColumn);
    return HDP_ERROR_NONE;
  }
  tile.numQuantizersLP = static_cast<std::uint8_t>(ReadBits(bitIO, 4) + 1);
  tile.numQuantizerIndexBitsLP = GetQuantizerBits(tile.numQuantizersLP);
  for (std::uint8_t index = 0; index < tile.numQuantizersLP; ++index) {
    const std::uint8_t mode = ReadQuantizer(
        tile.quantizerLP, bitIO, codec->codecParms.numChannels, index,
        codec->codecParms.transcode);
    FormatQuantizers(tile.quantizerLP, mode, codec->codecParms.numChannels,
                     index, 1, codec->codecParms.useScaledArith, false);
  }
  return HDP_ERROR_NONE;
}

hdpError_t ReadTileHeaderQuantizerHP(hdpCodec_t* codec, hdpBitIO_t* bitIO) {
  auto& tile = codec->tileQuantization[codec->currentTileColumn];
  if (tile.quantizerHP[0] == nullptr) {
    const hdpError_t error = AllocateQuantizers(
        &codec->codecParms, tile.quantizerHP,
        codec->codecParms.numChannels, 16);
    if (error != HDP_ERROR_NONE)
      return error;
  }
  if (ReadBits(bitIO, 1) != 0) {
    tile.numQuantizersHP = tile.numQuantizersLP;
    tile.numQuantizerIndexBitsHP = 0;
    UseLPQuantizerForHP(codec, tile.numQuantizersLP,
                        codec->currentTileColumn);
    return HDP_ERROR_NONE;
  }
  tile.numQuantizersHP = static_cast<std::uint8_t>(ReadBits(bitIO, 4) + 1);
  tile.numQuantizerIndexBitsHP = GetQuantizerBits(tile.numQuantizersHP);
  for (std::uint8_t index = 0; index < tile.numQuantizersHP; ++index) {
    const std::uint8_t mode = ReadQuantizer(
        tile.quantizerHP, bitIO, codec->codecParms.numChannels, index,
        codec->codecParms.transcode);
    FormatQuantizers(tile.quantizerHP, mode, codec->codecParms.numChannels,
                     index, 0, codec->codecParms.useScaledArith, false);
  }
  return HDP_ERROR_NONE;
}

hdpError_t ReadTileHeaderTrimFlexBits(hdpCodec_t* codec,
                                      hdpBitIO_t* bitIO) {
  int trim = static_cast<int>(ReadBits(bitIO, 4));
  if (!codec->codecParms.decodeFlexBits)
    trim = 16;
  codec->tileQuantization[codec->currentTileColumn].trimFlexBits = trim;
  return HDP_ERROR_NONE;
}

hdpError_t ReadTileHeader(hdpCodec_t* codec, hdpContext_t* context) {
  if (codec->isSecondaryCodec) {
    ResetCodingContext(context);
    return HDP_ERROR_NONE;
  }
  if (codec->tileIndex != nullptr) {
    for (std::uint64_t band = 0; band < codec->numSubBands; ++band) {
      const std::uint64_t streamIndex =
          band + codec->currentTileColumn * codec->numSubBands;
      if (codec->currentTileRow != 0)
        StreamReadDetach(&codec->tileBitIOs[streamIndex]);
      codec->codecParms.baseStream->SetPos(
          codec->codecParms.baseStream,
          codec->baseTileOffset +
              codec->tileIndex[streamIndex +
                               codec->currentTileRow * codec->numTileBitIOs]);
      StreamReadAttach(&codec->tileBitIOs[streamIndex],
                       codec->codecParms.baseStream);
    }
  }
  const std::uint8_t packetID = static_cast<std::uint8_t>(
      (codec->currentTileColumn +
       (codec->codecParms.numTilesWideMinusOne + 1ULL) *
           codec->currentTileRow) &
      31);
  if (codec->codecParms.streamLayout == SL_SPATIAL) {
    if (codec->codecParms.subBands < SB_ISOLATED) {
      const hdpError_t error = ReadPacketHeader(context->bitIODC, 0, packetID);
      if (error != HDP_ERROR_NONE)
        return error;
    }
  } else {
    if (codec->codecParms.subBands < SB_ISOLATED &&
        ReadPacketHeader(context->bitIODC, 1, packetID) != HDP_ERROR_NONE)
      return HDP_ERROR_DECODE;
    if (codec->codecParms.subBands < SB_NO_LOWPASS) {
      const hdpError_t error = ReadPacketHeader(context->bitIOLP, 2, packetID);
      if (error != HDP_ERROR_NONE)
        return error;
    }
    if (codec->codecParms.subBands < SB_NO_HIGHPASS) {
      const hdpError_t error = ReadPacketHeader(context->bitIOHP, 3, packetID);
      if (error != HDP_ERROR_NONE)
        return error;
    }
    if (codec->codecParms.subBands < SB_NO_FLEXBITS) {
      const hdpError_t error = ReadPacketHeader(context->bitIOFL, 4, packetID);
      if (error != HDP_ERROR_NONE)
        return error;
    }
  }

  if (codec->codecParms.subBands < SB_ISOLATED &&
      (codec->codecParms.quantizationMode & 1U) != 0) {
    for (hdpCodec_t* plane = codec; plane != nullptr;
         plane = plane->nextCodec) {
      const hdpError_t error =
          ReadTileHeaderQuantizerDC(plane, context->bitIODC);
      if (error != HDP_ERROR_NONE)
        return error;
    }
  }
  if (codec->codecParms.subBands < SB_NO_LOWPASS &&
      (codec->codecParms.quantizationMode & 2U) != 0) {
    for (hdpCodec_t* plane = codec; plane != nullptr;
         plane = plane->nextCodec) {
      const hdpError_t error =
          ReadTileHeaderQuantizerLP(plane, context->bitIOLP);
      if (error != HDP_ERROR_NONE)
        return error;
    }
  }
  if (codec->codecParms.subBands < SB_NO_HIGHPASS &&
      (codec->codecParms.quantizationMode & 4U) != 0) {
    for (hdpCodec_t* plane = codec; plane != nullptr;
         plane = plane->nextCodec) {
      const hdpError_t error =
          ReadTileHeaderQuantizerHP(plane, context->bitIOHP);
      if (error != HDP_ERROR_NONE)
        return error;
    }
  }
  if (codec->codecParms.subBands < SB_NO_FLEXBITS &&
      codec->codecParms.hasPerTileTrimFlexbits) {
    for (hdpCodec_t* plane = codec; plane != nullptr;
         plane = plane->nextCodec) {
      const hdpError_t error =
          ReadTileHeaderTrimFlexBits(plane, context->bitIOFL);
      if (error != HDP_ERROR_NONE)
        return error;
    }
  }
  ResetCodingContext(context);
  return HDP_ERROR_NONE;
}

std::uint64_t GetVLWordEsc(hdpBitIO_t* bitIO, int* escape) {
  if (escape != nullptr)
    *escape = 0;
  const std::uint8_t prefix = static_cast<std::uint8_t>(ReadBits(bitIO, 8));
  if (prefix >= 253) {
    if (escape != nullptr)
      *escape = prefix;
    return 0;
  }
  if (prefix < 251)
    return (static_cast<std::uint64_t>(prefix) << 8) | ReadBits(bitIO, 8);
  if (prefix == 251)
    return (ReadBits(bitIO, 16) << 16) | ReadBits(bitIO, 16);
  return (ReadBits(bitIO, 16) << 48) | (ReadBits(bitIO, 16) << 32) |
         (ReadBits(bitIO, 16) << 16) | ReadBits(bitIO, 16);
}

hdpError_t ReadIndexTable(hdpCodec_t* codec) {
  hdpBitIO_t* bitIO = codec->bitIOHeader;
  if (codec->numTileBitIOs != 0) {
    if (ReadBits(bitIO, 16) != 1)
      return HDP_ERROR_DECODE;
    const std::uint64_t count =
        codec->numTileBitIOs *
        (codec->codecParms.numTilesHighMinusOne + 1ULL);
    for (std::uint64_t index = 0; index < count; ++index)
      codec->tileIndex[index] = GetVLWordEsc(bitIO, nullptr);
  }
  codec->baseTileOffset = GetVLWordEsc(bitIO, nullptr);
  bitIO->bitCnt -= bitIO->bitCnt & 7;
  codec->baseTileOffset += static_cast<std::uint32_t>(bitIO->byteCount) +
                           static_cast<std::uint32_t>(bitIO->streamOffset) -
                           static_cast<std::uint32_t>(bitIO->bitCnt >> 3);
  return HDP_ERROR_NONE;
}

hdpError_t InitImagePlaneDec(hdpCodec_t* codec) {
  if (codec->codecParms.numTilesWideMinusOne >= 0x1000)
    return HDP_ERROR_INVALID_PARM;
  const hdpColorFormat_t internalFormat = codec->codecParms.colorFormat;
  const hdpColorFormat_t outputFormat = codec->imageInfo.colorFormat;
  codec->codecParms.changeSubsamplingUV =
      ((internalFormat == CF_YUV_420 && outputFormat != CF_YUV_420) ||
       (internalFormat == CF_YUV_422 && outputFormat != CF_YUV_422)) &&
      outputFormat != CF_Y_ONLY;
  if (codec->codecParms.changeSubsamplingUV) {
    const std::uint64_t samplesPerMacroblock =
        outputFormat == CF_YUV_422 ? 128 : 256;
    const std::uint64_t bytes =
        2 * codec->widthInMacroblocks * samplesPerMacroblock;
    codec->resampleU = static_cast<std::int16_t*>(
        TempAlloc(&codec->codecParms, bytes, 16));
    codec->resampleV = static_cast<std::int16_t*>(
        TempAlloc(&codec->codecParms, bytes, 16));
    if (codec->resampleU == nullptr || codec->resampleV == nullptr)
      return HDP_ERROR_OUT_OF_MEMORY;
  }
  hdpError_t error = AllocateTileQuantization(codec);
  if (error != HDP_ERROR_NONE)
    return error;
  auto& tile = codec->tileQuantization[0];
  if (codec->codecParms.subBands < SB_ISOLATED &&
      (codec->codecParms.quantizationMode & 1U) == 0) {
    error = AllocateQuantizers(&codec->codecParms, tile.quantizerDC,
                               codec->codecParms.numChannels, 1);
    if (error != HDP_ERROR_NONE)
      return error;
    SetUniformQuantizerDC(codec);
    for (std::uint64_t channel = 0;
         channel < codec->codecParms.numChannels; ++channel) {
      tile.quantizerDC[channel][0].quantization =
          codec->codecParms.transcode ? 0
                                      : codec->codecParms.quantizationDC[channel];
    }
    FormatQuantizers(tile.quantizerDC,
                     (codec->codecParms.quantizationMode >> 3) & 3,
                     codec->codecParms.numChannels, 0, 1,
                     codec->codecParms.useScaledArith, true);
  }
  if (codec->codecParms.subBands < SB_NO_LOWPASS &&
      (codec->codecParms.quantizationMode & 2U) == 0) {
    error = AllocateQuantizers(&codec->codecParms, tile.quantizerLP,
                               codec->codecParms.numChannels, 1);
    if (error != HDP_ERROR_NONE)
      return error;
    SetUniformQuantizerLP(codec);
    if ((codec->codecParms.quantizationMode & 0x200U) != 0) {
      for (std::uint64_t channel = 0;
           channel < codec->codecParms.numChannels; ++channel) {
        tile.quantizerLP[channel][0].quantization =
            codec->codecParms.transcode
                ? 0
                : codec->codecParms.quantizetionLP[channel];
      }
      FormatQuantizers(tile.quantizerLP,
                       (codec->codecParms.quantizationMode >> 5) & 3,
                       codec->codecParms.numChannels, 0, 1,
                       codec->codecParms.useScaledArith, false);
    } else {
      UseDCQuantizerForLP(codec, 0);
    }
  }
  if (codec->codecParms.subBands < SB_NO_HIGHPASS &&
      (codec->codecParms.quantizationMode & 4U) == 0) {
    error = AllocateQuantizers(&codec->codecParms, tile.quantizerHP,
                               codec->codecParms.numChannels, 1);
    if (error != HDP_ERROR_NONE)
      return error;
    SetUniformQuantizerHP(codec);
    if ((codec->codecParms.quantizationMode & 0x400U) != 0) {
      for (std::uint64_t channel = 0;
           channel < codec->codecParms.numChannels; ++channel) {
        tile.quantizerHP[channel][0].quantization =
            codec->codecParms.transcode
                ? 0
                : codec->codecParms.quantizetionHP[channel];
      }
      FormatQuantizers(tile.quantizerHP,
                       (codec->codecParms.quantizationMode >> 7) & 3,
                       codec->codecParms.numChannels, 0, 0,
                       codec->codecParms.useScaledArith, false);
    } else {
      UseLPQuantizerForHP(codec, 1, 0);
    }
  }
  SetUniformTrimFlexBits(codec, codec->codecParms.decodeFlexBits);
  error = AllocatePredictionInfo(codec);
  return error == HDP_ERROR_NONE ? AllocateCodingContexts(codec) : error;
}

hdpError_t ReadImagePlaneHeader(hdpCodecParms_t* codecParms,
                                hdpBitIO_t* bitIO) {
  codecParms->colorFormat =
      static_cast<hdpColorFormat_t>(ReadBits(bitIO, 3));
  codecParms->useScaledArith = ReadBits(bitIO, 1) != 0;
  codecParms->subBands = static_cast<hdpSubBands_t>(ReadBits(bitIO, 4));
  switch (codecParms->colorFormat) {
    case CF_Y_ONLY:
      codecParms->numChannels = 1;
      break;
    case CF_YUV_420:
    case CF_YUV_422:
    case CF_YUV_444:
      codecParms->numChannels = 3;
      ReadBits(bitIO, 4);
      ReadBits(bitIO, 4);
      break;
    case CF_CMYK:
    case CF_BAYER:
      codecParms->numChannels = 4;
      break;
    case CF_N_CHANNEL:
      codecParms->numChannels = ReadBits(bitIO, 4) + 1;
      ReadBits(bitIO, 4);
      break;
    default:
      break;
  }
  codecParms->quantizationMode = 0;
  if (codecParms->subBands < SB_ISOLATED) {
    if (ReadBits(bitIO, 1) != 0)
      codecParms->quantizationMode +=
          8U * ReadQuantizerIndex(codecParms->quantizationDC, bitIO,
                                  codecParms->numChannels);
    else
      codecParms->quantizationMode |= 1U;
  }
  if (codecParms->subBands < SB_NO_LOWPASS) {
    if (ReadBits(bitIO, 1) != 0) {
      codecParms->quantizationMode +=
          2U * ((codecParms->quantizationMode & 1U) +
                2U * (codecParms->quantizationMode & 0x18U));
    } else {
      codecParms->quantizationMode |= 0x200U;
      if (ReadBits(bitIO, 1) != 0)
        codecParms->quantizationMode +=
            32U * ReadQuantizerIndex(codecParms->quantizetionLP, bitIO,
                                     codecParms->numChannels);
      else
        codecParms->quantizationMode |= 2U;
    }
  }
  if (codecParms->subBands < SB_NO_HIGHPASS) {
    if (ReadBits(bitIO, 1) != 0) {
      codecParms->quantizationMode +=
          2U * ((codecParms->quantizationMode & 2U) +
                2U * (codecParms->quantizationMode & 0x60U));
    } else {
      codecParms->quantizationMode |= 0x400U;
      if (ReadBits(bitIO, 1) != 0)
        codecParms->quantizationMode +=
            static_cast<std::uint32_t>(ReadQuantizerIndex(
                codecParms->quantizetionHP, bitIO,
                codecParms->numChannels))
            << 7;
      else
        codecParms->quantizationMode |= 4U;
    }
  }
  if (codecParms->subBands == SB_NO_LOWPASS)
    codecParms->quantizationMode |= 0x200U;
  else if (codecParms->subBands == SB_NO_HIGHPASS)
    codecParms->quantizationMode |= 0x400U;
  if ((codecParms->quantizationMode & 0x600U) == 0)
    return HDP_ERROR_DECODE;
  bitIO->bitCnt -= bitIO->bitCnt & 7;
  return HDP_ERROR_NONE;
}

hdpError_t ReadImageHeader(hdpImageInfo_t* imageInfo,
                           hdpCodecParms_t* codecParms, hdpBitIO_t*) {
  if ((imageInfo->imageWidth & 15) != 0)
    codecParms->extraPixelsRight = 16 - (imageInfo->imageWidth & 15);
  if ((imageInfo->imageHeight & 15) != 0)
    codecParms->extraPixelsBottom = 16 - (imageInfo->imageHeight & 15);
  codecParms->numTilesHighMinusOne = 0;
  codecParms->numTilesWideMinusOne = 0;
  codecParms->tileMacroblockOffsetX = static_cast<std::uint32_t*>(
      TempAlloc(codecParms, sizeof(std::uint32_t), 16));
  codecParms->tileMacroblockOffsetY = static_cast<std::uint32_t*>(
      TempAlloc(codecParms, sizeof(std::uint32_t), 16));
  if (codecParms->tileMacroblockOffsetX == nullptr ||
      codecParms->tileMacroblockOffsetY == nullptr)
    return HDP_ERROR_OUT_OF_MEMORY;
  codecParms->tileMacroblockOffsetX[0] = 0;
  codecParms->tileMacroblockOffsetY[0] = 0;
  return HDP_ERROR_NONE;
}

hdpError_t ProcessMacroblockDec(hdpCodec_t* firstCodec) {
  const bool boundary =
      firstCodec->currentMacroblockRow == firstCodec->heightInMacroblocks ||
      firstCodec->currentMacroblockColumn == firstCodec->widthInMacroblocks;
  const std::uint64_t overlap =
      firstCodec->codecParms.overlap == OL_NONE ? 0 : 16;
  for (hdpCodec_t* codec = firstCodec; codec != nullptr;
       codec = codec->nextCodec) {
    codec->currentMacroblockRow = firstCodec->currentMacroblockRow;
    codec->currentMacroblockColumn = firstCodec->currentMacroblockColumn;
    const std::uint64_t x = codec->currentMacroblockColumn;
    const std::uint64_t y = codec->currentMacroblockRow;
    const bool inROI = codec->codecParms.decodeFullFrame ||
        !((16 * x + overlap < codec->codecParms.fullImageROILeftX) ||
          (16 * x > overlap + codec->codecParms.fullImageROIRightX) ||
          (16 * y + overlap < codec->codecParms.fullImageROITopY) ||
          (16 * y > overlap + codec->codecParms.fullImageROIBottomY));
    if (!boundary) {
      UpdateTilePos(codec, x, y);
      hdpContext_t* context = &codec->tileContexts[codec->currentTileColumn];
      if (codec->atTileLeftMB && codec->atTileTopMB) {
        const hdpError_t error = ReadTileHeader(codec, context);
        if (error != HDP_ERROR_NONE)
          return error;
      }
      hdpError_t error = DecodeMacroblock(
          codec, context, static_cast<int>(x), static_cast<int>(y));
      if (error != HDP_ERROR_NONE)
        return error;
      if (inROI) {
        error = DequantizeMacroblock(codec);
        if (error != HDP_ERROR_NONE)
          return error;
      }
    }
    if (inROI && !codec->codecParms.decodeCoeffientsOnly)
      InvTransformMacroblock_SSE2_16(codec, static_cast<int>(x),
                                     static_cast<int>(y));
  }
  return HDP_ERROR_NONE;
}

hdpError_t HDPDecDecodeMBRow(hdpCodec_t* firstCodec) {
  if (firstCodec == nullptr)
    return HDP_ERROR_DECODE;
  const auto& parms = firstCodec->codecParms;
  const std::uint64_t firstColumn =
      parms.tileMacroblockOffsetX[parms.fullImageROILeftTile];
  const std::uint64_t lastColumn =
      parms.fullImageROIRightTile >= parms.numTilesWideMinusOne
          ? firstCodec->widthInMacroblocks - 1
          : parms.tileMacroblockOffsetX[parms.fullImageROIRightTile + 1] - 1;
  const std::uint64_t firstRow =
      parms.tileMacroblockOffsetY[parms.fullImageROITopTile];
  const std::uint64_t lastRow =
      parms.fullImageROIBottomTile >= parms.numTilesHighMinusOne
          ? firstCodec->heightInMacroblocks - 1
          : parms.tileMacroblockOffsetY[parms.fullImageROIBottomTile + 1] - 1;
  if (firstCodec->processedFirstMBRow) {
    SwapMacroblockPredPtrs(firstCodec);
    SwapMacroblockRowPtrs(firstCodec);
    ++firstCodec->currentMacroblockRow;
  }
  if (firstCodec->currentMacroblockRow > lastRow + 1)
    return HDP_ERROR_NONE;
  InitMacroblockColumnPtrs(firstCodec);
  for (hdpCodec_t* codec = firstCodec; codec != nullptr;
       codec = codec->nextCodec) {
    std::memset(codec->macroBlockColumnPtr1[0], 0,
                codec->widthInMacroblocks * 512);
    const std::size_t chromaBytes =
        codec->widthInMacroblocks * 32 *
        kChromaBlocks[codec->codecParms.colorFormat];
    for (std::uint64_t channel = 1;
         channel < codec->codecParms.numChannels; ++channel)
      std::memset(codec->macroBlockColumnPtr1[channel], 0, chromaBytes);
    if (!firstCodec->processedFirstMBRow) {
      codec->currentMacroblockRow = firstRow;
      codec->currentTileRow = parms.fullImageROITopTile;
    }
    codec->currentMacroblockColumn = firstColumn;
    codec->currentTileColumn = parms.fullImageROILeftTile;
  }
  firstCodec->currentMacroblockColumn = firstColumn;
  while (firstCodec->currentMacroblockColumn <= lastColumn) {
    const hdpError_t error = ProcessMacroblockDec(firstCodec);
    if (error != HDP_ERROR_NONE)
      return error;
    AdvanceMacroblockColumnPtrs(firstCodec);
    ++firstCodec->currentMacroblockColumn;
  }
  const hdpError_t error = ProcessMacroblockDec(firstCodec);
  if (error == HDP_ERROR_NONE)
    firstCodec->processedFirstMBRow = true;
  return error;
}

hdpError_t HDPDecShutdown(hdpCodec_t* firstCodec) {
  if (firstCodec == nullptr)
    return HDP_ERROR_DECODE;
  for (hdpCodec_t* codec = firstCodec; codec != nullptr;) {
    hdpCodec_t* next = codec->nextCodec;
    FreePredictionInfo(codec);
    FreeTileQuantization(codec);
    FreeCodingContexts(codec);
    if (codec == firstCodec) {
      StreamReadDetach(codec->bitIOHeader);
      for (std::uint64_t index = 0; index < codec->numTileBitIOs; ++index) {
        hdpStream_t* stream = codec->tileStreams[index];
        stream->Close(stream);
      }
    }
    codec = next;
  }
  return HDP_ERROR_NONE;
}

void DeriveDecoderParms(hdpCodecParms_t* parms,
                        std::uint64_t fullWidth,
                        std::uint64_t fullHeight) {
  if (parms->fullImageROIRightX == 0 || parms->fullImageROIBottomY == 0) {
    parms->fullImageROILeftX = 0;
    parms->fullImageROITopY = 0;
    parms->fullImageROIRightX = fullWidth - 1;
    parms->fullImageROIBottomY = fullHeight - 1;
  }
  if (parms->fullImageROILeftX >= fullWidth)
    parms->fullImageROILeftX = 0;
  if (parms->fullImageROITopY >= fullHeight)
    parms->fullImageROITopY = 0;
  if (parms->fullImageROIRightX > fullWidth)
    parms->fullImageROIRightX = fullWidth - 1;
  if (parms->fullImageROIBottomY > fullHeight)
    parms->fullImageROIBottomY = fullHeight - 1;
  const std::uint64_t left =
      std::max(parms->fullImageROILeftX, parms->extraPixelsLeft);
  const std::uint64_t top =
      std::max(parms->fullImageROITopY, parms->extraPixelsTop);
  const std::uint64_t right = std::min(
      parms->fullImageROIRightX, fullWidth - parms->extraPixelsLeft - 1);
  const std::uint64_t bottom = std::min(
      parms->fullImageROIBottomY, fullHeight - parms->extraPixelsBottom - 1);
  parms->fullImageROILeftX = left;
  parms->fullImageROITopY = top;
  parms->fullImageROIRightX = right;
  parms->fullImageROIBottomY = bottom;
  const std::uint64_t overlap = parms->overlap == OL_NONE ? 0 : 16;
  parms->fullImageROILeftTile = 0;
  parms->fullImageROIRightTile = parms->numTilesWideMinusOne;
  parms->fullImageROITopTile = 0;
  parms->fullImageROIBottomTile = parms->numTilesHighMinusOne;
  for (std::uint32_t tile = 1; tile <= parms->numTilesWideMinusOne; ++tile) {
    if (16ULL * parms->tileMacroblockOffsetX[tile] < left -
        std::min(left, overlap))
      parms->fullImageROILeftTile = tile;
    if (16ULL * parms->tileMacroblockOffsetX[tile] <= right + overlap)
      parms->fullImageROIRightTile = tile;
  }
  for (std::uint32_t tile = 1; tile <= parms->numTilesHighMinusOne; ++tile) {
    if (16ULL * parms->tileMacroblockOffsetY[tile] < top -
        std::min(top, overlap))
      parms->fullImageROITopTile = tile;
    if (16ULL * parms->tileMacroblockOffsetY[tile] <= bottom + overlap)
      parms->fullImageROIBottomTile = tile;
  }
  parms->thumbnailScale = 1;
  const std::uint64_t roiWidth = right - left + 1;
  while (parms->thumbnailWidth * parms->thumbnailScale < roiWidth)
    parms->thumbnailScale *= 2;
  parms->decodeLP = parms->subBands < SB_NO_LOWPASS &&
      (parms->streamLayout != SL_FREQUENCY || parms->thumbnailScale < 16);
  parms->decodeHP = parms->subBands < SB_NO_HIGHPASS &&
      (parms->streamLayout != SL_FREQUENCY || parms->thumbnailScale < 4);
  parms->decodeFlexBits = parms->subBands < SB_NO_FLEXBITS &&
      (parms->streamLayout != SL_FREQUENCY || !parms->skipFlexbits);
  parms->decodeFullFrame = top + left == 0 &&
      ((right + 15) >> 4) >= fullWidth &&
      ((bottom + 15) >> 4) >= fullHeight;
  parms->decodeFullWidth = left == 0 && ((right + 15) >> 4) >= fullWidth;
}

hdpError_t HDPDecInit(hdpImageInfo_t* imageInfo,
                      hdpCodecParms_t* codecParms, hdpCodec_t** codecOut) {
  *codecOut = nullptr;
  hdpBitIO_t attached{};
  hdpError_t error = StreamReadAttach(&attached, codecParms->baseStream);
  if (error != HDP_ERROR_NONE)
    return error;
  error = ReadImageHeader(imageInfo, codecParms, &attached);
  if (error != HDP_ERROR_NONE)
    return error;
  error = ReadImagePlaneHeader(codecParms, &attached);
  if (error != HDP_ERROR_NONE)
    return error;
  const std::uint64_t fullWidth = imageInfo->imageWidth +
      codecParms->extraPixelsLeft + codecParms->extraPixelsRight;
  const std::uint64_t fullHeight = imageInfo->imageHeight +
      codecParms->extraPixelsTop + codecParms->extraPixelsBottom;
  DeriveDecoderParms(codecParms, fullWidth, fullHeight);
  const std::uint64_t widthMB = (fullWidth + 15) >> 4;
  const std::uint64_t heightMB = (fullHeight + 15) >> 4;
  const std::uint64_t chromaBytes =
      32ULL * kChromaBlocks[codecParms->colorFormat];
  const std::uint64_t allocationSize =
      2 * widthMB *
          (chromaBytes * (codecParms->numChannels - 1) + 512) +
      2679;
  auto* codec = static_cast<hdpCodec_t*>(
      TempAlloc(codecParms, allocationSize, 128));
  if (codec == nullptr)
    return HDP_ERROR_OUT_OF_MEMORY;
  std::memset(codec, 0, sizeof(*codec));
  CopyImageInfo(&codec->imageInfo, imageInfo);
  std::memcpy(&codec->codecParms, codecParms, sizeof(*codecParms));
  codec->widthInMacroblocks = widthMB;
  codec->heightInMacroblocks = heightMB;
  auto cursor = AlignDown(reinterpret_cast<std::uintptr_t>(codec) + 0xA38, 128);
  for (std::uint64_t channel = 0; channel < codec->codecParms.numChannels;
       ++channel) {
    codec->macroBlockRowBuffer0[channel] =
        reinterpret_cast<std::int16_t*>(cursor);
    const std::uint64_t channelBytes = channel == 0 ? 512 : chromaBytes;
    cursor += channelBytes * widthMB;
    codec->macroBlockRowBuffer1[channel] =
        reinterpret_cast<std::int16_t*>(cursor);
    cursor += channelBytes * widthMB;
  }
  codec->bitIOHeader = reinterpret_cast<hdpBitIO_t*>(cursor);
  *codec->bitIOHeader = attached;
  error = AllocateAdditionalBitIOs(codec);
  if (error == HDP_ERROR_NONE)
    error = ReadIndexTable(codec);
  if (error == HDP_ERROR_NONE)
    error = InitImagePlaneDec(codec);
  if (error != HDP_ERROR_NONE)
    return error;
  InitContextBitIOs(codec);

  if (codecParms->hasAlphaChannel) {
    auto* alpha = static_cast<hdpCodec_t*>(
        TempAlloc(codecParms, (widthMB << 10) + 2615, 16));
    if (alpha == nullptr)
      return HDP_ERROR_OUT_OF_MEMORY;
    std::memset(alpha, 0, (widthMB << 10) + 2615);
    CopyImageInfo(&alpha->imageInfo, imageInfo);
    std::memcpy(&alpha->codecParms, codecParms, sizeof(*codecParms));
    alpha->widthInMacroblocks = widthMB;
    alpha->heightInMacroblocks = heightMB;
    error = ReadImagePlaneHeader(&alpha->codecParms, codec->bitIOHeader);
    if (error != HDP_ERROR_NONE)
      return error;
    alpha->isSecondaryCodec = true;
    auto alphaRows = AlignDown(
        reinterpret_cast<std::uintptr_t>(alpha) + 0xA38, 128);
    alpha->macroBlockRowBuffer0[0] =
        reinterpret_cast<std::int16_t*>(alphaRows);
    alpha->macroBlockRowBuffer1[0] =
        reinterpret_cast<std::int16_t*>(alphaRows + (widthMB << 9));
    codec->nextCodec = alpha;
    error = InitImagePlaneDec(alpha);
    if (error != HDP_ERROR_NONE)
      return error;
    alpha->tileIndex = codec->tileIndex;
    alpha->tileStreams = codec->tileStreams;
    alpha->bitIOHeader = codec->bitIOHeader;
    alpha->tileBitIOs = codec->tileBitIOs;
    alpha->numTileBitIOs = codec->numTileBitIOs;
    alpha->numSubBands = codec->numSubBands;
    InitContextBitIOs(alpha);
  }
  CopyImageInfo(imageInfo, &codec->imageInfo);
  std::memcpy(codecParms, &codec->codecParms, sizeof(*codecParms));
  *codecOut = codec;
  return HDPDecDecodeMBRow(codec);
}
