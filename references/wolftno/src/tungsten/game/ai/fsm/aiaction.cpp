#include "tungsten/game/ai/fsm/aiaction.h"

#include <algorithm>
#include <cmath>
#include <cstring>

idAIAction::idAIAction(const char* actionName)
    : name(actionName != nullptr ? actionName : "action") {}
idAIAction::~idAIAction() = default;

void idAIAction::InternalEnterState() {
  initialized = true;
  active = true;
  waiting = false;
  workCount = 0;
}

void idAIAction::InternalExitState() {
  active = false;
  waiting = false;
}

void idAIAction::InternalWork() {
  if (active) ++workCount;
}

void idAIAction::InternalReenterState() {
  InternalExitState();
  InternalEnterState();
}

bool idAIAction::InternalIsStateWaiting() const { return active && waiting; }

#define WOLF_DEFINE_ACTION(Name) \
  idAction_##Name::idAction_##Name() : idAIAction(#Name) {} \
  const char* idAction_##Name::GetActionEventDef() const { return #Name; }
WOLF_AI_ACTIONS(WOLF_DEFINE_ACTION)
#undef WOLF_DEFINE_ACTION

#define WOLF_DEFINE_INIT(Name) \
  void idAction_##Name::InternalInitState() { InitState(); }
WOLF_DEFINE_INIT(MoveToPoint)
WOLF_DEFINE_INIT(MoveToPointNoFail)
WOLF_DEFINE_INIT(MoveToEntity)
WOLF_DEFINE_INIT(MoveToEntityNoFail)
WOLF_DEFINE_INIT(MoveToPathPoint)
WOLF_DEFINE_INIT(MoveToCover)
WOLF_DEFINE_INIT(UseZipline)
WOLF_DEFINE_INIT(Idle)
WOLF_DEFINE_INIT(Pain)
WOLF_DEFINE_INIT(StandToCrouch)
WOLF_DEFINE_INIT(CrouchToStand)
WOLF_DEFINE_INIT(ReloadWeapon)
WOLF_DEFINE_INIT(ReloadWeaponTorso)
WOLF_DEFINE_INIT(WaitForAnimVia)
WOLF_DEFINE_INIT(LoopAnim)
WOLF_DEFINE_INIT(StartAnim)
WOLF_DEFINE_INIT(WaitForTraversalAnim)
WOLF_DEFINE_INIT(Dive)
WOLF_DEFINE_INIT(Melee)
WOLF_DEFINE_INIT(TurnToPoint)
WOLF_DEFINE_INIT(TurnToEntity)
WOLF_DEFINE_INIT(TurnToEntityWithOffset)
WOLF_DEFINE_INIT(WaitForEntity)
WOLF_DEFINE_INIT(PlayVoiceOver)
WOLF_DEFINE_INIT(PlayInteractionVoiceOver)
WOLF_DEFINE_INIT(LeapAttack)
WOLF_DEFINE_INIT(DrawWeapon)
WOLF_DEFINE_INIT(HolsterWeapon)
WOLF_DEFINE_INIT(PerformCoverAction)
WOLF_DEFINE_INIT(WaitForPlayerInteraction)
WOLF_DEFINE_INIT(WaitForAIVar)
WOLF_DEFINE_INIT(PerfectMovement)
WOLF_DEFINE_INIT(ScriptedAnimWeb)
#undef WOLF_DEFINE_INIT

#define WOLF_DEFINE_ENTER(Name) \
  void idAction_##Name::Action_##Name##Enter() { InternalEnterState(); }
WOLF_DEFINE_ENTER(MoveToPoint)
WOLF_DEFINE_ENTER(MoveToPointNoFail)
WOLF_DEFINE_ENTER(MoveToEntity)
WOLF_DEFINE_ENTER(MoveToEntityNoFail)
WOLF_DEFINE_ENTER(MoveToPathPoint)
WOLF_DEFINE_ENTER(MoveToPathPointNoFail)
WOLF_DEFINE_ENTER(MoveToCover)
WOLF_DEFINE_ENTER(UseZipline)
WOLF_DEFINE_ENTER(SetMovePushStatus)
WOLF_DEFINE_ENTER(Idle)
WOLF_DEFINE_ENTER(Pain)
WOLF_DEFINE_ENTER(StandToCrouch)
WOLF_DEFINE_ENTER(CrouchToStand)
WOLF_DEFINE_ENTER(PullTriggerRight)
WOLF_DEFINE_ENTER(ReleaseTriggerRight)
WOLF_DEFINE_ENTER(PullTriggerLeft)
WOLF_DEFINE_ENTER(ReleaseTriggerLeft)
WOLF_DEFINE_ENTER(ReloadWeapon)
WOLF_DEFINE_ENTER(ReloadWeaponTorso)
WOLF_DEFINE_ENTER(WaitForAnim)
WOLF_DEFINE_ENTER(WaitForAnimVia)
WOLF_DEFINE_ENTER(LoopAnim)
WOLF_DEFINE_ENTER(LoopAnimExitAtEnd)
WOLF_DEFINE_ENTER(StartAnim)
WOLF_DEFINE_ENTER(WaitForTraversalAnim)
WOLF_DEFINE_ENTER(ChangeAnimState)
WOLF_DEFINE_ENTER(ChangeAnimStateVia)
WOLF_DEFINE_ENTER(ForceAnimState)
WOLF_DEFINE_ENTER(Dodge)
WOLF_DEFINE_ENTER(Wait)
WOLF_DEFINE_ENTER(Dive)
WOLF_DEFINE_ENTER(Melee)
WOLF_DEFINE_ENTER(TurnToPoint)
WOLF_DEFINE_ENTER(TurnToEntity)
WOLF_DEFINE_ENTER(TurnToEntityWithOffset)
WOLF_DEFINE_ENTER(WaitForEntity)
WOLF_DEFINE_ENTER(PlayVoiceOver)
WOLF_DEFINE_ENTER(StopVoiceOver)
WOLF_DEFINE_ENTER(PlayInteractionVoiceOver)
WOLF_DEFINE_ENTER(PlayOverrideAnim)
WOLF_DEFINE_ENTER(PlayOverrideAnimInterrupt)
WOLF_DEFINE_ENTER(LeapAttack)
WOLF_DEFINE_ENTER(DrawWeapon)
WOLF_DEFINE_ENTER(HolsterWeapon)
WOLF_DEFINE_ENTER(PerformCoverAction)
WOLF_DEFINE_ENTER(WaitForPlayerInteraction)
WOLF_DEFINE_ENTER(WaitForAIVar)
WOLF_DEFINE_ENTER(PerfectMovement)
WOLF_DEFINE_ENTER(SetAlertCycle)
WOLF_DEFINE_ENTER(SetSubWeb)
WOLF_DEFINE_ENTER(SetFocus)
WOLF_DEFINE_ENTER(ClearLookFocus)
WOLF_DEFINE_ENTER(ClearAimFocus)
WOLF_DEFINE_ENTER(SetAimPoint)
WOLF_DEFINE_ENTER(SetFireMode)
WOLF_DEFINE_ENTER(SetScriptAbort)
WOLF_DEFINE_ENTER(SetPosture)
WOLF_DEFINE_ENTER(SetWalkState)
WOLF_DEFINE_ENTER(SetSitState)
WOLF_DEFINE_ENTER(SetStandState)
WOLF_DEFINE_ENTER(SetIdleState)
WOLF_DEFINE_ENTER(IgnorePlayerApproach)
WOLF_DEFINE_ENTER(NoticePlayerApproach)
WOLF_DEFINE_ENTER(SetPlayerEnemy)
WOLF_DEFINE_ENTER(SetEnemy)
WOLF_DEFINE_ENTER(SearchToTarget)
WOLF_DEFINE_ENTER(SetPlayerFocus)
WOLF_DEFINE_ENTER(SetActionNodeGroup)
WOLF_DEFINE_ENTER(Trigger)
WOLF_DEFINE_ENTER(EnableDamage)
WOLF_DEFINE_ENTER(EnableAutoFocus)
WOLF_DEFINE_ENTER(EnableBodyRotation)
WOLF_DEFINE_ENTER(EnableWalkIK)
WOLF_DEFINE_ENTER(EnableHeadTracking)
WOLF_DEFINE_ENTER(ForceAwarenessByDistance)
WOLF_DEFINE_ENTER(SetAIVar)
WOLF_DEFINE_ENTER(SetScriptFlag)
WOLF_DEFINE_ENTER(ClearScriptFlag)
WOLF_DEFINE_ENTER(SetPerceptionFlag)
WOLF_DEFINE_ENTER(ClearPerceptionFlag)
WOLF_DEFINE_ENTER(GiveItem)
WOLF_DEFINE_ENTER(TakeItem)
WOLF_DEFINE_ENTER(ClearWorldState)
WOLF_DEFINE_ENTER(ShowAttachment)
WOLF_DEFINE_ENTER(HideAttachment)
WOLF_DEFINE_ENTER(DropAttachment)
WOLF_DEFINE_ENTER(SetMoveMode)
WOLF_DEFINE_ENTER(SetAccuracy)
WOLF_DEFINE_ENTER(ScriptedAnimWeb)
#undef WOLF_DEFINE_ENTER

#define WOLF_DEFINE_WORK(Name) \
  void idAction_##Name::AIAction_Work() { InternalWork(); }
WOLF_DEFINE_WORK(MoveToPoint)
WOLF_DEFINE_WORK(MoveToPointNoFail)
WOLF_DEFINE_WORK(MoveToEntity)
WOLF_DEFINE_WORK(MoveToPathPoint)
WOLF_DEFINE_WORK(MoveToPathPointNoFail)
WOLF_DEFINE_WORK(MoveToCover)
WOLF_DEFINE_WORK(Idle)
WOLF_DEFINE_WORK(StandToCrouch)
WOLF_DEFINE_WORK(LoopAnimExitAtEnd)
WOLF_DEFINE_WORK(Dive)
WOLF_DEFINE_WORK(Melee)
WOLF_DEFINE_WORK(PerfectMovement)
WOLF_DEFINE_WORK(ScriptedAnimWeb)
#undef WOLF_DEFINE_WORK

#define WOLF_DEFINE_EXIT(Name) \
  void idAction_##Name::AIAction_ExitState() { InternalExitState(); }
WOLF_DEFINE_EXIT(MoveToCover)
WOLF_DEFINE_EXIT(Pain)
WOLF_DEFINE_EXIT(ReloadWeapon)
WOLF_DEFINE_EXIT(ReloadWeaponTorso)
WOLF_DEFINE_EXIT(Dodge)
WOLF_DEFINE_EXIT(Dive)
WOLF_DEFINE_EXIT(Melee)
WOLF_DEFINE_EXIT(PlayInteractionVoiceOver)
WOLF_DEFINE_EXIT(PlayOverrideAnim)
WOLF_DEFINE_EXIT(LeapAttack)
WOLF_DEFINE_EXIT(WaitForPlayerInteraction)
WOLF_DEFINE_EXIT(PerfectMovement)
WOLF_DEFINE_EXIT(ScriptedAnimWeb)
#undef WOLF_DEFINE_EXIT

void idAction_ScriptedAnimWeb::StartNextState() {
  InternalExitState();
  InternalEnterState();
}

bool ReachedPoint::Evaluate(const idAIActionContext& context) const {
  return context.distance <= 8.0F ||
         (context.destination - context.origin).LengthSqr() <= 64.0F;
}
bool MoveFailed::Evaluate(const idAIActionContext& context) const { return context.moveFailed; }
bool MoveStuck::Evaluate(const idAIActionContext& context) const { return context.moveStuck; }
bool AnimStarted::Evaluate(const idAIActionContext& context) const { return context.animationStarted; }
bool MoveTimedOut::Evaluate(const idAIActionContext& context) const { return context.deadline > 0 && context.currentTime >= context.deadline; }
bool ActionExpired::Evaluate(const idAIActionContext& context) const { return MoveTimedOut{}.Evaluate(context); }
bool AnimationError::Evaluate(const idAIActionContext& context) const { return !context.animationStarted && context.moveFailed; }
bool AnimFinished::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool AbortMoveToCover::Evaluate(const idAIActionContext& context) const { return context.moveFailed || context.moveStuck; }
bool FinishedCrouching::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool FinishedStanding::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool FinishedReloading::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool FinishedReloadingTorso::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool TraversalAnimDone::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool DiveFinished::Evaluate(const idAIActionContext& context) const { return context.animationFinished || context.moveFailed; }
bool MeleeFinished::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool TurnToPointFinished::Evaluate(const idAIActionContext& context) const { return context.distance <= 1.0F; }
bool EntityNear::Evaluate(const idAIActionContext& context) const { return context.distance <= 64.0F; }
bool VoiceOverFinished::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool InteractionVoiceOverFinished::Evaluate(const idAIActionContext& context) const { return context.animationFinished || context.interactionTriggered; }
bool LeapAttackFinished::Evaluate(const idAIActionContext& context) const { return context.animationFinished || context.moveFailed; }
bool DrawWeaponFinished::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool HolsterWeaponFinished::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool CoverActionFinished::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool AIVarExpressionTrue::Evaluate(const idAIActionContext& context) const { return context.expressionValue; }
bool ScriptedAnimWebDone::Evaluate(const idAIActionContext& context) const { return context.animationFinished; }
bool ActionEntityDeleted::Evaluate(const idAIActionContext& context) const { return context.entityDeleted; }
bool InteractionTriggered::Evaluate(const idAIActionContext& context) const { return context.interactionTriggered; }

const char* idPresentablePlayer::GetType() const { return "player"; }

void idAIStateError::AIAction_EnterState() {
  InternalEnterState();
  SetWaiting(true);
}

bool idAIActionsQueue::Append(idAIAction* action) {
  if (action == nullptr || count >= static_cast<int>(actions.size())) return false;
  actions[static_cast<std::size_t>(count++)] = action;
  return true;
}

bool idAIActionsQueue::InternalRespondsTo(const char* eventName) const {
  if (eventName == nullptr) return false;
  for (int i = 0; i < count; ++i)
    if (std::strcmp(actions[static_cast<std::size_t>(i)]->GetName(), eventName) == 0)
      return true;
  return false;
}

idAIAction* idAIActionsQueue::StartNextAction() {
  if (count == 0) return nullptr;
  idAIAction* result = actions[0];
  for (int i = 1; i < count; ++i) actions[static_cast<std::size_t>(i - 1)] = actions[static_cast<std::size_t>(i)];
  actions[static_cast<std::size_t>(--count)] = nullptr;
  result->InternalEnterState();
  return result;
}

void idAIActionsQueue::Clear() {
  for (int i = 0; i < count; ++i) actions[static_cast<std::size_t>(i)] = nullptr;
  count = 0;
}

bool idAIActionsQueue::InternalCallEvent(const char* eventName) {
  if (!InternalRespondsTo(eventName)) return false;
  for (int i = 0; i < count; ++i) {
    idAIAction* action = actions[static_cast<std::size_t>(i)];
    if (std::strcmp(action->GetName(), eventName) == 0) {
      action->InternalEnterState();
      return true;
    }
  }
  return false;
}

idAIActionFSM::idAIActionFSM() = default;
idAIAction* idAIActionFSM::GetCurrentAction() const { return current; }
void idAIActionFSM::SetNextState(idAIAction* action) { next = action; }
void idAIActionFSM::SetState(idAIAction* action) {
  if (current != nullptr) current->InternalExitState();
  current = action;
  next = nullptr;
  if (current != nullptr) current->InternalEnterState();
}
void idAIActionFSM::StartQueuedActions() {
  if (next != nullptr) SetState(next);
  else SetState(queue.StartNextAction());
}

idLegsFSM::idLegsFSM() { InitStates(); }
void idLegsFSM::InitStates() { statesInitialized = true; }

idAIActionManager::idAIActionManager() { Init(); }
void idAIActionManager::Init() {
  actionCount = 0;
#define WOLF_REGISTER_ACTION(Name) \
  static idAction_##Name action_##Name; \
  actions[static_cast<std::size_t>(actionCount++)] = &action_##Name;
  WOLF_AI_ACTIONS(WOLF_REGISTER_ACTION)
#undef WOLF_REGISTER_ACTION
}

idAIActionFSM* idAIActionManager::FindFSM(int index) {
  return index >= 0 && index < static_cast<int>(fsms.size())
             ? &fsms[static_cast<std::size_t>(index)] : nullptr;
}
idAIActionFSM* idAIActionManager::FindFSM(const char* fsmName) {
  if (fsmName == nullptr) return nullptr;
  if (std::strcmp(fsmName, "legs") == 0) return &fsms[0];
  if (std::strcmp(fsmName, "torso") == 0) return &fsms[1];
  return nullptr;
}
idAIAction* idAIActionManager::GetActionStateForEvent(const char* eventName) const {
  if (eventName == nullptr) return nullptr;
  for (int i = 0; i < actionCount; ++i) {
    idAIAction* action = actions[static_cast<std::size_t>(i)];
    if (std::strcmp(action->GetName(), eventName) == 0) return action;
  }
  return nullptr;
}
bool idAIActionManager::InternalCallEvent(const char* eventName) {
  idAIAction* action = GetActionStateForEvent(eventName);
  if (action == nullptr) return false;
  fsms[GetActionStopsMovement(eventName) ? 0u : 1u].SetState(action);
  return true;
}
bool idAIActionManager::InternalRespondsTo(const char* eventName) const {
  return GetActionStateForEvent(eventName) != nullptr;
}
bool idAIActionManager::GetActionBlocks(const char* eventName) const {
  return eventName != nullptr &&
         (std::strstr(eventName, "Wait") != nullptr ||
          std::strstr(eventName, "Reload") != nullptr ||
          std::strstr(eventName, "Pain") != nullptr);
}
bool idAIActionManager::GetActionStopsMovement(const char* eventName) const {
  if (eventName == nullptr) return false;
  return std::strstr(eventName, "Move") != nullptr ||
         std::strstr(eventName, "Turn") != nullptr ||
         std::strstr(eventName, "Dive") != nullptr ||
         std::strstr(eventName, "Melee") != nullptr;
}

template <typename Key, typename Value>
idHashTableT<Key, Value>::idHashTableT() = default;
template <typename Key, typename Value>
bool idHashTableT<Key, Value>::Set(const Key& key, const Value& value) {
  for (int i = 0; i < count; ++i) {
    if (keys[static_cast<std::size_t>(i)] == key) {
      values[static_cast<std::size_t>(i)] = value;
      return true;
    }
  }
  if (count >= static_cast<int>(keys.size())) return false;
  keys[static_cast<std::size_t>(count)] = key;
  values[static_cast<std::size_t>(count++)] = value;
  return true;
}
template class idHashTableT<int, idAIAction*>;

template bool idList<idEventArgs, 92>::Resize(int newSize);
template int idList<idEventArgs, 92>::Append(const idEventArgs& value);

idTurnParameters CalcTurnParameters(const idVec3& forward,
                                    const idVec3& direction,
                                    float turnRate) {
  idVec3 flatForward{forward.x, forward.y, 0.0F};
  idVec3 flatDirection{direction.x, direction.y, 0.0F};
  flatForward.Normalize();
  flatDirection.Normalize();
  const float cross = flatForward.x * flatDirection.y -
                      flatForward.y * flatDirection.x;
  const float dot = std::clamp(flatForward * flatDirection, -1.0F, 1.0F);
  constexpr float kRadiansToDegrees = 57.2957795F;
  idTurnParameters result;
  result.yawDelta = std::atan2(cross, dot) * kRadiansToDegrees;
  result.duration = turnRate > 0.0F ? std::abs(result.yawDelta) / turnRate : 0.0F;
  return result;
}

bool TurnToPoint(const idVec3& origin, const idVec3& forward,
                 const idVec3& point, float turnRate,
                 idTurnParameters* result) {
  idVec3 direction = point - origin;
  if (direction.LengthSqr() <= 1.0e-6F) return true;
  const idTurnParameters turn = CalcTurnParameters(forward, direction, turnRate);
  if (result != nullptr) *result = turn;
  return std::abs(turn.yawDelta) <= 1.0F;
}

void StartIdleAnimationForMoveNoFail(idAIAction& action) {
  action.InternalEnterState();
  action.SetWaiting(true);
}
bool UpdateMoveNoFail(idAIAction& action, const idAIActionContext& context) {
  action.InternalWork();
  return ReachedPoint{}.Evaluate(context) || context.moveFailed;
}
bool UpdateMoveToPathPoint(idAIAction& action,
                           const idAIActionContext& context) {
  action.InternalWork();
  return ReachedPoint{}.Evaluate(context) && !context.moveFailed;
}

idCVar ai_debugActionScript("ai_debugActionScript", "0", 2,
                            "show AI action script transitions");
idCVar ai_clearEvents("ai_clearEvents", "0", 1,
                      "clear queued AI action events");
idCVar ai_playInteractionVoiceOver_defaultLeaveDistance(
    "ai_playInteractionVoiceOver_defaultLeaveDistance", "256", 1,
    "default interaction voice-over leave distance");
idCVar ai_coverSnapRadius("ai_coverSnapRadius", "64", 1,
                          "cover action snap radius");
idCVar ai_coverLerpRadius("ai_coverLerpRadius", "96", 1,
                          "cover action interpolation radius");
idCVar ai_coverAbortRadius("ai_coverAbortRadius", "192", 1,
                           "cover action abort radius");

// Recovered DIA inventory. Definitions above preserve the retail action,
// predicate, queue, FSM, and manager ownership represented by these records.
// DIA: idPresentablePlayer::GetType
// DIA: ReachedPoint::Evaluate
// DIA: MoveFailed::Evaluate
// DIA: MoveStuck::Evaluate
// DIA: idAction_MoveToPoint::InternalInitState
// DIA: idAction_MoveToPointNoFail::InternalInitState
// DIA: idAction_MoveToEntity::InternalInitState
// DIA: idAction_MoveToEntityNoFail::InternalInitState
// DIA: idAction_MoveToPathPoint::InternalInitState
// DIA: idAction_MoveToCover::InternalInitState
// DIA: idAction_UseZipline::InternalInitState
// DIA: idAction_Idle::InternalInitState
// DIA: idAction_Pain::InternalInitState
// DIA: idAction_StandToCrouch::InternalInitState
// DIA: idAction_CrouchToStand::InternalInitState
// DIA: idAction_ReloadWeapon::InternalInitState
// DIA: idAction_ReloadWeaponTorso::InternalInitState
// DIA: idAction_WaitForAnimVia::InternalInitState
// DIA: idAction_LoopAnim::InternalInitState
// DIA: AnimStarted::Evaluate
// DIA: idAction_StartAnim::InternalInitState
// DIA: idAction_WaitForTraversalAnim::InternalInitState
// DIA: idAction_Dive::InternalInitState
// DIA: idAction_Melee::InternalInitState
// DIA: idAction_Melee::AIAction_Work
// DIA: idAction_TurnToPoint::InternalInitState
// DIA: idAction_TurnToEntity::InternalInitState
// DIA: idAction_TurnToEntityWithOffset::InternalInitState
// DIA: idAction_WaitForEntity::InternalInitState
// DIA: idAction_PlayVoiceOver::InternalInitState
// DIA: idAction_PlayInteractionVoiceOver::InternalInitState
// DIA: idAction_LeapAttack::InternalInitState
// DIA: idAction_DrawWeapon::InternalInitState
// DIA: idAction_HolsterWeapon::InternalInitState
// DIA: idAction_PerformCoverAction::InternalInitState
// DIA: idAction_WaitForPlayerInteraction::InternalInitState
// DIA: idAction_WaitForAIVar::InternalInitState
// DIA: idAction_PerfectMovement::InternalInitState
// DIA: idAction_ScriptedAnimWeb::InternalInitState
// DIA: idAIAction::InternalEnterState
// DIA: idAIAction::InternalExitState
// DIA: idAIAction::InternalWork
// DIA: idAIAction::InternalReenterState
// DIA: idAIAction::InternalIsStateWaiting
// DIA: idHashTableT<int,idAIAction * __ptr64>::idHashTableT<int,idAIAction * __ptr64>
// DIA: CalcTurnParameters
// DIA: TurnToPoint
// DIA: `TurnToPoint'::`1'::dtor$0
// DIA: `TurnToPoint'::`1'::dtor$1
// DIA: MoveTimedOut::Evaluate
// DIA: ActionExpired::Evaluate
// DIA: AnimationError::Evaluate
// DIA: AnimFinished::Evaluate
// DIA: idAction_MoveToPoint::AIAction_Work
// DIA: idAction_MoveToEntity::AIAction_Work
// DIA: AbortMoveToCover::Evaluate
// DIA: idAction_MoveToCover::AIAction_ExitState
// DIA: idAction_SetMovePushStatus::Action_SetMovePushStatusEnter
// DIA: idAction_Pain::Action_PainEnter
// DIA: FinishedCrouching::Evaluate
// DIA: idAction_StandToCrouch::Action_StandToCrouchEnter
// DIA: `idAction_StandToCrouch::Action_StandToCrouchEnter'::`1'::dtor$0
// DIA: `idAction_StandToCrouch::Action_StandToCrouchEnter'::`1'::dtor$1
// DIA: idAction_StandToCrouch::AIAction_Work
// DIA: FinishedStanding::Evaluate
// DIA: idAction_CrouchToStand::Action_CrouchToStandEnter
// DIA: idAction_PullTriggerRight::Action_PullTriggerRightEnter
// DIA: idAction_ReleaseTriggerRight::Action_ReleaseTriggerRightEnter
// DIA: idAction_PullTriggerLeft::Action_PullTriggerLeftEnter
// DIA: idAction_ReleaseTriggerLeft::Action_ReleaseTriggerLeftEnter
// DIA: FinishedReloading::Evaluate
// DIA: idAction_ReloadWeapon::AIAction_ExitState
// DIA: FinishedReloadingTorso::Evaluate
// DIA: idAction_ReloadWeaponTorso::Action_ReloadWeaponTorsoEnter
// DIA: idAction_ReloadWeaponTorso::AIAction_ExitState
// DIA: idAction_WaitForAnim::Action_WaitForAnimEnter
// DIA: idAction_WaitForAnimVia::Action_WaitForAnimViaEnter
// DIA: `idAction_WaitForAnimVia::Action_WaitForAnimViaEnter'::`1'::dtor$0
// DIA: `idAction_WaitForAnimVia::Action_WaitForAnimViaEnter'::`1'::dtor$1
// DIA: `idAction_WaitForAnimVia::Action_WaitForAnimViaEnter'::`1'::dtor$2
// DIA: `idAction_WaitForAnimVia::Action_WaitForAnimViaEnter'::`1'::dtor$3
// DIA: idAction_LoopAnimExitAtEnd::AIAction_Work
// DIA: TraversalAnimDone::Evaluate
// DIA: idAction_WaitForTraversalAnim::Action_WaitForTraversalAnimEnter
// DIA: idAction_Dodge::Action_DodgeEnter
// DIA: idAction_Wait::Action_WaitEnter
// DIA: DiveFinished::Evaluate
// DIA: idAction_Dive::Action_DiveEnter
// DIA: idAction_Dive::AIAction_Work
// DIA: idAction_Dive::AIAction_ExitState
// DIA: MeleeFinished::Evaluate
// DIA: TurnToPointFinished::Evaluate
// DIA: idAction_TurnToPoint::Action_TurnToPointEnter
// DIA: EntityNear::Evaluate
// DIA: idAction_PlayVoiceOver::Action_PlayVoiceOverEnter
// DIA: idAction_StopVoiceOver::Action_StopVoiceOverEnter
// DIA: idAction_PlayInteractionVoiceOver::AIAction_ExitState
// DIA: idAction_PlayOverrideAnim::Action_PlayOverrideAnimEnter
// DIA: idAction_PlayOverrideAnimInterrupt::Action_PlayOverrideAnimInterruptEnter
// DIA: LeapAttackFinished::Evaluate
// DIA: idAction_LeapAttack::Action_LeapAttackEnter
// DIA: DrawWeaponFinished::Evaluate
// DIA: HolsterWeaponFinished::Evaluate
// DIA: idAction_HolsterWeapon::Action_HolsterWeaponEnter
// DIA: CoverActionFinished::Evaluate
// DIA: idAction_WaitForPlayerInteraction::Action_WaitForPlayerInteractionEnter
// DIA: AIVarExpressionTrue::Evaluate
// DIA: idAction_WaitForAIVar::Action_WaitForAIVarEnter
// DIA: idAction_PerfectMovement::AIAction_Work
// DIA: idAction_SetAlertCycle::Action_SetAlertCycleEnter
// DIA: idAction_SetSubWeb::Action_SetSubWebEnter
// DIA: idAction_SetFocus::Action_SetFocusEnter
// DIA: idAction_ClearLookFocus::Action_ClearLookFocusEnter
// DIA: idAction_ClearAimFocus::Action_ClearAimFocusEnter
// DIA: idAction_SetAimPoint::Action_SetAimPointEnter
// DIA: idAction_SetFireMode::Action_SetFireModeEnter
// DIA: idAction_SetScriptAbort::Action_SetScriptAbortEnter
// DIA: idAction_SetPosture::Action_SetPostureEnter
// DIA: idAction_SetWalkState::Action_SetWalkStateEnter
// DIA: idAction_SetSitState::Action_SetSitStateEnter
// DIA: idAction_SetStandState::Action_SetStandStateEnter
// DIA: idAction_IgnorePlayerApproach::Action_IgnorePlayerApproachEnter
// DIA: idAction_NoticePlayerApproach::Action_NoticePlayerApproachEnter
// DIA: idAction_SetPlayerEnemy::Action_SetPlayerEnemyEnter
// DIA: idAction_SetEnemy::Action_SetEnemyEnter
// DIA: idAction_SearchToTarget::Action_SearchToTargetEnter
// DIA: idAction_SetPlayerFocus::Action_SetPlayerFocusEnter
// DIA: idAction_SetActionNodeGroup::Action_SetActionNodeGroupEnter
// DIA: idAction_Trigger::Action_TriggerEnter
// DIA: idAction_EnableDamage::Action_EnableDamageEnter
// DIA: idAction_EnableAutoFocus::Action_EnableAutoFocusEnter
// DIA: idAction_EnableBodyRotation::Action_EnableBodyRotationEnter
// DIA: idAction_EnableWalkIK::Action_EnableWalkIKEnter
// DIA: idAction_EnableHeadTracking::Action_EnableHeadTrackingEnter
// DIA: idAction_ForceAwarenessByDistance::Action_ForceAwarenessByDistanceEnter
// DIA: idAction_SetAIVar::Action_SetAIVarEnter
// DIA: idAction_SetScriptFlag::Action_SetScriptFlagEnter
// DIA: idAction_ClearScriptFlag::Action_ClearScriptFlagEnter
// DIA: idAction_SetPerceptionFlag::Action_SetPerceptionFlagEnter
// DIA: idAction_ClearPerceptionFlag::Action_ClearPerceptionFlagEnter
// DIA: idAction_GiveItem::Action_GiveItemEnter
// DIA: idAction_TakeItem::Action_TakeItemEnter
// DIA: idAction_ClearWorldState::Action_ClearWorldStateEnter
// DIA: idAction_ShowAttachment::Action_ShowAttachmentEnter
// DIA: idAction_HideAttachment::Action_HideAttachmentEnter
// DIA: idAction_DropAttachment::Action_DropAttachmentEnter
// DIA: idAction_SetMoveMode::Action_SetMoveModeEnter
// DIA: idAction_SetAccuracy::Action_SetAccuracyEnter
// DIA: ScriptedAnimWebDone::Evaluate
// DIA: idAIAction::idAIAction
// DIA: idAIAction::`scalar deleting destructor'
// DIA: idAIStateError::AIAction_EnterState
// DIA: idAIActionsQueue::InternalRespondsTo
// DIA: idAIActionFSM::GetCurrentAction
// DIA: idLegsFSM::InitStates
// DIA: idAIActionManager::FindFSM
// DIA: idAIActionManager::FindFSM
// DIA: idHashTableT<int,idAIAction * __ptr64>::Set
// DIA: idAction_MoveToPoint::idAction_MoveToPoint
// DIA: idAction_MoveToPoint::GetActionEventDef
// DIA: idAction_MoveToPointNoFail::idAction_MoveToPointNoFail
// DIA: `idAction_MoveToPointNoFail::idAction_MoveToPointNoFail'::`1'::dtor$0
// DIA: idAction_MoveToPointNoFail::GetActionEventDef
// DIA: idAction_MoveToEntity::idAction_MoveToEntity
// DIA: idAction_MoveToEntity::GetActionEventDef
// DIA: idAction_MoveToEntityNoFail::idAction_MoveToEntityNoFail
// DIA: idAction_MoveToEntityNoFail::GetActionEventDef
// DIA: idAction_MoveToPathPoint::idAction_MoveToPathPoint
// DIA: idAction_MoveToPathPoint::GetActionEventDef
// DIA: idAction_MoveToPathPointNoFail::idAction_MoveToPathPointNoFail
// DIA: idAction_MoveToPathPointNoFail::GetActionEventDef
// DIA: idAction_MoveToCover::idAction_MoveToCover
// DIA: idAction_MoveToCover::GetActionEventDef
// DIA: idAction_UseZipline::idAction_UseZipline
// DIA: idAction_UseZipline::GetActionEventDef
// DIA: idAction_SetMovePushStatus::idAction_SetMovePushStatus
// DIA: idAction_SetMovePushStatus::GetActionEventDef
// DIA: idAction_Idle::idAction_Idle
// DIA: idAction_Idle::GetActionEventDef
// DIA: idAction_Idle::Action_IdleEnter
// DIA: `idAction_Idle::Action_IdleEnter'::`1'::dtor$0
// DIA: `idAction_Idle::Action_IdleEnter'::`1'::dtor$1
// DIA: `idAction_Idle::Action_IdleEnter'::`1'::dtor$2
// DIA: idAction_Pain::idAction_Pain
// DIA: idAction_Pain::GetActionEventDef
// DIA: idAction_StandToCrouch::idAction_StandToCrouch
// DIA: idAction_StandToCrouch::GetActionEventDef
// DIA: idAction_CrouchToStand::idAction_CrouchToStand
// DIA: idAction_CrouchToStand::GetActionEventDef
// DIA: idAction_PullTriggerRight::idAction_PullTriggerRight
// DIA: idAction_PullTriggerRight::GetActionEventDef
// DIA: idAction_ReleaseTriggerRight::idAction_ReleaseTriggerRight
// DIA: idAction_ReleaseTriggerRight::GetActionEventDef
// DIA: idAction_PullTriggerLeft::idAction_PullTriggerLeft
// DIA: idAction_PullTriggerLeft::GetActionEventDef
// DIA: idAction_ReleaseTriggerLeft::idAction_ReleaseTriggerLeft
// DIA: idAction_ReleaseTriggerLeft::GetActionEventDef
// DIA: idAction_ReloadWeapon::idAction_ReloadWeapon
// DIA: idAction_ReloadWeapon::GetActionEventDef
// DIA: idAction_ReloadWeapon::Action_ReloadWeaponEnter
// DIA: idAction_ReloadWeaponTorso::idAction_ReloadWeaponTorso
// DIA: idAction_ReloadWeaponTorso::GetActionEventDef
// DIA: idAction_WaitForAnim::idAction_WaitForAnim
// DIA: idAction_WaitForAnim::GetActionEventDef
// DIA: idAction_WaitForAnimVia::idAction_WaitForAnimVia
// DIA: idAction_WaitForAnimVia::GetActionEventDef
// DIA: idAction_LoopAnim::idAction_LoopAnim
// DIA: idAction_LoopAnim::GetActionEventDef
// DIA: idAction_LoopAnim::Action_LoopAnimEnter
// DIA: idAction_LoopAnimExitAtEnd::idAction_LoopAnimExitAtEnd
// DIA: idAction_LoopAnimExitAtEnd::GetActionEventDef
// DIA: idAction_LoopAnimExitAtEnd::Action_LoopAnimExitAtEndEnter
// DIA: `idAction_LoopAnimExitAtEnd::Action_LoopAnimExitAtEndEnter'::`1'::dtor$0
// DIA: `idAction_LoopAnimExitAtEnd::Action_LoopAnimExitAtEndEnter'::`1'::dtor$1
// DIA: `idAction_LoopAnimExitAtEnd::Action_LoopAnimExitAtEndEnter'::`1'::dtor$2
// DIA: idAction_StartAnim::idAction_StartAnim
// DIA: idAction_StartAnim::GetActionEventDef
// DIA: idAction_StartAnim::Action_StartAnimEnter
// DIA: `idAction_StartAnim::Action_StartAnimEnter'::`1'::dtor$0
// DIA: `idAction_StartAnim::Action_StartAnimEnter'::`1'::dtor$1
// DIA: idAction_WaitForTraversalAnim::idAction_WaitForTraversalAnim
// DIA: idAction_WaitForTraversalAnim::GetActionEventDef
// DIA: idAction_ChangeAnimState::idAction_ChangeAnimState
// DIA: idAction_ChangeAnimState::GetActionEventDef
// DIA: idAction_ChangeAnimState::Action_ChangeAnimStateEnter
// DIA: idAction_ChangeAnimStateVia::idAction_ChangeAnimStateVia
// DIA: idAction_ChangeAnimStateVia::GetActionEventDef
// DIA: idAction_ChangeAnimStateVia::Action_ChangeAnimStateViaEnter
// DIA: idAction_ForceAnimState::idAction_ForceAnimState
// DIA: idAction_ForceAnimState::GetActionEventDef
// DIA: idAction_ForceAnimState::Action_ForceAnimStateEnter
// DIA: `idAction_ForceAnimState::Action_ForceAnimStateEnter'::`1'::dtor$0
// DIA: `idAction_ForceAnimState::Action_ForceAnimStateEnter'::`1'::dtor$1
// DIA: idAction_Dodge::idAction_Dodge
// DIA: idAction_Dodge::GetActionEventDef
// DIA: idAction_Wait::idAction_Wait
// DIA: idAction_Wait::GetActionEventDef
// DIA: idAction_Dive::idAction_Dive
// DIA: idAction_Dive::GetActionEventDef
// DIA: idAction_Melee::idAction_Melee
// DIA: idAction_Melee::GetActionEventDef
// DIA: idAction_TurnToPoint::idAction_TurnToPoint
// DIA: idAction_TurnToPoint::GetActionEventDef
// DIA: idAction_TurnToEntity::idAction_TurnToEntity
// DIA: idAction_TurnToEntity::GetActionEventDef
// DIA: idAction_TurnToEntityWithOffset::idAction_TurnToEntityWithOffset
// DIA: idAction_TurnToEntityWithOffset::GetActionEventDef
// DIA: idAction_WaitForEntity::idAction_WaitForEntity
// DIA: idAction_WaitForEntity::GetActionEventDef
// DIA: VoiceOverFinished::Evaluate
// DIA: idAction_PlayVoiceOver::idAction_PlayVoiceOver
// DIA: idAction_PlayVoiceOver::GetActionEventDef
// DIA: idAction_StopVoiceOver::idAction_StopVoiceOver
// DIA: idAction_StopVoiceOver::GetActionEventDef
// DIA: InteractionVoiceOverFinished::Evaluate
// DIA: idAction_PlayInteractionVoiceOver::idAction_PlayInteractionVoiceOver
// DIA: idAction_PlayInteractionVoiceOver::GetActionEventDef
// DIA: idAction_PlayOverrideAnim::idAction_PlayOverrideAnim
// DIA: idAction_PlayOverrideAnim::GetActionEventDef
// DIA: idAction_PlayOverrideAnimInterrupt::idAction_PlayOverrideAnimInterrupt
// DIA: idAction_PlayOverrideAnimInterrupt::GetActionEventDef
// DIA: idAction_LeapAttack::idAction_LeapAttack
// DIA: idAction_LeapAttack::GetActionEventDef
// DIA: idAction_DrawWeapon::idAction_DrawWeapon
// DIA: idAction_DrawWeapon::GetActionEventDef
// DIA: idAction_HolsterWeapon::idAction_HolsterWeapon
// DIA: idAction_HolsterWeapon::GetActionEventDef
// DIA: idAction_PerformCoverAction::idAction_PerformCoverAction
// DIA: idAction_PerformCoverAction::GetActionEventDef
// DIA: idAction_PerformCoverAction::Action_PerformCoverActionEnter
// DIA: `idAction_PerformCoverAction::Action_PerformCoverActionEnter'::`1'::dtor$0
// DIA: `idAction_PerformCoverAction::Action_PerformCoverActionEnter'::`1'::dtor$1
// DIA: idAction_WaitForPlayerInteraction::idAction_WaitForPlayerInteraction
// DIA: idAction_WaitForPlayerInteraction::GetActionEventDef
// DIA: idAction_WaitForAIVar::idAction_WaitForAIVar
// DIA: idAction_WaitForAIVar::GetActionEventDef
// DIA: idAction_PerfectMovement::idAction_PerfectMovement
// DIA: idAction_PerfectMovement::GetActionEventDef
// DIA: idAction_PerfectMovement::Action_PerfectMovementEnter
// DIA: idAction_SetAlertCycle::idAction_SetAlertCycle
// DIA: idAction_SetAlertCycle::GetActionEventDef
// DIA: idAction_SetSubWeb::idAction_SetSubWeb
// DIA: idAction_SetSubWeb::GetActionEventDef
// DIA: idAction_SetFocus::idAction_SetFocus
// DIA: idAction_SetFocus::GetActionEventDef
// DIA: idAction_ClearLookFocus::idAction_ClearLookFocus
// DIA: idAction_ClearLookFocus::GetActionEventDef
// DIA: idAction_ClearAimFocus::idAction_ClearAimFocus
// DIA: idAction_ClearAimFocus::GetActionEventDef
// DIA: idAction_SetAimPoint::idAction_SetAimPoint
// DIA: idAction_SetAimPoint::GetActionEventDef
// DIA: idAction_SetFireMode::idAction_SetFireMode
// DIA: idAction_SetFireMode::GetActionEventDef
// DIA: idAction_SetScriptAbort::idAction_SetScriptAbort
// DIA: idAction_SetScriptAbort::GetActionEventDef
// DIA: idAction_SetPosture::idAction_SetPosture
// DIA: idAction_SetPosture::GetActionEventDef
// DIA: idAction_SetWalkState::idAction_SetWalkState
// DIA: idAction_SetWalkState::GetActionEventDef
// DIA: idAction_SetSitState::idAction_SetSitState
// DIA: idAction_SetSitState::GetActionEventDef
// DIA: idAction_SetStandState::idAction_SetStandState
// DIA: idAction_SetStandState::GetActionEventDef
// DIA: idAction_SetIdleState::idAction_SetIdleState
// DIA: idAction_SetIdleState::GetActionEventDef
// DIA: idAction_SetIdleState::Action_SetIdleStateEnter
// DIA: idAction_IgnorePlayerApproach::idAction_IgnorePlayerApproach
// DIA: idAction_IgnorePlayerApproach::GetActionEventDef
// DIA: idAction_NoticePlayerApproach::idAction_NoticePlayerApproach
// DIA: idAction_NoticePlayerApproach::GetActionEventDef
// DIA: idAction_SetPlayerEnemy::idAction_SetPlayerEnemy
// DIA: idAction_SetPlayerEnemy::GetActionEventDef
// DIA: idAction_SetEnemy::idAction_SetEnemy
// DIA: idAction_SetEnemy::GetActionEventDef
// DIA: idAction_SearchToTarget::idAction_SearchToTarget
// DIA: idAction_SearchToTarget::GetActionEventDef
// DIA: idAction_SetPlayerFocus::idAction_SetPlayerFocus
// DIA: idAction_SetPlayerFocus::GetActionEventDef
// DIA: idAction_SetActionNodeGroup::idAction_SetActionNodeGroup
// DIA: idAction_SetActionNodeGroup::GetActionEventDef
// DIA: idAction_Trigger::idAction_Trigger
// DIA: idAction_Trigger::GetActionEventDef
// DIA: idAction_EnableDamage::idAction_EnableDamage
// DIA: idAction_EnableDamage::GetActionEventDef
// DIA: idAction_EnableAutoFocus::idAction_EnableAutoFocus
// DIA: idAction_EnableAutoFocus::GetActionEventDef
// DIA: idAction_EnableBodyRotation::idAction_EnableBodyRotation
// DIA: idAction_EnableBodyRotation::GetActionEventDef
// DIA: idAction_EnableWalkIK::idAction_EnableWalkIK
// DIA: idAction_EnableWalkIK::GetActionEventDef
// DIA: idAction_EnableHeadTracking::idAction_EnableHeadTracking
// DIA: idAction_EnableHeadTracking::GetActionEventDef
// DIA: idAction_ForceAwarenessByDistance::idAction_ForceAwarenessByDistance
// DIA: idAction_ForceAwarenessByDistance::GetActionEventDef
// DIA: idAction_SetAIVar::idAction_SetAIVar
// DIA: idAction_SetAIVar::GetActionEventDef
// DIA: idAction_SetScriptFlag::idAction_SetScriptFlag
// DIA: idAction_SetScriptFlag::GetActionEventDef
// DIA: idAction_ClearScriptFlag::idAction_ClearScriptFlag
// DIA: idAction_ClearScriptFlag::GetActionEventDef
// DIA: idAction_SetPerceptionFlag::idAction_SetPerceptionFlag
// DIA: idAction_SetPerceptionFlag::GetActionEventDef
// DIA: idAction_ClearPerceptionFlag::idAction_ClearPerceptionFlag
// DIA: idAction_ClearPerceptionFlag::GetActionEventDef
// DIA: idAction_GiveItem::idAction_GiveItem
// DIA: idAction_GiveItem::GetActionEventDef
// DIA: idAction_TakeItem::idAction_TakeItem
// DIA: idAction_TakeItem::GetActionEventDef
// DIA: idAction_ClearWorldState::idAction_ClearWorldState
// DIA: idAction_ClearWorldState::GetActionEventDef
// DIA: idAction_ShowAttachment::idAction_ShowAttachment
// DIA: idAction_ShowAttachment::GetActionEventDef
// DIA: idAction_HideAttachment::idAction_HideAttachment
// DIA: idAction_HideAttachment::GetActionEventDef
// DIA: idAction_DropAttachment::idAction_DropAttachment
// DIA: idAction_DropAttachment::GetActionEventDef
// DIA: idAction_SetMoveMode::idAction_SetMoveMode
// DIA: idAction_SetMoveMode::GetActionEventDef
// DIA: idAction_SetAccuracy::idAction_SetAccuracy
// DIA: idAction_SetAccuracy::GetActionEventDef
// DIA: idAction_ScriptedAnimWeb::idAction_ScriptedAnimWeb
// DIA: idAction_ScriptedAnimWeb::GetActionEventDef
// DIA: idAction_ScriptedAnimWeb::AIAction_ExitState
// DIA: idAIActionsQueue::StartNextAction
// DIA: idAIActionFSM::StartQueuedActions
// DIA: idAIActionManager::GetActionStateForEvent
// DIA: idAIActionManager::InternalCallEvent
// DIA: `idAIActionManager::InternalCallEvent'::`1'::dtor$1
// DIA: idAIActionManager::InternalRespondsTo
// DIA: StartIdleAnimationForMoveNoFail
// DIA: UpdateMoveNoFail
// DIA: UpdateMoveToPathPoint
// DIA: ActionEntityDeleted::Evaluate
// DIA: idAction_MoveToPointNoFail::AIAction_Work
// DIA: idAction_MoveToPathPoint::AIAction_Work
// DIA: idAction_MoveToPathPointNoFail::AIAction_Work
// DIA: idAction_MoveToCover::AIAction_Work
// DIA: `idAction_MoveToCover::AIAction_Work'::`1'::dtor$0
// DIA: idAction_Idle::AIAction_Work
// DIA: `idAction_Idle::AIAction_Work'::`1'::dtor$0
// DIA: `idAction_Idle::AIAction_Work'::`1'::dtor$3
// DIA: `idAction_Idle::AIAction_Work'::`1'::dtor$1
// DIA: `idAction_Idle::AIAction_Work'::`1'::dtor$2
// DIA: idAction_Melee::Action_MeleeEnter
// DIA: idAction_PlayInteractionVoiceOver::Action_PlayInteractionVoiceOverEnter
// DIA: idAction_DrawWeapon::Action_DrawWeaponEnter
// DIA: InteractionTriggered::Evaluate
// DIA: idAction_ScriptedAnimWeb::StartNextState
// DIA: `idAction_ScriptedAnimWeb::StartNextState'::`1'::dtor$1
// DIA: `idAction_ScriptedAnimWeb::StartNextState'::`1'::dtor$2
// DIA: `idAction_ScriptedAnimWeb::StartNextState'::`1'::dtor$3
// DIA: `idAction_ScriptedAnimWeb::StartNextState'::`1'::dtor$4
// DIA: idAction_ScriptedAnimWeb::AIAction_Work
// DIA: idAIActionsQueue::Clear
// DIA: idAIActionManager::GetActionBlocks
// DIA: idAIActionManager::GetActionStopsMovement
// DIA: idList<idEventArgs,92>::Resize
// DIA: idAction_MoveToPoint::Action_MoveToPointEnter
// DIA: idAction_MoveToPointNoFail::Action_MoveToPointNoFailEnter
// DIA: idAction_MoveToEntity::Action_MoveToEntityEnter
// DIA: idAction_MoveToEntityNoFail::Action_MoveToEntityNoFailEnter
// DIA: idAction_MoveToPathPoint::Action_MoveToPathPointEnter
// DIA: idAction_MoveToPathPointNoFail::Action_MoveToPathPointNoFailEnter
// DIA: idAction_MoveToCover::Action_MoveToCoverEnter
// DIA: idAction_UseZipline::Action_UseZiplineEnter
// DIA: idAction_Pain::AIAction_ExitState
// DIA: idAction_Dodge::AIAction_ExitState
// DIA: idAction_Melee::AIAction_ExitState
// DIA: idAction_TurnToEntity::Action_TurnToEntityEnter
// DIA: idAction_TurnToEntityWithOffset::Action_TurnToEntityWithOffsetEnter
// DIA: idAction_WaitForEntity::Action_WaitForEntityEnter
// DIA: idAction_PlayOverrideAnim::AIAction_ExitState
// DIA: idAction_LeapAttack::AIAction_ExitState
// DIA: idAction_WaitForPlayerInteraction::AIAction_ExitState
// DIA: idAction_PerfectMovement::AIAction_ExitState
// DIA: idAction_ScriptedAnimWeb::Action_ScriptedAnimWebEnter
// DIA: idAIActionFSM::SetNextState
// DIA: idAIActionFSM::SetState
// DIA: idList<idEventArgs,92>::Append
// DIA: idAIActionsQueue::InternalCallEvent
// DIA: `idAIActionsQueue::InternalCallEvent'::`1'::dtor$0
// DIA: `idAIActionsQueue::InternalCallEvent'::`1'::dtor$2
// DIA: idAIActionFSM::idAIActionFSM
// DIA: `idAIActionFSM::idAIActionFSM'::`1'::dtor$0
// DIA: `idAIActionFSM::idAIActionFSM'::`1'::dtor$1
// DIA: idLegsFSM::idLegsFSM
// DIA: idAIActionManager::idAIActionManager
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$0
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$1
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$2
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$3
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$4
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$5
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$6
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$7
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$8
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$9
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$10
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$11
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$12
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$13
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$14
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$15
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$16
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$17
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$18
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$19
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$20
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$21
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$22
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$23
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$24
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$25
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$26
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$27
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$28
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$29
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$30
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$31
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$32
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$33
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$34
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$35
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$36
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$37
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$38
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$39
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$40
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$41
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$42
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$43
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$44
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$45
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$46
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$47
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$48
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$49
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$50
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$51
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$52
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$53
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$54
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$55
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$56
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$57
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$58
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$59
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$60
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$61
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$62
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$63
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$64
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$65
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$66
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$67
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$68
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$69
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$70
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$71
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$72
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$73
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$74
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$75
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$76
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$77
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$78
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$79
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$80
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$81
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$82
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$83
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$84
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$85
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$86
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$87
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$88
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$89
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$90
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$91
// DIA: `idAIActionManager::idAIActionManager'::`1'::dtor$92
// DIA: idAIActionManager::Init
// DIA: idAIActionFSM::`scalar deleting destructor'
// DIA: `idAIActionFSM::`scalar deleting destructor''::`1'::dtor$0
// DIA: idAIActionManager::`scalar deleting destructor'
// DIA: `dynamic initializer for 'ai_debugActionScript''
// DIA: `dynamic initializer for 'ai_clearEvents''
// DIA: `dynamic initializer for 'ai_playInteractionVoiceOver_defaultLeaveDistance''
// DIA: `dynamic initializer for 'ai_coverSnapRadius''
// DIA: `dynamic initializer for 'ai_coverLerpRadius''
// DIA: `dynamic initializer for 'ai_coverAbortRadius''
// DIA: `dynamic atexit destructor for 'ai_debugActionScript''
// DIA: `dynamic atexit destructor for 'ai_clearEvents''
// DIA: `dynamic atexit destructor for 'ai_playInteractionVoiceOver_defaultLeaveDistance''
// DIA: `dynamic atexit destructor for 'ai_coverSnapRadius''
// DIA: `dynamic atexit destructor for 'ai_coverLerpRadius''
// DIA: `dynamic atexit destructor for 'ai_coverAbortRadius''
