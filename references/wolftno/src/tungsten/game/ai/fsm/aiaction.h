#pragma once

#include "shared/idlib/containers/list.h"
#include "tungsten/game/game_runtime.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

struct idEventArgs {
  int event{0};
  idEntity* entity{nullptr};
  idVec3 point{};
  float value{0.0F};
};

struct idAIActionContext {
  idVec3 origin{};
  idVec3 destination{};
  float distance{0.0F};
  int currentTime{0};
  int deadline{0};
  bool moveFailed{false};
  bool moveStuck{false};
  bool animationStarted{false};
  bool animationFinished{false};
  bool interactionTriggered{false};
  bool expressionValue{false};
  bool entityDeleted{false};
};

class idAIFSM;
class idAIActionFSM;
class idAIState;
class idFiniteStateMachine;

struct idFSMTransitionResult;

class idStateTransition {
public:
  struct transDest_t {
    const char* nextStateType{nullptr};
    int transCode{0};
  };

  explicit idStateTransition(const char* transitionName = "transition")
      : name(transitionName != nullptr ? transitionName : "transition") {}
  explicit idStateTransition(int transitionFlags);
  virtual ~idStateTransition() = default;
  virtual bool Evaluate(idFiniteStateMachine*, idAIState*, int,
                        int eventId) const {
    return requiredEventId < 0 || requiredEventId == eventId;
  }
  void SetRequiredEventId(int value) { requiredEventId = value; }
  const char* GetName() const { return name; }
  int GetFlags() const { return flags; }
  void Save(std::string* output, const char* indent,
            const idList<transDest_t, 5>* dests) const;
  virtual void InternalSave(std::string*, const char*,
                            const idList<transDest_t, 5>*) const {}
private:
  const char* name;
  int requiredEventId{-1};
  int flags{0};
};

class idStateData {
public:
  struct transition_t {
    idStateTransition* transition{nullptr};
    idAIState* nextState{nullptr};
    int transCode{0};
    int eventId{-1};
    bool owned{false};
  };
  idStateData();
  virtual ~idStateData();
  void Condense();
  transition_t* AllocTransition();
  const idList<transition_t, 110>& GetTransitions() const { return transitions; }
private:
  idList<transition_t, 110> transitions;
};

class idState {
public:
  struct transitionInfo_t {
    const idState* from{nullptr};
    const idState* to{nullptr};
    int transCode{0};
    int time{0};
  };
  idState();
  virtual ~idState();
  idFiniteStateMachine* GetChildFSM() const;
  const char* GetChildFSMType() const;
  void SetChildFSM(idFiniteStateMachine* parentFSM,
                   idFiniteStateMachine* childFSM);
  bool IsChildDone() const;
  void SetChildDone(bool value) { forcedChildDone = value; }
  void Work(idFiniteStateMachine* fsm, int currentTime);
  void ReenterState(idFiniteStateMachine* fsm, unsigned int transCode,
                    int currentTime);
  bool IsStateWaiting(idFiniteStateMachine* fsm, unsigned int currentTime);
  void SetEnterFailed(bool value);
  void StartChildFSM(idFiniteStateMachine* parentFSM,
                     idFiniteStateMachine* childFSM, int currentTime);
  void InitState(idFiniteStateMachine* fsm);
  bool IsInterruptible() const;
  void EnterState(idFiniteStateMachine* fsm, unsigned int currentTime);
  void ExitState(idFiniteStateMachine* fsm, unsigned int currentTime);
  void Save(std::string* output, const char* indent) const;
  bool EvaluateTransitions(idFiniteStateMachine* fsm, int currentTime,
                           idFSMTransitionResult* result) const;
  bool EvaluateTransitions(idFiniteStateMachine* fsm, int currentTime,
                           int eventId, idFSMTransitionResult* result) const;
  bool EvaluateEventTransitions(idFiniteStateMachine* fsm, int currentTime,
                                idFSMTransitionResult* result) const;
  bool EvaluateFSMEventTransitions(idFiniteStateMachine* fsm, int eventId,
                                   int currentTime,
                                   idFSMTransitionResult* result) const;
  void Condense();
  void AddStateTransition(idStateTransition* transition,
                          idAIState* nextState, int transCode);
  void AddStateTransition(const char* transitionName,
                          idAIState* nextState, int transCode);
  void AddFSMEventTransition(int eventId, idAIState* nextState,
                             int transCode);
  bool DidEnterFail() const { return enterFailed; }

protected:
  idStateData stateData;
  idFiniteStateMachine* parentFSM{nullptr};
  idFiniteStateMachine* childFSM{nullptr};
  bool enterFailed{false};
  bool interruptible{true};
  bool forcedChildDone{false};
};

class idAIAction {
public:
  explicit idAIAction(const char* actionName = "action");
  virtual ~idAIAction();

  void InternalEnterState();
  void InternalExitState();
  void InternalWork();
  void InternalReenterState();
  bool InternalIsStateWaiting() const;

  const char* GetName() const { return name; }
  bool IsInitialized() const { return initialized; }
  bool IsActive() const { return active; }
  int GetWorkCount() const { return workCount; }
  void SetWaiting(bool value) { waiting = value; }

protected:
  void InitState() { initialized = true; }
  const char* name;
  bool initialized{false};
  bool active{false};
  bool waiting{false};
  int workCount{0};
};

#define WOLF_AI_ACTIONS(X) \
  X(MoveToPoint) X(MoveToPointNoFail) X(MoveToEntity) \
  X(MoveToEntityNoFail) X(MoveToPathPoint) X(MoveToPathPointNoFail) \
  X(MoveToCover) X(UseZipline) X(SetMovePushStatus) X(Idle) X(Pain) \
  X(StandToCrouch) X(CrouchToStand) X(PullTriggerRight) \
  X(ReleaseTriggerRight) X(PullTriggerLeft) X(ReleaseTriggerLeft) \
  X(ReloadWeapon) X(ReloadWeaponTorso) X(WaitForAnim) X(WaitForAnimVia) \
  X(LoopAnim) X(LoopAnimExitAtEnd) X(StartAnim) X(WaitForTraversalAnim) \
  X(ChangeAnimState) X(ChangeAnimStateVia) X(ForceAnimState) X(Dodge) \
  X(Wait) X(Dive) X(Melee) X(TurnToPoint) X(TurnToEntity) \
  X(TurnToEntityWithOffset) X(WaitForEntity) X(PlayVoiceOver) \
  X(StopVoiceOver) X(PlayInteractionVoiceOver) X(PlayOverrideAnim) \
  X(PlayOverrideAnimInterrupt) X(LeapAttack) X(DrawWeapon) \
  X(HolsterWeapon) X(PerformCoverAction) X(WaitForPlayerInteraction) \
  X(WaitForAIVar) X(PerfectMovement) X(SetAlertCycle) X(SetSubWeb) \
  X(SetFocus) X(ClearLookFocus) X(ClearAimFocus) X(SetAimPoint) \
  X(SetFireMode) X(SetScriptAbort) X(SetPosture) X(SetWalkState) \
  X(SetSitState) X(SetStandState) X(SetIdleState) X(IgnorePlayerApproach) \
  X(NoticePlayerApproach) X(SetPlayerEnemy) X(SetEnemy) X(SearchToTarget) \
  X(SetPlayerFocus) X(SetActionNodeGroup) X(Trigger) X(EnableDamage) \
  X(EnableAutoFocus) X(EnableBodyRotation) X(EnableWalkIK) \
  X(EnableHeadTracking) X(ForceAwarenessByDistance) X(SetAIVar) \
  X(SetScriptFlag) X(ClearScriptFlag) X(SetPerceptionFlag) \
  X(ClearPerceptionFlag) X(GiveItem) X(TakeItem) X(ClearWorldState) \
  X(ShowAttachment) X(HideAttachment) X(DropAttachment) X(SetMoveMode) \
  X(SetAccuracy) X(ScriptedAnimWeb)

#define WOLF_DECLARE_ACTION(Name) \
  class idAction_##Name : public idAIAction { \
  public: \
    idAction_##Name(); \
    const char* GetActionEventDef() const; \
    void InternalInitState(); \
    void Action_##Name##Enter(); \
    void AIAction_Work(); \
    void AIAction_ExitState(); \
    void StartNextState(); \
  };
WOLF_AI_ACTIONS(WOLF_DECLARE_ACTION)
#undef WOLF_DECLARE_ACTION

#define WOLF_AI_EVALUATORS(X) \
  X(ReachedPoint) X(MoveFailed) X(MoveStuck) X(AnimStarted) X(MoveTimedOut) \
  X(ActionExpired) X(AnimationError) X(AnimFinished) X(AbortMoveToCover) \
  X(FinishedCrouching) X(FinishedStanding) X(FinishedReloading) \
  X(FinishedReloadingTorso) X(TraversalAnimDone) X(DiveFinished) \
  X(MeleeFinished) X(TurnToPointFinished) X(EntityNear) X(VoiceOverFinished) \
  X(InteractionVoiceOverFinished) X(LeapAttackFinished) X(DrawWeaponFinished) \
  X(HolsterWeaponFinished) X(CoverActionFinished) X(AIVarExpressionTrue) \
  X(ScriptedAnimWebDone) X(ActionEntityDeleted) X(InteractionTriggered)

#define WOLF_DECLARE_EVALUATOR(Name) \
  class Name { public: bool Evaluate(const idAIActionContext& context) const; };
WOLF_AI_EVALUATORS(WOLF_DECLARE_EVALUATOR)
#undef WOLF_DECLARE_EVALUATOR

class idPresentablePlayer {
public:
  const char* GetType() const;
};

class idAIState : public idState, public idAIAction {
public:
  explicit idAIState(const char* stateName = "state") : idAIAction(stateName) {}
  using idAIAction::InternalEnterState;
  using idAIAction::InternalExitState;
  using idAIAction::InternalIsStateWaiting;
  using idAIAction::InternalReenterState;
  using idAIAction::InternalWork;
  virtual bool AIState_CanUse(idAIFSM*, unsigned int) { return true; }
  virtual void AIState_EnterState(idAIFSM*, unsigned int) {}
  virtual void AIState_ExitState(idAIFSM*, unsigned int) {}
  virtual void AIState_Work(idAIFSM*, unsigned int) {}
  virtual void AIState_ReenterState(idAIFSM*, unsigned int, int) {}
  virtual bool AIState_IsWaiting(idAIFSM*, unsigned int) { return false; }
  bool InternalCanUse(idAIFSM* fsm, unsigned int currentTime);
  void InternalEnterState(idAIFSM* fsm, unsigned int currentTime);
  void InternalExitState(idAIFSM* fsm, unsigned int currentTime);
  void InternalWork(idAIFSM* fsm, unsigned int currentTime);
  void InternalReenterState(idAIFSM* fsm, unsigned int transCode,
                            int currentTime);
  bool InternalIsStateWaiting(idAIFSM* fsm, unsigned int currentTime);
  void StartQueuedActions(idAIActionFSM* actionFSM, idAIFSM* parentFSM,
                          int currentTime);
  void ClearQueuedActions(idAIActionFSM* actionFSM, idAIFSM* parentFSM,
                          int currentTime);
  void SetChildError(bool value) { childError = value; }
  bool HasChildError() const { return childError; }
  void AddDefaultTransition() { ++defaultTransitionCount; }
  int GetDefaultTransitionCount() const { return defaultTransitionCount; }
  idAIActionFSM* GetChildActionFSM() const { return childActionFSM; }

protected:
  void InitState() { idAIAction::InitState(); }

private:
  bool childError{false};
  int defaultTransitionCount{0};
  idAIActionFSM* childActionFSM{nullptr};
};

class idAIStateError : public idAIState {
public:
  idAIStateError() : idAIState("StateError") {}
  void AIAction_EnterState();
};

class idAIStateDone : public idAIState {
public:
  idAIStateDone() : idAIState("StateDone") {}
};

class idAIActionsQueue {
public:
  bool InternalRespondsTo(const char* eventName) const;
  idAIAction* StartNextAction();
  void Clear();
  bool InternalCallEvent(const char* eventName);
  bool Append(idAIAction* action);
  int Num() const { return count; }

private:
  std::array<idAIAction*, 92> actions{};
  int count{0};
};

class idAIActionFSM {
public:
  idAIActionFSM();
  virtual ~idAIActionFSM() = default;
  idAIAction* GetCurrentAction() const;
  void StartQueuedActions();
  void SetNextState(idAIAction* action);
  void SetState(idAIAction* action);
  idAIActionsQueue& GetQueue() { return queue; }

private:
  idAIAction* current{nullptr};
  idAIAction* next{nullptr};
  idAIActionsQueue queue;
};

class idLegsFSM : public idAIActionFSM {
public:
  idLegsFSM();
  void InitStates();
  bool AreStatesInitialized() const { return statesInitialized; }

private:
  bool statesInitialized{false};
};

class idAIActionManager {
public:
  idAIActionManager();
  virtual ~idAIActionManager() = default;
  void Init();
  idAIActionFSM* FindFSM(int index);
  idAIActionFSM* FindFSM(const char* name);
  idAIAction* GetActionStateForEvent(const char* eventName) const;
  bool InternalCallEvent(const char* eventName);
  bool InternalRespondsTo(const char* eventName) const;
  bool GetActionBlocks(const char* eventName) const;
  bool GetActionStopsMovement(const char* eventName) const;
  int GetRegisteredActionCount() const { return actionCount; }

private:
  std::array<idAIAction*, 92> actions{};
  int actionCount{0};
  std::array<idAIActionFSM, 2> fsms{};
};

template <typename Key, typename Value>
class idHashTableT {
public:
  idHashTableT();
  ~idHashTableT() = default;
  bool Set(const Key& key, const Value& value);
  void Clear(...) { count = 0; }
  int Num() const { return count; }

private:
  std::array<Key, 128> keys{};
  std::array<Value, 128> values{};
  int count{0};
};

struct idTurnParameters {
  float yawDelta{0.0F};
  float duration{0.0F};
};

idTurnParameters CalcTurnParameters(const idVec3& forward,
                                    const idVec3& direction,
                                    float turnRate);
bool TurnToPoint(const idVec3& origin, const idVec3& forward,
                 const idVec3& point, float turnRate,
                 idTurnParameters* result);
void StartIdleAnimationForMoveNoFail(idAIAction& action);
bool UpdateMoveNoFail(idAIAction& action, const idAIActionContext& context);
bool UpdateMoveToPathPoint(idAIAction& action,
                           const idAIActionContext& context);

extern idCVar ai_debugActionScript;
extern idCVar ai_clearEvents;
extern idCVar ai_playInteractionVoiceOver_defaultLeaveDistance;
extern idCVar ai_coverSnapRadius;
extern idCVar ai_coverLerpRadius;
extern idCVar ai_coverAbortRadius;
