#include "engine/sound/dsp/dynamicrangecompressor.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kDcOffset = 1.0e-15F;
constexpr float kNaturalLogToDecibels = 8.6858892F;
constexpr float kDecibelsToNaturalLog = 0.11512925F;

float LevelToDecibels(float level) {
  return level <= 0.0F ? -999.0F : std::log(level) * kNaturalLogToDecibels;
}

float AdvanceCompressorEnvelope(mgDynamicRangeCompressor& compressor,
                                float detectorLevel) {
  const float target =
      std::max(0.0F, LevelToDecibels(detectorLevel) - compressor.threshold) +
      kDcOffset;
  const float difference = target - compressor.envelopeState;
  const float decay = difference < 0.0F ? compressor.releaseEnvelope.decay
                                        : compressor.attackEnvelope.decay;
  compressor.envelopeState =
      (compressor.envelopeState - target) * decay + target - kDcOffset;
  return std::exp(compressor.envelopeState * compressor.ratioMinusOne *
                  kDecibelsToNaturalLog) *
         compressor.makeupGain;
}

float DetectFrame(mgDynamicRangeCompressor& compressor, const float* frame,
                  int numChannels, bool rms) {
  if (!rms) {
    float peak = 0.0F;
    for (int channel = 0; channel < numChannels; ++channel) {
      peak = std::max(peak, std::fabs(frame[channel]));
    }
    return peak + kDcOffset;
  }

  float sumSquares = 0.0F;
  for (int channel = 0; channel < numChannels; ++channel) {
    sumSquares += frame[channel] * frame[channel];
  }
  const float target = sumSquares + kDcOffset;
  compressor.meanSquare =
      (compressor.meanSquare - target) * compressor.meanSquareEnvelope.decay + target;
  return std::sqrt(compressor.meanSquare) + kDcOffset;
}

void ProcessCompressor(mgDynamicRangeCompressor& compressor,
                       const float* bufferIn, float* bufferOut,
                       int numSamples, int numChannels, bool rms,
                       bool applyGain) {
  if (numSamples <= 0) {
    compressor.lastGainReduction = 0.0F;
    return;
  }

  float gain = 0.0F;
  for (int sample = 0; sample < numSamples; sample += numChannels) {
    gain = AdvanceCompressorEnvelope(
        compressor, DetectFrame(compressor, bufferIn + sample, numChannels, rms));
    if (applyGain) {
      for (int channel = 0; channel < numChannels; ++channel) {
        bufferOut[sample + channel] = gain * bufferIn[sample + channel];
      }
    }
  }
  compressor.lastGainReduction = gain;
}

}  // namespace

void mgDynamicRangeCompressor::Initialize(
    float sampleRate, const dynamicRangeCompressorParms_t* parms,
    bool applyGain) {
  apply = applyGain;
  threshold = parms->threshold;
  makeupGain = parms->makeupGain;
  envelopeState = kDcOffset;
  meanSquare = kDcOffset;
  useRMS = parms->useRMS;
  lastLevel = 0.0F;
  lastGainReduction = 1.0F;
  ratioMinusOne = parms->ratio - 1.0F;
  attackEnvelope.Initialize(sampleRate, parms->attackTime);
  releaseEnvelope.Initialize(sampleRate, parms->releaseTime);
  meanSquareEnvelope.Initialize(sampleRate, parms->rmsWindow);
}

bool mgDynamicRangeCompressor::ProcessSilence(unsigned int numFrames) {
  float gain = 0.0F;
  for (unsigned int frame = 0; frame < numFrames; ++frame) {
    gain = AdvanceCompressorEnvelope(*this, kDcOffset);
  }
  lastGainReduction = gain;
  return gain < 0.99900001F;
}

void mgDynamicRangeCompressor::ProcessInterleavedRMSAny(
    const float* bufferIn, float* bufferOut, int numSamples, int numChannels) {
  ProcessCompressor(*this, bufferIn, bufferOut, numSamples, numChannels, true, true);
}

void mgDynamicRangeCompressor::ProcessInterleavedPeakAny(
    const float* bufferIn, float* bufferOut, int numSamples, int numChannels) {
  ProcessCompressor(*this, bufferIn, bufferOut, numSamples, numChannels, false, true);
}

void mgDynamicRangeCompressor::ProcessInterleavedCalcRMSAny(
    const float* bufferIn, int numSamples, int numChannels) {
  ProcessCompressor(*this, bufferIn, nullptr, numSamples, numChannels, true, false);
}

void mgDynamicRangeCompressor::ProcessInterleavedCalcPeakAny(
    float* bufferIn, int numSamples, int numChannels) {
  ProcessCompressor(*this, bufferIn, nullptr, numSamples, numChannels, false, false);
}

void mgDynamicRangeCompressor::ProcessInterleavedRMS6(
    float* bufferIn, float* bufferOut, int numSamples) {
  ProcessInterleavedRMSAny(bufferIn, bufferOut, numSamples, 6);
}

void mgDynamicRangeCompressor::ProcessInterleavedPeak6(
    float* bufferIn, float* bufferOut, int numSamples) {
  ProcessInterleavedPeakAny(bufferIn, bufferOut, numSamples, 6);
}

void mgDynamicRangeCompressor::ProcessInterleavedCalcRMS6(
    __m128* bufferIn, int numSamples) {
  ProcessInterleavedCalcRMSAny(bufferIn->m128_f32, numSamples, 6);
}

void mgDynamicRangeCompressor::ProcessInterleavedCalcPeak6(
    __m128* bufferIn, int numSamples) {
  ProcessInterleavedCalcPeakAny(bufferIn->m128_f32, numSamples, 6);
}

void mgDynamicRangeCompressor::ProcessInterleavedRMS8(
    float* bufferIn, float* bufferOut, int numSamples) {
  ProcessInterleavedRMSAny(bufferIn, bufferOut, numSamples, 8);
}

void mgDynamicRangeCompressor::ProcessInterleavedPeak8(
    float* bufferIn, float* bufferOut, int numSamples) {
  ProcessInterleavedPeakAny(bufferIn, bufferOut, numSamples, 8);
}

void mgDynamicRangeCompressor::ProcessInterleavedCalcRMS8(
    __m128* bufferIn, int numSamples) {
  ProcessInterleavedCalcRMSAny(bufferIn->m128_f32, numSamples, 8);
}

void mgDynamicRangeCompressor::ProcessInterleavedCalcPeak8(
    __m128* bufferIn, int numSamples) {
  ProcessInterleavedCalcPeakAny(bufferIn->m128_f32, numSamples, 8);
}

void mgDynamicRangeCompressor::ProcessInterleaved(
    __m128* bufferIn, float* bufferOut, int numFrames, int numChannels) {
  const int numSamples = numChannels * numFrames;
  float* samples = bufferIn->m128_f32;
  if (apply) {
    if (numChannels == 6) {
      useRMS ? ProcessInterleavedRMS6(samples, bufferOut, numSamples)
             : ProcessInterleavedPeak6(samples, bufferOut, numSamples);
    } else if (numChannels == 8) {
      useRMS ? ProcessInterleavedRMS8(samples, bufferOut, numSamples)
             : ProcessInterleavedPeak8(samples, bufferOut, numSamples);
    } else if (useRMS) {
      ProcessInterleavedRMSAny(samples, bufferOut, numSamples, numChannels);
    } else {
      ProcessInterleavedPeakAny(samples, bufferOut, numSamples, numChannels);
    }
  } else if (numChannels == 6) {
    useRMS ? ProcessInterleavedCalcRMS6(bufferIn, numSamples)
           : ProcessInterleavedCalcPeak6(bufferIn, numSamples);
  } else if (numChannels == 8) {
    useRMS ? ProcessInterleavedCalcRMS8(bufferIn, numSamples)
           : ProcessInterleavedCalcPeak8(bufferIn, numSamples);
  } else if (useRMS) {
    ProcessInterleavedCalcRMSAny(samples, numSamples, numChannels);
  } else {
    ProcessInterleavedCalcPeakAny(samples, numSamples, numChannels);
  }
}

void mgDynamicRangeCompressorApply::ProcessInterleaved(
    const float* bufferIn, float* bufferOut, int numFrames, int numChannels) {
  const int numSamples = numChannels * numFrames;
  const float gain = compressorCalculate->lastGainReduction;
  for (int sample = 0; sample < numSamples; ++sample) {
    bufferOut[sample] = gain * bufferIn[sample];
  }
}
