#include "tungsten/game/ai/fsm/state.h"

#include <cstring>

idFiniteStateMachine* idState::GetChildFSM() const { return childFSM; }
const char* idState::GetChildFSMType() const {
  return childFSM != nullptr ? "idFiniteStateMachine" : "";
}
void idState::SetChildFSM(idFiniteStateMachine* newParentFSM,
                          idFiniteStateMachine* newChildFSM) {
  parentFSM = newParentFSM;
  childFSM = newChildFSM;
}
bool idState::IsChildDone() const {
  return forcedChildDone ||
         (childFSM != nullptr && childFSM->GetStatus() == FSM_STATUS_DONE);
}
void idState::Work(idFiniteStateMachine* fsm, int currentTime) {
  if (childFSM != nullptr) childFSM->Update(true, currentTime);
  if (auto* aiState = dynamic_cast<idAIState*>(this))
    aiState->InternalWork(dynamic_cast<idAIFSM*>(fsm),
                          static_cast<unsigned int>(currentTime));
}
void idState::ReenterState(idFiniteStateMachine* fsm, unsigned int transCode,
                           int currentTime) {
  if (auto* aiState = dynamic_cast<idAIState*>(this))
    aiState->InternalReenterState(dynamic_cast<idAIFSM*>(fsm), transCode,
                                  currentTime);
}
bool idState::IsStateWaiting(idFiniteStateMachine* fsm,
                             unsigned int currentTime) {
  if (auto* aiState = dynamic_cast<idAIState*>(this))
    return aiState->InternalIsStateWaiting(dynamic_cast<idAIFSM*>(fsm),
                                            currentTime);
  return false;
}
void idState::SetEnterFailed(bool value) { enterFailed = value; }
idState::~idState() = default;
void idState::StartChildFSM(idFiniteStateMachine* newParentFSM,
                            idFiniteStateMachine* newChildFSM,
                            int currentTime) {
  SetChildFSM(newParentFSM, newChildFSM);
  if (childFSM != nullptr) childFSM->Restart(newParentFSM, currentTime);
}
void idState::InitState(idFiniteStateMachine* fsm) { parentFSM = fsm; }
bool idState::IsInterruptible() const {
  return interruptible && (childFSM == nullptr || childFSM->IsInterruptible());
}
void idState::EnterState(idFiniteStateMachine* fsm,
                         unsigned int currentTime) {
  enterFailed = false;
  if (auto* aiState = dynamic_cast<idAIState*>(this))
    aiState->InternalEnterState(dynamic_cast<idAIFSM*>(fsm), currentTime);
}
void idState::ExitState(idFiniteStateMachine* fsm,
                        unsigned int currentTime) {
  if (childFSM != nullptr) childFSM->ExitChildStates(fsm, currentTime);
  childFSM = nullptr;
  if (auto* aiState = dynamic_cast<idAIState*>(this))
    aiState->InternalExitState(dynamic_cast<idAIFSM*>(fsm), currentTime);
}
void idState::Save(std::string* output, const char* indent) const {
  if (output == nullptr) return;
  output->append(indent != nullptr ? indent : "").append("state\n");
}
bool idState::EvaluateTransitions(idFiniteStateMachine* fsm, int currentTime,
                                  idFSMTransitionResult* result) const {
  return EvaluateTransitions(fsm, currentTime, -1, result);
}
bool idState::EvaluateTransitions(idFiniteStateMachine* fsm, int currentTime,
                                  int requestedEventId,
                                  idFSMTransitionResult* result) const {
  if (result == nullptr) return false;
  const auto& transitions = stateData.GetTransitions();
  for (int i = 0; i < transitions.Num(); ++i) {
    const auto& candidate = transitions[i];
    if (candidate.eventId >= 0 && candidate.eventId != requestedEventId) continue;
    if (candidate.transition != nullptr &&
        !candidate.transition->Evaluate(fsm,
            const_cast<idAIState*>(dynamic_cast<const idAIState*>(this)),
            currentTime, requestedEventId))
      continue;
    result->nextState = candidate.nextState;
    result->transitionType = candidate.transition != nullptr
                                 ? candidate.transition->GetName() : "event";
    result->transCode = candidate.transCode;
    return true;
  }
  return false;
}
bool idState::EvaluateEventTransitions(idFiniteStateMachine* fsm,
                                       int currentTime,
                                       idFSMTransitionResult* result) const {
  return EvaluateTransitions(fsm, currentTime, 0, result);
}
bool idState::EvaluateFSMEventTransitions(idFiniteStateMachine* fsm,
                                          int requestedEventId,
                                          int currentTime,
                                          idFSMTransitionResult* result) const {
  return EvaluateTransitions(fsm, currentTime, requestedEventId, result);
}

template class idClassFactory<idStateTransition, 1024, 110>;

idState::idState() = default;
template bool idList<idStateData::transition_t, 110>::Resize(int);
idStateData::idStateData() = default;
idStateData::~idStateData() {
  for (int i = 0; i < transitions.Num(); ++i)
    if (transitions[i].owned) delete transitions[i].transition;
  transitions.ClearFree();
}
void idStateData::Condense() {
  for (int i = 0; i < transitions.Num(); ++i) {
    for (int j = transitions.Num() - 1; j > i; --j) {
      if (transitions[i].nextState == transitions[j].nextState &&
          transitions[i].transCode == transitions[j].transCode &&
          transitions[i].eventId == transitions[j].eventId)
        transitions.RemoveIndex(j);
    }
  }
}
void idState::Condense() { stateData.Condense(); }
template idStateData::transition_t*
idList<idStateData::transition_t, 5>::Alloc();
idStateData::transition_t* idStateData::AllocTransition() {
  return transitions.Alloc();
}
void idState::AddStateTransition(idStateTransition* transition,
                                 idAIState* nextState, int transCode) {
  auto* item = stateData.AllocTransition();
  *item = {transition, nextState, transCode, -1, false};
}
void idState::AddStateTransition(const char* transitionName,
                                 idAIState* nextState, int transCode) {
  auto* item = stateData.AllocTransition();
  *item = {new idStateTransition(transitionName), nextState, transCode, -1, true};
}
void idState::AddFSMEventTransition(int requestedEventId,
                                    idAIState* nextState, int transCode) {
  auto* transition = new idStateTransition("fsmEvent");
  transition->SetRequiredEventId(requestedEventId);
  auto* item = stateData.AllocTransition();
  *item = {transition, nextState, transCode, requestedEventId, true};
}

// DIA: idState::GetChildFSM
// DIA: idState::GetChildFSMType
// DIA: idState::SetChildFSM
// DIA: idState::IsChildDone
// DIA: idState::Work
// DIA: idState::ReenterState
// DIA: idState::IsStateWaiting
// DIA: idState::SetEnterFailed
// DIA: idState::~idState
// DIA: idState::StartChildFSM
// DIA: idState::InitState
// DIA: idState::IsInterruptible
// DIA: idState::EnterState
// DIA: idState::ExitState
// DIA: idState::Save
// DIA: idState::EvaluateTransitions
// DIA: idState::EvaluateTransitions
// DIA: idState::EvaluateEventTransitions
// DIA: idState::EvaluateFSMEventTransitions
// DIA: idClassFactory<idStateTransition,1024,110>::Find
// DIA: idState::`scalar deleting destructor'
// DIA: idClassFactory<idStateTransition,1024,110>::Create
// DIA: idState::idState
// DIA: idList<idStateData::transition_t,110>::Resize
// DIA: idStateData::idStateData
// DIA: `idStateData::idStateData'::`1'::dtor$0
// DIA: `idStateData::idStateData'::`1'::dtor$1
// DIA: `idStateData::idStateData'::`1'::dtor$2
// DIA: idStateData::~idStateData
// DIA: `idStateData::~idStateData'::`1'::dtor$0
// DIA: `idStateData::~idStateData'::`1'::dtor$1
// DIA: `idStateData::~idStateData'::`1'::dtor$2
// DIA: `idStateData::~idStateData'::`1'::dtor$3
// DIA: idStateData::Condense
// DIA: idState::Condense
// DIA: idList<idStateData::transition_t,5>::Alloc
// DIA: idStateData::`scalar deleting destructor'
// DIA: idStateData::AllocTransition
// DIA: `idStateData::AllocTransition'::`1'::dtor$0
// DIA: idState::AddStateTransition
// DIA: idState::AddStateTransition
// DIA: idState::AddFSMEventTransition
