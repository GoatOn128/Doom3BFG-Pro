#include "tungsten/game/ai/fsm/aistate.h"

#include <cstdio>

// DIA: idAIStateTransition::Evaluate
idAIStateTransition::aiTransCode_t idAIStateTransition::Evaluate(
    idAIFSM* parentFSM, idAIState* state, int currentTime) {
  return EvaluateForAI(parentFSM != nullptr ? parentFSM->GetAI() : nullptr,
                       parentFSM, state, currentTime);
}

// DIA: idAIStateTransition::NameForTransitionCode
const char* idAIStateTransition::NameForTransitionCode(aiTransCode_t code) {
  switch (code) {
    case 0: return "AI_TRANS_NONE";
    case 3: return "AI_TRANS_UNHANDLED_CHILD_ERROR";
    case 4: return "AI_TRANS_STATE_ERROR";
    case 451: return "AI_TRANS_DEFAULT_USE";
    case 452: return "AI_TRANS_DONE";
    default: return "";
  }
}

// DIA: idAIState::InternalCanUse
bool idAIState::InternalCanUse(idAIFSM* fsm, unsigned int currentTime) {
  return AIState_CanUse(fsm, currentTime);
}

// DIA: idAIState::InternalEnterState
void idAIState::InternalEnterState(idAIFSM* fsm, unsigned int currentTime) {
  idAIAction::InternalEnterState();
  AIState_EnterState(fsm, currentTime);
}

// DIA: idAIState::InternalExitState
void idAIState::InternalExitState(idAIFSM* fsm, unsigned int currentTime) {
  AIState_ExitState(fsm, currentTime);
  idAIAction::InternalExitState();
}

// DIA: idAIState::InternalWork
void idAIState::InternalWork(idAIFSM* fsm, unsigned int currentTime) {
  idAIAction::InternalWork();
  AIState_Work(fsm, currentTime);
}

// DIA: idAIState::InternalReenterState
void idAIState::InternalReenterState(idAIFSM* fsm, unsigned int transCode,
                                     int currentTime) {
  idAIAction::InternalReenterState();
  AIState_ReenterState(fsm, transCode, currentTime);
}

// DIA: idAIState::InternalIsStateWaiting
bool idAIState::InternalIsStateWaiting(idAIFSM* fsm,
                                       unsigned int currentTime) {
  return AIState_IsWaiting(fsm, currentTime);
}

// DIA: idAIStateTransitionStatic::InternalSave
void idAIStateTransitionStatic::InternalSave(
    std::string* output, const char* indent,
    const idList<transDest_t, 5>* dests) const {
  if (output == nullptr || dests == nullptr) return;
  const char* prefix = indent != nullptr ? indent : "";
  char line[512];
  for (int i = 0; i < dests->Num(); ++i) {
    const transDest_t& destination = (*dests)[i];
    const char* stateName = destination.nextStateType != nullptr
                                ? destination.nextStateType : "NULL";
    if (destination.transCode != 0)
      std::snprintf(line, sizeof(line), "%s\tto \"%s\" (%s)\n", prefix,
                    stateName, NameForTransitionCode(destination.transCode));
    else
      std::snprintf(line, sizeof(line), "%s\tto \"%s\"\n", prefix,
                    stateName);
    output->append(line);
  }
}

// DIA: idAIState::StartQueuedActions
void idAIState::StartQueuedActions(idAIActionFSM* actionFSM,
                                   idAIFSM*, int) {
  if (actionFSM == nullptr) return;
  actionFSM->StartQueuedActions();
  childActionFSM = actionFSM;
}

// DIA: idAIState::ClearQueuedActions
void idAIState::ClearQueuedActions(idAIActionFSM* actionFSM,
                                   idAIFSM*, int) {
  if (actionFSM != nullptr) actionFSM->GetQueue().Clear();
  childActionFSM = nullptr;
}
