#pragma once

#include "shared/idlib/containers/staticlist.h"
#include "tungsten/game/ai/targetting/targetinfo.h"

class idAIWorldState;

class idTargetCache {
public:
  idTargetCache();
  void RemoveBySpawnId(const idSpawnId& spawnId);
  void Clear();
  void Init(const aiLatchedState_t* ownerState,
            const idAIWorldState* worldState);
  void Update(const aiLatchedState_t* ownerState,
              idAIWorldState* worldState, const idSpawnId& targetSpawnId);
  int NumTargets() const { return targets.Num(); }
  const idTargetInfo* GetTarget(int index) const {
    return index >= 0 && index < targets.Num() ? &targets[index] : nullptr;
  }
  int GetCurrentFrame() const { return curFrame; }

private:
  idStaticList<idTargetInfo, 8> targets;
  int curFrame{-1};
};
