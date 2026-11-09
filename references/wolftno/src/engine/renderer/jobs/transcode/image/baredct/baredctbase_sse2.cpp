#include "engine/renderer/jobs/transcode/image/baredct/baredct.h"

#include <algorithm>

void idBareDctBase::ScaleQuantTable_SSE2(std::uint16_t* result,
                                         const std::uint16_t* standard,
                                         int scale) {
  const std::int16_t multiplier = static_cast<std::int16_t>(
      2 * scale + ((36701 * scale) >> 16));
  for (int i = 0; i < 64; ++i) {
    const int highWord =
        (static_cast<std::int16_t>(standard[i]) * multiplier) >> 16;
    result[i] = static_cast<std::uint16_t>(std::clamp(highWord, 1, 255));
  }
}

void idBareDctBase::SetQuality_SSE2(int luminance, int chrominance,
                                    int alpha) {
  const int luminanceScale = QuantizationScaleFromQuality(luminance);
  const int chrominanceScale = QuantizationScaleFromQuality(chrominance);
  const int alphaScale = QuantizationScaleFromQuality(alpha);
  ScaleQuantTable_SSE2(quantTableY, std_luminance_quant_tbl, luminanceScale);
  ScaleQuantTable_SSE2(quantTableCoCg, std_chrominance_quant_tbl,
                       chrominanceScale);
  ScaleQuantTable_SSE2(quantTableA, std_alpha_quant_tbl, alphaScale);
}

