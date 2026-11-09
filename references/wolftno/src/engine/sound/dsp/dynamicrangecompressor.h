#pragma once

#include "engine/sound/dsp/envelope.h"

struct alignas(4) dynamicRangeCompressorParms_t {
  float ratio{0.25F};
  float threshold{-5.0F};
  float attackTime{0.1F};
  float releaseTime{2.0F};
  float makeupGain{1.0F};
  float rmsWindow{0.005F};
  bool useRMS{false};
};

struct mgDynamicRangeCompressor {
  void Initialize(float sampleRate, const dynamicRangeCompressorParms_t* parms,
                  bool applyGain);
  bool ProcessSilence(unsigned int numFrames);
  void ProcessInterleavedRMSAny(const float* bufferIn, float* bufferOut,
                                int numSamples, int numChannels);
  void ProcessInterleavedPeakAny(const float* bufferIn, float* bufferOut,
                                 int numSamples, int numChannels);
  void ProcessInterleavedCalcRMSAny(const float* bufferIn,
                                    int numSamples, int numChannels);
  void ProcessInterleavedCalcPeakAny(float* bufferIn,
                                     int numSamples, int numChannels);
  void ProcessInterleavedRMS6(float* bufferIn, float* bufferOut, int numSamples);
  void ProcessInterleavedPeak6(float* bufferIn, float* bufferOut, int numSamples);
  void ProcessInterleavedCalcRMS6(__m128* bufferIn, int numSamples);
  void ProcessInterleavedCalcPeak6(__m128* bufferIn, int numSamples);
  void ProcessInterleavedRMS8(float* bufferIn, float* bufferOut, int numSamples);
  void ProcessInterleavedPeak8(float* bufferIn, float* bufferOut, int numSamples);
  void ProcessInterleavedCalcRMS8(__m128* bufferIn, int numSamples);
  void ProcessInterleavedCalcPeak8(__m128* bufferIn, int numSamples);
  void ProcessInterleaved(__m128* bufferIn, float* bufferOut,
                          int numFrames, int numChannels);

  mgEnvelope attackEnvelope;
  mgEnvelope releaseEnvelope;
  mgEnvelope meanSquareEnvelope;
  float threshold{0.0F};
  float makeupGain{1.0F};
  float meanSquare{1.0e-15F};
  float envelopeState{1.0e-15F};
  float ratioMinusOne{-0.75F};
  bool useRMS{false};
  bool apply{true};
  float lastLevel{0.0F};
  float lastGainReduction{1.0F};
};

struct mgDynamicRangeCompressorApply {
  void ProcessInterleaved(const float* bufferIn, float* bufferOut,
                          int numFrames, int numChannels);

  const mgDynamicRangeCompressor* compressorCalculate{nullptr};
  float lastLevel{0.0F};
};

static_assert(sizeof(dynamicRangeCompressorParms_t) == 28);
