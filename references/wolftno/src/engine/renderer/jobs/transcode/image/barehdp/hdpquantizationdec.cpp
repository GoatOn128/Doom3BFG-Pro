#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <cstdint>
#include <emmintrin.h>

namespace {

std::int16_t MultiplyLow16(std::int16_t value, int factor) {
  const auto product = static_cast<std::uint32_t>(
      static_cast<std::uint16_t>(value)) *
      static_cast<std::uint16_t>(factor);
  return static_cast<std::int16_t>(static_cast<std::uint16_t>(product));
}

constexpr int kLPScatter[15] = {
    128, 64, 208, 32, 240, 48, 224, 16,
    192, 80, 144, 112, 176, 96, 160,
};

}  // namespace

void DequantizeHP_444(__m128i* coefficients, unsigned int* quantizer) {
  auto* vectors = coefficients;
  const __m128i factor = _mm_shuffle_epi32(
      _mm_shufflelo_epi16(_mm_cvtsi32_si128(static_cast<int>(*quantizer)), 0),
      0);
  for (int vector = 0; vector < 32; ++vector) {
    vectors[vector] = _mm_mullo_epi16(_mm_load_si128(vectors + vector), factor);
  }
}

hdpError_t DequantizeMacroblock(hdpCodec_t* codec) {
  const std::uint64_t numChannels = codec->codecParms.numChannels;
  const std::uint64_t transformChannels =
      codec->codecParms.colorFormat == CF_YUV_422 ? 1 : numChannels;
  hdpTileQuantization_t& tile =
      codec->tileQuantization[codec->currentTileColumn];

  if (codec->codecParms.decodeHP) {
    for (std::uint64_t channel = 0; channel < transformChannels; ++channel) {
      DequantizeHP_444(
          reinterpret_cast<__m128i*>(codec->macroBlockColumnPtr1[channel]),
          reinterpret_cast<unsigned int*>(
              &tile.quantizerHP[channel][codec->MBInfo.quantizerIndexHP]
                   .divisor));
    }
  }

  if (codec->codecParms.decodeLP) {
    for (std::uint64_t channel = 0; channel < transformChannels; ++channel) {
      std::int16_t* reconstructed = codec->macroBlockColumnPtr1[channel];
      const int divisor =
          tile.quantizerLP[channel][codec->MBInfo.quantizerIndexLP].divisor;
      for (int coefficient = 1; coefficient < 16; ++coefficient) {
        reconstructed[kLPScatter[coefficient - 1]] = MultiplyLow16(
            codec->MBInfo.compactDCLP[channel][coefficient], divisor);
      }
    }
  }

  for (std::uint64_t channel = 0; channel < numChannels; ++channel) {
    codec->macroBlockColumnPtr1[channel][0] = MultiplyLow16(
        codec->MBInfo.compactDCLP[channel][0],
        tile.quantizerDC[channel][0].divisor);
  }
  return HDP_ERROR_NONE;
}
