#include "tungsten/game/ai/targetting/filters/nonenemytargetfilter.h"

// DIA: idNonEnemyTargetFilter::idNonEnemyTargetFilter
idNonEnemyTargetFilter::idNonEnemyTargetFilter(int minimumAwareness)
    : idTargetFilter(minimumAwareness) {}

// DIA: idNonEnemyTargetFilter::InternalFilter
bool idNonEnemyTargetFilter::InternalFilter(
    const idEntity*, const idTargetInfo* targetInfo) const {
  return targetInfo != nullptr && targetInfo->es != nullptr &&
         targetInfo->es->entityType != 3 && targetInfo->attitude >= 2;
}
