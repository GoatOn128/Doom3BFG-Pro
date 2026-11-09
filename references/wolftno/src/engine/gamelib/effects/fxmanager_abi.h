#pragma once

#include "engine/decls/declfx.h"
#include "engine/gamelib/effects/lensflaremanager.h"
#include "engine/gamelib/effects/ribbonmanager.h"
#include "engine/models/rendermodel.h"
#include "engine/models/skeletalanimation/taginfo.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/random.h"
#include "shared/idlib/text/str.h"

#include <cstddef>

class idDeclRenderParm;
class idGameLibEffects;
class idRenderLight;
class idRenderModelGodRays;
class idRenderModelParticle;
class idRenderWorld;
class idSerializer;
class idSoundEmitter;
class idSoundShader;
class idTreeAnimator;
class idViewCallbacks;

struct fxEmitterSound_t {
  fxEmitterSound_t() : emitter(nullptr), channel(SND_CHANNEL_ANY) {}

  idSoundEmitter* emitter;
  soundChannel_t channel;
  int padding;
};

class idFXAction {
public:
  idFXAction();

  int tagIndex;
  idVec3 startOrg;
  idMat3 startAxis;
  idRenderLight* rLight;
  idRenderModel* rModel;
  idRenderModelParticle* rParticle;
  idRenderModelGodRays* rGodrays;
  int screenPrtHandle;
  idFlareHandle flareHandle;
  decalHandle_t decalHandle;
  idRibbonManager ribbonManager;
  idList<tagData_t, 108> tagData;
  idVec3 lastParticleDropPos;
  idVec4 renderParmStartValue;
  idAngles randomAngles;
  int viewCallbacksID;
  int paddingEC;
};

struct fxActionState_t {
  fxActionState_t() = default;

  int startDelay;
  int startTime;
  int stopTime;
  bool hidden;
  bool started;
  bool shouldTrigger;
  bool forceStop;
  int fadeInStartTime;
  int fadeInEndTime;
  int fadeOutStartTime;
};

class idFXManager {
public:
  enum fxActionCallType_t : int {
    FXACTION_START,
    FXACTION_STOP,
    FXACTION_STOP_ALL,
    FXACTION_MAX
  };

  struct fxActionCall_t {
    fxActionCall_t()
        : org(vec3_origin), axis(mat3_identity), condition(FX_NONE), time(0),
          extraCondition(FX_EXTRA_COND_NONE), tagIdx(-1),
          actionType(FXACTION_START), immediate(false), viewCallbacksID(-1) {}

    idVec3 org;
    idMat3 axis;
    fxCondition_t condition;
    int time;
    fxExtraCondition_t extraCondition;
    int tagIdx;
    fxActionCallType_t actionType;
    bool immediate;
    unsigned char padding45[3];
    int viewCallbacksID;
  };

  idFXManager();
  idFXManager(const idFXManager& other);
  ~idFXManager();
  idFXManager& operator=(const idFXManager& other);

  void Init(const idDeclFX* declaration, idRenderWorld* renderWorld,
            fxEmitterSound_t* soundInfo, idGameLibEffects* effects,
            float diversity, idTreeAnimator* treeAnimator,
            const idStr owner);
  void Shutdown();
  void Update(const idVec3& origin, const idMat3& axis,
              const idVec3& velocity, int currentTime,
              int gameMillisecondsPerFrame, int serverCurrentTime,
              float attenuation, float shakeScale);
  void LocalStartFX(const idVec3& origin, const idMat3& axis, int currentTime,
                    fxCondition_t condition);
  int StartFX(const idVec3& origin, const idMat3& axis, int currentTime,
              fxCondition_t condition, fxExtraCondition_t extraCondition,
              int tagIndex);
  int StartFX(const idVec3& origin, const idMat3& axis, int currentTime,
              fxCondition_t condition, int extraCondition, int tagIndex) {
    return StartFX(origin, axis, currentTime, condition,
                   static_cast<fxExtraCondition_t>(extraCondition), tagIndex);
  }
  int StartFX(const idVec3& origin, const idMat3& axis, int currentTime,
              fxCondition_t condition);
  void LocalStopAllFX(int currentTime, bool instant);
  void StopAllFX(int currentTime, bool instant);
  void LocalStopFX(int currentTime, fxCondition_t condition, bool instant);
  void StopFX(int currentTime, fxCondition_t condition,
              fxExtraCondition_t extraCondition, bool instant);
  void StopFX(int currentTime, fxCondition_t condition, bool instant);
  void Serialize(idSerializer& serializer);
  bool IsInitialized() const { return initialized; }
  idTreeAnimator* GetAnimator() const { return ta; }
  void ResetTreeAnimator(idTreeAnimator* treeAnimator);
  void SetSystemColor(const idVec4& color) { systemColor = color; }
  idVec4 GetSystemColor() const { return systemColor; }
  void SetViewCallbacksID(int id) { viewCallbacksID = id; }
  void SetAllowSurfaceOnlyInViewID(int id) {
    allowSurfaceOnlyInViewID = id;
  }
  void SetSuppressSurfaceOnlyInViewID(int id) {
    suppressSurfaceInViewID = id;
  }
  void SetRemoteFlag(bool value) { remote = value; }
  void SetExternalTransform(const idVec3& position,
                            const idMat3& rotation) {
    externalPosition = position;
    externalRotation = rotation;
    hasExternalPositionAndRotation = true;
  }
  bool IsStopped(int time) const;

  bool initialized;
  unsigned char padding01[7];
  const idDeclFX* fxDecl;
  idGameLibEffects* gameLibEffects;
  idTreeAnimator* ta;
  idRenderWorld* rw;
  idVec4 systemColor;
  idRandom2 random;
  int padding3C;
  fxEmitterSound_t soundInfo;
  idList<idFXAction, 108> actions;
  idList<fxActionState_t, 108> actionState;
  bool remote;
  unsigned char padding81[3];
  int allowSurfaceOnlyInViewID;
  int suppressSurfaceInViewID;
  int viewCallbacksID;
  idStr owner;
  int declChangeId;
  idMat3 externalRotation;
  idVec3 externalPosition;
  bool hasExternalPositionAndRotation;
  unsigned char paddingED[3];
  fxActionCall_t actionBuffer[8];
  int actionBufferPos;
  int serializeActionCount;

  static const idList<idViewCallbacks*, 108> emptyFallBackList;

private:
  void UpdateActions(const idVec3& origin, const idMat3& axis,
                     const idVec3& velocity, int currentTime,
                     int gameMillisecondsPerFrame, int serverCurrentTime,
                     float attenuation, float shakeScale);
  int StartActions(const idVec3& origin, const idMat3& axis, int currentTime,
                   fxCondition_t condition,
                   fxExtraCondition_t extraCondition, int tagIndex);
  void StopActions(int currentTime, fxCondition_t condition,
                   fxExtraCondition_t extraCondition, bool stopAll,
                   bool instant);
  void ApplyFade(const idFXSingleAction& declaration, idFXAction& action,
                 fxActionState_t& state, int currentTime, float distance);
  idVec4 GetTableColor(const idFXSingleAction& declaration,
                       float fraction) const;
  void StopAction(int index, int currentTime, const bool instant,
                  bool triggeredStop);
  void StartAction(int index, const idVec3& origin, const idMat3& axis,
                   int currentTime, int explicitTagIndex);
  void RestartAction(int index, int currentTime);
  void CreateAction(int index, idRenderWorld* renderWorld);
  void FreeActions();
  const idList<idViewCallbacks*, 108>& GetViewCallbacks();
  const char* GetName() const;
  void EnumerateTags(const idList<idAtomicString, 108>& names,
                     idList<tagData_t, 108>& tags);
  bool GetWorldSpaceTagVelocity(const tagData_t& tag,
                                int gameMillisecondsPerFrame,
                                idVec3& velocity);
  void SetFade(float value);
  int StartSound(soundChannel_t channel, const idSoundShader* shader);
  int InternalStartFX(const fxActionCall_t& actionCall);
};

static_assert(sizeof(fxEmitterSound_t) == 0x10,
              "FX emitter sound ABI drift");
static_assert(sizeof(idFXAction) == 0xF0, "FX action ABI drift");
static_assert(offsetof(idFXAction, ribbonManager) == 0x70,
              "FX action ribbon offset drift");
static_assert(offsetof(idFXAction, tagData) == 0xA8,
              "FX action tag-list offset drift");
static_assert(sizeof(fxActionState_t) == 0x1C,
              "FX action state ABI drift");
static_assert(sizeof(idFXManager::fxActionCall_t) == 0x4C,
              "FX buffered call ABI drift");
static_assert(offsetof(idFXManager, actions) == 0x50,
              "FX action-list offset drift");
static_assert(offsetof(idFXManager, owner) == 0x90,
              "FX owner offset drift");
static_assert(offsetof(idFXManager, actionBuffer) == 0xF0,
              "FX buffered call offset drift");
static_assert(offsetof(idFXManager, actionBufferPos) == 0x350,
              "FX buffer position offset drift");
static_assert(sizeof(idFXManager) == 0x358, "FX manager ABI drift");
