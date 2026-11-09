#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

constexpr int kNaturalOrder[64] = {
    0,  1,  8,  16, 9,  2,  3,  10, 17, 24, 32, 25, 18, 11, 4,  5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6,  7,  14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63,
};

constexpr std::uint8_t kBitsYDC[17] = {
    0, 0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
constexpr std::uint8_t kBitsYAC[17] = {
    0, 0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7D};
constexpr std::uint8_t kBitsCoCgDC[17] = {
    0, 0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
constexpr std::uint8_t kBitsCoCgAC[17] = {
    0, 0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77};

int Luminance(const std::uint8_t* rgba) {
  return static_cast<int>((7471u * rgba[2] + 19595u * rgba[0] +
                           38470u * rgba[1] + 0x8000u) >>
                          16) -
         128;
}

int ChromaBlue(const std::uint8_t* rgba) {
  return (-(11059 * static_cast<int>(rgba[0])) -
          21709 * static_cast<int>(rgba[1]) +
          (static_cast<int>(rgba[2]) << 15) + 0x8000) >>
         16;
}

int ChromaRed(const std::uint8_t* rgba) {
  return ((static_cast<int>(rgba[0]) << 15) -
          27439 * static_cast<int>(rgba[1]) -
          5329 * static_cast<int>(rgba[2]) + 0x8000) >>
         16;
}

void WriteByte(std::uint8_t*& output, std::uint8_t value) {
  *output++ = value;
}

void WriteWord(std::uint8_t*& output, int value) {
  WriteByte(output, static_cast<std::uint8_t>(value >> 8));
  WriteByte(output, static_cast<std::uint8_t>(value));
}

void WriteDht(std::uint8_t*& output, std::uint8_t tableInfo,
              const std::uint8_t (&bits)[17],
              const idBareDCTHuffmanTable& table, int symbolCount) {
  WriteByte(output, tableInfo);
  for (int length = 1; length <= 16; ++length)
    WriteByte(output, bits[length]);
  for (int symbol = 0; symbol < symbolCount; ++symbol)
    WriteByte(output, table.symbols[symbol]);
}

}  // namespace

idBareDctEncoder::idBareDctEncoder()
    : idBareDctBase(),
      huffTableYDC(HUFFMAN_TYPE_Y_DC, true),
      huffTableYAC(HUFFMAN_TYPE_Y_AC, true),
      huffTableCoCgDC(HUFFMAN_TYPE_COCG_DC, true),
      huffTableCoCgAC(HUFFMAN_TYPE_COCG_AC, true),
      huffTableADC(HUFFMAN_TYPE_Y_DC, true),
      huffTableAAC(HUFFMAN_TYPE_Y_AC, true),
      imageWidth(0),
      imageHeight(0),
      dcY(0),
      dcCo(0),
      dcCg(0),
      dcA(0),
      bitCnt(0),
      bitBuff(0),
      dataBytes(0),
      data(nullptr),
      useMarkers(false) {
  InitQuantTable();
}

void idBareDctEncoder::RGBToYCbCrTile(const std::uint8_t* rgba,
                                      std::int16_t* yCbCr, int stride) {
  for (int y = 0; y < 16; ++y) {
    for (int x = 0; x < 16; ++x) {
      const int block = (y >> 3) * 2 + (x >> 3);
      const int coefficient = (y & 7) * 8 + (x & 7);
      yCbCr[block * 64 + coefficient] =
          static_cast<std::int16_t>(Luminance(rgba + y * stride + x * 4));
    }
  }
  for (int y = 0; y < 16; y += 2) {
    for (int x = 0; x < 16; x += 2) {
      int cb = 0;
      int cr = 0;
      for (int subY = 0; subY < 2; ++subY) {
        for (int subX = 0; subX < 2; ++subX) {
          const std::uint8_t* pixel =
              rgba + (y + subY) * stride + (x + subX) * 4;
          cb += ChromaBlue(pixel);
          cr += ChromaRed(pixel);
        }
      }
      const int chromaIndex = (y >> 1) * 8 + (x >> 1);
      yCbCr[256 + chromaIndex] = static_cast<std::int16_t>(cb >> 2);
      yCbCr[320 + chromaIndex] = static_cast<std::int16_t>(cr >> 2);
    }
  }
}

bool idBareDctEncoder::EmitBits(std::uint8_t* output, unsigned int code,
                                int size) {
  if (size == 0)
    return false;
  int totalBits = bitCnt + size;
  std::uint32_t buffer = static_cast<std::uint32_t>(bitBuff) |
                         ((code & ((1u << size) - 1u)) <<
                          (24 - (size + static_cast<std::uint8_t>(bitCnt))));
  const unsigned int outputByteCount =
      static_cast<unsigned int>(totalBits) >> 3;
  totalBits -= static_cast<int>(outputByteCount * 8);
  for (unsigned int index = 0; index < outputByteCount; ++index) {
    *output++ = static_cast<std::uint8_t>(buffer >> 16);
    if (useMarkers && output[-1] == 0xFF) {
      *output++ = 0;
      ++dataBytes;
    }
    ++dataBytes;
    buffer <<= 8;
  }
  bitBuff = static_cast<int>(buffer);
  bitCnt = totalBits;
  return true;
}

bool idBareDctEncoder::HuffmanEncode(
    const std::int16_t* coefficients, const idBareDCTHuffmanTable* dcTable,
    const idBareDCTHuffmanTable* acTable, int* lastDC) {
  int difference = coefficients[0] - *lastDC;
  *lastDC = coefficients[0];
  int magnitude = difference;
  if (difference < 0)
    magnitude = -difference--;
  int magnitudeBits = 0;
  for (int value = magnitude; value != 0; value >>= 1)
    ++magnitudeBits;
  if (!EmitBits(data + dataBytes, dcTable->code[magnitudeBits],
                dcTable->size[magnitudeBits]) ||
      (magnitudeBits != 0 &&
       !EmitBits(data + dataBytes, static_cast<unsigned int>(difference),
                 magnitudeBits))) {
    return false;
  }

  int zeroRun = 0;
  for (int zigZag = 1; zigZag < 64; ++zigZag) {
    int value = coefficients[kNaturalOrder[zigZag]];
    if (value == 0) {
      ++zeroRun;
      continue;
    }
    while (zeroRun > 15) {
      if (!EmitBits(data + dataBytes, acTable->code[0xF0],
                    acTable->size[0xF0]))
        return false;
      zeroRun -= 16;
    }
    unsigned int encodedValue = static_cast<unsigned int>(value);
    if (value < 0) {
      value = -value;
      --encodedValue;
    }
    int valueBits = 1;
    for (int shifted = value >> 1; shifted != 0; shifted >>= 1)
      ++valueBits;
    const int symbol = zeroRun * 16 + valueBits;
    if (!EmitBits(data + dataBytes, acTable->code[symbol],
                  acTable->size[symbol]) ||
        !EmitBits(data + dataBytes, encodedValue, valueBits)) {
      return false;
    }
    zeroRun = 0;
  }
  return zeroRun <= 0 ||
         EmitBits(data + dataBytes, acTable->code[0], acTable->size[0]);
}

void idBareDctEncoder::CompressOneTileJPEG(const std::uint8_t* rgba,
                                           int stride) {
  std::int16_t yCbCr[384];
  RGBToYCbCrTile(rgba, yCbCr, stride);
  for (int block = 0; block < 4; ++block)
    FDCT_AP922_float(yCbCr + block * 64, quantTableY,
                     yCbCr + block * 64);
  FDCT_AP922_float(yCbCr + 256, quantTableCoCg, yCbCr + 256);
  FDCT_AP922_float(yCbCr + 320, quantTableCoCg, yCbCr + 320);
  for (int block = 0; block < 4; ++block)
    HuffmanEncode(yCbCr + block * 64, &huffTableYDC, &huffTableYAC, &dcY);
  HuffmanEncode(yCbCr + 256, &huffTableCoCgDC, &huffTableCoCgAC, &dcCo);
  HuffmanEncode(yCbCr + 320, &huffTableCoCgDC, &huffTableCoCgAC, &dcCg);
}

bool idBareDctEncoder::CompressImageRGB_JPEG(
    const std::uint8_t* input, std::uint8_t* output, int width, int height,
    int* outputBytes, bool useMarkersValue) {
  data = output;
  useMarkers = useMarkersValue;
  bitCnt = bitBuff = 0;
  dcY = dcCo = dcCg = dcA = 0;
  imageWidth = width;
  imageHeight = height;

  std::uint8_t* header = output;
  WriteByte(header, 0xFF);
  WriteByte(header, 0xD8);
  WriteByte(header, 0xFF);
  WriteByte(header, 0xE0);
  WriteWord(header, 16);
  const std::uint8_t jfif[] = {'J', 'F', 'I', 'F', 0, 1, 1, 0,
                               0,   1,   0,   1,   0, 0};
  for (std::uint8_t value : jfif)
    WriteByte(header, value);

  WriteByte(header, 0xFF);
  WriteByte(header, 0xDB);
  WriteWord(header, 132);
  WriteByte(header, 0);
  for (int index : kNaturalOrder)
    WriteByte(header, static_cast<std::uint8_t>(quantTableY[index]));
  WriteByte(header, 1);
  for (int index : kNaturalOrder)
    WriteByte(header, static_cast<std::uint8_t>(quantTableCoCg[index]));

  WriteByte(header, 0xFF);
  WriteByte(header, 0xC0);
  WriteWord(header, 17);
  WriteByte(header, 8);
  WriteWord(header, height);
  WriteWord(header, width);
  WriteByte(header, 3);
  WriteByte(header, 1);
  WriteByte(header, 0x22);
  WriteByte(header, 0);
  WriteByte(header, 2);
  WriteByte(header, 0x11);
  WriteByte(header, 1);
  WriteByte(header, 3);
  WriteByte(header, 0x11);
  WriteByte(header, 1);

  WriteByte(header, 0xFF);
  WriteByte(header, 0xC4);
  WriteWord(header, 418);
  WriteDht(header, 0x00, kBitsYDC, huffTableYDC, 12);
  WriteDht(header, 0x10, kBitsYAC, huffTableYAC, 162);
  WriteDht(header, 0x01, kBitsCoCgDC, huffTableCoCgDC, 12);
  WriteDht(header, 0x11, kBitsCoCgAC, huffTableCoCgAC, 162);

  WriteByte(header, 0xFF);
  WriteByte(header, 0xDA);
  WriteWord(header, 12);
  WriteByte(header, 3);
  WriteByte(header, 1);
  WriteByte(header, 0);
  WriteByte(header, 2);
  WriteByte(header, 0x11);
  WriteByte(header, 3);
  WriteByte(header, 0x11);
  WriteByte(header, 0);
  WriteByte(header, 63);
  WriteByte(header, 0);

  dataBytes = static_cast<int>(header - output);
  data = output;
  const int inputStride = width * 4;
  const int tileColumns = (width + 15) >> 4;
  const int tileRows = (height + 15) >> 4;
  std::uint8_t tile[16 * 16 * 4];
  for (int tileY = 0; tileY < tileRows; ++tileY) {
    for (int tileX = 0; tileX < tileColumns; ++tileX) {
      for (int localY = 0; localY < 16; ++localY) {
        const int sourceY =
            std::max(0, height - 1 - (tileY * 16 + localY));
        for (int localX = 0; localX < 16; ++localX) {
          const int sourceX = std::min(width - 1, tileX * 16 + localX);
          std::memmove(tile + (localY * 16 + localX) * 4,
                       input + sourceY * inputStride + sourceX * 4, 4);
        }
      }
      CompressOneTileJPEG(tile, 64);
    }
  }
  if (bitCnt > 0) {
    data[dataBytes++] = static_cast<std::uint8_t>(bitBuff >> 16);
    bitCnt = 0;
    bitBuff = 0;
  }
  data[dataBytes++] = 0xFF;
  data[dataBytes++] = 0xD9;
  *outputBytes = dataBytes + 2;
  useMarkers = false;
  return true;
}
