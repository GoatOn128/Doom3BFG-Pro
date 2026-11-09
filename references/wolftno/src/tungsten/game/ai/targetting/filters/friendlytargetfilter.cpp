#include "tungsten/game/ai/targetting/filters/friendlytargetfilter.h"

// DIA: idFriendlyTargetFilter::idFriendlyTargetFilter
idFriendlyTargetFilter::idFriendlyTargetFilter(int minimumAwareness)
    : idTargetFilter(minimumAwareness) {}

// DIA: idFriendlyTargetFilter::InternalFilter
bool idFriendlyTargetFilter::InternalFilter(
    const idEntity*, const idTargetInfo* targetInfo) const {
  if (targetInfo == nullptr || targetInfo->es == nullptr) return false;
  const idAIEntityState* state = targetInfo->es;
  if (state->entityType == 3 || targetInfo->attitude <= 3) return false;
  if (state->entityType <= 1 && state->IsDead()) return false;
  if (state->entityType == 0 && state->IsDying()) return false;
  return true;
}

// DIA: idFriendlyDeadTargetFilter::idFriendlyDeadTargetFilter
idFriendlyDeadTargetFilter::idFriendlyDeadTargetFilter(int minimumAwareness)
    : idTargetFilter(minimumAwareness) {}

// DIA: idFriendlyDeadTargetFilter::InternalFilter
bool idFriendlyDeadTargetFilter::InternalFilter(
    const idEntity*, const idTargetInfo* targetInfo) const {
  if (targetInfo == nullptr || targetInfo->es == nullptr) return false;
  const idAIEntityState* state = targetInfo->es;
  return state->entityType != 3 && state->entityType <= 1 &&
         targetInfo->attitude > 3 && state->IsDead();
}
