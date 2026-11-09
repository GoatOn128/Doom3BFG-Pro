#pragma once

#include "engine/sound/streamcache.h"
#include "shared/idlib/text/str.h"

#include <cstdint>

class idSoundShader;

// Injection boundary used by the voice-track declarations so they can preload
// sound data without depending on the concrete sound-system singleton.
class idVoiceSoundPreload {
public:
  virtual ~idVoiceSoundPreload() = default;
  virtual const idSoundShader* ResolveSoundShader(const char* name) const {
    (void)name;
    return nullptr;
  }
  virtual const char* GetShaderName(const idSoundShader* shader) const {
    (void)shader;
    return "";
  }
  virtual const char* GetLanguage() const { return "english"; }
  virtual bool ShouldLocalizeVoiceTracks() const { return false; }
  virtual idStr GetLocalizedVoiceTrackName(const char* resourceName) const {
    return idStr(resourceName ? resourceName : "");
  }
  virtual bool IsProduction() const { return false; }
  virtual bool IsSoundDisabled() const { return false; }
  virtual int GetMaxLength(const idSoundShader* shader) const = 0;
  virtual int GetExactLength(const idSoundShader* shader) const = 0;
  virtual std::uint32_t Pin(const idSoundShader* shader, pinnedCacheLines_t*& lines) = 0;
  virtual void Unpin(pinnedCacheLines_t* lines, std::uint32_t count) = 0;
};

inline idVoiceSoundPreload*& VoiceSoundPreloadService() {
  static idVoiceSoundPreload* service = nullptr;
  return service;
}
