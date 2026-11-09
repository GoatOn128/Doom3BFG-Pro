#pragma once

#include "tungsten/game/ai/ai2_actions.h"

#include <cstdint>

struct encounterGroupHandle_t {
  std::int16_t value{-1};
  bool operator==(const encounterGroupHandle_t& other) const {
    return value == other.value;
  }
};

class idEncounterGroupManager {
public:
  idEncounterGroupManager();
  virtual ~idEncounterGroupManager();

  void Init();
  void Update();
  idEncounterGroup* GroupForHandle(encounterGroupHandle_t handle);
  int FindIndex(const char* groupName) const;
  int FindIndex(encounterGroupHandle_t handle) const;
  encounterGroupHandle_t FindGroup(const char* groupName) const;
  encounterGroupHandle_t HandleForName(const char* groupName) const;
  bool SendGroupMessage(encounterGroupHandle_t handle, aiGroupMessage_t message,
                        idEntity* entity);
  void Shutdown();
  void GetActiveHintNodeCoverIndices(idList<int, 5>* coverIndices) const;
  encounterGroupHandle_t AddGroup(const idEncounterGroup& otherGroup);
  encounterGroupHandle_t AddGroup(const char* groupName);

  int GetGroupCount() const { return groups.Num(); }
  idEncounterGroup& GetGroup(int index) { return groups[index]; }
  const idEncounterGroup& GetGroup(int index) const { return groups[index]; }
  int GetNextThinkTime() const { return nextThinkTime; }

private:
  idList<idEncounterGroup, 5> groups;
  std::int16_t curHandle{0};
  int nextThinkTime{0};
};
