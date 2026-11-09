#include "engine/sound/sound_runtime.h"

#include "shared/idlib/lib.h"

#include <algorithm>

void idSoundFade::Clear() {
  fadeStartTime = 0;
  fadeEndTime = 0;
  fadeStartVolume = 1.0F;
  fadeEndVolume = 1.0F;
  linear = false;
}

void idSoundFade::SetVolume(float to) {
  fadeStartVolume = to;
  fadeEndVolume = to;
  fadeStartTime = 0;
  fadeEndTime = 0;
}

void idSoundFade::Fade(float to, int length, int soundTime, int delay,
                       bool linear_) {
  fadeStartVolume = GetVolume(soundTime);
  fadeEndVolume = to;
  fadeStartTime = soundTime + delay;
  fadeEndTime = fadeStartTime + std::max(0, length);
  linear = linear_;
}

float idSoundFade::GetVolume(int soundTime) const {
  if (fadeEndTime <= fadeStartTime || soundTime <= fadeStartTime)
    return soundTime < fadeStartTime ? fadeStartVolume : fadeEndVolume;
  if (soundTime >= fadeEndTime) return fadeEndVolume;
  const float amount = static_cast<float>(soundTime - fadeStartTime) /
                       static_cast<float>(fadeEndTime - fadeStartTime);
  return fadeStartVolume + (fadeEndVolume - fadeStartVolume) * amount;
}

void soundShaderParms_t::Override(const soundShaderParms_t* over) {
  if (over != nullptr) *this = *over;
}

idSoundEmitter::~idSoundEmitter() = default;

idSoundEmitterLocal::idSoundEmitterLocal() = default;

idSoundEmitterLocal::~idSoundEmitterLocal() {
  Reset();
}

void idSoundEmitterLocal::Init(int i, idSoundWorldLocal* sw) {
  index = i;
  soundWorld = sw;
  canFree = false;
}

bool idSoundEmitterLocal::CheckForCompletion(int currentTime) {
  bool removed = false;
  for (auto iterator = channels.begin(); iterator != channels.end();) {
    if ((*iterator)->CheckForCompletion(currentTime)) {
      delete *iterator;
      iterator = channels.erase(iterator);
      removed = true;
    } else {
      ++iterator;
    }
  }
  return removed;
}

float idSoundEmitterLocal::Update(listener_t* listener, int currentTime) {
  (void)listener;
  CheckForCompletion(currentTime);
  for (idSoundChannel* channel : channels) channel->UpdateVolume(currentTime);
  return channels.empty() ? 0.0F : 1.0F;
}

void idSoundEmitterLocal::Free(bool instant) {
  (void)instant;
  Reset();
  canFree = true;
}

void idSoundEmitterLocal::Reset() {
  for (idSoundChannel* channel : channels) delete channel;
  channels.clear();
}

void idSoundEmitterLocal::UpdateEmitter(const idVec3& newOrigin,
                                        const idMat3& newAxis,
                                        const idVec3& newVelocity,
                                        int listenerId, bool virtualize) {
  (void)listenerId;
  (void)virtualize;
  origin = newOrigin;
  axis = newAxis;
  velocity = newVelocity;
}

millisecondUnique_t idSoundEmitterLocal::StartSound(
    soundChannel_t channel, const idSoundShader* shader,
    const soundShaderParms_t* parms) {
  StopSound(channel);
  auto* soundChannel = new idSoundChannel;
  soundChannel->logicalChannel = channel;
  soundChannel->soundShader = shader;
  soundChannel->startTime = 0;
  soundChannel->volumeFade.SetVolume(parms != nullptr ? parms->volume.min : 0.0F);
  soundChannel->pitchFade.SetVolume(parms != nullptr ? parms->pitch.min : 1.0F);
  if (parms != nullptr) soundChannel->endTime = 0;
  channels.push_back(soundChannel);
  millisecondUnique_t result;
  result.value = soundChannel->startTime;
  return result;
}

void idSoundEmitterLocal::ModifySound(soundChannel_t channel,
                                      const idSoundShader* shader,
                                      const soundShaderParms_t* parms) {
  for (idSoundChannel* soundChannel : channels) {
    if (soundChannel->logicalChannel != channel) continue;
    soundChannel->soundShader = shader;
    if (parms != nullptr) {
      soundChannel->volumeFade.SetVolume(parms->volume.min);
      soundChannel->pitchFade.SetVolume(parms->pitch.min);
    }
  }
}

void idSoundEmitterLocal::StopSound(void* channel) {
  StopSound(static_cast<idSoundChannel*>(channel));
}

void idSoundEmitterLocal::StopSound(idSoundChannel* channel) {
  if (channel == nullptr) return;
  auto iterator = std::find(channels.begin(), channels.end(), channel);
  if (iterator != channels.end()) {
    delete *iterator;
    channels.erase(iterator);
  }
}

void idSoundEmitterLocal::StopSound(soundChannel_t channel) {
  for (auto iterator = channels.begin(); iterator != channels.end();) {
    if ((*iterator)->logicalChannel == channel || channel == SND_CHANNEL_ANY) {
      delete *iterator;
      iterator = channels.erase(iterator);
    } else {
      ++iterator;
    }
  }
}

void idSoundEmitterLocal::SetVolume(soundChannel_t channel, float volume) {
  for (idSoundChannel* soundChannel : channels)
    if (soundChannel->logicalChannel == channel) soundChannel->volumeFade.SetVolume(volume);
}

void idSoundEmitterLocal::SetPitch(soundChannel_t channel, float pitch) {
  for (idSoundChannel* soundChannel : channels)
    if (soundChannel->logicalChannel == channel) soundChannel->pitchFade.SetVolume(pitch);
}

void idSoundEmitterLocal::FadeSound(soundChannel_t channel, float to, float over,
                                     float delay, bool linear) {
  for (idSoundChannel* soundChannel : channels) {
    if (soundChannel->logicalChannel == channel)
      soundChannel->volumeFade.Fade(to, static_cast<int>(over * 1000.0F), 0,
                                    static_cast<int>(delay * 1000.0F), linear);
  }
}

void idSoundEmitterLocal::FadePitch(soundChannel_t channel, float to, float over,
                                     float delay) {
  (void)delay;
  for (idSoundChannel* soundChannel : channels)
    if (soundChannel->logicalChannel == channel)
      soundChannel->pitchFade.Fade(to, static_cast<int>(over * 1000.0F), 0);
}

bool idSoundEmitterLocal::IsCurrentlyPlaying(soundChannel_t channel) {
  return std::any_of(channels.begin(), channels.end(), [channel](const idSoundChannel* value) {
    return channel == SND_CHANNEL_ANY || value->logicalChannel == channel;
  });
}

void idSoundEmitterLocal::OnReloadSound(const idSoundShader* decl) {
  for (idSoundChannel* channel : channels)
    if (channel->soundShader == decl) channel->sample = nullptr;
}

void idSoundEmitterLocal::SetVolumeAdjustment(float adjust) { volumeAdjustment = adjust; }
void idSoundEmitterLocal::ClearVolumeAdjustment() { volumeAdjustment = 0.0F; }
float idSoundEmitterLocal::GetVolumeAdjustment() const { return volumeAdjustment; }

const idSoundShader* idSoundEmitterLocal::GetCurrentSoundShader(
    soundChannel_t channel) const {
  for (const idSoundChannel* value : channels)
    if (value->logicalChannel == channel) return value->soundShader;
  return nullptr;
}

soundChannel_t idSoundEmitterLocal::GetCurrentSoundChannel(
    const idSoundShader* shader) const {
  for (const idSoundChannel* value : channels)
    if (value->soundShader == shader) return value->logicalChannel;
  return SND_CHANNEL_ANY;
}

const idSoundSample* idSoundEmitterLocal::GetCurrentSample(soundChannel_t channel) const {
  for (const idSoundChannel* value : channels)
    if (value->logicalChannel == channel) return value->sample;
  return nullptr;
}

millisecondUnique_t idSoundEmitterLocal::GetCurrentSampleTime(soundChannel_t channel) const {
  for (const idSoundChannel* value : channels)
    if (value->logicalChannel == channel) return value->GetCurrentSampleTime();
  return {};
}

millisecondUnique_t idSoundEmitterLocal::GetRemainingSampleTime(soundChannel_t channel) const {
  millisecondUnique_t result = GetCurrentSampleTime(channel);
  return result;
}

float idSoundEmitterLocal::GetSubtitleVolume(soundChannel_t channel) const {
  return GetCurrentSoundShader(channel) == nullptr ? 0.0F : 1.0F;
}

bool idSoundEmitterLocal::GetDebugInfo(int index_, soundDebugInfo_t* info) const {
  (void)index_;
  (void)info;
  return false;
}

void idSoundEmitterLocal::DrawDebugInfo(idRenderWorld* rw) { (void)rw; }

void idSoundEmitterLocal::KillChannelWithFadeout(idSoundChannel* channel) {
  if (channel != nullptr) channel->volumeFade.Fade(-120.0F, 100, 0);
}

int idSoundEmitterLocal::Index() { return index; }
void idSoundEmitterLocal::SetDebugName(const char* name) { debugName = name; }
const char* idSoundEmitterLocal::GetDebugName() const { return debugName ? debugName : ""; }
const idVec3* idSoundEmitterLocal::GetPosition() const { return &origin; }

int idSoundSample::GetLengthInMsec() const {
  if (format.basic.samplesPerSec == 0) return 0;
  const int samples = std::max(0, lastValidSample - firstValidSample);
  return static_cast<int>((static_cast<std::int64_t>(samples) * 1000) /
                          format.basic.samplesPerSec);
}

