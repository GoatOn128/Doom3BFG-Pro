#pragma once

#include <cstddef>
#include <cstdint>
#include <emmintrin.h>

struct idDxtEncoder {
  int width;
  int height;
  std::uint8_t* outData;
  int srcPadding;
  int dstPadding;

  idDxtEncoder()
      : width(0), height(0), outData(nullptr), srcPadding(0), dstPadding(0) {}

  void WriteTinyColorDXT1(const std::uint8_t* input, int imageWidth,
                          int imageHeight);
  void WriteTinyColorDXT5(const std::uint8_t* input, int imageWidth,
                          int imageHeight);
  int GetSquareAlphaError(const std::uint8_t* colorBlock, int alphaOffset,
                          std::uint8_t minAlpha, std::uint8_t maxAlpha,
                          int lastError) const;
  int GetSquareColorsError(const std::uint8_t* colorBlock,
                           std::uint16_t color0, std::uint16_t color1,
                           int lastError) const;
  int GetSquareNormalYError(const std::uint8_t* colorBlock,
                            std::uint16_t color0, std::uint16_t color1,
                            int lastError, int scale) const;
  int FindColorIndices(const std::uint8_t* colorBlock, std::uint16_t color0,
                       std::uint16_t color1, unsigned int* result) const;
  int FindAlphaIndices(const std::uint8_t* colorBlock, int alphaOffset,
                       std::uint8_t alpha0, std::uint8_t alpha1,
                       std::uint8_t* result) const;
  void EmitColorIndices(const std::uint8_t* colorBlock,
                        const std::uint8_t* minColor,
                        const std::uint8_t* maxColor);
  void EmitAlphaIndices(const std::uint8_t* colorBlock, int offset,
                        std::uint8_t minAlpha, std::uint8_t maxAlpha);
  void InsetYCoCgAlpaBBox(std::uint8_t* minColor,
                          std::uint8_t* maxColor) const;
  int GetMinMaxColorsHQ(const std::uint8_t* colorBlock,
                        std::uint8_t* minColor, std::uint8_t* maxColor,
                        bool noBlack) const;
  int GetMinMaxNormalYHQ(const std::uint8_t* colorBlock,
                         std::uint8_t* minColor, std::uint8_t* maxColor,
                         bool noBlack, int scale) const;
  void CompressImageDXT1HQ(const std::uint8_t* input, std::uint8_t* output,
                           int imageWidth, int imageHeight);
  void CompressImageDXT5HQ(const std::uint8_t* input, std::uint8_t* output,
                           int imageWidth, int imageHeight);
  void CompressNormalMapDXT5HQ(const std::uint8_t* input,
                               std::uint8_t* output, int imageWidth,
                               int imageHeight);
  void SelectColorsDiagonal(const std::uint8_t* colorBlock,
                            std::uint8_t* minColor,
                            std::uint8_t* maxColor) const;
  void CompressYCoCgAlphaDXT5Fast(const std::uint8_t* input,
                                  std::uint8_t* output, int imageWidth,
                                  int imageHeight);

  void EmitColorIndices_SSE2(const std::uint8_t* colorBlock,
                             const std::uint8_t* minColor,
                             const std::uint8_t* maxColor);
  void EmitColorAlphaIndices_SSE2(const std::uint8_t* colorBlock,
                                  const std::uint8_t* minColor,
                                  const std::uint8_t* maxColor);
  void EmitCoCgIndices_SSE2(const std::uint8_t* colorBlock,
                            const std::uint8_t* minColor,
                            const std::uint8_t* maxColor);
  void EmitAlphaIndices_SSE2(const __m128i* block,
                             unsigned int minAlpha,
                             unsigned int maxAlpha);
  void EmitAlphaIndices_SSE2(const __m128i* block,
                             unsigned int channelBitOffset,
                             unsigned int minAlpha, int maxAlpha);
  void CompressImageDXT1Fast_SSE2(const std::uint8_t* input,
                                  std::uint8_t* output, int imageWidth,
                                  int imageHeight);
  void CompressImageDXT1AlphaFast_SSE2(const std::uint8_t* input,
                                       std::uint8_t* output, int imageWidth,
                                       int imageHeight);
  void CompressImageDXT5Fast_SSE2(const std::uint8_t* input,
                                  std::uint8_t* output, int imageWidth,
                                  int imageHeight);
  void ScaleYCoCg_SSE2(__m128i* colorBlock, std::uint8_t* minColor,
                       std::uint8_t* maxColor) const;
  void CompressYCoCgDXT5Fast_SSE2(const std::uint8_t* input,
                                  std::uint8_t* output, int imageWidth,
                                  int imageHeight);
  void EmitGreenIndices_SSE2(const __m128i* block,
                             unsigned int channelBitOffset,
                             unsigned int minGreen, int maxGreen);
  void CompressNormalMapDXT5Fast_SSE2(std::uint8_t* input,
                                      std::uint8_t* output, int imageWidth,
                                      int imageHeight);
};

static_assert(offsetof(idDxtEncoder, outData) == 8,
              "idDxtEncoder output pointer offset drift");
static_assert(offsetof(idDxtEncoder, srcPadding) == 16,
              "idDxtEncoder source padding offset drift");
static_assert(sizeof(idDxtEncoder) == 24, "idDxtEncoder ABI drift");
