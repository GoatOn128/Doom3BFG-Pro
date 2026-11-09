#include "tungsten/game/ai/targetting/targetinfo.h"

#include <cmath>

// DIA: idTargetInfo::idTargetInfo
idTargetInfo::idTargetInfo() = default;

// DIA: idTargetInfo::Cache
void idTargetInfo::Cache(const aiLatchedState_t* ownerState,
                         const idAIEntityState* entityState) {
  es = entityState;
  distSqr = 0.0F;
  distTo = 0.0F;
  delta = {};
  dirTo = {1.0F, 0.0F, 0.0F};
  faceDir = {1.0F, 0.0F, 0.0F};
  attitude = -1;
  healthRatio = 0;
  if (ownerState == nullptr || entityState == nullptr) return;
  const aiSense_t selectedSense = entityState->lastSense == AISENSE_NONE
                                      ? AISENSE_SIGHT : entityState->lastSense;
  const aiPhysicalState_t* physical =
      entityState->senses[static_cast<std::size_t>(selectedSense)]
          ->GetConfirmedPhysicalSafe();
  delta = physical->origin - ownerState->physical.origin;
  distSqr = delta.LengthSqr();
  distTo = std::sqrt(distSqr);
  if (distTo > 0.0F) dirTo = delta / distTo;
  faceDir = physical->viewfwd;
  attitude = entityState->targetAttitude;
  healthRatio = entityState->targetHealthRatio;
}
