#include "tungsten/game/ai/animfsm/animwebstate.h"

#include "tungsten/game/ai/ai2_actions.h"

#include <string>
#include <vector>

namespace {

std::vector<std::string> SplitPath(const char* path) {
  std::vector<std::string> parts;
  const std::string text = path != nullptr ? path : "";
  std::size_t start = 0;
  while (start <= text.size()) {
    const std::size_t separator = text.find_first_of("/\\", start);
    parts.emplace_back(text.substr(start, separator - start));
    if (separator == std::string::npos) break;
    start = separator + 1;
  }
  return parts;
}

const char* Safe(const char* value) { return value != nullptr ? value : ""; }

}  // namespace

// DIA: idAnimWebState::operator==
bool idAnimWebState::operator==(const idAnimWebState& other) const {
  return owner == other.owner && subWeb.value == other.subWeb.value &&
         state.value == other.state.value && node.value == other.node.value;
}

// DIA: idAnimWebState::UpdateDebugInfo
void idAnimWebState::UpdateDebugInfo(const char* defaultSubWebName,
                                     const char* defaultStateName) {
  if (owner != nullptr && node.value >= 0 && node.value < owner->nodes.Num() &&
      owner->nodes[node.value] != nullptr) {
    const idDeclAnimWebNode& webNode = *owner->nodes[node.value];
    subWeb = webNode.subWebIndex;
    state = webNode.states.numStates > 0 ? webNode.states.stateIndex[0]
                                         : animWebStateIndex_t{};
    debugSubWeb = owner->GetSubWebName(subWeb);
    debugState = owner->GetStateName(state);
    return;
  }
  debugSubWeb = Safe(defaultSubWebName);
  debugState = Safe(defaultStateName);
}

// DIA: idAnimWebState::Init
void idAnimWebState::Init(const idDeclAnimWeb* declaration,
                          idAnimWebPath* path) {
  pathConstructed = false;
  owner = declaration;
  const std::vector<std::string> parts =
      SplitPath(path != nullptr ? path->data : nullptr);
  const char* subWebName = parts.size() >= 3 ? parts[parts.size() - 2].c_str()
                                            : (parts.size() >= 2
                                                   ? parts[0].c_str()
                                                   : "");
  const char* stateName = parts.empty() ? "" : parts.back().c_str();
  Init(declaration, subWebName, stateName);
}

// DIA: idAnimWebState::Init
void idAnimWebState::Init(const idDeclAnimWeb* declaration,
                          const char* subWebName, const char* stateName) {
  pathConstructed = false;
  owner = declaration;
  subWeb = {};
  state = {};
  node = {};
  if (owner != nullptr) {
    subWeb = owner->GetSubWebIndexForName(subWebName);
    state = owner->GetStateIndexForName(stateName);
    node = owner->FindNodeIndex(subWeb, state);
  }
  UpdateDebugInfo(subWebName, stateName);
}

// DIA: idAIAnimWebState::idAIAnimWebState
idAIAnimWebState::idAIAnimWebState(idAI2* ai, const char* subWebName,
                                   const char* stateName) {
  Init(ai != nullptr ? ai->GetAnimWebState().declaration : nullptr,
       subWebName, stateName);
}

// DIA: idAIAnimWebState::idAIAnimWebState
idAIAnimWebState::idAIAnimWebState(const idAI2* ai, const char* path) {
  idAnimWebPath webPath;
  static_cast<idStr&>(webPath) = path;
  Init(ai != nullptr ? ai->GetAnimWebState().declaration : nullptr, &webPath);
}

// DIA: `idAIAnimWebState::idAIAnimWebState'::`1'::dtor$0

// DIA: idAIAnimWebState_Current::idAIAnimWebState_Current
idAIAnimWebState_Current::idAIAnimWebState_Current(const idAI2* ai) {
  owner = ai != nullptr ? ai->GetAnimWebState().declaration : nullptr;
  if (ai != nullptr) {
    subWeb.value = ai->GetAnimWebState().currentSubWeb.value;
    state.value = ai->GetAnimWebState().currentState.value;
    node.value = ai->GetAnimWebState().currentNode.value;
  }
  UpdateDebugInfo(nullptr, nullptr);
}
