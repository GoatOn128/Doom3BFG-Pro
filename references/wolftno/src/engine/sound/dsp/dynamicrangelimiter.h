#pragma once

#include "engine/sound/dsp/envelope.h"

struct dynamicRangeLimiterParms_t {
  float threshold{0.0F};
  float releaseTime{1.0F};
};

struct mgDynamicRangeLimiter {
  void Initialize(float sampleRate, const dynamicRangeLimiterParms_t* parms);
  bool ProcessSilence(unsigned int numFrames);
  void ProcessInterleavedAny(const float* bufferIn, float* bufferOut,
                             int numSamples, int numChannels);
  void ProcessInterleaved6(float* bufferIn, float* bufferOut, int numSamples);
  void ProcessInterleaved8(float* bufferIn, float* bufferOut, int numSamples);
  void ProcessInterleaved(float* bufferIn, float* bufferOut,
                          int numFrames, int numChannels);

  mgEnvelope releaseEnvelope;
  float threshold{0.0F};
  float envelopeState{0.0F};
  float lastLevel{0.0F};
  float lastGainReduction{1.0F};
};

struct mgDynamicRangeLimiterApply {
  void ProcessInterleaved(const float* bufferIn, float* bufferOut,
                          int numFrames, int numChannels);

  const mgDynamicRangeLimiter* limiter{nullptr};
};

static_assert(sizeof(dynamicRangeLimiterParms_t) == 8);
