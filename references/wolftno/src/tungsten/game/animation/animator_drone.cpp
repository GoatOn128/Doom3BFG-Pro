#include "tungsten/game/animation/animator_drone.h"

#include <algorithm>
#include <cmath>

namespace {

int GameTime() { return gameLocal != nullptr ? gameLocal->GetGameTimeMs() : 0; }

idVec3 Normalized(idVec3 value) {
  if (value.Normalize() == 0.0F) return {};
  return value;
}

} // namespace

// DIA: idAnimator_Drone::SetLastDamageImpact
void idAnimator_Drone::SetLastDamageImpact(float damage, float damageFraction,
                                           const idVec3& damagePosition,
                                           const idVec3& damageDirection) {
  const bool crossedThreshold =
      (lastDamageFraction > 0.66F && damageFraction <= 0.66F) ||
      (lastDamageFraction > 0.33F && damageFraction <= 0.33F);
  lastDamageFraction = damageFraction;
  if (!crossedThreshold) return;

  lastDamageTick = GameTime() + 150;
  lastDamagePos = damagePosition;
  const idVec3 towardBody{(presentOrigin.x - damagePosition.x) * 0.5F,
                          (presentOrigin.y - damagePosition.y) * 0.5F,
                          presentOrigin.z - damagePosition.z + collisionRadius};
  const idVec3 shaped = Normalized(towardBody);
  lastDamageDir = Normalized(damageDirection +
      idVec3{shaped.x * 0.5F, shaped.y * 0.5F,
             (shaped.z - 0.5F) * 0.5F});
  lastDamagePush = std::min(damage, 5.0F);
}

// DIA: idAnimator_Drone::ShouldHoldFire
bool idAnimator_Drone::ShouldHoldFire(idAI2* ai, const idEntity* enemy,
                                      aiHoldFireReason_t* holdFireReason) {
  (void)enemy;
  if (ai == nullptr) return false;
  const int now = GameTime();
  if (launchDoneTime > 0 && now < launchDoneTime) return true;

  if (curFireWindupTick <= 0) {
    if (keepFiringStopTick > 0) {
      if (now >= keepFiringStopTick) {
        nextFireWindupTick = now + 500;
        keepFiringStopTick = 0;
      }
      if (holdFireReason != nullptr) *holdFireReason = AI_HOLDFIRE_REASON_NONE;
      return false;
    }
    if (noFireMoving && goalRange > 200.0F) {
      if (holdFireReason != nullptr) *holdFireReason = AI_HOLDFIRE_REASON_NOTRACK;
      return true;
    }
    return false;
  }

  if (curFireWindupTick + 500 >= now) {
    if (holdFireReason != nullptr) *holdFireReason = AI_HOLDFIRE_REASON_NOREGAIN;
    return true;
  }
  curFireWindupTick = 0;
  keepFiringStopTick = std::max(keepFiringStopTick, now + 500);
  if (holdFireReason != nullptr) *holdFireReason = AI_HOLDFIRE_REASON_NONE;
  return false;
}

// DIA: idSpring<idVec2>::Update
template <>
void idSpring<idVec2>::Update(float dt) {
  for (float remaining = std::max(0.0F, dt); remaining > 0.0F;) {
    const float step = std::min(remaining, 0.0085F);
    remaining -= step;
    idVec2 delta = p1 - p0;
    const float length = delta.Length();
    const idVec2 direction = length > 0.00001F ? delta * (1.0F / length)
                                                : idVec2{};
    const idVec2 force = direction * (-(length - restLength) * k);
    vel.x += ((force.x - c * vel.x) / std::max(m, 0.00001F)) * step;
    vel.y += ((force.y - c * vel.y) / std::max(m, 0.00001F)) * step;
    float speed = vel.Length();
    if (maxSpeed > 0.0F && speed > maxSpeed) {
      vel = vel * (maxSpeed / speed);
      speed = maxSpeed;
    }
    p1 = p1 + vel * step;
    if (speed < 0.00001F) vel = {};
    if (length < 0.00001F) p1 = p0;
  }
  if (hasPMin) {
    p1.x = std::max(p1.x, pMin.x);
    p1.y = std::max(p1.y, pMin.y);
  }
  if (hasPMax) {
    p1.x = std::min(p1.x, pMax.x);
    p1.y = std::min(p1.y, pMax.y);
  }
  if (fixDenormals) {
    if (std::fabs(p1.x) < 1.0e-30F) p1.x = 0.0F;
    if (std::fabs(p1.y) < 1.0e-30F) p1.y = 0.0F;
  }
}

// DIA: idAnimator_Drone::InternalPreBlendTree
void idAnimator_Drone::InternalPreBlendTree(const idAnimStack* stack,
                                            int curTime, int ticksPerSec) {
  (void)curTime;
  if (stack == nullptr) return;
  const float dt = ticksPerSec > 0 ? 1.0F / static_cast<float>(ticksPerSec)
                                   : 0.016F;
  for (feeler_t& feeler : feelers) feeler.angles.Update(dt);
  for (thruster_t& thruster : thrusters) {
    thruster.angles.Update(dt);
    thruster.propSpeed.Update(dt);
    thruster.propAngle = std::fmod(
        thruster.propAngle + thruster.propSpeed.p1.x * dt, 360.0F);
    if (thruster.propAngle < 0.0F) thruster.propAngle += 360.0F;
  }
  ++preBlendUpdates;
}

// DIA: idSpring<idVec2>::SetConstants
template <>
void idSpring<idVec2>::SetConstants(float springConstant, float damping) {
  k = std::min(springConstant, 10000.0F);
  c = damping < 0.0F ? 2.0F * std::sqrt(std::max(0.0F, k * m)) : damping;
}

// DIA: idAnimator_Drone::thruster_t::thruster_t
idAnimator_Drone::thruster_t::thruster_t() = default;

// DIA: idAnimator_Drone::feeler_t::feeler_t
idAnimator_Drone::feeler_t::feeler_t() = default;

// DIA: idAnimator_Drone::Update
void idAnimator_Drone::Update(idAI2* ai, const idMat3* axis,
                              const idVec3* velocity) {
  (void)ai;
  (void)axis;
  const idVec3 movement = velocity != nullptr ? *velocity : idVec3{};
  currentSpeed = movement.Length();
  const float moveScale = droneAnimator_thrusterAngleMovescale.GetFloat();
  for (thruster_t& thruster : thrusters) {
    thruster.angles.p0 = {movement.y * moveScale, -movement.x * moveScale};
    thruster.propSpeed.p0 = {currentSpeed, currentSpeed};
  }
  for (std::size_t index = 0; index < feelers.size(); ++index) {
    const float side = (index & 1U) != 0U ? -1.0F : 1.0F;
    feelers[index].angles.p0 = {movement.y * side * 0.1F,
                                movement.z * 0.1F};
  }
}

// DIA: idAnimator_Drone::InternalInit
bool idAnimator_Drone::InternalInit(const idAnimatorParms_Base* parms) {
  if (parms == nullptr || parms->animStack == nullptr) return false;
  for (feeler_t& feeler : feelers) {
    feeler.angles.SetConstants(droneAnimator_angleSpringK.GetFloat(), -1.0F);
    feeler.angles.pMin = feeler.minAngles;
    feeler.angles.pMax = feeler.maxAngles;
    feeler.angles.hasPMin = true;
    feeler.angles.hasPMax = true;
  }
  for (thruster_t& thruster : thrusters) {
    thruster.angles.SetConstants(
        droneAnimator_thrusterAngleSpringK.GetFloat(), -1.0F);
    thruster.propSpeed.SetConstants(
        droneAnimator_thrusterPropSpeedSpringK.GetFloat(), -1.0F);
  }
  mergeBranch.currentAlpha = 1.0F;
  mergeBranch.targetAlpha = 1.0F;
  mergeBranch.hasRightLeaf = true;
  initialized = true;
  return true;
}

// DIA: idAnimator_Drone::idAnimator_Drone
idAnimator_Drone::idAnimator_Drone() = default;

// DIA: `idAnimator_Drone::idAnimator_Drone'::`1'::dtor$0

// DIA: idAnimator_Drone::InternalGetMergeBranch
const idMD6Branch* idAnimator_Drone::InternalGetMergeBranch() const {
  return &mergeBranch;
}

// DIA: idAnimator_Drone::`scalar deleting destructor'

// DIA: `dynamic initializer for 'droneAnimator_angleSpringK''
// DIA: `dynamic atexit destructor for 'droneAnimator_angleSpringK''
idCVar droneAnimator_angleSpringK("droneAnimator_angleSpringK", "30", 4,
                                  "spring constant for angles");
// DIA: `dynamic initializer for 'droneAnimator_MaxAcc''
// DIA: `dynamic atexit destructor for 'droneAnimator_MaxAcc''
idCVar droneAnimator_MaxAcc("droneAnimator_droneAnimator_MaxAcc", "100", 4,
                            "maximum acceleration");
// DIA: `dynamic initializer for 'droneAnimator_thrusterAngleSpringK''
// DIA: `dynamic atexit destructor for 'droneAnimator_thrusterAngleSpringK''
idCVar droneAnimator_thrusterAngleSpringK(
    "droneAnimator_thrusterAngleSpringK", "300", 4,
    "thruster angle spring constant");
// DIA: `dynamic initializer for 'droneAnimator_thrusterPropSpeedSpringK''
// DIA: `dynamic atexit destructor for 'droneAnimator_thrusterPropSpeedSpringK''
idCVar droneAnimator_thrusterPropSpeedSpringK(
    "droneAnimator_thrusterPropSpeedSpringK", "300", 4,
    "thruster propeller-speed spring constant");
// DIA: `dynamic initializer for 'droneAnimator_thrusterAngleTurnscale''
// DIA: `dynamic atexit destructor for 'droneAnimator_thrusterAngleTurnscale''
idCVar droneAnimator_thrusterAngleTurnscale(
    "droneAnimator_thrusterAngleTurnscale", "0.1", 4, "turn scale");
// DIA: `dynamic initializer for 'droneAnimator_thrusterAngleMovescale''
// DIA: `dynamic atexit destructor for 'droneAnimator_thrusterAngleMovescale''
idCVar droneAnimator_thrusterAngleMovescale(
    "droneAnimator_thrusterAngleMovescale", "0.6", 4, "movement scale");
