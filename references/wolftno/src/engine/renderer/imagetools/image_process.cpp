#include "engine/renderer/imagetools/image_process.h"

#include "engine/renderer/imagetools/filter/mipmap.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

void* AllocateImage(std::size_t size, const char* location) {
  return idMem::Singleton()->AllocWithLocation(
      location, size, TAG_IMAGE, false, ALIGN_16, HEAP_DEFAULTHEAP);
}

void FreeImage(void* data) {
  idMem::Singleton()->Free(data, ALIGN_16);
}

std::uint8_t FloatToByte(float value) {
  const int integer = static_cast<int>(value);
  return static_cast<std::uint8_t>(std::clamp(integer, 0, 255));
}

constexpr int kCubeAxes[3][3] = {{0, 1, 2}, {1, 0, 2}, {2, 0, 1}};

}  // namespace

std::uint8_t* R_ResampleTexture(const std::uint8_t* input, int inputWidth,
                                unsigned int inputHeight,
                                unsigned int outputWidth, int outputHeight) {
  auto* firstOffsets = static_cast<unsigned int*>(AllocateImage(
      0x10000, "image_process.cpp:R_ResampleTexture:firstOffsets"));
  auto* secondOffsets = static_cast<unsigned int*>(AllocateImage(
      0x10000, "image_process.cpp:R_ResampleTexture:secondOffsets"));
  auto* output = static_cast<std::uint8_t*>(AllocateImage(
      4ULL * outputHeight * outputWidth,
      "image_process.cpp:R_ResampleTexture:output"));

  const unsigned int fractionStep =
      static_cast<unsigned int>((inputWidth << 16) /
                                static_cast<int>(outputWidth));
  unsigned int fraction = fractionStep >> 2;
  for (unsigned int x = 0; x < outputWidth; ++x) {
    firstOffsets[x] = 4 * (fraction >> 16);
    fraction += fractionStep;
  }
  fraction = (fractionStep >> 1) + (fractionStep >> 2);
  for (unsigned int x = 0; x < outputWidth; ++x) {
    secondOffsets[x] = 4 * (fraction >> 16);
    fraction += fractionStep;
  }

  for (int y = 0; y < outputHeight; ++y) {
    const auto* row0 =
        input + 4 * inputWidth * static_cast<int>(
                                     ((static_cast<float>(y) + 0.25f) *
                                      static_cast<float>(inputHeight)) /
                                     static_cast<float>(outputHeight));
    const auto* row1 =
        input + 4 * inputWidth * static_cast<int>(
                                     ((static_cast<float>(y) + 0.75f) *
                                      static_cast<float>(inputHeight)) /
                                     static_cast<float>(outputHeight));
    std::uint8_t* destination = output + 4ULL * outputWidth * y;
    for (unsigned int x = 0; x < outputWidth; ++x) {
      const std::uint8_t* samples[4] = {
          row0 + firstOffsets[x], row0 + secondOffsets[x],
          row1 + firstOffsets[x], row1 + secondOffsets[x]};
      for (int channel = 0; channel < 4; ++channel) {
        destination[4 * x + channel] = static_cast<std::uint8_t>(
            (samples[0][channel] + samples[1][channel] +
             samples[2][channel] + samples[3][channel]) >>
            2);
      }
    }
  }

  FreeImage(firstOffsets);
  FreeImage(secondOffsets);
  return output;
}

void R_HorizontalFlip(std::uint8_t* data, int width, int height) {
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width / 2; ++x) {
      auto* left = reinterpret_cast<std::uint32_t*>(data) + y * width + x;
      auto* right =
          reinterpret_cast<std::uint32_t*>(data) + y * width + width - 1 - x;
      std::swap(*left, *right);
    }
  }
}

void R_VerticalFlip(std::uint8_t* data, int width, int height) {
  for (int x = 0; x < width; ++x) {
    for (int y = 0; y < height / 2; ++y) {
      auto* top = reinterpret_cast<std::uint32_t*>(data) + y * width + x;
      auto* bottom = reinterpret_cast<std::uint32_t*>(data) +
                     (height - 1 - y) * width + x;
      std::swap(*top, *bottom);
    }
  }
}

void R_RotatePic(std::uint8_t* data, int width) {
  const std::size_t byteCount = 4ULL * width * width;
  auto* temporary = static_cast<std::uint32_t*>(
      AllocateImage(byteCount, "image_process.cpp:R_RotatePic"));
  const auto* source = reinterpret_cast<const std::uint32_t*>(data);
  for (int y = 0; y < width; ++y) {
    for (int x = 0; x < width; ++x)
      temporary[y * width + x] = source[x * width + y];
  }
  std::memmove(data, temporary, byteCount);
  FreeImage(temporary);
}

std::uint8_t* R_Dropsample(const std::uint8_t* input, int inputWidth,
                           int inputHeight, unsigned int outputWidth,
                           int outputHeight) {
  auto* output = static_cast<std::uint8_t*>(AllocateImage(
      4ULL * outputHeight * outputWidth,
      "image_process.cpp:R_Dropsample"));
  for (int y = 0; y < outputHeight; ++y) {
    const int sourceY = static_cast<int>(
        ((static_cast<float>(y) + 0.25f) * static_cast<float>(inputHeight)) /
        static_cast<float>(outputHeight));
    for (unsigned int x = 0; x < outputWidth; ++x) {
      const int sourceX = static_cast<int>(x * inputWidth / outputWidth);
      const std::uint8_t* source =
          input + 4 * (sourceY * inputWidth + sourceX);
      std::memcpy(output + 4 * (y * outputWidth + x), source, 4);
    }
  }
  return output;
}

void R_SetAlphaNormalDivergence(std::uint8_t* data, int width, int height) {
  auto* divergence = static_cast<std::uint8_t*>(AllocateImage(
      static_cast<std::size_t>(width) * height,
      "image_process.cpp:R_SetAlphaNormalDivergence"));
  const float smallestNormal = 1.17549435e-38f;
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::uint8_t* centerPixel = data + 4 * (y * width + x);
      float center[3] = {centerPixel[0] * (2.0f / 255.0f) - 1.0f,
                         centerPixel[1] * (2.0f / 255.0f) - 1.0f,
                         centerPixel[2] * (2.0f / 255.0f) - 1.0f};
      const float centerScale =
          1.0f / std::sqrt(std::max(center[0] * center[0] +
                                        center[1] * center[1] +
                                        center[2] * center[2],
                                    smallestNormal));
      for (float& component : center)
        component *= centerScale;

      float minimumDot = 1.0f;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          if (dx == 0 && dy == 0)
            continue;
          const int sampleX = (x + dx) & (width - 1);
          const int sampleY = (y + dy) & (height - 1);
          const std::uint8_t* sample = data + 4 * (sampleY * width + sampleX);
          float normal[3] = {sample[0] * (2.0f / 255.0f) - 1.0f,
                             sample[1] * (2.0f / 255.0f) - 1.0f,
                             sample[2] * (2.0f / 255.0f) - 1.0f};
          const float scale =
              1.0f / std::sqrt(std::max(normal[0] * normal[0] +
                                            normal[1] * normal[1] +
                                            normal[2] * normal[2],
                                        smallestNormal));
          const float dot = (normal[0] * scale) * center[0] +
                            (normal[1] * scale) * center[1] +
                            (normal[2] * scale) * center[2];
          minimumDot = std::min(minimumDot, dot);
        }
      }
      divergence[y * width + x] =
          FloatToByte(std::max(minimumDot, 0.0f) * 255.0f);
    }
  }

  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      int minimum = 255;
      for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
          const int sampleX = (x + dx) & (width - 1);
          const int sampleY = (y + dy) & (height - 1);
          minimum = std::min(minimum,
                             static_cast<int>(divergence[sampleY * width +
                                                         sampleX]));
        }
      }
      data[4 * (y * width + x) + 3] = static_cast<std::uint8_t>(minimum);
    }
  }
  FreeImage(divergence);
}

std::uint8_t* R_MipMap(const std::uint8_t* input, int width, int height) {
  const int mipWidth = std::max(width >> 1, 1);
  const int mipHeight = std::max(height >> 1, 1);
  auto* output = static_cast<std::uint8_t*>(AllocateImage(
      4ULL * mipWidth * mipHeight, "image_process.cpp:R_MipMap"));
  idMipMap::CreateSingleMip(input, width, height, output);
  return output;
}

std::uint8_t* R_CubeTexel(int size, std::uint8_t** data, idVec3* direction) {
  const float x = direction->x;
  const float y = direction->y;
  const float z = direction->z;
  if (x == 0.0f && y == 0.0f && z == 0.0f)
    return data[0];

  const float absoluteX = std::fabs(x);
  const float absoluteY = std::fabs(y);
  const float absoluteZ = std::fabs(z);
  int face;
  float s;
  float t;
  float major;
  if (absoluteX >= absoluteY && absoluteX >= absoluteZ) {
    major = absoluteX;
    if (x >= 0.0f) {
      face = 0;
      s = -z;
      t = -y;
    } else {
      face = 1;
      s = z;
      t = -y;
    }
  } else if (absoluteY >= absoluteZ) {
    major = absoluteY;
    if (y >= 0.0f) {
      face = 2;
      s = x;
      t = z;
    } else {
      face = 3;
      s = x;
      t = -z;
    }
  } else {
    major = absoluteZ;
    if (z >= 0.0f) {
      face = 4;
      s = x;
      t = -y;
    } else {
      face = 5;
      s = -x;
      t = -y;
    }
  }

  int pixelS = static_cast<int>(((s / major) * 0.5f + 0.5f) * size);
  int pixelT = static_cast<int>(((t / major) * 0.5f + 0.5f) * size);
  pixelS = std::clamp(pixelS, 0, size - 1);
  pixelT = std::clamp(pixelT, 0, size - 1);
  return data[face] + 4 * (pixelT * size + pixelS);
}

void R_FilterCube(int size, std::uint8_t** data, int filterSize) {
  const std::size_t faceBytes = 4ULL * size * size;
  std::uint8_t* copy[6];
  for (int face = 0; face < 6; ++face) {
    copy[face] = static_cast<std::uint8_t*>(AllocateImage(
        faceBytes, "image_process.cpp:R_FilterCube"));
    std::memmove(copy[face], data[face], faceBytes);
  }

  for (const auto& axes : kCubeAxes) {
    for (int sign = -1; sign <= 1; sign += 2) {
      for (int y = 0; y < size; ++y) {
        const float centerY = ((y + 0.5f) * 2.0f / size) - 1.0f;
        for (int x = 0; x < size; ++x) {
          int sums[4] = {};
          int sampleCount = 0;
          for (int dy = -filterSize; dy <= filterSize; ++dy) {
            for (int dx = -filterSize; dx <= filterSize; ++dx) {
              idVec3 direction;
              direction[axes[0]] = static_cast<float>(sign);
              direction[axes[1]] =
                  ((y + dy + 0.5f) * 2.0f / size) - 1.0f;
              direction[axes[2]] =
                  ((x + dx + 0.5f) * 2.0f / size) - 1.0f;
              const std::uint8_t* sample = R_CubeTexel(size, copy, &direction);
              for (int channel = 0; channel < 4; ++channel)
                sums[channel] += sample[channel];
              ++sampleCount;
            }
          }
          idVec3 destinationDirection;
          destinationDirection[axes[0]] = static_cast<float>(sign);
          destinationDirection[axes[1]] = centerY;
          destinationDirection[axes[2]] =
              ((x + 0.5f) * 2.0f / size) - 1.0f;
          std::uint8_t* destination =
              R_CubeTexel(size, data, &destinationDirection);
          for (int channel = 0; channel < 4; ++channel)
            destination[channel] =
                static_cast<std::uint8_t>(sums[channel] / sampleCount);
        }
      }
    }
  }

  for (std::uint8_t* face : copy)
    FreeImage(face);
}

void R_BlendCubeBorders(int size, std::uint8_t** data) {
  constexpr float outside = 1.0001f;
  for (const auto& axes : kCubeAxes) {
    for (int sign = -1; sign <= 1; sign += 2) {
      for (int index = 0; index < size; ++index) {
        const float coordinate = ((index + 0.5f) * 2.0f / size) - 1.0f;
        for (int edge = 0; edge < 4; ++edge) {
          idVec3 firstDirection;
          firstDirection[axes[0]] = static_cast<float>(sign);
          idVec3 secondDirection = firstDirection;
          if (edge < 2) {
            const float edgeSign = edge == 0 ? 1.0f : -1.0f;
            firstDirection[axes[1]] = edgeSign;
            firstDirection[axes[2]] = coordinate;
            secondDirection = firstDirection;
            secondDirection[axes[1]] = edgeSign > 0.0f ? outside : -outside;
          } else {
            const float edgeSign = edge == 2 ? 1.0f : -1.0f;
            firstDirection[axes[2]] = edgeSign;
            firstDirection[axes[1]] = coordinate;
            secondDirection = firstDirection;
            secondDirection[axes[2]] = edgeSign > 0.0f ? outside : -outside;
          }
          std::uint8_t* first = R_CubeTexel(size, data, &firstDirection);
          std::uint8_t* second = R_CubeTexel(size, data, &secondDirection);
          for (int channel = 0; channel < 4; ++channel) {
            const std::uint8_t average = static_cast<std::uint8_t>(
                (first[channel] + second[channel]) >> 1);
            first[channel] = average;
            second[channel] = average;
          }
        }
      }
    }
  }
}
