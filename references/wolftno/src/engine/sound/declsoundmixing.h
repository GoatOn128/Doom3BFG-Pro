#pragma once

#include "engine/decls/decl.h"
#include "engine/sound/dsp/dynamicrangecompressor.h"
#include "engine/sound/dsp/dynamicrangelimiter.h"

enum soundShaderGroups_t : int {
  SSG_MASTER = 0,
  SSG_UI = 1,
  SSG_MUSIC_MENU = 2,
  SSG_MUSIC_INGAME = 4,
  SSG_SFX_GENERAL = 8,
  SSG_AMB_CORE = 0x10,
  SSG_AMB_SPOT = 0x20,
  SSG_EXPLOSIONS = 0x40,
  SSG_KEY_EVENTS = 0x80,
  SSG_IMPACTS = 0x100,
  SSG_RICO_BYS = 0x200,
  SSG_MOVEMENT_AI = 0x400,
  SSG_MOVEMENT_PL = 0x800,
  SSG_VEH_AI = 0x1000,
  SSG_VEH_PL = 0x2000,
  SSG_VEH_COLLISIONS_AI = 0x4000,
  SSG_VEH_COLLISIONS_PL = 0x8000,
  SSG_WEAPONS_AI = 0x10000,
  SSG_WEAPONS_PL = 0x20000,
  SSG_SPECIAL_ENEMY_MOVEMENT = 0x40000,
  SSG_SPECIAL_ENEMY_VOICE = 0x80000,
  SSG_VO_PLAYER = 0x100000,
  SSG_VO_EVENT = 0x200000,
  SSG_VO_INGAME = 0x400000,
  SSG_VO_RADIO = 0x800000,
  SSG_VO_BATTLE = 0x1000000,
  SSG_MIX_GROUP_1 = 0x2000000,
  SSG_MIX_GROUP_2 = 0x4000000,
  SSG_MIX_GROUP_3 = 0x8000000,
  SSG_MIX_GROUP_4 = 0x10000000,
  SSG_MIX_GROUP_5 = 0x20000000
};

enum soundShaderBus_t : int {
  SSB_NONE = -1,
  SSB_BUS0 = 0,
  SSB_BUS1,
  SSB_BUS2,
  SSB_BUS3,
  SSB_BUS4,
  SSB_BUS5,
  SSB_BUS6,
  SSB_BUS7,
  SSB_BUS8,
  SSB_BUS9,
  SSB_BUS10,
  SSB_BUS11,
  SSB_BUS12,
  SSB_MASTER
};

enum soundFilterType_t : int {
  SND_FILTERTYPE_NONE,
  SND_FILTERTYPE_LOWPASS,
  SND_FILTERTYPE_HIGHPASS
};

struct soundCompressorSettings_t {
  bool enabled{false};
  bool sideChain{false};
  soundShaderBus_t sideChainSourceBus{SSB_NONE};
  dynamicRangeCompressorParms_t parms;
};

struct soundLimiterSettings_t {
  bool enabled{false};
  dynamicRangeLimiterParms_t parms;
};

struct soundMasterBusSettings_t {
  soundCompressorSettings_t compressor;
  soundLimiterSettings_t limiter;
};

struct soundFilter_t {
  soundFilterType_t type{SND_FILTERTYPE_NONE};
  int frequency{1000};
};

struct alignas(8) soundSubBusSettings_t {
  soundSubBusSettings_t();

  idStr name;
  soundCompressorSettings_t compressor;
  soundLimiterSettings_t limiter;
  soundFilter_t filter;
  soundShaderBus_t output{SSB_MASTER};
  float gain{0.0F};
  float wetGain{0.0F};
};

struct soundGroupSettings_t {
  soundShaderBus_t bus{SSB_MASTER};
};

class alignas(8) idDeclSoundMixing : public idDeclTypeInfo {
public:
  struct groups_t {
    groups_t();

    soundGroupSettings_t master, ui, musicMenu, musicIngame, sfxGeneral;
    soundGroupSettings_t ambCore, ambSpot, explosions, keyEvents, impacts, ricoBys;
    soundGroupSettings_t movementAI, movementPlayer, vehicleAI, vehiclePlayer;
    soundGroupSettings_t vehicleCollisionsAI, vehicleCollisionsPlayer;
    soundGroupSettings_t weaponsAI, weaponsPlayer;
    soundGroupSettings_t specialEnemyMovement, specialEnemyVoice;
    soundGroupSettings_t voPlayer, voEvent, voIngame, voRadio, voBattle;
    soundGroupSettings_t voMixGroup1, voMixGroup2, voMixGroup3, voMixGroup4,
        voMixGroup5;
  };

  idDeclSoundMixing();
  idDeclInfo* GetDeclInfo() const override;
  bool SetImplicitText() override;
  soundShaderBus_t GetGroupRouting(soundShaderGroups_t group) const;

  soundMasterBusSettings_t masterBus;
  soundSubBusSettings_t subBus[13];
  groups_t groups;

  static idDeclInfoTemplate<idDeclSoundMixing> resourceList;
  static idDeclSoundMixing* defaultSoundMixing;
};

static_assert(sizeof(soundCompressorSettings_t) == 36);
static_assert(sizeof(soundLimiterSettings_t) == 12);
static_assert(sizeof(soundMasterBusSettings_t) == 48);
static_assert(sizeof(soundSubBusSettings_t) == 112);
static_assert(sizeof(idDeclSoundMixing::groups_t) == 124);
static_assert(sizeof(idDeclSoundMixing) == 1728);
