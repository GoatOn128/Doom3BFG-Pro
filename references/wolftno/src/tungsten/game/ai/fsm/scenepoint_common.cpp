#include "tungsten/game/ai/fsm/scenepoint_common.h"

#include <cfloat>

// DIA: IsScriptScenePointValid
bool IsScriptScenePointValid(const idScenePointSelectionContext& context,
                             const idScenePoint* point) {
  if (point == nullptr || !point->IsEnabled() ||
      (point->GetScriptFlags() & 1u) == 0)
    return false;
  const unsigned int flags = point->GetScriptFlags();
  if ((flags & 0x40u) != 0 && context.lastDamageTime > 0 &&
      context.scenePointTakeTime > 0 &&
      context.lastDamageTime > context.scenePointTakeTime)
    return false;
  if ((flags & 0x80u) != 0 && context.lastDamageTime > 0 &&
      context.scenePointActivateTime > 0 &&
      context.lastDamageTime > context.scenePointActivateTime)
    return false;
  if ((flags & 0x100u) != 0 && context.enemyAwareness > 1) return false;
  if ((flags & 0x200u) != 0 && context.enemyVisible) return false;
  return true;
}

// DIA: GetBestScenePoint
idScenePoint* GetBestScenePoint(const idScenePointSelectionContext& context,
                                float minRange, float maxRange,
                                const idVec3& focus, scenePointType_t type,
                                unsigned int) {
  idScenePoint* best = nullptr;
  float bestScore = -FLT_MAX;
  for (int i = 0; i < context.pointCount && i < static_cast<int>(context.points.size()); ++i) {
    idScenePoint* point = context.points[static_cast<std::size_t>(i)];
    if (point == nullptr || !point->IsEnabled() ||
        point->GetScenePointType() != type || point->GetPhysics() == nullptr)
      continue;
    if ((point->GetScriptFlags() & 1u) != 0 &&
        !IsScriptScenePointValid(context, point))
      continue;
    const idVec3 origin = point->GetPhysics()->GetOrigin();
    const float distance = (origin - context.actorOrigin).Length();
    if (distance < minRange || (maxRange > 0.0F && distance > maxRange))
      continue;
    const float focusDistance = (origin - focus).Length();
    const float score = -focusDistance - distance * 0.1F;
    if (score > bestScore) { bestScore = score; best = point; }
  }
  return best;
}
