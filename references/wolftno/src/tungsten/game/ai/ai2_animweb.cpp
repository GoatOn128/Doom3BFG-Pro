#include "tungsten/game/ai/ai2_actions.h"
#include "tungsten/game/ai/ai2_core_support.h"

#include <algorithm>
#include <cstdint>
#include <string>

namespace {

constexpr const char* kSubWebNames[] = {
    "relaxed",       "combat", "hostile", "search",  "pain",
    "pain_stagger",  "pain_moving", "pain_shock", "dying",
    "actionscript",  "fcover", "melee", "surprise", "dive",
    "dodge",         "grenade", "throw", "override", "turret",
    "rccontrol",     "traversal", "zipline", "dropship_fall",
    "react"};

std::int16_t NameIndex(const char* text) {
  if (text == nullptr || *text == '\0') return -1;
  std::uint32_t hash = 2166136261u;
  for (const unsigned char* cursor =
           reinterpret_cast<const unsigned char*>(text);
       *cursor != 0; ++cursor) {
    hash = (hash ^ *cursor) * 16777619u;
  }
  return static_cast<std::int16_t>((hash % 30000u) + 1u);
}

int SubWebNumber(aiSubWeb_t subWeb) {
  return static_cast<int>(subWeb);
}

}  // namespace

// DIA: idAI2::GetMovementStateName
void idAI2::GetMovementStateName(idStr* output) {
  if (output != nullptr) *output = animWebState.movementState;
}

// DIA: idWeapon::GetLoadedAmmo
idAmmoItem* idWeapon::GetLoadedAmmo(bool secondaryAmmo) {
  return secondaryAmmo ? ammoClipSecondary : ammoClip;
}

// DIA: idAI2::GetWeaponPrefix
void idAI2::GetWeaponPrefix(const idWeapon* weapon, idStr* output) {
  if (output == nullptr) return;
  *output = "hands_";
  if (weapon == nullptr) return;
  *output = weapon->GetAnimationPrefix();
  idAmmoItem* ammo = const_cast<idWeapon*>(weapon)->GetLoadedAmmo(false);
  if (ammo != nullptr && !ammo->animationPrefix.IsEmpty()) {
    *output = ammo->animationPrefix;
  }
}

// DIA: idAI2::GetWeaponPrefix
void idAI2::GetWeaponPrefix(idStr* output) {
  GetWeaponPrefix(meleeWeapon, output);
}

// DIA: idAI2::GetSubWebPrefix
void idAI2::GetSubWebPrefix(const idWeapon* weapon, int subWeb,
                            idStr* output) {
  if (output == nullptr || subWeb < 0 ||
      subWeb >= static_cast<int>(aiSubWeb_t::max)) {
    return;
  }
  GetWeaponPrefix(weapon, output);
  *output += kSubWebNames[subWeb];
}

// DIA: idAI2::GetSubWebPrefix
void idAI2::GetSubWebPrefix(aiSubWeb_t subWeb, idStr* output) const {
  const_cast<idAI2*>(this)->GetSubWebPrefix(meleeWeapon, SubWebNumber(subWeb),
                                           output);
}

// DIA: idAI2::GetSubWebPrefix
void idAI2::GetSubWebPrefix(idStr* output) {
  GetSubWebPrefix(animWebState.subWeb, output);
}

// DIA: idAI2::GetIdleStateName
void idAI2::GetIdleStateName(idStr* output) {
  if (output != nullptr) *output = animWebState.idleState;
}

// DIA: idAI2::GetIdleTurnStateName
void idAI2::GetIdleTurnStateName(int turnDirection, float turnDeltaDot,
                                 const char* baseTurnName, idStr* output) {
  if (output == nullptr) return;
  if (turnDirection == 0) {
    GetIdleStateName(output);
    return;
  }
  *output = baseTurnName != nullptr ? baseTurnName : "turn";
  *output += turnDirection < 0 ? "_left" : "_right";
  *output += turnDeltaDot < -0.70710671F ? "_back" : "_front";
}

// DIA: idAI2::ChangeAnimState
awPathResult_t idAI2::ChangeAnimState(
    const aiAnimWeb_t web,
    const idIndex<short, idDeclAnimWeb::invalidSubWebIndex_t> subWebIndex,
    const idIndex<short, idDeclAnimWeb::invalidStateIndex_t> stateIndex,
    const interruptPath_t interruptPath,
    const interruptBlend_t interruptBlend) {
  return animWebAnimators[static_cast<std::size_t>(web)]->ChangeState(
      subWebIndex, stateIndex, interruptPath, interruptBlend, 0);
}

// DIA: idAI2::ChangeAnimState
int idAI2::ChangeAnimState(int web, int subWebIndex, int stateIndex,
                           int interruptPath, int interruptBlend) {
  (void)web;
  (void)interruptPath;
  (void)interruptBlend;
  if (subWebIndex < 0 || stateIndex < 0) return 0;
  animWebState.currentSubWeb.value = static_cast<std::int16_t>(subWebIndex);
  animWebState.currentState.value = static_cast<std::int16_t>(stateIndex);
  animWebState.lastPathResult = 1;
  return 1;
}

// DIA: idAI2::ChangeAnimState
std::int64_t idAI2::ChangeAnimState(int web, aiSubWeb_t subWeb, int stateIndex,
                                    int interruptPath, int interruptBlend,
                                    idAnimWebIndex* subWebIndex) {
  const int resolvedSubWeb = SubWebNumber(subWeb);
  const int result = ChangeAnimState(web, resolvedSubWeb, stateIndex,
                                     interruptPath, interruptBlend);
  if (subWebIndex != nullptr) {
    subWebIndex->value = static_cast<std::int16_t>(resolvedSubWeb);
  }
  return result;
}

// DIA: idAI2::ChangeAnimState
std::int64_t idAI2::ChangeAnimState(int web, aiSubWeb_t subWeb,
                                    const char* stateName, int interruptPath,
                                    int interruptBlend,
                                    idAnimWebIndex* subWebIndex,
                                    idAnimWebIndex* stateIndex) {
  const std::int16_t resolvedState = NameIndex(stateName);
  if (stateIndex != nullptr) stateIndex->value = resolvedState;
  if (resolvedState < 0) return 0;
  return ChangeAnimState(web, subWeb, resolvedState, interruptPath,
                         interruptBlend, subWebIndex);
}

// DIA: idAI2::FindAnimWebAndStateIndices
void idAI2::FindAnimWebAndStateIndices(int web, int subWeb,
                                       const char* stateName,
                                       bool changeWeapon,
                                       const idWeapon* newWeapon,
                                       idAnimWebIndex* subWebIndex,
                                       idAnimWebIndex* stateIndex) {
  (void)web;
  if (changeWeapon) meleeWeapon = const_cast<idWeapon*>(newWeapon);
  if (subWebIndex != nullptr) {
    subWebIndex->value = static_cast<std::int16_t>(subWeb);
  }
  if (stateIndex != nullptr) stateIndex->value = NameIndex(stateName);
}

// DIA: idAI2::ChangeAnimStateVia
awPathResult_t idAI2::ChangeAnimStateVia(
    const aiAnimWeb_t web,
    const idIndex<short, idDeclAnimWeb::invalidSubWebIndex_t> subWebIndex,
    const idIndex<short, idDeclAnimWeb::invalidStateIndex_t> stateIndex,
    const idIndex<short, idDeclAnimWeb::invalidSubWebIndex_t> viaSubWebIndex,
    const idIndex<short, idDeclAnimWeb::invalidStateIndex_t> viaStateIndex,
    const interruptPath_t interruptPath,
    const interruptBlend_t interruptBlend) {
  return animWebAnimators[static_cast<std::size_t>(web)]->ChangeStateVia(
      subWebIndex, stateIndex, viaSubWebIndex, viaStateIndex, interruptPath,
      interruptBlend, 0);
}

// DIA: idAI2::ChangeAnimStateVia
int idAI2::ChangeAnimStateVia(int web, int subWebIndex, int stateIndex,
                              int viaSubWebIndex, int viaStateIndex,
                              int interruptPath, int interruptBlend) {
  if (viaSubWebIndex < 0 || viaStateIndex < 0) return 0;
  return ChangeAnimState(web, subWebIndex, stateIndex, interruptPath,
                         interruptBlend);
}

// DIA: idAI2::ChangeAnimStateVia
std::int64_t idAI2::ChangeAnimStateVia(
    int web, aiSubWeb_t subWeb, int stateIndex, aiSubWeb_t viaSubWeb,
    int viaStateIndex, int interruptPath, int interruptBlend,
    idAnimWebIndex* subWebIndex, idAnimWebIndex* viaSubWebIndex) {
  const int destination = SubWebNumber(subWeb);
  const int via = SubWebNumber(viaSubWeb);
  if (subWebIndex != nullptr) {
    subWebIndex->value = static_cast<std::int16_t>(destination);
  }
  if (viaSubWebIndex != nullptr) {
    viaSubWebIndex->value = static_cast<std::int16_t>(via);
  }
  return ChangeAnimStateVia(web, destination, stateIndex, via, viaStateIndex,
                            interruptPath, interruptBlend);
}

// DIA: idAI2::ChangeAnimStateVia
std::int64_t idAI2::ChangeAnimStateVia(
    int web, aiSubWeb_t subWeb, const char* stateName, aiSubWeb_t viaSubWeb,
    const char* viaStateName, int interruptPath, int interruptBlend,
    idAnimWebIndex* subWebIndex, idAnimWebIndex* stateIndex,
    idAnimWebIndex* viaSubWebIndex, idAnimWebIndex* viaStateIndex) {
  const std::int16_t destinationState = NameIndex(stateName);
  const std::int16_t intermediateState = NameIndex(viaStateName);
  if (stateIndex != nullptr) stateIndex->value = destinationState;
  if (viaStateIndex != nullptr) viaStateIndex->value = intermediateState;
  if (destinationState < 0 || intermediateState < 0) return 0;
  return ChangeAnimStateVia(web, subWeb, destinationState, viaSubWeb,
                            intermediateState, interruptPath, interruptBlend,
                            subWebIndex, viaSubWebIndex);
}

// DIA: idAI2::GetSubWebName
void idAI2::GetSubWebName(const idStr* input, idStr* output) {
  if (input == nullptr || output == nullptr) return;
  *output = *input;
  idStr weaponPrefix;
  GetWeaponPrefix(&weaponPrefix);
  output->Replace("$weapon", weaponPrefix.data);
}

// DIA: idAI2::ForceAndChangeAnimWebState
std::int64_t idAI2::ForceAndChangeAnimWebState(
    const char* viaSubWebName, const char* viaStateName,
    const char* destinationSubWebName, const char* destinationStateName,
    const void* blendParameters) {
  (void)blendParameters;
  const std::int16_t viaSubWeb = NameIndex(viaSubWebName);
  const std::int16_t viaState = NameIndex(viaStateName);
  const std::int16_t destinationSubWeb = NameIndex(destinationSubWebName);
  const std::int16_t destinationState = NameIndex(destinationStateName);
  if (viaSubWeb < 0 || viaState < 0 || destinationSubWeb < 0 ||
      destinationState < 0) {
    return 0;
  }
  return ChangeAnimStateVia(0, destinationSubWeb, destinationState,
                            viaSubWeb, viaState, 0, 0);
}

// DIA: idAI2::GetSubWebAndStateFromAnimWebPath
bool idAI2::GetSubWebAndStateFromAnimWebPath(const char* path,
                                             idStr& subWebName,
                                             idStr& stateName) const {
  if (path == nullptr) return false;
  std::string normalized(path);
  std::replace(normalized.begin(), normalized.end(), '\\', '/');
  std::replace(normalized.begin(), normalized.end(), ':', '/');
  const std::size_t stateSeparator = normalized.find_last_of('/');
  if (stateSeparator == std::string::npos || stateSeparator + 1 >= normalized.size()) {
    return false;
  }
  const std::size_t subWebSeparator =
      stateSeparator == 0 ? std::string::npos
                          : normalized.find_last_of('/', stateSeparator - 1);
  const std::size_t subWebStart =
      subWebSeparator == std::string::npos ? 0 : subWebSeparator + 1;
  if (subWebStart == stateSeparator) return false;
  subWebName = normalized.substr(subWebStart,
                                 stateSeparator - subWebStart).c_str();
  stateName = normalized.substr(stateSeparator + 1).c_str();
  const_cast<idAI2*>(this)->GetSubWebName(&subWebName, &subWebName);
  return true;
}

// DIA: `idAI2::GetSubWebAndStateFromAnimWebPath'::`1'::dtor$0
// DIA: `idAI2::GetSubWebAndStateFromAnimWebPath'::`1'::dtor$1
// DIA: `idAI2::GetSubWebAndStateFromAnimWebPath'::`1'::dtor$2
// DIA: `idAI2::GetSubWebAndStateFromAnimWebPath'::`1'::dtor$3

// DIA: idAI2::GetAnimWebNodeForOverride
bool idAI2::GetAnimWebNodeForOverride(int overrideType,
                                      idAnimWebIndex* subWebIndex,
                                      idAnimWebIndex* stateIndex) {
  if (overrideType < 0 || overrideType >= 8) return false;
  if (subWebIndex != nullptr) {
    subWebIndex->value = static_cast<std::int16_t>(overrideType % 4);
  }
  if (stateIndex != nullptr) {
    stateIndex->value = static_cast<std::int16_t>(overrideType);
  }
  return true;
}

// DIA: idAI2::GetAnimForAnimWebNode
const idAIAnimNodePlayback* idAI2::GetAnimForAnimWebNode(
    const idAnimWebIndex* subWebIndex, const idAnimWebIndex* stateIndex) {
  if (subWebIndex == nullptr || stateIndex == nullptr ||
      subWebIndex->value < 0 || stateIndex->value < 0) {
    return nullptr;
  }
  const int index = (subWebIndex->value + stateIndex->value) %
                    static_cast<int>(animWebState.nodeAnimations.size());
  return &animWebState.nodeAnimations[static_cast<std::size_t>(index)];
}

const idMD6Anim* idAI2::GetAnimForAnimWebNode(
    const idIndex<short, idDeclAnimWeb::invalidSubWebIndex_t>& subWebIndex,
    const idIndex<short, idDeclAnimWeb::invalidStateIndex_t>& stateIndex)
    const {
  ++animWebState.nodeAnimationLookupCount;
  if (subWebIndex.value < 0 || stateIndex.value < 0) return nullptr;
  const int index = (subWebIndex.value + stateIndex.value) %
                    static_cast<int>(animWebState.nodeAnimations.size());
  return animWebState.nodeAnimations[static_cast<std::size_t>(index)]
      .animation;
}

// DIA: idAI2::GetAnimTimeScaleForAnimWebNode
float idAI2::GetAnimTimeScaleForAnimWebNode(
    const idAnimWebIndex* subWebIndex, const idAnimWebIndex* stateIndex) {
  const idAIAnimNodePlayback* animation =
      GetAnimForAnimWebNode(subWebIndex, stateIndex);
  return animation != nullptr ? animation->timeScale : 0.0F;
}

float idAI2::GetAnimTimeScaleForAnimWebNode(
    const idIndex<short, idDeclAnimWeb::invalidSubWebIndex_t>& subWebIndex,
    const idIndex<short, idDeclAnimWeb::invalidStateIndex_t>& stateIndex)
    const {
  ++animWebState.nodeTimeScaleLookupCount;
  if (subWebIndex.value < 0 || stateIndex.value < 0) return 0.0F;
  const int index = (subWebIndex.value + stateIndex.value) %
                    static_cast<int>(animWebState.nodeAnimations.size());
  return animWebState.nodeAnimations[static_cast<std::size_t>(index)]
      .timeScale;
}

// DIA: idAI2::GetAnimForAnimWebNodeIndex
const idAIAnimNodePlayback* idAI2::GetAnimForAnimWebNodeIndex(
    const idAnimWebIndex* subWebIndex, const idAnimWebIndex* stateIndex,
    int index) {
  if (subWebIndex == nullptr || stateIndex == nullptr ||
      subWebIndex->value < 0 || stateIndex->value < 0 || index < 0 ||
      index >= static_cast<int>(animWebState.nodeAnimations.size())) {
    return nullptr;
  }
  return &animWebState.nodeAnimations[static_cast<std::size_t>(index)];
}

// DIA: idAI2::GetMovementSubWebAndStateIndices
void idAI2::GetMovementSubWebAndStateIndices(idAnimWebIndex* subWebIndex,
                                              idAnimWebIndex* stateIndex) {
  if (subWebIndex != nullptr) {
    subWebIndex->value = static_cast<std::int16_t>(SubWebNumber(animWebState.subWeb));
  }
  if (stateIndex != nullptr) {
    stateIndex->value = NameIndex(animWebState.movementState.data);
  }
}

// DIA: `idAI2::GetMovementSubWebAndStateIndices'::`1'::dtor$0
// DIA: `idAI2::GetMovementSubWebAndStateIndices'::`1'::dtor$1
// DIA: `idAI2::GetMovementSubWebAndStateIndices'::`1'::dtor$2
// DIA: `idAI2::GetMovementSubWebAndStateIndices'::`1'::dtor$3

// DIA: idAI2::GetCurrentSubWebAndStateIndices
void idAI2::GetCurrentSubWebAndStateIndices(idAnimWebIndex* subWebIndex,
                                             idAnimWebIndex* stateIndex) {
  if (subWebIndex != nullptr) *subWebIndex = animWebState.currentSubWeb;
  if (stateIndex != nullptr) *stateIndex = animWebState.currentState;
}
