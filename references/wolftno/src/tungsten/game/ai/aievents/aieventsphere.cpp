#include "tungsten/game/ai/aievents/aieventsound.h"

#include <algorithm>

// DIA: idAIEventSphere::idAIEventSphere
idAIEventSphere::idAIEventSphere() = default;

// DIA: idAIEventSphere::GetRadius
float idAIEventSphere::GetRadius() const {
  const idDeclAiEvent* declaration = GetDeclaration();
  return declaration != nullptr ? std::max(0.0F, declaration->distance) : 0.0F;
}

// DIA: idAIEventSphere::InternalIsTouching
bool idAIEventSphere::InternalIsTouching(idEntity* entity,
                                         int currentTime) const {
  (void)currentTime;
  return entity != nullptr && entity->GetPhysics() != nullptr &&
         (entity->GetPhysics()->GetOrigin() - GetOrigin()).LengthSqr() <=
             GetRadius() * GetRadius();
}

// DIA: idAIEventSphere::InternalDrawDebug
void idAIEventSphere::InternalDrawDebug(int level, int currentTime,
                                        int duration) {
  (void)currentTime;
  (void)duration;
  if (level > 0) ++debugDrawCount;
  if (level > 1) ++debugDrawCount;
}

// DIA: idAIEventProjectedSphere::InternalUpdateAttached
idAIEvent::aiEventUpdateResult_t
idAIEventProjectedSphere::InternalUpdateAttached(int currentTime) {
  if (hasProjection) {
    SetTrackedOrigin(projectedOrigin);
    return AIUPDATE_KEEP;
  }
  if (GetOriginator() == nullptr || GetOriginator()->IsRemoved())
    return AIUPDATE_REMOVE;
  if (lastSimulationTime < 0) {
    const aiEventUpdateResult_t result =
        idAIEvent::InternalUpdateAttached(currentTime);
    if (result == AIUPDATE_REMOVE) return result;
    simulationPosition = GetOrigin();
    lastSimulationTime = currentTime;
    return AIUPDATE_KEEP;
  }
  const float dt = std::clamp((currentTime - lastSimulationTime) * 0.001F,
                              0.0F, 0.25F);
  lastSimulationTime = currentTime;
  simulationPosition += simulationVelocity * dt;
  simulationVelocity.z -= 1000.0F * dt;
  if (simulationPosition.z <= 0.0F) {
    simulationPosition.z = 0.0F;
    simulationVelocity.z = -simulationVelocity.z * 0.35F;
    simulationVelocity.x *= 0.8F;
    simulationVelocity.y *= 0.8F;
    ++bounces;
  }
  if (bounces >= 4 || currentTime - GetStartTime() >= 1500) {
    projectedOrigin = simulationPosition;
    hasProjection = true;
  }
  SetTrackedOrigin(hasProjection ? projectedOrigin : simulationPosition);
  return AIUPDATE_KEEP;
}
