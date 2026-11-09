#include "engine/renderer/jobs/transcode/image/barehdp/barehdp.h"

#include <algorithm>
#include <cstdint>
#include <cstring>

namespace {

enum class BareHdpTarget {
  Mono,
  GrayScale,
  RGB,
  YCoCg,
  NormalMap,
};

hdpImageInfo_t MakeImageInfo(BareHdpTarget target, int width, int height) {
  hdpImageInfo_t imageInfo{};
  imageInfo.imageWidth = static_cast<std::uint64_t>(width);
  imageInfo.imageHeight = static_cast<std::uint64_t>(height);
  imageInfo.bitDepth = BD_8;
  imageInfo.orientation = O_NONE;
  switch (target) {
    case BareHdpTarget::Mono:
      imageInfo.colorFormat = CF_MONO;
      imageInfo.bitsPerUnit = 8;
      break;
    case BareHdpTarget::GrayScale:
      imageInfo.colorFormat = CF_GRAYSCALE;
      imageInfo.bitsPerUnit = 32;
      break;
    case BareHdpTarget::RGB:
      imageInfo.colorFormat = CF_RGB;
      imageInfo.bitsPerUnit = 32;
      break;
    case BareHdpTarget::YCoCg:
      imageInfo.colorFormat = CF_COCG_Y;
      imageInfo.bitsPerUnit = 32;
      break;
    case BareHdpTarget::NormalMap:
      imageInfo.colorFormat = CF_NORMAL_Y_X;
      imageInfo.bitsPerUnit = 32;
      break;
  }
  return imageInfo;
}

std::uint8_t ChromaQuantization(int quality) {
  if (quality < 16)
    return static_cast<std::uint8_t>(quality * 2);
  const int adjusted = quality + (quality <= 48 ? 18 : 20);
  return static_cast<std::uint8_t>(std::min(adjusted, 255));
}

hdpCodecParms_t MakeCodecParms(const idBareHDP& decoder,
                               BareHdpTarget target,
                               hdpStream_t* stream, int width, int height) {
  hdpCodecParms_t codecParms{};
  codecParms.streamLayout = SL_SPATIAL;
  codecParms.subBands = decoder.quality != 0 ? SB_NO_FLEXBITS : SB_ALL;
  codecParms.overlap = OL_ONE;
  codecParms.quantizationDefault =
      static_cast<std::uint8_t>(decoder.quality);
  codecParms.baseStream = stream;
  codecParms.tempBuffer = decoder.tempBuffer;
  codecParms.tempBufferSize =
      static_cast<std::uint64_t>(decoder.tempBufferSize);
  codecParms.thumbnailWidth = static_cast<std::uint64_t>(width);
  codecParms.thumbnailHeight = static_cast<std::uint64_t>(height);

  if (target == BareHdpTarget::RGB || target == BareHdpTarget::YCoCg) {
    codecParms.colorFormat = CF_YUV_444;
    codecParms.numChannels = 3;
    codecParms.quantizationUV = ChromaQuantization(decoder.quality);
  } else {
    codecParms.colorFormat = CF_N_CHANNEL;
    codecParms.numChannels = target == BareHdpTarget::NormalMap ? 2 : 1;
  }

  codecParms.fullImageROILeftX =
      static_cast<std::uint64_t>(decoder.imageROILeftX);
  codecParms.fullImageROITopY =
      static_cast<std::uint64_t>(decoder.imageROITopY);
  const int roiRight = decoder.imageROIWidth > 0
                           ? decoder.imageROILeftX + decoder.imageROIWidth
                           : width;
  const int roiBottom = decoder.imageROIHeight > 0
                            ? decoder.imageROITopY + decoder.imageROIHeight
                            : height;
  codecParms.fullImageROIRightX =
      static_cast<std::uint64_t>(roiRight - 1);
  codecParms.fullImageROIBottomY =
      static_cast<std::uint64_t>(roiBottom - 1);
  return codecParms;
}

bool DecompressImage(idBareHDP& decoder, BareHdpTarget target,
                     const std::uint8_t* input, std::uint8_t* output,
                     int width, int height, int inputBytes, char mipLevel) {
  hdpStream_t stream{};
  hdpImageInfo_t imageInfo = MakeImageInfo(target, width, height);
  hdpCodecParms_t codecParms =
      MakeCodecParms(decoder, target, &stream, width, height);
  CreateWS_Memory(&stream, const_cast<std::uint8_t*>(input),
                  static_cast<std::uint64_t>(inputBytes));
  return DecodeHDPhoto(&imageInfo, &codecParms, output, width, height,
                       mipLevel, &decoder.tempBufferUsed);
}

void BeginDecompressImage(idBareHDP& decoder, BareHdpTarget target,
                          const std::uint8_t* input, int width, int height,
                          int inputBytes) {
  auto* stream = reinterpret_cast<hdpStream_t*>(decoder.streamTemp);
  hdpImageInfo_t imageInfo = MakeImageInfo(target, width, height);
  hdpCodecParms_t codecParms =
      MakeCodecParms(decoder, target, stream, width, height);
  CreateWS_Memory(stream, const_cast<std::uint8_t*>(input),
                  static_cast<std::uint64_t>(inputBytes));
  decoder.imageByteStride =
      16 * static_cast<int>(imageInfo.bitsPerUnit >> 3);
  hdpCodec_t* codec = nullptr;
  if (HDPDecInit(&imageInfo, &codecParms, &codec) != HDP_ERROR_NONE) {
    decoder.codec = nullptr;
    return;
  }
  decoder.tempBufferUsed = static_cast<int>(codecParms.tempBufferUsed);
  decoder.codec = codec;
}

hdpImageBufferInfo_t MakeMacroBlockInfo(std::uint8_t* output,
                                        int blockNumber, int byteStride) {
  hdpImageBufferInfo_t bufferInfo{};
  bufferInfo.buffer = output;
  bufferInfo.lineCount = 16;
  bufferInfo.macroblockIndex = static_cast<std::uint64_t>(blockNumber);
  bufferInfo.byteStride = static_cast<std::uint64_t>(byteStride);
  return bufferInfo;
}

}  // namespace

void idBareHDP::BeginDecompressImageGrayScaleMacroBlockRow() {
  HDPDecDecodeMBRow(codec);
}

void idBareHDP::DecompressImageGrayScaleMacroBlock(std::uint8_t* output,
                                                   int blockNumber,
                                                   int byteStride) {
  const hdpImageBufferInfo_t bufferInfo =
      MakeMacroBlockInfo(output, blockNumber, byteStride);
  HDPDecOutputMB_GrayScale_SSE2_16(codec, &bufferInfo);
}

void idBareHDP::DecompressImageRGBMacroBlock(std::uint8_t* output,
                                             int blockNumber,
                                             int byteStride) {
  const hdpImageBufferInfo_t bufferInfo =
      MakeMacroBlockInfo(output, blockNumber, byteStride);
  HDPDecOutputMB_RGB_SSE2_16(codec, &bufferInfo);
}

void idBareHDP::DecompressImageYCoCgMacroBlock(std::uint8_t* output,
                                               int blockNumber,
                                               int byteStride) {
  const hdpImageBufferInfo_t bufferInfo =
      MakeMacroBlockInfo(output, blockNumber, byteStride);
  HDPDecOutputMB_CoCg_Y_SSE2_16(codec, &bufferInfo);
}

void idBareHDP::DecompressImageNormalMapMacroBlock(std::uint8_t* output,
                                                   int blockNumber,
                                                   int byteStride) {
  const hdpImageBufferInfo_t bufferInfo =
      MakeMacroBlockInfo(output, blockNumber, byteStride);
  HDPDecOutputMB_Normal_Y_X_SSE2_16(codec, &bufferInfo);
}

void idBareHDP::TerminateMacroBlockRowDecoding() {
  HDPDecShutdown(codec);
}

bool DecodeHDPhoto(hdpImageInfo_t* imageInfo, hdpCodecParms_t* codecParms,
                   std::uint8_t* output, int width, int height,
                   char mipLevel, int* tempUsed) {
  const int thumbnailWidth = width >> mipLevel;
  const int thumbnailHeight = height >> mipLevel;
  codecParms->thumbnailWidth = static_cast<std::uint64_t>(thumbnailWidth);
  codecParms->thumbnailHeight = static_cast<std::uint64_t>(thumbnailHeight);
  const std::uint64_t byteStride =
      static_cast<std::uint64_t>(thumbnailWidth) *
      (imageInfo->bitsPerUnit >> 3);
  const int linesPerMacroblock = 16 >> mipLevel;
  hdpCodec_t* codec = nullptr;
  if (HDPDecInit(imageInfo, codecParms, &codec) != HDP_ERROR_NONE)
    return false;
  *tempUsed = static_cast<int>(codecParms->tempBufferUsed);

  for (int row = 0; row < thumbnailHeight; row += linesPerMacroblock) {
    hdpImageBufferInfo_t bufferInfo{};
    bufferInfo.buffer = output + byteStride * row;
    bufferInfo.lineCount = static_cast<std::uint64_t>(
        std::min(linesPerMacroblock, thumbnailHeight - row));
    bufferInfo.macroblockIndex = 0;
    bufferInfo.byteStride = byteStride;
    if (HDPDecDecodeMBRow(codec) != HDP_ERROR_NONE ||
        HDPDecOutputMBRowFast_SSE2_16(codec, &bufferInfo) != HDP_ERROR_NONE) {
      return false;
    }
  }
  if (HDPDecShutdown(codec) != HDP_ERROR_NONE)
    return false;

  std::uint64_t streamPosition = 0;
  GetPosWS_Memory(codecParms->baseStream, &streamPosition);
  codecParms->baseStream->Close(codecParms->baseStream);
  return true;
}

idBareHDP::idBareHDP()
    : quality(0),
      tempBuffer(nullptr),
      tempBufferSize(0),
      tempBufferUsed(0),
      imageROILeftX(0),
      imageROITopY(0),
      imageROIWidth(0),
      imageROIHeight(0),
      imageByteStride(0),
      codec(nullptr),
      streamTemp{} {}

bool idBareHDP::DecompressImageMono(const std::uint8_t* input,
                                    std::uint8_t* output, int width,
                                    int height, int inputBytes,
                                    char mipLevel) {
  return DecompressImage(*this, BareHdpTarget::Mono, input, output, width,
                         height, inputBytes, mipLevel);
}

bool idBareHDP::DecompressImageGrayScale(const std::uint8_t* input,
                                         std::uint8_t* output, int width,
                                         int height, int inputBytes,
                                         char mipLevel) {
  return DecompressImage(*this, BareHdpTarget::GrayScale, input, output,
                         width, height, inputBytes, mipLevel);
}

bool idBareHDP::DecompressImageRGB(const std::uint8_t* input,
                                   std::uint8_t* output, int width,
                                   int height, int inputBytes,
                                   char mipLevel) {
  return DecompressImage(*this, BareHdpTarget::RGB, input, output, width,
                         height, inputBytes, mipLevel);
}

bool idBareHDP::DecompressImageYCoCg(const std::uint8_t* input,
                                     std::uint8_t* output, int width,
                                     int height, int inputBytes,
                                     char mipLevel) {
  return DecompressImage(*this, BareHdpTarget::YCoCg, input, output, width,
                         height, inputBytes, mipLevel);
}

bool idBareHDP::DecompressImageNormalMap(const std::uint8_t* input,
                                         std::uint8_t* output, int width,
                                         int height, int inputBytes,
                                         char mipLevel) {
  return DecompressImage(*this, BareHdpTarget::NormalMap, input, output,
                         width, height, inputBytes, mipLevel);
}

void idBareHDP::BeginDecompressImageGrayScale(const std::uint8_t* input,
                                              int width, int height,
                                              int inputBytes) {
  BeginDecompressImage(*this, BareHdpTarget::GrayScale, input, width, height,
                       inputBytes);
}

void idBareHDP::BeginDecompressImageRGB(const std::uint8_t* input, int width,
                                        int height, int inputBytes) {
  BeginDecompressImage(*this, BareHdpTarget::RGB, input, width, height,
                       inputBytes);
}

void idBareHDP::BeginDecompressImageYCoCg(const std::uint8_t* input,
                                          int width, int height,
                                          int inputBytes) {
  BeginDecompressImage(*this, BareHdpTarget::YCoCg, input, width, height,
                       inputBytes);
}

void idBareHDP::BeginDecompressImageNormalMap(const std::uint8_t* input,
                                              int width, int height,
                                              int inputBytes) {
  BeginDecompressImage(*this, BareHdpTarget::NormalMap, input, width, height,
                       inputBytes);
}
