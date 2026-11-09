#include "tungsten/game/ai/targetting/filters/enemytargetfilter.h"

// DIA: idEnemyTargetFilter::idEnemyTargetFilter
idEnemyTargetFilter::idEnemyTargetFilter(int minimumAwareness)
    : idTargetFilter(minimumAwareness) {}

// DIA: idEnemyTargetFilter::InternalFilter
bool idEnemyTargetFilter::InternalFilter(const idEntity*,
                                         const idTargetInfo* targetInfo) const {
  if (targetInfo == nullptr || targetInfo->es == nullptr) return false;
  const idAIEntityState* state = targetInfo->es;
  if (state->entityType == 3) return false;
  if (state->entityType < 2) {
    if (state->IsDead() && !state->IsDying()) return false;
  } else if (state->IsDead() || state->IsDying()) {
    return false;
  }
  return state->entityType == 6 || targetInfo->attitude < 2;
}
