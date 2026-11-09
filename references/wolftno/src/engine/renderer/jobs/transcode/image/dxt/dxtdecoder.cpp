#include "engine/renderer/jobs/transcode/image/dxt/dxtdecoder.h"

#include <cstdint>
#include <cstring>

namespace {

std::uint16_t ReadLittle16(const std::uint8_t* data) {
  return static_cast<std::uint16_t>(data[0] | (data[1] << 8));
}

std::uint32_t ReadLittle32(const std::uint8_t* data) {
  return static_cast<std::uint32_t>(data[0]) |
         (static_cast<std::uint32_t>(data[1]) << 8) |
         (static_cast<std::uint32_t>(data[2]) << 16) |
         (static_cast<std::uint32_t>(data[3]) << 24);
}

void Decode565(std::uint16_t packed, std::uint8_t* color) {
  const int red = (packed >> 11) & 31;
  const int green = (packed >> 5) & 63;
  const int blue = packed & 31;
  color[0] = static_cast<std::uint8_t>((red << 3) | (red >> 2));
  color[1] = static_cast<std::uint8_t>((green << 2) | (green >> 4));
  color[2] = static_cast<std::uint8_t>((blue << 3) | (blue >> 2));
  color[3] = 255;
}

void CopyBlock(const std::uint8_t* block, std::uint8_t* output, int stride) {
  for (int row = 0; row < 4; ++row)
    std::memcpy(output + row * stride, block + row * 16, 16);
}

}  // namespace

void idDxtDecoder::DecodeAlphaValues(std::uint8_t* colorBlock, int offset) {
  std::uint8_t alpha[8];
  alpha[0] = inData[0];
  alpha[1] = inData[1];
  if (alpha[0] <= alpha[1]) {
    alpha[2] = static_cast<std::uint8_t>((4 * alpha[0] + alpha[1]) / 5);
    alpha[3] = static_cast<std::uint8_t>((3 * alpha[0] + 2 * alpha[1]) / 5);
    alpha[4] = static_cast<std::uint8_t>((2 * alpha[0] + 3 * alpha[1]) / 5);
    alpha[5] = static_cast<std::uint8_t>((alpha[0] + 4 * alpha[1]) / 5);
    alpha[6] = 0;
    alpha[7] = 255;
  } else {
    alpha[2] = static_cast<std::uint8_t>((6 * alpha[0] + alpha[1]) / 7);
    alpha[3] = static_cast<std::uint8_t>((5 * alpha[0] + 2 * alpha[1]) / 7);
    alpha[4] = static_cast<std::uint8_t>((4 * alpha[0] + 3 * alpha[1]) / 7);
    alpha[5] = static_cast<std::uint8_t>((3 * alpha[0] + 4 * alpha[1]) / 7);
    alpha[6] = static_cast<std::uint8_t>((2 * alpha[0] + 5 * alpha[1]) / 7);
    alpha[7] = static_cast<std::uint8_t>((alpha[0] + 6 * alpha[1]) / 7);
  }

  std::uint64_t indices = 0;
  for (int byte = 0; byte < 6; ++byte)
    indices |= static_cast<std::uint64_t>(inData[2 + byte]) << (byte * 8);
  inData += 8;
  for (int pixel = 0; pixel < 16; ++pixel) {
    colorBlock[pixel * 4 + offset] = alpha[indices & 7u];
    indices >>= 3;
  }
}

void idDxtDecoder::DecodeColorValues(std::uint8_t* colorBlock, bool noBlack,
                                     bool writeAlpha) {
  const std::uint16_t endpoint0 = ReadLittle16(inData);
  const std::uint16_t endpoint1 = ReadLittle16(inData + 2);
  std::uint8_t colors[4][4]{};
  Decode565(endpoint0, colors[0]);
  Decode565(endpoint1, colors[1]);
  if (noBlack || endpoint0 > endpoint1) {
    for (int channel = 0; channel < 3; ++channel) {
      colors[2][channel] = static_cast<std::uint8_t>(
          (2 * colors[0][channel] + colors[1][channel]) / 3);
      colors[3][channel] = static_cast<std::uint8_t>(
          (colors[0][channel] + 2 * colors[1][channel]) / 3);
    }
    colors[2][3] = 255;
    colors[3][3] = 255;
  } else {
    for (int channel = 0; channel < 3; ++channel)
      colors[2][channel] = static_cast<std::uint8_t>(
          (colors[0][channel] + colors[1][channel]) / 2);
    colors[2][3] = 255;
    colors[3][0] = colors[3][1] = colors[3][2] = colors[3][3] = 0;
  }

  std::uint32_t indices = ReadLittle32(inData + 4);
  inData += 8;
  for (int pixel = 0; pixel < 16; ++pixel) {
    const std::uint8_t* color = colors[indices & 3u];
    std::uint8_t* destination = colorBlock + pixel * 4;
    destination[0] = color[0];
    destination[1] = color[1];
    destination[2] = color[2];
    if (writeAlpha)
      destination[3] = color[3];
    indices >>= 2;
  }
}

void idDxtDecoder::DecompressImageDXT1(const std::uint8_t* input,
                                       std::uint8_t* output, int imageWidth,
                                       int imageHeight) {
  width = imageWidth;
  height = imageHeight;
  inData = input;
  std::uint8_t block[64];
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      DecodeColorValues(block, false, true);
      CopyBlock(block, output + 4 * (x + y * width), width * 4);
    }
  }
}

void idDxtDecoder::DecompressImageDXT5(const std::uint8_t* input,
                                       std::uint8_t* output, int imageWidth,
                                       int imageHeight) {
  width = imageWidth;
  height = imageHeight;
  inData = input;
  std::uint8_t block[64];
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      DecodeAlphaValues(block, 3);
      DecodeColorValues(block, true, false);
      CopyBlock(block, output + 4 * (x + y * width), width * 4);
    }
  }
}

void idDxtDecoder::DecompressImageDXT5_nVidia7x(
    const std::uint8_t* input, std::uint8_t* output, int imageWidth,
    int imageHeight) {
  width = imageWidth;
  height = imageHeight;
  inData = input;
  std::uint8_t block[64];
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      DecodeAlphaValues(block, 3);
      DecodeColorValues(block, false, false);
      CopyBlock(block, output + 4 * (x + y * width), width * 4);
    }
  }
}

void idDxtDecoder::DecompressYCoCgDXT5(const std::uint8_t* input,
                                       std::uint8_t* output, int imageWidth,
                                       int imageHeight) {
  DecompressImageDXT5_nVidia7x(input, output, imageWidth, imageHeight);
  const int pixels = width * height;
  for (int pixel = 0; pixel < pixels; ++pixel) {
    std::uint8_t* color = output + pixel * 4;
    const int scale = (color[2] >> 3) + 1;
    color[0] = static_cast<std::uint8_t>((color[0] - 128) / scale + 128);
    color[1] = static_cast<std::uint8_t>((color[1] - 128) / scale + 128);
    color[2] = 0;
  }
}
