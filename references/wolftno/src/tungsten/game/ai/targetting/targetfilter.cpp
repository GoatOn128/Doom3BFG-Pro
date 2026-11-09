#include "tungsten/game/ai/targetting/targetfilter.h"

// DIA: idTargetFilter::Filter
bool idTargetFilter::Filter(const idEntity* owner,
                            const idTargetInfo* targetInfo) const {
  if (targetInfo == nullptr || targetInfo->es == nullptr) return false;
  const idAIEntityState* state = targetInfo->es;
  if (state->curAwareness < minAwareness) return false;
  const idEntity* entity = state->GetEntity();
  if (entity == nullptr) return false;
  if ((state->entityType == 1 || state->entityType == 6) &&
      (entity->IsHidden() || entity->IsRemoved())) return false;
  if (state->entityType == 5 && state->IsDead()) return false;
  return InternalFilter(owner, targetInfo);
}
