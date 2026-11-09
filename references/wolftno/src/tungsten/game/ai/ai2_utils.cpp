#include "tungsten/game/ai/ai2_actions.h"
#include "tungsten/game/ai/aimemory.h"

#include <algorithm>
#include <cmath>

namespace {

idMat3 IdentityAxis() {
  idMat3 axis{};
  axis[0] = {1.0F, 0.0F, 0.0F};
  axis[1] = {0.0F, 1.0F, 0.0F};
  axis[2] = {0.0F, 0.0F, 1.0F};
  return axis;
}

idVec3 EntityOrigin(const idEntity& entity) {
  return entity.GetPhysics() != nullptr ? entity.GetPhysics()->GetOrigin()
                                        : idVec3{};
}

}  // namespace

bool idWeapon::QueryMuzzleTransform(idVec3* position, idMat3* axis) const {
  if (!hasMuzzleTransform || position == nullptr || axis == nullptr) return false;
  *position = muzzlePosition;
  *axis = muzzleAxis;
  return true;
}

bool idWeapon::QueryLaserTransform(idVec3* position, idMat3* axis) const {
  if (!hasLaserTransform || position == nullptr || axis == nullptr) return false;
  *position = laserPosition;
  *axis = laserAxis;
  return true;
}

// DIA: idAI2::SelectThrowable
bool idAI2::SelectThrowable(int selectorType, const idVec3* targetPosition,
                            int coverAction, idAIProjectileInfo* info) {
  (void)selectorType;
  (void)coverAction;
  if (targetPosition == nullptr || info == nullptr || meleeWeapon == nullptr) {
    return false;
  }
  info->itemDecl = meleeWeapon;
  info->actualTargetPos = *targetPosition;
  info->predictedTargetPos = *targetPosition;
  utilityState.selectedProjectile = *info;
  return true;
}

// DIA: idAI2::HasWaitingTrajectoryResults
bool idAI2::HasWaitingTrajectoryResults() {
  return std::any_of(utilityState.trajectoryHasResults.begin(),
                     utilityState.trajectoryHasResults.end(),
                     [](bool value) { return value; });
}

// DIA: idAI2::ClearTrajectoryResults
void idAI2::ClearTrajectoryResults() {
  utilityState.trajectoryHasResults.fill(false);
}

// DIA: idAI2::GetMuzzleTransform
bool idAI2::GetMuzzleTransform(idWeapon* weapon, idVec3* muzzlePosition,
                               idMat3* muzzleAxis) {
  if (weapon == nullptr || muzzlePosition == nullptr || muzzleAxis == nullptr) {
    return false;
  }
  return static_cast<const idAI2*>(this)->GetMuzzleTransform(
      weapon, *muzzlePosition, *muzzleAxis);
}

bool idAI2::GetMuzzleTransform(idWeapon* weapon, idVec3& muzzlePosition,
                               idMat3& muzzleAxis) const {
  if (weapon == nullptr) return false;
  if (weapon->ForcesMuzzleTransformFailureForTest()) return false;
  if (weapon->QueryMuzzleTransform(&muzzlePosition, &muzzleAxis)) return true;
  muzzlePosition = EntityOrigin(*this);
  muzzleAxis = *GetPresentableAxis();
  return true;
}

// DIA: idAI2::GetMuzzlePosition
bool idAI2::GetMuzzlePosition(idWeapon* weapon, idVec3* muzzlePosition) {
  if (muzzlePosition == nullptr) return false;
  return static_cast<const idAI2*>(this)->GetMuzzlePosition(
      weapon, *muzzlePosition);
}

bool idAI2::GetMuzzlePosition(idWeapon* weapon,
                              idVec3& muzzlePosition) const {
  idMat3 unusedAxis{};
  return GetMuzzleTransform(weapon, muzzlePosition, unusedAxis);
}

// DIA: idAI2::GetLaserSightTransform
bool idAI2::GetLaserSightTransform(idWeapon* weapon, idVec3* laserPosition,
                                   idMat3* laserAxis) {
  if (weapon == nullptr || laserPosition == nullptr || laserAxis == nullptr) {
    return false;
  }
  if (weapon->QueryLaserTransform(laserPosition, laserAxis)) return true;
  return GetMuzzleTransform(weapon, laserPosition, laserAxis);
}

// DIA: idAI2::TargetMovingPoint
void idAI2::TargetMovingPoint(const idVec3* launchPoint, float launchSpeed,
                              const idVec3* targetPosition,
                              const idVec3* targetVelocity, float* lagTime,
                              float* animationDelay, idVec3* launchDirection,
                              idVec3* predictedTargetPosition) {
  if (launchPoint == nullptr || targetPosition == nullptr ||
      targetVelocity == nullptr || launchDirection == nullptr ||
      predictedTargetPosition == nullptr) {
    return;
  }
  const float safeSpeed = std::max(launchSpeed, 0.001F);
  const float lag = lagTime != nullptr ? *lagTime : 0.0F;
  const float delay = animationDelay != nullptr ? *animationDelay : 0.0F;
  const float travelTime = (*targetPosition - *launchPoint).Length() / safeSpeed;
  *predictedTargetPosition =
      *targetPosition + *targetVelocity * (travelTime + lag + delay);
  *launchDirection = *predictedTargetPosition - *launchPoint;
  launchDirection->Normalize();
}

// DIA: idAIMemory::SetProjectileInfo
void idAIMemory::SetProjectileInfo(const idAIProjectileInfo* info) {
  if (info != nullptr) projectileInfo = *info;
}

// DIA: idAI2::LaunchParabolicProjectile
bool idAI2::LaunchParabolicProjectile(
    const idDeclProjectile* projectileDecl,
    const idParabolicParms* trajectoryParameters,
    const idVec3* targetPosition, const idVec3* startPosition, int startTime) {
  if (projectileDecl == nullptr || !projectileDecl->spawnable ||
      trajectoryParameters == nullptr || targetPosition == nullptr ||
      startPosition == nullptr || trajectoryParameters->speed <= 0.0F) {
    return false;
  }
  utilityState.lastLaunch.actualTargetPos = *targetPosition;
  utilityState.lastLaunch.predictedTargetPos = *targetPosition;
  utilityState.lastLaunch.actualTargetVel = *targetPosition - *startPosition;
  utilityState.lastLaunch.actualTargetVel.Normalize();
  utilityState.lastLaunch.linearSpeed = trajectoryParameters->speed;
  utilityState.lastLaunch.launchTime = startTime;
  return true;
}

// DIA: idAI2::LeadTargetWithLinearProjectile
bool idAI2::LeadTargetWithLinearProjectile(
    const idAIProjectileInfo* info, const idVec3* currentTargetPosition,
    const idVec3* currentTargetVelocity, const idVec3* projectileStart,
    idVec3* extrapolatedPosition, idVec3* directionToExtrapolatedPosition) {
  if (info == nullptr || currentTargetPosition == nullptr ||
      currentTargetVelocity == nullptr || projectileStart == nullptr ||
      extrapolatedPosition == nullptr ||
      directionToExtrapolatedPosition == nullptr || info->linearSpeed <= 0.0F) {
    return false;
  }
  idVec3 previousDirection = info->actualTargetVel;
  idVec3 currentDirection = *currentTargetVelocity;
  const bool previousMoving = previousDirection.Normalize() > 0.001F;
  const bool currentMoving = currentDirection.Normalize() > 0.001F;
  const bool sameDirection = previousMoving && currentMoving &&
                             (previousDirection * currentDirection) >= 0.0F;
  const idVec3 target = sameDirection
                            ? *currentTargetPosition
                            : (info->actualTargetPos + *currentTargetPosition) *
                                  0.5F;
  float lag = 1.0F;
  float animationDelay = 0.0F;
  TargetMovingPoint(projectileStart, info->linearSpeed, &target,
                    currentTargetVelocity, &lag, &animationDelay,
                    directionToExtrapolatedPosition, extrapolatedPosition);
  return sameDirection;
}

// DIA: idAI2::LaunchLinearProjectile
bool idAI2::LaunchLinearProjectile(const idDeclProjectile* projectileDecl,
                                   float speed, const idVec3* startPosition,
                                   const idVec3* targetPosition,
                                   idVec3* launchDirection, int startTime) {
  if (projectileDecl == nullptr || !projectileDecl->spawnable || speed <= 0.0F ||
      startPosition == nullptr || targetPosition == nullptr ||
      launchDirection == nullptr) {
    return false;
  }
  *launchDirection = *targetPosition - *startPosition;
  launchDirection->Normalize();
  utilityState.lastLaunch.actualTargetPos = *targetPosition;
  utilityState.lastLaunch.predictedTargetPos = *targetPosition;
  utilityState.lastLaunch.actualTargetVel = *launchDirection;
  utilityState.lastLaunch.linearSpeed = speed;
  utilityState.lastLaunch.launchTime = startTime;
  return true;
}

// DIA: idAI2::GetModelSpaceJointTransformForAnimEvent
const idCachedJoint* idAI2::GetModelSpaceJointTransformForAnimEvent(
    const idEventDef* eventDefinition, const idAIAnimNodePlayback* animation,
    int cachedIndex, idVec3* jointOrigin, idMat3* jointAxis,
    idMilliseconds* eventTime) {
  (void)eventDefinition;
  if (animation == nullptr || cachedIndex < 0 ||
      cachedIndex >= utilityState.cachedJointCount || jointOrigin == nullptr ||
      jointAxis == nullptr || eventTime == nullptr) {
    return nullptr;
  }
  const idCachedJoint& joint =
      utilityState.cachedJoints[static_cast<std::size_t>(cachedIndex)];
  *jointOrigin = joint.translation;
  *jointAxis = joint.axis;
  eventTime->value = 960 * joint.frameNumber / 30;
  return &joint;
}

// DIA: idAI2::GetWorldSpaceJointTransformForAnimEvent
const idCachedJoint* idAI2::GetWorldSpaceJointTransformForAnimEvent(
    const idEventDef* eventDefinition, const idAIAnimNodePlayback* animation,
    int cachedIndex, idVec3* worldOrigin, idMat3* worldAxis,
    idMilliseconds* eventTime) {
  idVec3 localOrigin{};
  idMat3 localAxis = IdentityAxis();
  const idCachedJoint* joint = GetModelSpaceJointTransformForAnimEvent(
      eventDefinition, animation, cachedIndex, &localOrigin, &localAxis,
      eventTime);
  if (joint == nullptr || worldOrigin == nullptr || worldAxis == nullptr) {
    return nullptr;
  }
  const idMat3& entityAxis = *GetPresentableAxis();
  *worldOrigin = EntityOrigin(*this) + entityAxis * localOrigin;
  (*worldAxis)[0] = entityAxis * localAxis[0];
  (*worldAxis)[1] = entityAxis * localAxis[1];
  (*worldAxis)[2] = entityAxis * localAxis[2];
  return joint;
}

// DIA: idAI2::SetupGrenadeThrow
bool idAI2::SetupGrenadeThrow(idScenePoint_Combat* combatScenePoint,
                              const idAIPos* grenadePosition) {
  if (combatScenePoint == nullptr || grenadePosition == nullptr ||
      !grenadePosition->valid) {
    return false;
  }
  idAIProjectileInfo info;
  if (!SelectThrowable(0, &grenadePosition->pos, 0, &info)) return false;
  info.predictedTargetPos = grenadePosition->pos + idVec3{0.0F, 0.0F, 16.0F};
  info.linearSpeed = 1.0F;
  utilityState.selectedProjectile = info;
  utilityState.trajectoryHasResults.fill(true);
  return true;
}
