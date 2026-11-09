#include "tungsten/game/ai/fsm/finitestatemachine.h"

#include <cstring>

// DIA: idFiniteStateMachine::ExitChildStates
void idFiniteStateMachine::ExitChildStates(idFiniteStateMachine*,
                                           int currentTime) {
  lastTransitionCode = 2;
  if (currentState != nullptr)
    currentState->InternalExitState(nullptr,
                                   static_cast<unsigned int>(currentTime));
}

// DIA: idFiniteStateMachine::SetNextState
void idFiniteStateMachine::SetNextState(const idFSMTransitionResult& result,
                                        int currentTime) {
  SetState(result, static_cast<unsigned int>(currentTime), false);
}

// DIA: idFiniteStateMachine::ForceUpdate
void idFiniteStateMachine::ForceUpdate() { nextUpdateTime = -1; }

// DIA: idFiniteStateMachine::GetStatus
idFSMStatus idFiniteStateMachine::GetStatus() const {
  if (HadError() || currentState == GetErrorStateBase()) return FSM_STATUS_ERROR;
  return currentState == GetDoneStateBase() ? FSM_STATUS_DONE
                                             : FSM_STATUS_RUNNING;
}

// DIA: idFiniteStateMachine::SetError
void idFiniteStateMachine::SetError(const char* transitionType,
                                    int transCode) {
  errorSourceState = currentState;
  errorTransitionType = transitionType;
  errorTransCode = transCode;
}

// DIA: idFiniteStateMachine::HadError
bool idFiniteStateMachine::HadError() const {
  return errorTransCode > 0 ||
         (currentState != nullptr && currentState == GetErrorStateBase());
}

// DIA: idFiniteStateMachine::IsInterruptible
bool idFiniteStateMachine::IsInterruptible() const { return interruptible; }

// DIA: idFiniteStateMachine::SetDefaultState
void idFiniteStateMachine::SetDefaultState(idAIState* state) {
  defaultState = state;
}

// DIA: idFiniteStateMachine::Init
void idFiniteStateMachine::Init(const idFiniteStateMachineParams* params) {
  states.Clear();
  if (params != nullptr) {
    owner = params->owner;
    manager = params->manager;
  }
  InitStates(params);
  for (int i = 0; i < states.Num(); ++i) AppendDefaultTransitions(states[i]);
  currentState = GetDoneStateBase();
  previousState = nullptr;
  if (defaultState == nullptr && states.Num() > 0) defaultState = states[0];
  nextUpdateTime = -1;
  stateFrame = 0;
}

// DIA: idFiniteStateMachine::FindState
idAIState* idFiniteStateMachine::FindState(const char* stateName) const {
  if (stateName == nullptr) return nullptr;
  for (int i = 0; i < states.Num(); ++i)
    if (std::strcmp(states[i]->GetName(), stateName) == 0) return states[i];
  return nullptr;
}

// DIA: idFiniteStateMachine::InternalRespondsTo
bool idFiniteStateMachine::InternalRespondsTo(const char* eventName) const {
  return currentState != nullptr && eventName != nullptr;
}

// DIA: idFiniteStateMachine::SetState
void idFiniteStateMachine::SetState(const idFSMTransitionResult& result,
                                    unsigned int currentTime, bool) {
  lastTransitionTime = static_cast<int>(currentTime);
  lastTransitionCode = result.transCode;
  if (result.nextState == currentState && currentState != nullptr) {
    currentState->InternalReenterState(nullptr,
                                      static_cast<unsigned int>(result.transCode),
                                      static_cast<int>(currentTime));
    previousState = currentState;
    nextUpdateTime = static_cast<int>(currentTime) + 1;
    return;
  }
  if (result.nextState != GetErrorStateBase()) {
    errorSourceState = nullptr;
    errorTransitionType = nullptr;
    errorTransCode = 0;
  }
  if (currentState != nullptr)
    currentState->InternalExitState(nullptr, currentTime);
  previousState = currentState;
  currentState = result.nextState;
  stateFrame = 0;
  if (currentState != nullptr)
    currentState->InternalEnterState(nullptr, currentTime);
  nextUpdateTime = static_cast<int>(currentTime) + 1;
}

// DIA: idFiniteStateMachine::SetCurrentState
void idFiniteStateMachine::SetCurrentState(idAIState* newState,
                                           const char* transitionType,
                                           int transCode,
                                           unsigned int currentTime) {
  SetState({newState, transitionType, transCode}, currentTime, false);
}

// DIA: idFiniteStateMachine::TransitionState
void idFiniteStateMachine::TransitionState(const idFSMTransitionResult& result,
                                           int currentTime) {
  if (result.nextState == GetErrorStateBase())
    SetError(result.transitionType, result.transCode);
  SetNextState(result, currentTime);
}

// DIA: idFiniteStateMachine::Save
void idFiniteStateMachine::Save(std::string* output, const char* indent) const {
  if (output == nullptr) return;
  const char* prefix = indent != nullptr ? indent : "";
  output->append(prefix).append("fsm {\n");
  for (int i = 0; i < states.Num(); ++i)
    output->append(prefix).append("\tstate \"").append(states[i]->GetName()).append("\"\n");
  output->append(prefix).append("}\n");
}

// DIA: idListArrayNew<idState::transitionInfo_t>
template idState::transitionInfo_t* idListArrayNew<idState::transitionInfo_t>(int);

// DIA: idFiniteStateMachine::PostEvent
bool idFiniteStateMachine::PostEvent(int newEventId, const void* newEventData,
                                     unsigned int currentTime) {
  if (currentState == nullptr) return false;
  eventId = newEventId;
  eventData = newEventData;
  nextUpdateTime = static_cast<int>(currentTime);
  const bool handled = newEventId >= 0;
  eventId = -1;
  eventData = nullptr;
  return handled;
}

// DIA: idFiniteStateMachine::InternalCallEvent
bool idFiniteStateMachine::InternalCallEvent(const char* eventName) {
  return InternalRespondsTo(eventName);
}
// DIA: `idFiniteStateMachine::InternalCallEvent'::`1'::dtor$1

// DIA: idFSMManager::SetFSMStack
void idFSMManager::SetFSMStack(const idFSMStack& value) { stack = value; }

// DIA: idListArrayResize<idState::transitionInfo_t>
template idState::transitionInfo_t* idListArrayResize<idState::transitionInfo_t>(
    idState::transitionInfo_t*, int, int);

// DIA: idFiniteStateMachine::idFiniteStateMachine
idFiniteStateMachine::idFiniteStateMachine() = default;

// DIA: idFiniteStateMachine::~idFiniteStateMachine
idFiniteStateMachine::~idFiniteStateMachine() = default;

// DIA: idFiniteStateMachine::GetOwner
idEntity* idFiniteStateMachine::GetOwner() const { return owner; }

// DIA: idList<idState::transitionInfo_t,5>::Resize
template bool idList<idState::transitionInfo_t, 5>::Resize(int);

// DIA: idFiniteStateMachine::`scalar deleting destructor'

// DIA: idFiniteStateMachine::AddState
void idFiniteStateMachine::AddState(idAIState* state) {
  if (state != nullptr && FindState(state->GetName()) == nullptr)
    states.Append(state);
}

// DIA: idFSMManager::idFSMStack::Add
bool idFSMManager::idFSMStack::Add(idFiniteStateMachine* fsm) {
  if (fsm == nullptr) return false;
  for (int i = 0; i < fsms.Num(); ++i)
    if (fsms[i] == fsm) return false;
  fsms.Append(fsm);
  return true;
}

// DIA: idList<idState::transitionInfo_t,5>::Alloc
template idState::transitionInfo_t*
idList<idState::transitionInfo_t, 5>::Alloc();

// DIA: idFiniteStateMachine::Update
void idFiniteStateMachine::Update(
    bool evaluate, int currentTime,
    idList<idState::transitionInfo_t, 5>* info) {
  if (currentState == nullptr ||
      (nextUpdateTime >= 0 && currentTime < nextUpdateTime))
    return;
  idAIState* before = currentState;
  currentState->InternalWork(nullptr, static_cast<unsigned int>(currentTime));
  ++stateFrame;
  nextUpdateTime = currentTime + (ai_oldFSMUpdate.GetInteger() != 0 ? 1 : 16);
  if (evaluate && info != nullptr) {
    auto* transition = info->Alloc();
    transition->from = before;
    transition->to = currentState;
    transition->transCode = lastTransitionCode;
    transition->time = currentTime;
  }
}

// DIA: idFiniteStateMachine::Restart
void idFiniteStateMachine::Restart(unsigned int currentTime) {
  errorSourceState = nullptr;
  errorTransitionType = nullptr;
  errorTransCode = 0;
  SetState({defaultState, nullptr, 1}, currentTime, false);
  nextUpdateTime = static_cast<int>(currentTime);
  Update(true, static_cast<int>(currentTime), nullptr);
}

// DIA: idFiniteStateMachine::Restart
void idFiniteStateMachine::Restart(idFiniteStateMachine*,
                                   unsigned int currentTime) {
  errorSourceState = nullptr;
  errorTransitionType = nullptr;
  errorTransCode = 0;
  SetState({defaultState, nullptr, 2}, currentTime, false);
  nextUpdateTime = static_cast<int>(currentTime);
  Update(true, static_cast<int>(currentTime), nullptr);
}

// DIA: `dynamic initializer for 'ai_oldFSMUpdate''
idCVar ai_oldFSMUpdate("ai_oldFSMUpdate", "0", 1,
                       "1 = use old fsm update policy");
// DIA: `dynamic atexit destructor for 'ai_oldFSMUpdate''
