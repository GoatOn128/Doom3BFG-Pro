#pragma once

#include <cstddef>
#include <cstdint>

struct idBareJXR {
  int quality;
  void* tempBuffer;
  int tempBufferSize;
  int tempBufferUsed;
  int cROILeftX;
  int cROITopY;
  int cROIWidth;
  int cROIHeight;

  idBareJXR();

  void BeginDecompressImageRGBMacroBlockRow();
  void DecompressImageGrayScaleMacroBlock(std::uint8_t* output,
                                          int blockNumber, int byteStride);
  void DecompressImageRGBMacroBlock(std::uint8_t* output, int blockNumber,
                                    int byteStride);
  void DecompressImageYCoCgMacroBlock(std::uint8_t* output, int blockNumber,
                                      int byteStride);
  void DecompressImageNormalMapMacroBlock(std::uint8_t* output,
                                          int blockNumber, int byteStride);

  bool DecompressImageMono(const std::uint8_t* input, std::uint8_t* output,
                           int width, int height, int inputBytes);
  bool DecompressImageGrayScale(const std::uint8_t* input,
                                std::uint8_t* output, int width, int height,
                                int inputBytes);
  bool DecompressImageRGB(const std::uint8_t* input, std::uint8_t* output,
                          int width, int height, int inputBytes);
  bool DecompressImageYCoCg(const std::uint8_t* input, std::uint8_t* output,
                            int width, int height, int inputBytes);
  bool DecompressImageNormalMap(const std::uint8_t* input,
                                std::uint8_t* output, int width, int height,
                                int inputBytes);

  void BeginDecompressImageGrayScale(const std::uint8_t* input, int width,
                                     int height, int inputBytes);
  void BeginDecompressImageRGB(const std::uint8_t* input, int width,
                               int height, int inputBytes);
  void BeginDecompressImageYCoCg(const std::uint8_t* input, int width,
                                 int height, int inputBytes);
  void BeginDecompressImageNormalMap(const std::uint8_t* input, int width,
                                     int height, int inputBytes);
};

static_assert(offsetof(idBareJXR, tempBuffer) == 0x08,
              "idBareJXR temp buffer offset drift");
static_assert(offsetof(idBareJXR, tempBufferSize) == 0x10,
              "idBareJXR temp buffer size offset drift");
static_assert(offsetof(idBareJXR, cROILeftX) == 0x18,
              "idBareJXR ROI offset drift");
static_assert(sizeof(idBareJXR) == 0x28, "idBareJXR ABI drift");
