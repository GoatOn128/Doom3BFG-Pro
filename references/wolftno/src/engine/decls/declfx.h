#pragma once

#include "decl.h"
#include "declflare.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/sound/soundchannel.h"
#include "shared/idlib/color.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/math/vector.h"

class idDeclEnv;
class idDeclParticle;
class idDeclRibbon;
class idDeclTable;
class idMaterial;
class idSoundShader;

enum fxActionType_t : int {
  FX_LIGHT,
  FX_PARTICLE,
  FX_DECAL,
  FX_DECAL2,
  FX_MODEL,
  FX_SOUND,
  FX_SCREEN_SHAKE,
  FX_CONTROLLER_SHAKE,
  FX_WIND,
  FX_RENDERPARM,
  FX_ENV_OVERRIDE,
  FX_ENV_CHANGE,
  FX_FLARE,
  FX_RADIAL_BLUR,
  FX_RIBBON,
  FX_FADE_PARENT,
  FX_GODRAY,
  FX_MAX
};

enum fxMultiTagUseType_t : int {
  FX_MULTI_TAG_USE_RND,
  FX_MULTI_TAG_USE_EXPLICIT,
  FX_MULTI_TAG_USE_ALL
};

enum fxRotationType_t : int {
  FX_ROT_START_AXIS,
  FX_ROT_START_AXIS_PARENT,
  FX_ROT_TRACK_AXIS,
  FX_ROT_TRACK_AXIS_PARENT,
  FX_ROT_TRACK_LOCAL_AXIS,
  FX_ROT_EXPLICIT_ANGLES,
  FX_ROT_EXPLICIT_TABLES,
  FX_ROT_EXPLICIT_TABLES_LOCAL,
  FX_ROT_EXTERNALROT
};

enum fxOriginType_t : int {
  FX_ORG_START_POS,
  FX_ORG_TRACK_POS,
  FX_ORG_TRACK_LOCAL_POS,
  FX_ORG_EXTERNALPOS
};

// The full condition enum is game-wide and is recovered with gamelib.  DeclFX
// stores its ABI as a 32-bit value and only requires FX_NONE locally.
enum fxCondition_t : int { FX_NONE = 0 };

enum fxExtraCondition_t : int {
  FX_EXTRA_COND_NONE = 0,
  FX_EXTRA_COND_WEAP_AMMOTYPE_0 = 0x4,
  FX_EXTRA_COND_WEAP_AMMOTYPE_1 = 0x8,
  FX_EXTRA_COND_WEAP_AMMOTYPE_2 = 0x10,
  FX_EXTRA_COND_WEAP_AMMOTYPE_3 = 0x20,
  FX_EXTRA_COND_WEAP_ATTACK_DEFAULT = 0x40,
  FX_EXTRA_COND_WEAP_ATTACK_LEFT = 0x80,
  FX_EXTRA_COND_WEAP_ATTACK_RIGHT = 0x100,
  FX_EXTRA_COND_WEAP_ATTACK_LEFT2 = 0x200,
  FX_EXTRA_COND_WEAP_ATTACK_RIGHT2 = 0x400,
  FX_EXTRA_COND_SMALL = 0x800,
  FX_EXTRA_COND_MEDIUM = 0x1000,
  FX_EXTRA_COND_LARGE = 0x2000,
  FX_EXTRA_COND_ENGINE_1 = 0x4000,
  FX_EXTRA_COND_ENGINE_2 = 0x8000,
  FX_EXTRA_COND_UNDERWATER = 0x10000,
  FX_EXTRA_COND_ANY = 0x20000,
  FX_EXTRA_COND_MAX = 0x40000
};

struct idFXRenderParm {
  const idDeclRenderParm* parm;
  idVec4 value;
};

class idFXSingleAction {
public:
  struct fxLightParms_t {
    idVec3 radius;
    float intensity;
    const idMaterial* lightMtr;
  };
  struct fxGodrayParms_t {
    const idMaterial* godrayMaterial;
    idColor color;
    float colorScale;
    int size;
    int sourceSize;
  };
  struct fxParticleParms_t {
    const idDeclParticle* declPrt;
    float velocityScale;
    float minVelocity;
    int smokeSystemRate;
    float trailSpacing;
    float screenPrtExcludeAngle;
    bool trackVelocity;
    bool useSmokeSystem;
    bool isScreenPrt;
  };
  struct fxDecalParms_t {
    float angle;
    float depth;
    const idMaterial* decalMtr;
  };
  struct fxModelParms_t {
    idAtomicString modelName;
    const idMaterial* customMaterial;
  };
  struct fxSoundParms_t {
    soundChannel_t channel;
    const idSoundShader* sound;
  };
  struct fxScreenShakeParms_t {
    float magnitude;
    float controllerShakeScale;
    idAngles maxAngles;
    idVec3 maxOffset;
  };
  struct fxControllerShakeParms_t {
    float highMag;
    float lowMag;
    float highDuration;
    float lowDuration;
    const idDeclTable* ampTable;
  };
  struct fxWindParms_t {
    float angle;
    float multiplier;
    idVec2 strength;
  };
  struct fxRenderParmParms_t { const idDeclRenderParm* declRenderParm; };
  struct fxEnvParms_t {
    const idDeclEnv* declEnv;
    idStaticList<idFXRenderParm, 16> envRenderParm;
  };
  struct fxRadialBlurParms_t { float maxScale; };
  struct fxFlareParms_t {
    idVec3 position;
    const idDeclFlare* declFlare;
    bool isAutosprited;
  };
  struct fxRibbonParms_t { const idDeclRibbon* declRibbon; };

  idFXSingleAction();
  void Default();

  idAtomicString name;
  idAtomicString group;
  bool filterHighViolence;
  fxActionType_t type;
  float duration;
  idVec2 delay;
  bool restart;
  bool looping;
  float fadeInTime;
  float fadeOutTime;
  float fadeStartDistance;
  float distance;
  float size;
  idVec4 color;
  bool noshadows;
  idAtomicString fire;
  fxMultiTagUseType_t multiTagUseType;
  idList<idAtomicString, 108> tagNames;
  idList<fxCondition_t, 108> startCondition;
  idList<fxCondition_t, 108> stopCondition;
  idList<fxExtraCondition_t, 108> extraCondition;
  fxOriginType_t originType;
  idVec3 offset;
  fxRotationType_t rotationType;
  idAngles rotOffsetAngles;
  idVec2 rndRotX;
  idVec2 rndRotY;
  idVec2 rndRotZ;
  idAngles explicitAngles;
  const idDeclTable* rotateTablePitch;
  const idDeclTable* rotateTableYaw;
  const idDeclTable* rotateTableRoll;
  const idDeclTable* colorTableRGB;
  const idDeclTable* colorTableA;
  const idDeclTable* customTable1;
  const idDeclTable* customTable2;
  const idDeclRenderParm* customRenderParm;
  fxLightParms_t lightParms;
  fxGodrayParms_t godrayParms;
  fxParticleParms_t particleParms;
  fxDecalParms_t decalParms;
  fxModelParms_t modelParms;
  fxSoundParms_t soundParms;
  fxScreenShakeParms_t screenShakeParms;
  fxControllerShakeParms_t controllerShakeParms;
  fxWindParms_t windParms;
  fxRenderParmParms_t renderParmParms;
  fxEnvParms_t envParms;
  fxRadialBlurParms_t radialBlurParms;
  fxFlareParms_t flareParms;
  fxRibbonParms_t ribbonParms;
  idMat3 axis;
  idMat3 rotOffset;
  bool implicit;
  bool triggered;
};

class idDeclFX final : public idDeclTypeInfo {
public:
  idDeclFX();
  ~idDeclFX() override;

  void FreeData() override;
  void Parse(idParser& source) override;
  idDeclInfo* GetDeclInfo() const override;

  int GetMaxDuration(fxCondition_t startCondition = FX_NONE) const;
  int GetMaxFadeOutTime(fxCondition_t startCondition = FX_NONE) const;
  static const char* FXActionTypeToName(fxActionType_t type);
  static const char* FXActionTypeToName(int type);
  static const char* FXConditionToName(fxCondition_t condition);
  static const char* FXConditionToName(int condition);

  idList<idFXSingleAction, 108> editEvents;
  idList<idFXSingleAction, 108> events;
  int changeId;

  static idDeclInfoTemplate<idDeclFX> resourceList;
};

static_assert(sizeof(idFXRenderParm) == 24, "idFXRenderParm ABI drift");
static_assert(sizeof(idFXSingleAction) == 1096, "idFXSingleAction ABI drift");
static_assert(sizeof(idDeclFX) == 152, "idDeclFX ABI drift");
