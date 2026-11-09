#include "engine/renderer/jobs/transcode/transcode.h"

#include "engine/renderer/jobs/transcode/image/dxt/dxtdecoder.h"
#include "engine/renderer/jobs/transcode/image/dxt/dxtencoder.h"
#include "shared/idlib/parallelism/paralleljoblist.h"

#include <cstdint>
#include <cstring>
#include <immintrin.h>
#include <malloc.h>

namespace {

alignas(16) const __m128i vector_byte_255 = _mm_set1_epi8(-1);
alignas(16) const __m128i vector_byte_cover_mask0 = _mm_setr_epi8(
    0, 0, 0, 1, 0, 0, 0, 2, 0, 0, 0, 4, 0, 0, 0, 8);
alignas(16) const __m128i vector_byte_cover_mask1 = _mm_setr_epi8(
    0, 0, 0, 16, 0, 0, 0, 32, 0, 0, 0, 64, 0, 0, 0,
    static_cast<char>(0x80));
alignas(16) const __m128i vector_byte_cover_mask2 = _mm_setr_epi32(
    0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff);
alignas(16) const __m128i vector_byte_specular_mask0 = _mm_setr_epi32(
    static_cast<int>(0xff000000u), static_cast<int>(0xff000000u),
    static_cast<int>(0xff000000u), static_cast<int>(0xff000000u));
alignas(16) const __m128i vector_byte_specular_mask1 = _mm_setr_epi32(
    0x00ffffff, 0x00ffffff, 0x00ffffff, 0x00ffffff);
alignas(16) const __m128i vector_byte_specular_mask2 = _mm_setr_epi32(
    0x000000ff, 0x000000ff, 0x000000ff, 0x000000ff);
alignas(16) const __m128i vector_byte_specular_mask3 = _mm_setr_epi32(
    0x000000ff, 0, 0, 0);
alignas(16) const __m128i vector_int_2 = _mm_set1_epi32(2);
alignas(16) const __m128i vector_int_63 = _mm_set1_epi32(63);
alignas(16) const __m128i vector_int_127 = _mm_set1_epi32(127);

// These vectors are instantiated by the retail compiland through the shared
// upsampling definitions.  Keeping them here preserves that object surface.
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

}  // namespace

void TranscodePage_DXT_DXT_upsample(const transcodeParms_t* parms) {
  if (parms->imageFlags == 0) {
    return;
  }
  std::uint8_t* tempData = parms->tempData;
  if (tempData == nullptr) {
    const int allocationSize = ((parms->tempSize + 127) & ~127) + 128;
    auto* allocation = static_cast<std::uint8_t*>(_alloca(allocationSize));
    tempData = reinterpret_cast<std::uint8_t*>(
        (reinterpret_cast<std::uintptr_t>(allocation) + 127u) &
        ~std::uintptr_t(127));
    const_cast<transcodeParms_t*>(parms)->tempData = tempData;
  }

  std::memset(tempData, 0, 1024);
  std::uint8_t* sourceRows = tempData + 1024;
  std::uint8_t* tileRGBA = tempData + 17408;
  std::uint8_t* tiledOutput = tempData + 18432;
  unsigned int sourceX = 0;
  unsigned int sourceY = 0;
  for (int level = 0, bit = 1; level < parms->upsampleLevel;
       ++level, bit <<= 1) {
    sourceX += (parms->upsampleX & bit) != 0 ? 124u * bit : 4u * bit;
    sourceY += (parms->upsampleY & bit) != 0 ? 124u * bit : 4u * bit;
  }

  idDxtDecoder decoder{};
  idDxtEncoder encoder;

  if ((parms->imageFlags & 1) != 0) {
    encoder.srcPadding = 0;
    encoder.dstPadding =
        parms->tiled ? 0 : parms->targetBytePitch[0] - 32;
    const int radius = 4 << parms->upsampleLevel;
    const int rowBase = (static_cast<int>(sourceY) + radius) / 16;
    const int rowDelta =
        (static_cast<int>(sourceY) - radius) / 16 - rowBase - 1;
    int decodedRow = -1;

    for (int tileY = 0; tileY < 8; ++tileY) {
      const int rowCursor = tileY + rowBase + 1;
      const int neededLast = (rowCursor >> parms->upsampleLevel) & 7;
      const int neededFirst =
          ((rowCursor + rowDelta) >> parms->upsampleLevel) & 7;
      while (decodedRow < neededLast) {
        ++decodedRow;
        if (decodedRow >= neededFirst) {
          decoder.DecompressImageDXT1(
              parms->inPageData + decodedRow * 1024,
              sourceRows + 8192 * (decodedRow & 1), 128, 16);
        }
      }
      for (int tileX = 0; tileX < 8; ++tileX) {
        UpSample16x16(tileRGBA, tileX * 16, tileY * 16, 64, sourceRows,
                     sourceX, sourceY, 31, parms->upsampleLevel,
                     parms->upsampleFilter, 0, 0);
        std::uint8_t* output =
            parms->tiled
                ? tiledOutput
                : parms->targetImage[0] +
                      tileY * 4 * parms->targetBytePitch[0] + tileX * 32;
        encoder.CompressImageDXT1AlphaFast_SSE2(tileRGBA, output, 16, 16);
      }
    }
  }

  if ((parms->imageFlags & 2) != 0) {
    encoder.srcPadding = 0;
    encoder.dstPadding =
        parms->tiled ? 0 : parms->targetBytePitch[1] - 64;
    const int radius = 4 << parms->upsampleLevel;
    const int rowBase = (static_cast<int>(sourceY) + radius) / 16;
    const int rowDelta =
        (static_cast<int>(sourceY) - radius) / 16 - rowBase - 1;
    const int sharpenScale = static_cast<int>(
        static_cast<std::uint32_t>(parms->upsampleSharpen) << 24);
    const int noiseScale = static_cast<int>(
        static_cast<std::uint32_t>(parms->upsampleNoise >> parms->pageLevel)
        << 24);
    int decodedRow = -1;

    for (int tileY = 0; tileY < 8; ++tileY) {
      const int rowCursor = tileY + rowBase + 1;
      const int neededLast = (rowCursor >> parms->upsampleLevel) & 7;
      const int neededFirst =
          ((rowCursor + rowDelta) >> parms->upsampleLevel) & 7;
      while (decodedRow < neededLast) {
        ++decodedRow;
        if (decodedRow >= neededFirst) {
          decoder.DecompressYCoCgDXT5(
              parms->inPageData + decodedRow * 2048,
              sourceRows + 8192 * (decodedRow & 1), 128, 16);
        }
      }
      for (int tileX = 0; tileX < 8; ++tileX) {
        UpSample16x16(tileRGBA, tileX * 16, tileY * 16, 64, sourceRows,
                     sourceX, sourceY, 31, parms->upsampleLevel,
                     parms->upsampleFilter, sharpenScale, noiseScale);
        std::uint8_t* output =
            parms->tiled
                ? tiledOutput
                : parms->targetImage[1] +
                      tileY * 4 * parms->targetBytePitch[1] + tileX * 64;
        encoder.CompressYCoCgDXT5Fast_SSE2(tileRGBA, output, 16, 16);
      }
    }
  }

  if ((parms->imageFlags & 4) != 0) {
    encoder.srcPadding = 0;
    encoder.dstPadding =
        parms->tiled ? 0 : parms->targetBytePitch[2] - 64;
    const int radius = 4 << parms->upsampleLevel;
    const int rowBase = (static_cast<int>(sourceY) + radius) / 16;
    const int rowDelta =
        (static_cast<int>(sourceY) - radius) / 16 - rowBase - 1;
    int decodedRow = -1;

    for (int tileY = 0; tileY < 8; ++tileY) {
      const int rowCursor = tileY + rowBase + 1;
      const int neededLast = (rowCursor >> parms->upsampleLevel) & 7;
      const int neededFirst =
          ((rowCursor + rowDelta) >> parms->upsampleLevel) & 7;
      while (decodedRow < neededLast) {
        ++decodedRow;
        if (decodedRow >= neededFirst) {
          decoder.DecompressImageDXT5(
              parms->inPageData + decodedRow * 2048,
              sourceRows + 8192 * (decodedRow & 1), 128, 16);
        }
      }
      for (int tileX = 0; tileX < 8; ++tileX) {
        UpSample16x16(tileRGBA, tileX * 16, tileY * 16, 64, sourceRows,
                     sourceX, sourceY, 31, parms->upsampleLevel,
                     parms->upsampleFilter, 0, 0);
        std::uint8_t* output =
            parms->tiled
                ? tiledOutput
                : parms->targetImage[2] +
                      tileY * 4 * parms->targetBytePitch[2] + tileX * 64;
        encoder.CompressNormalMapDXT5Fast_SSE2(tileRGBA, output, 16, 16);
      }
    }
  }
}

idParallelJobRegistration register_TranscodePage_DXT_DXT_upsample(
    reinterpret_cast<jobRun_t>(TranscodePage_DXT_DXT_upsample),
    "TranscodePage_DXT_DXT_upsample");

