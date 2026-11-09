#include "engine/renderer/imagetools/filter/mipmap.h"

bool idMipMap::CreateSingleMipMultiChannel(const std::uint8_t* src, int width,
                                           int height, int channels,
                                           std::uint8_t* dest) {
  if (width < 1 || height < 1 || width + height == 2)
    return false;

  const int mipWidth = width >> 1;
  const int mipHeight = height >> 1;
  if (mipWidth != 0 && mipHeight != 0) {
    const int sourceStride = channels * width;
    for (int y = 0; y < mipHeight; ++y) {
      const std::uint8_t* row0 = src + y * 2 * sourceStride;
      const std::uint8_t* row1 = row0 + sourceStride;
      for (int x = 0; x < mipWidth; ++x) {
        const std::uint8_t* p00 = row0 + x * 2 * channels;
        const std::uint8_t* p01 = p00 + channels;
        const std::uint8_t* p10 = row1 + x * 2 * channels;
        const std::uint8_t* p11 = p10 + channels;
        for (int channel = 0; channel < channels; ++channel) {
          *dest++ = static_cast<std::uint8_t>(
              (p00[channel] + p01[channel] + p10[channel] + p11[channel]) >> 2);
        }
      }
    }
  } else {
    const int mipLength = mipWidth + mipHeight;
    for (int i = 0; i < mipLength; ++i) {
      for (int channel = 0; channel < channels; ++channel)
        dest[channel] = static_cast<std::uint8_t>(
            (src[channel] + src[channels + channel]) >> 1);
      src += 2 * channels;
      dest += channels;
    }
  }
  return true;
}

bool idMipMap::CreateSingleMip(const std::uint8_t* src, int width, int height,
                               std::uint8_t* dest) {
  return CreateSingleMipMultiChannel(src, width, height, 4, dest);
}
