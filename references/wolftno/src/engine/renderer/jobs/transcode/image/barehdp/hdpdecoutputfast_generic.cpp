#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace {

constexpr std::uint8_t kColorConvertIndex[16][16] = {
    {0, 1, 5, 4, 64, 65, 69, 68, 128, 129, 133, 132, 192, 193, 197, 196},
    {2, 3, 7, 6, 66, 67, 71, 70, 130, 131, 135, 134, 194, 195, 199, 198},
    {10, 11, 15, 14, 74, 75, 79, 78, 138, 139, 143, 142, 202, 203, 207, 206},
    {8, 9, 13, 12, 72, 73, 77, 76, 136, 137, 141, 140, 200, 201, 205, 204},
    {16, 17, 21, 20, 80, 81, 85, 84, 144, 145, 149, 148, 208, 209, 213, 212},
    {18, 19, 23, 22, 82, 83, 87, 86, 146, 147, 151, 150, 210, 211, 215, 214},
    {26, 27, 31, 30, 90, 91, 95, 94, 154, 155, 159, 158, 218, 219, 223, 222},
    {24, 25, 29, 28, 88, 89, 93, 92, 152, 153, 157, 156, 216, 217, 221, 220},
    {32, 33, 37, 36, 96, 97, 101, 100, 160, 161, 165, 164, 224, 225, 229, 228},
    {34, 35, 39, 38, 98, 99, 103, 102, 162, 163, 167, 166, 226, 227, 231, 230},
    {42, 43, 47, 46, 106, 107, 111, 110, 170, 171, 175, 174, 234, 235, 239, 238},
    {40, 41, 45, 44, 104, 105, 109, 108, 168, 169, 173, 172, 232, 233, 237, 236},
    {48, 49, 53, 52, 112, 113, 117, 116, 176, 177, 181, 180, 240, 241, 245, 244},
    {50, 51, 55, 54, 114, 115, 119, 118, 178, 179, 183, 182, 242, 243, 247, 246},
    {58, 59, 63, 62, 122, 123, 127, 126, 186, 187, 191, 190, 250, 251, 255, 254},
    {56, 57, 61, 60, 120, 121, 125, 124, 184, 185, 189, 188, 248, 249, 253, 252},
};

std::uint8_t ClampByte(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
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
             codec->codecParms.fullImageROIBottomY + 2 &&
         16 * macroblockX + 16 >= codec->codecParms.fullImageROILeftX &&
         16 * macroblockX <= codec->codecParms.fullImageROIRightX + 2;
}

}  // namespace

hdpError_t HDPDecOutputMB_Vmtr_Generic(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo) {
  if (codec == nullptr)
    return HDP_ERROR_DECODE;
  if (!MacroblockIntersectsROI(codec, bufferInfo))
    return HDP_ERROR_NONE;

  auto* output = static_cast<std::uint8_t*>(bufferInfo->buffer);
  const int shift = codec->codecParms.useScaledArith ? 3 : 0;
  const std::int16_t bias =
      static_cast<std::int16_t>(shift + (128 << shift));
  const std::uint64_t bytesPerPixel = codec->imageInfo.bitsPerUnit >> 3;
  const std::uint64_t macroblockBase = bufferInfo->macroblockIndex << 8;

  for (int row = 0; row < 16; ++row) {
    for (int column = 0; column < 16; ++column) {
      const std::uint64_t source =
          macroblockBase + kColorConvertIndex[row][column];
      for (int group = 0; group < 2; ++group) {
        const int channel = 3 * group;
        const std::int16_t intermediate = static_cast<std::int16_t>(
            bias + codec->macroBlockRowBuffer0[channel][source] -
            (static_cast<std::int16_t>(
                 -codec->macroBlockRowBuffer0[channel + 1][source]) >> 1));
        const std::int16_t green = static_cast<std::int16_t>(
            intermediate -
            ((codec->macroBlockRowBuffer0[channel + 2][source] + 1) >> 1) -
            codec->macroBlockRowBuffer0[channel + 1][source]);
        const std::int16_t blue = static_cast<std::int16_t>(
            green + codec->macroBlockRowBuffer0[channel + 2][source]);
        output[channel] = ClampByte(green >> shift);
        output[channel + 1] = ClampByte(intermediate >> shift);
        output[channel + 2] = ClampByte(blue >> shift);
      }

      int normalY = bias + codec->macroBlockRowBuffer0[7][source];
      int normalX = normalY + bias + codec->macroBlockRowBuffer0[6][source];
      const float x = static_cast<float>(normalX) * 0.0078431377F - 1.0F;
      const float y = static_cast<float>(normalY) * 0.0078431377F - 1.0F;
      const float z = std::sqrt(std::fabs(1.0F - x * x - y * y));
      const int encodedZ = static_cast<int>((z + 1.0F) * 127.5F + 0.5F);
      output[6] = ClampByte(normalX);
      output[7] = ClampByte(normalY);
      output[8] = ClampByte(ClampByte(encodedZ) +
                            codec->macroBlockRowBuffer0[8][source]);
      for (std::uint64_t channel = 9; channel < bytesPerPixel; ++channel) {
        output[channel] = ClampByte(
            (bias + codec->macroBlockRowBuffer0[channel][source]) >> shift);
      }
      output += bytesPerPixel;
    }
    output += bufferInfo->byteStride - 16 * bytesPerPixel;
  }
  return HDP_ERROR_NONE;
}

hdpError_t HDPDecOutputMB_NChannel_Generic(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo) {
  if (codec == nullptr)
    return HDP_ERROR_DECODE;
  if (!MacroblockIntersectsROI(codec, bufferInfo))
    return HDP_ERROR_NONE;

  auto* output = static_cast<std::uint8_t*>(bufferInfo->buffer);
  const std::uint64_t bytesPerPixel = codec->imageInfo.bitsPerUnit >> 3;
  const std::uint64_t macroblockBase = bufferInfo->macroblockIndex << 8;
  const int shift = codec->codecParms.useScaledArith ? 3 : 0;
  const int bias = shift + (128 << shift);
  for (int row = 0; row < 16; ++row) {
    for (int column = 0; column < 16; ++column) {
      const std::uint64_t source =
          macroblockBase + kColorConvertIndex[row][column];
      for (std::uint64_t channel = 0; channel < bytesPerPixel; ++channel) {
        output[channel] = ClampByte(
            (bias + codec->macroBlockRowBuffer0[channel][source]) >> shift);
      }
      output += bytesPerPixel;
    }
    output += bufferInfo->byteStride - 16 * bytesPerPixel;
  }
  return HDP_ERROR_NONE;
}
