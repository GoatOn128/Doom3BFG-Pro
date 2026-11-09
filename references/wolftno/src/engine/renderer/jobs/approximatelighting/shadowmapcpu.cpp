#include "engine/renderer/jobs/approximatelighting/shadowmapcpu.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <immintrin.h>

float shadowMapCPU_t::ShadowSample(const idVec3& worldPoint) const {
  if (!useShadowMap || map == nullptr) {
    return 1.0f;
  }

  const float projected[4] = {
      (worldPoint.y * mvp.m[1] + worldPoint.x * mvp.m[0]) +
          worldPoint.z * mvp.m[2] + mvp.m[3],
      (worldPoint.x * mvp.m[4] + worldPoint.y * mvp.m[5]) +
          worldPoint.z * mvp.m[6] + mvp.m[7],
      (worldPoint.x * mvp.m[8] + worldPoint.y * mvp.m[9]) +
          worldPoint.z * mvp.m[10] + mvp.m[11],
      (worldPoint.x * mvp.m[12] + worldPoint.y * mvp.m[13]) +
          worldPoint.z * mvp.m[14] + mvp.m[15]};

  int coords[2];
  float fractions[2][2];
  for (int axis = 0; axis < 2; ++axis) {
    const float coordinate = width * projected[axis] + coordinateBias;
    const float integral = std::floor(coordinate);
    coords[axis] = static_cast<int>(integral);
    fractions[axis][1] = coordinate - integral;
    if (coords[axis] < 0) {
      coords[axis] = 0;
      fractions[axis][1] = 0.0f;
    } else if (coords[axis] >= width - 1) {
      coords[axis] = width - 2;
      fractions[axis][1] = 1.0f;
    }
    fractions[axis][0] = 1.0f - fractions[axis][1];
  }

  const int sampleOffsets[4] = {
      coords[0] + coords[1] * width,
      coords[0] + 1 + coords[1] * width,
      coords[0] + (coords[1] + 1) * width,
      coords[0] + 1 + (coords[1] + 1) * width};
  float projectedDepth = projected[2];
  if (projectedDepth > 1.0f) {
    projectedDepth = 1.0f;
  } else if (projectedDepth < 0.0f) {
    projectedDepth = 0.0f;
  }
  const float depth = projectedDepth * 65535.0f + depthBias;
  const std::uint32_t sampleDepth =
      static_cast<std::uint32_t>(static_cast<int>(depth));
  const float equalDepthWeight = 1.0f - (depth - std::floor(depth));

  float shadow = 0.0f;
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y) {
      const std::uint32_t mapDepth =
          map[std::abs(sampleOffsets[y * 2 + x])];
      const float weight = fractions[1][y] * fractions[0][x];
      if (mapDepth > sampleDepth) {
        shadow += weight;
      } else if (mapDepth == sampleDepth) {
        shadow += equalDepthWeight * weight;
      }
    }
  }
  return shadow;
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
