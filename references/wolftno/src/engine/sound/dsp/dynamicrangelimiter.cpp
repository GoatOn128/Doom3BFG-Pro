#include "engine/sound/dsp/dynamicrangelimiter.h"

#include <algorithm>
#include <cmath>

namespace {

float AdvanceLimiterEnvelope(mgDynamicRangeLimiter& limiter, float level) {
  const float target = std::max(limiter.threshold, level);
  if (target < limiter.envelopeState) {
    limiter.envelopeState =
        (limiter.envelopeState - target) * limiter.releaseEnvelope.decay + target;
  } else {
    limiter.envelopeState = target;
  }
  return limiter.threshold / limiter.envelopeState;
}

void ProcessLimiter(mgDynamicRangeLimiter& limiter, const float* bufferIn,
                    float* bufferOut, int numSamples, int numChannels) {
  if (numSamples <= 0) {
    limiter.lastGainReduction = 0.0F;
    return;
  }

  float gain = 0.0F;
  for (int sample = 0; sample < numSamples; sample += numChannels) {
    float level = 0.0F;
    for (int channel = 0; channel < numChannels; ++channel) {
      level = std::max(level, std::fabs(bufferIn[sample + channel]));
    }
    gain = AdvanceLimiterEnvelope(limiter, level);
    for (int channel = 0; channel < numChannels; ++channel) {
      bufferOut[sample + channel] = gain * bufferIn[sample + channel];
    }
  }
  limiter.lastGainReduction = gain;
}

}  // namespace

void mgDynamicRangeLimiter::Initialize(
    float sampleRate, const dynamicRangeLimiterParms_t* parms) {
  threshold = std::exp(parms->threshold * 0.11512925F);
  lastLevel = 0.0F;
  lastGainReduction = 1.0F;
  envelopeState = threshold;
  releaseEnvelope.Initialize(sampleRate, parms->releaseTime);
}

bool mgDynamicRangeLimiter::ProcessSilence(unsigned int numFrames) {
  float gain = 0.0F;
  for (unsigned int frame = 0; frame < numFrames; ++frame) {
    gain = AdvanceLimiterEnvelope(*this, 0.0F);
  }
  lastGainReduction = gain;
  return gain < 0.99900001F;
}

void mgDynamicRangeLimiter::ProcessInterleavedAny(
    const float* bufferIn, float* bufferOut, int numSamples, int numChannels) {
  ProcessLimiter(*this, bufferIn, bufferOut, numSamples, numChannels);
}

void mgDynamicRangeLimiter::ProcessInterleaved6(
    float* bufferIn, float* bufferOut, int numSamples) {
  ProcessLimiter(*this, bufferIn, bufferOut, numSamples, 6);
}

void mgDynamicRangeLimiter::ProcessInterleaved8(
    float* bufferIn, float* bufferOut, int numSamples) {
  ProcessLimiter(*this, bufferIn, bufferOut, numSamples, 8);
}

void mgDynamicRangeLimiter::ProcessInterleaved(
    float* bufferIn, float* bufferOut, int numFrames, int numChannels) {
  const int numSamples = numChannels * numFrames;
  if (numChannels == 6) {
    ProcessInterleaved6(bufferIn, bufferOut, numSamples);
  } else if (numChannels == 8) {
    ProcessInterleaved8(bufferIn, bufferOut, numSamples);
  } else {
    ProcessInterleavedAny(bufferIn, bufferOut, numSamples, numChannels);
  }
}

void mgDynamicRangeLimiterApply::ProcessInterleaved(
    const float* bufferIn, float* bufferOut, int numFrames, int numChannels) {
  const int numSamples = numChannels * numFrames;
  const float gain = limiter->lastGainReduction;
  for (int sample = 0; sample < numSamples; ++sample) {
    bufferOut[sample] = gain * bufferIn[sample];
  }
}
