#include "tungsten/game/ai/fsm/fsm_alertcycle.h"

#define WOLF_DEFINE_ALERT_EVALUATOR(Name, Field) \
  int Name::Evaluate(const idAlertCycleContext& context) const { \
    return context.Field ? 452 : 0; \
  }
WOLF_ALERT_EVALUATORS(WOLF_DEFINE_ALERT_EVALUATOR)
#undef WOLF_DEFINE_ALERT_EVALUATOR

int AC_NoActionScript::Evaluate(const idAlertCycleContext& context) const {
  return !context.hasActionScript ? 452 : 0;
}
bool AC_InPain(const idAlertCycleContext& context) { return context.inPain; }
bool FSM_SetupReaction(idAlertCycleContext& context, unsigned int reaction,
                       int) {
  if (reaction == 0 || (context.forbiddenReactions & reaction) != 0) return false;
  context.pendingReactions |= reaction;
  return true;
}

#define WOLF_ALERT_INIT(Name) \
  void idAlertCycle_##Name::InternalInitState() { InitState(); }
WOLF_ALERT_INIT(FSMReaction)
WOLF_ALERT_INIT(Default)
WOLF_ALERT_INIT(ExecuteDoomscript)
WOLF_ALERT_INIT(ExecuteCodeMoveAction)
WOLF_ALERT_INIT(ActionScript)
WOLF_ALERT_INIT(EquipWeapon)
WOLF_ALERT_INIT(CombatScenePoint)
WOLF_ALERT_INIT(RoamScenePoint)
WOLF_ALERT_INIT(BackStand)
WOLF_ALERT_INIT(Pain)
WOLF_ALERT_INIT(Dying)
WOLF_ALERT_INIT(Stuck)
WOLF_ALERT_INIT(UnhandledError)
#undef WOLF_ALERT_INIT

#define WOLF_ALERT_ENTER(Name) \
  void idAlertCycle_##Name::AIState_EnterState(idAIFSM*, unsigned int time) { \
    RecordEnter(time); \
  }
WOLF_ALERT_ENTER(FSMReaction)
WOLF_ALERT_ENTER(Default)
WOLF_ALERT_ENTER(ActionScript)
WOLF_ALERT_ENTER(EquipWeapon)
WOLF_ALERT_ENTER(CombatScenePoint)
WOLF_ALERT_ENTER(SearchScenePoint)
WOLF_ALERT_ENTER(RoamScenePoint)
WOLF_ALERT_ENTER(Pain)
WOLF_ALERT_ENTER(Stuck)
WOLF_ALERT_ENTER(UnhandledError)
WOLF_ALERT_ENTER(Dying)
WOLF_ALERT_ENTER(Dead)
#undef WOLF_ALERT_ENTER

void idAlertCycle_ExecuteDoomscript::AIState_EnterState(idAIFSM*, unsigned int time) { RecordEnter(time); }
void idAlertCycle_ExecuteCodeMoveAction::AIState_EnterState(idAIFSM*, unsigned int time) { RecordEnter(time); }
void idAlertCycle_BackStand::AIState_EnterState(idAIFSM*, unsigned int time) { RecordEnter(time); }

void idAlertCycle_EquipWeapon::AIState_Work(idAIFSM*, unsigned int time) { RecordWork(time); }
void idAlertCycle_Pain::AIState_Work(idAIFSM*, unsigned int time) { RecordWork(time); }
void idAlertCycle_Stuck::AIState_Work(idAIFSM*, unsigned int time) { RecordWork(time); }
#define WOLF_ALERT_DEFAULT_WORK(Name) \
  void idAlertCycle_##Name::AIState_Work(idAIFSM*, unsigned int time) { RecordWork(time); }
WOLF_ALERT_DEFAULT_WORK(FSMReaction)
WOLF_ALERT_DEFAULT_WORK(Default)
WOLF_ALERT_DEFAULT_WORK(ExecuteDoomscript)
WOLF_ALERT_DEFAULT_WORK(ExecuteCodeMoveAction)
WOLF_ALERT_DEFAULT_WORK(ActionScript)
WOLF_ALERT_DEFAULT_WORK(CombatScenePoint)
WOLF_ALERT_DEFAULT_WORK(SearchScenePoint)
WOLF_ALERT_DEFAULT_WORK(RoamScenePoint)
WOLF_ALERT_DEFAULT_WORK(BackStand)
WOLF_ALERT_DEFAULT_WORK(Dying)
WOLF_ALERT_DEFAULT_WORK(UnhandledError)
WOLF_ALERT_DEFAULT_WORK(Dead)
#undef WOLF_ALERT_DEFAULT_WORK
void idAlertCycle_ActionScript::AIState_ExitState(idAIFSM*, unsigned int time) { RecordExit(time); }
void idAlertCycle_Dying::AIState_ExitState(idAIFSM*, unsigned int time) { RecordExit(time); }
void idAlertCycle_Pain::AIState_ExitState(idAIFSM*, unsigned int time) { RecordExit(time); }
#define WOLF_ALERT_DEFAULT_EXIT(Name) \
  void idAlertCycle_##Name::AIState_ExitState(idAIFSM*, unsigned int time) { RecordExit(time); }
WOLF_ALERT_DEFAULT_EXIT(FSMReaction)
WOLF_ALERT_DEFAULT_EXIT(Default)
WOLF_ALERT_DEFAULT_EXIT(ExecuteDoomscript)
WOLF_ALERT_DEFAULT_EXIT(ExecuteCodeMoveAction)
WOLF_ALERT_DEFAULT_EXIT(EquipWeapon)
WOLF_ALERT_DEFAULT_EXIT(CombatScenePoint)
WOLF_ALERT_DEFAULT_EXIT(SearchScenePoint)
WOLF_ALERT_DEFAULT_EXIT(RoamScenePoint)
WOLF_ALERT_DEFAULT_EXIT(BackStand)
WOLF_ALERT_DEFAULT_EXIT(Stuck)
WOLF_ALERT_DEFAULT_EXIT(UnhandledError)
WOLF_ALERT_DEFAULT_EXIT(Dead)
#undef WOLF_ALERT_DEFAULT_EXIT

idAlertCycleFSM::idAlertCycleFSM() { SetIdentity("alertCycle", 100); }
idAlertCycleFSM::~idAlertCycleFSM() = default;

void idAlertCycleFSM::InitStates(const idFiniteStateMachineParams* params) {
  idAIFSM::InitStates(params);
#define WOLF_ADD_ALERT_STATE(Name) AddState(&state##Name);
  WOLF_ALERT_STATES(WOLF_ADD_ALERT_STATE)
#undef WOLF_ADD_ALERT_STATE
  SetDefaultState(&stateDefault);
}

const idInfoPath* idAIMemory::GetCurrentPath() const { return currentPath; }

idCVar ai_showDeathInfo("ai_showDeathInfo", "0", 2,
                        "show AI death state information");
idCVar ai_painGoreTest("ai_painGoreTest", "0", 1,
                       "force pain gore transition testing");

// Recovered DIA inventory for the complete alert-cycle state graph.
// DIA: idAlertCycle_FSMReaction::InternalInitState
// DIA: AC_ExecutingScript::Evaluate
// DIA: AC_UnhandledError::Evaluate
// DIA: AC_IsRelaxed::Evaluate
// DIA: idAlertCycle_Default::InternalInitState
// DIA: idAlertCycle_Default::AIState_EnterState
// DIA: AC_DoomscriptDone::Evaluate
// DIA: idAlertCycle_ExecuteDoomscript::InternalInitState
// DIA: idAlertCycle_ExecuteCodeMoveAction::InternalInitState
// DIA: idAlertCycle_ActionScript::InternalInitState
// DIA: idAlertCycle_ActionScript::AIState_ExitState
// DIA: idAlertCycle_EquipWeapon::InternalInitState
// DIA: AC_HasEquippedWeapon::Evaluate
// DIA: idAlertCycle_CombatScenePoint::InternalInitState
// DIA: idAlertCycle_CombatScenePoint::AIState_EnterState
// DIA: idAlertCycle_RoamScenePoint::InternalInitState
// DIA: idAlertCycle_RoamScenePoint::AIState_EnterState
// DIA: idAlertCycle_BackStand::InternalInitState
// DIA: idAlertCycle_Pain::InternalInitState
// DIA: idAlertCycle_Dying::InternalInitState
// DIA: idAlertCycle_Stuck::InternalInitState
// DIA: AC_Restart::Evaluate
// DIA: idAlertCycle_UnhandledError::InternalInitState
// DIA: idAlertCycleFSM::InitStates
// DIA: idAlertCycle_FSMReaction::AIState_EnterState
// DIA: AC_CanUseBackstand::Evaluate
// DIA: AC_InPain
// DIA: AC_NoActionScript::Evaluate
// DIA: idAlertCycle_ActionScript::AIState_EnterState
// DIA: idAlertCycle_EquipWeapon::AIState_EnterState
// DIA: idAlertCycle_SearchScenePoint::AIState_EnterState
// DIA: AC_PainFinished::Evaluate
// DIA: idAlertCycle_Pain::AIState_Work
// DIA: AC_Unstuck::Evaluate
// DIA: idAlertCycle_Stuck::AIState_EnterState
// DIA: `idAlertCycle_Stuck::AIState_EnterState'::`1'::dtor$0
// DIA: `idAlertCycle_Stuck::AIState_EnterState'::`1'::dtor$1
// DIA: `idAlertCycle_Stuck::AIState_EnterState'::`1'::dtor$2
// DIA: `idAlertCycle_Stuck::AIState_EnterState'::`1'::dtor$3
// DIA: `idAlertCycle_Stuck::AIState_EnterState'::`1'::dtor$4
// DIA: `idAlertCycle_Stuck::AIState_EnterState'::`1'::dtor$5
// DIA: `idAlertCycle_Stuck::AIState_EnterState'::`1'::dtor$8
// DIA: `idAlertCycle_Stuck::AIState_EnterState'::`1'::dtor$9
// DIA: `idAlertCycle_Stuck::AIState_EnterState'::`1'::dtor$10
// DIA: `idAlertCycle_Stuck::AIState_EnterState'::`1'::dtor$11
// DIA: idAlertCycle_Stuck::AIState_Work
// DIA: idAlertCycle_UnhandledError::AIState_EnterState
// DIA: idAlertCycleFSM::idAlertCycleFSM
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$0
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$1
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$2
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$3
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$4
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$5
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$6
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$7
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$8
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$9
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$10
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$11
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$12
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$13
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$14
// DIA: `idAlertCycleFSM::idAlertCycleFSM'::`1'::dtor$15
// DIA: idAlertCycleFSM::~idAlertCycleFSM
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$0
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$1
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$2
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$3
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$4
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$5
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$6
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$7
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$8
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$9
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$10
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$11
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$12
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$13
// DIA: `idAlertCycleFSM::~idAlertCycleFSM'::`1'::dtor$14
// DIA: idAlertCycle_EquipWeapon::AIState_Work
// DIA: idAlertCycle_Pain::AIState_EnterState
// DIA: `idAlertCycle_Pain::AIState_EnterState'::`1'::dtor$0
// DIA: `idAlertCycle_Pain::AIState_EnterState'::`1'::dtor$1
// DIA: `idAlertCycle_Pain::AIState_EnterState'::`1'::dtor$2
// DIA: AC_DyingFinished::Evaluate
// DIA: `AC_DyingFinished::Evaluate'::`1'::dtor$0
// DIA: `AC_DyingFinished::Evaluate'::`1'::dtor$1
// DIA: `AC_DyingFinished::Evaluate'::`1'::dtor$2
// DIA: idAlertCycleFSM::`scalar deleting destructor'
// DIA: idAIMemory::GetCurrentPath
// DIA: FSM_SetupReaction
// DIA: AC_Shall_FSMReact::Evaluate
// DIA: AC_CanUseCombatScenePoint::Evaluate
// DIA: AC_CanUseSearchScenePoint::Evaluate
// DIA: AC_CanUseRoamScenePoint::Evaluate
// DIA: AC_NoEnemy::Evaluate
// DIA: AC_PlayerApproaching::Evaluate
// DIA: idAlertCycle_Dying::AIState_EnterState
// DIA: idAlertCycle_Dying::AIState_ExitState
// DIA: idAlertCycle_Pain::AIState_ExitState
// DIA: idAlertCycle_Dead::AIState_EnterState
// DIA: `dynamic initializer for 'ai_showDeathInfo''
// DIA: `dynamic initializer for 'ai_painGoreTest''
// DIA: `dynamic atexit destructor for 'ai_showDeathInfo''
// DIA: `dynamic atexit destructor for 'ai_painGoreTest''
