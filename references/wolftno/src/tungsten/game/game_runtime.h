#pragma once

#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/containers/list.h"
#include "engine/models/renderworld.h"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class idAAS2;
class idAASVisCache;
class idAIGameState;
class idEncounterGroupManager;
class idDeclEntityDef;
class idDeclDamage;
class idDeclFlare;
class idFaction;
class idAI2;
class idAIVoiceController;
class idTarget_AITensionManager;
class idEventDef;
class idEntity;
class idFXManager;
class idPlayer;
class idWaypoint;
enum aimPoint_t : int;
enum usableState_t : int;
class idFocusTrace {};
class idCrosshairInfo;
class idClipModel;
class idRenderModel;
struct trace_t;

enum aiHoldFireReason_t : int {
  AI_HOLDFIRE_REASON_NONE = 0,
  AI_HOLDFIRE_REASON_FRIENDLY = 1,
  AI_HOLDFIRE_REASON_RANGE = 2,
  AI_HOLDFIRE_REASON_NOREGAIN = 3,
  AI_HOLDFIRE_REASON_SPOTLOST = 4,
  AI_HOLDFIRE_REASON_NOTRACK = 5,
};

struct idPostedEvent {
  const idEventDef* eventDefinition{nullptr};
  int delayMilliseconds{0};
  const idEntity* entityArgument{nullptr};
};

struct idStoppedSound {
  int channel{0};
  unsigned int peerMask{0};
};

struct soundEnvironment_t {
  float DryGain{0.0F};
  float RoomGain{0.0F};
  float RoomHFGain{0.0F};
  int DecayTime{1000};
  int DecayHFTime{1000};
  float ReflectionsGain{0.0F};
  int ReflectionsDelay{0};
  float ReverbGain{0.0F};
  int ReverbDelay{0};
  float Diffusion{1.0F};
  float Density{1.0F};
  float HFReference{5000.0F};
};

struct inputSettings_t {
  int bindset{0};
  float mouseSensScale{1.0F};
  float joySensScale{1.0F};
  float joyPitchSpeed{0.0F};
  float joyYawSpeed{0.0F};
  float mouseSens{0.0F};
  bool absoluteAngles{false};
  bool lookInvert{false};
  idAngles aimAssistAngles{};
};
static_assert(sizeof(inputSettings_t) == 40,
              "inputSettings_t ABI drift");

// Minimal shared game-side contracts used while the recovered Tungsten game
// translation units are brought back online.  These interfaces model the
// retail entity/physics interactions without coupling the game port to the
// Doom 3 BFG reference tree.

class idPhysics {
public:
  struct contact_t {
    int entityNumber{-1};
    int bodyId{0};
    idVec3 normal{};
  };

  virtual ~idPhysics() = default;

  virtual const idBounds& GetAbsBounds() const = 0;
  virtual const idVec3& GetOrigin() const = 0;
  virtual const idMat3& GetAxis() const {
    static const idMat3 identity = [] {
      idMat3 value{};
      value[0] = {1.0F, 0.0F, 0.0F};
      value[1] = {0.0F, 1.0F, 0.0F};
      value[2] = {0.0F, 0.0F, 1.0F};
      return value;
    }();
    return identity;
  }
  virtual const idVec3& GetGravityNormal() const {
    static const idVec3 down{0.0F, 0.0F, -1.0F};
    return down;
  }
  virtual void SetAxis(const idMat3& axis) { (void)axis; }
  virtual void SetOrigin(const idVec3& origin, int bodyId = 0) {
    (void)origin;
    (void)bodyId;
  }
  virtual std::uint32_t GetContents() const = 0;
  virtual void SetContents(std::uint32_t contents) = 0;
  virtual void SetContents(std::uint32_t contents, int bodyId) {
    if (bodyId == 0) SetContents(contents);
  }
  virtual void EnableClip() = 0;
  virtual void DisableClip() = 0;
  virtual void LinkClip() { EnableClip(); }
  virtual void UnlinkClip() { DisableClip(); }
  virtual int GetNumClipModels() const { return 1; }
  virtual int GetClipMask(int bodyId = -1) const {
    (void)bodyId;
    return 0;
  }
  virtual void SetClipMask(int mask, int bodyId = 0) {
    (void)mask;
    (void)bodyId;
  }
  virtual void ApplyImpulse(int bodyId, const idVec3& point,
                            const idVec3& impulse) {
    (void)bodyId;
    (void)point;
    (void)impulse;
  }
  virtual void ApplyForce(int bodyId, const idVec3& point,
                          const idVec3& force) {
    (void)bodyId;
    (void)point;
    (void)force;
  }
  virtual idVec3 GetLinearVelocity(int bodyId = 0) const {
    (void)bodyId;
    return {};
  }
  virtual void SetLinearVelocity(const idVec3& velocity, int bodyId = 0) {
    (void)velocity;
    (void)bodyId;
  }
  virtual void SetAngularVelocity(const idVec3& velocity, int bodyId = 0) {
    (void)velocity;
    (void)bodyId;
  }
  virtual bool IsAIInjuryEligible() const { return false; }
  virtual bool ShouldEnterDormancyImmediately() const { return false; }
  virtual int GetBoundaryType() const { return 1; }
  virtual const idBounds& GetBounds(int bodyId = -1) const {
    (void)bodyId;
    return GetAbsBounds();
  }
  virtual int GetEntityNumber() const { return -1; }
  virtual int GetPhysicsType() const { return 0; }
  virtual int GetNumContacts() const { return 0; }
  virtual bool GetContact(int index, contact_t& contact) const {
    (void)index;
    (void)contact;
    return false;
  }
  virtual bool IsGroundNormal(const idVec3& normal) const {
    const idVec3 up = GetGravityNormal() * -1.0F;
    return normal * up > 0.7F;
  }
  virtual idVec3 GetPushedLinearVelocity(int bodyId = 0) const {
    (void)bodyId;
    return {};
  }
  virtual idVec3 GetOriginHistory(int millisecondsBack) const {
    (void)millisecondsBack;
    return GetOrigin();
  }
};

class idEntity {
public:
  virtual ~idEntity() = default;

  virtual void PostSpawn() {}
  virtual void Remove() { removed = true; }
  virtual void OnHide(bool stopFx) {
    (void)stopFx;
    hidden = true;
  }
  virtual void OnShow() { hidden = false; }
  virtual void OnActivate(idEntity* activator) { (void)activator; }
  void UpdateVisuals() { ++updateVisualsCount; }
  void BecomeActive(int flags) {
    lastBecomeActiveFlags = flags;
    ++becomeActiveCount;
  }
  void BecomeInactive(int flags) {
    lastBecomeInactiveFlags = flags;
    ++becomeInactiveCount;
  }
  int GetBecomeInactiveCountForTest() const { return becomeInactiveCount; }
  int GetLastBecomeInactiveFlagsForTest() const {
    return lastBecomeInactiveFlags;
  }
  int GetUpdateVisualsCountForTest() const { return updateVisualsCount; }
  int GetBecomeActiveCountForTest() const { return becomeActiveCount; }
  int GetLastBecomeActiveFlagsForTest() const {
    return lastBecomeActiveFlags;
  }
  virtual void GetColor(idVec4& color) const;
  void SetColorForTest(const idVec4& color) { entityColor = color; }
  virtual void KilledNotification(idEntity* victim, idEntity* inflictor,
                                  const idDeclDamage* damageDecl,
                                  float damage = 0.0F,
                                  bool killedByDualWield = false,
                                  bool coverKill = false) {
    (void)victim;
    (void)inflictor;
    (void)damageDecl;
    (void)damage;
    (void)killedByDualWield;
    (void)coverKill;
  }
  virtual void PostEventMS(const idEventDef* eventDefinition,
                           int delayMilliseconds) {
    postedEvents.Append({eventDefinition, delayMilliseconds});
  }
  virtual void PostEventMS(const idEventDef* eventDefinition,
                           int delayMilliseconds,
                           const idEntity* entityArgument) {
    postedEvents.Append(
        {eventDefinition, delayMilliseconds, entityArgument});
  }
  virtual const void* GetAIGroupIdentity() const { return nullptr; }
  virtual bool ProvidesDynamicCover() const { return false; }
  virtual bool IsWeaponStaticProp() const { return false; }
  virtual idFaction* GetFaction() const { return nullptr; }
  virtual bool IsDead() const { return false; }
  virtual float Damage(idEntity* inflictor, idEntity* attacker,
                       const idDeclDamage* damageDecl, float damageScale,
                       const idVec3& direction, trace_t* trace) {
    (void)inflictor;
    (void)attacker;
    (void)damageDecl;
    (void)damageScale;
    (void)direction;
    (void)trace;
    return 0.0F;
  }
  virtual void GetAimPoint(aimPoint_t point, idVec3* result) const {
    (void)point;
    *result = physics != nullptr ? physics->GetOrigin() : idVec3{};
  }
  virtual idVec3 GetLinearVelocity() const { return {}; }
  virtual void ApplyImpulse(int physicsId, int bodyId,
                            const idVec3& point,
                            const idVec3& impulse) {
    (void)physicsId;
    if (physics != nullptr) physics->ApplyImpulse(bodyId, point, impulse);
  }
  virtual void ApplyForce(int physicsId, int bodyId,
                          const idVec3& point,
                          const idVec3& force) {
    (void)physicsId;
    if (physics != nullptr) physics->ApplyForce(bodyId, point, force);
  }
  virtual void GetModelTransform(idVec3& origin, idMat3& axis) const {
    if (physics != nullptr) {
      origin = physics->GetOrigin();
      axis = physics->GetAxis();
    } else {
      origin = {};
      axis = {};
    }
  }
  virtual bool ShouldLeaveDormancy() { return leaveDormancy; }
  virtual void GetSoundTransform(idVec3& origin, idMat3& axis) const {
    if (physics != nullptr) {
      origin = physics->GetOrigin();
      axis = physics->GetAxis();
    } else {
      origin = {};
      axis = {};
    }
  }
  virtual bool GetStaticWeaponViewForAI(const idPlayer* player,
                                        idVec3& origin,
                                        idMat3& axis) const {
    (void)player;
    (void)origin;
    (void)axis;
    return false;
  }
  virtual bool ShouldEnterDormancy() const { return hidden; }
  virtual void Unbind() { ++unbindCount; }
  virtual void StopSound(int channel, unsigned int peerMask) {
    stoppedSounds.Append({channel, peerMask});
  }
  virtual void SetOrigin(const idVec3& origin) {
    if (physics != nullptr) physics->SetOrigin(origin, 0);
  }
  virtual void SetAxis(const idMat3& axis) {
    if (physics != nullptr) physics->SetAxis(axis);
  }
  virtual void BindToJoint(idEntity* master, short jointIndex,
                           bool orientated) {
    bindMaster = master;
    bindJoint = jointIndex;
    bindOrientated = orientated;
    ++bindCount;
  }
  virtual bool IsMoveableProp() const { return false; }
  virtual void SetWhoDroppedMe(idEntity* entity) { whoDroppedMe = entity; }
  virtual void DropToFloor(bool activate) {
    lastDropToFloorActivate = activate;
    ++dropToFloorCount;
  }
  virtual void SetDeathSpawnedFlag() { entityFlags |= 4U; }
  virtual bool ModifyCrosshairInfo(const idEntity* activator,
                                   const idFocusTrace* focusTrace,
                                   usableState_t usable,
                                   idCrosshairInfo* info);
  virtual bool IsCrosshairSubdued(const idEntity* activator,
                                  const idFocusTrace& focusTrace,
                                  usableState_t usable) const {
    (void)activator;
    (void)focusTrace;
    (void)usable;
    return false;
  }

  idMat3* GetPresentableAxis();
  const idMat3* GetPresentableAxis() const { return &presentableAxis; }
  void SetPresentableAxis(const idMat3& value) { presentableAxis = value; }
  const char* GetEntityDefName();
  const char* PeekEntityDefName() const { return entityDefName.c_str(); }
  const idVec3* GetPresentableLeftDir();
  const idVec3* GetPresentableUpDir();
  idEntity* GetTarget(int index);
  void AddTarget(idEntity* target) { targets.Append(target); }
  void ClearTargets() { targets.Clear(); }

  idPhysics* GetPhysics() const { return physics; }
  void SetPhysics(idPhysics* value) { physics = value; }
  idFXManager* GetFXManagerForAI();

  const char* GetName() const { return name.c_str(); }
  void SetName(const char* value) { name = value != nullptr ? value : ""; }
  void SetEntityDefName(const char* value) {
    entityDefName = value != nullptr ? value : "";
  }
  int GetEntityNumber() const { return entityNumber; }
  void SetEntityNumber(int value) { entityNumber = value; }

  bool IsHidden() const { return hidden; }
  void SetHiddenForTest(bool value) { hidden = value; }
  bool IsRemoved() const { return removed; }
  void SetShouldLeaveDormancy(bool value) { leaveDormancy = value; }
  int GetPostedEventCount() const { return postedEvents.Num(); }
  const idPostedEvent& GetPostedEvent(int index) const {
    return postedEvents[index];
  }
  void ClearPostedEvents() { postedEvents.Clear(); }
  int GetUnbindCountForTest() const { return unbindCount; }
  int GetStoppedSoundCountForTest() const { return stoppedSounds.Num(); }
  int GetStoppedSoundChannelForTest(int index) const {
    return stoppedSounds[index].channel;
  }
  unsigned int GetStoppedSoundPeerMaskForTest(int index) const {
    return stoppedSounds[index].peerMask;
  }
  int GetBindCountForTest() const { return bindCount; }
  idEntity* GetBindMasterForTest() const { return bindMaster; }
  short GetBindJointForTest() const { return bindJoint; }
  bool GetBindOrientatedForTest() const { return bindOrientated; }
  idEntity* GetWhoDroppedMeForTest() const { return whoDroppedMe; }
  int GetDropToFloorCountForTest() const { return dropToFloorCount; }
  bool GetLastDropToFloorActivateForTest() const {
    return lastDropToFloorActivate;
  }
  unsigned int GetEntityFlagsForTest() const { return entityFlags; }
  void ConfigureModifyCrosshairInfoResultForTest(bool value);
  int GetModifyCrosshairInfoCountForTest() const;
  const idEntity* GetLastCrosshairActivatorForTest() const;
  const idFocusTrace* GetLastCrosshairFocusTraceForTest() const;
  int GetLastCrosshairUsableForTest() const;
  idCrosshairInfo* GetLastCrosshairInfoForTest() const;

protected:
  idPhysics* physics{nullptr};
  std::string name;
  std::string entityDefName;
  bool hidden{false};
  bool removed{false};
  idMat3 presentableAxis{};
  int entityNumber{-1};
  bool leaveDormancy{false};
  idList<idEntity*, 5> targets;
  idList<idPostedEvent, 5> postedEvents;
  int unbindCount{0};
  idList<idStoppedSound, 5> stoppedSounds;
  idEntity* bindMaster{nullptr};
  short bindJoint{-1};
  bool bindOrientated{false};
  int bindCount{0};
  idEntity* whoDroppedMe{nullptr};
  int dropToFloorCount{0};
  bool lastDropToFloorActivate{false};
  unsigned int entityFlags{0};
  idVec4 entityColor{1.0F, 1.0F, 1.0F, 1.0F};
  int becomeInactiveCount{0};
  int lastBecomeInactiveFlags{0};
  int updateVisualsCount{0};
  int becomeActiveCount{0};
  int lastBecomeActiveFlags{0};
  std::shared_ptr<idFXManager> aiFXManager;
};

class idSoundWorld {
public:
  virtual ~idSoundWorld() = default;
  virtual bool Trace(const idVec3& from, const idVec3& to) = 0;
  virtual void OverrideEnvironmentParams(
      const soundEnvironment_t* parameters, int listenerId) {
    (void)parameters;
    (void)listenerId;
  }
};

class idMover : public idEntity {
public:
  virtual bool IsPusher(int bodyId) const {
    (void)bodyId;
    return false;
  }
};

class idGameLocal {
public:
  virtual ~idGameLocal() = default;

  virtual void ChangeAreaTravelFlags(const idBounds& bounds,
                                     std::uint32_t areaContents,
                                     std::uint32_t travelFlags,
                                     bool enabled,
                                     const char* debugName) = 0;
  virtual int GetFrameNum() const { return 0; }
  virtual bool IsMultiplayer() const { return false; }
  virtual int GetAlarmTimestamp() const { return -1; }
  virtual std::uint32_t GetRandomSeed() const { return 0; }
  virtual void SetRandomSeed(std::uint32_t seed) { (void)seed; }
  virtual int GetGameHz(int gameTimeType) const {
    (void)gameTimeType;
    const int millisecondsPerFrame = GetGameMsPerFrame();
    return millisecondsPerFrame > 0 ? 1000 / millisecondsPerFrame : 0;
  }
  virtual int GetGameMsPerFrame() const { return 16; }
  virtual int GetGameMsPerRealSec() const { return 1000; }
  virtual int GetGameTimeMs() const { return GetFrameNum() * GetGameMsPerFrame(); }
  virtual const idVec3& GetGravity() const {
    static const idVec3 gravity{0.0F, 0.0F, -1066.0F};
    return gravity;
  }
  virtual bool TestGrappler(int spawnIndex, int timestamp,
                            float rangeToEnemy) {
    (void)spawnIndex;
    (void)timestamp;
    (void)rangeToEnemy;
    return true;
  }
  virtual int GetNextAITauntTime() const { return 0; }
  virtual void SetNextAITauntTime(int value) { (void)value; }
  virtual idEntity* GetEntityByNumber(int entityNumber) const {
    (void)entityNumber;
    return nullptr;
  }
  virtual idPhysics* GetPhysicsForId(int physicsId) const {
    idEntity* entity = GetEntityByNumber(physicsId);
    return entity != nullptr ? entity->GetPhysics() : nullptr;
  }
  virtual idRenderWorld* GetRenderWorld() const { return nullptr; }
  virtual idAASVisCache* GetAASVisCache() { return nullptr; }
  virtual idAIGameState* GetAIGameState() { return nullptr; }
  virtual int GetChallengeMode() const;
  virtual idEntity* GetDebugPlayer() const { return nullptr; }
  virtual idPlayer* GetPlayer(int index) const {
    (void)index;
    return nullptr;
  }
  virtual int NumAAS() const { return 0; }
  virtual idAAS2* GetAAS(int index) const {
    (void)index;
    return nullptr;
  }
  virtual idAAS2* GetAAS(const char* name, bool allowDefault) const {
    (void)name;
    return allowDefault && NumAAS() > 0 ? GetAAS(0) : nullptr;
  }
  virtual void AddTrackAAS(const idAAS2* aas) { (void)aas; }
  virtual int GetGameFlags() const { return 0; }
  virtual float GetAIIncomingDamageScale() const { return 1.0F; }
  virtual std::uint64_t SubmitTranslationQuery(
      const idVec3& start, const idVec3& end, int clipSize,
      const idMat3& axis, int contentsMask, int passEntityNumber,
      bool asynchronous, const char* debugName) {
    (void)start;
    (void)end;
    (void)clipSize;
    (void)axis;
    (void)contentsMask;
    (void)passEntityNumber;
    (void)asynchronous;
    (void)debugName;
    return 0;
  }
  virtual void TranslateClipModel(trace_t& result, const idVec3& start,
                                  const idVec3& end,
                                  const idClipModel* clipModel,
                                  const idMat3& startAxis, int clipMask,
                                  int passEntityNumber, bool moveClipModel,
                                  const char* userName) {
    (void)result;
    (void)start;
    (void)end;
    (void)clipModel;
    (void)startAxis;
    (void)clipMask;
    (void)passEntityNumber;
    (void)moveClipModel;
    (void)userName;
  }
  virtual bool GetStepMoveQueryResult(std::uint64_t queryIndex,
                                      trace_t& result, bool peek) {
    (void)queryIndex;
    (void)result;
    (void)peek;
    return false;
  }
  virtual bool GetClipQueryResult(std::uint64_t queryIndex,
                                  std::vector<idVec3>& contactPoints,
                                  bool peek) {
    (void)queryIndex;
    (void)contactPoints;
    (void)peek;
    return false;
  }
  virtual bool GetContactsQueryResult(std::uint64_t queryIndex,
                                      std::vector<idVec3>& contactPoints,
                                      bool peek) {
    (void)queryIndex;
    (void)contactPoints;
    (void)peek;
    return false;
  }
  virtual int GetEntitiesTouchingBounds(const idBounds& bounds,
                                        int contentsMask,
                                        int* entityNumbers,
                                        int maximumEntities,
                                        const char* debugName) {
    (void)bounds;
    (void)contentsMask;
    (void)entityNumbers;
    (void)maximumEntities;
    (void)debugName;
    return 0;
  }
  virtual bool AcquireRecycledTraceModel() { return false; }
  virtual std::uint64_t SubmitClipQuery(
      const idBounds& localBounds, const idVec3& origin,
      const idMat3& axis, int maximumResults, int contentsMask,
      const char* debugName) {
    (void)localBounds;
    (void)origin;
    (void)axis;
    (void)maximumResults;
    (void)contentsMask;
    (void)debugName;
    return 0;
  }
  virtual std::uint64_t SubmitContactsQuery(
      const idBounds& localBounds, const idVec3& origin,
      const idVec3& direction, float depth, const idMat3& axis,
      int maximumResults, int contentsMask, const char* debugName) {
    (void)localBounds;
    (void)origin;
    (void)direction;
    (void)depth;
    (void)axis;
    (void)maximumResults;
    (void)contentsMask;
    (void)debugName;
    return 0;
  }
  virtual void ShowCollisionPoint(const idVec3& point) {
    (void)point;
  }
  virtual void RecycleTraceModel(bool traceModelWasAcquired,
                                 bool clear) {
    (void)traceModelWasAcquired;
    (void)clear;
  }
  virtual idEncounterGroupManager* GetEncounterGroupManager() {
    return nullptr;
  }
  virtual idAIVoiceController* GetGlobalVoiceController() {
    return nullptr;
  }
  virtual int GetCurrentCheckpoint() const { return -1; }
  virtual void RemoveAIWorldStateUser(idEntity* entity) { (void)entity; }
  virtual void RemoveActiveAI(idAI2* ai) { (void)ai; }
  virtual void RemoveAIEventReceiver(idEntity* entity) { (void)entity; }
  virtual void RemoveAIEntityEvents(const idEntity* entity) { (void)entity; }
  virtual void AddAIBehaviorBodyEvent(const void* declaration,
                                      int currentTime,
                                      const idEntity* originator,
                                      const idEntity* instigator,
                                      const idVec3& origin, int delay) {
    (void)declaration;
    (void)currentTime;
    (void)originator;
    (void)instigator;
    (void)origin;
    (void)delay;
  }
  virtual void AddAIEvent(const void* declaration, int currentTime,
                          const idEntity* originator,
                          const idEntity* instigator,
                          const idVec3& origin, int delay) {
    (void)declaration;
    (void)currentTime;
    (void)originator;
    (void)instigator;
    (void)origin;
    (void)delay;
  }
  virtual idEntity* SpawnEntityFromDef(const idDeclEntityDef* declaration) {
    (void)declaration;
    return nullptr;
  }
  virtual void AddUpdateSlot(int spawnIndex, int timestamp, int slotType,
                             float range, bool alarming, bool dying,
                             bool spotted, bool killedByTakedown) {
    (void)spawnIndex;
    (void)timestamp;
    (void)slotType;
    (void)range;
    (void)alarming;
    (void)dying;
    (void)spotted;
    (void)killedByTakedown;
  }
  virtual int AddGlobalLensFlare(idRenderWorld* renderWorld,
                                 const idDeclFlare* declaration,
                                 const idVec3& sunDirection,
                                 const idVec3& occlusionOffset,
                                 bool isAutosprited) {
    (void)renderWorld;
    (void)declaration;
    (void)sunDirection;
    (void)occlusionOffset;
    (void)isAutosprited;
    return -1;
  }
  virtual void SetupGlobalLensFlareRenderModels(int handle,
                                                 float quadSize) {
    (void)handle;
    (void)quadSize;
  }
  virtual void UpdateGlobalLensFlarePosition(int handle,
                                             const idVec3& origin,
                                             const idMat3& axis,
                                             bool bindOrigin,
                                             bool bindAxis) {
    (void)handle;
    (void)origin;
    (void)axis;
    (void)bindOrigin;
    (void)bindAxis;
  }
  virtual void SetGlobalLensFlareColor(int handle,
                                       const idVec4& color) {
    (void)handle;
    (void)color;
  }
  virtual void StartGlobalLensFlare(int handle, const idVec3& origin,
                                    const idMat3& axis, const idVec4& color,
                                    float fadeStart, float fadeEnd) {
    (void)handle;
    (void)origin;
    (void)axis;
    (void)color;
    (void)fadeStart;
    (void)fadeEnd;
  }
  virtual void StopGlobalLensFlare(int handle) { (void)handle; }
  virtual bool GetTrackAASPos(const idAAS2* aas, idVec3& position) const {
    (void)aas;
    (void)position;
    return false;
  }
  virtual idSoundWorld* GetSoundWorld() const { return nullptr; }
  virtual int GetMapNameLength() const { return 0; }
  virtual void RemoveAIEvent(int handle) { (void)handle; }
  virtual int GetNumActiveAI() const { return 0; }
  virtual idAI2* GetActiveAI(int index) const {
    (void)index;
    return nullptr;
  }
  virtual int GetBestForwardPlayerViewDataID() const { return -1; }
  virtual int GetBestBackwardPlayerViewDataID() const { return -1; }
  virtual bool IsDangerZoneBad() const { return false; }
  virtual bool GetDangerZone(int currentTime, idVec3* origin,
                             float* radius) const {
    (void)currentTime;
    (void)origin;
    (void)radius;
    return false;
  }
  virtual void ClearDangerZone(int currentTime) { (void)currentTime; }
  virtual void ReportDangerZone(int currentTime, const idVec3& origin,
                                float radius) {
    (void)currentTime;
    (void)origin;
    (void)radius;
  }
  virtual idTarget_AITensionManager* GetAITensionManager() const {
    return nullptr;
  }
  void AddDyingAI(idEntity* entity) {
    if (entity == nullptr) return;
    for (int i = 0; i < dyingAI.Num(); ++i)
      if (dyingAI[i] == entity) return;
    dyingAI.Append(entity);
  }
  void RemoveDyingAI(idEntity* entity) {
    for (int i = 0; i < dyingAI.Num(); ++i) {
      if (dyingAI[i] == entity) {
        dyingAI.RemoveIndex(i);
        return;
      }
    }
  }
  bool HasDyingAI(const idEntity* entity) const {
    for (int i = 0; i < dyingAI.Num(); ++i)
      if (dyingAI[i] == entity) return true;
    return false;
  }
  std::intptr_t GetDebugEntity(...);
  std::intptr_t GetTensionManager(...);

  idList<idWaypoint*, 107> waypointEntities;

private:
  idList<idEntity*, 5> dyingAI;
};

inline idGameLocal* gameLocal = nullptr;

constexpr std::uint32_t AAS_AREA_CONTENTS_OBSTACLE = 0x800u;
constexpr std::uint32_t CONTENTS_DYNAMIC_OBSTACLE = 0x20000u;
constexpr std::uint32_t AAS_TFL_INVALID = 1u;

class idCVar {
public:
  idCVar(const char* cvarName, const char* defaultValue, int cvarFlags,
         const char* cvarDescription)
      : name(cvarName != nullptr ? cvarName : ""),
        valueString(defaultValue != nullptr ? defaultValue : ""),
        flags(cvarFlags),
        description(cvarDescription != nullptr ? cvarDescription : "") {
    try {
      valueFloat = std::stof(valueString);
      valueInteger = static_cast<int>(valueFloat);
    } catch (...) {
      valueInteger = 0;
      valueFloat = 0.0F;
    }
  }

  int GetInteger() const { return valueInteger; }
  float GetFloat() const { return valueFloat; }
  void SetInteger(int value) {
    valueInteger = value;
    valueFloat = static_cast<float>(value);
    valueString = std::to_string(value);
  }
  void SetFloat(float value) {
    valueFloat = value;
    valueInteger = static_cast<int>(value);
    valueString = std::to_string(value);
  }

  std::string name;
  std::string valueString;
  int valueInteger{0};
  float valueFloat{0.0F};
  int flags{0};
  std::string description;
};
