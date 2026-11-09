#include "tungsten/game/ai/targetting/selectors/friendlytargetselector.h"

#include <algorithm>

// DIA: idFriendlyTargetSelector::GetWeight
float idFriendlyTargetSelector::GetWeight(
    const idEntity*, const idTargetInfo* targetInfo) const {
  if (targetInfo == nullptr || targetInfo->es == nullptr ||
      targetInfo->es->IsDead() || ownerDying) return 0.0F;
  if (targetInfo->es->curAwareness <= 1) return 0.0F;
  const float angle = std::clamp(
      (ownerDirection * targetInfo->dirTo + 1.0F) * 0.5F, 0.0F, 1.0F);
  const float distance = 1.0F -
      std::min(targetInfo->distSqr / 4194304.0F, 1.0F);
  const float relationship = std::clamp(targetInfo->attitude / 6.0F,
                                         0.0F, 1.0F);
  const float visibility = targetInfo->es->IsVisible() ? 1.0F : 0.5F;
  float result = (angle * 0.25F + distance * 0.25F +
                  relationship * 0.5F) * visibility;
  if (hasEnemy && enemyDirection * targetInfo->dirTo > enemyFocusDot)
    result += 1.0F;
  if (targetInfo->es->curAwareness == 2) result *= 0.5F;
  return result;
}
