#include "engine/renderer/jobs/transcode/transcode.h"

#include "engine/renderer/jobs/transcode/image/barehdp/barehdp.h"
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

void TranscodePage_HDP_RAW(const transcodeParms_t* parms) {
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
  std::uint8_t* power = tempData + 49152;
  std::uint8_t* specularScale = tempData + 50176;
  auto* tileRGBA = reinterpret_cast<__m128i*>(tempData + 51200);
  const auto* header = reinterpret_cast<const HDPHeader_t*>(parms->inPageHeader);
  const std::uint8_t* input = parms->inPageData;
  const bool dynamicSpecular =
      (parms->imageFlags & 1) != 0 && (parms->imageFlags & 4) != 0 &&
      (header->flags & 3) != 2;

  if ((parms->imageFlags & 1) != 0) {
    idBareHDP decoder;
    decoder.quality = header->qualitySpecular;
    decoder.tempBuffer = tempData;
    decoder.tempBufferSize = 49152;
    const bool grayscale = (header->flags & 4) != 0;
    if (grayscale) {
      decoder.BeginDecompressImageGrayScale(
          input + header->diffuseSize + header->normalSize, 128, 128,
          header->specularSize);
    } else {
      decoder.BeginDecompressImageRGB(
          input + header->diffuseSize + header->normalSize, 128, 128,
          header->specularSize);
    }
    const bool hasCover = (header->flags & 8) != 0;
    const std::uint8_t* cover =
        input + header->diffuseSize + header->normalSize +
        header->specularSize + header->powerSize;
    for (int tileY = 0; tileY < 8; ++tileY) {
      decoder.BeginDecompressImageGrayScaleMacroBlockRow();
      for (int tileX = 0; tileX < 8; ++tileX) {
        if (grayscale) {
          decoder.DecompressImageGrayScaleMacroBlock(
              reinterpret_cast<std::uint8_t*>(tileRGBA), tileX, 64);
        } else {
          decoder.DecompressImageRGBMacroBlock(
              reinterpret_cast<std::uint8_t*>(tileRGBA), tileX, 64);
        }
        if (dynamicSpecular) {
          DynamicScaleSpecular16x16(
              specularScale, tileX, tileY,
              reinterpret_cast<std::uint8_t*>(tileRGBA));
        } else {
          ConstantScaleSpecular16x16(specularScale, tileX, tileY, tileRGBA,
                                     header->flags & 3);
        }
        if (hasCover) {
          DecompressCover16x16(cover, tileX, tileY, tileRGBA, 64);
        }
        std::uint8_t* destination =
            parms->targetImage[0] + tileY * 16 * parms->targetBytePitch[0] +
            tileX * 64;
        const auto* tile = reinterpret_cast<const std::uint8_t*>(tileRGBA);
        for (int row = 0; row < 16; ++row) {
          std::memcpy(destination + row * parms->targetBytePitch[0],
                      tile + row * 64, 64);
        }
      }
    }
    decoder.TerminateMacroBlockRowDecoding();
  }

  if ((parms->imageFlags & 2) != 0) {
    idBareHDP decoder;
    decoder.quality = header->qualityDiffuse;
    decoder.tempBuffer = tempData;
    decoder.tempBufferSize = 49152;
    decoder.BeginDecompressImageYCoCg(input, 128, 128,
                                      header->diffuseSize);
    for (int tileY = 0; tileY < 8; ++tileY) {
      decoder.BeginDecompressImageGrayScaleMacroBlockRow();
      for (int tileX = 0; tileX < 8; ++tileX) {
        decoder.DecompressImageYCoCgMacroBlock(
            reinterpret_cast<std::uint8_t*>(tileRGBA), tileX, 64);
        std::uint8_t* destination =
            parms->targetImage[1] + tileY * 16 * parms->targetBytePitch[1] +
            tileX * 64;
        const auto* tile = reinterpret_cast<const std::uint8_t*>(tileRGBA);
        for (int row = 0; row < 16; ++row) {
          std::memcpy(destination + row * parms->targetBytePitch[1],
                      tile + row * 64, 64);
        }
      }
    }
    decoder.TerminateMacroBlockRowDecoding();
  }

  if ((parms->imageFlags & 4) != 0) {
    if ((parms->imageFlags & 1) == 0) {
      std::memset(specularScale, 8 * (1 << (header->flags & 3)) - 8, 1024);
    }
    idBareHDP decoder;
    decoder.tempBuffer = tempData;
    decoder.tempBufferSize = 49152;
    decoder.quality = header->qualityPower;
    decoder.DecompressImageMono(
        input + header->diffuseSize + header->normalSize +
            header->specularSize,
        power, 32, 32, header->powerSize, 0);
    decoder.quality = header->qualityNormal;
    decoder.BeginDecompressImageNormalMap(input + header->diffuseSize, 128,
                                          128, header->normalSize);
    for (int tileY = 0; tileY < 8; ++tileY) {
      decoder.BeginDecompressImageGrayScaleMacroBlockRow();
      for (int tileX = 0; tileX < 8; ++tileX) {
        decoder.DecompressImageNormalMapMacroBlock(
            reinterpret_cast<std::uint8_t*>(tileRGBA), tileX, 64);
        auto* tile = reinterpret_cast<std::uint8_t*>(tileRGBA);
        for (int blockY = 0; blockY < 4; ++blockY) {
          for (int blockX = 0; blockX < 4; ++blockX) {
            const int metadataIndex =
                (tileY * 4 + blockY) * 32 + tileX * 4 + blockX;
            const std::uint8_t powerValue = power[metadataIndex];
            const std::uint8_t scaleValue = specularScale[metadataIndex];
            for (int y = 0; y < 4; ++y) {
              for (int x = 0; x < 4; ++x) {
                std::uint8_t* pixel =
                    tile + (blockY * 4 + y) * 64 +
                    (blockX * 4 + x) * 4;
                pixel[0] = powerValue;
                pixel[2] = scaleValue;
              }
            }
          }
        }
        std::uint8_t* destination =
            parms->targetImage[2] + tileY * 16 * parms->targetBytePitch[2] +
            tileX * 64;
        for (int row = 0; row < 16; ++row) {
          std::memcpy(destination + row * parms->targetBytePitch[2],
                      tile + row * 64, 64);
        }
      }
    }
    decoder.TerminateMacroBlockRowDecoding();
  }
}

idParallelJobRegistration register_TranscodePage_HDP_RAW(
    reinterpret_cast<jobRun_t>(TranscodePage_HDP_RAW),
    "TranscodePage_HDP_RAW");



