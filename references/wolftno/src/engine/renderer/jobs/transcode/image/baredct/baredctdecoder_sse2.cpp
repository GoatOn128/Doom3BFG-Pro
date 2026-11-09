#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"

#include <algorithm>
#include <cstdint>

namespace {

std::int16_t Saturate16(int value) {
  return static_cast<std::int16_t>(std::clamp(value, -32768, 32767));
}

std::int16_t AddSaturated(std::int16_t left, std::int16_t right) {
  return Saturate16(static_cast<int>(left) + right);
}

std::int16_t SubtractSaturated(std::int16_t left, std::int16_t right) {
  return Saturate16(static_cast<int>(left) - right);
}

std::uint8_t PackUnsigned(std::int16_t value) {
  return static_cast<std::uint8_t>(std::clamp(static_cast<int>(value), 0, 255));
}

int BlockIndex16(int x, int y) {
  return ((y >> 3) * 2 + (x >> 3)) * 64 + (y & 7) * 8 + (x & 7);
}

constexpr int kNaturalOrder[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

std::int16_t LevelShift(std::int16_t value) {
  return AddSaturated(value, 128);
}

}  // namespace

void idBareDctDecoder::StoreMono_SSE2(const std::int16_t* samples,
                                      std::uint8_t* mono, int stride) {
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x)
      mono[y * stride + x] = PackUnsigned(LevelShift(samples[y * 8 + x]));
  }
}

void idBareDctDecoder::StoreGrayScale_SSE2(const std::int16_t* samples,
                                           std::uint8_t* rgba, int stride) {
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const std::uint8_t gray =
          PackUnsigned(LevelShift(samples[y * 8 + x]));
      std::uint8_t* pixel = rgba + y * stride + x * 4;
      pixel[0] = gray;
      pixel[1] = gray;
      pixel[2] = gray;
      pixel[3] = 255;
    }
  }
}

void idBareDctDecoder::YCoCgToRGB_SSE2(const std::int16_t* yCoCg,
                                       std::uint8_t* rgba, int stride) {
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      const std::int16_t luminance = LevelShift(yCoCg[BlockIndex16(x, y)]);
      const int chromaIndex = (y >> 1) * 8 + (x >> 1);
      const std::int16_t co = yCoCg[256 + chromaIndex];
      const std::int16_t cg = yCoCg[320 + chromaIndex];
      const std::int16_t green = AddSaturated(luminance, cg);
      const std::int16_t red =
          AddSaturated(luminance, SubtractSaturated(co, cg));
      const std::int16_t blue =
          SubtractSaturated(luminance, AddSaturated(co, cg));
      std::uint8_t* pixel = rgba + y * stride + x * 4;
      pixel[0] = PackUnsigned(red);
      pixel[1] = PackUnsigned(green);
      pixel[2] = PackUnsigned(blue);
      pixel[3] = 255;
    }
  }
}

void idBareDctDecoder::StoreYCoCg420_SSE2(const std::int16_t* yCoCg,
                                          std::uint8_t* coCgY, int stride) {
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      const int chromaIndex = (y >> 1) * 8 + (x >> 1);
      std::uint8_t* pixel = coCgY + y * stride + x * 4;
      pixel[0] = PackUnsigned(yCoCg[256 + chromaIndex]);
      pixel[1] = PackUnsigned(yCoCg[320 + chromaIndex]);
      pixel[2] = 0;
      pixel[3] = PackUnsigned(LevelShift(yCoCg[BlockIndex16(x, y)]));
    }
  }
}

void idBareDctDecoder::StoreYCoCg444_SSE2(const std::int16_t* yCoCg,
                                          std::uint8_t* coCgY, int stride) {
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      const int index = BlockIndex16(x, y);
      std::uint8_t* pixel = coCgY + y * stride + x * 4;
      pixel[0] = PackUnsigned(yCoCg[256 + index]);
      pixel[1] = PackUnsigned(yCoCg[512 + index]);
      pixel[2] = 0;
      pixel[3] = PackUnsigned(LevelShift(yCoCg[index]));
    }
  }
}

void idBareDctDecoder::StoreNormalMap_SSE2(const std::int16_t* samples,
                                           std::uint8_t* normalMap,
                                           int stride) {
  for (int y = 0; y < 8; ++y) {
    for (int x = 0; x < 8; ++x) {
      const int index = y * 8 + x;
      std::uint8_t* pixel = normalMap + y * stride + x * 4;
      pixel[0] = 255;
      pixel[1] = PackUnsigned(LevelShift(samples[index]));
      pixel[2] = 255;
      pixel[3] = PackUnsigned(LevelShift(samples[64 + index]));
    }
  }
}

int idBareDctDecoder::FillBitBuffer_SSE2() {
  if (dataBytes <= 0)
    return 0;
  const int remaining = dataBytes - 2;
  dataBytes = remaining;
  const int secondIndex = remaining >= 0 ? 1 : 0;
  const std::uint32_t buffer =
      (static_cast<std::uint32_t>(bitBuff) << 16) |
      (static_cast<std::uint32_t>(data[0]) << 8) | data[secondIndex];
  bitBuff = static_cast<int>(buffer);
  bitCnt += 16;
  if (remaining > 0)
    data += 2;
  return 2;
}

int idBareDctDecoder::DecodeLong_SSE2(
    const idBareDCTHuffmanTable* table) {
  if (bitCnt < 16 && FillBitBuffer_SSE2() == 0)
    return 0;
  const int bits = static_cast<std::uint16_t>(
      static_cast<std::uint32_t>(bitBuff) >> (bitCnt - 16));
  const int length = 9 + (bits >= table->test_nbits[9]) +
                     (bits >= table->test_nbits[10]) +
                     (bits >= table->test_nbits[11]) +
                     (bits >= table->test_nbits[12]) +
                     (bits >= table->test_nbits[13]) +
                     (bits >= table->test_nbits[14]) +
                     (bits >= table->test_nbits[15]);
  bitCnt -= length;
  const std::uint8_t symbolIndex = static_cast<std::uint8_t>(
      static_cast<std::uint8_t>(table->symOffset[length]) +
      (bits >> (16 - length)) -
      static_cast<std::uint8_t>(table->minCode[length]));
  return table->symbols[symbolIndex];
}

int idBareDctDecoder::GetValue_SSE2(int category) {
  if (bitCnt < category && FillBitBuffer_SSE2() == 0)
    return 0;
  bitCnt -= category;
  const unsigned int limit = 1u << category;
  const int value = static_cast<int>(
      (limit - 1u) & (static_cast<unsigned int>(bitBuff) >> bitCnt));
  return value +
         ((1 - static_cast<int>(limit)) &
          ((value - static_cast<int>(limit >> 1)) >> 31));
}

int idBareDctDecoder::GetCategory_SSE2(
    const idBareDCTHuffmanTable* table) {
  if (bitCnt < 8 && FillBitBuffer_SSE2() == 0)
    return 0;
  const std::uint8_t lookup = static_cast<std::uint8_t>(
      static_cast<unsigned int>(bitBuff) >> (bitCnt - 8));
  if (table->look_nbits[lookup] != 0) {
    bitCnt -= table->look_nbits[lookup];
    return table->look_sym[lookup];
  }
  return DecodeLong_SSE2(table);
}

void idBareDctDecoder::HuffmanDecode_SSE2(
    std::int16_t* coefficients, const idBareDCTHuffmanTable* dcTable,
    const idBareDCTHuffmanTable* acTable, int* lastDC) {
  std::fill(coefficients, coefficients + 64, std::int16_t{0});

  const int dcCategory = GetCategory_SSE2(dcTable);
  *lastDC += dcCategory == 0 ? 0 : GetValue_SSE2(dcCategory);
  coefficients[0] = static_cast<std::int16_t>(*lastDC);

  unsigned int coefficient = 1;
  do {
    const int symbol = GetCategory_SSE2(acTable);
    const int zeroRun = symbol >> 4;
    const int category = symbol & 15;
    const unsigned int zigZagIndex = coefficient + zeroRun;
    if (category != 0) {
      coefficients[kNaturalOrder[zigZagIndex]] =
          static_cast<std::int16_t>(GetValue_SSE2(category));
    } else if (zeroRun != 15) {
      return;
    }
    coefficient = zigZagIndex + 1;
  } while (coefficient < 64);
}

void idBareDctDecoder::DecompressOneTileGrayScale_SSE2(
    std::uint8_t* rgba, int stride) {
  alignas(16) std::int16_t samples[256];
  for (int block = 0; block < 4; ++block) {
    HuffmanDecode_SSE2(samples + block * 64, pHuffTableYDC,
                       pHuffTableYAC, &dcY);
    IDCT_AP922_int_w_SSE2(samples + block * 64, quantTableY,
                          samples + block * 64);
  }
  StoreGrayScale_SSE2(samples, rgba, stride);
  StoreGrayScale_SSE2(samples + 64, rgba + 32, stride);
  StoreGrayScale_SSE2(samples + 128, rgba + 8 * stride, stride);
  StoreGrayScale_SSE2(samples + 192, rgba + 8 * stride + 32, stride);
}

void idBareDctDecoder::DecompressOneTileRGB_SSE2(std::uint8_t* rgba,
                                                 int stride) {
  alignas(16) std::int16_t yCoCg[384];
  for (int block = 0; block < 4; ++block)
    HuffmanDecode_SSE2(yCoCg + block * 64, pHuffTableYDC, pHuffTableYAC,
                       &dcY);
  HuffmanDecode_SSE2(yCoCg + 256, pHuffTableCoCgDC, pHuffTableCoCgAC,
                     &dcCo);
  HuffmanDecode_SSE2(yCoCg + 320, pHuffTableCoCgDC, pHuffTableCoCgAC,
                     &dcCg);
  for (int block = 0; block < 4; ++block)
    IDCT_AP922_int_w_SSE2(yCoCg + block * 64, quantTableY,
                          yCoCg + block * 64);
  IDCT_AP922_int_w_SSE2(yCoCg + 256, quantTableCoCg, yCoCg + 256);
  IDCT_AP922_int_w_SSE2(yCoCg + 320, quantTableCoCg, yCoCg + 320);
  YCoCgToRGB_SSE2(yCoCg, rgba, stride);
}

void idBareDctDecoder::DecompressOneTileYCoCg_SSE2(std::uint8_t* coCgY,
                                                   int stride) {
  alignas(16) std::int16_t yCoCg[768];
  for (int block = 0; block < 4; ++block)
    HuffmanDecode_SSE2(yCoCg + block * 64, pHuffTableYDC, pHuffTableYAC,
                       &dcY);

  if (sampleMode == SAMPLE_420) {
    HuffmanDecode_SSE2(yCoCg + 256, pHuffTableCoCgDC, pHuffTableCoCgAC,
                       &dcCo);
    HuffmanDecode_SSE2(yCoCg + 320, pHuffTableCoCgDC, pHuffTableCoCgAC,
                       &dcCg);
    for (int block = 0; block < 4; ++block)
      IDCT_AP922_int_w_SSE2(yCoCg + block * 64, quantTableY,
                            yCoCg + block * 64);
    IDCT_AP922_int_w_SSE2(yCoCg + 256, quantTableCoCg, yCoCg + 256);
    IDCT_AP922_int_w_SSE2(yCoCg + 320, quantTableCoCg, yCoCg + 320);
    StoreYCoCg420_SSE2(yCoCg, coCgY, stride);
    return;
  }

  for (int block = 0; block < 4; ++block)
    HuffmanDecode_SSE2(yCoCg + 256 + block * 64, pHuffTableCoCgDC,
                       pHuffTableCoCgAC, &dcCo);
  for (int block = 0; block < 4; ++block)
    HuffmanDecode_SSE2(yCoCg + 512 + block * 64, pHuffTableCoCgDC,
                       pHuffTableCoCgAC, &dcCg);
  for (int block = 0; block < 4; ++block)
    IDCT_AP922_int_w_SSE2(yCoCg + block * 64, quantTableY,
                          yCoCg + block * 64);
  for (int block = 4; block < 12; ++block)
    IDCT_AP922_int_w_SSE2(yCoCg + block * 64, quantTableCoCg,
                          yCoCg + block * 64);
  StoreYCoCg444_SSE2(yCoCg, coCgY, stride);
}

void idBareDctDecoder::DecompressOneTileNormalMap_SSE2(
    std::uint8_t* normalMap, int stride) {
  alignas(16) std::int16_t samples[512];
  for (int block = 0; block < 4; ++block) {
    HuffmanDecode_SSE2(samples + block * 128, pHuffTableYDC,
                       pHuffTableYAC, &dcY);
    HuffmanDecode_SSE2(samples + block * 128 + 64, pHuffTableYDC,
                       pHuffTableYAC, &dcA);
  }
  for (int block = 0; block < 8; ++block)
    IDCT_AP922_int_w_SSE2(samples + block * 64, quantTableY,
                          samples + block * 64);
  StoreNormalMap_SSE2(samples, normalMap, stride);
  StoreNormalMap_SSE2(samples + 128, normalMap + 32, stride);
  StoreNormalMap_SSE2(samples + 256, normalMap + 8 * stride, stride);
  StoreNormalMap_SSE2(samples + 384, normalMap + 8 * stride + 32, stride);
}

bool idBareDctDecoder::DecompressImageMono_SSE2(
    const std::uint8_t* input, std::uint8_t* output, int width, int height,
    int inputBytes) {
  imageWidth = width;
  imageHeight = height;
  data = input;
  dataBytes = inputBytes;
  bitCnt = bitBuff = 0;
  dcY = dcCo = dcCg = dcA = 0;
  const int columns = (width + 15) >> 4;
  const int rows = (height + 15) >> 4;
  alignas(16) std::int16_t samples[4][64];
  for (int tileY = 0; tileY < rows; ++tileY) {
    for (int tileX = 0; tileX < columns; ++tileX) {
      for (int block = 0; block < 4; ++block) {
        HuffmanDecode_SSE2(samples[block], pHuffTableYDC, pHuffTableYAC,
                           &dcY);
        IDCT_AP922_int_w_SSE2(samples[block], quantTableY, samples[block]);
      }
      std::uint8_t* tile = output + tileY * 16 * width + tileX * 16;
      StoreMono_SSE2(samples[0], tile, width);
      StoreMono_SSE2(samples[1], tile + 8, width);
      StoreMono_SSE2(samples[2], tile + 8 * width, width);
      StoreMono_SSE2(samples[3], tile + 8 * width + 8, width);
    }
  }
  return true;
}

bool idBareDctDecoder::DecompressImageGrayScale_SSE2(
    const std::uint8_t* input, std::uint8_t* output, int width, int height,
    int inputBytes) {
  imageWidth = width;
  imageHeight = height;
  data = input;
  dataBytes = inputBytes;
  bitCnt = bitBuff = 0;
  dcY = dcCo = dcCg = dcA = 0;
  const int columns = (width + 15) >> 4;
  const int rows = (height + 15) >> 4;
  const int stride = width * 4;
  for (int tileY = 0; tileY < rows; ++tileY) {
    for (int tileX = 0; tileX < columns; ++tileX) {
      DecompressOneTileGrayScale_SSE2(
          output + tileY * 16 * stride + tileX * 64, stride);
    }
  }
  return true;
}

bool idBareDctDecoder::DecompressImageRGB_SSE2(
    const std::uint8_t* input, std::uint8_t* output, int width, int height,
    int inputBytes) {
  imageWidth = width;
  imageHeight = height;
  data = input;
  dataBytes = inputBytes;
  bitCnt = bitBuff = 0;
  dcY = dcCo = dcCg = dcA = 0;
  const int columns = (width + 15) >> 4;
  const int rows = (height + 15) >> 4;
  const int stride = width * 4;
  for (int tileY = 0; tileY < rows; ++tileY) {
    for (int tileX = 0; tileX < columns; ++tileX) {
      DecompressOneTileRGB_SSE2(
          output + tileY * 16 * stride + tileX * 64, stride);
    }
  }
  return true;
}

bool idBareDctDecoder::DecompressImageYCoCg_SSE2(
    const std::uint8_t* input, std::uint8_t* output, int width, int height,
    int inputBytes) {
  imageWidth = width;
  imageHeight = height;
  data = input;
  dataBytes = inputBytes;
  bitCnt = bitBuff = 0;
  dcY = dcCo = dcCg = dcA = 0;
  const int columns = (width + 15) >> 4;
  const int rows = (height + 15) >> 4;
  const int stride = width * 4;
  for (int tileY = 0; tileY < rows; ++tileY) {
    for (int tileX = 0; tileX < columns; ++tileX) {
      DecompressOneTileYCoCg_SSE2(
          output + tileY * 16 * stride + tileX * 64, stride);
    }
  }
  return true;
}

bool idBareDctDecoder::DecompressImageNormalMap_SSE2(
    const std::uint8_t* input, std::uint8_t* output, int width, int height,
    int inputBytes) {
  imageWidth = width;
  imageHeight = height;
  data = input;
  dataBytes = inputBytes;
  bitCnt = bitBuff = 0;
  dcY = dcCo = dcCg = dcA = 0;
  const int columns = (width + 15) >> 4;
  const int rows = (height + 15) >> 4;
  const int stride = width * 4;
  for (int tileY = 0; tileY < rows; ++tileY) {
    for (int tileX = 0; tileX < columns; ++tileX) {
      DecompressOneTileNormalMap_SSE2(
          output + tileY * 16 * stride + tileX * 64, stride);
    }
  }
  return true;
}
