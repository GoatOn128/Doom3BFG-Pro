#pragma once

#include "tungsten/game/ai/fsm/fsm_alertcycle.h"
#include "tungsten/game/ai/fsm/fsm_backstand.h"
#include "tungsten/game/ai/fsm/fsm_combat_scenepoints.h"
#include "tungsten/game/ai/fsm/fsm_roam_scenepoints.h"
#include "tungsten/game/ai/fsm/fsm_search_scenepoints.h"

#include <array>

struct idAIFSMTypeInfo {
  int typeNum{0};
  int lastChild{0};
};


class idAIFSMManager {
public:
  idAIFSMManager();
  virtual ~idAIFSMManager();

  idAIFSM* AllocFSM(const idAIFSMTypeInfo* type,
                    const idFiniteStateMachineParams* params);
  idAIFSM* FindFSM(const char* name);
  idAIFSM* FindFSM(const idAIFSMTypeInfo* type);
  void Init(idEntity* owner);

  int GetFSMCount() const { return fsmCount; }
  idEntity* GetOwner() const { return owner; }

private:
  idEntity* owner{nullptr};
  std::array<idAIFSM*, 5> fsms{};
  int fsmCount{0};
  idAlertCycleFSM alertCycle;
  idBackStandFSM backStand;
  idCombatScenePointFSM combatScenePointFSM;
  idSearchScenePointFSM searchScenePointFSM;
  idRoamScenePointFSM roamScenePointFSM;
};
