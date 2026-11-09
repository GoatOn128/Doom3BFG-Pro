#include "tungsten/game/ai/encountergroupmanager.h"

// DIA: idEncounterGroupManager::Init
void idEncounterGroupManager::Init() {
  nextThinkTime = gameLocal != nullptr ? gameLocal->GetGameTimeMs() : 0;
}

// DIA: idEncounterGroupManager::Update
void idEncounterGroupManager::Update() {
  const int now = gameLocal != nullptr ? gameLocal->GetGameTimeMs() : 0;
  if (now < nextThinkTime) return;
  for (int index = 0; index < groups.Num(); ++index) groups[index].Think();
  nextThinkTime = now + 512;
}

// DIA: idEncounterGroupManager::GroupForHandle
idEncounterGroup* idEncounterGroupManager::GroupForHandle(
    encounterGroupHandle_t handle) {
  const int index = FindIndex(handle);
  return index >= 0 ? &groups[index] : nullptr;
}

// DIA: idEncounterGroupManager::FindIndex
int idEncounterGroupManager::FindIndex(const char* groupName) const {
  if (groupName == nullptr) return -1;
  for (int index = 0; index < groups.Num(); ++index) {
    if (groups[index].GetGroupName().Icmp(groupName) == 0) return index;
  }
  return -1;
}

// DIA: idEncounterGroupManager::FindIndex
int idEncounterGroupManager::FindIndex(encounterGroupHandle_t handle) const {
  for (int index = 0; index < groups.Num(); ++index) {
    if (groups[index].GetHandle() == handle.value) return index;
  }
  return -1;
}

// DIA: idEncounterGroupManager::FindGroup
encounterGroupHandle_t idEncounterGroupManager::FindGroup(
    const char* groupName) const {
  const int index = FindIndex(groupName);
  return {static_cast<std::int16_t>(index >= 0 ? groups[index].GetHandle() : -1)};
}

// DIA: idEncounterGroupManager::HandleForName
encounterGroupHandle_t idEncounterGroupManager::HandleForName(
    const char* groupName) const {
  return FindGroup(groupName);
}

// DIA: idEncounterGroupManager::SendGroupMessage
bool idEncounterGroupManager::SendGroupMessage(encounterGroupHandle_t handle,
                                                aiGroupMessage_t message,
                                                idEntity* entity) {
  idEncounterGroup* group = GroupForHandle(handle);
  if (group == nullptr) return false;
  group->OnGroupMessage(message, entity);
  return true;
}

// DIA: idEncounterGroupManager::Shutdown
void idEncounterGroupManager::Shutdown() { groups.ClearFree(); }

// DIA: idEncounterGroupManager::GetActiveHintNodeCoverIndices
void idEncounterGroupManager::GetActiveHintNodeCoverIndices(
    idList<int, 5>* coverIndices) const {
  if (coverIndices == nullptr) return;
  coverIndices->Clear();
  for (int groupIndex = 0; groupIndex < groups.Num(); ++groupIndex) {
    if (groups[groupIndex].NumLivingMembers() <= 0) continue;
    idList<int, 5> groupCovers;
    groups[groupIndex].GetCurrentCombatStageCoverIndices(&groupCovers);
    for (int coverIndex = 0; coverIndex < groupCovers.Num(); ++coverIndex) {
      bool duplicate = false;
      for (int existing = 0; existing < coverIndices->Num(); ++existing) {
        if ((*coverIndices)[existing] == groupCovers[coverIndex]) {
          duplicate = true;
          break;
        }
      }
      if (!duplicate) coverIndices->Append(groupCovers[coverIndex]);
    }
  }
}

// DIA: idList<idEncounterGroup,5>::~idList<idEncounterGroup,5>
template idList<idEncounterGroup, 5>::~idList();

// DIA: idEncounterGroupManager::idEncounterGroupManager
idEncounterGroupManager::idEncounterGroupManager() = default;

// DIA: `idEncounterGroupManager::idEncounterGroupManager'::`1'::dtor$0
// DIA: `idEncounterGroupManager::idEncounterGroupManager'::`1'::dtor$1

// DIA: idEncounterGroupManager::~idEncounterGroupManager
idEncounterGroupManager::~idEncounterGroupManager() = default;

// DIA: `idEncounterGroupManager::~idEncounterGroupManager'::`1'::dtor$0
// DIA: `idEncounterGroupManager::~idEncounterGroupManager'::`1'::dtor$1
// DIA: `idEncounterGroupManager::~idEncounterGroupManager'::`1'::dtor$2

// DIA: idEncounterGroupManager::`scalar deleting destructor'

// DIA: idList<idEncounterGroup,5>::Resize
template bool idList<idEncounterGroup, 5>::Resize(int newSize);

// DIA: idList<idEncounterGroup,5>::Append
template int idList<idEncounterGroup, 5>::Append(const idEncounterGroup& value);

// DIA: idEncounterGroupManager::AddGroup
encounterGroupHandle_t idEncounterGroupManager::AddGroup(
    const idEncounterGroup& otherGroup) {
  const int existing = FindIndex(otherGroup.GetGroupName().data);
  if (existing >= 0) return {groups[existing].GetHandle()};
  const encounterGroupHandle_t handle{curHandle++};
  const int index = groups.Append(otherGroup);
  groups[index].Init(otherGroup.GetGroupName().data, handle.value);
  return handle;
}

// DIA: idList<idEncounterGroup,5>::Alloc
template idEncounterGroup* idList<idEncounterGroup, 5>::Alloc();

// DIA: idEncounterGroupManager::AddGroup
encounterGroupHandle_t idEncounterGroupManager::AddGroup(
    const char* groupName) {
  const int existing = FindIndex(groupName);
  if (existing >= 0) return {groups[existing].GetHandle()};
  const encounterGroupHandle_t handle{curHandle++};
  idEncounterGroup* group = groups.Alloc();
  group->Init(groupName, handle.value);
  return handle;
}
