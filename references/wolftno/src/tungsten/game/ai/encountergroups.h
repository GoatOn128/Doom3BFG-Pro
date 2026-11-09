#pragma once

#include "tungsten/game/ai/ai2_actions.h"
#include "tungsten/game/ai/encountergroupmanager.h"

struct idEncounterDebugPrimitive {
  idVec3 origin{};
  idStr name;
  int duration{0};
};

void DrawPrimitive(const idAICombatHint* hint, const char* name, int duration);
const idList<idEncounterDebugPrimitive, 5>& GetEncounterDebugPrimitives();
void ClearEncounterDebugPrimitives();

class idEncounterGroupMgr : public idEntity {
public:
  idEncounterGroupMgr();
  ~idEncounterGroupMgr() override;

  void Spawn();
  void SetManager(idEncounterGroupManager* value) { manager = value; }
  void AddSpawnGroup(const char* groupName) {
    spawnGroups.Append(idStr(groupName));
  }
  int GetSpawnedGroupCount() const { return spawnedGroupCount; }

private:
  idEncounterGroupManager* manager{nullptr};
  idList<idStr, 5> spawnGroups;
  int spawnedGroupCount{0};
};

extern idCVar ai_debugGrenadeHints;
extern idCVar ai_debugGoodWill;
