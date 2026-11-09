#include "engine/renderer/jobs/transcode/image/dxt/dxtencoder.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

constexpr int kInsetColorShift = 4;
constexpr int kInsetAlphaShift = 5;
constexpr std::uint8_t kColor565Mask5 = 0xF8;
constexpr std::uint8_t kColor565Mask6 = 0xFC;

// The retail object contains 77 compiler-generated initializers for SIMD
// splats and masks. In the reconstructed portable body those source constants
// reduce to these compile-time values, so no startup initializer functions are
// required.
constexpr int kColorRange = 3;
constexpr int kYCoCgCenter = 128;

std::uint16_t ColorTo565(const std::uint8_t* color) {
  return static_cast<std::uint16_t>(((color[0] >> 3) << 11) |
                                    ((color[1] >> 2) << 5) |
                                    (color[2] >> 3));
}

std::uint16_t ColorTo565(std::uint8_t red, std::uint8_t green,
                         std::uint8_t blue) {
  return static_cast<std::uint16_t>(((red >> 3) << 11) |
                                    ((green >> 2) << 5) | (blue >> 3));
}

void WriteLittle16(std::uint8_t*& output, std::uint16_t value) {
  output[0] = static_cast<std::uint8_t>(value);
  output[1] = static_cast<std::uint8_t>(value >> 8);
  output += 2;
}

void WriteLittle32(std::uint8_t*& output, std::uint32_t value) {
  output[0] = static_cast<std::uint8_t>(value);
  output[1] = static_cast<std::uint8_t>(value >> 8);
  output[2] = static_cast<std::uint8_t>(value >> 16);
  output[3] = static_cast<std::uint8_t>(value >> 24);
  output += 4;
}

void ExtractBlock(const std::uint8_t* input, int width,
                  std::uint8_t block[64]) {
  for (int row = 0; row < 4; ++row)
    std::memcpy(block + row * 16, input + row * width * 4, 16);
}

void GetMinMaxBBox(const std::uint8_t* block, std::uint8_t* minimum,
                   std::uint8_t* maximum) {
  for (int channel = 0; channel < 4; ++channel) {
    minimum[channel] = 255;
    maximum[channel] = 0;
  }
  for (int pixel = 0; pixel < 16; ++pixel) {
    for (int channel = 0; channel < 4; ++channel) {
      minimum[channel] = std::min(minimum[channel], block[pixel * 4 + channel]);
      maximum[channel] = std::max(maximum[channel], block[pixel * 4 + channel]);
    }
  }
}

void InsetColorsBBox(std::uint8_t* minimum, std::uint8_t* maximum) {
  for (int channel = 0; channel < 4; ++channel) {
    const int shift = channel == 3 ? kInsetAlphaShift : kInsetColorShift;
    const int inset = (maximum[channel] - minimum[channel]) >> shift;
    minimum[channel] = static_cast<std::uint8_t>(minimum[channel] + inset);
    maximum[channel] = static_cast<std::uint8_t>(maximum[channel] - inset);
  }
}

void InsetYCoCgBBox(std::uint8_t* minimum, std::uint8_t* maximum) {
  int mini[4]{};
  int maxi[4]{};
  for (int channel : {0, 1, 3}) {
    const int shift = channel == 3 ? kInsetAlphaShift : kInsetColorShift;
    const int inset = maximum[channel] - minimum[channel] -
                      ((1 << (shift - 1)) - 1);
    mini[channel] = ((minimum[channel] << shift) + inset) >> shift;
    maxi[channel] = ((maximum[channel] << shift) - inset) >> shift;
    mini[channel] = std::max(mini[channel], 0);
    maxi[channel] = std::min(maxi[channel], 255);
  }
  minimum[0] = static_cast<std::uint8_t>((mini[0] & kColor565Mask5) |
                                         (mini[0] >> 5));
  minimum[1] = static_cast<std::uint8_t>((mini[1] & kColor565Mask6) |
                                         (mini[1] >> 6));
  minimum[3] = static_cast<std::uint8_t>(mini[3]);
  maximum[0] = static_cast<std::uint8_t>((maxi[0] & kColor565Mask5) |
                                         (maxi[0] >> 5));
  maximum[1] = static_cast<std::uint8_t>((maxi[1] & kColor565Mask6) |
                                         (maxi[1] >> 6));
  maximum[3] = static_cast<std::uint8_t>(maxi[3]);
}

void SelectYCoCgDiagonal(const std::uint8_t* block, std::uint8_t* minimum,
                         std::uint8_t* maximum) {
  const int midpoint0 = (minimum[0] + maximum[0] + 1) >> 1;
  const int midpoint1 = (minimum[1] + maximum[1] + 1) >> 1;
  int side = 0;
  for (int pixel = 0; pixel < 16; ++pixel) {
    const bool side0 = block[pixel * 4] >= midpoint0;
    const bool side1 = block[pixel * 4 + 1] >= midpoint1;
    side += side0 != side1;
  }
  if (side > 8 && minimum[0] != maximum[0])
    std::swap(minimum[1], maximum[1]);
}

void InsetNormalsBBoxDXT5(std::uint8_t* minimum, std::uint8_t* maximum) {
  const int alphaInset = maximum[3] - minimum[3] -
                         ((1 << (kInsetAlphaShift - 1)) - 1);
  const int greenInset = maximum[1] - minimum[1] -
                         ((1 << (kInsetColorShift - 1)) - 1);
  int minimumAlpha = ((minimum[3] << kInsetAlphaShift) + alphaInset) >>
                     kInsetAlphaShift;
  int minimumGreen = ((minimum[1] << kInsetColorShift) + greenInset) >>
                     kInsetColorShift;
  int maximumAlpha = ((maximum[3] << kInsetAlphaShift) - alphaInset) >>
                     kInsetAlphaShift;
  int maximumGreen = ((maximum[1] << kInsetColorShift) - greenInset) >>
                     kInsetColorShift;
  minimumAlpha = std::clamp(minimumAlpha, 0, 255);
  minimumGreen = std::clamp(minimumGreen, 0, 255);
  maximumAlpha = std::clamp(maximumAlpha, 0, 255);
  maximumGreen = std::clamp(maximumGreen, 0, 255);
  minimum[3] = static_cast<std::uint8_t>(minimumAlpha);
  minimum[1] = static_cast<std::uint8_t>((minimumGreen & kColor565Mask6) |
                                         (minimumGreen >> 6));
  maximum[3] = static_cast<std::uint8_t>(maximumAlpha);
  maximum[1] = static_cast<std::uint8_t>((maximumGreen & kColor565Mask6) |
                                         (maximumGreen >> 6));
}

std::uint32_t SelectFourColorIndices(const std::uint8_t* block,
                                     const std::uint8_t colors[4][4],
                                     int channels) {
  std::uint32_t result = 0;
  for (int pixel = 15; pixel >= 0; --pixel) {
    int distance[4]{};
    for (int index = 0; index < 4; ++index) {
      for (int channel = 0; channel < channels; ++channel) {
        distance[index] += std::abs(static_cast<int>(colors[index][channel]) -
                                    block[pixel * 4 + channel]);
      }
    }
    const bool b0 = distance[0] > distance[3];
    const bool b1 = distance[1] > distance[2];
    const bool b2 = distance[0] > distance[2];
    const bool b3 = distance[1] > distance[3];
    const bool b4 = distance[2] > distance[3];
    const int x0 = b1 && b2;
    const int x1 = b0 && b3;
    const int x2 = b0 && b4;
    result |= static_cast<std::uint32_t>(x2 | ((x0 | x1) << 1))
              << (pixel * 2);
  }
  return result;
}

}  // namespace

void idDxtEncoder::EmitColorIndices_SSE2(const std::uint8_t* colorBlock,
                                         const std::uint8_t* minColor,
                                         const std::uint8_t* maxColor) {
  EmitColorIndices(colorBlock, minColor, maxColor);
}

void idDxtEncoder::EmitColorAlphaIndices_SSE2(
    const std::uint8_t* colorBlock, const std::uint8_t* minColor,
    const std::uint8_t* maxColor) {
  std::uint16_t colors[4][4]{};
  colors[0][0] = (minColor[0] & kColor565Mask5) | (minColor[0] >> 5);
  colors[0][1] = (minColor[1] & kColor565Mask6) | (minColor[1] >> 6);
  colors[0][2] = (minColor[2] & kColor565Mask5) | (minColor[2] >> 5);
  colors[1][0] = (maxColor[0] & kColor565Mask5) | (maxColor[0] >> 5);
  colors[1][1] = (maxColor[1] & kColor565Mask6) | (maxColor[1] >> 6);
  colors[1][2] = (maxColor[2] & kColor565Mask5) | (maxColor[2] >> 5);
  for (int channel = 0; channel < 3; ++channel)
    colors[2][channel] = (colors[0][channel] + colors[1][channel]) / 2;

  std::uint32_t result = 0;
  for (int pixel = 15; pixel >= 0; --pixel) {
    int distance[3]{};
    for (int index = 0; index < 3; ++index) {
      for (int channel = 0; channel < 3; ++channel) {
        distance[index] += std::abs(static_cast<int>(colors[index][channel]) -
                                    colorBlock[pixel * 4 + channel]);
      }
    }
    const unsigned int b0 = distance[2] > distance[0];
    const unsigned int b1 = distance[2] > distance[1];
    const unsigned int b2 = distance[1] > distance[0];
    const unsigned int transparent = colorBlock[pixel * 4 + 3] < 128;
    const unsigned int index =
        (((b0 & b1) | transparent) << 1) |
        ((b2 ^ b1) | transparent);
    result |= index << (pixel * 2);
  }
  WriteLittle32(outData, result);
}

void idDxtEncoder::EmitCoCgIndices_SSE2(const std::uint8_t* colorBlock,
                                        const std::uint8_t* minColor,
                                        const std::uint8_t* maxColor) {
  std::uint8_t colors[4][4]{};
  for (int channel = 0; channel < 3; ++channel) {
    colors[0][channel] = maxColor[channel];
    colors[1][channel] = minColor[channel];
    colors[2][channel] = static_cast<std::uint8_t>(
        (2 * maxColor[channel] + minColor[channel]) / 3);
    colors[3][channel] = static_cast<std::uint8_t>(
        (maxColor[channel] + 2 * minColor[channel]) / 3);
  }
  WriteLittle32(outData, SelectFourColorIndices(colorBlock, colors, 3));
}

void idDxtEncoder::EmitAlphaIndices_SSE2(const __m128i* block,
                                         unsigned int minAlpha,
                                         unsigned int maxAlpha) {
  EmitAlphaIndices(reinterpret_cast<const std::uint8_t*>(block), 3,
                   static_cast<std::uint8_t>(minAlpha),
                   static_cast<std::uint8_t>(maxAlpha));
}

void idDxtEncoder::EmitAlphaIndices_SSE2(const __m128i* block,
                                         unsigned int channelBitOffset,
                                         unsigned int minAlpha,
                                         int maxAlpha) {
  assert((channelBitOffset & 7U) == 0 && channelBitOffset <= 24);
  EmitAlphaIndices(reinterpret_cast<const std::uint8_t*>(block),
                   static_cast<int>(channelBitOffset >> 3),
                   static_cast<std::uint8_t>(minAlpha),
                   static_cast<std::uint8_t>(maxAlpha));
}

void idDxtEncoder::CompressImageDXT1Fast_SSE2(const std::uint8_t* input,
                                              std::uint8_t* output,
                                              int imageWidth,
                                              int imageHeight) {
  assert(imageWidth >= 4 && (imageWidth & 3) == 0);
  assert(imageHeight >= 4 && (imageHeight & 3) == 0);
  width = imageWidth;
  height = imageHeight;
  outData = output;
  alignas(16) std::uint8_t block[64];
  std::uint8_t minimum[4];
  std::uint8_t maximum[4];
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      ExtractBlock(input + x * 4, width, block);
      GetMinMaxBBox(block, minimum, maximum);
      InsetColorsBBox(minimum, maximum);
      WriteLittle16(outData, ColorTo565(maximum));
      WriteLittle16(outData, ColorTo565(minimum));
      EmitColorIndices_SSE2(block, minimum, maximum);
    }
    outData += dstPadding;
    input += width * 16 + srcPadding;
  }
}

void idDxtEncoder::CompressImageDXT1AlphaFast_SSE2(
    const std::uint8_t* input, std::uint8_t* output, int imageWidth,
    int imageHeight) {
  assert(imageWidth >= 4 && (imageWidth & 3) == 0);
  assert(imageHeight >= 4 && (imageHeight & 3) == 0);
  width = imageWidth;
  height = imageHeight;
  outData = output;
  alignas(16) std::uint8_t block[64];
  std::uint8_t minimum[4];
  std::uint8_t maximum[4];
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      ExtractBlock(input + x * 4, width, block);
      GetMinMaxBBox(block, minimum, maximum);
      const std::uint8_t minimumAlpha = minimum[3];
      InsetColorsBBox(minimum, maximum);
      if (minimumAlpha >= 128) {
        WriteLittle16(outData, ColorTo565(maximum));
        WriteLittle16(outData, ColorTo565(minimum));
        EmitColorIndices_SSE2(block, minimum, maximum);
      } else {
        WriteLittle16(outData, ColorTo565(minimum));
        WriteLittle16(outData, ColorTo565(maximum));
        EmitColorAlphaIndices_SSE2(block, minimum, maximum);
      }
    }
    outData += dstPadding;
    input += width * 16 + srcPadding;
  }
}

void idDxtEncoder::CompressImageDXT5Fast_SSE2(const std::uint8_t* input,
                                              std::uint8_t* output,
                                              int imageWidth,
                                              int imageHeight) {
  assert(imageWidth >= 4 && (imageWidth & 3) == 0);
  assert(imageHeight >= 4 && (imageHeight & 3) == 0);
  width = imageWidth;
  height = imageHeight;
  outData = output;
  alignas(16) std::uint8_t block[64];
  std::uint8_t minimum[4];
  std::uint8_t maximum[4];
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      ExtractBlock(input + x * 4, width, block);
      GetMinMaxBBox(block, minimum, maximum);
      InsetColorsBBox(minimum, maximum);
      *outData++ = maximum[3];
      *outData++ = minimum[3];
      EmitAlphaIndices_SSE2(reinterpret_cast<const __m128i*>(block),
                            minimum[3], maximum[3]);
      WriteLittle16(outData, ColorTo565(maximum));
      WriteLittle16(outData, ColorTo565(minimum));
      EmitColorIndices_SSE2(block, minimum, maximum);
    }
    outData += dstPadding;
    input += width * 16 + srcPadding;
  }
}

void idDxtEncoder::ScaleYCoCg_SSE2(__m128i* colorBlock,
                                   std::uint8_t* minColor,
                                   std::uint8_t* maxColor) const {
  auto* block = reinterpret_cast<std::uint8_t*>(colorBlock);
  int maximumMagnitude = std::abs(static_cast<int>(minColor[0]) -
                                  kYCoCgCenter);
  maximumMagnitude = std::max(
      maximumMagnitude,
      std::abs(static_cast<int>(minColor[1]) - kYCoCgCenter));
  maximumMagnitude = std::max(
      maximumMagnitude,
      std::abs(static_cast<int>(maxColor[0]) - kYCoCgCenter));
  maximumMagnitude = std::max(
      maximumMagnitude,
      std::abs(static_cast<int>(maxColor[1]) - kYCoCgCenter));
  int scale = 1;
  if (maximumMagnitude <= 63)
    ++scale;
  if (maximumMagnitude <= 31)
    scale += 2;
  minColor[0] = static_cast<std::uint8_t>(
      (static_cast<int>(minColor[0]) - kYCoCgCenter) * scale + kYCoCgCenter);
  minColor[1] = static_cast<std::uint8_t>(
      (static_cast<int>(minColor[1]) - kYCoCgCenter) * scale + kYCoCgCenter);
  minColor[2] = static_cast<std::uint8_t>((scale - 1) << 3);
  maxColor[0] = static_cast<std::uint8_t>(
      (static_cast<int>(maxColor[0]) - kYCoCgCenter) * scale + kYCoCgCenter);
  maxColor[1] = static_cast<std::uint8_t>(
      (static_cast<int>(maxColor[1]) - kYCoCgCenter) * scale + kYCoCgCenter);
  maxColor[2] = static_cast<std::uint8_t>((scale - 1) << 3);
  for (int pixel = 0; pixel < 16; ++pixel) {
    block[pixel * 4] = static_cast<std::uint8_t>(
        (static_cast<int>(block[pixel * 4]) - kYCoCgCenter) * scale +
        kYCoCgCenter);
    block[pixel * 4 + 1] = static_cast<std::uint8_t>(
        (static_cast<int>(block[pixel * 4 + 1]) - kYCoCgCenter) * scale +
        kYCoCgCenter);
  }
}

void idDxtEncoder::CompressYCoCgDXT5Fast_SSE2(const std::uint8_t* input,
                                              std::uint8_t* output,
                                              int imageWidth,
                                              int imageHeight) {
  assert(imageWidth >= 4 && (imageWidth & 3) == 0);
  assert(imageHeight >= 4 && (imageHeight & 3) == 0);
  width = imageWidth;
  height = imageHeight;
  outData = output;
  alignas(16) std::uint8_t block[64];
  std::uint8_t minimum[4];
  std::uint8_t maximum[4];
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      ExtractBlock(input + x * 4, width, block);
      GetMinMaxBBox(block, minimum, maximum);
      ScaleYCoCg_SSE2(reinterpret_cast<__m128i*>(block), minimum, maximum);
      InsetYCoCgBBox(minimum, maximum);
      SelectYCoCgDiagonal(block, minimum, maximum);
      *outData++ = maximum[3];
      *outData++ = minimum[3];
      EmitAlphaIndices_SSE2(reinterpret_cast<const __m128i*>(block),
                            minimum[3], maximum[3]);
      WriteLittle16(outData, ColorTo565(maximum));
      WriteLittle16(outData, ColorTo565(minimum));
      EmitCoCgIndices_SSE2(block, minimum, maximum);
    }
    outData += dstPadding;
    input += width * 16 + srcPadding;
  }
}

void idDxtEncoder::EmitGreenIndices_SSE2(const __m128i* block,
                                         unsigned int channelBitOffset,
                                         unsigned int minGreen,
                                         int maxGreen) {
  assert(maxGreen >= static_cast<int>(minGreen));
  assert((channelBitOffset & 7U) == 0 && channelBitOffset <= 24);
  const int offset = static_cast<int>(channelBitOffset >> 3);
  const int boundary1 = (5 * maxGreen + static_cast<int>(minGreen) +
                         kColorRange) /
                        (2 * kColorRange);
  const int boundary2 = (3 * maxGreen + 3 * static_cast<int>(minGreen) +
                         kColorRange) /
                        (2 * kColorRange);
  const int boundary3 = (maxGreen + 5 * static_cast<int>(minGreen) +
                         kColorRange) /
                        (2 * kColorRange);
  const auto* bytes = reinterpret_cast<const std::uint8_t*>(block);
  std::uint32_t result = 0;
  for (int pixel = 15; pixel >= 0; --pixel) {
    result <<= 2;
    const int value = bytes[pixel * 4 + offset];
    const int b1 = value >= boundary1;
    const int b2 = value >= boundary2;
    const int b3 = value >= boundary3;
    int index = (4 - b1 - b2 - b3) & 3;
    index ^= 2 > index;
    result |= static_cast<std::uint32_t>(index);
  }
  WriteLittle32(outData, result);
}

void idDxtEncoder::CompressNormalMapDXT5Fast_SSE2(
    std::uint8_t* input, std::uint8_t* output, int imageWidth,
    int imageHeight) {
  assert(imageWidth >= 4 && (imageWidth & 3) == 0);
  assert(imageHeight >= 4 && (imageHeight & 3) == 0);
  width = imageWidth;
  height = imageHeight;
  outData = output;
  alignas(16) std::uint8_t block[64];
  std::uint8_t normal1[4];
  std::uint8_t normal2[4];
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      ExtractBlock(input + x * 4, width, block);
      GetMinMaxBBox(block, normal1, normal2);
      InsetNormalsBBoxDXT5(normal1, normal2);
      *outData++ = normal2[3];
      *outData++ = normal1[3];
      EmitAlphaIndices_SSE2(reinterpret_cast<const __m128i*>(block), 24,
                            normal1[3], normal2[3]);
      WriteLittle16(outData, ColorTo565(block[0], normal2[1], block[2]));
      WriteLittle16(outData, ColorTo565(block[0], normal1[1], block[2]));
      EmitGreenIndices_SSE2(reinterpret_cast<const __m128i*>(block), 8,
                            normal1[1], normal2[1]);
    }
    outData += dstPadding;
    input += width * 16 + srcPadding;
  }
}
