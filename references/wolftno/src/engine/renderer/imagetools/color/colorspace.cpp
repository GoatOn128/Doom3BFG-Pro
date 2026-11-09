#include "engine/renderer/imagetools/color/colorspace.h"

#include <algorithm>

namespace {

std::uint8_t ClampByte(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}

} // namespace

void idColorSpace::ConvertRGBToCoCg_Y(std::uint8_t* dst,
                                      const std::uint8_t* src,
                                      int width, int height) {
  const int pixelCount = width * height;
  for (int i = 0; i < pixelCount; ++i, src += 4, dst += 4) {
    const int red = src[0];
    const int green = src[1];
    const int blue = src[2];
    dst[0] = ClampByte(((2 * red + 2 - 2 * blue) >> 2) + 128);
    dst[1] = ClampByte(((2 * green - blue - red + 2) >> 2) + 128);
    dst[2] = 0;
    dst[3] = ClampByte((2 * green + blue + red + 2) >> 2);
  }
}

void idColorSpace::ConvertRGBAToCoACg_Y(std::uint8_t* dst,
                                        const std::uint8_t* src,
                                        int width, int height) {
  const int pixelCount = width * height;
  for (int i = 0; i < pixelCount; ++i, src += 4, dst += 4) {
    const int red = src[0];
    const int green = src[1];
    const int blue = src[2];
    dst[0] = ClampByte(((2 * red + 2 - 2 * blue) >> 2) + 132);
    dst[1] = src[3];
    dst[2] = ClampByte(((2 * green - blue - red + 2) >> 2) + 132);
    dst[3] = ClampByte((2 * green + blue + red + 2) >> 2);
  }
}
