#include "tungsten/game/ai/targetting/selectors/neutraltargetselector.h"

#include <algorithm>

// DIA: idNeutralTargetSelector::GetWeight
float idNeutralTargetSelector::GetWeight(
    const idEntity*, const idTargetInfo* targetInfo) const {
  if (targetInfo == nullptr || targetInfo->es == nullptr ||
      targetInfo->es->IsDead() || targetInfo->es->curAwareness <= 1)
    return 0.0F;
  const float facingDot = ownerDirection * targetInfo->dirTo;
  const float angle = facingDot <= -0.965926F ? 0.0F
      : std::clamp((facingDot + 1.0F) * 0.5F, 0.0F, 1.0F);
  const float distance = 1.0F -
      std::min(targetInfo->distSqr / 4194304.0F, 1.0F);
  const float visibility = targetInfo->es->IsVisible() ? 1.0F : 0.5F;
  float result = (angle * 0.2F + distance * 0.8F) * visibility;
  if (targetInfo->es->curAwareness == 2) result *= 0.5F;
  return result;
}
