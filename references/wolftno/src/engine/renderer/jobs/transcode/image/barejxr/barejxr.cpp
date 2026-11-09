#include "engine/renderer/jobs/transcode/image/barejxr/barejxr.h"

#include "engine/renderer/jobs/transcode/image/barejxr/barejxr_types.h"

int jxr_init_read_stripe_bitstream(jxr_image* image,
                                   const std::uint8_t* input,
                                   std::uint64_t inputBytes);
int jxr_read_stripe_bitstream(jxr_image* image);
void jxr_output_mb_id_mono(jxr_image* image, int blockNumber,
                           std::uint8_t* output, int byteStride);
void jxr_output_mb_id_grayscale(jxr_image* image, int blockNumber,
                                std::uint8_t* output, int byteStride);
void jxr_output_mb_id_rgb(jxr_image* image, int blockNumber,
                          std::uint8_t* output, int byteStride);
void jxr_output_mb_id_cocg_y(jxr_image* image, int blockNumber,
                             std::uint8_t* output, int byteStride);
void jxr_output_mb_id_normal_y_x(jxr_image* image, int blockNumber,
                                 std::uint8_t* output, int byteStride);

namespace {

using OutputMacroblock = void (*)(jxr_image*, int, std::uint8_t*, int);

void BeginDecompression(idBareJXR& decoder, const std::uint8_t* input,
                        int inputBytes) {
  jxr_image* image = jxr_create_input(decoder.tempBuffer);
  jxr_set_temp_memory(image, decoder.tempBuffer, decoder.tempBufferSize,
                      jxr_image_size());
  jxr_init_read_stripe_bitstream(image, input,
                                 static_cast<std::uint64_t>(inputBytes));
}

void ReadMacroblockRow(jxr_image* image) {
  int result = jxr_read_stripe_bitstream(image);
  while (result > 0) {
    result = jxr_read_stripe_bitstream(image);
  }
}

bool DecompressImage(idBareJXR& decoder, const std::uint8_t* input,
                     std::uint8_t* output, int width, int height,
                     int inputBytes, int bytesPerPixel,
                     OutputMacroblock outputMacroblock) {
  BeginDecompression(decoder, input, inputBytes);
  if (height > 0) {
    unsigned int rowCount =
        (static_cast<unsigned int>(height - 1) >> 4) + 1;
    do {
      auto* image = static_cast<jxr_image*>(decoder.tempBuffer);
      ReadMacroblockRow(image);
      std::uint8_t* blockOutput = output;
      for (int x = 0; x < width; x += 16) {
        outputMacroblock(image, static_cast<unsigned int>(x) >> 4,
                         blockOutput, width);
        blockOutput += 16 * bytesPerPixel;
      }
      output += 16 * width * bytesPerPixel;
      --rowCount;
    } while (rowCount != 0);
  }
  return true;
}

}  // namespace

idBareJXR::idBareJXR()
    : quality(0),
      tempBuffer(nullptr),
      tempBufferSize(0),
      tempBufferUsed(0),
      cROILeftX(0),
      cROITopY(0),
      cROIWidth(0),
      cROIHeight(0) {}

void idBareJXR::BeginDecompressImageRGBMacroBlockRow() {
  ReadMacroblockRow(static_cast<jxr_image*>(tempBuffer));
}

void idBareJXR::DecompressImageGrayScaleMacroBlock(std::uint8_t* output,
                                                   int blockNumber,
                                                   int byteStride) {
  jxr_output_mb_id_grayscale(static_cast<jxr_image*>(tempBuffer), blockNumber,
                             output, byteStride);
}

void idBareJXR::DecompressImageRGBMacroBlock(std::uint8_t* output,
                                             int blockNumber,
                                             int byteStride) {
  jxr_output_mb_id_rgb(static_cast<jxr_image*>(tempBuffer), blockNumber, output,
                       byteStride);
}

void idBareJXR::DecompressImageYCoCgMacroBlock(std::uint8_t* output,
                                               int blockNumber,
                                               int byteStride) {
  jxr_output_mb_id_cocg_y(static_cast<jxr_image*>(tempBuffer), blockNumber,
                          output, byteStride);
}

void idBareJXR::DecompressImageNormalMapMacroBlock(std::uint8_t* output,
                                                   int blockNumber,
                                                   int byteStride) {
  jxr_output_mb_id_normal_y_x(static_cast<jxr_image*>(tempBuffer), blockNumber,
                              output, byteStride);
}

bool idBareJXR::DecompressImageMono(const std::uint8_t* input,
                                    std::uint8_t* output, int width,
                                    int height, int inputBytes) {
  return DecompressImage(*this, input, output, width, height, inputBytes, 1,
                         jxr_output_mb_id_mono);
}

bool idBareJXR::DecompressImageGrayScale(const std::uint8_t* input,
                                         std::uint8_t* output, int width,
                                         int height, int inputBytes) {
  return DecompressImage(*this, input, output, width, height, inputBytes, 4,
                         jxr_output_mb_id_grayscale);
}

bool idBareJXR::DecompressImageRGB(const std::uint8_t* input,
                                   std::uint8_t* output, int width, int height,
                                   int inputBytes) {
  return DecompressImage(*this, input, output, width, height, inputBytes, 4,
                         jxr_output_mb_id_rgb);
}

bool idBareJXR::DecompressImageYCoCg(const std::uint8_t* input,
                                     std::uint8_t* output, int width,
                                     int height, int inputBytes) {
  return DecompressImage(*this, input, output, width, height, inputBytes, 4,
                         jxr_output_mb_id_cocg_y);
}

bool idBareJXR::DecompressImageNormalMap(const std::uint8_t* input,
                                         std::uint8_t* output, int width,
                                         int height, int inputBytes) {
  return DecompressImage(*this, input, output, width, height, inputBytes, 4,
                         jxr_output_mb_id_normal_y_x);
}

void idBareJXR::BeginDecompressImageGrayScale(const std::uint8_t* input,
                                              int, int, int inputBytes) {
  BeginDecompression(*this, input, inputBytes);
}

void idBareJXR::BeginDecompressImageRGB(const std::uint8_t* input, int, int,
                                        int inputBytes) {
  BeginDecompression(*this, input, inputBytes);
}

void idBareJXR::BeginDecompressImageYCoCg(const std::uint8_t* input, int, int,
                                          int inputBytes) {
  BeginDecompression(*this, input, inputBytes);
}

void idBareJXR::BeginDecompressImageNormalMap(const std::uint8_t* input, int,
                                              int, int inputBytes) {
  BeginDecompression(*this, input, inputBytes);
}
