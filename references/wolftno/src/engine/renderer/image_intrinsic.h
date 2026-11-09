#pragma once

#include <cstdint>

class idImageIntrinsic {
public:
  static void WaveImage();
  static void BeckmannImage();
  static void FresnelImage();
  static void DefaultImage();
  static void WhiteImage();
  static void BlackImage();
  static void FlatNormalImage();
  static void DefaultCubeImage();
  static void DefaultColorCorrectionTex();
  static void Dither2Image();
  static void Dither4Image();
  static void Dither16Image();
  static void Dither256Image();
  static void BorderedPointWhiteImage();
  static void NoFalloffImage();
};

float uni();
float PHBeckmann(float ndoth, float m);
std::uint8_t* GenerateColorCorrectionTex(int cubeDim, int texDim);
