#include "tungsten/game/ai/aievents/aievents.h"

#include <algorithm>

namespace {

float NormalizeSafe(idVec3& value) {
  const float length = value.Length();
  if (length > 0.0F) value *= 1.0F / length;
  return length;
}

}  // namespace

// DIA: idAIEvent_Vehicle::idAIEvent_Vehicle
idAIEvent_Vehicle::idAIEvent_Vehicle() = default;

// DIA: idAIEvent_PlayerAim::idAIEvent_PlayerAim
idAIEvent_PlayerAim::idAIEvent_PlayerAim() = default;

// DIA: idAIEvent_PlayerAim::GetDistance
float idAIEvent_PlayerAim::GetDistance() const {
  const auto* player = dynamic_cast<const idPlayer*>(GetOriginator());
  return player != nullptr ? std::clamp(player->GetAIAimDistance(), 0.0F, 4096.0F)
                           : 4096.0F;
}

// DIA: idAIEvent_PlayerAim::InternalUpdateAttached
idAIEvent::aiEventUpdateResult_t idAIEvent_PlayerAim::InternalUpdateAttached(
    int currentTime) {
  (void)currentTime;
  const auto* player = dynamic_cast<const idPlayer*>(GetOriginator());
  if (player == nullptr || player->IsRemoved()) return AIUPDATE_REMOVE;
  idVec3 aimDirection = player->GetAIAimDirection();
  if (NormalizeSafe(aimDirection) == 0.0F) aimDirection = {1, 0, 0};
  direction = aimDirection;
  SetTrackedOrigin(player->GetAIAimOrigin());
  return AIUPDATE_KEEP;
}

// DIA: idAIEvent_Vehicle::InternalUpdateAttached
idAIEvent::aiEventUpdateResult_t idAIEvent_Vehicle::InternalUpdateAttached(
    int currentTime) {
  auto* vehicle = const_cast<idVehicle*>(
      dynamic_cast<const idVehicle*>(GetOriginator()));
  if (vehicle == nullptr || vehicle->IsRemoved()) return AIUPDATE_REMOVE;
  SetInstigator(vehicle->GetDriver() != nullptr ? vehicle->GetDriver()
                                                : vehicle);
  idVec3 velocity = vehicle->GetVelocity();
  const float speed = NormalizeSafe(velocity);
  if (speed > 0.0F) direction = velocity;
  const idDeclAiEvent* declaration = GetDeclaration();
  radius = std::clamp((declaration != nullptr ? declaration->lookAheadTime
                                              : 0.0F) * speed,
                      0.0F, 4096.0F);
  return idAIEventWedge::InternalUpdateAttached(currentTime);
}
