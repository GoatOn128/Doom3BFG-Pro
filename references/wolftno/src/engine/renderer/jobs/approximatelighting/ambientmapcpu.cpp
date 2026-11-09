#include "engine/renderer/jobs/approximatelighting/ambientmapcpu.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <immintrin.h>

namespace {

int TruncateFloatToInt(float value) {
  return _mm_cvtt_ss2si(_mm_set_ss(value));
}

std::uint8_t ClampByte(int value) {
  if (value < 0) {
    return 0;
  }
  if (value > 255) {
    return 255;
  }
  return static_cast<std::uint8_t>(value);
}

std::int64_t DistanceSquared(const ambientSample_t& sample,
                             const std::int64_t corner[3]) {
  const std::int64_t dx =
      static_cast<std::int64_t>(sample.origin[0]) - corner[0];
  const std::int64_t dy =
      static_cast<std::int64_t>(sample.origin[1]) - corner[1];
  const std::int64_t dz =
      static_cast<std::int64_t>(sample.origin[2]) - corner[2];
  return dx * dx + dy * dy + dz * dz;
}

}  // namespace

void ambientMapCPU_t::SamplesForPoint(const idVec3& worldPoint, int* cube,
                                      ambientSample_t* cubeSamples) const {
  if (nodes == nullptr) {
    std::memset(cubeSamples, 0, sizeof(ambientSample_t) * 8);
    cube[0] = -32768;
    cube[1] = -32768;
    cube[2] = -32768;
    cube[3] = 65536;
    return;
  }

  const int point[3] = {TruncateFloatToInt(worldPoint.x),
                        TruncateFloatToInt(worldPoint.y),
                        TruncateFloatToInt(worldPoint.z)};
  int bounds[2][3] = {{-32768, -32768, -32768},
                      {32768, 32768, 32768}};
  int center[3] = {0, 0, 0};
  bool side[3] = {point[0] > 0, point[1] > 0, point[2] > 0};

  const ambientNode_t* node = nodes;
  std::uint16_t child = node->children[(side[0] ? 1 : 0) |
                                       (side[1] ? 2 : 0) |
                                       (side[2] ? 4 : 0)];
  while ((child & 0x8000u) == 0) {
    for (int axis = 0; axis < 3; ++axis) {
      bounds[side[axis] ? 0 : 1][axis] = center[axis];
      center[axis] = (bounds[0][axis] + bounds[1][axis]) / 2;
      side[axis] = point[axis] > center[axis];
    }
    node = &nodes[child];
    child = node->children[(side[0] ? 1 : 0) |
                           (side[1] ? 2 : 0) |
                           (side[2] ? 4 : 0)];
  }

  int sampleIndices[8];
  bool allChildrenAreSamples = true;
  for (const std::uint16_t nodeChild : node->children) {
    if ((nodeChild & 0x8000u) == 0) {
      allChildrenAreSamples = false;
      break;
    }
  }
  if (allChildrenAreSamples) {
    for (int index = 0; index < 8; ++index) {
      sampleIndices[index] = node->children[index] & 0x7FFFu;
    }
  } else {
    const int sampleIndex = child & 0x7FFFu;
    for (int& index : sampleIndices) {
      index = sampleIndex;
    }
    for (int axis = 0; axis < 3; ++axis) {
      bounds[side[axis] ? 0 : 1][axis] = center[axis];
    }
  }

  int sampleCount = 1;
  while (sampleCount < 8 &&
         sampleIndices[sampleCount] != sampleIndices[sampleCount - 1]) {
    ++sampleCount;
  }

  const ambientSample_t* localSamples[8];
  for (int index = 0; index < sampleCount; ++index) {
    const int sampleIndex = sampleIndices[index];
    const int absoluteIndex = sampleIndex < 0 ? -sampleIndex : sampleIndex;
    localSamples[index] = &samples[absoluteIndex];
  }

  std::int64_t corners[8][3];
  std::int64_t closestDistanceSquared[8];
  int closestSamples[8]{};
  for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
    corners[cornerIndex][0] = bounds[(cornerIndex >> 0) & 1][0];
    corners[cornerIndex][1] = bounds[(cornerIndex >> 1) & 1][1];
    corners[cornerIndex][2] = bounds[(cornerIndex >> 2) & 1][2];
    closestDistanceSquared[cornerIndex] =
        DistanceSquared(*localSamples[0], corners[cornerIndex]);
  }

  for (int sampleIndex = 1; sampleIndex < sampleCount; ++sampleIndex) {
    for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
      const std::int64_t distance =
          DistanceSquared(*localSamples[sampleIndex], corners[cornerIndex]);
      if (distance < closestDistanceSquared[cornerIndex]) {
        closestDistanceSquared[cornerIndex] = distance;
        closestSamples[cornerIndex] = sampleIndex;
      }
    }
  }

  for (int cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
    cubeSamples[cornerIndex] = *localSamples[closestSamples[cornerIndex]];
  }
  cube[0] = bounds[0][0];
  cube[1] = bounds[0][1];
  cube[2] = bounds[0][2];
  cube[3] = bounds[1][0] - bounds[0][0];
}

void ambientMapCPU_t::AmbientSample(const idVec3& worldPoint,
                                    ambientColors_t* lighting) const {
  if (useForcedColors) {
    *lighting = forcedColors;
    return;
  }
  if (data == nullptr) {
    std::memset(lighting, 0, sizeof(*lighting));
    return;
  }

  int cube[4];
  ambientSample_t cubeSamples[8];
  for (ambientSample_t& sample : cubeSamples) {
    std::memset(&sample.colors, 0, sizeof(sample.colors));
  }
  SamplesForPoint(worldPoint, cube, cubeSamples);

  const float inverseSize = 1.0f / static_cast<float>(cube[3]);
  const float x = (worldPoint.x - static_cast<float>(cube[0])) * inverseSize;
  const float y = (worldPoint.y - static_cast<float>(cube[1])) * inverseSize;
  const float z = (worldPoint.z - static_cast<float>(cube[2])) * inverseSize;
  const float oneMinusX = 1.0f - x;
  const float oneMinusY = 1.0f - y;
  const float oneMinusZ = 1.0f - z;
  const float x0y0 = oneMinusX * oneMinusY;
  const float x1y0 = oneMinusY * x;
  const float x0y1 = oneMinusX * y;
  const float x1y1 = y * x;
  const float weights[8] = {oneMinusZ * x0y0, x1y0 * oneMinusZ,
                            x0y1 * oneMinusZ, x1y1 * oneMinusZ,
                            x0y0 * z,         x1y0 * z,
                            x0y1 * z,         x1y1 * z};

  for (int direction = 0; direction < 6; ++direction) {
    for (int channel = 0; channel < 3; ++channel) {
      float value =
          static_cast<float>(cubeSamples[0].colors.channels[direction][channel]) *
          weights[0];
      for (int corner = 1; corner < 8; ++corner) {
        value += static_cast<float>(
                     cubeSamples[corner].colors.channels[direction][channel]) *
                 weights[corner];
      }
      lighting->channels[direction][channel] =
          ClampByte(TruncateFloatToInt(value));
    }
  }
}

const __m128 vector_float_abs_mask = _mm_castsi128_ps(
    _mm_setr_epi32(0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF, 0x7FFFFFFF));
const __m128 vector_float_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u)));
const __m128 vector_float_last_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(0, 0, 0, static_cast<int>(0x80000000u)));
const __m128 vector_float_first_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u), 0, 0, 0));
const __m128 vector_float_clear_last = _mm_castsi128_ps(
    _mm_setr_epi32(-1, -1, -1, 0));
