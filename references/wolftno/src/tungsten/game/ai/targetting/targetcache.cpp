#include "tungsten/game/ai/targetting/targetcache.h"
#include "tungsten/game/ai/gamestate/aiworldstate.h"

// DIA: idTargetCache::RemoveBySpawnId
void idTargetCache::RemoveBySpawnId(const idSpawnId& spawnId) {
  for (int index = 0; index < targets.Num(); ++index) {
    if (targets[index].es != nullptr &&
        targets[index].es->spawnId.value == spawnId.value) {
      targets.RemoveIndexFast(index);
      return;
    }
  }
}

// DIA: idStaticList<idTargetInfo,8>::idStaticList<idTargetInfo,8>
template idStaticList<idTargetInfo, 8>::idStaticList() noexcept;

// DIA: idList<idTargetInfo,5>::Resize
template bool idList<idTargetInfo, 5>::Resize(int);

// DIA: idTargetCache::idTargetCache
idTargetCache::idTargetCache() = default;

// DIA: idTargetCache::Clear
void idTargetCache::Clear() {
  targets.Clear();
  curFrame = -1;
}

// DIA: idList<idTargetInfo,5>::Alloc
template idTargetInfo* idList<idTargetInfo, 5>::Alloc();

// DIA: idTargetCache::Init
void idTargetCache::Init(const aiLatchedState_t* ownerState,
                         const idAIWorldState* worldState) {
  targets.Clear();
  ++curFrame;
  if (ownerState == nullptr || worldState == nullptr) return;
  for (int index = 0; index < worldState->NumEntityStates(); ++index) {
    const idAIEntityState* entityState = worldState->GetEntityStateAt(index);
    if (entityState == nullptr || entityState->highestAwareness == 0 ||
        targets.Num() >= 8) continue;
    targets.Alloc()->Cache(ownerState, entityState);
  }
}

// DIA: idTargetCache::Update
void idTargetCache::Update(const aiLatchedState_t* ownerState,
                           idAIWorldState* worldState,
                           const idSpawnId& targetSpawnId) {
  if (ownerState == nullptr || worldState == nullptr) return;
  const idAIEntityState* entityState =
      worldState->StateForSpawnId(targetSpawnId);
  if (entityState == nullptr) return;
  for (int index = 0; index < targets.Num(); ++index) {
    if (targets[index].es != nullptr &&
        targets[index].es->spawnId.value == targetSpawnId.value) {
      targets[index].Cache(ownerState, entityState);
      return;
    }
  }
  if (targets.Num() < 8) targets.Alloc()->Cache(ownerState, entityState);
}
