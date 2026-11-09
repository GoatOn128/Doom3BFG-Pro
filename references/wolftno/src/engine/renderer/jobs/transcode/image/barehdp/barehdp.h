#pragma once

#include "engine/renderer/jobs/transcode/image/barehdp/hdpcodec.h"

#include <cstddef>
#include <cstdint>

struct idBareHDP {
  int quality;
  void* tempBuffer;
  int tempBufferSize;
  int tempBufferUsed;
  int imageROILeftX;
  int imageROITopY;
  int imageROIWidth;
  int imageROIHeight;
  int imageByteStride;
  hdpCodec_t* codec;
  alignas(8) std::uint8_t streamTemp[128];

  idBareHDP();

  void BeginDecompressImageGrayScaleMacroBlockRow();
  void DecompressImageGrayScaleMacroBlock(std::uint8_t* output,
                                          int blockNumber, int byteStride);
  void DecompressImageRGBMacroBlock(std::uint8_t* output, int blockNumber,
                                    int byteStride);
  void DecompressImageYCoCgMacroBlock(std::uint8_t* output, int blockNumber,
                                      int byteStride);
  void DecompressImageNormalMapMacroBlock(std::uint8_t* output,
                                          int blockNumber, int byteStride);
  void TerminateMacroBlockRowDecoding();

  bool DecompressImageMono(const std::uint8_t* input, std::uint8_t* output,
                           int width, int height, int inputBytes,
                           char mipLevel);
  bool DecompressImageGrayScale(const std::uint8_t* input,
                                std::uint8_t* output, int width, int height,
                                int inputBytes, char mipLevel);
  bool DecompressImageRGB(const std::uint8_t* input, std::uint8_t* output,
                          int width, int height, int inputBytes,
                          char mipLevel);
  bool DecompressImageYCoCg(const std::uint8_t* input, std::uint8_t* output,
                            int width, int height, int inputBytes,
                            char mipLevel);
  bool DecompressImageNormalMap(const std::uint8_t* input,
                                std::uint8_t* output, int width, int height,
                                int inputBytes, char mipLevel);

  void BeginDecompressImageGrayScale(const std::uint8_t* input, int width,
                                     int height, int inputBytes);
  void BeginDecompressImageRGB(const std::uint8_t* input, int width,
                               int height, int inputBytes);
  void BeginDecompressImageYCoCg(const std::uint8_t* input, int width,
                                 int height, int inputBytes);
  void BeginDecompressImageNormalMap(const std::uint8_t* input, int width,
                                     int height, int inputBytes);
};

bool DecodeHDPhoto(hdpImageInfo_t* imageInfo, hdpCodecParms_t* codecParms,
                   std::uint8_t* output, int width, int height,
                   char mipLevel, int* tempUsed);

static_assert(offsetof(idBareHDP, tempBuffer) == 0x08,
              "idBareHDP temp buffer offset drift");
static_assert(offsetof(idBareHDP, codec) == 0x30,
              "idBareHDP codec offset drift");
static_assert(offsetof(idBareHDP, streamTemp) == 0x38,
              "idBareHDP stream offset drift");
static_assert(sizeof(idBareHDP) == 0xB8, "idBareHDP ABI drift");
