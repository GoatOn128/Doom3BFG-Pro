#pragma once

#include <cstdint>

class idMipMap {
public:
  static bool CreateSingleMipMultiChannel(const std::uint8_t* src, int width,
                                          int height, int channels,
                                          std::uint8_t* dest);
  static bool CreateSingleMip(const std::uint8_t* src, int width, int height,
                              std::uint8_t* dest);
};
