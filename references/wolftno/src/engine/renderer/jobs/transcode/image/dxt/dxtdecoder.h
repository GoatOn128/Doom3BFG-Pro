#pragma once

#include <cstddef>
#include <cstdint>

struct idDxtDecoder {
  int width;
  int height;
  const std::uint8_t* inData;

  void DecodeAlphaValues(std::uint8_t* colorBlock, int offset);
  void DecodeColorValues(std::uint8_t* colorBlock, bool noBlack,
                         bool writeAlpha);
  void DecompressImageDXT1(const std::uint8_t* input, std::uint8_t* output,
                           int imageWidth, int imageHeight);
  void DecompressImageDXT5(const std::uint8_t* input, std::uint8_t* output,
                           int imageWidth, int imageHeight);
  void DecompressImageDXT5_nVidia7x(const std::uint8_t* input,
                                    std::uint8_t* output, int imageWidth,
                                    int imageHeight);
  void DecompressYCoCgDXT5(const std::uint8_t* input, std::uint8_t* output,
                           int imageWidth, int imageHeight);
};

static_assert(offsetof(idDxtDecoder, inData) == 8,
              "idDxtDecoder input pointer offset drift");
static_assert(sizeof(idDxtDecoder) == 16, "idDxtDecoder ABI drift");
