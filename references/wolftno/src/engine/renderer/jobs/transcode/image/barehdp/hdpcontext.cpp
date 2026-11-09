#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

constexpr int kWeightByBand[] = {240, 12, 1};
constexpr int kCoefficientOrderHP[16] = {
    0, 5, 1, 6, 10, 12, 8, 14, 2, 4, 3, 7, 9, 13, 11, 15,
};
constexpr int kDefaultScanOrderLP[16] = {
    0, 1, 4, 5, 2, 8, 6, 9, 3, 12, 10, 7, 13, 11, 14, 15,
};
constexpr int kDefaultHorScanOrderHP[16] = {
    0, 1, 4, 5, 2, 8, 6, 9, 3, 12, 10, 7, 13, 11, 14, 15,
};
constexpr int kDefaultVerScanOrderHP[16] = {
    0, 4, 8, 5, 1, 12, 9, 6, 2, 13, 3, 15, 7, 10, 14, 11,
};
constexpr int kAlphabetDC[3] = {8, 7, 7};
constexpr int kAlphabetLP[8] = {12, 6, 6, 12, 6, 6, 7, 7};
constexpr int kAlphabetHP[8] = {12, 6, 6, 12, 6, 6, 7, 7};

void ReleaseTemp(hdpCodecParms_t*, void*) {
  // The retail temporary allocator is arena based. Its release operation is
  // intentionally a no-op and was folded with unrelated no-op code by ICF.
}

template <std::size_t Count>
void AdaptHuffmanArray(hdpAdaptiveHuffman_t (&array)[Count]) {
  for (auto& huffman : array)
    AdaptiveHuffmanDiscriminant(&huffman);
}

}  // namespace

void FreeCodingContexts(hdpCodec_t* codec) {
  if (codec->tileContexts == nullptr)
    return;
  const std::uint32_t contextCount =
      codec->codecParms.numTilesWideMinusOne + 1U;
  for (std::uint32_t tile = 0; tile < contextCount; ++tile) {
    hdpContext_t& context = codec->tileContexts[tile];
    for (auto& huffman : context.adaptHuffDC)
      ShutdownAdaptiveHuffman(&codec->codecParms, &huffman);
    for (auto& huffman : context.adaptHuffLP)
      ShutdownAdaptiveHuffman(&codec->codecParms, &huffman);
    for (auto& huffman : context.adaptHuffHP)
      ShutdownAdaptiveHuffman(&codec->codecParms, &huffman);
    for (auto& huffman : context.adaptHuffCBP)
      ShutdownAdaptiveHuffman(&codec->codecParms, &huffman);
  }
  ReleaseTemp(&codec->codecParms, codec->tileContexts);
}

void InitContextBitIOs(hdpCodec_t* codec) {
  if (codec->numTileBitIOs == 0) {
    hdpContext_t& context = codec->tileContexts[0];
    context.bitIODC = codec->bitIOHeader;
    context.bitIOLP = codec->bitIOHeader;
    context.bitIOHP = codec->bitIOHeader;
    context.bitIOFL = codec->bitIOHeader;
    return;
  }

  const std::uint32_t tileColumns =
      codec->codecParms.numTilesWideMinusOne + 1U;
  for (std::uint32_t tile = 0; tile < tileColumns; ++tile) {
    hdpContext_t& context = codec->tileContexts[tile];
    if (codec->codecParms.streamLayout == SL_SPATIAL) {
      context.bitIODC = &codec->tileBitIOs[tile];
      context.bitIOLP = context.bitIODC;
      context.bitIOHP = context.bitIODC;
      context.bitIOFL = context.bitIODC;
      continue;
    }
    const std::uint64_t base = codec->numSubBands * tile;
    if (codec->numSubBands > 0)
      context.bitIODC = &codec->tileBitIOs[base];
    if (codec->numSubBands > 1)
      context.bitIOLP = &codec->tileBitIOs[base + 1];
    if (codec->numSubBands > 2)
      context.bitIOHP = &codec->tileBitIOs[base + 2];
    if (codec->numSubBands > 3)
      context.bitIOFL = &codec->tileBitIOs[base + 3];
  }
}

void ResetAdaptiveScanTotalsLP(hdpContext_t* context) {
  context->adaptScanLP[0][0].scanTotal = 0x7FFF;
  for (int coefficient = 1; coefficient < 16; ++coefficient)
    context->adaptScanLP[0][coefficient].scanTotal =
        static_cast<std::uint32_t>(34 - coefficient * 2);
}

void ResetAdaptiveScanTotalsHP(hdpContext_t* context) {
  context->adaptScanHorHP[0][0].scanTotal = 0x7FFF;
  context->adaptScanVerHP[0][0].scanTotal = 0x7FFF;
  for (int coefficient = 1; coefficient < 16; ++coefficient) {
    const auto total = static_cast<std::uint32_t>(34 - coefficient * 2);
    context->adaptScanHorHP[0][coefficient].scanTotal = total;
    context->adaptScanVerHP[0][coefficient].scanTotal = total;
  }
}

hdpError_t AdaptContextHuffmanDC(hdpContext_t* context) {
  AdaptHuffmanArray(context->adaptHuffDC);
  return HDP_ERROR_NONE;
}

hdpError_t AdaptContextHuffmanLP(hdpContext_t* context) {
  AdaptHuffmanArray(context->adaptHuffLP);
  return HDP_ERROR_NONE;
}

hdpError_t AdaptContextHuffmanHP(hdpContext_t* context) {
  AdaptHuffmanArray(context->adaptHuffHP);
  AdaptHuffmanArray(context->adaptHuffCBP);
  return HDP_ERROR_NONE;
}

void UpdateAdaptiveCoefficientNormalization(
    hdpColorFormat_t, int numChannels, int* laplacianMean,
    hdpAdaptiveCNModel_t* model) {
  const int weight = kWeightByBand[model->band - BAND_DC];
  for (int channel = 0; channel < numChannels; ++channel)
    laplacianMean[channel] *= weight;

  for (int channel = 0; channel < numChannels; ++channel) {
    int state = model->fixedLengthCodeState[channel];
    const int delta = (laplacianMean[channel] - 70) >> 2;
    if (delta <= -8) {
      state += std::max(delta + 4, -16);
      if (state < -8) {
        if (model->fixedLengthCodeBits[channel] != 0) {
          --model->fixedLengthCodeBits[channel];
          state = 0;
        } else {
          state = -8;
        }
      }
    } else if (delta >= 8) {
      state += std::min(delta - 4, 15);
      if (state > 8) {
        if (model->fixedLengthCodeBits[channel] < 15) {
          ++model->fixedLengthCodeBits[channel];
          state = 0;
        } else {
          model->fixedLengthCodeBits[channel] = 15;
          state = 8;
        }
      }
    }
    model->fixedLengthCodeState[channel] = state;
  }
}

void ResetCodingContext(hdpContext_t* context) {
  context->adaptCBPModelLP.maxCount = 1;
  context->adaptCBPModelLP.zeroCount = 1;
  for (int channel = 0; channel < 16; ++channel) {
    context->adaptCBPModelHP.countOnes[channel] = -4;
    context->adaptCBPModelHP.countZeros[channel] = 4;
    context->adaptCBPModelHP.state[channel] = 0;
    context->adaptCNModelDC.fixedLengthCodeState[channel] = 0;
    context->adaptCNModelDC.fixedLengthCodeBits[channel] = 8;
    context->adaptCNModelLP.fixedLengthCodeState[channel] = 0;
    context->adaptCNModelLP.fixedLengthCodeBits[channel] = 4;
    context->adaptCNModelHP.fixedLengthCodeState[channel] = 0;
    context->adaptCNModelHP.fixedLengthCodeBits[channel] = 0;
  }
  context->adaptCNModelDC.band = BAND_DC;
  context->adaptCNModelLP.band = BAND_LP;
  context->adaptCNModelHP.band = BAND_HP;

  for (int coefficient = 0; coefficient < 16; ++coefficient) {
    const std::uint32_t total = coefficient == 0
                                    ? 0x7FFFU
                                    : static_cast<std::uint32_t>(
                                          34 - coefficient * 2);
    context->adaptScanLP[0][coefficient] = {
        total, static_cast<std::uint32_t>(kDefaultScanOrderLP[coefficient])};
    context->adaptScanHorHP[0][coefficient] = {
        total,
        static_cast<std::uint32_t>(kCoefficientOrderHP[
            kDefaultHorScanOrderHP[coefficient]])};
    context->adaptScanVerHP[0][coefficient] = {
        total,
        static_cast<std::uint32_t>(kCoefficientOrderHP[
            kDefaultVerScanOrderHP[coefficient]])};
  }

  for (auto& huffman : context->adaptHuffDC)
    huffman.isInitialized = 0;
  for (auto& huffman : context->adaptHuffLP)
    huffman.isInitialized = 0;
  for (auto& huffman : context->adaptHuffHP)
    huffman.isInitialized = 0;
  for (auto& huffman : context->adaptHuffCBP)
    huffman.isInitialized = 0;
  AdaptContextHuffmanDC(context);
  AdaptContextHuffmanLP(context);
  AdaptContextHuffmanHP(context);
}

hdpError_t AllocateCodingContexts(hdpCodec_t* codec) {
  if (codec->codecParms.numTilesWideMinusOne >= 0x1000)
    return HDP_ERROR_INVALID_PARM;
  const std::uint32_t contextCount =
      codec->codecParms.numTilesWideMinusOne + 1U;
  codec->tileContexts = static_cast<hdpContext_t*>(TempAlloc(
      &codec->codecParms,
      static_cast<std::uint64_t>(contextCount) * sizeof(hdpContext_t), 16));
  if (codec->tileContexts == nullptr)
    return HDP_ERROR_OUT_OF_MEMORY;
  std::memset(codec->tileContexts, 0,
              static_cast<std::size_t>(contextCount) * sizeof(hdpContext_t));

  int alphabetCBP[3] = {9, 5, 4};
  if (codec->codecParms.colorFormat == CF_Y_ONLY ||
      codec->codecParms.colorFormat == CF_N_CHANNEL ||
      codec->codecParms.colorFormat == CF_CMYK) {
    alphabetCBP[0] = 5;
  }
  for (std::uint32_t tile = 0; tile < contextCount; ++tile) {
    hdpContext_t& context = codec->tileContexts[tile];
    for (int index = 0; index < 3; ++index)
      InitAdaptiveHuffman(&codec->codecParms, &context.adaptHuffDC[index],
                          kAlphabetDC[index]);
    for (int index = 0; index < 8; ++index)
      InitAdaptiveHuffman(&codec->codecParms, &context.adaptHuffLP[index],
                          kAlphabetLP[index]);
    for (int index = 0; index < 8; ++index)
      InitAdaptiveHuffman(&codec->codecParms, &context.adaptHuffHP[index],
                          kAlphabetHP[index]);
    for (int index = 0; index < 3; ++index)
      InitAdaptiveHuffman(&codec->codecParms, &context.adaptHuffCBP[index],
                          alphabetCBP[index]);
    ResetCodingContext(&context);
  }
  return HDP_ERROR_NONE;
}
