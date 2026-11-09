#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"

#include <algorithm>
#include <cstring>

namespace {

constexpr int kDctNaturalOrder[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

std::uint8_t ClampByte(int value) {
  if (value <= 0)
    return 0;
  if (value > 255)
    return 255;
  return static_cast<std::uint8_t>(value);
}

int ExtendJpegValue(int value, int bitCount) {
  return value + ((1 - (1 << bitCount)) &
                  ((value - ((1 << bitCount) >> 1)) >> 31));
}

}  // namespace

void idBareDctDecoder::YCbCrToRGB(const std::int16_t* yCbCr,
                                  std::uint8_t* rgb, int stride) {
  for (int block = 0; block < 4; ++block) {
    const int blockX = (block & 1) * 8;
    const int blockY = (block & 2) * 4;
    const std::int16_t* luminance = yCbCr + block * 64;
    for (int y = 0; y < 8; ++y) {
      std::uint8_t* output = rgb + (blockY + y) * stride + blockX * 4;
      for (int x = 0; x < 8; ++x) {
        const int chromaIndex = (blockY / 2 + y / 2) * 8 + blockX / 2 + x / 2;
        const int cb = yCbCr[256 + chromaIndex];
        const int cr = yCbCr[320 + chromaIndex];
        const int yValue = luminance[y * 8 + x];
        const int redOffset = (91881 * cr + 0x8000) >> 16;
        const int greenOffset = (-46802 * cr - 22554 * cb + 0x8000) >> 16;
        const int blueOffset = (116130 * cb + 0x8000) >> 16;
        output[x * 4 + 0] = ClampByte(yValue + redOffset + 128);
        output[x * 4 + 1] = ClampByte(yValue + greenOffset + 128);
        output[x * 4 + 2] = ClampByte(yValue + blueOffset + 128);
        output[x * 4 + 3] = 255;
      }
    }
  }
}

int idBareDctDecoder::GetBits(int bits) {
  if (bitCnt < bits) {
    dataBytes -= 2;
    const int secondIndex = dataBytes >= 0 ? 1 : 0;
    bitBuff = (bitBuff << 16) | (static_cast<int>(data[0]) << 8) |
              data[secondIndex];
    bitCnt += 16;
    if (dataBytes > 0)
      data += 2;
  }
  bitCnt -= bits;
  return static_cast<int>(((1u << bits) - 1u) &
                          (static_cast<unsigned int>(bitBuff) >> bitCnt));
}

int idBareDctDecoder::PeekBits(int bits) {
  if (bitCnt < bits) {
    dataBytes -= 2;
    const int secondIndex = dataBytes >= 0 ? 1 : 0;
    bitBuff = (bitBuff << 16) | (static_cast<int>(data[0]) << 8) |
              data[secondIndex];
    bitCnt += 16;
    if (dataBytes > 0)
      data += 2;
  }
  return static_cast<int>(((1u << bits) - 1u) &
                          (static_cast<unsigned int>(bitBuff) >>
                           (bitCnt - bits)));
}

int idBareDctDecoder::DecodeLong(const idBareDCTHuffmanTable* table) {
  const int value = PeekBits(16);
  const int length = 9 + (value >= table->test_nbits[9]) +
                     (value >= table->test_nbits[10]) +
                     (value >= table->test_nbits[11]) +
                     (value >= table->test_nbits[12]) +
                     (value >= table->test_nbits[13]) +
                     (value >= table->test_nbits[14]) +
                     (value >= table->test_nbits[15]);
  bitCnt -= length;
  const std::uint8_t symbolIndex = static_cast<std::uint8_t>(
      static_cast<std::uint8_t>(table->symOffset[length]) +
      (value >> (16 - length)) -
      static_cast<std::uint8_t>(table->minCode[length]));
  return table->symbols[symbolIndex];
}

void idBareDctDecoder::HuffmanDecode(
    std::int16_t* coefficients, const idBareDCTHuffmanTable* dcTable,
    const idBareDCTHuffmanTable* acTable, int* lastDC) {
  std::memset(coefficients, 0, 64 * sizeof(*coefficients));

  int lookup = PeekBits(8);
  int symbol;
  if (dcTable->look_nbits[lookup] != 0) {
    bitCnt -= dcTable->look_nbits[lookup];
    symbol = dcTable->look_sym[lookup];
  } else {
    symbol = DecodeLong(dcTable);
  }
  if (symbol != 0)
    symbol = ExtendJpegValue(GetBits(symbol), symbol);
  *lastDC += symbol;
  coefficients[0] = static_cast<std::int16_t>(*lastDC);

  unsigned int coefficientIndex = 1;
  do {
    lookup = PeekBits(8);
    if (acTable->look_nbits[lookup] != 0) {
      bitCnt -= acTable->look_nbits[lookup];
      symbol = acTable->look_sym[lookup];
    } else {
      symbol = DecodeLong(acTable);
    }
    const int zeroRun = symbol >> 4;
    const int valueBits = symbol & 0xF;
    const unsigned int naturalIndex = coefficientIndex + zeroRun;
    if (valueBits != 0) {
      const int value = ExtendJpegValue(GetBits(valueBits), valueBits);
      coefficients[kDctNaturalOrder[naturalIndex]] =
          static_cast<std::int16_t>(value);
    } else if (zeroRun != 15) {
      return;
    }
    coefficientIndex = naturalIndex + 1;
  } while (coefficientIndex < 64);
}

void idBareDctDecoder::DecompressOneTileJPEG(std::uint8_t* rgb, int stride) {
  std::int16_t yCbCr[384];
  for (int block = 0; block < 4; ++block)
    HuffmanDecode(yCbCr + block * 64, pHuffTableYDC, pHuffTableYAC, &dcY);
  HuffmanDecode(yCbCr + 256, pHuffTableCoCgDC, pHuffTableCoCgAC, &dcCo);
  HuffmanDecode(yCbCr + 320, pHuffTableCoCgDC, pHuffTableCoCgAC, &dcCg);
  for (int block = 0; block < 4; ++block)
    IDCT_AP922_float(yCbCr + block * 64, quantTableY,
                     yCbCr + block * 64);
  IDCT_AP922_float(yCbCr + 256, quantTableCoCg, yCbCr + 256);
  IDCT_AP922_float(yCbCr + 320, quantTableCoCg, yCbCr + 320);
  YCbCrToRGB(yCbCr, rgb, stride);
}

bool idBareDctDecoder::DecompressImageRGB_JPEG(
    const std::uint8_t* input, std::uint8_t* output, int width, int height,
    int inputBytes) {
  data = input + 25;
  for (int index = 0; index < 64; ++index)
    quantTableY[kDctNaturalOrder[index]] = *data++;
  ++data;
  for (int index = 0; index < 64; ++index)
    quantTableCoCg[kDctNaturalOrder[index]] = *data++;
  data += 5;
  imageHeight = (static_cast<int>(data[0]) << 8) | data[1];
  data += 2;
  imageWidth = (static_cast<int>(data[0]) << 8) | data[1];
  data += 2;

  if (output == nullptr)
    return true;

  data = input + 607;
  dataBytes = inputBytes - 607;
  bitCnt = 0;
  bitBuff = 0;
  dcY = 0;
  dcCo = 0;
  dcCg = 0;

  const int tileColumns = (width + 15) >> 4;
  const int tileRows = (height + 15) >> 4;
  const int outputStride = width * 4;
  std::uint8_t tile[16 * 16 * 4];
  for (int tileY = 0; tileY < tileRows; ++tileY) {
    for (int tileX = 0; tileX < tileColumns; ++tileX) {
      DecompressOneTileJPEG(tile, 64);
      const int copyRows = std::min(16, height - tileY * 16);
      const int copyColumns = std::min(16, width - tileX * 16);
      std::uint8_t* destination =
          output + tileY * 16 * outputStride + tileX * 64;
      for (int row = 0; row < copyRows; ++row) {
        std::memmove(destination, tile + row * 64, copyColumns * 4);
        destination += outputStride;
      }
    }
  }
  return true;
}
