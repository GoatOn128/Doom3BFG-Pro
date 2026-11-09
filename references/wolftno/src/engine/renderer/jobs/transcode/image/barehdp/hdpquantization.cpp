#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <cstdint>
#include <cstring>

namespace {

struct ReciprocalEntry {
  std::uint32_t mantissa;
  std::uint8_t exponent;
};

constexpr ReciprocalEntry kReciprocals[32] = {
    {0x00000000, 0}, {0x00000000, 0}, {0x00000000, 1},
    {0xAAAAAAAB, 1}, {0x00000000, 2}, {0xCCCCCCCD, 2},
    {0xAAAAAAAB, 2}, {0x92492493, 2}, {0x00000000, 3},
    {0xE38E38E4, 3}, {0xCCCCCCCD, 3}, {0xBA2E8BA3, 3},
    {0xAAAAAAAB, 3}, {0x9D89D89E, 3}, {0x92492493, 3},
    {0x88888889, 3}, {0x00000000, 4}, {0xF0F0F0F1, 4},
    {0xE38E38E4, 4}, {0xD79435E6, 4}, {0xCCCCCCCD, 4},
    {0xC30C30C4, 4}, {0xBA2E8BA3, 4}, {0xB21642C9, 4},
    {0xAAAAAAAB, 4}, {0xA3D70A3E, 4}, {0x9D89D89E, 4},
    {0x97B425EE, 4}, {0x92492493, 4}, {0x8D3DCB09, 4},
    {0x88888889, 4}, {0x84210843, 4},
};

}  // namespace

hdpError_t AllocateQuantizers(hdpCodecParms_t* codecParms,
                              hdpQuantizer_t** quantizers,
                              std::uint64_t numChannels,
                              std::uint64_t numQuantizers) {
  if (numChannels > 16 || numQuantizers > 16)
    return HDP_ERROR_INVALID_PARM;
  quantizers[0] = static_cast<hdpQuantizer_t*>(TempAlloc(
      codecParms, sizeof(hdpQuantizer_t) * numChannels * numQuantizers, 16));
  if (quantizers[0] == nullptr)
    return HDP_ERROR_OUT_OF_MEMORY;
  for (std::uint64_t channel = 1; channel < numChannels; ++channel)
    quantizers[channel] = quantizers[channel - 1] + numQuantizers;
  return HDP_ERROR_NONE;
}

void FormatQuantizers(hdpQuantizer_t** quantizers, std::uint8_t channelMode,
                      std::uint64_t numChannels, std::uint64_t position,
                      std::uint8_t, bool useScaledArithmetic,
                      bool useRoundDC) {
  for (std::uint64_t channel = 0; channel < numChannels; ++channel) {
    if (channel != 0) {
      if (channelMode == 0)
        quantizers[channel][position] = quantizers[0][position];
      else if (channelMode == 1)
        quantizers[channel][position] = quantizers[1][position];
    }

    hdpQuantizer_t& quantizer = quantizers[channel][position];
    const std::uint8_t quantization = quantizer.quantization;
    if (quantization == 0) {
      quantizer.divisor = 1;
      quantizer.mantissa = 0;
      quantizer.exponent = 0;
    } else {
      int mantissa;
      int exponent;
      if (useScaledArithmetic) {
        if (quantization >= 16) {
          mantissa = (quantization & 15) + 16;
          exponent = quantization >> 4;
        } else {
          mantissa = quantization;
          exponent = 1;
        }
      } else if (quantization >= 32) {
        exponent = quantization >> 4;
        const int fraction = quantization & 15;
        if (quantization >= 48) {
          mantissa = fraction + 16;
          exponent -= 3;
        } else {
          mantissa = (fraction + 17) >> 1;
          exponent -= 2;
        }
      } else {
        exponent = 0;
        mantissa = (quantization + 3) >> 2;
      }
      quantizer.divisor = mantissa << exponent;
      quantizer.mantissa =
          static_cast<std::int32_t>(kReciprocals[mantissa].mantissa);
      quantizer.exponent = static_cast<std::uint8_t>(
          exponent + kReciprocals[mantissa].exponent);
    }
    quantizer.rounder = useRoundDC ? quantizer.divisor >> 1
                                   : (3 * quantizer.divisor + 1) >> 3;
  }
}

hdpError_t AllocateTileQuantization(hdpCodec_t* codec) {
  if (codec->codecParms.numTilesWideMinusOne >= 0x1000)
    return HDP_ERROR_INVALID_PARM;
  const std::uint64_t tileCount =
      static_cast<std::uint64_t>(codec->codecParms.numTilesWideMinusOne) + 1;
  codec->tileQuantization = static_cast<hdpTileQuantization_t*>(TempAlloc(
      &codec->codecParms, tileCount * sizeof(hdpTileQuantization_t), 16));
  if (codec->tileQuantization == nullptr)
    return HDP_ERROR_OUT_OF_MEMORY;
  std::memset(codec->tileQuantization, 0,
              tileCount * sizeof(hdpTileQuantization_t));
  for (std::uint64_t tile = 0; tile < tileCount; ++tile) {
    codec->tileQuantization[tile].numQuantizersLP = 1;
    codec->tileQuantization[tile].numQuantizersHP = 1;
  }
  return HDP_ERROR_NONE;
}

void FreeTileQuantization(hdpCodec_t*) {
  // All quantizer storage belongs to codecParms' temporary arena. Retail's
  // release callback is a no-op, and whole-program optimization removes every
  // pointer evaluation and leaves this routine with no observable mutation.
}

void SetUniformQuantizerDC(hdpCodec_t* codec) {
  for (std::uint32_t tile = 1;
       tile <= codec->codecParms.numTilesWideMinusOne; ++tile) {
    for (std::uint64_t channel = 0;
         channel < codec->codecParms.numChannels; ++channel) {
      codec->tileQuantization[tile].quantizerDC[channel] =
          codec->tileQuantization[0].quantizerDC[channel];
    }
  }
}

void SetUniformQuantizerLP(hdpCodec_t* codec) {
  for (std::uint32_t tile = 1;
       tile <= codec->codecParms.numTilesWideMinusOne; ++tile) {
    for (std::uint64_t channel = 0;
         channel < codec->codecParms.numChannels; ++channel) {
      codec->tileQuantization[tile].quantizerLP[channel] =
          codec->tileQuantization[0].quantizerLP[channel];
    }
  }
}

void SetUniformQuantizerHP(hdpCodec_t* codec) {
  for (std::uint32_t tile = 1;
       tile <= codec->codecParms.numTilesWideMinusOne; ++tile) {
    for (std::uint64_t channel = 0;
         channel < codec->codecParms.numChannels; ++channel) {
      codec->tileQuantization[tile].quantizerHP[channel] =
          codec->tileQuantization[0].quantizerHP[channel];
    }
  }
}

void SetUniformTrimFlexBits(hdpCodec_t* codec, bool decodeFlexBits) {
  const int trimFlexBits =
      decodeFlexBits ? codec->codecParms.uniformTrimFlexBits : 16;
  for (std::uint32_t tile = 0;
       tile <= codec->codecParms.numTilesWideMinusOne; ++tile) {
    codec->tileQuantization[tile].trimFlexBits = trimFlexBits;
  }
}

void UseDCQuantizerForLP(hdpCodec_t* codec, std::uint64_t tile) {
  for (std::uint64_t channel = 0;
       channel < codec->codecParms.numChannels; ++channel) {
    codec->tileQuantization[tile].quantizerLP[channel][0] =
        codec->tileQuantization[tile].quantizerDC[channel][0];
  }
}

void UseLPQuantizerForHP(hdpCodec_t* codec, std::uint64_t numQuantizers,
                         std::uint64_t tile) {
  for (std::uint64_t channel = 0;
       channel < codec->codecParms.numChannels; ++channel) {
    for (std::uint64_t quantizer = 0; quantizer < numQuantizers; ++quantizer) {
      codec->tileQuantization[tile].quantizerHP[channel][quantizer] =
          codec->tileQuantization[tile].quantizerLP[channel][quantizer];
    }
  }
}

std::uint8_t GetQuantizerBits(std::uint8_t numQuantizers) {
  if (numQuantizers < 2)
    return 0;
  if (numQuantizers < 4)
    return 1;
  if (numQuantizers < 6)
    return 2;
  return numQuantizers < 10 ? 3 : 4;
}
