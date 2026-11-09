#include "engine/renderer/jobs/transcode/transcode.h"

#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"
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

void DecompressCover16x16(const std::uint8_t* cover, int tileX, int tileY,
                          __m128i* rgba, int byteStride) {
  auto* pixels = reinterpret_cast<std::uint8_t*>(rgba);
  const std::uint8_t* coverTile = cover + tileY * 256 + tileX * 2;
  for (int y = 0; y < 16; ++y) {
    const std::uint8_t* coverRow = coverTile + y * 16;
    std::uint8_t* pixelRow = pixels + y * byteStride;
    for (int x = 0; x < 16; ++x) {
      pixelRow[x * 4 + 3] =
          (coverRow[x >> 3] & (1u << (x & 7))) != 0 ? 255 : 0;
    }
  }
}

void ConstantScaleSpecular16x16(std::uint8_t* specularScale, int tileX,
                                int tileY, const __m128i* rgba,
                                std::uint8_t specularShift) {
  auto* pixels = const_cast<__m128i*>(rgba);
  const __m128i shift = _mm_cvtsi32_si128(specularShift);
  for (int vector = 0; vector < 64; ++vector) {
    const __m128i source = _mm_load_si128(pixels + vector);
    pixels[vector] = _mm_or_si128(
        _mm_and_si128(_mm_sll_epi16(source, shift),
                      vector_byte_specular_mask1),
        _mm_and_si128(source, vector_byte_specular_mask0));
  }
  const std::uint8_t scale =
      static_cast<std::uint8_t>(8 * ((1 << specularShift) - 1));
  std::uint8_t* scaleBlock = specularScale + tileY * 128 + tileX * 4;
  for (int row = 0; row < 4; ++row) {
    std::memset(scaleBlock + row * 32, scale, 4);
  }
}

void DynamicScaleSpecular16x16(std::uint8_t* specularScale, int tileX,
                               int tileY, std::uint8_t* rgba) {
  for (int blockY = 0; blockY < 4; ++blockY) {
    for (int blockX = 0; blockX < 4; ++blockX) {
      int maximum = 0;
      for (int y = 0; y < 4; ++y) {
        const std::uint8_t* row =
            rgba + (blockY * 4 + y) * 64 + blockX * 16;
        for (int x = 0; x < 4; ++x) {
          maximum = maximum < row[x * 4 + 0] ? row[x * 4 + 0] : maximum;
          maximum = maximum < row[x * 4 + 1] ? row[x * 4 + 1] : maximum;
          maximum = maximum < row[x * 4 + 2] ? row[x * 4 + 2] : maximum;
        }
      }
      const int shift = maximum > 127 ? 0 : maximum > 63 ? 1 : 2;
      const __m128i shiftVector = _mm_cvtsi32_si128(shift);
      for (int y = 0; y < 4; ++y) {
        auto* row = reinterpret_cast<__m128i*>(
            rgba + (blockY * 4 + y) * 64 + blockX * 16);
        const __m128i source = _mm_load_si128(row);
        _mm_store_si128(
            row,
            _mm_or_si128(
                _mm_and_si128(_mm_sll_epi16(source, shiftVector),
                              vector_byte_specular_mask1),
                _mm_and_si128(source, vector_byte_specular_mask0)));
      }
      specularScale[(tileY * 4 + blockY) * 32 + tileX * 4 + blockX] =
          static_cast<std::uint8_t>((8 << shift) - 8);
    }
  }
}

}  // namespace

void TranscodePage_DCT_DXT_upsample(const transcodeParms_t* parms) {
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
  std::uint8_t* power = tempData;
  std::uint8_t* specularScale = tempData + 1024;
  std::uint8_t* sourceRows = tempData + 3072;
  std::uint8_t* tileRGBA = tempData + 19456;
  std::uint8_t* tiledOutput = tileRGBA + 1024;
  const auto* header = reinterpret_cast<const DCTHeader_t*>(parms->inPageHeader);
  const std::uint8_t* input = parms->inPageData;

  idBareDCTHuffmanTable tableYDC(HUFFMAN_TYPE_Y_DC, false);
  idBareDCTHuffmanTable tableYAC(HUFFMAN_TYPE_Y_AC, false);
  idBareDCTHuffmanTable tableCoCgDC(HUFFMAN_TYPE_COCG_DC, false);
  idBareDCTHuffmanTable tableCoCgAC(HUFFMAN_TYPE_COCG_AC, false);

  unsigned int sourceX = 0;
  unsigned int sourceY = 0;
  for (int level = 0, bit = 1; level < parms->upsampleLevel;
       ++level, bit <<= 1) {
    sourceX += (parms->upsampleX & bit) != 0 ? 124u * bit : 4u * bit;
    sourceY += (parms->upsampleY & bit) != 0 ? 124u * bit : 4u * bit;
  }

  const bool dynamicSpecular =
      (parms->imageFlags & 1) != 0 && (parms->imageFlags & 4) != 0 &&
      (header->flags & 3) != 2;
  idDxtEncoder encoder;

  if ((parms->imageFlags & 1) != 0) {
    idBareDctDecoder decoder;
    decoder.SetHuffmanTables(&tableYDC, &tableYAC, &tableCoCgDC,
                             &tableCoCgAC, nullptr, nullptr);
    decoder.SetQuality_SSE2(header->qualitySpecular,
                            header->qualitySpecular,
                            header->qualitySpecular);
    decoder.imageWidth = 128;
    decoder.imageHeight = 128;
    decoder.data = input + header->diffuseSize + header->normalSize;
    decoder.dataBytes = header->specularSize;
    std::memset(&decoder.dcY, 0, 24);
    encoder.srcPadding = 0;
    encoder.dstPadding =
        parms->tiled ? 0 : parms->targetBytePitch[0] - 32;
    const std::uint8_t* cover =
        input + header->diffuseSize + header->normalSize +
        header->specularSize + header->powerSize;
    const bool grayscale = (header->flags & 4) != 0;
    const bool hasCover = (header->flags & 8) != 0;
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
        if (decodedRow < neededFirst) {
          for (int tileX = 0; tileX < 8; ++tileX) {
            if (grayscale) {
              decoder.DecompressOneTileGrayScale_SSE2(tileRGBA, 64);
            } else {
              decoder.DecompressOneTileRGB_SSE2(tileRGBA, 64);
            }
          }
        } else {
          std::uint8_t* row = sourceRows + 8192 * (decodedRow & 1);
          for (int tileX = 0; tileX < 8; ++tileX) {
            std::uint8_t* tile = row + tileX * 64;
            if (grayscale) {
              decoder.DecompressOneTileGrayScale_SSE2(tile, 512);
            } else {
              decoder.DecompressOneTileRGB_SSE2(tile, 512);
            }
            if (hasCover) {
              DecompressCover16x16(cover, tileX, decodedRow,
                                   reinterpret_cast<__m128i*>(tile), 512);
            }
          }
        }
      }

      for (int tileX = 0; tileX < 8; ++tileX) {
        UpSample16x16(tileRGBA, tileX * 16, tileY * 16, 64, sourceRows,
                     sourceX, sourceY, 31, parms->upsampleLevel,
                     parms->upsampleFilter, 0, 0);
        if (dynamicSpecular) {
          DynamicScaleSpecular16x16(specularScale, tileX, tileY, tileRGBA);
        } else {
          ConstantScaleSpecular16x16(
              specularScale, tileX, tileY,
              reinterpret_cast<const __m128i*>(tileRGBA),
              header->flags & 3);
        }
        std::uint8_t* output =
            parms->tiled
                ? tiledOutput
                : parms->targetImage[0] +
                      tileY * 4 * parms->targetBytePitch[0] + tileX * 32;
        if (hasCover) {
          encoder.CompressImageDXT1AlphaFast_SSE2(tileRGBA, output, 16, 16);
        } else {
          encoder.CompressImageDXT1Fast_SSE2(tileRGBA, output, 16, 16);
        }
      }
    }
  }

  if ((parms->imageFlags & 2) != 0) {
    idBareDctDecoder decoder;
    decoder.SetHuffmanTables(&tableYDC, &tableYAC, &tableCoCgDC,
                             &tableCoCgAC, nullptr, nullptr);
    decoder.SetQuality_SSE2(header->qualityLuma, header->qualityChroma,
                            header->qualityLuma);
    decoder.imageWidth = 128;
    decoder.imageHeight = 128;
    decoder.data = input;
    decoder.dataBytes = header->diffuseSize;
    std::memset(&decoder.dcY, 0, 24);
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
        if (decodedRow < neededFirst) {
          for (int tileX = 0; tileX < 8; ++tileX) {
            decoder.DecompressOneTileYCoCg_SSE2(tileRGBA, 64);
          }
        } else {
          std::uint8_t* row = sourceRows + 8192 * (decodedRow & 1);
          for (int tileX = 0; tileX < 8; ++tileX) {
            decoder.DecompressOneTileYCoCg_SSE2(row + tileX * 64, 512);
          }
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
    if ((parms->imageFlags & 1) == 0) {
      std::memset(specularScale, 8 * (1 << (header->flags & 3)) - 8, 1024);
    }
    idBareDctDecoder powerDecoder;
    powerDecoder.SetHuffmanTables(&tableYDC, &tableYAC, nullptr, nullptr,
                                  nullptr, nullptr);
    powerDecoder.SetQuality_SSE2(header->qualityPower, header->qualityPower,
                                 header->qualityPower);
    powerDecoder.DecompressImageMono_SSE2(
        input + header->diffuseSize + header->normalSize +
            header->specularSize,
        power, 32, 32, header->powerSize);

    idBareDctDecoder decoder;
    decoder.SetHuffmanTables(&tableYDC, &tableYAC, nullptr, nullptr, nullptr,
                             nullptr);
    decoder.SetQuality_SSE2(header->qualityNormal, header->qualityNormal,
                            header->qualityNormal);
    decoder.imageWidth = 128;
    decoder.imageHeight = 128;
    decoder.data = input + header->diffuseSize;
    decoder.dataBytes = header->normalSize;
    std::memset(&decoder.dcY, 0, 24);
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
        if (decodedRow < neededFirst) {
          for (int tileX = 0; tileX < 8; ++tileX) {
            decoder.DecompressOneTileNormalMap_SSE2(tileRGBA, 64);
          }
        } else {
          std::uint8_t* row = sourceRows + 8192 * (decodedRow & 1);
          for (int tileX = 0; tileX < 8; ++tileX) {
            std::uint8_t* tile = row + tileX * 64;
            decoder.DecompressOneTileNormalMap_SSE2(tile, 512);
            for (int y = 0; y < 16; ++y) {
              const std::uint8_t* powerBlock =
                  power + (decodedRow * 4 + (y >> 2)) * 32 + tileX * 4;
              std::uint8_t* pixel = tile + y * 512;
              for (int x = 0; x < 16; ++x) {
                pixel[x * 4] = powerBlock[x >> 2];
              }
            }
          }
        }
      }

      for (int tileX = 0; tileX < 8; ++tileX) {
        UpSample16x16(tileRGBA, tileX * 16, tileY * 16, 64, sourceRows,
                     sourceX, sourceY, 31, parms->upsampleLevel,
                     parms->upsampleFilter, 0, 0);
        StoreSpecularScale16x16(specularScale, tileX, tileY, tileRGBA);
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

idParallelJobRegistration register_TranscodePage_DCT_DXT_upsample(
    reinterpret_cast<jobRun_t>(TranscodePage_DCT_DXT_upsample),
    "TranscodePage_DCT_DXT_upsample");
