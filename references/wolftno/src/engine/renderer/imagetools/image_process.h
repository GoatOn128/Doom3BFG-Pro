#pragma once

#include "shared/idlib/math/vector.h"

#include <cstdint>

std::uint8_t* R_ResampleTexture(const std::uint8_t* input, int inputWidth,
                                unsigned int inputHeight,
                                unsigned int outputWidth, int outputHeight);
void R_HorizontalFlip(std::uint8_t* data, int width, int height);
void R_VerticalFlip(std::uint8_t* data, int width, int height);
void R_RotatePic(std::uint8_t* data, int width);
std::uint8_t* R_Dropsample(const std::uint8_t* input, int inputWidth,
                           int inputHeight, unsigned int outputWidth,
                           int outputHeight);
void R_SetAlphaNormalDivergence(std::uint8_t* data, int width, int height);
std::uint8_t* R_MipMap(const std::uint8_t* input, int width, int height);
std::uint8_t* R_CubeTexel(int size, std::uint8_t** data, idVec3* direction);
void R_FilterCube(int size, std::uint8_t** data, int filterSize);
void R_BlendCubeBorders(int size, std::uint8_t** data);
