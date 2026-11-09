#include "tungsten/game/ai/fsm/aifsmmanager.h"

#include <cctype>
#include <cstring>

namespace {

bool EqualNoCase(const char* left, const char* right) {
  if (left == nullptr || right == nullptr) return left == right;
  while (*left != '\0' && *right != '\0') {
    if (std::tolower(static_cast<unsigned char>(*left)) !=
        std::tolower(static_cast<unsigned char>(*right)))
      return false;
    ++left;
    ++right;
  }
  return *left == *right;
}

}  // namespace

// DIA: idAIFSMManager::AllocFSM
idAIFSM* idAIFSMManager::AllocFSM(
    const idAIFSMTypeInfo* type,
    const idFiniteStateMachineParams* params) {
  idAIFSM* result = FindFSM(type);
  if (result != nullptr) result->InitStates(params);
  return result;
}

// DIA: idBackStandFSM::~idBackStandFSM
idBackStandFSM::~idBackStandFSM() = default;

// DIA: idAIFSMManager::FindFSM
idAIFSM* idAIFSMManager::FindFSM(const char* name) {
  for (int i = 0; i < fsmCount; ++i)
    if (EqualNoCase(fsms[static_cast<std::size_t>(i)]->GetName(), name))
      return fsms[static_cast<std::size_t>(i)];
  return nullptr;
}

// DIA: idAIFSMManager::FindFSM
idAIFSM* idAIFSMManager::FindFSM(const idAIFSMTypeInfo* type) {
  if (type == nullptr) return nullptr;
  for (int i = 0; i < fsmCount; ++i) {
    idAIFSM* fsm = fsms[static_cast<std::size_t>(i)];
    if (fsm->GetTypeNum() >= type->typeNum &&
        fsm->GetTypeNum() <= type->lastChild)
      return fsm;
  }
  return nullptr;
}

// DIA: idAIFSMManager::idAIFSMManager
idAIFSMManager::idAIFSMManager() = default;
// DIA: `idAIFSMManager::idAIFSMManager'::`1'::dtor$0
// DIA: `idAIFSMManager::idAIFSMManager'::`1'::dtor$1
// DIA: `idAIFSMManager::idAIFSMManager'::`1'::dtor$2
// DIA: `idAIFSMManager::idAIFSMManager'::`1'::dtor$3
// DIA: `idAIFSMManager::idAIFSMManager'::`1'::dtor$4
// DIA: `idAIFSMManager::idAIFSMManager'::`1'::dtor$5

// DIA: idAIFSMManager::~idAIFSMManager
idAIFSMManager::~idAIFSMManager() {
  fsmCount = 0;
  owner = nullptr;
}

// DIA: idAIFSMManager::`scalar deleting destructor'

// DIA: idAIFSMManager::Init
void idAIFSMManager::Init(idEntity* newOwner) {
  owner = newOwner;
  fsmCount = 0;
  fsms[static_cast<std::size_t>(fsmCount++)] = &alertCycle;
  fsms[static_cast<std::size_t>(fsmCount++)] = &backStand;
  fsms[static_cast<std::size_t>(fsmCount++)] = &combatScenePointFSM;
  fsms[static_cast<std::size_t>(fsmCount++)] = &searchScenePointFSM;
  fsms[static_cast<std::size_t>(fsmCount++)] = &roamScenePointFSM;
  idFiniteStateMachineParams params;
  for (int i = 0; i < fsmCount; ++i)
    fsms[static_cast<std::size_t>(i)]->InitStates(&params);
}
