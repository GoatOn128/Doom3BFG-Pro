#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <algorithm>
#include <cstdint>

namespace {

constexpr int kAxisOrder[4] = {0, 1, 5, 4};
constexpr int kRowOrder[4] = {0, 2, 10, 8};

std::uint64_t ColorConvertIndex(int row, int column) {
  return static_cast<std::uint64_t>((row >> 2) * 16 + kRowOrder[row & 3] +
                                    (column >> 2) * 64 +
                                    kAxisOrder[column & 3]);
}

std::uint8_t ClampByte(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

std::uint8_t ConvertSample(std::int16_t sample, std::int16_t bias,
                           int shift) {
  const std::int16_t biased = static_cast<std::int16_t>(sample + bias);
  return ClampByte(biased >> shift);
}

bool MacroblockIntersectsROI(const hdpCodec_t* codec,
                             const hdpImageBufferInfo_t* bufferInfo) {
  if (codec->codecParms.decodeFullFrame)
    return true;
  const std::uint64_t macroblockX =
      bufferInfo->macroblockIndex +
      codec->codecParms.tileMacroblockOffsetX[codec->currentTileColumn];
  return 16 * codec->currentMacroblockRow >=
             codec->codecParms.fullImageROITopY &&
         16 * (codec->currentMacroblockRow - 1) <=
             codec->codecParms.fullImageROIBottomY &&
         16 * macroblockX + 16 >= codec->codecParms.fullImageROILeftX &&
         16 * macroblockX <= codec->codecParms.fullImageROIRightX;
}

template <typename PixelWriter>
hdpError_t OutputMacroblock(hdpCodec_t* codec,
                            const hdpImageBufferInfo_t* bufferInfo,
                            std::uint64_t bytesPerPixel,
                            PixelWriter&& writePixel) {
  if (codec == nullptr)
    return HDP_ERROR_DECODE;
  if (!MacroblockIntersectsROI(codec, bufferInfo))
    return HDP_ERROR_NONE;
  auto* output = static_cast<std::uint8_t*>(bufferInfo->buffer);
  const int shift = codec->codecParms.useScaledArith ? 3 : 0;
  const std::int16_t bias =
      static_cast<std::int16_t>(shift + (128 << shift));
  const std::uint64_t base = bufferInfo->macroblockIndex << 8;
  for (int row = 0; row < 16; ++row) {
    for (int column = 0; column < 16; ++column) {
      writePixel(output, base + ColorConvertIndex(row, column), bias, shift);
      output += bytesPerPixel;
    }
    output += bufferInfo->byteStride - 16 * bytesPerPixel;
  }
  return HDP_ERROR_NONE;
}

}  // namespace

hdpError_t HDPDecOutputMB_Mono_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo) {
  return OutputMacroblock(
      codec, bufferInfo, 1,
      [codec](std::uint8_t* output, std::uint64_t source,
              std::int16_t bias, int shift) {
        output[0] = ConvertSample(
            codec->macroBlockRowBuffer0[0][source], bias, shift);
      });
}

hdpError_t HDPDecOutputMB_GrayScale_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo) {
  return OutputMacroblock(
      codec, bufferInfo, 4,
      [codec](std::uint8_t* output, std::uint64_t source,
              std::int16_t bias, int shift) {
        const std::uint8_t gray = ConvertSample(
            codec->macroBlockRowBuffer0[0][source], bias, shift);
        output[0] = gray;
        output[1] = gray;
        output[2] = gray;
        output[3] = 255;
      });
}

hdpError_t HDPDecOutputMB_RGB_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo) {
  return OutputMacroblock(
      codec, bufferInfo, 4,
      [codec](std::uint8_t* output, std::uint64_t source,
              std::int16_t bias, int shift) {
        const std::int16_t y = codec->macroBlockRowBuffer0[0][source];
        const std::int16_t co = codec->macroBlockRowBuffer0[1][source];
        const std::int16_t cg = codec->macroBlockRowBuffer0[2][source];
        const std::int16_t negativeCo = static_cast<std::int16_t>(-co);
        const std::int16_t intermediate = static_cast<std::int16_t>(
            static_cast<std::int16_t>(y + bias) - (negativeCo >> 1));
        const std::int16_t halfCg =
            static_cast<std::int16_t>(static_cast<std::int16_t>(cg + 1) >> 1);
        const std::int16_t first = static_cast<std::int16_t>(
            negativeCo - static_cast<std::int16_t>(halfCg - intermediate));
        const std::int16_t third = static_cast<std::int16_t>(cg + first);
        output[0] = ClampByte(first >> shift);
        output[1] = ClampByte(intermediate >> shift);
        output[2] = ClampByte(third >> shift);
        output[3] = 255;
      });
}

hdpError_t HDPDecOutputMB_CoCg_Y_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo) {
  return OutputMacroblock(
      codec, bufferInfo, 4,
      [codec](std::uint8_t* output, std::uint64_t source,
              std::int16_t bias, int shift) {
        output[0] = ConvertSample(
            codec->macroBlockRowBuffer0[1][source], bias, shift);
        output[1] = ConvertSample(
            codec->macroBlockRowBuffer0[2][source], bias, shift);
        output[2] = 0;
        output[3] = ConvertSample(
            codec->macroBlockRowBuffer0[0][source], bias, shift);
      });
}

hdpError_t HDPDecOutputMB_Normal_Y_X_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo) {
  return OutputMacroblock(
      codec, bufferInfo, 4,
      [codec](std::uint8_t* output, std::uint64_t source,
              std::int16_t bias, int shift) {
        output[0] = 0;
        output[1] = ConvertSample(
            codec->macroBlockRowBuffer0[1][source], bias, shift);
        output[2] = 0;
        output[3] = ConvertSample(
            codec->macroBlockRowBuffer0[0][source], bias, shift);
      });
}

hdpError_t HDPDecOutputMBRowFast_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo) {
  if (codec == nullptr)
    return HDP_ERROR_DECODE;
  hdpImageBufferInfo_t macroblockInfo = *bufferInfo;
  macroblockInfo.macroblockIndex = 0;
  const std::uint64_t outputMacroblockBytes =
      16 * (codec->imageInfo.bitsPerUnit >> 3);
  const std::uint64_t macroblockCount =
      codec->imageInfo.imageWidth == 0
          ? 0
          : ((codec->imageInfo.imageWidth - 1) >> 4) + 1;
  for (std::uint64_t macroblock = 0; macroblock < macroblockCount;
       ++macroblock) {
    switch (codec->imageInfo.colorFormat) {
      case CF_N_CHANNEL:
        HDPDecOutputMB_NChannel_Generic(codec, &macroblockInfo);
        break;
      case CF_RGB:
        HDPDecOutputMB_RGB_SSE2_16(codec, &macroblockInfo);
        break;
      case CF_MONO:
        HDPDecOutputMB_Mono_SSE2_16(codec, &macroblockInfo);
        break;
      case CF_GRAYSCALE:
        HDPDecOutputMB_GrayScale_SSE2_16(codec, &macroblockInfo);
        break;
      case CF_COCG_Y:
        HDPDecOutputMB_CoCg_Y_SSE2_16(codec, &macroblockInfo);
        break;
      case CF_NORMAL_Y_X:
        HDPDecOutputMB_Normal_Y_X_SSE2_16(codec, &macroblockInfo);
        break;
      case CF_VMTR:
        HDPDecOutputMB_Vmtr_Generic(codec, &macroblockInfo);
        break;
      default:
        return HDP_ERROR_NONE;
    }
    macroblockInfo.buffer =
        static_cast<std::uint8_t*>(macroblockInfo.buffer) +
        outputMacroblockBytes;
    ++macroblockInfo.macroblockIndex;
  }
  return HDP_ERROR_NONE;
}
