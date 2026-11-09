#pragma once

#include <cstddef>
#include <cstdint>

enum huffmanType_t {
  HUFFMAN_TYPE_Y_DC,
  HUFFMAN_TYPE_Y_AC,
  HUFFMAN_TYPE_COCG_DC,
  HUFFMAN_TYPE_COCG_AC
};

enum sampleMode_t {
  SAMPLE_444,
  SAMPLE_420
};

struct idBareDCTHuffmanTable {
  unsigned int code[256];
  char size[256];
  std::uint8_t look_nbits[256];
  std::uint8_t look_sym[256];
  std::uint8_t symbols[256];
  int test_nbits[16];
  int minCode[17];
  int maxCode[18];
  int symOffset[17];

  idBareDCTHuffmanTable() = default;
  idBareDCTHuffmanTable(huffmanType_t huffmanType, bool encode);

 private:
  void Init(const std::uint8_t* bits, const std::uint8_t* values, bool encode);
};

struct idBareDctBase {
  int luminanceQuality;
  int chrominanceQuality;
  int alphaQuality;
  sampleMode_t sampleMode;
  std::uint16_t quantTableY[64];
  std::uint16_t quantTableCoCg[64];
  std::uint16_t quantTableA[64];

  idBareDctBase();
  void InitQuantTable();
  static int QuantizationScaleFromQuality(int quality);
  void SetQuality_Generic(int luminance, int chrominance, int alpha);
  static void ScaleQuantTable_SSE2(std::uint16_t* result,
                                  const std::uint16_t* standard, int scale);
  void SetQuality_SSE2(int luminance, int chrominance, int alpha);
};

struct idBareDctDecoder : idBareDctBase {
  int imageWidth;
  int imageHeight;
  int dcY;
  int dcCo;
  int dcCg;
  int dcA;
  int bitCnt;
  int bitBuff;
  int dataBytes;
  const std::uint8_t* data;
  const idBareDCTHuffmanTable* pHuffTableYDC;
  const idBareDCTHuffmanTable* pHuffTableYAC;
  const idBareDCTHuffmanTable* pHuffTableCoCgDC;
  const idBareDCTHuffmanTable* pHuffTableCoCgAC;
  const idBareDCTHuffmanTable* pHuffTableADC;
  const idBareDCTHuffmanTable* pHuffTableAAC;

  idBareDctDecoder();
  void SetHuffmanTables(const idBareDCTHuffmanTable* tableYDC,
                        const idBareDCTHuffmanTable* tableYAC,
                        const idBareDCTHuffmanTable* tableCoCgDC,
                        const idBareDCTHuffmanTable* tableCoCgAC,
                        const idBareDCTHuffmanTable* tableADC,
                        const idBareDCTHuffmanTable* tableAAC);
  void YCbCrToRGB(const std::int16_t* yCbCr, std::uint8_t* rgb, int stride);
  int GetBits(int bits);
  int PeekBits(int bits);
  int DecodeLong(const idBareDCTHuffmanTable* table);
  void HuffmanDecode(std::int16_t* coefficients,
                     const idBareDCTHuffmanTable* dcTable,
                     const idBareDCTHuffmanTable* acTable, int* lastDC);
  void DecompressOneTileJPEG(std::uint8_t* rgb, int stride);
  bool DecompressImageRGB_JPEG(const std::uint8_t* input,
                               std::uint8_t* output, int width, int height,
                               int inputBytes);
  void StoreMono_SSE2(const std::int16_t* samples, std::uint8_t* mono,
                      int stride);
  void StoreGrayScale_SSE2(const std::int16_t* samples, std::uint8_t* rgba,
                           int stride);
  void YCoCgToRGB_SSE2(const std::int16_t* yCoCg, std::uint8_t* rgba,
                       int stride);
  void StoreYCoCg420_SSE2(const std::int16_t* yCoCg, std::uint8_t* coCgY,
                          int stride);
  void StoreYCoCg444_SSE2(const std::int16_t* yCoCg, std::uint8_t* coCgY,
                          int stride);
  void StoreNormalMap_SSE2(const std::int16_t* samples,
                           std::uint8_t* normalMap, int stride);
  int FillBitBuffer_SSE2();
  int DecodeLong_SSE2(const idBareDCTHuffmanTable* table);
  int GetValue_SSE2(int category);
  int GetCategory_SSE2(const idBareDCTHuffmanTable* table);
  void HuffmanDecode_SSE2(std::int16_t* coefficients,
                          const idBareDCTHuffmanTable* dcTable,
                          const idBareDCTHuffmanTable* acTable, int* lastDC);
  void DecompressOneTileGrayScale_SSE2(std::uint8_t* rgba, int stride);
  void DecompressOneTileRGB_SSE2(std::uint8_t* rgba, int stride);
  void DecompressOneTileYCoCg_SSE2(std::uint8_t* coCgY, int stride);
  void DecompressOneTileNormalMap_SSE2(std::uint8_t* normalMap, int stride);
  bool DecompressImageMono_SSE2(const std::uint8_t* input,
                                std::uint8_t* output, int width, int height,
                                int inputBytes);
  bool DecompressImageGrayScale_SSE2(const std::uint8_t* input,
                                     std::uint8_t* output, int width,
                                     int height, int inputBytes);
  bool DecompressImageRGB_SSE2(const std::uint8_t* input,
                               std::uint8_t* output, int width, int height,
                               int inputBytes);
  bool DecompressImageYCoCg_SSE2(const std::uint8_t* input,
                                 std::uint8_t* output, int width, int height,
                                 int inputBytes);
  bool DecompressImageNormalMap_SSE2(const std::uint8_t* input,
                                     std::uint8_t* output, int width,
                                     int height, int inputBytes);
};

struct idBareDctEncoder : idBareDctBase {
  idBareDCTHuffmanTable huffTableYDC;
  idBareDCTHuffmanTable huffTableYAC;
  idBareDCTHuffmanTable huffTableCoCgDC;
  idBareDCTHuffmanTable huffTableCoCgAC;
  idBareDCTHuffmanTable huffTableADC;
  idBareDCTHuffmanTable huffTableAAC;
  int imageWidth;
  int imageHeight;
  int dcY;
  int dcCo;
  int dcCg;
  int dcA;
  int bitCnt;
  int bitBuff;
  int dataBytes;
  std::uint8_t* data;
  bool useMarkers;

  idBareDctEncoder();
  void RGBToYCbCrTile(const std::uint8_t* rgba, std::int16_t* yCbCr,
                      int stride);
  bool EmitBits(std::uint8_t* output, unsigned int bits, int bitCount);
  bool HuffmanEncode(const std::int16_t* coefficients,
                     const idBareDCTHuffmanTable* dcTable,
                     const idBareDCTHuffmanTable* acTable, int* lastDC);
  void CompressOneTileJPEG(const std::uint8_t* rgba, int stride);
  bool CompressImageRGB_JPEG(const std::uint8_t* input,
                             std::uint8_t* output, int width, int height,
                             int* outputBytes, bool useMarkersValue);
};

void IDCT_AP922_float(const std::int16_t* coefficients,
                      const std::uint16_t* quantizers,
                      std::int16_t* destination);
void FDCT_AP922_float(const std::int16_t* source,
                      const std::uint16_t* quantizers,
                      std::int16_t* coefficients);
void IDCT_AP922_int_w_SSE2(const std::int16_t* coefficients,
                           const std::uint16_t* quantizers,
                           std::int16_t* destination);

extern const std::uint16_t std_luminance_quant_tbl[64];
extern const std::uint16_t std_chrominance_quant_tbl[64];
extern const std::uint16_t std_alpha_quant_tbl[64];

static_assert(sizeof(idBareDCTHuffmanTable) == 0x910,
              "idBareDCTHuffmanTable ABI drift");
static_assert(offsetof(idBareDCTHuffmanTable, test_nbits) == 0x800,
              "idBareDCTHuffmanTable test table offset drift");
static_assert(offsetof(idBareDctBase, quantTableY) == 0x10,
              "idBareDctBase Y quantizer offset drift");
static_assert(offsetof(idBareDctBase, quantTableCoCg) == 0x90,
              "idBareDctBase CoCg quantizer offset drift");
static_assert(offsetof(idBareDctBase, quantTableA) == 0x110,
              "idBareDctBase alpha quantizer offset drift");
static_assert(sizeof(idBareDctBase) == 0x190, "idBareDctBase ABI drift");
static_assert(offsetof(idBareDctDecoder, imageWidth) == 0x190,
              "idBareDctDecoder image size offset drift");
static_assert(offsetof(idBareDctDecoder, data) == 0x1B8,
              "idBareDctDecoder data offset drift");
static_assert(offsetof(idBareDctDecoder, pHuffTableYDC) == 0x1C0,
              "idBareDctDecoder Huffman table offset drift");
static_assert(sizeof(idBareDctDecoder) == 0x1F0,
              "idBareDctDecoder ABI drift");
static_assert(offsetof(idBareDctEncoder, huffTableYDC) == 0x190,
              "idBareDctEncoder Huffman table offset drift");
static_assert(offsetof(idBareDctEncoder, imageWidth) == 0x37F0,
              "idBareDctEncoder image size offset drift");
static_assert(offsetof(idBareDctEncoder, data) == 0x3818,
              "idBareDctEncoder output data offset drift");
static_assert(offsetof(idBareDctEncoder, useMarkers) == 0x3820,
              "idBareDctEncoder marker flag offset drift");
static_assert(sizeof(idBareDctEncoder) == 0x3828,
              "idBareDctEncoder ABI drift");
