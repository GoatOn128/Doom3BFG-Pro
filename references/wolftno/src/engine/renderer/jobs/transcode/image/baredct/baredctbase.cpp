#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"

#include <algorithm>

#define JPEG_ROW(a,b,c,d,e,f,g,h) \
  (a)*256, (b)*256, (c)*256, (d)*256, (e)*256, (f)*256, (g)*256, (h)*256

alignas(16) const std::uint16_t std_luminance_quant_tbl[64] = {
    JPEG_ROW(16,11,10,16,24,40,51,61), JPEG_ROW(12,12,14,19,26,58,60,55),
    JPEG_ROW(14,13,16,24,40,57,69,56), JPEG_ROW(14,17,22,29,51,87,80,62),
    JPEG_ROW(18,22,37,56,68,109,103,77), JPEG_ROW(24,35,55,64,81,104,113,92),
    JPEG_ROW(49,64,78,87,103,121,120,101), JPEG_ROW(72,92,95,98,112,100,103,99)};

alignas(16) const std::uint16_t std_chrominance_quant_tbl[64] = {
    JPEG_ROW(17,18,24,47,99,99,99,99), JPEG_ROW(18,21,26,66,99,99,99,99),
    JPEG_ROW(24,26,56,99,99,99,99,99), JPEG_ROW(47,66,99,99,99,99,99,99),
    JPEG_ROW(99,99,99,99,99,99,99,99), JPEG_ROW(99,99,99,99,99,99,99,99),
    JPEG_ROW(99,99,99,99,99,99,99,99), JPEG_ROW(99,99,99,99,99,99,99,99)};

alignas(16) const std::uint16_t std_alpha_quant_tbl[64] = {
    JPEG_ROW(16,11,10,16,24,40,51,61), JPEG_ROW(12,12,14,19,26,58,60,55),
    JPEG_ROW(14,13,16,24,40,57,69,56), JPEG_ROW(14,17,22,29,51,87,80,62),
    JPEG_ROW(18,22,37,56,68,109,103,77), JPEG_ROW(24,35,55,64,81,104,113,92),
    JPEG_ROW(49,64,78,87,103,121,120,101), JPEG_ROW(72,92,95,98,112,100,103,99)};

#undef JPEG_ROW

namespace {
std::uint16_t ScaleEntry(std::uint16_t standard, int scale) {
  const int multiplier = ((36701 * scale) >> 16) + 2 * scale;
  return static_cast<std::uint16_t>(
      std::clamp((multiplier * static_cast<int>(standard)) >> 16, 1, 255));
}
}

idBareDctBase::idBareDctBase()
    : luminanceQuality(75), chrominanceQuality(75), alphaQuality(75),
      sampleMode(SAMPLE_420) {}

void idBareDctBase::InitQuantTable() {
  SetQuality_SSE2(luminanceQuality, chrominanceQuality, alphaQuality);
}

int idBareDctBase::QuantizationScaleFromQuality(int quality) {
  if (quality <= 0)
    return 5000;
  if (quality > 100)
    quality = 100;
  if (quality >= 50)
    return 2 * (100 - quality);
  return 5000 / quality;
}

void idBareDctBase::SetQuality_Generic(int luminance, int chrominance,
                                       int alpha) {
  const int luminanceScale = QuantizationScaleFromQuality(luminance);
  const int chrominanceScale = QuantizationScaleFromQuality(chrominance);
  const int alphaScale = QuantizationScaleFromQuality(alpha);
  for (int i = 0; i < 64; ++i) {
    quantTableY[i] = ScaleEntry(std_luminance_quant_tbl[i], luminanceScale);
    quantTableCoCg[i] =
        ScaleEntry(std_chrominance_quant_tbl[i], chrominanceScale);
    quantTableA[i] = ScaleEntry(std_alpha_quant_tbl[i], alphaScale);
  }
}

