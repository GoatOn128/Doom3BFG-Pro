#include "engine/sound/sound_runtime.h"

#include "shared/idlib/lib.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace {

constexpr int kSpeakerCount = 11;
constexpr float kSqrtHalf = 0.70710678F;

// Windows speaker-mask order: FL, FR, FC, LFE, BL, BR, FLC, FRC, BC, SL, SR.
constexpr std::array<idVec2, kSpeakerCount> kSpeakerPositions{{
    {-kSqrtHalf, kSqrtHalf}, {kSqrtHalf, kSqrtHalf}, {0.0F, 1.0F},
    {0.0F, 0.0F},           {-kSqrtHalf, -kSqrtHalf},
    {kSqrtHalf, -kSqrtHalf}, {-0.38268343F, 0.92387950F},
    {0.38268343F, 0.92387950F}, {0.0F, -1.0F}, {-1.0F, 0.0F},
    {1.0F, 0.0F}}};

float Length2D(const idVec3& value) {
  return std::sqrt(value.x * value.x + value.y * value.y);
}

idVec3 TransformDirection(const idMat3& matrix, const idVec3& value) {
  return {value.x * matrix.mat[0].x + value.y * matrix.mat[1].x +
              value.z * matrix.mat[2].x,
          value.x * matrix.mat[0].y + value.y * matrix.mat[1].y +
              value.z * matrix.mat[2].y,
          value.x * matrix.mat[0].z + value.y * matrix.mat[1].z +
              value.z * matrix.mat[2].z};
}

}  // namespace

int idSoundVoice_Base::dstChannels = 0;
int idSoundVoice_Base::dstMask = 0;
int idSoundVoice_Base::dstLFE = -1;
int idSoundVoice_Base::dstCenter = -1;
int idSoundVoice_Base::dstMap[8]{};
int idSoundVoice_Base::invMap[11]{};
int idSoundVoice_Base::speakerLeft[11]{};
int idSoundVoice_Base::speakerRight[11]{};
float idSoundVoice_Base::omniLevel = 1.0F;

idSoundVoice_Base::idSoundVoice_Base() {
  axis = mat3_identity;
  invListenerAxis = mat3_identity;
}

void idSoundVoice_Base::InitSurround(int outputChannels, int channelMask) {
  dstChannels = std::clamp(outputChannels, 0, 8);
  dstMask = channelMask;
  dstLFE = -1;
  dstCenter = -1;
  std::fill(std::begin(dstMap), std::end(dstMap), 0);
  std::fill(std::begin(invMap), std::end(invMap), -1);

  int destination = 0;
  for (int speaker = 0; speaker < kSpeakerCount && destination < dstChannels;
       ++speaker) {
    if ((channelMask & (1 << speaker)) == 0) continue;
    dstMap[destination] = speaker;
    invMap[speaker] = destination;
    if (speaker == 2) dstCenter = destination;
    if (speaker == 3) dstLFE = destination;
    ++destination;
  }

  std::vector<int> ring;
  for (int destinationIndex = 0; destinationIndex < destination;
       ++destinationIndex) {
    const int speaker = dstMap[destinationIndex];
    if (speaker != 2 && speaker != 3) ring.push_back(speaker);
  }
  std::sort(ring.begin(), ring.end(), [](int left, int right) {
    const float leftAngle =
        std::atan2(kSpeakerPositions[left].x, kSpeakerPositions[left].y);
    const float rightAngle =
        std::atan2(kSpeakerPositions[right].x, kSpeakerPositions[right].y);
    return leftAngle < rightAngle;
  });
  for (std::size_t index = 0; index < ring.size(); ++index) {
    speakerLeft[ring[index]] =
        ring[(index + ring.size() - 1) % ring.size()];
    speakerRight[ring[index]] = ring[(index + 1) % ring.size()];
  }
  speakerLeft[2] = speakerRight[2] = 2;
  speakerLeft[3] = speakerRight[3] = 3;

  const int spatialChannels =
      std::max(1, dstChannels - (dstLFE >= 0 ? 1 : 0) -
                      (dstCenter >= 0 ? 1 : 0));
  omniLevel = 1.0F / static_cast<float>(spatialChannels);
}

void idSoundVoice_Base::ReleaseKey() {
  if (keyOff) return;
  keyOff = true;
  keyOffHandled = false;
  if (tailSample != nullptr) tailSample->PreCacheStreamData(true);
}

void idSoundVoice_Base::CalculateChannelSurround(
    int sourceChannel, int sourceChannels, float* levelMatrix,
    const idVec3& direction, float centerScale, float scale) {
  if (levelMatrix == nullptr || sourceChannel < 0 ||
      sourceChannel >= sourceChannels || dstChannels <= 0)
    return;

  const float length = Length2D(direction);
  const float clampedCenter = std::clamp(centerScale, 0.0F, 1.0F);
  if (length <= 1.0e-6F) {
    const float nonCenterScale = dstCenter >= 0 ? 1.0F - clampedCenter : 1.0F;
    for (int destination = 0; destination < dstChannels; ++destination) {
      if (destination == dstLFE) continue;
      const float level = destination == dstCenter
                              ? std::sqrt(clampedCenter)
                              : std::sqrt(omniLevel * nonCenterScale);
      levelMatrix[sourceChannel * dstChannels + destination] = level * scale;
    }
    return;
  }

  const float x = direction.x / length;
  const float y = direction.y / length;
  int best = -1;
  int second = -1;
  float bestDot = -2.0F;
  float secondDot = -2.0F;
  for (int destination = 0; destination < dstChannels; ++destination) {
    if (destination == dstLFE || destination == dstCenter) continue;
    const idVec2& speaker = kSpeakerPositions[dstMap[destination]];
    const float dot = x * speaker.x + y * speaker.y;
    if (dot > bestDot) {
      second = best;
      secondDot = bestDot;
      best = destination;
      bestDot = dot;
    } else if (dot > secondDot) {
      second = destination;
      secondDot = dot;
    }
  }

  const float distanceBlend = innerRadius <= 0.0F
                                  ? 1.0F
                                  : std::clamp(length / innerRadius, 0.0F, 1.0F);
  const float center = dstCenter >= 0
                           ? std::clamp(std::max(0.0F, y) * clampedCenter,
                                        0.0F, 1.0F)
                           : 0.0F;
  const float spatialScale = 1.0F - center;
  for (int destination = 0; destination < dstChannels; ++destination) {
    if (destination == dstLFE) continue;
    float energy = (1.0F - distanceBlend) * omniLevel;
    if (destination == best) energy += distanceBlend * 0.65F;
    if (destination == second) energy += distanceBlend * 0.35F;
    if (destination == dstCenter) {
      energy = center;
    } else {
      energy *= spatialScale;
    }
    levelMatrix[sourceChannel * dstChannels + destination] =
        std::sqrt(std::max(0.0F, energy)) * scale;
  }
}

std::int64_t idSoundVoice_Base::MsecToSamples(int offsetMS,
                                               bool looping) const {
  if (sample == nullptr || sample->format.basic.samplesPerSec == 0 ||
      offsetMS < 0)
    return -1;
  const std::int64_t sampleOffset =
      static_cast<std::int64_t>(offsetMS) *
      sample->format.basic.samplesPerSec / 1000;
  const int sampleLength = sample->lastValidSample - sample->firstValidSample;
  if (sampleOffset < sampleLength) return sampleOffset;
  if (!looping) return -1;
  const int loopLength =
      sample->lastValidSample - sample->loopPoint - sample->firstValidSample;
  if (loopLength > 0)
    return sample->loopPoint + (sampleOffset - sample->loopPoint) % loopLength;
  idLib::Warning("Loop point at end of sample '%s'\n", sample->GetName());
  return sampleOffset;
}

void idSoundVoice_Base::CalculateSurround(int sourceChannels,
                                           float* levelMatrix, float scale,
                                           int startFlags) {
  if (sourceChannels <= 0 || levelMatrix == nullptr) return;
  std::fill(levelMatrix,
            levelMatrix + static_cast<std::size_t>(sourceChannels) *
                              static_cast<std::size_t>(dstChannels),
            0.0F);

  int spatialSourceChannels = sourceChannels;
  if ((startFlags & SSF_LFE) != 0 && dstLFE >= 0) {
    levelMatrix[(sourceChannels - 1) * dstChannels + dstLFE] =
        scale * gainLFE;
    --spatialSourceChannels;
  }
  if (spatialSourceChannels <= 0) return;

  const idVec3 listenerDirection = TransformDirection(invListenerAxis, position);
  if (spatialSourceChannels == 1) {
    CalculateChannelSurround(0, sourceChannels, levelMatrix,
                             listenerDirection, centerChannel, scale);
    return;
  }

  if ((startFlags & SSF_OMNIDIRECTIONAL) != 0) {
    for (int source = 0; source < spatialSourceChannels; ++source) {
      CalculateChannelSurround(source, sourceChannels, levelMatrix, {},
                               centerChannel * 0.5F, scale);
    }
    return;
  }

  for (int source = 0; source < spatialSourceChannels; ++source) {
    idVec3 sourceDirection = listenerDirection;
    if (spatialSourceChannels == 2) {
      const float side = source == 0 ? -innerRadius : innerRadius;
      sourceDirection = TransformDirection(
          invListenerAxis,
          position + axis.mat[1] * side);
    }
    CalculateChannelSurround(source, sourceChannels, levelMatrix,
                             sourceDirection, centerChannel * 0.5F, scale);
  }
}

const idSoundSample* idSoundVoice_Base::SelectSample(
    const idSoundSample* previousSample) {
  if (samples.empty())
    return previousSample != nullptr ? previousSample : tailSample;
  if (isPreRandomLoopSample) {
    previousSample = nullptr;
    isPreRandomLoopSample = false;
  }
  if (samples.size() == 1) return samples.front();

  std::vector<const idSoundSample*> candidates;
  candidates.reserve(samples.size());
  for (const idSoundSample* candidate : samples) {
    if (candidate != previousSample) candidates.push_back(candidate);
  }
  if (candidates.empty()) return previousSample;
  soundRandomSeed = 1664525U * soundRandomSeed + 1013904223U;
  const std::size_t index =
      static_cast<std::size_t>((soundRandomSeed >> 10U) & 0x7FFFU) %
      candidates.size();
  return candidates[index];
}
