#include "engine/renderer/jobs/render/setupcolorgrading.h"

#include "engine/renderer/image.h"
#include "engine/renderer/jobs/render/parmstate.h"

#include <algorithm>
#include <cstdint>
#include <immintrin.h>

namespace {
alignas(16) std::uint8_t colorGradingData[256 * 4];

constexpr float kCurveScale = 2.0504936e-05f; // retail bits 0x37AC0204
constexpr float kEpsilon = 1.0e-05f;
constexpr float kOneThird = 0.33333334f;
constexpr float kTwoThirds = 0.6666667f;
constexpr float kOneOver255 = 0.003921569f;

int TruncateToInt(float value) {
  return _mm_cvtt_ss2si(_mm_set_ss(value));
}

std::uint8_t PackUnsignedByte(int value) {
  return static_cast<std::uint8_t>(std::clamp(value, 0, 255));
}
} // namespace

void SetupColorGrading(const setupColorGradingParms_t* parameters) {
  const int width = parameters->imageLUT->opts.width;
  const int height = parameters->imageLUT->opts.height;
  const int pixelCount = width * height;

  const idVec4& shadow =
      renderThreadParmState->GetParmVec4(parameters->rpCGShadow);
  const idVec4& midtone =
      renderThreadParmState->GetParmVec4(parameters->rpCGMidtone);
  const idVec4& highlight =
      renderThreadParmState->GetParmVec4(parameters->rpCGHighlight);
  const idVec4& colorDodge =
      renderThreadParmState->GetParmVec4(parameters->rpCGColorDodgeBlend);
  const idVec4& colorBurn =
      renderThreadParmState->GetParmVec4(parameters->rpCGColorBurnBlend);
  const idVec4& colorMultiply =
      renderThreadParmState->GetParmVec4(parameters->rpCGColorMultiplyBlend);
  const idVec4& colorScreen =
      renderThreadParmState->GetParmVec4(parameters->rpCGColorScreenBlend);

  float postScale[4];
  float postOffset[4];
  for (int channel = 0; channel < 4; ++channel) {
    const float burnScale =
        (1.0f / (colorBurn[channel] + kEpsilon)) *
        (1.0f - colorScreen[channel]);
    postOffset[channel] = 1.0f - burnScale;
    postScale[channel] =
        (1.0f / ((1.0f - colorDodge[channel]) + kEpsilon)) *
        colorMultiply[channel] * burnScale * kOneOver255;
  }

  if (pixelCount > 0) {
    const int roundedPixelCount = (pixelCount + 3) & ~3;
    for (int pixel = 0; pixel < roundedPixelCount; ++pixel) {
      const float input = static_cast<float>(pixel);
      const float centered = input - 127.5f;
      const float curve = centered * centered * kCurveScale;
      const float positiveShadowCurve = kOneThird - curve;
      const float midtoneCurve = kTwoThirds - curve;
      const float negativeShadowCurve =
          (256.0f - input) / (272.0f - input);
      const float positiveHighlightCurve = input / (input + 16.0f);

      for (int channel = 0; channel < 4; ++channel) {
        const float shadowCurve = shadow[channel] >= 0.0f
                                      ? positiveShadowCurve
                                      : negativeShadowCurve;
        const int afterShadow = TruncateToInt(
            shadowCurve * shadow[channel] + input);
        const int afterMidtone = TruncateToInt(
            static_cast<float>(afterShadow) +
            midtoneCurve * midtone[channel]);
        const float highlightCurve = highlight[channel] >= 0.0f
                                         ? positiveHighlightCurve
                                         : positiveShadowCurve;
        const int afterHighlight = TruncateToInt(
            static_cast<float>(afterMidtone) +
            highlightCurve * highlight[channel]);
        const int output = TruncateToInt(
            (static_cast<float>(afterHighlight) * postScale[channel] +
             postOffset[channel]) *
            255.0f);
        colorGradingData[pixel * 4 + channel] = PackUnsignedByte(output);
      }
    }
  }

  parameters->imageLUT->SubImageUpload(0, 0, 0, 0, width, height,
                                       colorGradingData, 0);
  renderThreadParmState->SetParmSampler(
      parameters->rpColorGradingLUT,
      reinterpret_cast<const idTextureSampler*>(parameters->imageLUT));
}
