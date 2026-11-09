#include "engine/sound/sound_runtime.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <algorithm>

idCVar s_useDistanceLowPass("s_useDistanceLowPass", "1", CVAR_SYSTEM,
                            "Apply distance low-pass filtering");
idCVar s_muteMusic("s_muteMusic", "0", CVAR_SYSTEM,
                   "Mute music channels");

idSoundChannel::idSoundChannel() = default;
idSoundChannel::~idSoundChannel() = default;

bool idSoundChannel::CanMute() const {
  return soundShader == nullptr || (soundShader->parms.soundShaderFlags & SSF_GLOBAL) == 0;
}

bool idSoundChannel::IsLooping() const {
  return soundShader != nullptr &&
         (soundShader->parms.soundShaderFlags & SSF_LOOPING) != 0;
}

bool idSoundChannel::CheckForCompletion(int currentTime) {
  return endTime != 0 && currentTime >= endTime && !IsLooping();
}

void idSoundChannel::SetSilent() {
  silent = true;
  volumeDB = -120.0F;
}

void idSoundChannel::Mute() {
  if (CanMute()) SetSilent();
}

void idSoundChannel::UpdateVolume(int currentTime) {
  volumeDB = volumeFade.GetVolume(currentTime);
  if (silent) volumeDB = -120.0F;
}

millisecondUnique_t idSoundChannel::GetCurrentSampleTime() const {
  millisecondUnique_t result;
  result.value = std::max(0, startTime);
  return result;
}

void idSoundChannel::UpdateHardware(float volumeAdd, int currentTime) {
  UpdateVolume(currentTime);
  volumeDB += volumeAdd;
}

float idSoundWorldLocal::idDuckLayer::GetVolume(int soundTime) const {
  (void)soundTime;
  return 1.0F;
}

