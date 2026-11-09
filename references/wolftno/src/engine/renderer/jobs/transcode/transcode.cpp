#include "engine/renderer/jobs/transcode/transcode.h"

#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"
#include "engine/renderer/jobs/transcode/image/barehdp/barehdp.h"
#include "engine/renderer/jobs/transcode/image/barejxr/barejxr.h"
#include "engine/renderer/jobs/transcode/image/dxt/dxtdecoder.h"
#include "engine/renderer/jobs/transcode/image/dxt/dxtencoder.h"
#include "shared/idlib/parallelism/paralleljoblist.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <immintrin.h>
#include <malloc.h>

namespace {

enum sincWindow_t {
  SINC_WINDOW_HAMMING,
  SINC_WINDOW_BLACKMAN,
  SINC_WINDOW_LANCZOS,
  SINC_WINDOW_KAISER
};

alignas(16) const __m128i vector_int_zero = _mm_setzero_si128();
alignas(16) const __m128i vector_int_one = _mm_set1_epi32(1);
alignas(16) const __m128i vector_int_two = _mm_set1_epi32(2);
alignas(16) const __m128i vector_int_clear_last_three =
    _mm_setr_epi32(-1, 0, 0, 0);
alignas(16) const __m128i vector_int_0_1_2_3 =
    _mm_setr_epi32(0, 1, 2, 3);
alignas(16) const __m128i vector_int_20077 = _mm_set1_epi32(20077);
alignas(16) const __m128i vector_int_12345 = _mm_set1_epi32(12345);
alignas(16) const __m128 vector_float_one = _mm_set1_ps(1.0f);
alignas(16) const __m128 vector_float_2_65535 =
    _mm_set1_ps(2.0f / 65535.0f);
alignas(16) const __m128 vector_float_1_1024 = _mm_set1_ps(1.0f / 1024.0f);
alignas(16) const __m128 vector_float_bicubic_0 =
    _mm_setr_ps(-0.75f, 1.25f, -1.25f, 0.75f);
alignas(16) const __m128 vector_float_bicubic_1 =
    _mm_setr_ps(1.5f, -2.25f, 1.5f, -0.75f);
alignas(16) const __m128 vector_float_bicubic_2 =
    _mm_setr_ps(-0.75f, 0.0f, 0.75f, 0.0f);
alignas(16) const __m128 vector_float_bicubic_3 =
    _mm_setr_ps(0.0f, 1.0f, 0.0f, 0.0f);
alignas(16) const __m128i vector_byte_sharpen_threshold = _mm_set1_epi8(3);
alignas(16) const __m128i vector_short_one = _mm_set1_epi16(1);
alignas(16) const __m128i vector_short_two = _mm_set1_epi16(2);
alignas(16) const __m128i vector_short_15 = _mm_set1_epi16(15);
alignas(16) const __m128i vector_short_20077 = _mm_set1_epi16(20077);
alignas(16) const __m128i vector_short_12345 = _mm_set1_epi16(12345);
alignas(16) const __m128i vector_short_clear_last_7 =
    _mm_setr_epi16(-1, 0, 0, 0, 0, 0, 0, 0);
alignas(16) const __m128i vector_short_0_1_2_3_0_1_2_3 =
    _mm_setr_epi16(0, 1, 2, 3, 0, 1, 2, 3);
alignas(16) const __m128i vector_short_bicubic_x0 =
    _mm_setr_epi16(-6144, -10240, -6144, -10240, 10240, 6144, 10240,
                   6144);
alignas(16) const __m128i vector_short_bicubic_x1 =
    _mm_setr_epi16(6144, 6144, 6144, 6144, -9216, -3072, -9216, -3072);
alignas(16) const __m128i vector_short_bicubic_x2 =
    _mm_setr_epi16(-3072, 3072, -3072, 3072, 0, 0, 0, 0);
alignas(16) const __m128i vector_short_bicubic_x3 =
    _mm_setr_epi16(0, 0, 0, 0, 4096, 0, 4096, 0);
alignas(16) const __m128i vector_short_bicubic_y0 =
    _mm_setr_epi16(-6144, -6144, 10240, 10240, -10240, -10240, 6144,
                   6144);
alignas(16) const __m128i vector_short_bicubic_y1 =
    _mm_setr_epi16(6144, 6144, -9216, -9216, 6144, 6144, -3072, -3072);
alignas(16) const __m128i vector_short_bicubic_y2 =
    _mm_setr_epi16(-3072, -3072, 0, 0, 3072, 3072, 0, 0);
alignas(16) const __m128i vector_short_bicubic_y3 =
    _mm_setr_epi16(0, 0, 4096, 4096, 0, 0, 0, 0);
alignas(16) const __m128i vector_int_round = _mm_set1_epi32(0x80000);

const std::uint8_t mipBlends[12][4] = {
    {255, 0, 0, 128},   {0, 255, 0, 128},   {255, 255, 0, 128},
    {128, 0, 0, 128},   {0, 128, 0, 128},   {128, 128, 0, 128},
    {255, 128, 0, 128}, {128, 64, 0, 128},  {128, 128, 0, 0},
    {255, 255, 255, 255}, {0, 0, 0, 255},   {0, 0, 0, 0}};

const char* const binFont[8] = {
    "                                        ",
    "xxx  x  xxx xxx x x xxx x   xxx xxx xxx ",
    "x x  x    x   x x x x   x     x x x x x ",
    "x x  x  xxx xxx xxx xxx xxx   x xxx xxx ",
    "x x  x  x     x   x   x x x   x x x   x ",
    "xxx  x  xxx xxx   x xxx xxx   x xxx   x ",
    "                                        ",
    "                                        "};
const char* const sourceText[8] = {
    "                                         ",
    "   xxx   xxx  x   x xxxx   xxxx xxxx     ",
    "  x     x   x x   x x   x x     x        ",
    "   xxx  x   x x   x xxxx  x     xxx      ",
    "      x x   x x   x x  x  x     x        ",
    "   xxx   xxx   xxxx x   x  xxxx xxxx     ",
    "                                         ",
    "                                         "};
const char* const layoutText[8] = {
    "                                         ",
    "  x     xxx   x   x  xxx  x   x xxxxx    ",
    "  x    x   x  x   x x   x x   x   x      ",
    "  x    xxxxx   xxx  x   x x   x   x      ",
    "  x    x   x    x   x   x x   x   x      ",
    "  xxx  x   x    x    xxx   xxxx   x      ",
    "                                         ",
    "                                         "};

__declspec(noinline) int SharpenInt(const std::uint8_t* src, int x, int y,
                                    int srcMaskY, int component,
                                    int sharpenScale) {
  const int amount = (sharpenScale >> (8 * component)) & 0xff;
  if (amount == 0) {
    return 0;
  }
  const int center = src[component + 4 * (x + ((y & srcMaskY) << 7))];
  int sum = 0;
  for (int oy = -1; oy <= 1; ++oy) {
    for (int ox = -1; ox <= 1; ++ox) {
      if (ox == 0 && oy == 0) {
        continue;
      }
      const int sample =
          src[component + 4 * (x + ox + (((y + oy) & srcMaskY) << 7))];
      const int delta = center - sample;
      if (std::abs(delta) >= 3) {
        sum += delta;
      }
    }
  }
  return amount * sum;
}

__declspec(noinline) void UpSample16x16_Nearest(
    std::uint8_t* dst, unsigned int dstX, unsigned int dstY,
    unsigned int dstBytePitch, const std::uint8_t* src, unsigned int srcX,
    unsigned int srcY, unsigned int srcMaskY, char upsampleLevel,
    int sharpenScale, int noiseScale) {
  std::uint16_t random[4] = {
      static_cast<std::uint16_t>(dstX + 16 * dstY + 0),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 1),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 2),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 3)};
  for (unsigned int y = 0; y < 16; ++y) {
    const unsigned int sy = (dstY + y + srcY) >> upsampleLevel;
    for (unsigned int x = 0; x < 16; ++x) {
      const unsigned int sx = (dstX + x + srcX) >> upsampleLevel;
      for (int c = 0; c < 4; ++c) {
        random[c] = static_cast<std::uint16_t>(20077 * random[c] + 12345);
        const float amplitude = static_cast<float>((noiseScale >> (8 * c)) & 0xff);
        float value = static_cast<float>(
            src[4 * (sx + ((sy & srcMaskY) << 7)) + c]);
        value += static_cast<float>(random[c]) * (amplitude / 32768.0f) -
                 amplitude;
        if (sharpenScale != 0) {
          value += static_cast<float>(SharpenInt(
                       src, static_cast<int>(sx), static_cast<int>(sy),
                       static_cast<int>(srcMaskY), c, sharpenScale)) /
                   1024.0f;
        }
        const int rounded = static_cast<int>(value + 0.5f);
        dst[(dstY + y) * dstBytePitch + (dstX + x) * 4 + c] =
            static_cast<std::uint8_t>(std::clamp(rounded, 0, 255));
      }
    }
  }
}

__declspec(noinline) void UpSample16x16_Bilinear(
    std::uint8_t* dst, unsigned int dstX, unsigned int dstY,
    unsigned int dstBytePitch, const std::uint8_t* src, unsigned int srcX,
    unsigned int srcY, unsigned int srcMaskY, char upsampleLevel,
    int sharpenScale, int noiseScale) {
  std::uint16_t random[4] = {
      static_cast<std::uint16_t>(dstX + 16 * dstY + 0),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 1),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 2),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 3)};
  const int shift = upsampleLevel + 1;
  const int mask = (2 << upsampleLevel) - 1;
  const float inverse = 1.0f / static_cast<float>(2 << upsampleLevel);
  for (unsigned int y = 0; y < 16; ++y) {
    const unsigned int fy = 2 * (dstY + y + srcY) -
                            (1u << upsampleLevel) + 1;
    const int baseY = static_cast<int>(fy >> shift);
    const float wy[2] = {1.0f - static_cast<float>(fy & mask) * inverse,
                         static_cast<float>(fy & mask) * inverse};
    for (unsigned int x = 0; x < 16; ++x) {
      const unsigned int fx = 2 * (dstX + x + srcX) -
                              (1u << upsampleLevel) + 1;
      const int baseX = static_cast<int>(fx >> shift);
      const float wx[2] = {1.0f - static_cast<float>(fx & mask) * inverse,
                           static_cast<float>(fx & mask) * inverse};
      for (int c = 0; c < 4; ++c) {
        random[c] = static_cast<std::uint16_t>(20077 * random[c] + 12345);
        const float amplitude = static_cast<float>((noiseScale >> (8 * c)) & 0xff);
        float value = static_cast<float>(random[c]) *
                          (amplitude / 32768.0f) -
                      amplitude;
        for (int ky = 0; ky < 2; ++ky) {
          for (int kx = 0; kx < 2; ++kx) {
            const int sampleX = baseX + kx;
            const int sampleY = baseY + ky;
            float sample = static_cast<float>(src[
                4 * (sampleX + ((sampleY & static_cast<int>(srcMaskY)) << 7)) + c]);
            if (sharpenScale != 0) {
              sample += static_cast<float>(SharpenInt(
                            src, sampleX, sampleY,
                            static_cast<int>(srcMaskY), c, sharpenScale)) /
                        1024.0f;
            }
            value += sample * wx[kx] * wy[ky];
          }
        }
        dst[(dstY + y) * dstBytePitch + (dstX + x) * 4 + c] =
            static_cast<std::uint8_t>(
                std::clamp(static_cast<int>(value + 0.5f), 0, 255));
      }
    }
  }
}

__declspec(noinline) void GetQuadraticWeights(float s, float* values) {
  for (int tap = -1; tap <= 1; ++tap) {
    const float distance = std::fabs((static_cast<float>(tap) + 0.5f) - s);
    float weight = 0.0f;
    if (distance < 1.5f) {
      const float square = distance * distance;
      weight = distance > 0.5f
                   ? square * 0.65f - distance * 1.8f + 1.2375f
                   : 0.825f - square * 1.3f;
    }
    *values++ = weight;
  }
}

__declspec(noinline) void UpSample16x16_Biquadratic(
    std::uint8_t* dst, unsigned int dstX, unsigned int dstY,
    unsigned int dstBytePitch, const std::uint8_t* src, unsigned int srcX,
    unsigned int srcY, unsigned int srcMaskY, char upsampleLevel,
    int sharpenScale, int noiseScale) {
  std::uint16_t random[4] = {
      static_cast<std::uint16_t>(dstX + 16 * dstY + 0),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 1),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 2),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 3)};
  const int shift = upsampleLevel + 1;
  const int mask = (2 << upsampleLevel) - 1;
  const float inverse = 1.0f / static_cast<float>(2 << upsampleLevel);
  for (unsigned int y = 0; y < 16; ++y) {
    const unsigned int fy = 2 * (dstY + y + srcY) + 1;
    float wy[3];
    GetQuadraticWeights(static_cast<float>(fy & mask) * inverse, wy);
    const int baseY = static_cast<int>(fy >> shift) - 1;
    for (unsigned int x = 0; x < 16; ++x) {
      const unsigned int fx = 2 * (dstX + x + srcX) + 1;
      float wx[3];
      GetQuadraticWeights(static_cast<float>(fx & mask) * inverse, wx);
      const int baseX = static_cast<int>(fx >> shift) - 1;
      for (int c = 0; c < 4; ++c) {
        random[c] = static_cast<std::uint16_t>(20077 * random[c] + 12345);
        const float amplitude = static_cast<float>((noiseScale >> (8 * c)) & 0xff);
        float value = static_cast<float>(random[c]) *
                          (amplitude / 32768.0f) -
                      amplitude;
        for (int ky = 0; ky < 3; ++ky) {
          for (int kx = 0; kx < 3; ++kx) {
            const int sampleX = baseX + kx;
            const int sampleY = baseY + ky;
            float sample = static_cast<float>(src[
                4 * (sampleX + ((sampleY & static_cast<int>(srcMaskY)) << 7)) + c]);
            if (sharpenScale != 0) {
              sample += static_cast<float>(SharpenInt(
                            src, sampleX, sampleY,
                            static_cast<int>(srcMaskY), c, sharpenScale)) /
                        1024.0f;
            }
            value += sample * wx[kx] * wy[ky];
          }
        }
        dst[(dstY + y) * dstBytePitch + (dstX + x) * 4 + c] =
            static_cast<std::uint8_t>(
                std::clamp(static_cast<int>(value + 0.5f), 0, 255));
      }
    }
  }
}

__declspec(noinline) void Sharpen4x4Center_SSE2(
    const __m128i* b0, const __m128i* b1, const __m128i* b2,
    const __m128i* b3, const __m128* amount, __m128* f0, __m128* f1,
    __m128* f2, __m128* f3) {
  alignas(16) std::uint8_t rows[4][16];
  _mm_store_si128(reinterpret_cast<__m128i*>(rows[0]), *b0);
  _mm_store_si128(reinterpret_cast<__m128i*>(rows[1]), *b1);
  _mm_store_si128(reinterpret_cast<__m128i*>(rows[2]), *b2);
  _mm_store_si128(reinterpret_cast<__m128i*>(rows[3]), *b3);
  __m128* outputs[4] = {f0, f1, f2, f3};
  for (int center = 0; center < 4; ++center) {
    const int cx = 1 + (center & 1);
    const int cy = 1 + (center >> 1);
    alignas(16) float delta[4]{};
    for (int c = 0; c < 4; ++c) {
      const int value = rows[cy][cx * 4 + c];
      for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
          if (ox == 0 && oy == 0) continue;
          const int d = value - rows[cy + oy][(cx + ox) * 4 + c];
          if (std::abs(d) >= 3) delta[c] += static_cast<float>(d);
        }
      }
    }
    *outputs[center] =
        _mm_add_ps(*outputs[center], _mm_mul_ps(_mm_load_ps(delta), *amount));
  }
}

__declspec(noinline) void Sharpen4x4CenterInt_SSE2(
    const __m128i* b0, const __m128i* b1, const __m128i* b2,
    const __m128i* b3, const __m128i* amount, __m128i* f0, __m128i* f1,
    __m128i* f2, __m128i* f3) {
  alignas(16) std::uint8_t rows[4][16];
  alignas(16) std::int16_t scale[8];
  _mm_store_si128(reinterpret_cast<__m128i*>(rows[0]), *b0);
  _mm_store_si128(reinterpret_cast<__m128i*>(rows[1]), *b1);
  _mm_store_si128(reinterpret_cast<__m128i*>(rows[2]), *b2);
  _mm_store_si128(reinterpret_cast<__m128i*>(rows[3]), *b3);
  _mm_store_si128(reinterpret_cast<__m128i*>(scale), *amount);
  __m128i* outputs[4] = {f0, f1, f2, f3};
  for (int center = 0; center < 4; ++center) {
    alignas(16) std::int16_t values[8];
    _mm_store_si128(reinterpret_cast<__m128i*>(values), *outputs[center]);
    const int cx = 1 + (center & 1);
    const int cy = 1 + (center >> 1);
    for (int c = 0; c < 4; ++c) {
      int sum = 0;
      const int value = rows[cy][cx * 4 + c];
      for (int oy = -1; oy <= 1; ++oy) {
        for (int ox = -1; ox <= 1; ++ox) {
          if (ox == 0 && oy == 0) continue;
          const int d = value - rows[cy + oy][(cx + ox) * 4 + c];
          if (std::abs(d) >= 3) sum += d;
        }
      }
      values[c] = static_cast<std::int16_t>(
          values[c] + ((sum * static_cast<int>(scale[c])) >> 15));
    }
    *outputs[center] = _mm_load_si128(reinterpret_cast<const __m128i*>(values));
  }
}

__declspec(noinline) float sinc(const float x) {
  constexpr float pi = 3.1415927f;
  const float angle = pi * x;
  if (std::fabs(angle) < 0.001f) {
    const float square = angle * angle;
    return ((square / 120.0f) - 0.16666667f) * square + 1.0f;
  }
  float reduced = angle;
  if (angle < 0.0f || angle >= 6.2831855f) {
    reduced = angle - std::floor(angle * 0.15915494f) * 6.2831855f;
  }
  return std::sin(reduced) / angle;
}

__declspec(noinline) float blackman(const float x) {
  constexpr float pi = 3.1415927f;
  float angle = pi * x;
  float angle1 = angle;
  if (angle1 < 0.0f || angle1 >= 6.2831855f) {
    angle1 -= std::floor(angle1 * 0.15915494f) * 6.2831855f;
  }
  float angle2 = angle * 2.0f;
  if (angle2 < 0.0f || angle2 >= 6.2831855f) {
    angle2 -= std::floor(angle2 * 0.15915494f) * 6.2831855f;
  }
  return std::cos(angle2) * 0.076848671f +
         std::cos(angle1) * 0.49656063f + 0.42659071f;
}

__declspec(noinline) float bessel0(const float x) {
  float result = 1.0f;
  float term = 1.0f;
  const float half = x * 0.5f;
  for (unsigned int index = 1;; ++index) {
    term *= half / static_cast<float>(index);
    const float add = term * term;
    result += add;
    if (add <= result * 1.0e-10f) return result;
  }
}

__declspec(noinline) float kaiser(const float x) {
  const float root = std::sqrt(std::max(1.0f - x * x, 1.17549435e-38f));
  return bessel0(root * 4.0f) / bessel0(4.0f);
}

__declspec(noinline) void GetWindowedSincWeights(float s,
                                                 sincWindow_t window,
                                                 float* values) {
  float sum = 0.0f;
  for (int tap = -4; tap <= 4; ++tap) {
    const float distance = static_cast<float>(tap) + 0.5f - s;
    float weight = 0.0f;
    if (std::fabs(distance) < 4.0f) {
      switch (window) {
        case SINC_WINDOW_HAMMING:
          weight = sinc(distance) *
                   (std::cos(distance * 0.25f * 3.1415927f) * 0.46164f +
                    0.53836f);
          break;
        case SINC_WINDOW_BLACKMAN:
          weight = sinc(distance) * blackman(distance * 0.25f);
          break;
        case SINC_WINDOW_LANCZOS:
          weight = sinc(distance) * sinc(distance * 0.25f);
          break;
        case SINC_WINDOW_KAISER:
          weight = sinc(distance) * kaiser(distance * 0.25f);
          break;
      }
    }
    values[tap + 4] = weight;
    sum += weight;
  }
  const float inverse = 1.0f / sum;
  for (int tap = 0; tap < 9; ++tap) values[tap] *= inverse;
}

__declspec(noinline) void UpSample16x16_WindowedSinc(
    std::uint8_t* dst, unsigned int dstX, unsigned int dstY,
    unsigned int dstBytePitch, const std::uint8_t* src, unsigned int srcX,
    unsigned int srcY, unsigned int srcMaskY, char upsampleLevel,
    int sharpenScale, int noiseScale, sincWindow_t window) {
  std::uint16_t random[4] = {
      static_cast<std::uint16_t>(dstX + 16 * dstY + 0),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 1),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 2),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 3)};
  const int shift = upsampleLevel + 1;
  const int mask = (2 << upsampleLevel) - 1;
  const float inverse = 1.0f / static_cast<float>(2 << upsampleLevel);
  for (unsigned int y = 0; y < 16; ++y) {
    const unsigned int fy = 2 * (dstY + y + srcY) + 1;
    float wy[9];
    GetWindowedSincWeights(static_cast<float>(fy & mask) * inverse, window,
                           wy);
    const int baseY = static_cast<int>(fy >> shift) - 4;
    for (unsigned int x = 0; x < 16; ++x) {
      const unsigned int fx = 2 * (dstX + x + srcX) + 1;
      float wx[9];
      GetWindowedSincWeights(static_cast<float>(fx & mask) * inverse, window,
                             wx);
      const int baseX = static_cast<int>(fx >> shift) - 4;
      for (int c = 0; c < 4; ++c) {
        random[c] = static_cast<std::uint16_t>(20077 * random[c] + 12345);
        const float amplitude = static_cast<float>((noiseScale >> (8 * c)) & 0xff);
        float value = static_cast<float>(random[c]) *
                          (amplitude / 32768.0f) -
                      amplitude;
        for (int ky = 0; ky < 9; ++ky) {
          for (int kx = 0; kx < 9; ++kx) {
            const int sampleX = baseX + kx;
            const int sampleY = baseY + ky;
            float sample = static_cast<float>(src[
                4 * (sampleX + ((sampleY & static_cast<int>(srcMaskY)) << 7)) + c]);
            if (sharpenScale != 0) {
              sample += static_cast<float>(SharpenInt(
                            src, sampleX, sampleY,
                            static_cast<int>(srcMaskY), c, sharpenScale)) /
                        1024.0f;
            }
            value += sample * wx[kx] * wy[ky];
          }
        }
        dst[(dstY + y) * dstBytePitch + (dstX + x) * 4 + c] =
            static_cast<std::uint8_t>(
                std::clamp(static_cast<int>(value + 0.5f), 0, 255));
      }
    }
  }
}

__declspec(noinline) void DrawPixelNumber(
    std::uint8_t* dest, int byteStride, char* text, int scaleX, int scaleY,
    int pixelStride, int frontColor, int backColor) {
  for (const char* digit = text; *digit != '\0'; ++digit) {
    if (*digit >= '0' && *digit <= '9') {
      const int glyph = *digit - '0';
      for (int row = 0; row < 8; ++row) {
        for (int column = 0; column < 4; ++column) {
          const int color = binFont[row][glyph * 4 + column] == ' '
                                ? backColor
                                : frontColor;
          for (int sy = 0; sy < scaleY; ++sy) {
            for (int sx = 0; sx < scaleX; ++sx) {
              std::memcpy(dest + (row * scaleY + sy) * byteStride +
                                      (column * scaleX + sx) * pixelStride,
                          mipBlends[color], 4);
            }
          }
        }
      }
    }
    dest += 4 * pixelStride * scaleX;
  }
}

__declspec(noinline) void DrawText(std::uint8_t* dest, int byteStride,
                                   const char* const* text, int scale,
                                   int pixelStride, int frontColor,
                                   int backColor) {
  for (int row = 0; row < 8; ++row) {
    for (int column = 0; text[row][column] != '\0'; ++column) {
      const int color = text[row][column] == ' ' ? backColor : frontColor;
      for (int sy = 0; sy < scale; ++sy) {
        for (int sx = 0; sx < scale; ++sx) {
          std::memcpy(dest + (row * scale + sy) * byteStride +
                                  (column * scale + sx) * pixelStride,
                      mipBlends[color], 4);
        }
      }
    }
  }
}

__declspec(noinline) void itoa(char* output, int value) {
  output[0] = value < 1000 ? ' ' : static_cast<char>('0' + value % 10000 / 1000);
  output[1] = value < 100 ? ' ' : static_cast<char>('0' + value % 1000 / 100);
  output[2] = value < 10 ? ' ' : static_cast<char>('0' + value % 100 / 10);
  output[3] = static_cast<char>('0' + value % 10);
}

__declspec(noinline) void DXT_Decompress(int pageHeaderSize,
                                         int diskOffsetScale,
                                         const std::uint8_t* src, int srcLen,
                                         std::uint8_t* dest) {
  const int mask = ~(diskOffsetScale - 1);
  const int alignedLength = mask & (pageHeaderSize + srcLen + diskOffsetScale - 1);
  idDxtDecoder decoder{};
  if (alignedLength ==
      (mask & (pageHeaderSize + diskOffsetScale + 40959))) {
    decoder.DecompressImageDXT1(src, dest + 0x20000, 128, 128);
    decoder.DecompressYCoCgDXT5(src + 0x2000, dest, 128, 128);
    decoder.DecompressImageDXT5(src + 0x6000, dest + 0x10000, 128, 128);
  } else if (alignedLength ==
             (mask & (pageHeaderSize + diskOffsetScale + 49151))) {
    decoder.DecompressImageDXT5(src, dest + 0x20000, 128, 128);
    decoder.DecompressYCoCgDXT5(src + 0x4000, dest, 128, 128);
    decoder.DecompressImageDXT5(src + 0x8000, dest + 0x10000, 128, 128);
  }
}

__declspec(noinline) void HDP_Decompress(const HDPHeader_t* header,
                                         const std::uint8_t* src,
                                         unsigned int srcLen,
                                         std::uint8_t* dest) {
  const unsigned int coverSize = (header->flags & 8) ? 2048u : 0u;
  const unsigned int streams = header->diffuseSize + header->normalSize +
                               header->specularSize + header->powerSize;
  const bool valid = header->diffuseSize != 0 && header->normalSize != 0 &&
                     header->specularSize != 0 && header->powerSize != 0 &&
                     streams + coverSize + header->alphaSize <= srcLen;
  if (!valid) {
    std::memset(dest, 0, 0x30000);
    return;
  }
  idBareHDP decoder;
  void* raw = _alloca(49152 + 127);
  decoder.tempBuffer = reinterpret_cast<void*>(
      (reinterpret_cast<std::uintptr_t>(raw) + 127) & ~std::uintptr_t(127));
  decoder.tempBufferSize = 49152;
  decoder.quality = header->qualityNormal;
  decoder.DecompressImageNormalMap(src + header->diffuseSize,
                                   dest + 0x10000, 128, 128,
                                   header->normalSize, 0);
  decoder.quality = header->qualitySpecular;
  const std::uint8_t* specularInput =
      src + header->diffuseSize + header->normalSize;
  if ((header->flags & 4) != 0) {
    decoder.DecompressImageGrayScale(specularInput, dest + 0x20000, 128,
                                     128, header->specularSize, 0);
  } else {
    decoder.DecompressImageRGB(specularInput, dest + 0x20000, 128, 128,
                               header->specularSize, 0);
  }
  decoder.quality = header->qualityPower;
  decoder.DecompressImageMono(
      src + header->diffuseSize + header->normalSize + header->specularSize,
      dest, 32, 32, header->powerSize, 0);
  if (coverSize != 0) {
    const std::uint8_t* cover = src + streams;
    for (int pixel = 0; pixel < 16384; ++pixel) {
      dest[0x20000 + pixel * 4 + 3] =
          (cover[pixel >> 3] & (1u << (pixel & 7))) != 0 ? 255 : 0;
    }
  }
  if (header->alphaSize != 0) {
    decoder.quality = header->qualityDiffuse;
    decoder.DecompressImageMono(src + streams + coverSize, dest + 1024, 128,
                                128, header->alphaSize, 0);
    for (int pixel = 0; pixel < 16384; ++pixel) {
      dest[0x20000 + pixel * 4 + 3] = dest[1024 + pixel];
    }
  }
  const int shift = header->flags & 3;
  for (int pixel = 0; pixel < 16384; ++pixel) {
    std::uint8_t* specular = dest + 0x20000 + pixel * 4;
    specular[0] = static_cast<std::uint8_t>(specular[0] << shift);
    specular[1] = static_cast<std::uint8_t>(specular[1] << shift);
    specular[2] = static_cast<std::uint8_t>(specular[2] << shift);
    std::uint8_t* normal = dest + 0x10000 + pixel * 4;
    normal[0] = dest[(pixel >> 9) * 32 + ((pixel & 127) >> 2)];
    normal[2] = static_cast<std::uint8_t>(8 * ((1 << shift) - 1));
  }
  decoder.quality = header->qualityDiffuse;
  decoder.DecompressImageYCoCg(src, dest, 128, 128, header->diffuseSize, 0);
}

__declspec(noinline) void JXR_Decompress(const JXRHeader_t* header,
                                         const std::uint8_t* src, int,
                                         std::uint8_t* dest) {
  if (header->diffuseSize == 0 || header->normalSize == 0 ||
      header->specularSize == 0 || header->powerSize == 0) {
    std::memset(dest, 0, 0x30000);
    return;
  }
  idBareJXR decoder;
  void* raw = _alloca(131072 + 127);
  decoder.tempBuffer = reinterpret_cast<void*>(
      (reinterpret_cast<std::uintptr_t>(raw) + 127) & ~std::uintptr_t(127));
  decoder.tempBufferSize = 131072;
  decoder.quality = header->qualityNormal;
  decoder.DecompressImageNormalMap(src + header->diffuseSize,
                                   dest + 0x10000, 128, 128,
                                   header->normalSize);
  decoder.quality = header->qualitySpecular;
  const std::uint8_t* specularInput =
      src + header->diffuseSize + header->normalSize;
  if ((header->flags & 4) != 0) {
    decoder.DecompressImageGrayScale(specularInput, dest + 0x20000, 128,
                                     128, header->specularSize);
  } else {
    decoder.DecompressImageRGB(specularInput, dest + 0x20000, 128, 128,
                               header->specularSize);
  }
  decoder.quality = header->qualityPower;
  decoder.DecompressImageMono(
      src + header->diffuseSize + header->normalSize + header->specularSize,
      dest, 32, 32, header->powerSize);
  const unsigned int streams = header->diffuseSize + header->normalSize +
                               header->specularSize + header->powerSize;
  const unsigned int coverSize = (header->flags & 8) ? 2048u : 0u;
  if (coverSize != 0) {
    const std::uint8_t* cover = src + streams;
    for (int pixel = 0; pixel < 16384; ++pixel) {
      dest[0x20000 + pixel * 4 + 3] =
          (cover[pixel >> 3] & (1u << (pixel & 7))) != 0 ? 255 : 0;
    }
  }
  if (header->alphaSize != 0) {
    decoder.quality = header->qualityDiffuse;
    decoder.DecompressImageMono(src + streams + coverSize, dest + 1024, 128,
                                128, header->alphaSize);
    for (int pixel = 0; pixel < 16384; ++pixel) {
      dest[0x20000 + pixel * 4 + 3] = dest[1024 + pixel];
    }
  }
  const int shift = header->flags & 3;
  for (int pixel = 0; pixel < 16384; ++pixel) {
    std::uint8_t* specular = dest + 0x20000 + pixel * 4;
    specular[0] = static_cast<std::uint8_t>(specular[0] << shift);
    specular[1] = static_cast<std::uint8_t>(specular[1] << shift);
    specular[2] = static_cast<std::uint8_t>(specular[2] << shift);
    std::uint8_t* normal = dest + 0x10000 + pixel * 4;
    normal[0] = dest[(pixel >> 9) * 32 + ((pixel & 127) >> 2)];
    normal[2] = static_cast<std::uint8_t>(8 * ((1 << shift) - 1));
  }
  decoder.quality = header->qualityDiffuse;
  decoder.DecompressImageYCoCg(src, dest, 128, 128, header->diffuseSize);
}

__declspec(noinline) void CopyUncompressed(
    std::uint8_t* const* targetImage, const int* targetBytePitch,
    char imageFlags, const std::uint8_t* src) {
  const std::size_t offsets[3] = {0x20000, 0, 0x10000};
  const int flags[3] = {1, 2, 4};
  for (int image = 0; image < 3; ++image) {
    if ((imageFlags & flags[image]) == 0) continue;
    for (int row = 0; row < 128; ++row) {
      std::memmove(targetImage[image] + row * targetBytePitch[image],
                   src + offsets[image] + row * 512, 512);
    }
  }
}

__declspec(noinline) void ConvertDXT5_DXT1Alpha(std::uint8_t* dst,
                                                int dstBytePitch,
                                                const std::uint8_t* src) {
  for (int blockY = 0; blockY < 32; ++blockY) {
    for (int blockX = 0; blockX < 32; ++blockX) {
      const std::uint8_t* input = src + (blockY * 32 + blockX) * 16;
      std::uint8_t* output = dst + blockY * dstBytePitch + blockX * 8;
      std::uint8_t alpha[8] = {input[0], input[1]};
      if (alpha[0] > alpha[1]) {
        for (int i = 1; i <= 6; ++i) {
          alpha[i + 1] = static_cast<std::uint8_t>(
              ((7 - i) * alpha[0] + i * alpha[1]) / 7);
        }
      } else {
        for (int i = 1; i <= 4; ++i) {
          alpha[i + 1] = static_cast<std::uint8_t>(
              ((5 - i) * alpha[0] + i * alpha[1]) / 5);
        }
        alpha[6] = 0;
        alpha[7] = 255;
      }
      std::uint64_t alphaBits = 0;
      for (int byte = 0; byte < 6; ++byte) {
        alphaBits |= static_cast<std::uint64_t>(input[2 + byte]) << (8 * byte);
      }
      bool opaque = true;
      for (int pixel = 0; pixel < 16; ++pixel) {
        opaque &= alpha[(alphaBits >> (3 * pixel)) & 7] >= 128;
      }
      if (opaque) {
        std::memcpy(output, input + 8, 8);
        continue;
      }
      std::uint16_t color0;
      std::uint16_t color1;
      std::uint32_t indices;
      std::memcpy(&color0, input + 8, 2);
      std::memcpy(&color1, input + 10, 2);
      std::memcpy(&indices, input + 12, 4);
      if (color0 > color1) {
        std::swap(color0, color1);
        indices = ~indices;
      }
      std::uint32_t converted = 0;
      for (int pixel = 0; pixel < 16; ++pixel) {
        int index = (indices >> (2 * pixel)) & 3;
        if (alpha[(alphaBits >> (3 * pixel)) & 7] < 128) {
          index = 3;
        } else if (index == 2) {
          index = 1;
        } else if (index == 3) {
          index = 2;
        }
        converted |= static_cast<std::uint32_t>(index) << (2 * pixel);
      }
      std::memcpy(output, &color0, 2);
      std::memcpy(output + 2, &color1, 2);
      std::memcpy(output + 4, &converted, 4);
    }
  }
}

__declspec(noinline) void CopyDXT(std::uint8_t* dst, int dstBytePitch,
                                  const std::uint8_t* src,
                                  int srcBytePitch) {
  for (int row = 0; row < 32; ++row) {
    std::memmove(dst, src, static_cast<std::size_t>(srcBytePitch));
    dst += dstBytePitch;
    src += srcBytePitch;
  }
}

__declspec(noinline) void CopyDXTCompressed(
    std::uint8_t* const* targetImage, const int* targetBytePitch,
    char imageFlags,
    const std::uint8_t* src, int srcLength, int pageHeaderSize,
    int diskOffsetScale) {
  const int mask = ~(diskOffsetScale - 1);
  const int alignedLength =
      mask & (pageHeaderSize + srcLength + diskOffsetScale - 1);
  if (alignedLength ==
      (mask & (pageHeaderSize + diskOffsetScale + 40959))) {
    if ((imageFlags & 1) != 0)
      CopyDXT(targetImage[0], targetBytePitch[0], src, 256);
    if ((imageFlags & 2) != 0)
      CopyDXT(targetImage[1], targetBytePitch[1], src + 0x2000, 512);
    if ((imageFlags & 4) != 0)
      CopyDXT(targetImage[2], targetBytePitch[2], src + 0x6000, 512);
  } else if (alignedLength ==
             (mask & (pageHeaderSize + diskOffsetScale + 49151))) {
    if ((imageFlags & 1) != 0)
      ConvertDXT5_DXT1Alpha(targetImage[0], targetBytePitch[0], src);
    if ((imageFlags & 2) != 0)
      CopyDXT(targetImage[1], targetBytePitch[1], src + 0x4000, 512);
    if ((imageFlags & 4) != 0)
      CopyDXT(targetImage[2], targetBytePitch[2], src + 0x8000, 512);
  }
}

__declspec(noinline) void StripPageBorder(std::uint8_t* dst) {
  alignas(16) std::array<std::uint8_t, 0x10000> source{};
  std::memcpy(source.data(), dst, source.size());
  for (int y = 0; y < 128; ++y) {
    const float sourceY = 4.0f + 0.9375f * static_cast<float>(y);
    const int baseY = static_cast<int>(sourceY);
    const float fy = sourceY - static_cast<float>(baseY);
    const float wy[4] = {
        ((1.5f - 0.75f * fy) * fy - 0.75f) * fy,
        ((1.25f * fy - 2.25f) * fy) * fy + 1.0f,
        ((1.5f - 1.25f * fy) * fy + 0.75f) * fy,
        ((0.75f * fy - 0.75f) * fy) * fy};
    for (int x = 0; x < 128; ++x) {
      const float sourceX = 4.0f + 0.9375f * static_cast<float>(x);
      const int baseX = static_cast<int>(sourceX);
      const float fx = sourceX - static_cast<float>(baseX);
      const float wx[4] = {
          ((1.5f - 0.75f * fx) * fx - 0.75f) * fx,
          ((1.25f * fx - 2.25f) * fx) * fx + 1.0f,
          ((1.5f - 1.25f * fx) * fx + 0.75f) * fx,
          ((0.75f * fx - 0.75f) * fx) * fx};
      for (int c = 0; c < 4; ++c) {
        float value = 0.0f;
        for (int ky = 0; ky < 4; ++ky) {
          for (int kx = 0; kx < 4; ++kx) {
            value += source[4 * ((baseY + ky - 1) * 128 +
                                 (baseX + kx - 1)) + c] *
                     wx[kx] * wy[ky];
          }
        }
        dst[4 * (y * 128 + x) + c] = static_cast<std::uint8_t>(
            std::clamp(static_cast<int>(value), 0, 255));
      }
    }
  }
}

__declspec(noinline) void UpSample16x16_Bicubic(
    std::uint8_t* dst, unsigned int dstX, unsigned int dstY,
    unsigned int dstBytePitch, const std::uint8_t* src, unsigned int srcX,
    unsigned int srcY, unsigned int srcMaskY, unsigned int upsampleLevel,
    unsigned int sharpenScale, unsigned int noiseScale) {
  const unsigned int shift = upsampleLevel + 1;
  const unsigned int mask = (2u << upsampleLevel) - 1;
  const float inverse = 1.0f / static_cast<float>(2u << upsampleLevel);
  std::uint16_t random[4] = {
      static_cast<std::uint16_t>(dstX + 16 * dstY + 0),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 1),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 2),
      static_cast<std::uint16_t>(dstX + 16 * dstY + 3)};
  for (unsigned int y = 0; y < 16; ++y) {
    const unsigned int fyFixed = 2 * (dstY + y + srcY) + 1;
    const float fy = static_cast<float>(fyFixed & mask) * inverse;
    const int baseY = static_cast<int>(fyFixed >> shift) - 1;
    const float wy[4] = {
        ((1.5f - 0.75f * fy) * fy - 0.75f) * fy,
        ((1.25f * fy - 2.25f) * fy) * fy + 1.0f,
        ((1.5f - 1.25f * fy) * fy + 0.75f) * fy,
        ((0.75f * fy - 0.75f) * fy) * fy};
    for (unsigned int x = 0; x < 16; ++x) {
      const unsigned int fxFixed = 2 * (dstX + x + srcX) + 1;
      const float fx = static_cast<float>(fxFixed & mask) * inverse;
      const int baseX = static_cast<int>(fxFixed >> shift) - 1;
      const float wx[4] = {
          ((1.5f - 0.75f * fx) * fx - 0.75f) * fx,
          ((1.25f * fx - 2.25f) * fx) * fx + 1.0f,
          ((1.5f - 1.25f * fx) * fx + 0.75f) * fx,
          ((0.75f * fx - 0.75f) * fx) * fx};
      for (int c = 0; c < 4; ++c) {
        random[c] = static_cast<std::uint16_t>(20077 * random[c] + 12345);
        const float amplitude =
            static_cast<float>((noiseScale >> (8 * c)) & 0xff);
        float value = static_cast<float>(random[c]) *
                          (amplitude / 32768.0f) -
                      amplitude;
        for (int ky = 0; ky < 4; ++ky) {
          for (int kx = 0; kx < 4; ++kx) {
            const int sampleX = baseX + kx;
            const int sampleY = baseY + ky;
            float sample = static_cast<float>(src[
                4 * (sampleX + ((sampleY & static_cast<int>(srcMaskY)) << 7)) + c]);
            if (sharpenScale != 0) {
              sample += static_cast<float>(SharpenInt(
                            src, sampleX, sampleY,
                            static_cast<int>(srcMaskY), c,
                            static_cast<int>(sharpenScale))) /
                        1024.0f;
            }
            value += sample * wx[kx] * wy[ky];
          }
        }
        dst[(dstY + y) * dstBytePitch + (dstX + x) * 4 + c] =
            static_cast<std::uint8_t>(
                std::clamp(static_cast<int>(value + 0.5f), 0, 255));
      }
    }
  }
  if (sharpenScale == 0xffffffffu) {
    __m128i block{};
    __m128 amount = _mm_setzero_ps();
    __m128 output = _mm_setzero_ps();
    Sharpen4x4Center_SSE2(&block, &block, &block, &block, &amount, &output,
                          &output, &output, &output);
  }
}

__declspec(noinline) void UpSample16x16_BicubicInt(
    std::uint8_t* dst, unsigned int dstX, unsigned int dstY,
    unsigned int dstBytePitch, const std::uint8_t* src, unsigned int srcX,
    unsigned int srcY, unsigned int srcMaskY, unsigned int upsampleLevel,
    unsigned int sharpenScale, unsigned int noiseScale) {
  UpSample16x16_Bicubic(dst, dstX, dstY, dstBytePitch, src, srcX, srcY,
                        srcMaskY, upsampleLevel, sharpenScale, noiseScale);
  if (sharpenScale == 0xffffffffu) {
    __m128i block{};
    Sharpen4x4CenterInt_SSE2(&block, &block, &block, &block, &block, &block,
                             &block, &block, &block);
  }
}

}  // namespace

void UpSample16x16(std::uint8_t* destination, unsigned int destinationX,
                   unsigned int destinationY,
                   unsigned int destinationBytePitch,
                   const std::uint8_t* source, unsigned int sourceX,
                   unsigned int sourceY, unsigned int sourceMaskY,
                   int upsampleLevel, int upsampleFilter, int sharpenScale,
                   int noiseScale) {
  switch (upsampleFilter) {
    case 8:
      UpSample16x16_WindowedSinc(destination, destinationX, destinationY,
          destinationBytePitch, source, sourceX, sourceY, sourceMaskY,
          static_cast<char>(upsampleLevel), sharpenScale, noiseScale,
          SINC_WINDOW_KAISER);
      break;
    case 7:
      UpSample16x16_WindowedSinc(destination, destinationX, destinationY,
          destinationBytePitch, source, sourceX, sourceY, sourceMaskY,
          static_cast<char>(upsampleLevel), sharpenScale, noiseScale,
          SINC_WINDOW_LANCZOS);
      break;
    case 6:
      UpSample16x16_WindowedSinc(destination, destinationX, destinationY,
          destinationBytePitch, source, sourceX, sourceY, sourceMaskY,
          static_cast<char>(upsampleLevel), sharpenScale, noiseScale,
          SINC_WINDOW_BLACKMAN);
      break;
    case 5:
      UpSample16x16_WindowedSinc(destination, destinationX, destinationY,
          destinationBytePitch, source, sourceX, sourceY, sourceMaskY,
          static_cast<char>(upsampleLevel), sharpenScale, noiseScale,
          SINC_WINDOW_HAMMING);
      break;
    case 4:
      UpSample16x16_Bicubic(destination, destinationX, destinationY,
          destinationBytePitch, source, sourceX, sourceY, sourceMaskY,
          upsampleLevel, sharpenScale, noiseScale);
      break;
    case 3:
      UpSample16x16_BicubicInt(destination, destinationX, destinationY,
          destinationBytePitch, source, sourceX, sourceY, sourceMaskY,
          upsampleLevel, sharpenScale, noiseScale);
      break;
    case 2:
      UpSample16x16_Biquadratic(destination, destinationX, destinationY,
          destinationBytePitch, source, sourceX, sourceY, sourceMaskY,
          static_cast<char>(upsampleLevel), sharpenScale, noiseScale);
      break;
    case 1:
      UpSample16x16_Bilinear(destination, destinationX, destinationY,
          destinationBytePitch, source, sourceX, sourceY, sourceMaskY,
          static_cast<char>(upsampleLevel), sharpenScale, noiseScale);
      break;
    default:
      UpSample16x16_Nearest(destination, destinationX, destinationY,
          destinationBytePitch, source, sourceX, sourceY, sourceMaskY,
          static_cast<char>(upsampleLevel), sharpenScale, noiseScale);
      break;
  }
}

namespace {

__declspec(noinline) void DCT_Decompress(const DCTHeader_t* header,
                                         const std::uint8_t* src,
                                         unsigned int srcLen,
                                         std::uint8_t* dest) {
  const unsigned int coverSize = (header->flags & 8) ? 2048u : 0u;
  const unsigned int streams = header->diffuseSize + header->normalSize +
                               header->specularSize + header->powerSize;
  if (streams + coverSize + header->alphaSize > srcLen ||
      (header->diffuseSize != 0 && header->diffuseSize > srcLen)) {
    return;
  }
  idBareDCTHuffmanTable ydc(HUFFMAN_TYPE_Y_DC, false);
  idBareDCTHuffmanTable yac(HUFFMAN_TYPE_Y_AC, false);
  idBareDCTHuffmanTable cocgdc(HUFFMAN_TYPE_COCG_DC, false);
  idBareDCTHuffmanTable cocgac(HUFFMAN_TYPE_COCG_AC, false);
  idBareDctDecoder decoder;
  decoder.SetHuffmanTables(&ydc, &yac, &cocgdc, &cocgac, nullptr, nullptr);
  decoder.SetQuality_SSE2(header->qualityNormal, header->qualityNormal,
                          header->qualityNormal);
  decoder.DecompressImageNormalMap_SSE2(src + header->diffuseSize,
                                        dest + 0x10000, 128, 128,
                                        header->normalSize);
  decoder.SetQuality_SSE2(header->qualitySpecular, header->qualitySpecular,
                          header->qualitySpecular);
  const std::uint8_t* specularInput =
      src + header->diffuseSize + header->normalSize;
  if ((header->flags & 4) != 0) {
    decoder.DecompressImageGrayScale_SSE2(specularInput, dest + 0x20000,
                                          128, 128, header->specularSize);
  } else {
    decoder.DecompressImageRGB_SSE2(specularInput, dest + 0x20000, 128, 128,
                                    header->specularSize);
  }
  decoder.SetQuality_SSE2(header->qualityPower, header->qualityPower,
                          header->qualityPower);
  decoder.DecompressImageMono_SSE2(src + header->diffuseSize +
                                       header->normalSize +
                                       header->specularSize,
                                   dest, 32, 32, header->powerSize);
  if (coverSize != 0) {
    const std::uint8_t* cover = src + streams;
    for (int pixel = 0; pixel < 16384; ++pixel) {
      dest[0x20000 + pixel * 4 + 3] =
          (cover[pixel >> 3] & (1u << (pixel & 7))) != 0 ? 255 : 0;
    }
  }
  if (header->alphaSize != 0) {
    decoder.SetQuality_SSE2(header->qualityLuma, header->qualityLuma,
                            header->qualityLuma);
    decoder.DecompressImageMono_SSE2(src + streams + coverSize, dest + 1024,
                                     128, 128, header->alphaSize);
    for (int pixel = 0; pixel < 16384; ++pixel) {
      dest[0x20000 + pixel * 4 + 3] = dest[1024 + pixel];
    }
  }
  const int shift = header->flags & 3;
  for (int pixel = 0; pixel < 16384; ++pixel) {
    std::uint8_t* specular = dest + 0x20000 + pixel * 4;
    specular[0] = static_cast<std::uint8_t>(specular[0] << shift);
    specular[1] = static_cast<std::uint8_t>(specular[1] << shift);
    specular[2] = static_cast<std::uint8_t>(specular[2] << shift);
    std::uint8_t* normal = dest + 0x10000 + pixel * 4;
    normal[0] = dest[(pixel >> 9) * 32 + ((pixel & 127) >> 2)];
    normal[2] = static_cast<std::uint8_t>(8 * ((1 << shift) - 1));
  }
  decoder.SetQuality_SSE2(header->qualityLuma, header->qualityChroma,
                          header->qualityLuma);
  decoder.DecompressImageYCoCg_SSE2(src, dest, 128, 128,
                                    header->diffuseSize);
}

__declspec(noinline) void UpsamplePage(std::uint8_t* pageDataBuffer,
                                       int upsampleLevel, int upsampleX,
                                       int upsampleY, int upsampleFilter,
                                       int upsampleSharpen,
                                       int upsampleNoise) {
  if (upsampleLevel == 0) return;
  auto* output = static_cast<std::uint8_t*>(_aligned_malloc(0x30000, 16));
  if (output == nullptr) return;
  unsigned int sourceX = 0;
  unsigned int sourceY = 0;
  for (int level = 0, bit = 1; level < upsampleLevel; ++level, bit <<= 1) {
    sourceX += (upsampleX & bit) != 0 ? 124u * bit : 4u * bit;
    sourceY += (upsampleY & bit) != 0 ? 124u * bit : 4u * bit;
  }
  for (int image = 0; image < 3; ++image) {
    const int sharpen = image == 0 ? upsampleSharpen << 24 : 0;
    const int noise = image == 0 ? upsampleNoise << 24 : 0;
    for (unsigned int y = 0; y < 128; y += 16) {
      for (unsigned int x = 0; x < 128; x += 16) {
        UpSample16x16(output + image * 0x10000, x, y, 512,
                     pageDataBuffer + image * 0x10000, sourceX, sourceY, 127,
                     upsampleLevel, upsampleFilter, sharpen, noise);
      }
    }
  }
  std::memmove(pageDataBuffer, output, 0x30000);
  _aligned_free(output);
}

}  // namespace

void TranscodePage(const transcodeParms_t* parms) {
  const bool visualDebug = parms->dbgShowPageSources ||
                           parms->dbgShowPageColors ||
                           parms->dbgShowPageNumbers ||
                           parms->dbgShowPageInfo ||
                           parms->dbgShowPageBorders ||
                           parms->dbgShowPageCheckered ||
                           parms->dbgShowPageMips != 0 ||
                           parms->dbgShowPageSizes != 0 ||
                           parms->dbgShowPageUsage != 0;
  if (parms->outPageCompression == parms->inPageCompression &&
      !parms->stripPageBorders && !visualDebug &&
      parms->inPageData != nullptr && parms->upsampleLevel == 0) {
    if (parms->inPageCompression == COMP_NONE) {
      CopyUncompressed(parms->targetImage, parms->targetBytePitch,
                       static_cast<char>(parms->imageFlags),
                       parms->inPageData);
      return;
    }
    if (parms->inPageCompression == COMP_DXT) {
      CopyDXTCompressed(parms->targetImage, parms->targetBytePitch,
                        static_cast<char>(parms->imageFlags),
                        parms->inPageData, parms->inPageDataLength,
                        parms->pageHeaderSize, parms->diskOffsetScale);
      return;
    }
  }

  std::uint8_t* allocation = nullptr;
  std::uint8_t* tempData = parms->tempData;
  if (tempData == nullptr) {
    allocation = static_cast<std::uint8_t*>(
        _aligned_malloc(static_cast<std::size_t>(parms->tempSize), 128));
    if (allocation == nullptr) return;
    tempData = allocation;
    const_cast<transcodeParms_t*>(parms)->tempData = tempData;
  }

  bool monoSpecular = false;
  if (parms->inPageData == nullptr ||
      parms->inPageCompression == COMP_INVALID) {
    const int divisor = std::max(parms->numLevels - 1, 1);
    const std::uint8_t level = static_cast<std::uint8_t>(
        std::clamp(255 * parms->pageLevel / divisor, 0, 255));
    for (int image = 0; image < 3; ++image) {
      for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
          std::uint8_t* pixel = tempData + image * 0x10000 +
                                (y * 128 + x) * 4;
          pixel[0] = static_cast<std::uint8_t>(2 * x);
          pixel[1] = static_cast<std::uint8_t>(2 * y);
          pixel[2] = level;
          pixel[3] = 255;
        }
      }
    }
  } else {
    switch (parms->inPageCompression) {
      case COMP_NONE:
        std::memmove(tempData, parms->inPageData, 0x30000);
        break;
      case COMP_DXT:
        DXT_Decompress(parms->pageHeaderSize, parms->diskOffsetScale,
                       parms->inPageData, parms->inPageDataLength, tempData);
        break;
      case COMP_DCT:
        DCT_Decompress(reinterpret_cast<const DCTHeader_t*>(
                           parms->inPageHeader),
                       parms->inPageData, parms->inPageDataLength, tempData);
        monoSpecular = (parms->inPageHeader[5] & 4) != 0;
        break;
      case COMP_HDP:
        HDP_Decompress(reinterpret_cast<const HDPHeader_t*>(
                           parms->inPageHeader),
                       parms->inPageData, parms->inPageDataLength, tempData);
        monoSpecular = (parms->inPageHeader[4] & 4) != 0;
        break;
      case COMP_JXR:
        JXR_Decompress(reinterpret_cast<const JXRHeader_t*>(
                           parms->inPageHeader),
                       parms->inPageData, parms->inPageDataLength, tempData);
        monoSpecular = (parms->inPageHeader[4] & 4) != 0;
        break;
      default:
        break;
    }
  }

  if (parms->upsampleLevel != 0) {
    UpsamplePage(tempData, parms->upsampleLevel, parms->upsampleX,
                 parms->upsampleY, parms->upsampleFilter,
                 parms->upsampleSharpen,
                 parms->upsampleNoise >> parms->pageLevel);
  }

  auto fillDebug = [&](std::uint8_t y, std::uint8_t co, std::uint8_t cg) {
    std::memset(tempData + 0x20000, 0, 0x10000);
    for (int pixel = 0; pixel < 16384; ++pixel) {
      std::uint8_t* diffuse = tempData + pixel * 4;
      diffuse[0] = co;
      diffuse[1] = cg;
      diffuse[2] = 0;
      diffuse[3] = y;
      std::uint8_t* normal = tempData + 0x10000 + pixel * 4;
      normal[0] = 0;
      normal[1] = 255;
      normal[2] = 255;
      normal[3] = 255;
    }
  };

  if (parms->dbgShowPageColors) {
    const std::uint8_t* color = mipBlends[parms->pageSource & 7];
    fillDebug(color[3], color[0], color[1]);
  }
  if (parms->dbgShowPageMips != 0 &&
      parms->pageLevel >= parms->dbgShowPageMips - 1) {
    const int divisor = std::max(parms->numLevels - 1, 1);
    const int red = 255 * (parms->numLevels - parms->pageLevel - 1) / divisor;
    const int green = 255 * parms->pageLevel / divisor;
    fillDebug(static_cast<std::uint8_t>(std::clamp((2 * green + red + 2) / 4, 0, 255)),
              static_cast<std::uint8_t>(std::clamp((2 * red + 2) / 4 + 128, 0, 255)),
              static_cast<std::uint8_t>(std::clamp((2 * green - red + 2) / 4 + 128, 0, 255)));
  }
  if (parms->dbgShowPageUsage != 0) {
    const int red = 255 - parms->dbgShowPageUsage;
    const int green = parms->dbgShowPageUsage;
    fillDebug(static_cast<std::uint8_t>(std::clamp((2 * green + red + 2) / 4, 0, 255)),
              static_cast<std::uint8_t>(std::clamp((2 * red + 2) / 4 + 128, 0, 255)),
              static_cast<std::uint8_t>(std::clamp((2 * green - red + 2) / 4 + 128, 0, 255)));
  }
  if (parms->dbgShowPageCheckered) {
    for (int y = 0; y < 128; ++y) {
      for (int x = 0; x < 128; ++x) {
        if (((x ^ y) & 4) != 0) {
          const std::uint32_t value = 32896;
          std::memcpy(tempData + (y * 128 + x) * 4, &value, 4);
        }
      }
    }
    std::memset(tempData + 0x20000, 0, 0x10000);
  }
  if (parms->dbgShowPageSizes != 0) {
    char text[128];
    std::snprintf(text, sizeof(text), "%d", parms->inPageDataLength);
    DrawPixelNumber(tempData + 172160, 512, text, 2, 2, 4, 10, 10);
    DrawPixelNumber(tempData + 41088, 512, text, 2, 2, 4,
                    parms->pageLevel, 8);
    DrawPixelNumber(tempData + 106624, 512, text, 2, 2, 4,
                    parms->pageLevel, 8);
  }
  if (parms->dbgShowPageNumbers || parms->inPageData == nullptr) {
    char number[5]{};
    itoa(number, parms->pageX << parms->pageLevel);
    number[4] = '\0';
    DrawPixelNumber(tempData + 159872, 512, number, 2, 2, 4, 10, 10);
  }
  if (parms->dbgShowPageInfo || parms->inPageData == nullptr) {
    DrawText(tempData + 139312, 512, sourceText, 2, 4, 10, 10);
    DrawText(tempData + 151600, 512, layoutText, 2, 4, 10, 10);
    char number[16];
    std::snprintf(number, sizeof(number), "%d", parms->pageSource);
    DrawPixelNumber(tempData + 139640, 512, number, 2, 2, 4, 10, 10);
    (void)monoSpecular;
  }
  if (visualDebug || parms->inPageData == nullptr) {
    for (int x = 4; x < 124; ++x) {
      std::memcpy(tempData + (4 * 128 + x) * 4, mipBlends[9], 4);
      std::memcpy(tempData + (123 * 128 + x) * 4, mipBlends[9], 4);
    }
    for (int y = 4; y < 124; ++y) {
      std::memcpy(tempData + (y * 128 + 4) * 4, mipBlends[9], 4);
      std::memcpy(tempData + (y * 128 + 123) * 4, mipBlends[9], 4);
    }
  }

  if (parms->stripPageBorders) {
    StripPageBorder(tempData + 0x20000);
    StripPageBorder(tempData);
    StripPageBorder(tempData + 0x10000);
  }

  if (parms->outPageCompression == COMP_NONE) {
    if (!parms->tiled) {
      CopyUncompressed(parms->targetImage, parms->targetBytePitch,
                       static_cast<char>(parms->imageFlags), tempData);
    }
  } else if (parms->outPageCompression == COMP_DXT) {
    alignas(16) std::array<std::uint8_t, 16384> tiledOutput{};
    idDxtEncoder encoder;
    encoder.srcPadding = 0;
    if ((parms->imageFlags & 1) != 0) {
      std::uint8_t* output = parms->tiled ? tiledOutput.data()
                                          : parms->targetImage[0];
      encoder.dstPadding = parms->tiled ? 0 : parms->targetBytePitch[0] - 256;
      encoder.CompressImageDXT1AlphaFast_SSE2(tempData + 0x20000, output,
                                              128, 128);
    }
    if ((parms->imageFlags & 2) != 0) {
      std::uint8_t* output = parms->tiled ? tiledOutput.data()
                                          : parms->targetImage[1];
      encoder.dstPadding = parms->tiled ? 0 : parms->targetBytePitch[1] - 512;
      encoder.CompressYCoCgDXT5Fast_SSE2(tempData, output, 128, 128);
    }
    if ((parms->imageFlags & 4) != 0) {
      std::uint8_t* output = parms->tiled ? tiledOutput.data()
                                          : parms->targetImage[2];
      encoder.dstPadding = parms->tiled ? 0 : parms->targetBytePitch[2] - 512;
      encoder.CompressNormalMapDXT5Fast_SSE2(tempData + 0x10000, output,
                                             128, 128);
    }
  }

  if (allocation != nullptr) {
    _aligned_free(allocation);
  }
}

idParallelJobRegistration register_TranscodePage(
    reinterpret_cast<jobRun_t>(TranscodePage), "TranscodePage");
