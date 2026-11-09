#include "engine/renderer/jobs/transcode/image/dxt/dxtencoder.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

std::uint16_t ColorTo565(const std::uint8_t* color) {
  return static_cast<std::uint16_t>(((color[0] >> 3) << 11) |
                                    ((color[1] >> 2) << 5) |
                                    (color[2] >> 3));
}

void ColorFrom565(std::uint16_t packed, std::uint8_t* color) {
  const int red = (packed >> 11) & 31;
  const int green = (packed >> 5) & 63;
  const int blue = packed & 31;
  color[0] = static_cast<std::uint8_t>((red << 3) | (red >> 2));
  color[1] = static_cast<std::uint8_t>((green << 2) | (green >> 4));
  color[2] = static_cast<std::uint8_t>((blue << 3) | (blue >> 2));
}

void MakeColorPalette(std::uint16_t color0, std::uint16_t color1,
                      std::uint8_t colors[4][4]) {
  std::memset(colors, 0, 16);
  ColorFrom565(color0, colors[0]);
  ColorFrom565(color1, colors[1]);
  if (color0 > color1) {
    for (int channel = 0; channel < 3; ++channel) {
      colors[2][channel] = static_cast<std::uint8_t>(
          (2 * colors[0][channel] + colors[1][channel]) / 3);
      colors[3][channel] = static_cast<std::uint8_t>(
          (colors[0][channel] + 2 * colors[1][channel]) / 3);
    }
  } else {
    for (int channel = 0; channel < 3; ++channel)
      colors[2][channel] = static_cast<std::uint8_t>(
          (colors[0][channel] + colors[1][channel]) / 2);
  }
}

void MakeAlphaPalette(std::uint8_t alpha0, std::uint8_t alpha1,
                      std::uint8_t alpha[8]) {
  alpha[0] = alpha0;
  alpha[1] = alpha1;
  if (alpha0 > alpha1) {
    alpha[2] = static_cast<std::uint8_t>((6 * alpha0 + alpha1) / 7);
    alpha[3] = static_cast<std::uint8_t>((5 * alpha0 + 2 * alpha1) / 7);
    alpha[4] = static_cast<std::uint8_t>((4 * alpha0 + 3 * alpha1) / 7);
    alpha[5] = static_cast<std::uint8_t>((3 * alpha0 + 4 * alpha1) / 7);
    alpha[6] = static_cast<std::uint8_t>((2 * alpha0 + 5 * alpha1) / 7);
    alpha[7] = static_cast<std::uint8_t>((alpha0 + 6 * alpha1) / 7);
  } else {
    alpha[2] = static_cast<std::uint8_t>((4 * alpha0 + alpha1) / 5);
    alpha[3] = static_cast<std::uint8_t>((3 * alpha0 + 2 * alpha1) / 5);
    alpha[4] = static_cast<std::uint8_t>((2 * alpha0 + 3 * alpha1) / 5);
    alpha[5] = static_cast<std::uint8_t>((alpha0 + 4 * alpha1) / 5);
    alpha[6] = 0;
    alpha[7] = 255;
  }
}

unsigned int ColorDistance(const std::uint8_t* left,
                           const std::uint8_t* right) {
  const int red = static_cast<int>(left[0]) - right[0];
  const int green = static_cast<int>(left[1]) - right[1];
  const int blue = static_cast<int>(left[2]) - right[2];
  return red * red + green * green + blue * blue;
}

void WriteLittle16(std::uint8_t*& output, std::uint16_t value) {
  output[0] = static_cast<std::uint8_t>(value);
  output[1] = static_cast<std::uint8_t>(value >> 8);
  output += 2;
}

void WriteLittle32(std::uint8_t*& output, std::uint32_t value) {
  for (int byte = 0; byte < 4; ++byte)
    output[byte] = static_cast<std::uint8_t>(value >> (byte * 8));
  output += 4;
}

}  // namespace

void idDxtEncoder::WriteTinyColorDXT1(const std::uint8_t* input,
                                      int imageWidth, int imageHeight) {
  const int blocks = ((imageWidth + 3) / 4) * ((imageHeight + 3) / 4);
  if (blocks <= 0)
    return;
  const int stride = 4 * (imageWidth * imageHeight / blocks);
  for (int block = 0; block < blocks; ++block, input += stride) {
    WriteLittle16(outData, ColorTo565(input));
    WriteLittle16(outData, 0);
    WriteLittle32(outData, 0);
  }
}

void idDxtEncoder::WriteTinyColorDXT5(const std::uint8_t* input,
                                      int imageWidth, int imageHeight) {
  const int blocks = ((imageWidth + 3) / 4) * ((imageHeight + 3) / 4);
  if (blocks <= 0)
    return;
  const int stride = 4 * (imageWidth * imageHeight / blocks);
  for (int block = 0; block < blocks; ++block, input += stride) {
    *outData++ = input[3];
    for (int byte = 0; byte < 7; ++byte)
      *outData++ = 0;
    WriteLittle16(outData, ColorTo565(input));
    WriteLittle16(outData, 0);
    WriteLittle32(outData, 0);
  }
}

int idDxtEncoder::GetSquareAlphaError(const std::uint8_t* colorBlock,
                                      int alphaOffset,
                                      std::uint8_t minAlpha,
                                      std::uint8_t maxAlpha,
                                      int lastError) const {
  std::uint8_t alpha[8];
  MakeAlphaPalette(maxAlpha, minAlpha, alpha);
  int error = 0;
  for (int pixel = 0; pixel < 16; ++pixel) {
    unsigned int minimum = std::numeric_limits<unsigned int>::max();
    const int value = colorBlock[pixel * 4 + alphaOffset];
    for (std::uint8_t candidate : alpha) {
      const int difference = value - candidate;
      minimum = std::min(minimum,
                         static_cast<unsigned int>(difference * difference));
    }
    error += static_cast<int>(minimum);
    if (error >= lastError)
      break;
  }
  return error;
}

int idDxtEncoder::GetSquareColorsError(const std::uint8_t* colorBlock,
                                       std::uint16_t color0,
                                       std::uint16_t color1,
                                       int lastError) const {
  std::uint8_t colors[4][4];
  MakeColorPalette(color0, color1, colors);
  int error = 0;
  for (int pixel = 0; pixel < 16; ++pixel) {
    unsigned int minimum = std::numeric_limits<unsigned int>::max();
    for (const auto& color : colors)
      minimum = std::min(minimum, ColorDistance(colorBlock + pixel * 4, color));
    error += static_cast<int>(minimum);
    if (error > lastError)
      break;
  }
  return error;
}

int idDxtEncoder::GetSquareNormalYError(const std::uint8_t* colorBlock,
                                        std::uint16_t color0,
                                        std::uint16_t color1, int lastError,
                                        int scale) const {
  std::uint8_t colors[4][4];
  MakeColorPalette(color0, color1, colors);
  int error = 0;
  for (int pixel = 0; pixel < 16; ++pixel) {
    unsigned int minimum = std::numeric_limits<unsigned int>::max();
    const float value = static_cast<float>(colorBlock[pixel * 4 + 1]) /
                        static_cast<float>(scale);
    for (const auto& color : colors) {
      const float candidate =
          static_cast<float>(color[1]) / static_cast<float>(scale);
      const float difference = value - candidate;
      minimum = std::min(minimum,
                         static_cast<unsigned int>(difference * difference));
    }
    error += static_cast<int>(minimum);
    if (error > lastError)
      break;
  }
  return error;
}

int idDxtEncoder::FindColorIndices(const std::uint8_t* colorBlock,
                                   std::uint16_t color0,
                                   std::uint16_t color1,
                                   unsigned int* result) const {
  std::uint8_t colors[4][4];
  MakeColorPalette(color0, color1, colors);
  *result = 0;
  int error = 0;
  for (int pixel = 0; pixel < 16; ++pixel) {
    unsigned int minimum = std::numeric_limits<unsigned int>::max();
    unsigned int best = 0;
    for (unsigned int index = 0; index < 4; ++index) {
      const unsigned int distance =
          ColorDistance(colorBlock + pixel * 4, colors[index]);
      if (distance < minimum) {
        minimum = distance;
        best = index;
      }
    }
    error += static_cast<int>(minimum);
    *result |= best << (pixel * 2);
  }
  return error;
}

int idDxtEncoder::FindAlphaIndices(const std::uint8_t* colorBlock,
                                   int alphaOffset, std::uint8_t alpha0,
                                   std::uint8_t alpha1,
                                   std::uint8_t* result) const {
  std::uint8_t alpha[8];
  MakeAlphaPalette(alpha0, alpha1, alpha);
  std::uint64_t packed = 0;
  int error = 0;
  for (int pixel = 0; pixel < 16; ++pixel) {
    unsigned int minimum = std::numeric_limits<unsigned int>::max();
    unsigned int best = 0;
    const int value = colorBlock[pixel * 4 + alphaOffset];
    for (unsigned int index = 0; index < 8; ++index) {
      const int difference = value - alpha[index];
      const unsigned int distance = difference * difference;
      if (distance < minimum) {
        minimum = distance;
        best = index;
      }
    }
    error += static_cast<int>(minimum);
    packed |= static_cast<std::uint64_t>(best) << (pixel * 3);
  }
  for (int byte = 0; byte < 6; ++byte)
    result[byte] = static_cast<std::uint8_t>(packed >> (byte * 8));
  return error;
}

void idDxtEncoder::EmitColorIndices(const std::uint8_t* colorBlock,
                                    const std::uint8_t* minColor,
                                    const std::uint8_t* maxColor) {
  std::uint16_t colors[4][4]{};
  colors[0][0] = (maxColor[0] & 0xF8) | (maxColor[0] >> 5);
  colors[0][1] = (maxColor[1] & 0xFC) | (maxColor[1] >> 6);
  colors[0][2] = (maxColor[2] & 0xF8) | (maxColor[2] >> 5);
  colors[1][0] = (minColor[0] & 0xF8) | (minColor[0] >> 5);
  colors[1][1] = (minColor[1] & 0xFC) | (minColor[1] >> 6);
  colors[1][2] = (minColor[2] & 0xF8) | (minColor[2] >> 5);
  for (int channel = 0; channel < 3; ++channel) {
    colors[2][channel] =
        static_cast<std::uint16_t>((2 * colors[0][channel] +
                                    colors[1][channel]) /
                                   3);
    colors[3][channel] =
        static_cast<std::uint16_t>((colors[0][channel] +
                                    2 * colors[1][channel]) /
                                   3);
  }
  unsigned int packed = 0;
  for (int pixel = 15; pixel >= 0; --pixel) {
    int distance[4]{};
    for (int index = 0; index < 4; ++index) {
      for (int channel = 0; channel < 3; ++channel)
        distance[index] += std::abs(
            static_cast<int>(colors[index][channel]) -
            colorBlock[pixel * 4 + channel]);
    }
    const bool b0 = distance[0] > distance[3];
    const bool b1 = distance[1] > distance[2];
    const bool b2 = distance[0] > distance[2];
    const bool b3 = distance[1] > distance[3];
    const bool b4 = distance[2] > distance[3];
    const int x0 = b1 && b2;
    const int x1 = b0 && b3;
    const int x2 = b0 && b4;
    packed |= static_cast<unsigned int>(x2 | ((x0 | x1) << 1))
              << (pixel * 2);
  }
  WriteLittle32(outData, packed);
}

void idDxtEncoder::EmitAlphaIndices(const std::uint8_t* colorBlock,
                                    int offset, std::uint8_t minAlpha,
                                    std::uint8_t maxAlpha) {
  const int threshold[7] = {
      (13 * maxAlpha + minAlpha + 7) / 14,
      (11 * maxAlpha + 3 * minAlpha + 7) / 14,
      (9 * maxAlpha + 5 * minAlpha + 7) / 14,
      (7 * maxAlpha + 7 * minAlpha + 7) / 14,
      (5 * maxAlpha + 9 * minAlpha + 7) / 14,
      (3 * maxAlpha + 11 * minAlpha + 7) / 14,
      (maxAlpha + 13 * minAlpha + 7) / 14,
  };
  std::uint64_t packed = 0;
  for (int pixel = 0; pixel < 16; ++pixel) {
    const int value = colorBlock[pixel * 4 + offset];
    int index = 8;
    for (int boundary : threshold)
      index -= value >= boundary;
    index &= 7;
    index ^= 2 > index;
    packed |= static_cast<std::uint64_t>(index) << (pixel * 3);
  }
  for (int byte = 0; byte < 6; ++byte)
    *outData++ = static_cast<std::uint8_t>(packed >> (byte * 8));
}

void idDxtEncoder::InsetYCoCgAlpaBBox(std::uint8_t* minColor,
                                      std::uint8_t* maxColor) const {
  for (int channel = 0; channel < 4; ++channel) {
    const int shift = channel == 3 ? 5 : 4;
    const int inset = static_cast<int>(maxColor[channel]) -
                      minColor[channel] - ((1 << (shift - 1)) - 1);
    int minimum = ((static_cast<int>(minColor[channel]) << shift) + inset) >>
                  shift;
    int maximum = ((static_cast<int>(maxColor[channel]) << shift) - inset) >>
                  shift;
    minimum = std::max(minimum, 0);
    maximum = std::min(maximum, 255);
    if (channel == 0 || channel == 2) {
      minColor[channel] =
          static_cast<std::uint8_t>((minimum & 0xF8) | (minimum >> 5));
      maxColor[channel] =
          static_cast<std::uint8_t>((maximum & 0xF8) | (maximum >> 5));
    } else if (channel == 1) {
      minColor[channel] =
          static_cast<std::uint8_t>((minimum & 0xFC) | (minimum >> 6));
      maxColor[channel] =
          static_cast<std::uint8_t>((maximum & 0xFC) | (maximum >> 6));
    } else {
      minColor[channel] = static_cast<std::uint8_t>(minimum);
      maxColor[channel] = static_cast<std::uint8_t>(maximum);
    }
  }
}

int idDxtEncoder::GetMinMaxColorsHQ(const std::uint8_t* colorBlock,
                                    std::uint8_t* minColor,
                                    std::uint8_t* maxColor,
                                    bool noBlack) const {
  int bboxMin[3] = {255, 255, 255};
  int bboxMax[3] = {0, 0, 0};
  for (int pixel = 0; pixel < 16; ++pixel) {
    for (int channel = 0; channel < 3; ++channel) {
      bboxMin[channel] =
          std::min(bboxMin[channel], static_cast<int>(colorBlock[pixel * 4 + channel]));
      bboxMax[channel] =
          std::max(bboxMax[channel], static_cast<int>(colorBlock[pixel * 4 + channel]));
    }
  }
  bboxMin[0] >>= 3;
  bboxMin[1] >>= 2;
  bboxMin[2] >>= 3;
  bboxMax[0] >>= 3;
  bboxMax[1] >>= 2;
  bboxMax[2] >>= 3;
  int minimumAxisDistance[3];
  for (int channel = 0; channel < 3; ++channel) {
    const int range = bboxMax[channel] - bboxMin[channel];
    minimumAxisDistance[channel] =
        range >= 16 ? range * 3 / 4
                    : range >= 8 ? range * 2 / 4
                                 : range >= 4 ? range / 4 : 0;
  }
  const int channelMaximum[3] = {31, 63, 31};
  for (int channel = 0; channel < 3; ++channel) {
    bboxMin[channel] = std::max(0, bboxMin[channel] - 1);
    bboxMax[channel] = std::min(channelMaximum[channel], bboxMax[channel] + 1);
  }

  int bestError = std::numeric_limits<int>::max();
  std::uint16_t bestMinimum = 0;
  std::uint16_t bestMaximum = 0;
  for (int minR = bboxMin[0]; minR <= bboxMax[0]; ++minR) {
    for (int maxR = bboxMax[0]; maxR >= bboxMin[0]; --maxR) {
      if (std::abs(minR - maxR) < minimumAxisDistance[0])
        continue;
      for (int minG = bboxMin[1]; minG <= bboxMax[1]; ++minG) {
        for (int maxG = bboxMax[1]; maxG >= bboxMin[1]; --maxG) {
          if (std::abs(minG - maxG) < minimumAxisDistance[1])
            continue;
          for (int minB = bboxMin[2]; minB <= bboxMax[2]; ++minB) {
            for (int maxB = bboxMax[2]; maxB >= bboxMin[2]; --maxB) {
              if (std::abs(minB - maxB) < minimumAxisDistance[2])
                continue;
              std::uint16_t minimum = static_cast<std::uint16_t>(
                  (minR << 11) | (minG << 5) | minB);
              std::uint16_t maximum = static_cast<std::uint16_t>(
                  (maxR << 11) | (maxG << 5) | maxB);
              if (!noBlack) {
                const int reverseError = GetSquareColorsError(
                    colorBlock, maximum, minimum, bestError);
                if (reverseError < bestError) {
                  bestError = reverseError;
                  bestMinimum = minimum;
                  bestMaximum = maximum;
                }
              } else if (minimum <= maximum) {
                std::swap(minimum, maximum);
              }
              const int error =
                  GetSquareColorsError(colorBlock, minimum, maximum, bestError);
              if (error < bestError) {
                bestError = error;
                bestMinimum = minimum;
                bestMaximum = maximum;
              }
            }
          }
        }
      }
    }
  }
  ColorFrom565(bestMinimum, minColor);
  ColorFrom565(bestMaximum, maxColor);
  return bestError;
}

int idDxtEncoder::GetMinMaxNormalYHQ(const std::uint8_t* colorBlock,
                                     std::uint8_t* minColor,
                                     std::uint8_t* maxColor, bool noBlack,
                                     int scale) const {
  int minimumGreen = 63;
  int maximumGreen = 0;
  for (int pixel = 0; pixel < 16; ++pixel) {
    minimumGreen = std::min(minimumGreen,
                            static_cast<int>(colorBlock[pixel * 4 + 1] >> 2));
    maximumGreen = std::max(maximumGreen,
                            static_cast<int>(colorBlock[pixel * 4 + 1] >> 2));
  }
  minimumGreen = std::max(0, minimumGreen - 1);
  maximumGreen = std::min(63, maximumGreen + 1);
  int bestError = std::numeric_limits<int>::max();
  std::uint16_t bestMinimum = 0;
  std::uint16_t bestMaximum = 0;
  for (int minimumValue = minimumGreen; minimumValue <= maximumGreen;
       ++minimumValue) {
    for (int maximumValue = maximumGreen; maximumValue >= minimumGreen;
         --maximumValue) {
      std::uint16_t minimum =
          static_cast<std::uint16_t>(minimumValue << 5);
      std::uint16_t maximum =
          static_cast<std::uint16_t>(maximumValue << 5);
      if (!noBlack) {
        const int reverseError = GetSquareNormalYError(
            colorBlock, maximum, minimum, bestError, scale);
        if (reverseError < bestError) {
          bestError = reverseError;
          bestMinimum = minimum;
          bestMaximum = maximum;
        }
      } else if (minimum <= maximum) {
        std::swap(minimum, maximum);
      }
      const int error = GetSquareNormalYError(colorBlock, minimum, maximum,
                                              bestError, scale);
      if (error < bestError) {
        bestError = error;
        bestMinimum = minimum;
        bestMaximum = maximum;
      }
    }
  }
  ColorFrom565(bestMinimum, minColor);
  ColorFrom565(bestMaximum, maxColor);
  minColor[0] = maxColor[0] = colorBlock[0];
  minColor[2] = maxColor[2] = colorBlock[2];
  return bestError;
}

namespace {

void ExtractBlock(const std::uint8_t* input, int width,
                  std::uint8_t block[64]) {
  for (int row = 0; row < 4; ++row)
    std::memcpy(block + row * 16, input + row * width * 4, 16);
}

int GetMinMaxAlphaHQ(const idDxtEncoder& encoder,
                     const std::uint8_t* block, int offset,
                     std::uint8_t* minimum, std::uint8_t* maximum) {
  int alphaMinimum = 255;
  int alphaMaximum = 0;
  for (int pixel = 0; pixel < 16; ++pixel) {
    alphaMinimum = std::min(alphaMinimum,
                            static_cast<int>(block[pixel * 4 + offset]));
    alphaMaximum = std::max(alphaMaximum,
                            static_cast<int>(block[pixel * 4 + offset]));
  }
  alphaMinimum = alphaMinimum <= 32 ? 0 : alphaMinimum - 32;
  alphaMaximum = alphaMaximum >= 223 ? 255 : alphaMaximum + 32;
  int bestError = std::numeric_limits<int>::max();
  for (int low = alphaMinimum; low <= alphaMaximum; ++low) {
    for (int high = alphaMaximum; high >= low; --high) {
      int error = encoder.GetSquareAlphaError(
          block, offset, static_cast<std::uint8_t>(low),
          static_cast<std::uint8_t>(high), bestError);
      if (error < bestError) {
        bestError = error;
        minimum[offset] = static_cast<std::uint8_t>(low);
        maximum[offset] = static_cast<std::uint8_t>(high);
      }
      error = encoder.GetSquareAlphaError(
          block, offset, static_cast<std::uint8_t>(high),
          static_cast<std::uint8_t>(low), bestError);
      if (error < bestError) {
        bestError = error;
        minimum[offset] = static_cast<std::uint8_t>(low);
        maximum[offset] = static_cast<std::uint8_t>(high);
      }
    }
  }
  return bestError;
}

void SortForNvidia(std::uint8_t* minimum, std::uint8_t* maximum) {
  if (ColorTo565(minimum) > ColorTo565(maximum)) {
    for (int channel = 0; channel < 3; ++channel)
      std::swap(minimum[channel], maximum[channel]);
  }
}

void GetBoundingBox(const std::uint8_t* block, std::uint8_t* minimum,
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

}  // namespace

void idDxtEncoder::CompressImageDXT1HQ(const std::uint8_t* input,
                                       std::uint8_t* output, int imageWidth,
                                       int imageHeight) {
  width = imageWidth;
  height = imageHeight;
  outData = output;
  if ((width > 4 && (width & 3) != 0) ||
      (height > 4 && (height & 3) != 0))
    return;
  if (width < 4 || height < 4) {
    WriteTinyColorDXT1(input, width, height);
    return;
  }
  std::uint8_t block[64];
  std::uint8_t color1[4]{};
  std::uint8_t color2[4]{};
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      ExtractBlock(input + x * 4, width, block);
      GetMinMaxColorsHQ(block, color1, color2, false);
      const std::uint16_t packed1 = ColorTo565(color1);
      const std::uint16_t packed2 = ColorTo565(color2);
      unsigned int indices1;
      unsigned int indices2;
      const int error1 = FindColorIndices(block, packed1, packed2, &indices1);
      const int error2 = FindColorIndices(block, packed2, packed1, &indices2);
      if (error1 < error2) {
        WriteLittle16(outData, packed1);
        WriteLittle16(outData, packed2);
        WriteLittle32(outData, indices1);
      } else {
        WriteLittle16(outData, packed2);
        WriteLittle16(outData, packed1);
        WriteLittle32(outData, indices2);
      }
    }
    outData += dstPadding;
    input += width * 16 + srcPadding;
  }
}

void idDxtEncoder::CompressImageDXT5HQ(const std::uint8_t* input,
                                       std::uint8_t* output, int imageWidth,
                                       int imageHeight) {
  width = imageWidth;
  height = imageHeight;
  outData = output;
  if ((width > 4 && (width & 3) != 0) ||
      (height > 4 && (height & 3) != 0))
    return;
  if (width < 4 || height < 4) {
    WriteTinyColorDXT5(input, width, height);
    return;
  }
  std::uint8_t block[64];
  std::uint8_t color1[4]{};
  std::uint8_t color2[4]{};
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      ExtractBlock(input + x * 4, width, block);
      GetMinMaxColorsHQ(block, color1, color2, true);
      GetMinMaxAlphaHQ(*this, block, 3, color1, color2);
      std::uint8_t indices1[6];
      std::uint8_t indices2[6];
      const int error1 =
          FindAlphaIndices(block, 3, color1[3], color2[3], indices1);
      const int error2 =
          FindAlphaIndices(block, 3, color2[3], color1[3], indices2);
      const bool first = error1 < error2;
      *outData++ = first ? color1[3] : color2[3];
      *outData++ = first ? color2[3] : color1[3];
      const std::uint8_t* alphaIndices = first ? indices1 : indices2;
      for (int byte = 0; byte < 6; ++byte)
        *outData++ = alphaIndices[byte];
      SortForNvidia(color2, color1);
      const std::uint16_t packed1 = ColorTo565(color1);
      const std::uint16_t packed2 = ColorTo565(color2);
      WriteLittle16(outData, packed1);
      WriteLittle16(outData, packed2);
      unsigned int colorIndices;
      FindColorIndices(block, packed1, packed2, &colorIndices);
      WriteLittle32(outData, colorIndices);
    }
    outData += dstPadding;
    input += width * 16 + srcPadding;
  }
}

void idDxtEncoder::CompressNormalMapDXT5HQ(const std::uint8_t* input,
                                           std::uint8_t* output,
                                           int imageWidth,
                                           int imageHeight) {
  width = imageWidth;
  height = imageHeight;
  outData = output;
  if ((width > 4 && (width & 3) != 0) ||
      (height > 4 && (height & 3) != 0))
    return;
  if (width < 4 || height < 4) {
    WriteTinyColorDXT5(input, width, height);
    return;
  }
  std::uint8_t block[64];
  std::uint8_t color1[4]{};
  std::uint8_t color2[4]{};
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      ExtractBlock(input + x * 4, width, block);
      for (int pixel = 0; pixel < 16; ++pixel) {
        block[pixel * 4 + 3] = block[pixel * 4];
        block[pixel * 4] = 0;
        block[pixel * 4 + 2] = 0;
      }
      GetMinMaxNormalYHQ(block, color1, color2, true, 1);
      GetMinMaxAlphaHQ(*this, block, 3, color1, color2);
      std::uint8_t indices1[6];
      std::uint8_t indices2[6];
      const int error1 =
          FindAlphaIndices(block, 3, color1[3], color2[3], indices1);
      const int error2 =
          FindAlphaIndices(block, 3, color2[3], color1[3], indices2);
      const bool first = error1 < error2;
      *outData++ = first ? color1[3] : color2[3];
      *outData++ = first ? color2[3] : color1[3];
      const std::uint8_t* alphaIndices = first ? indices1 : indices2;
      for (int byte = 0; byte < 6; ++byte)
        *outData++ = alphaIndices[byte];
      SortForNvidia(color2, color1);
      const std::uint16_t packed1 = ColorTo565(color1);
      const std::uint16_t packed2 = ColorTo565(color2);
      WriteLittle16(outData, packed1);
      WriteLittle16(outData, packed2);
      unsigned int colorIndices;
      FindColorIndices(block, packed1, packed2, &colorIndices);
      WriteLittle32(outData, colorIndices);
    }
    outData += dstPadding;
    input += width * 16 + srcPadding;
  }
}

void idDxtEncoder::SelectColorsDiagonal(const std::uint8_t* colorBlock,
                                        std::uint8_t* minColor,
                                        std::uint8_t* maxColor) const {
  const int midpoint[3] = {
      (minColor[0] + maxColor[0] + 1) >> 1,
      (minColor[1] + maxColor[1] + 1) >> 1,
      (minColor[2] + maxColor[2] + 1) >> 1,
  };
  int side0 = 0;
  int side1 = 0;
  for (int pixel = 0; pixel < 16; ++pixel) {
    const bool above0 = colorBlock[pixel * 4] >= midpoint[0];
    const bool above1 = colorBlock[pixel * 4 + 1] >= midpoint[1];
    const bool above2 = colorBlock[pixel * 4 + 2] >= midpoint[2];
    side0 += above0 != above1;
    side1 += above1 != above2;
  }
  if (side0 > 8)
    std::swap(minColor[0], maxColor[0]);
  if (side1 > 8)
    std::swap(minColor[2], maxColor[2]);
  if (ColorTo565(minColor) > ColorTo565(maxColor)) {
    for (int channel = 0; channel < 3; ++channel)
      std::swap(minColor[channel], maxColor[channel]);
  }
}

void idDxtEncoder::CompressYCoCgAlphaDXT5Fast(
    const std::uint8_t* input, std::uint8_t* output, int imageWidth,
    int imageHeight) {
  width = imageWidth;
  height = imageHeight;
  outData = output;
  std::uint8_t block[64];
  std::uint8_t minimum[4];
  std::uint8_t maximum[4];
  for (int y = 0; y < height; y += 4) {
    for (int x = 0; x < width; x += 4) {
      ExtractBlock(input + x * 4, width, block);
      for (int pixel = 0; pixel < 16; ++pixel) {
        if (std::abs(static_cast<int>(block[pixel * 4]) - 132) <= 8 &&
            std::abs(static_cast<int>(block[pixel * 4 + 2]) - 132) <= 8 &&
            block[pixel * 4 + 3] < 96) {
          block[pixel * 4] = static_cast<std::uint8_t>(
              (static_cast<int>(block[pixel * 4]) - 132) / 2 + 132);
          block[pixel * 4 + 2] = static_cast<std::uint8_t>(
              (static_cast<int>(block[pixel * 4 + 2]) - 132) / 2 + 132);
        }
      }
      GetBoundingBox(block, minimum, maximum);
      InsetYCoCgAlpaBBox(minimum, maximum);
      SelectColorsDiagonal(block, minimum, maximum);
      *outData++ = maximum[3];
      *outData++ = minimum[3];
      EmitAlphaIndices(block, 3, minimum[3], maximum[3]);
      WriteLittle16(outData, ColorTo565(maximum));
      WriteLittle16(outData, ColorTo565(minimum));
      EmitColorIndices(block, minimum, maximum);
    }
    outData += dstPadding;
    input += width * 16 + srcPadding;
  }
}
