#include "tungsten/game/ai/targetting/selectors/enemytargetselector.h"

#include <algorithm>

// DIA: idEnemyTargetSelector::GetWeight
float idEnemyTargetSelector::GetWeight(
    const idEntity*, const idTargetInfo* targetInfo) const {
  if (targetInfo == nullptr || targetInfo->es == nullptr) return 0.0F;
  const idAIEntityState* state = targetInfo->es;
  const idEntity* entity = state->GetEntity();
  if (state->IsDead()) return 0.0F;
  if (entity == nullptr && state->entityType != 6) return 0.0F;
  const bool isFake = state->entityType == 6;
  if (isFake && entity != fakeEnemy) return 0.0F;
  if (!isFake && targetInfo->attitude >= 3) return 0.0F;
  if (targetInfo->distTo > rangeMax) return 0.0F;

  const float rangeDenominator = std::max(1.0F, std::min(rangeMax, rangeCap));
  const float rangeScore = std::max(0.0F,
      1.0F - targetInfo->distTo / rangeDenominator);
  const float angleScore = std::max(0.0F, targetInfo->dirTo * facing);
  const float relationScore = isFake ? 0.0F
      : std::clamp((3.0F - targetInfo->attitude) / 3.0F, 0.0F, 1.0F);
  const float groupScore = entity != nullptr && entity == groupTarget ? 1.0F : 0.0F;
  const float currentScore = entity != nullptr && entity == currentTarget ? 1.0F : 0.0F;
  const float damagerScore = entity != nullptr && entity == recentDamager ? 1.0F : 0.0F;
  float spottedScore = state->IsVisible() ? 1.0F : 0.0F;
  if (spottedScore == 0.0F && state->curAwareness == 0) spottedScore = 0.1F;

  float typeScore = 0.0F;
  switch (state->entityType) {
    case 0: typeScore = weights.ai; break;
    case 1: typeScore = weights.player; break;
    case 2:
    case 7: typeScore = weights.vehicle; break;
    case 6: typeScore = weights.fake * fakeEnemyWeight; break;
    default: break;
  }
  const float scriptedAggro = entity != nullptr && entity == aggroTarget
                                  ? aggro * weights.aggro : 0.0F;
  return std::max(0.0F,
      rangeScore * weights.range + angleScore * weights.angle +
      relationScore * weights.relation + groupScore * weights.group +
      currentScore * weights.current + damagerScore * weights.damager +
      spottedScore * weights.spotted + typeScore + scriptedAggro);
}
