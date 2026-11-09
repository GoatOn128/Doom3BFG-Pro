#include "tungsten/game/ai/fsm/fsm_roam_scenepoints.h"

#define WOLF_DEFINE_ROAM_EVALUATOR(Name, Field) \
  int Name::Evaluate(const idRoamScenePointContext& context) const { \
    return context.Field ? 452 : 0; \
  }
WOLF_ROAM_EVALUATORS(WOLF_DEFINE_ROAM_EVALUATOR)
#undef WOLF_DEFINE_ROAM_EVALUATOR

#define WOLF_ROAM_INIT(Name) \
  void idRoamScenePoint_##Name::InternalInitState() { InitState(); }
WOLF_ROAM_STATES(WOLF_ROAM_INIT)
#undef WOLF_ROAM_INIT
#define WOLF_ROAM_ENTER(Name) \
  void idRoamScenePoint_##Name::AIState_EnterState(idAIFSM*, unsigned int t) { RecordEnter(t); }
#define WOLF_ROAM_WORK(Name) \
  void idRoamScenePoint_##Name::AIState_Work(idAIFSM*, unsigned int t) { RecordWork(t); }
#define WOLF_ROAM_EXIT(Name) \
  void idRoamScenePoint_##Name::AIState_ExitState(idAIFSM*, unsigned int t) { RecordExit(t); }
#define WOLF_ROAM_REENTER(Name) \
  void idRoamScenePoint_##Name::AIState_ReenterState(idAIFSM*, unsigned int, int t) { RecordEnter(static_cast<unsigned int>(t)); }
WOLF_ROAM_STATES(WOLF_ROAM_ENTER)
WOLF_ROAM_STATES(WOLF_ROAM_WORK)
WOLF_ROAM_STATES(WOLF_ROAM_EXIT)
WOLF_ROAM_STATES(WOLF_ROAM_REENTER)
#undef WOLF_ROAM_ENTER
#undef WOLF_ROAM_WORK
#undef WOLF_ROAM_EXIT
#undef WOLF_ROAM_REENTER

idRoamScenePointFSM::idRoamScenePointFSM() { SetIdentity("roamScenePoint", 500); }
idRoamScenePointFSM::~idRoamScenePointFSM() = default;
void idRoamScenePointFSM::InitStates(const idFiniteStateMachineParams* params) {
  idAIFSM::InitStates(params);
#define WOLF_ADD_ROAM_STATE(Name) AddState(&state##Name);
  WOLF_ROAM_STATES(WOLF_ADD_ROAM_STATE)
#undef WOLF_ADD_ROAM_STATE
  SetDefaultState(&stateFind);
}
bool idRoamScenePointFSM::CanUse(const idRoamScenePointContext& context,
                                 int) const {
  return context.enabled && context.scenePoint != nullptr &&
         context.scenePoint->IsEnabled() &&
         context.scenePoint->GetScenePointType() == ROAM;
}

// Recovered DIA inventory for the complete roam scene-point graph.
// DIA: idRoamScenePoint_Idle::InternalInitState
// DIA: idRoamScenePoint_Find::InternalInitState
// DIA: idRoamScenePoint_MoveTo::InternalInitState
// DIA: idRoamScenePoint_StayAt::InternalInitState
// DIA: idRoamScenePoint_SharedWaitForAnimation::InternalInitState
// DIA: idRoamScenePoint_SharedFinished::InternalInitState
// DIA: idRoamScenePointFSM::InitStates
// DIA: RoamScenePoint_Shall_Idle::Evaluate
// DIA: RoamScenePoint_Shall_MoveToScenePoint::Evaluate
// DIA: idRoamScenePoint_Find::AIState_EnterState
// DIA: idRoamScenePoint_Find::AIState_ReenterState
// DIA: idRoamScenePoint_MoveTo::AIState_EnterState
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$0
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$1
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$2
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$3
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$4
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$5
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$6
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$7
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$10
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$11
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$12
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$13
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$14
// DIA: `idRoamScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$15
// DIA: idRoamScenePoint_MoveTo::AIState_ExitState
// DIA: idRoamScenePoint_StayAt::AIState_Work
// DIA: idRoamScenePointFSM::idRoamScenePointFSM
// DIA: `idRoamScenePointFSM::idRoamScenePointFSM'::`1'::dtor$0
// DIA: `idRoamScenePointFSM::idRoamScenePointFSM'::`1'::dtor$1
// DIA: `idRoamScenePointFSM::idRoamScenePointFSM'::`1'::dtor$2
// DIA: `idRoamScenePointFSM::idRoamScenePointFSM'::`1'::dtor$3
// DIA: `idRoamScenePointFSM::idRoamScenePointFSM'::`1'::dtor$4
// DIA: `idRoamScenePointFSM::idRoamScenePointFSM'::`1'::dtor$5
// DIA: `idRoamScenePointFSM::idRoamScenePointFSM'::`1'::dtor$6
// DIA: idRoamScenePointFSM::~idRoamScenePointFSM
// DIA: RoamScenePoint_Shall_FindScenePoint::Evaluate
// DIA: RoamScenePoint_Shall_StayAtScenePoint::Evaluate
// DIA: RoamScenePoint_Shall_LookAtScenePoint::Evaluate
// DIA: idRoamScenePoint_StayAt::AIState_EnterState
// DIA: `idRoamScenePoint_StayAt::AIState_EnterState'::`1'::dtor$0
// DIA: `idRoamScenePoint_StayAt::AIState_EnterState'::`1'::dtor$1
// DIA: `idRoamScenePoint_StayAt::AIState_EnterState'::`1'::dtor$2
// DIA: `idRoamScenePoint_StayAt::AIState_EnterState'::`1'::dtor$5
// DIA: `idRoamScenePoint_StayAt::AIState_EnterState'::`1'::dtor$6
// DIA: `idRoamScenePoint_StayAt::AIState_EnterState'::`1'::dtor$7
// DIA: `idRoamScenePoint_StayAt::AIState_EnterState'::`1'::dtor$8
// DIA: `idRoamScenePoint_StayAt::AIState_EnterState'::`1'::dtor$11
// DIA: `idRoamScenePoint_StayAt::AIState_EnterState'::`1'::dtor$12
// DIA: `idRoamScenePoint_StayAt::AIState_EnterState'::`1'::dtor$13
// DIA: `idRoamScenePoint_StayAt::AIState_EnterState'::`1'::dtor$14
// DIA: idRoamScenePointFSM::`scalar deleting destructor'
// DIA: idRoamScenePoint_Idle::AIState_EnterState
// DIA: idRoamScenePoint_Idle::AIState_Work
// DIA: idRoamScenePointFSM::CanUse
