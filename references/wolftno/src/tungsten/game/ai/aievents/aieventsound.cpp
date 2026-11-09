#include "tungsten/game/ai/aievents/aieventsound.h"

#include <algorithm>
#include <cmath>

// DIA: idAIEventSound::InternalIsTouching
bool idAIEventSound::InternalIsTouching(idEntity* entity,
                                        int currentTime) const {
  return dynamic_cast<idTrigger_EventListener*>(entity) != nullptr ||
         idAIEventSphere::InternalIsTouching(entity, currentTime);
}

// DIA: idAIEventSound::idAIEventSound
idAIEventSound::idAIEventSound() = default;

// DIA: idAIEventSound::InternalGetIntensity
float idAIEventSound::InternalGetIntensity(const idEntity* entity) const {
  const idDeclAiEvent* declaration = GetDeclaration();
  if (declaration == nullptr || entity == nullptr ||
      entity->GetPhysics() == nullptr || GetRadius() <= 0.0F)
    return 0.0F;
  const idVec3 delta = entity->GetPhysics()->GetOrigin() - GetOrigin();
  const float horizontalSquared = delta.x * delta.x + delta.y * delta.y;
  const float verticalScale = delta.z > 0.0F ? 3.0F : 1.0F;
  const float distance = std::sqrt(horizontalSquared +
                                   delta.z * delta.z * verticalScale * verticalScale);
  const float attenuation = std::clamp(1.0F - distance / GetRadius(),
                                       0.0F, 1.0F);
  return std::max(0.0F, declaration->volume * declaration->intensityScale *
                            attenuation * (1.0F - 0.5F * occlusion));
}
