#pragma once

#include "engine/sound/soundshader.h"
#include "shared/idlib/math/vector.h"

#ifndef ID_MILLISECOND_UNIQUE_T_DEFINED
#define ID_MILLISECOND_UNIQUE_T_DEFINED
struct millisecondUnique_t {
  int value{0};
};
#endif

class idSoundEmitter {
public:
  virtual int Index() = 0;
  virtual void Free(bool instant) = 0;
  virtual void Reset() = 0;
  virtual void UpdateEmitter(const idVec3& origin, const idMat3& axis,
                             const idVec3& velocity, int listenerId,
                             bool virtualize) = 0;
  virtual millisecondUnique_t StartSound(soundChannel_t channel,
                                         const idSoundShader* shader,
                                         const soundShaderParms_t* parms) = 0;
  virtual void ModifySound(soundChannel_t, const idSoundShader*,
                           const soundShaderParms_t*) = 0;
  virtual void StopSound(void*) = 0;
  virtual void StopSound(soundChannel_t channel) = 0;
  virtual void SetVolume(soundChannel_t, float) = 0;
  virtual void SetPitch(soundChannel_t, float) = 0;
  virtual void FadeSound(soundChannel_t, float, float, float, bool) = 0;
  virtual void FadePitch(soundChannel_t, float, float, float) = 0;
  virtual bool IsCurrentlyPlaying(soundChannel_t channel) = 0;
  virtual ~idSoundEmitter();
};

#if !defined(WOLFTNO_ID_SOUND_WORLD_DEFINED)
class idSoundWorld {
public:
  virtual ~idSoundWorld();
  virtual void StopAllSounds() = 0;
  virtual idSoundEmitter* AllocSoundEmitter() = 0;
};
#endif

class idSoundSystem {
public:
  virtual ~idSoundSystem();
  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  virtual idSoundWorld* AllocSoundWorld(const char*, void*) = 0;
  virtual void FreeSoundWorld(idSoundWorld*) = 0;
  virtual void SetPlayingSoundWorld(idSoundWorld*) = 0;
  virtual void SetMixingDecl(const idDeclSoundMixing*) = 0;
  virtual const idDeclSoundMixing* GetMixingDecl() = 0;
  virtual idSoundWorld* GetPlayingSoundWorld() = 0;
  virtual void SetEditorSoundWorld(idSoundWorld*) = 0;
  virtual idSoundWorld* GetEditorSoundWorld() = 0;
  virtual idSoundWorld* GetGlobalSoundWorld() = 0;
  virtual void Render(bool) = 0;
  virtual void RenderDebugPage(class idRenderModelGui*) {}
  virtual void WaitForSoundThread() = 0;
  virtual void MuteBackgroundMusic(bool) {}
  virtual void SetMute(bool) {}
  virtual void SoundSystemSlot17() {}
  virtual void SoundSystemSlot18() {}
  virtual void StopAllSounds() = 0;
  virtual void* GetIXAudio2() = 0;
  virtual void SoundSystemSlot21() {}
  virtual int GetSpeakerMask() const = 0;

  std::int64_t GetSampleClock() const {
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    return reinterpret_cast<std::int64_t(*)(const idSoundSystem*)>(slots[23])(this);
  }
  int GetSampleRate() const {
    const auto slots=*reinterpret_cast<const void* const* const*>(this);
    return reinterpret_cast<int(*)(const idSoundSystem*)>(slots[24])(this);
  }
};

extern idSoundSystem* soundSystem;
