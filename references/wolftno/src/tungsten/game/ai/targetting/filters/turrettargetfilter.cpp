#include "tungsten/game/ai/targetting/filters/turrettargetfilter.h"

// DIA: idTurretTargetFilter::InternalFilter
bool idTurretTargetFilter::InternalFilter(
    const idEntity*, const idTargetInfo* targetInfo) const {
  if (targetInfo == nullptr || targetInfo->es == nullptr) return false;
  const idAIEntityState* state = targetInfo->es;
  if (state->entityType == 3) return false;
  if (state->entityType == 2 && state->IsDead()) return false;
  return targetInfo->attitude < 2;
}

// DIA: idTurretTargetFilter::idTurretTargetFilter
idTurretTargetFilter::idTurretTargetFilter(int minimumAwareness)
    : idTargetFilter(minimumAwareness) {}

// DIA: idTurretTargetFilter::~idTurretTargetFilter
idTurretTargetFilter::~idTurretTargetFilter() = default;

// DIA: idTurretTargetFilter::`scalar deleting destructor'
