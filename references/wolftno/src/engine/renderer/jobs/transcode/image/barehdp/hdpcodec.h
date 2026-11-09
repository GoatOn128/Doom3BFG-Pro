#pragma once

#include "engine/renderer/jobs/transcode/image/barehdp/hdpbitioread.h"

#include <cstddef>
#include <cstdint>
#include <emmintrin.h>

enum hdpColorFormat_t : int {
  CF_Y_ONLY = 0,
  CF_YUV_420 = 1,
  CF_YUV_422 = 2,
  CF_YUV_444 = 3,
  CF_CMYK = 4,
  CF_BAYER = 5,
  CF_N_CHANNEL = 6,
  CF_RGB = 7,
  CF_RGBE = 8,
  CF_PALLETIZED = 9,
  CF_MONO = 10,
  CF_GRAYSCALE = 11,
  CF_COCG_Y = 12,
  CF_NORMAL_Y_X = 13,
  CF_VMTR = 14
};

enum hdpStreamLayout_t : int { SL_SPATIAL = 0, SL_FREQUENCY = 1 };

enum hdpSubBands_t : int {
  SB_ALL = 0,
  SB_NO_FLEXBITS = 1,
  SB_NO_HIGHPASS = 2,
  SB_NO_LOWPASS = 3,
  SB_ISOLATED = 4,
  SB_MAX = 5
};

enum hdpOverlap_t : int { OL_NONE = 0, OL_ONE = 1, OL_TWO = 2, OL_MAX = 3 };

enum hdpBand_t : int {
  BAND_HEADER = 0,
  BAND_DC = 1,
  BAND_LP = 2,
  BAND_HP = 3,
  BAND_FL = 4
};

enum hdpBitDepth_t : int {
  BD_1 = 0,
  BD_8 = 1,
  BD_16 = 2,
  BD_16S = 3,
  BD_16F = 4,
  BD_32 = 5,
  BD_32S = 6,
  BD_32F = 7,
  BD_5 = 8,
  BD_10 = 9,
  BD_565 = 10,
  BD_1alt = 15,
  BD_MAX = 16
};

enum hdpOrientation_t : int {
  O_NONE = 0,
  O_FLIPV = 1,
  O_FLIPH = 2,
  O_FLIPVH = 3,
  O_RCW = 4,
  O_RCW_FLIPV = 5,
  O_RCW_FLIPH = 6,
  O_RCW_FLIPVH = 7,
  O_MAX = 8
};

struct hdpImageInfo_t {
  std::uint64_t imageWidth;
  std::uint64_t imageHeight;
  hdpColorFormat_t colorFormat;
  hdpBitDepth_t bitDepth;
  std::uint64_t bitsPerUnit;
  std::uint64_t leadingPadding;
  std::uint8_t patternRGB;
  std::uint8_t patternBayer;
  hdpOrientation_t orientation;
};

struct hdpImageBufferInfo_t {
  void* buffer;
  std::uint64_t lineCount;
  std::uint64_t macroblockIndex;
  std::uint64_t byteStride;
};

struct hdpCodecParms_t {
  hdpColorFormat_t colorFormat;
  std::uint64_t numChannels;
  hdpStreamLayout_t streamLayout;
  hdpSubBands_t subBands;
  hdpOverlap_t overlap;
  std::uint8_t mantissaLengthOrShift;
  std::int8_t exponentBias;
  bool hasAlphaChannel;
  std::uint32_t quantizationMode;
  std::uint8_t quantizationDefault;
  std::uint8_t quantizationUV;
  std::uint8_t quantizationDC[16];
  std::uint8_t quantizetionLP[16];
  std::uint8_t quantizetionHP[16];
  std::uint8_t uniformTrimFlexBits;
  bool hasPerTileTrimFlexbits;
  bool useScaledArith;
  bool hasTileIndex;
  std::uint32_t numTilesHighMinusOne;
  std::uint32_t numTilesWideMinusOne;
  std::uint32_t* tileMacroblockOffsetX;
  std::uint32_t* tileMacroblockOffsetY;
  std::uint64_t extraPixelsTop;
  std::uint64_t extraPixelsLeft;
  std::uint64_t extraPixelsBottom;
  std::uint64_t extraPixelsRight;
  bool transcode;
  bool changeSubsamplingUV;
  hdpStream_t* baseStream;
  void* tempBuffer;
  std::uint64_t tempBufferSize;
  std::uint64_t tempBufferUsed;
  bool skipFlexbits;
  bool decodeCoeffientsOnly;
  std::uint64_t fullImageROILeftX;
  std::uint64_t fullImageROIRightX;
  std::uint64_t fullImageROITopY;
  std::uint64_t fullImageROIBottomY;
  std::uint64_t thumbnailWidth;
  std::uint64_t thumbnailHeight;
  std::uint8_t postProcStrength;
  std::uint64_t fullImageROILeftTile;
  std::uint64_t fullImageROIRightTile;
  std::uint64_t fullImageROITopTile;
  std::uint64_t fullImageROIBottomTile;
  std::uint64_t thumbnailScale;
  bool decodeLP;
  bool decodeHP;
  bool decodeFlexBits;
  bool decodeFullFrame;
  bool decodeFullWidth;
};

struct hdpCodec_t;

struct hdpMacroblockInfo_t {
  std::int16_t compactDCLP[16][16];
  int actualCBP[16];
  int residualCBP[16];
  int predOrientationHP[16];
  std::uint8_t quantizerIndexLP;
  std::uint8_t quantizerIndexHP;
  std::uint8_t pad[62];
};

struct hdpPredictionInfo_t {
  int quantizerIndexLP;
  int macroblockHPCBP;
  std::int16_t macroblockDC;
  std::int16_t macroblockLP[6];
};

struct hdpPostProcessInfo_t {
  int iMBDC;
  std::uint8_t ucMBTexture;
};

struct hdpAdaptiveCBPModelLP_t {
  int zeroCount;
  int maxCount;
};

struct hdpAdaptiveCBPModelHP_t {
  int countOnes[16];
  int countZeros[16];
  int state[16];
};

struct hdpAdaptiveCNModel_t {
  int fixedLengthCodeState[16];
  int fixedLengthCodeBits[16];
  hdpBand_t band;
};

struct hdpAdaptiveScan_t {
  std::uint32_t scanTotal;
  std::uint32_t scanIndex;
};

struct hdpAdaptiveHuffman_t {
  const int* encTable;
  const std::int16_t* decTable;
  const int* deltaTable;
  const int* deltaTable1;
  int numSymbols;
  int tableIndex;
  int isInitialized;
  int discriminant;
  int discriminant1;
  int upperBound;
  int lowerBound;
  int pad[1];
};

struct hdpContext_t {
  hdpBitIO_t* bitIODC;
  hdpBitIO_t* bitIOLP;
  hdpBitIO_t* bitIOHP;
  hdpBitIO_t* bitIOFL;
  hdpAdaptiveCBPModelLP_t adaptCBPModelLP;
  hdpAdaptiveCBPModelHP_t adaptCBPModelHP;
  hdpAdaptiveCNModel_t adaptCNModelDC;
  hdpAdaptiveCNModel_t adaptCNModelLP;
  hdpAdaptiveCNModel_t adaptCNModelHP;
  hdpAdaptiveScan_t adaptScanLP[1][16];
  hdpAdaptiveScan_t adaptScanHorHP[1][16];
  hdpAdaptiveScan_t adaptScanVerHP[1][16];
  hdpAdaptiveHuffman_t adaptHuffDC[3];
  hdpAdaptiveHuffman_t adaptHuffLP[8];
  hdpAdaptiveHuffman_t adaptHuffHP[8];
  hdpAdaptiveHuffman_t adaptHuffCBP[3];
};

struct hdpQuantizer_t {
  int divisor;
  int rounder;
  int mantissa;
  std::uint8_t exponent;
  std::uint8_t quantization;
  std::uint16_t pad;
};

struct hdpTileQuantization_t {
  hdpQuantizer_t* quantizerDC[16];
  hdpQuantizer_t* quantizerLP[16];
  hdpQuantizer_t* quantizerHP[16];
  std::uint8_t numQuantizersLP;
  std::uint8_t numQuantizersHP;
  std::uint8_t numQuantizerIndexBitsLP;
  std::uint8_t numQuantizerIndexBitsHP;
  int trimFlexBits;
  int pad[30];
};

struct hdpCodec_t {
  hdpMacroblockInfo_t MBInfo;
  hdpImageInfo_t imageInfo;
  hdpImageBufferInfo_t imageBufferInfo;
  hdpCodecParms_t codecParms;
  std::uint64_t* tileIndex;
  std::uint64_t baseTileOffset;
  hdpBitIO_t* bitIOHeader;
  hdpStream_t** tileStreams;
  hdpBitIO_t* tileBitIOs;
  std::uint64_t numTileBitIOs;
  std::uint64_t numSubBands;
  hdpTileQuantization_t* tileQuantization;
  hdpContext_t* tileContexts;
  std::uint64_t currentTileRow;
  std::uint64_t currentTileColumn;
  bool atTileLeftMB;
  bool atTileTopMB;
  bool resetAdaptiveScanTotals;
  bool adaptContextHuffman;
  bool processedFirstMBRow;
  std::uint64_t widthInMacroblocks;
  std::uint64_t heightInMacroblocks;
  std::uint64_t currentMacroblockRow;
  std::uint64_t currentMacroblockColumn;
  std::int16_t* macroBlockRowBuffer0[16];
  std::int16_t* macroBlockRowBuffer1[16];
  std::int16_t* macroBlockColumnPtr0[16];
  std::int16_t* macroBlockColumnPtr1[16];
  std::int16_t* macroBlockPrevColumnPtr[16];
  hdpPredictionInfo_t* predInfoCurRow[16];
  hdpPredictionInfo_t* predInfoPrevRow[16];
  hdpPredictionInfo_t* predInfoMemory;
  hdpPostProcessInfo_t* postProcInfo[16][2];
  std::int16_t* resampleU;
  std::int16_t* resampleV;
  std::uint64_t* rotFlipOffsetX;
  std::uint64_t* rotFlipOffsetY;
  hdpCodec_t* nextCodec;
  bool isSecondaryCodec;
};

void* TempAlloc(hdpCodecParms_t* codecParms, std::uint64_t size,
                std::uint64_t alignment);
hdpError_t AllocateAdditionalBitIOs(hdpCodec_t* codec);
void UpdateTilePos(hdpCodec_t* codec, std::uint64_t macroblockX,
                   std::uint64_t macroblockY);
void InitMacroblockColumnPtrs(hdpCodec_t* firstCodec);
void AdvanceMacroblockColumnPtrs(hdpCodec_t* firstCodec);
void SwapMacroblockRowPtrs(hdpCodec_t* firstCodec);
void SwapMacroblockPredPtrs(hdpCodec_t* firstCodec);
void InitAdaptiveHuffman(hdpCodecParms_t* codecParms,
                         hdpAdaptiveHuffman_t* adaptiveHuffman,
                         int numSymbols);
void ShutdownAdaptiveHuffman(hdpCodecParms_t* codecParms,
                             hdpAdaptiveHuffman_t* adaptiveHuffman);
void AdaptiveHuffmanDiscriminant(hdpAdaptiveHuffman_t* adaptiveHuffman);
void FreeCodingContexts(hdpCodec_t* codec);
void InitContextBitIOs(hdpCodec_t* codec);
void ResetAdaptiveScanTotalsLP(hdpContext_t* context);
void ResetAdaptiveScanTotalsHP(hdpContext_t* context);
hdpError_t AdaptContextHuffmanDC(hdpContext_t* context);
hdpError_t AdaptContextHuffmanLP(hdpContext_t* context);
hdpError_t AdaptContextHuffmanHP(hdpContext_t* context);
void UpdateAdaptiveCoefficientNormalization(
    hdpColorFormat_t colorFormat, int numChannels, int* laplacianMean,
    hdpAdaptiveCNModel_t* model);
void ResetCodingContext(hdpContext_t* context);
hdpError_t AllocateCodingContexts(hdpCodec_t* codec);
hdpError_t AllocateQuantizers(hdpCodecParms_t* codecParms,
                              hdpQuantizer_t** quantizers,
                              std::uint64_t numChannels,
                              std::uint64_t numQuantizers);
void FormatQuantizers(hdpQuantizer_t** quantizers, std::uint8_t channelMode,
                      std::uint64_t numChannels, std::uint64_t position,
                      std::uint8_t useShiftedUV, bool useScaledArithmetic,
                      bool useRoundDC);
hdpError_t AllocateTileQuantization(hdpCodec_t* codec);
void FreeTileQuantization(hdpCodec_t* codec);
void SetUniformQuantizerDC(hdpCodec_t* codec);
void SetUniformQuantizerLP(hdpCodec_t* codec);
void SetUniformQuantizerHP(hdpCodec_t* codec);
void SetUniformTrimFlexBits(hdpCodec_t* codec, bool decodeFlexBits);
void UseDCQuantizerForLP(hdpCodec_t* codec, std::uint64_t tile);
void UseLPQuantizerForHP(hdpCodec_t* codec, std::uint64_t numQuantizers,
                         std::uint64_t tile);
std::uint8_t GetQuantizerBits(std::uint8_t numQuantizers);
void DequantizeHP_444(__m128i* coefficients, unsigned int* quantizer);
hdpError_t DequantizeMacroblock(hdpCodec_t* codec);
hdpError_t AllocatePredictionInfo(hdpCodec_t* codec);
void FreePredictionInfo(hdpCodec_t* codec);
int GetPredictionModeHP(hdpCodec_t* codec, int channel);
int GetPredictionModeDCLP(hdpCodec_t* codec, int channel,
                          std::uint64_t macroblockX);
void UpdatePredictionInfo(hdpCodec_t* codec, std::uint64_t macroblockX);
void PredictDCLPDec(hdpCodec_t* codec);
void PredictHPDec(hdpCodec_t* codec);
void PredictHPCBPDec(hdpCodec_t* codec, hdpContext_t* context,
                     int macroblockX, int macroblockY);
std::uint8_t ReadQuantizerIndex(std::uint8_t* quantizerIndex,
                                hdpBitIO_t* bitIO,
                                std::uint64_t numChannels);
std::uint8_t ReadQuantizer(hdpQuantizer_t** quantizers, hdpBitIO_t* bitIO,
                           std::uint64_t numChannels,
                           std::uint64_t position, bool transcode);
hdpError_t ReadPacketHeader(hdpBitIO_t* bitIO, std::uint8_t packetType,
                            std::uint8_t packetID);
hdpError_t ReadTileHeaderQuantizerDC(hdpCodec_t* codec, hdpBitIO_t* bitIO);
hdpError_t ReadTileHeaderQuantizerLP(hdpCodec_t* codec, hdpBitIO_t* bitIO);
hdpError_t ReadTileHeaderQuantizerHP(hdpCodec_t* codec, hdpBitIO_t* bitIO);
hdpError_t ReadTileHeaderTrimFlexBits(hdpCodec_t* codec, hdpBitIO_t* bitIO);
hdpError_t ReadTileHeader(hdpCodec_t* codec, hdpContext_t* context);
std::uint64_t GetVLWordEsc(hdpBitIO_t* bitIO, int* escape);
hdpError_t ReadIndexTable(hdpCodec_t* codec);
hdpError_t InitImagePlaneDec(hdpCodec_t* codec);
hdpError_t ReadImagePlaneHeader(hdpCodecParms_t* codecParms,
                                hdpBitIO_t* bitIO);
hdpError_t ReadImageHeader(hdpImageInfo_t* imageInfo,
                           hdpCodecParms_t* codecParms, hdpBitIO_t* bitIO);
hdpError_t ProcessMacroblockDec(hdpCodec_t* firstCodec);
void DeriveDecoderParms(hdpCodecParms_t* codecParms,
                        std::uint64_t fullImageWidth,
                        std::uint64_t fullImageHeight);

std::uint8_t DecodeQuantizerIndex(hdpBitIO_t* bitIO,
                                  std::uint8_t indexBitCount);
void DecodeHPCBP(hdpCodec_t* codec, hdpContext_t* context);
int DecodeLPCBP(hdpCodec_t* codec, hdpContext_t* context);
void DecodeBlockFlexBits(hdpBitIO_t* bitIO, int flexBitCount,
                         int trimFlexBits, std::int16_t* coefficients);
void DecodeBlockModelBits(hdpBitIO_t* bitIO, int modelBitCount,
                          std::int16_t* coefficients);
hdpError_t DecodeMacroblockDC(hdpCodec_t* codec, hdpContext_t* context,
                              int macroblockX, int macroblockY);
hdpError_t DecodeQuantizerIndexLPHP(hdpCodec_t* codec,
                                    hdpContext_t* context);
int DecodeBlockInverseAdaptiveScan(
    std::int16_t* coefficients, hdpAdaptiveHuffman_t* adaptiveHuffman,
    hdpAdaptiveScan_t* adaptiveScan, hdpBitIO_t* bitIO,
    std::int8_t modelBitCount, bool chroma);
hdpError_t DecodeMacroblockHighPass(hdpCodec_t* codec,
                                    hdpContext_t* context,
                                    int macroblockX, int macroblockY);
hdpError_t DecodeMacroblockLowPass(hdpCodec_t* codec,
                                   hdpContext_t* context,
                                   int macroblockX, int macroblockY);
hdpError_t DecodeMacroblock(hdpCodec_t* codec, hdpContext_t* context,
                            int macroblockX, int macroblockY);
void InvTransform4x4Stage1_4(__m128i* coefficients);
void InvTransform4x4Stage1_8(__m128i* first, __m128i* second);
void InvTransform4x4Stage2(__m128i* coefficients);
void PostFilter4x4Stage1(__m128i* p0, __m128i* p1,
                         __m128i* p2, __m128i* p3);
void PostFilter2x4Stage1(std::int16_t* first, __m128i* second);
void PostFilter4x2Stage1(__m128i* first, __m128i* second);
void PostFilter2x2Stage1(__m128i* coefficients);
void InvTransformMacroblock_SSE2_16(hdpCodec_t* codec,
                                    int macroblockX, int macroblockY);

hdpError_t HDPDecInit(hdpImageInfo_t* imageInfo,
                      hdpCodecParms_t* codecParms, hdpCodec_t** codec);
hdpError_t HDPDecDecodeMBRow(hdpCodec_t* codec);
hdpError_t HDPDecShutdown(hdpCodec_t* codec);
hdpError_t HDPDecOutputMBRowFast_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo);
hdpError_t HDPDecOutputMB_Mono_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo);
hdpError_t HDPDecOutputMB_GrayScale_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo);
hdpError_t HDPDecOutputMB_RGB_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo);
hdpError_t HDPDecOutputMB_CoCg_Y_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo);
hdpError_t HDPDecOutputMB_Normal_Y_X_SSE2_16(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo);
hdpError_t HDPDecOutputMB_Vmtr_Generic(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo);
hdpError_t HDPDecOutputMB_NChannel_Generic(
    hdpCodec_t* codec, const hdpImageBufferInfo_t* bufferInfo);

static_assert(sizeof(hdpImageInfo_t) == 0x30,
              "hdpImageInfo_t ABI drift");
static_assert(sizeof(hdpImageBufferInfo_t) == 0x20,
              "hdpImageBufferInfo_t ABI drift");
static_assert(offsetof(hdpCodecParms_t, baseStream) == 0xA0,
              "hdpCodecParms_t stream offset drift");
static_assert(offsetof(hdpCodecParms_t, tempBufferUsed) == 0xB8,
              "hdpCodecParms_t temp usage offset drift");
static_assert(sizeof(hdpCodecParms_t) == 0x130,
              "hdpCodecParms_t ABI drift");
static_assert(sizeof(hdpMacroblockInfo_t) == 0x300,
              "hdpMacroblockInfo_t ABI drift");
static_assert(sizeof(hdpAdaptiveHuffman_t) == 0x40,
              "hdpAdaptiveHuffman_t ABI drift");
static_assert(sizeof(hdpContext_t) == 0x978, "hdpContext_t ABI drift");
static_assert(sizeof(hdpQuantizer_t) == 0x10, "hdpQuantizer_t ABI drift");
static_assert(sizeof(hdpTileQuantization_t) == 0x200,
              "hdpTileQuantization_t ABI drift");
static_assert(offsetof(hdpCodec_t, codecParms) == 0x350,
              "hdpCodec_t parameter offset drift");
static_assert(offsetof(hdpCodec_t, macroBlockRowBuffer0) == 0x500,
              "hdpCodec_t row-buffer offset drift");
static_assert(offsetof(hdpCodec_t, nextCodec) == 0x9A8,
              "hdpCodec_t chain offset drift");
static_assert(sizeof(hdpCodec_t) == 0x9B8, "hdpCodec_t ABI drift");
