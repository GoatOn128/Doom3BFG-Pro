#include "tungsten/game/ai/ai2_core_support.h"

#include <algorithm>
#include <cmath>

namespace {

int WanderingTime() {
  return gameLocal != nullptr ? gameLocal->GetGameTimeMs() : 0;
}

int WanderingFrameTime() {
  return gameLocal != nullptr ? gameLocal->GetGameMsPerFrame() : 16;
}

float NextSigned(unsigned int& seed) {
  seed = seed * 1664525U + 1013904223U;
  return static_cast<float>((seed >> 8U) & 0xFFFFU) / 32767.5F - 1.0F;
}

float NormalizeSafe(idVec3& value) {
  const float length = value.Length();
  if (length > 0.0F) value *= 1.0F / length;
  return length;
}

}  // namespace

// DIA: idWanderingAim::ConvergeTo
void idWanderingAim::ConvergeTo(float rate, const idVec3& target) {
  idVec3 delta = target - error;
  const float distance = delta.Length();
  const float step = std::max(0.0F, rate);
  if (distance <= step || distance == 0.0F) {
    error = target;
    return;
  }
  error += delta * (step / distance);
}

// DIA: idWanderingAim::AddWeaponFireKickbackImpulse
void idWanderingAim::AddWeaponFireKickbackImpulse(idAI2* ai,
                                                   const idWeapon* weapon) {
  if (ai == nullptr || weapon == nullptr) return;
  const float impulse = weapon->GetAimKickback();
  error.x += NextSigned(randomSeed) * impulse;
  error.y += NextSigned(randomSeed) * impulse;
  error.z += NextSigned(randomSeed) * impulse;
  kickback = error;
  ++kickCount;
}

// DIA: idWanderingAim::SmoothWander
void idWanderingAim::SmoothWander(float scale, float maxDivergence) {
  const float seconds = WanderingTime() * 0.001F;
  idVec3 drift{std::cos(seconds) * scale,
               std::cos(seconds * 4.23F) * scale,
               std::sin(seconds * 2.17F) * scale * 0.5F};
  if (drift * error < 0.0F) drift *= 0.2F;
  error += drift;
  const float maximum = std::max(0.0F, maxDivergence);
  const float length = error.Length();
  if (length > maximum && length > 0.0F) error *= maximum / length;
}

// DIA: idWanderingAim::GetEffectiveWeapon
idWeapon* idWanderingAim::GetEffectiveWeapon(const idAI2* ai) const {
  if (ai == nullptr) return nullptr;
  idWeapon* weapon = ai->GetFireControlWeapon(0);
  return weapon != nullptr ? weapon : ai->GetFireControlWeapon(1);
}

// DIA: idWanderingAim::Diverge
void idWanderingAim::Diverge(idAI2* ai) {
  idWeapon* weapon = GetEffectiveWeapon(ai);
  if (weapon == nullptr) return;
  lastTime = WanderingTime();
  SmoothWander(weapon->GetAimWanderScale(), weapon->GetAimMaxDivergence());
}

// DIA: idWanderingAim::Update
void idWanderingAim::Update(idAI2* ai, const idVec3& eyePosition,
                            const idVec3& directionToTarget,
                            float distanceToTarget, idVec3* newFocusPoint,
                            bool showDebug) {
  (void)showDebug;
  if (newFocusPoint == nullptr) return;
  idVec3 forward = directionToTarget;
  if (NormalizeSafe(forward) == 0.0F) forward = {1.0F, 0.0F, 0.0F};
  const float distance = std::max(0.0F, distanceToTarget);
  *newFocusPoint = eyePosition + forward * distance;

  idWeapon* weapon = GetEffectiveWeapon(ai);
  if (weapon == nullptr) return;
  const int now = WanderingTime();
  const bool inCombat = ai != nullptr && ai->GetEncounterAlertCycle() >= 3;
  if (!inCombat || weapon->fireState == idWeapon::FIRESTATE_RELOADING) {
    ConvergeTo(weapon->GetAimConvergeRate(), {});
  } else if (now - lastTime >= weapon->GetAimDivergeInterval()) {
    Diverge(ai);
  } else {
    ConvergeTo(weapon->GetAimConvergeRate() * 0.25F, {});
  }

  if (error.LengthSqr() > 0.0000001F)
    convergeTime = 0;
  else
    convergeTime += WanderingFrameTime();

  idVec3 up{0.0F, 0.0F, 1.0F};
  idVec3 right = forward.Cross(up);
  if (NormalizeSafe(right) == 0.0F) right = {0.0F, 1.0F, 0.0F};
  up = right.Cross(forward);
  NormalizeSafe(up);
  *newFocusPoint += (right * error.x + up * error.y + forward * error.z) *
                    distance;
}
