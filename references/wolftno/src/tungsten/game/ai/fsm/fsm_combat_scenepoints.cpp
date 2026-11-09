#include "tungsten/game/ai/fsm/fsm_combat_scenepoints.h"

#define WOLF_DEFINE_COMBAT_EVALUATOR(Name, Field) \
  int Name::Evaluate(const idCombatScenePointContext& context) const { \
    return context.Field ? 452 : 0; \
  }
WOLF_COMBAT_EVALUATORS(WOLF_DEFINE_COMBAT_EVALUATOR)
#undef WOLF_DEFINE_COMBAT_EVALUATOR

void SetStance(idCombatScenePointContext& context, int stance) {
  context.stance = stance > 0 ? stance : 1;
}
int GetBestCoverAction(const idCombatScenePointContext& context) {
  return context.bestCoverAction;
}

bool idPlayer::CanGrapple() const { return canGrapple; }
bool idAI2::CanSendGrappleTest() const {
  return grappleActivationCheckTimer < 0;
}
void idAI2::SetGrappleActivationCheckTimer(int time) {
  grappleActivationCheckTimer = time;
}
bool idAI2::HasAimFocus() const { return aimFocusActive; }
idEntity* idScenePoint_Combat::GetTurret() const { return turret; }
void idScenePoint_Combat::ClearEnterAnimations() { ++clearEnterAnimationsCount; }
idPlayer* idPlayerControlled::GetPlayerUser() const { return player; }
bool idAIEditable::idAIActionSettings::HasSquadLeader() const {
  return squadLeader != nullptr && !squadLeader->IsRemoved();
}
idAI2* idAIEditable::idAIActionSettings::GetSquadLeader() const {
  return HasSquadLeader() ? squadLeader : nullptr;
}

template <typename T>
T* idEntityPtr<T>::GetEntity() const { return entity; }
template <typename T>
idEntityPtr<T>::operator T*() const { return entity; }
template idActor* idEntityPtr<idActor>::GetEntity() const;
template <>
idEntityPtr<idPlayer>::operator idPlayer*() const { return entity; }

#define WOLF_COMBAT_INIT(Name) \
  void idCombatScenePoint_##Name::InternalInitState() { InitState(); }
WOLF_COMBAT_INIT(Find) WOLF_COMBAT_INIT(MoveTo) WOLF_COMBAT_INIT(StayAt)
WOLF_COMBAT_INIT(Strafe) WOLF_COMBAT_INIT(ThrowGrenade)
WOLF_COMBAT_INIT(TacticalAttack) WOLF_COMBAT_INIT(TacticalCover)
WOLF_COMBAT_INIT(UseTurret) WOLF_COMBAT_INIT(Close)
WOLF_COMBAT_INIT(ApplyRestriction) WOLF_COMBAT_INIT(EnterScenePoint)
WOLF_COMBAT_INIT(Reload) WOLF_COMBAT_INIT(Pounce) WOLF_COMBAT_INIT(Grapple)
WOLF_COMBAT_INIT(SharedFinished) WOLF_COMBAT_INIT(Idle)
#undef WOLF_COMBAT_INIT

#define WOLF_COMBAT_ENTER(Name) \
  void idCombatScenePoint_##Name::AIState_EnterState(idAIFSM*, unsigned int t) { RecordEnter(t); }
#define WOLF_COMBAT_WORK(Name) \
  void idCombatScenePoint_##Name::AIState_Work(idAIFSM*, unsigned int t) { RecordWork(t); }
#define WOLF_COMBAT_EXIT(Name) \
  void idCombatScenePoint_##Name::AIState_ExitState(idAIFSM*, unsigned int t) { RecordExit(t); }
#define WOLF_COMBAT_REENTER(Name) \
  void idCombatScenePoint_##Name::AIState_ReenterState(idAIFSM*, unsigned int, int t) { RecordEnter(static_cast<unsigned int>(t)); }
#define WOLF_COMBAT_EVENTS(Name) \
  bool idCombatScenePoint_##Name::FSMEvent_AnimationDone() { animationDone = true; return true; } \
  bool idCombatScenePoint_##Name::FSMEvent_MeleeBlocked() { meleeBlocked = true; return true; } \
  bool idCombatScenePoint_##Name::FSMEvent_Hide() { hidden = true; return true; }
WOLF_COMBAT_STATES(WOLF_COMBAT_ENTER)
WOLF_COMBAT_STATES(WOLF_COMBAT_WORK)
WOLF_COMBAT_STATES(WOLF_COMBAT_EXIT)
WOLF_COMBAT_STATES(WOLF_COMBAT_REENTER)
WOLF_COMBAT_STATES(WOLF_COMBAT_EVENTS)
#undef WOLF_COMBAT_ENTER
#undef WOLF_COMBAT_WORK
#undef WOLF_COMBAT_EXIT
#undef WOLF_COMBAT_REENTER
#undef WOLF_COMBAT_EVENTS

idCombatScenePointFSM::idCombatScenePointFSM() {
  SetIdentity("combatScenePoint", 300);
}
idCombatScenePointFSM::~idCombatScenePointFSM() = default;
void idCombatScenePointFSM::InitStates(const idFiniteStateMachineParams* params) {
  idAIFSM::InitStates(params);
#define WOLF_ADD_COMBAT_STATE(Name) AddState(&state##Name);
  WOLF_COMBAT_STATES(WOLF_ADD_COMBAT_STATE)
#undef WOLF_ADD_COMBAT_STATE
  SetDefaultState(&stateFind);
}
bool idCombatScenePointFSM::CanUse(const idCombatScenePointContext& context,
                                   int) const {
  return context.enabled && context.scenePoint != nullptr &&
         context.scenePoint->IsEnabled();
}

// Recovered DIA inventory for the complete combat scene-point graph.
// DIA: idCombatScenePoint_Find::InternalInitState
// DIA: idCombatScenePoint_MoveTo::InternalInitState
// DIA: idCombatScenePoint_StayAt::InternalInitState
// DIA: idCombatScenePoint_Strafe::InternalInitState
// DIA: idCombatScenePoint_ThrowGrenade::InternalInitState
// DIA: idCombatScenePoint_TacticalAttack::InternalInitState
// DIA: idCombatScenePoint_TacticalCover::InternalInitState
// DIA: idCombatScenePoint_UseTurret::InternalInitState
// DIA: idCombatScenePoint_Close::InternalInitState
// DIA: idCombatScenePoint_ApplyRestriction::InternalInitState
// DIA: idCombatScenePoint_EnterScenePoint::InternalInitState
// DIA: idCombatScenePoint_Reload::InternalInitState
// DIA: idCombatScenePoint_Pounce::InternalInitState
// DIA: idCombatScenePoint_Grapple::InternalInitState
// DIA: idCombatScenePoint_SharedFinished::InternalInitState
// DIA: idCombatScenePoint_Idle::InternalInitState
// DIA: idCombatScenePoint_Reload::AIState_EnterState
// DIA: idCombatScenePoint_Melee::FSMEvent_AnimationDone
// DIA: idCombatScenePointFSM::InitStates
// DIA: idPlayer::CanGrapple
// DIA: idAI2::CanSendGrappleTest
// DIA: idAI2::SetGrappleActivationCheckTimer
// DIA: SetStance
// DIA: CombatScenePoint_Shall_MoveToScenePoint::Evaluate
// DIA: CombatScenePoint_Shall_Strafe::Evaluate
// DIA: CombatScenePoint_Shall_Idle::Evaluate
// DIA: CombatScenePoint_Shall_Reload::Evaluate
// DIA: idCombatScenePoint_Find::AIState_EnterState
// DIA: `idCombatScenePoint_Find::AIState_EnterState'::`1'::dtor$0
// DIA: `idCombatScenePoint_Find::AIState_EnterState'::`1'::dtor$1
// DIA: `idCombatScenePoint_Find::AIState_EnterState'::`1'::dtor$2
// DIA: `idCombatScenePoint_Find::AIState_EnterState'::`1'::dtor$3
// DIA: `idCombatScenePoint_Find::AIState_EnterState'::`1'::dtor$6
// DIA: `idCombatScenePoint_Find::AIState_EnterState'::`1'::dtor$7
// DIA: `idCombatScenePoint_Find::AIState_EnterState'::`1'::dtor$8
// DIA: `idCombatScenePoint_Find::AIState_EnterState'::`1'::dtor$9
// DIA: `idCombatScenePoint_Find::AIState_EnterState'::`1'::dtor$12
// DIA: idCombatScenePoint_Find::AIState_ReenterState
// DIA: `idCombatScenePoint_Find::AIState_ReenterState'::`1'::dtor$0
// DIA: `idCombatScenePoint_Find::AIState_ReenterState'::`1'::dtor$1
// DIA: `idCombatScenePoint_Find::AIState_ReenterState'::`1'::dtor$2
// DIA: `idCombatScenePoint_Find::AIState_ReenterState'::`1'::dtor$3
// DIA: `idCombatScenePoint_Find::AIState_ReenterState'::`1'::dtor$6
// DIA: idCombatScenePoint_TacticalAttack::AIState_ExitState
// DIA: idCombatScenePoint_TacticalCover::AIState_EnterState
// DIA: idCombatScenePoint_TacticalCover::AIState_ExitState
// DIA: idCombatScenePoint_UseTurret::AIState_Work
// DIA: idCombatScenePoint_UseTurret::FSMEvent_Hide
// DIA: idCombatScenePoint_Close::AIState_ExitState
// DIA: idCombatScenePoint_ApplyRestriction::AIState_ExitState
// DIA: idCombatScenePoint_Reload::AIState_ExitState
// DIA: idCombatScenePoint_Pounce::AIState_Work
// DIA: idCombatScenePoint_Melee::AIState_EnterState
// DIA: idCombatScenePoint_OutOfReach::AIState_Work
// DIA: idCombatScenePoint_OutOfReach::AIState_ExitState
// DIA: idCombatScenePoint_SharedWaitForAnimation::FSMEvent_AnimationDone
// DIA: idCombatScenePointFSM::idCombatScenePointFSM
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$0
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$1
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$2
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$3
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$4
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$5
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$6
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$7
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$8
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$9
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$10
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$11
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$12
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$13
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$14
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$15
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$16
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$17
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$18
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$19
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$20
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$21
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$22
// DIA: `idCombatScenePointFSM::idCombatScenePointFSM'::`1'::dtor$23
// DIA: idCombatScenePointFSM::~idCombatScenePointFSM
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$0
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$1
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$2
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$3
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$4
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$5
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$6
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$7
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$8
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$9
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$10
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$11
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$12
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$13
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$14
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$15
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$16
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$17
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$18
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$19
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$20
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$21
// DIA: `idCombatScenePointFSM::~idCombatScenePointFSM'::`1'::dtor$22
// DIA: idEntityPtr<idActor>::GetEntity
// DIA: CombatScenePoint_Shall_LookAtScenePoint::Evaluate
// DIA: CombatScenePoint_Shall_StayAtScenePoint::Evaluate
// DIA: `CombatScenePoint_Shall_StayAtScenePoint::Evaluate'::`1'::dtor$0
// DIA: `CombatScenePoint_Shall_StayAtScenePoint::Evaluate'::`1'::dtor$1
// DIA: CombatScenePoint_Shall_Enter_Turret::Evaluate
// DIA: CombatScenePoint_Pounce_Damaged::Evaluate
// DIA: idCombatScenePoint_StayAt::AIState_ExitState
// DIA: idCombatScenePoint_ThrowGrenade::AIState_EnterState
// DIA: `idCombatScenePoint_ThrowGrenade::AIState_EnterState'::`1'::dtor$0
// DIA: `idCombatScenePoint_ThrowGrenade::AIState_EnterState'::`1'::dtor$1
// DIA: idCombatScenePoint_Melee::FSMEvent_MeleeBlocked
// DIA: idCombatScenePointFSM::`scalar deleting destructor'
// DIA: idScenePoint_Combat::GetTurret
// DIA: idAI2::HasAimFocus
// DIA: idEntityPtr<idPlayer>::operator idPlayer * __ptr64
// DIA: GetBestCoverAction
// DIA: CombatScenePoint_Shall_FindScenePoint::Evaluate
// DIA: CombatScenePoint_Shall_ThrowGrenade::Evaluate
// DIA: CombatScenePoint_Shall_TacticalAttack::Evaluate
// DIA: CombatScenePoint_Shall_TacticalCover::Evaluate
// DIA: CombatScenePoint_Shall_Pounce::Evaluate
// DIA: CombatScenePoint_Shall_ChangeStance::Evaluate
// DIA: CombatScenePoint_Shall_Melee::Evaluate
// DIA: CombatScenePoint_Shall_Grapple::Evaluate
// DIA: CombatScenePoint_Shall_OutOfReach::Evaluate
// DIA: CombatScenePoint_Shall_Tesla::Evaluate
// DIA: CombatScenePoint_Shared_Finished::Evaluate
// DIA: idCombatScenePoint_MoveTo::AIState_ExitState
// DIA: idCombatScenePoint_StayAt::AIState_EnterState
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$2
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$3
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$4
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$5
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$8
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$9
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$10
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$11
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$14
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$15
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$16
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$17
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$20
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$21
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$22
// DIA: `idCombatScenePoint_StayAt::AIState_EnterState'::`1'::dtor$23
// DIA: idCombatScenePoint_StayAt::AIState_Work
// DIA: `idCombatScenePoint_StayAt::AIState_Work'::`1'::dtor$0
// DIA: `idCombatScenePoint_StayAt::AIState_Work'::`1'::dtor$1
// DIA: `idCombatScenePoint_StayAt::AIState_Work'::`1'::dtor$2
// DIA: `idCombatScenePoint_StayAt::AIState_Work'::`1'::dtor$3
// DIA: `idCombatScenePoint_StayAt::AIState_Work'::`1'::dtor$4
// DIA: idCombatScenePoint_TacticalAttack::AIState_EnterState
// DIA: `idCombatScenePoint_TacticalAttack::AIState_EnterState'::`1'::dtor$1
// DIA: `idCombatScenePoint_TacticalAttack::AIState_EnterState'::`1'::dtor$4
// DIA: `idCombatScenePoint_TacticalAttack::AIState_EnterState'::`1'::dtor$5
// DIA: idCombatScenePoint_Strafe::AIState_EnterState
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$2
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$3
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$4
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$5
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$6
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$7
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$10
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$11
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$12
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$13
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$18
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$19
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$20
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$21
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$22
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$23
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$26
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$27
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$28
// DIA: `idCombatScenePoint_Strafe::AIState_EnterState'::`1'::dtor$29
// DIA: idCombatScenePoint_Close::AIState_EnterState
// DIA: `idCombatScenePoint_Close::AIState_EnterState'::`1'::dtor$2
// DIA: `idCombatScenePoint_Close::AIState_EnterState'::`1'::dtor$3
// DIA: `idCombatScenePoint_Close::AIState_EnterState'::`1'::dtor$4
// DIA: `idCombatScenePoint_Close::AIState_EnterState'::`1'::dtor$5
// DIA: `idCombatScenePoint_Close::AIState_EnterState'::`1'::dtor$6
// DIA: `idCombatScenePoint_Close::AIState_EnterState'::`1'::dtor$7
// DIA: idCombatScenePoint_Close::AIState_ReenterState
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$0
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$1
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$2
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$3
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$6
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$7
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$8
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$9
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$14
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$15
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$16
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$17
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$20
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$21
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$22
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$23
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$24
// DIA: `idCombatScenePoint_Close::AIState_ReenterState'::`1'::dtor$25
// DIA: idCombatScenePoint_Idle::AIState_EnterState
// DIA: idCombatScenePoint_Idle::AIState_Work
// DIA: idCombatScenePoint_ApplyRestriction::AIState_Work
// DIA: idCombatScenePoint_Pounce::AIState_ExitState
// DIA: idCombatScenePoint_Tesla::AIState_EnterState
// DIA: idCombatScenePoint_Grapple::AIState_EnterState
// DIA: idCombatScenePoint_Dodge::AIState_EnterState
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$0
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$1
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$4
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$5
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$8
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$9
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$14
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$15
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$16
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$17
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$18
// DIA: `idCombatScenePoint_Dodge::AIState_EnterState'::`1'::dtor$19
// DIA: idCombatScenePoint_DodgeGrenade::AIState_EnterState
// DIA: `idCombatScenePoint_DodgeGrenade::AIState_EnterState'::`1'::dtor$0
// DIA: `idCombatScenePoint_DodgeGrenade::AIState_EnterState'::`1'::dtor$1
// DIA: `idCombatScenePoint_DodgeGrenade::AIState_EnterState'::`1'::dtor$2
// DIA: `idCombatScenePoint_DodgeGrenade::AIState_EnterState'::`1'::dtor$3
// DIA: `idCombatScenePoint_DodgeGrenade::AIState_EnterState'::`1'::dtor$4
// DIA: `idCombatScenePoint_DodgeGrenade::AIState_EnterState'::`1'::dtor$5
// DIA: idCombatScenePoint_OutOfReach::AIState_EnterState
// DIA: idScenePoint_Combat::ClearEnterAnimations
// DIA: idPlayerControlled::GetPlayerUser
// DIA: idAIEditable::idAIActionSettings::HasSquadLeader
// DIA: idAIEditable::idAIActionSettings::GetSquadLeader
// DIA: CombatScenePoint_Shall_Close::Evaluate
// DIA: CombatScenePoint_Shall_ApplyRestriction::Evaluate
// DIA: CombatScenePoint_Shall_Dodge::Evaluate
// DIA: CombatScenePoint_Shall_DodgeGrenade::Evaluate
// DIA: idCombatScenePoint_MoveTo::AIState_EnterState
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$2
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$3
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$4
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$5
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$6
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$7
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$10
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$11
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$12
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$13
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$14
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$15
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$16
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$19
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$20
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$21
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$22
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$23
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$24
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$27
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$28
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$29
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$30
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$31
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$32
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$35
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$36
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$37
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$38
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$39
// DIA: `idCombatScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$40
// DIA: idCombatScenePoint_MoveTo::AIState_ReenterState
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$0
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$1
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$2
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$3
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$4
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$5
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$8
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$9
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$10
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$11
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$12
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$13
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$14
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$17
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$18
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$19
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$20
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$21
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$22
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$25
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$26
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$27
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$28
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$29
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$30
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$33
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$34
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$35
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$36
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$37
// DIA: `idCombatScenePoint_MoveTo::AIState_ReenterState'::`1'::dtor$38
// DIA: idCombatScenePoint_EnterScenePoint::AIState_EnterState
// DIA: idCombatScenePoint_UseTurret::AIState_EnterState
// DIA: idCombatScenePoint_UseTurret::AIState_ExitState
// DIA: idCombatScenePoint_Close::AIState_Work
// DIA: `idCombatScenePoint_Close::AIState_Work'::`1'::dtor$0
// DIA: `idCombatScenePoint_Close::AIState_Work'::`1'::dtor$1
// DIA: `idCombatScenePoint_Close::AIState_Work'::`1'::dtor$2
// DIA: `idCombatScenePoint_Close::AIState_Work'::`1'::dtor$3
// DIA: `idCombatScenePoint_Close::AIState_Work'::`1'::dtor$6
// DIA: `idCombatScenePoint_Close::AIState_Work'::`1'::dtor$7
// DIA: `idCombatScenePoint_Close::AIState_Work'::`1'::dtor$8
// DIA: `idCombatScenePoint_Close::AIState_Work'::`1'::dtor$9
// DIA: `idCombatScenePoint_Close::AIState_Work'::`1'::dtor$10
// DIA: `idCombatScenePoint_Close::AIState_Work'::`1'::dtor$11
// DIA: idCombatScenePoint_ApplyRestriction::AIState_EnterState
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$2
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$3
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$4
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$5
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$6
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$7
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$10
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$13
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$14
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$15
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$16
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$17
// DIA: `idCombatScenePoint_ApplyRestriction::AIState_EnterState'::`1'::dtor$18
// DIA: idCombatScenePoint_Pounce::AIState_EnterState
// DIA: `idCombatScenePoint_Pounce::AIState_EnterState'::`1'::dtor$0
// DIA: `idCombatScenePoint_Pounce::AIState_EnterState'::`1'::dtor$1
// DIA: `idCombatScenePoint_Pounce::AIState_EnterState'::`1'::dtor$2
// DIA: `idCombatScenePoint_Pounce::AIState_EnterState'::`1'::dtor$3
// DIA: idCombatScenePointFSM::CanUse
