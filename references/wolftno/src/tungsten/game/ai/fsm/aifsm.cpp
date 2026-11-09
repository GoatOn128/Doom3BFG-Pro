#include "tungsten/game/ai/fsm/aifsm.h"

int idStateTransition_UnhandledChildError::Evaluate(
    const idAIState& state) const {
  return state.HasChildError() ? 3 : 0;
}

int idStateTransition_StateError::Evaluate(const idAIFSM& parentFSM) const {
  return parentFSM.HadError() ? 4 : 0;
}

idAI2* idAIFSM::GetAI() const { return reinterpret_cast<idAI2*>(GetOwner()); }

void idAIFSM::AppendDefaultTransitions(idAIState* state) {
  if (state == nullptr || state == &errorState || state == &doneState) return;
  state->AddDefaultTransition();
  state->AddDefaultTransition();
}

int Shared_CanUse::Evaluate() const { return 452; }
int Shared_DefaultUse::Evaluate() const { return 451; }
int Shared_ChildFinished::Evaluate(const idAIState& state) const {
  return state.IsChildDone() ? 452 : 0;
}
int Shared_MoveDone::Evaluate(const idAIFSM& parentFSM) const {
  return parentFSM.IsMoveDone() ? 452 : 0;
}

void idAIFSM::InitStates(const idFiniteStateMachineParams* params) {
  states.Clear();
  AddState(&errorState);
  AddState(&doneState);
  if (params != nullptr && params->startActive) doneState.InternalExitState();
}

idAIStateError* idAIFSM::GetErrorState() { return &errorState; }
idAIStateDone* idAIFSM::GetDoneState() { return &doneState; }

idAIFSMEventResult idAIFSM::InternalCallEvent(const idAIFSMEvent& event,
                                              const idEventArgs*) {
  idAIFSMEventResult result;
  if (event.type != FSM_EVENT_ACTION || actionCallerState == nullptr ||
      actionManager == nullptr || event.name == nullptr)
    return result;
  result.action = actionManager->GetActionStateForEvent(event.name);
  if (result.action != nullptr) {
    result.handled = actionManager->InternalCallEvent(event.name);
    actionCallerState = nullptr;
    lastFSMEventTransCode = result.handled ? 7 : 0;
  }
  return result;
}

bool idAIFSM::InternalRespondsTo(const idAIFSMEvent& event) const {
  if (event.type != FSM_EVENT_ACTION) return event.name != nullptr;
  return actionCallerState != nullptr && actionManager != nullptr &&
         actionManager->InternalRespondsTo(event.name);
}

int Shared_WorkTransCode::Evaluate(const idAIFSM& parentFSM) const {
  return parentFSM.lastWorkTransCode;
}
int Shared_FSMEventTransCode::Evaluate(const idAIFSM& parentFSM) const {
  return parentFSM.lastFSMEventTransCode;
}

// DIA: idStateTransition_UnhandledChildError::Evaluate
// DIA: idStateTransition_StateError::Evaluate
// DIA: idAIFSM::GetAI
// DIA: idAIFSM::AppendDefaultTransitions
// DIA: Shared_CanUse::Evaluate
// DIA: Shared_DefaultUse::Evaluate
// DIA: Shared_ChildFinished::Evaluate
// DIA: Shared_MoveDone::Evaluate
// DIA: idAIFSM::InitStates
// DIA: idAIFSM::GetErrorState
// DIA: idAIFSM::GetDoneState
// DIA: idAIFSM::InternalCallEvent
// DIA: `idAIFSM::InternalCallEvent'::`1'::dtor$0
// DIA: idAIFSM::InternalRespondsTo
// DIA: Shared_WorkTransCode::Evaluate
// DIA: Shared_FSMEventTransCode::Evaluate
