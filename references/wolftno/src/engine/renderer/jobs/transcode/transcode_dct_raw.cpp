#include "engine/renderer/jobs/transcode/transcode.h"

#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"
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

void CopyRawTile(const std::uint8_t* tile, std::uint8_t* target,
                 int targetPitch, int tileX, int tileY) {
  std::uint8_t* destination =
      target + tileY * 16 * targetPitch + tileX * 64;
  for (int row = 0; row < 16; ++row) {
    std::memcpy(destination, tile + row * 64, 64);
    destination += targetPitch;
  }
}

void BroadcastNormalMetadata(const std::uint8_t* power,
                             const std::uint8_t* specularScale, int tileX,
                             int tileY, std::uint8_t* rgba) {
  for (int blockY = 0; blockY < 4; ++blockY) {
    for (int blockX = 0; blockX < 4; ++blockX) {
      const int metadataIndex =
          (tileY * 4 + blockY) * 32 + tileX * 4 + blockX;
      const std::uint8_t powerValue = power[metadataIndex];
      const std::uint8_t scaleValue = specularScale[metadataIndex];
      for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x) {
          std::uint8_t* pixel =
              rgba + (blockY * 4 + y) * 64 + (blockX * 4 + x) * 4;
          pixel[0] = powerValue;
          pixel[2] = scaleValue;
        }
      }
    }
  }
}

}  // namespace

void TranscodePage_DCT_RAW(const transcodeParms_t* parms) {
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
  auto* tileRGBA = reinterpret_cast<__m128i*>(tempData + 2048);
  const auto* header = reinterpret_cast<const DCTHeader_t*>(parms->inPageHeader);
  const std::uint8_t* input = parms->inPageData;

  idBareDCTHuffmanTable tableYDC(HUFFMAN_TYPE_Y_DC, false);
  idBareDCTHuffmanTable tableYAC(HUFFMAN_TYPE_Y_AC, false);
  idBareDCTHuffmanTable tableCoCgDC(HUFFMAN_TYPE_COCG_DC, false);
  idBareDCTHuffmanTable tableCoCgAC(HUFFMAN_TYPE_COCG_AC, false);
  const bool dynamicSpecular =
      (parms->imageFlags & 1) != 0 && (parms->imageFlags & 4) != 0 &&
      (header->flags & 3) != 2;

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
    const std::uint8_t* cover =
        input + header->diffuseSize + header->normalSize +
        header->specularSize + header->powerSize;
    const bool grayscale = (header->flags & 4) != 0;
    const bool hasCover = (header->flags & 8) != 0;
    for (int tileY = 0; tileY < 8; ++tileY) {
      for (int tileX = 0; tileX < 8; ++tileX) {
        if (grayscale) {
          decoder.DecompressOneTileGrayScale_SSE2(
              reinterpret_cast<std::uint8_t*>(tileRGBA), 64);
        } else {
          decoder.DecompressOneTileRGB_SSE2(
              reinterpret_cast<std::uint8_t*>(tileRGBA), 64);
        }
        if (dynamicSpecular) {
          DynamicScaleSpecular16x16(specularScale, tileX, tileY,
                                    reinterpret_cast<std::uint8_t*>(tileRGBA));
        } else {
          ConstantScaleSpecular16x16(specularScale, tileX, tileY, tileRGBA,
                                     header->flags & 3);
        }
        if (hasCover) {
          DecompressCover16x16(cover, tileX, tileY, tileRGBA, 64);
        }
        CopyRawTile(reinterpret_cast<const std::uint8_t*>(tileRGBA),
                    parms->targetImage[0], parms->targetBytePitch[0], tileX,
                    tileY);
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
    for (int tileY = 0; tileY < 8; ++tileY) {
      for (int tileX = 0; tileX < 8; ++tileX) {
        decoder.DecompressOneTileYCoCg_SSE2(
            reinterpret_cast<std::uint8_t*>(tileRGBA), 64);
        CopyRawTile(reinterpret_cast<const std::uint8_t*>(tileRGBA),
                    parms->targetImage[1], parms->targetBytePitch[1], tileX,
                    tileY);
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

    idBareDctDecoder normalDecoder;
    normalDecoder.SetHuffmanTables(&tableYDC, &tableYAC, nullptr, nullptr,
                                   nullptr, nullptr);
    normalDecoder.SetQuality_SSE2(header->qualityNormal,
                                  header->qualityNormal,
                                  header->qualityNormal);
    normalDecoder.imageWidth = 128;
    normalDecoder.imageHeight = 128;
    normalDecoder.data = input + header->diffuseSize;
    normalDecoder.dataBytes = header->normalSize;
    std::memset(&normalDecoder.dcY, 0, 24);
    for (int tileY = 0; tileY < 8; ++tileY) {
      for (int tileX = 0; tileX < 8; ++tileX) {
        normalDecoder.DecompressOneTileNormalMap_SSE2(
            reinterpret_cast<std::uint8_t*>(tileRGBA), 64);
        BroadcastNormalMetadata(power, specularScale, tileX, tileY,
                                reinterpret_cast<std::uint8_t*>(tileRGBA));
        CopyRawTile(reinterpret_cast<const std::uint8_t*>(tileRGBA),
                    parms->targetImage[2], parms->targetBytePitch[2], tileX,
                    tileY);
      }
    }
  }
}

idParallelJobRegistration register_TranscodePage_DCT_RAW(
    reinterpret_cast<jobRun_t>(TranscodePage_DCT_RAW),
    "TranscodePage_DCT_RAW");
