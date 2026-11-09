#include "tungsten/game/ai/fsm/fsm_search_scenepoints.h"

int GetSearchAction(const idSearchScenePointContext& context) {
  return context.searchAction;
}
#define WOLF_DEFINE_SEARCH_EVALUATOR(Name, Field) \
  int Name::Evaluate(const idSearchScenePointContext& context) const { \
    return context.Field ? 452 : 0; \
  }
WOLF_SEARCH_EVALUATORS(WOLF_DEFINE_SEARCH_EVALUATOR)
#undef WOLF_DEFINE_SEARCH_EVALUATOR

#define WOLF_SEARCH_INIT(Name) \
  void idSearchScenePoint_##Name::InternalInitState() { InitState(); }
WOLF_SEARCH_STATES(WOLF_SEARCH_INIT)
#undef WOLF_SEARCH_INIT
#define WOLF_SEARCH_ENTER(Name) \
  void idSearchScenePoint_##Name::AIState_EnterState(idAIFSM*, unsigned int t) { RecordEnter(t); }
#define WOLF_SEARCH_WORK(Name) \
  void idSearchScenePoint_##Name::AIState_Work(idAIFSM*, unsigned int t) { RecordWork(t); }
#define WOLF_SEARCH_EXIT(Name) \
  void idSearchScenePoint_##Name::AIState_ExitState(idAIFSM*, unsigned int t) { RecordExit(t); }
#define WOLF_SEARCH_REENTER(Name) \
  void idSearchScenePoint_##Name::AIState_ReenterState(idAIFSM*, unsigned int, int t) { RecordEnter(static_cast<unsigned int>(t)); }
WOLF_SEARCH_STATES(WOLF_SEARCH_ENTER)
WOLF_SEARCH_STATES(WOLF_SEARCH_WORK)
WOLF_SEARCH_STATES(WOLF_SEARCH_EXIT)
WOLF_SEARCH_STATES(WOLF_SEARCH_REENTER)
#undef WOLF_SEARCH_ENTER
#undef WOLF_SEARCH_WORK
#undef WOLF_SEARCH_EXIT
#undef WOLF_SEARCH_REENTER

idSearchScenePointFSM::idSearchScenePointFSM() {
  SetIdentity("searchScenePoint", 400);
}
idSearchScenePointFSM::~idSearchScenePointFSM() = default;
void idSearchScenePointFSM::InitStates(const idFiniteStateMachineParams* params) {
  idAIFSM::InitStates(params);
#define WOLF_ADD_SEARCH_STATE(Name) AddState(&state##Name);
  WOLF_SEARCH_STATES(WOLF_ADD_SEARCH_STATE)
#undef WOLF_ADD_SEARCH_STATE
  SetDefaultState(&stateFind);
}
bool idSearchScenePointFSM::CanUse(const idSearchScenePointContext& context,
                                   int) const {
  const bool hasScenePoint = context.scenePoint != nullptr &&
      context.scenePoint->IsEnabled() &&
      context.scenePoint->GetScenePointType() == SEARCH;
  return context.enabled && (hasScenePoint || context.shallMoveToPos);
}

// Recovered DIA inventory for the complete search scene-point graph.
// DIA: idSearchScenePoint_Find::InternalInitState
// DIA: idSearchScenePoint_MoveTo::InternalInitState
// DIA: idSearchScenePoint_StayAt::InternalInitState
// DIA: idSearchScenePoint_MoveToPos::InternalInitState
// DIA: idSearchScenePoint_PerformSearchAnimation::InternalInitState
// DIA: idSearchScenePointFSM::InitStates
// DIA: GetSearchAction
// DIA: SearchScenePoint_Shall_MoveToScenePoint::Evaluate
// DIA: SearchScenePoint_Shall_MoveToPos::Evaluate
// DIA: SearchScenePoint_Shall_PerformSearch::Evaluate
// DIA: idSearchScenePoint_Find::AIState_EnterState
// DIA: `idSearchScenePoint_Find::AIState_EnterState'::`1'::dtor$0
// DIA: `idSearchScenePoint_Find::AIState_EnterState'::`1'::dtor$1
// DIA: `idSearchScenePoint_Find::AIState_EnterState'::`1'::dtor$2
// DIA: `idSearchScenePoint_Find::AIState_EnterState'::`1'::dtor$3
// DIA: idSearchScenePoint_Find::AIState_ReenterState
// DIA: idSearchScenePoint_MoveTo::AIState_EnterState
// DIA: `idSearchScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$0
// DIA: `idSearchScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$1
// DIA: `idSearchScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$2
// DIA: `idSearchScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$3
// DIA: `idSearchScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$4
// DIA: `idSearchScenePoint_MoveTo::AIState_EnterState'::`1'::dtor$5
// DIA: idSearchScenePoint_MoveTo::AIState_ExitState
// DIA: idSearchScenePoint_StayAt::AIState_ExitState
// DIA: idSearchScenePoint_MoveToPos::AIState_ExitState
// DIA: idSearchScenePointFSM::idSearchScenePointFSM
// DIA: `idSearchScenePointFSM::idSearchScenePointFSM'::`1'::dtor$0
// DIA: `idSearchScenePointFSM::idSearchScenePointFSM'::`1'::dtor$1
// DIA: `idSearchScenePointFSM::idSearchScenePointFSM'::`1'::dtor$2
// DIA: `idSearchScenePointFSM::idSearchScenePointFSM'::`1'::dtor$3
// DIA: `idSearchScenePointFSM::idSearchScenePointFSM'::`1'::dtor$4
// DIA: `idSearchScenePointFSM::idSearchScenePointFSM'::`1'::dtor$5
// DIA: idSearchScenePointFSM::~idSearchScenePointFSM
// DIA: `idSearchScenePointFSM::~idSearchScenePointFSM'::`1'::dtor$0
// DIA: `idSearchScenePointFSM::~idSearchScenePointFSM'::`1'::dtor$1
// DIA: `idSearchScenePointFSM::~idSearchScenePointFSM'::`1'::dtor$2
// DIA: `idSearchScenePointFSM::~idSearchScenePointFSM'::`1'::dtor$3
// DIA: `idSearchScenePointFSM::~idSearchScenePointFSM'::`1'::dtor$4
// DIA: SearchScenePoint_Shall_FindScenePoint::Evaluate
// DIA: SearchScenePoint_Shall_StayAtScenePoint::Evaluate
// DIA: SearchScenePoint_Shall_LookAtScenePoint::Evaluate
// DIA: idSearchScenePoint_StayAt::AIState_EnterState
// DIA: idSearchScenePoint_PerformSearchAnimation::AIState_EnterState
// DIA: `idSearchScenePoint_PerformSearchAnimation::AIState_EnterState'::`1'::dtor$0
// DIA: `idSearchScenePoint_PerformSearchAnimation::AIState_EnterState'::`1'::dtor$1
// DIA: `idSearchScenePoint_PerformSearchAnimation::AIState_EnterState'::`1'::dtor$2
// DIA: idSearchScenePointFSM::`scalar deleting destructor'
// DIA: idSearchScenePoint_MoveToPos::AIState_EnterState
// DIA: `idSearchScenePoint_MoveToPos::AIState_EnterState'::`1'::dtor$2
// DIA: `idSearchScenePoint_MoveToPos::AIState_EnterState'::`1'::dtor$3
// DIA: `idSearchScenePoint_MoveToPos::AIState_EnterState'::`1'::dtor$4
// DIA: `idSearchScenePoint_MoveToPos::AIState_EnterState'::`1'::dtor$5
// DIA: `idSearchScenePoint_MoveToPos::AIState_EnterState'::`1'::dtor$6
// DIA: `idSearchScenePoint_MoveToPos::AIState_EnterState'::`1'::dtor$7
// DIA: idSearchScenePoint_MoveToPos::AIState_ReenterState
// DIA: `idSearchScenePoint_MoveToPos::AIState_ReenterState'::`1'::dtor$2
// DIA: `idSearchScenePoint_MoveToPos::AIState_ReenterState'::`1'::dtor$3
// DIA: `idSearchScenePoint_MoveToPos::AIState_ReenterState'::`1'::dtor$4
// DIA: `idSearchScenePoint_MoveToPos::AIState_ReenterState'::`1'::dtor$5
// DIA: `idSearchScenePoint_MoveToPos::AIState_ReenterState'::`1'::dtor$6
// DIA: `idSearchScenePoint_MoveToPos::AIState_ReenterState'::`1'::dtor$7
// DIA: idSearchScenePointFSM::CanUse
// DIA: idSearchScenePoint_MoveToPos::AIState_Work
// DIA: `idSearchScenePoint_MoveToPos::AIState_Work'::`1'::dtor$0
// DIA: `idSearchScenePoint_MoveToPos::AIState_Work'::`1'::dtor$1
// DIA: `idSearchScenePoint_MoveToPos::AIState_Work'::`1'::dtor$2
// DIA: `idSearchScenePoint_MoveToPos::AIState_Work'::`1'::dtor$3
