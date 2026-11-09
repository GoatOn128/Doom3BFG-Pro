#include "tungsten/game/ai/targetting/selectors/turrettargetselector.h"

#include <algorithm>
#include <cmath>

// DIA: idTurretTargetSelector::GetWeight
float idTurretTargetSelector::GetWeight(
    const idEntity*, const idTargetInfo* targetInfo) const {
  if (targetInfo == nullptr || targetInfo->es == nullptr) return 0.0F;
  const idAIEntityState* state = targetInfo->es;
  const idEntity* entity = state->GetEntity();
  if (entity == nullptr || state->IsDead()) return 0.0F;
  if (trackOnlyPlayer) return state->entityType == 1 ? 1.0F : 0.0F;
  const float distance = std::sqrt(std::max(0.0F, targetInfo->distSqr));
  if (distance < minTargetDistance || distance > maxTargetDistance)
    return 0.0F;
  const float halfAngleRadians = maxSearchAngle * 0.5F *
                                 3.14159265358979323846F / 180.0F;
  if (forwardSearchDirection * targetInfo->dirTo < std::cos(halfAngleRadians))
    return 0.0F;
  const float distanceWeight = std::exp2(-distance / 512.0F) * 0.9F + 0.1F;
  const float visibilityWeight = state->IsVisible() ? 1.0F : 0.5F;
  const float targetWeight = entity == lastTarget ? 1.0F : 0.5F;
  (void)lastAttacker;
  return visibilityWeight * distanceWeight * targetWeight;
}

// DIA: `dynamic initializer for 'turret_debugTargeting''
// DIA: `dynamic atexit destructor for 'turret_debugTargeting''
idCVar turret_debugTargeting("turret_debugTargeting", "0", 1,
                             "draw turret target scoring");
