#pragma once

#include <cstdint>

class idColorSpace {
public:
  static void ConvertRGBToCoCg_Y(std::uint8_t* dst, const std::uint8_t* src,
                                 int width, int height);
  static void ConvertRGBAToCoACg_Y(std::uint8_t* dst, const std::uint8_t* src,
                                   int width, int height);
};
