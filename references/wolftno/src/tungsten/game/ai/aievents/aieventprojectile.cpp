#include "tungsten/game/ai/aievents/aieventprojectile.h"

#include <algorithm>

namespace {

float NormalizeSafe(idVec3& value) {
  const float length = value.Length();
  if (length > 0.0F) value *= 1.0F / length;
  return length;
}

}  // namespace

// DIA: idAIEventProjectile::idAIEventProjectile
idAIEventProjectile::idAIEventProjectile() = default;

// DIA: idAIEventProjectile::InternalUpdateAttached
idAIEvent::aiEventUpdateResult_t idAIEventProjectile::InternalUpdateAttached(
    int currentTime) {
  const aiEventUpdateResult_t result =
      idAIEvent::InternalUpdateAttached(currentTime);
  if (result == AIUPDATE_REMOVE) return result;
  idVec3 normalized = velocity;
  if (NormalizeSafe(normalized) > 0.0F) direction = normalized;
  return AIUPDATE_KEEP;
}

// DIA: idAIEventProjectile::GetDistance
float idAIEventProjectile::GetDistance() const {
  const idDeclAiEvent* declaration = GetDeclaration();
  const float lookAhead = declaration != nullptr ? declaration->lookAheadTime
                                                  : 0.0F;
  return std::clamp(lookAhead * velocity.Length(), 0.0F, 4096.0F);
}
