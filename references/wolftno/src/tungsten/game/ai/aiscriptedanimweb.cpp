#include "tungsten/game/ai/aiscriptedanimweb.h"

#include "tungsten/game/ai/aiactionscript.h"

#include <algorithm>
#include <string>

namespace {

bool PathsEqual(const char* left, const std::string& right) {
  return idStr::IcmpPath(left != nullptr ? left : "", right.c_str()) == 0;
}

}  // namespace

// DIA: idAIScriptedAnimWeb::animState_t::SetcurrentState
void idAIScriptedAnimWeb::animState_t::SetcurrentState(int state) {
  if (animWebStates == nullptr || state >= animWebStates->Num() ||
      (state < 0 && state != -2 && state != -3)) {
    return;
  }
  if (state == -2) state = GetRandomState();
  currentState = state;
}

// DIA: idAIScriptedAnimWeb::animState_t::GetRandomState
int idAIScriptedAnimWeb::animState_t::GetRandomState() {
  if (randomStates.Num() <= 0) return -1;
  randomSeed = 1664525u * randomSeed + 1013904223u;
  int index = static_cast<int>((randomSeed >> 10) & 0x7fffu) %
              randomStates.Num();
  int state = randomStates[index];
  if (currentState != -1 && state == currentState) {
    state = randomStates[(index + 1) % randomStates.Num()];
  }
  return state;
}

// DIA: idAIScriptedAnimWeb::animState_t::GotoNextState
void idAIScriptedAnimWeb::animState_t::GotoNextState() {
  if (animWebStates != nullptr && currentState + 1 < animWebStates->Num()) {
    ++currentState;
  }
}

// DIA: `idAIScriptedAnimWeb::PostSpawn'::`2'::local_t::ValidateAWebPath
bool idAIScriptedAnimWeb::ValidateAWebPath(idAI2* ai,
                                           const idStr& path) {
  if (path.IsEmpty()) return true;
  idStr subWebName;
  idStr stateName;
  if (ai == nullptr ||
      !ai->GetSubWebAndStateFromAnimWebPath(path.data, subWebName,
                                            stateName)) {
    validationErrors.Append(path);
    return false;
  }
  const auto& validPaths = ai->GetAnimWebState().validScriptedPaths;
  if (!validPaths.empty() &&
      std::none_of(validPaths.begin(), validPaths.end(),
                   [&path](const std::string& candidate) {
                     return PathsEqual(path.data, candidate);
                   })) {
    validationErrors.Append(path);
    return false;
  }
  return true;
}

// DIA: idAIScriptedAnimWeb::PostSpawn
void idAIScriptedAnimWeb::PostSpawn() {
  idEntity::PostSpawn();
  validationErrors.Clear();
  nonAITargetCount = 0;
  for (int targetIndex = 0; targetIndex < targets.Num(); ++targetIndex) {
    idAI2* ai = dynamic_cast<idAI2*>(targets[targetIndex]);
    if (ai == nullptr) {
      if (targets[targetIndex] != nullptr) ++nonAITargetCount;
      continue;
    }
    for (int stateIndex = 0; stateIndex < animWebStates.Num(); ++stateIndex) {
      const animWebState_t& state = animWebStates[stateIndex];
      ValidateAWebPath(ai, state.destAnimRef);
      ValidateAWebPath(ai, state.viaAnimRef);
      ValidateAWebPath(ai, state.exitFastRef);
    }
  }
}

// DIA: idAIScriptedAnimWeb::animState_t::SetAnimWebStates
void idAIScriptedAnimWeb::animState_t::SetAnimWebStates(
    idList<animWebState_t, 5>* states) {
  animWebStates = states;
  randomStates.Clear();
  if (states == nullptr) {
    currentState = -1;
    return;
  }
  for (int index = 0; index < states->Num(); ++index) {
    if ((*states)[index].randomState >= 0) {
      randomStates.Append(static_cast<std::uint16_t>(index));
    }
  }
  if (currentState >= states->Num()) currentState = -1;
}

// DIA: idAIScriptedAnimWeb::Spawn
void idAIScriptedAnimWeb::Spawn() {
  stateInfo.SetAnimWebStates(&animWebStates);
}

// DIA: idAIScriptedAnimWeb::StartAIAction
void idAIScriptedAnimWeb::StartAIAction(idEntity* activator) {
  idList<idScriptAction, 5> actionScript;
  idScriptAction action;
  action.name = "action_ScriptedAnimWeb";
  action.parameters.SetNum(2);
  action.parameters[0].Set(GetName(), idAIActionParm::SUBPARM_ENTITY);
  action.parameters[1].Set(stateInfo.currentState);
  actionScript.Append(action);

  const int gameTime = gameLocal != nullptr ? gameLocal->GetFrameNum() : 0;
  for (int index = 0; index < targets.Num(); ++index) {
    idAI2* ai = dynamic_cast<idAI2*>(targets[index]);
    if (ai == nullptr) continue;
    ai->SetActionScript(actionScript, this, activator);
    ai->StartActionScript(gameTime);
  }
}

// DIA: `idAIScriptedAnimWeb::StartAIAction'::`1'::dtor$0

// DIA: idAIScriptedAnimWeb::OnActivate
void idAIScriptedAnimWeb::OnActivate(idEntity* activator) {
  if (stateInfo.animWebStates != nullptr &&
      stateInfo.animWebStates->Num() > 0) {
    stateInfo.currentState = 0;
  }
  StartAIAction(activator);
}

// DIA: idAIScriptedAnimWeb::Event_SetScriptedAnimWebState
eventVoid* idAIScriptedAnimWeb::Event_SetScriptedAnimWebState(
    eventVoid* result, int state) {
  stateInfo.SetcurrentState(state);
  StartAIAction(this);
  if (result != nullptr) {
    result->accepted = true;
    result->completed = true;
  }
  return result;
}
