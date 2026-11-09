#pragma once

#include "engine/decls/decl.h"
#include "engine/sound/declsoundmixing.h"
#include "engine/sound/soundchannel.h"
#include "engine/sound/soundsample.h"

#include <cstdint>

class idDeclTable;

enum soundShaderFlags_t : int {
  SSF_PRIVATE_SOUND = 0x1,
  SSF_ANTI_PRIVATE_SOUND = 0x2,
  SSF_NO_OCCLUSION = 0x4,
  SSF_GLOBAL = 0x8,
  SSF_OMNIDIRECTIONAL = 0x10,
  SSF_FRONTDIRECTIONAL = 0x20,
  SSF_REARDIRECTIONAL = 0x40,
  SSF_EXPAND_REAR_FRONT_7_1 = 0x80,
  SSF_WORLDROTATE_DIRECTIONAL = 0x100,
  SSF_ENVIRONMENTAL = 0x200,
  SSF_CONEATTENUATION = 0x400,
  SSF_LOOPING = 0x800,
  SSF_PLAY_ONCE = 0x1000,
  SSF_SEQUENTIAL = 0x2000,
  SSF_NO_RANDOM_OFFSET = 0x4000,
  SSF_AUTO_RESTART = 0x8000,
  SSF_FLICKER = 0x10000,
  SSF_NO_EFFECTS = 0x20000,
  SSF_FINITE_SPEED_OF_SOUND = 0x40000,
  SSF_DISTANCE_LOW_PASS = 0x80000,
  SSF_LFE = 0x100000,
  SSF_DISTANCE_BASED_STEREO = 0x200000,
  SSF_RANDOM_LOOPING = 0x400000,
  SSF_PASS_HDR = 0x800000,
  SSF_BLOCK_ON_LOAD = 0x1000000,
  SSF_SAMPLEPACK = 0x2000000
};

enum soundMaxPlaybackType_t : int {
  SMAX_IGNORE,
  SMAX_REPLACE,
  SMAX_RESTART,
  SMAX_KILLFARTHEST
};
enum soundPriority_t : int {
  SPRIORITY_LOW,
  SPRIORITY_MEDIUM,
  SPRIORITY_HIGH,
  SPRIORITY_ULTRA
};
enum soundShaderGetNextEntryMode_t : int {
  SNDSHD_GET_NEXT_ENTRY_NO_PRECACHE,
  SNDSHD_GET_NEXT_ENTRY_AND_PRECACHE,
  SNDSHD_ONLY_PRECACHE_NEXT_ENTRY
};

struct randomValue_t {
  float min;
  float max;
  float offset;
  float granularity;
};

struct shakeParms_t {
  float intensity;
  float minDistance;
  float maxDistance;
  const idDeclTable* falloff;
  const idDeclTable* timeFalloff;
};

struct soundShaderParms_t {
  struct coneAttenuationVars_t {
    float innerAngle;
    float outerAngle;
    float outerVolume;
  };
  struct distancesBasedVars_t {
    float nearMinRange;
    float farMaxRange;
  };
  struct maxPlayerVars_t {
    int maxShaders;
    soundMaxPlaybackType_t playbackType;
  };

  soundShaderParms_t();
  void Override(const soundShaderParms_t* over);

  float minDistance;
  float maxDistance;
  float distanceFactor;
  float wetMinDistanceOffset;
  float wetMaxDistanceOffset;
  coneAttenuationVars_t coneAttenuation;
  distancesBasedVars_t distanceBasedRanges;
  randomValue_t volume;
  float wetVolume;
  randomValue_t pitch;
  float loopFadeInDuration;
  float loopFadeOutDuration;
  float dopplerMultiplier;
  shakeParms_t cameraShake;
  shakeParms_t controllerShake;
  soundShaderGroups_t groups;
  soundChannel_t soundChannel;
  soundShaderFlags_t soundShaderFlags;
  float lowPassMinDistance;
  float lowPassStrength;
  float lfeVolume;
  float centerChannel;
  const idDeclTable* falloff;
  const idDeclTable* wetFalloff;
  float headroom;
  maxPlayerVars_t maxPlay;
  soundPriority_t priority;
  float frontDirectionalAngle;
};

struct idStrSound : idAtomicString {
  idStrSound() = default;
  explicit idStrSound(const char* value) : idAtomicString(value) {}
};

class idSoundShader final : public idDeclTypeInfo {
public:
  idSoundShader();
  ~idSoundShader() override;

  idDeclInfo* GetDeclInfo() const override;
  bool SetImplicitText() override;
  void Parse(idParser& source) override;
  void FreeData() override;
  void LoadResource() override;
  void List() const override;

  const idSoundSample* SelectTailSample();
  const idSoundSample* GetNextEntry(soundShaderGetNextEntryMode_t mode);
  bool PinCacheData(pinnedCacheLines_t* lines, bool highPriority);
  bool PreCacheData();
  int GetMaxLength() const;
  void ReloadSamples();

  soundShaderParms_t parms;
  idList<idStrSound, 38> soundFiles;
  idList<idStrSound, 38> preSoundFiles;
  idList<idStrSound, 38> tailSoundFiles;
  const idDeclTable* rmsTable;
  const idDeclTable* rmsPreTable;
  const idDeclTable* rmsTailTable;
  idList<const idSoundSample*, 38> entries;
  idList<const idSoundSample*, 38> preEntries;
  idList<const idSoundSample*, 38> tailEntries;
  idSoundSamplePack* samplePack;
  const idSoundShader* preSound;
  const idSoundShader* postSound;
  const idSoundShader* secondSound;
  const idSoundShader* remoteSound;
  const idSoundSample* nextSample;

  static idDeclInfoTemplate<idSoundShader> resourceList;
};

class idDeclDuck final : public idDecl {
public:
  idDeclDuck();
  idDeclInfo* GetDeclInfo() const override;
  const char* DefaultDefinition() const override;
  bool RebuildTextSource() override;
  void Parse(idParser& source) override;

  int layer;
  float groupVolume[30];
  static idDeclInfoTemplate<idDeclDuck> resourceList;
};

extern std::uint32_t soundRandomSeed;

static_assert(sizeof(randomValue_t) == 16);
static_assert(sizeof(shakeParms_t) == 32);
static_assert(sizeof(soundShaderParms_t) == 224, "sound shader parms ABI drift");
static_assert(sizeof(idStrSound) == 8);
static_assert(sizeof(idSoundShader) == 536, "idSoundShader ABI drift");
static_assert(sizeof(idDeclDuck) == 208, "idDeclDuck ABI drift");
