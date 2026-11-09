#include "tungsten/game/ai/ai2_core_support.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.14159265358979323846F;
constexpr float kDegreesToRadians = kPi / 180.0F;
constexpr float kRadiansToDegrees = 180.0F / kPi;

float NormalizeSafe(idVec3& value) {
  const float length = value.Length();
  if (length > 0.0F) value *= 1.0F / length;
  return length;
}

idVec3 Cross(const idVec3& lhs, const idVec3& rhs) {
  return {lhs.y * rhs.z - lhs.z * rhs.y,
          lhs.z * rhs.x - lhs.x * rhs.z,
          lhs.x * rhs.y - lhs.y * rhs.x};
}

}  // namespace

// DIA: idRotationTracker::~idRotationTracker
idRotationTracker::~idRotationTracker() = default;

// DIA: idRotationTracker::idRotationTracker
idRotationTracker::idRotationTracker() = default;

// DIA: idRotationTracker::`scalar deleting destructor'

// DIA: ClampOnAxis
void ClampOnAxis(const idVec3& rotationAxis, const idVec3& referenceAxis,
                 const idVec3& idealDirection, float maxDot,
                 float halfMaxDot, idQuat* rotation, float* dot,
                 bool* clamped) {
  idVec3 axis = rotationAxis;
  idVec3 reference = referenceAxis;
  idVec3 ideal = idealDirection - axis * (idealDirection * axis);
  NormalizeSafe(axis);
  NormalizeSafe(reference);
  if (NormalizeSafe(ideal) == 0.0F) ideal = reference;
  const float currentDot = std::clamp(reference * ideal, -1.0F, 1.0F);
  const float limitedDot = std::max(currentDot, maxDot);
  if (dot != nullptr) *dot = limitedDot;
  if (clamped != nullptr) *clamped = currentDot < maxDot;
  if (rotation == nullptr) return;
  idVec3 cross = Cross(reference, ideal);
  if (NormalizeSafe(cross) == 0.0F) cross = axis;
  const float cosineHalf = currentDot < maxDot
                               ? std::clamp(halfMaxDot, -1.0F, 1.0F)
                               : std::sqrt(std::max(0.0F,
                                     (1.0F + currentDot) * 0.5F));
  const float sineHalf = std::sqrt(std::max(0.0F, 1.0F -
                                                     cosineHalf * cosineHalf));
  *rotation = {cross.x * sineHalf, cross.y * sineHalf,
               cross.z * sineHalf, cosineHalf};
}

// DIA: ScaleForDot
float ScaleForDot(float currentDot, float maxDot, float minimumClamp) {
  if (currentDot <= maxDot) return 0.0F;
  const float denominator = std::max(0.000001F, 1.0F - maxDot);
  return std::clamp((currentDot - maxDot) / denominator,
                    std::clamp(minimumClamp, 0.0F, 1.0F), 1.0F);
}

// DIA: idRotationTracker::GetWorldSpaceDir
void idRotationTracker::GetWorldSpaceDir(const idMat3& referenceAxis,
                                         idVec3* output) const {
  (void)referenceAxis;
  if (output != nullptr) *output = worldDirection;
}

// DIA: idRotationTracker::GetWorldSpaceAxis
void idRotationTracker::GetWorldSpaceAxis(const idMat3& referenceAxis,
                                          idMat3* output) const {
  if (output == nullptr) return;
  idVec3 forward = worldDirection;
  if (NormalizeSafe(forward) == 0.0F) forward = referenceAxis[0];
  idVec3 up = referenceAxis[2];
  if (NormalizeSafe(up) == 0.0F) up = {0.0F, 0.0F, 1.0F};
  idVec3 left = Cross(up, forward);
  if (NormalizeSafe(left) == 0.0F) left = referenceAxis[1];
  up = Cross(forward, left);
  NormalizeSafe(up);
  (*output)[0] = forward;
  (*output)[1] = left;
  (*output)[2] = up;
}

// DIA: idRotationTracker::GetConstraints
void idRotationTracker::GetConstraints(range_t* output) const {
  if (output != nullptr) *output = constraints;
}

// DIA: idRotationTracker::SetConstraints
void idRotationTracker::SetConstraints(float maxPitchRadians,
                                       float maxYawRadians) {
  const float pitch = std::max(0.0F, maxPitchRadians) * kRadiansToDegrees;
  const float yaw = std::max(0.0F, maxYawRadians) * kRadiansToDegrees;
  constraints.pitchUp = pitch;
  constraints.pitchDown = pitch;
  constraints.yawLeft = yaw;
  constraints.yawRight = yaw;
}

// DIA: idRotationTracker::SetYawOnlyThreshold
void idRotationTracker::SetYawOnlyThreshold(float value) {
  yawOnlyThreshold = std::max(0.0F, value);
}

// DIA: idRotationTracker::Update
void idRotationTracker::Update(const idVec3& referenceDirection,
                               const idVec3& idealDirection,
                               const idAngles& rate, float elapsedSeconds,
                               bool shouldProjectWhenBehind,
                               bool useAcceleration) {
  idVec3 reference = referenceDirection;
  idVec3 ideal = idealDirection;
  if (NormalizeSafe(reference) == 0.0F || NormalizeSafe(ideal) == 0.0F) return;

  const idVec3 worldUp{0.0F, 0.0F, 1.0F};
  idVec3 left = Cross(worldUp, reference);
  if (NormalizeSafe(left) == 0.0F) left = {0.0F, 1.0F, 0.0F};
  idVec3 up = Cross(reference, left);
  NormalizeSafe(up);
  const float forwardDot = ideal * reference;
  float yaw = std::atan2(ideal * left, forwardDot) * kRadiansToDegrees;
  const float horizontal = std::sqrt(std::max(0.0F,
      forwardDot * forwardDot + (ideal * left) * (ideal * left)));
  float pitch = std::atan2(ideal * up, horizontal) * kRadiansToDegrees;
  if (shouldProjectWhenBehind && forwardDot < 0.0F) {
    yaw = std::copysign(90.0F, yaw == 0.0F ? 1.0F : yaw);
  }
  const float constrainedYaw = std::clamp(
      yaw, -std::max(0.0F, constraints.yawRight),
      std::max(0.0F, constraints.yawLeft));
  const float constrainedPitch = std::clamp(
      pitch, -std::max(0.0F, constraints.pitchDown),
      std::max(0.0F, constraints.pitchUp));
  clampedYaw = constrainedYaw != yaw;
  clampedPitch = constrainedPitch != pitch;
  const float yawRadians = constrainedYaw * kDegreesToRadians;
  const float pitchRadians = constrainedPitch * kDegreesToRadians;
  idVec3 target = reference * (std::cos(yawRadians) * std::cos(pitchRadians)) +
                  left * (std::sin(yawRadians) * std::cos(pitchRadians)) +
                  up * std::sin(pitchRadians);
  NormalizeSafe(target);

  idVec3 current = worldDirection;
  if (NormalizeSafe(current) == 0.0F) current = reference;
  const float dot = std::clamp(current * target, -1.0F, 1.0F);
  const float angle = std::acos(dot) * kRadiansToDegrees;
  const float dt = std::max(0.0F, elapsedSeconds);
  maxVelocity = std::max(0.0F, std::max(std::fabs(rate.pitch),
                                        std::fabs(rate.yaw)));
  acceleration = maxVelocity;
  deceleration = maxVelocity;
  if (useAcceleration) {
    const float stoppingDistance = deceleration > 0.0F
        ? velocity * velocity / (2.0F * deceleration) : 0.0F;
    velocity += (stoppingDistance >= angle ? -deceleration : acceleration) * dt;
    velocity = std::clamp(velocity, 0.0F, maxVelocity);
  } else {
    velocity = maxVelocity;
  }
  const float step = std::min(angle, velocity * dt);
  if (angle <= 0.0001F || step >= angle) {
    worldDirection = target;
    velocity = 0.0F;
  } else {
    worldDirection = current * (1.0F - step / angle) + target * (step / angle);
    NormalizeSafe(worldDirection);
  }
  projectWhenBehind = shouldProjectWhenBehind;
}

// DIA: idRotationTracker::SetConstraints
// The recovered object emits a second line mapping for the radians overload.

// DIA: idAimTracker::Update
void idAimTracker::Update(const idVec3& referenceDirection,
                          const idVec3& idealDirection, const idAngles& rate,
                          float elapsedSeconds, bool shouldProjectWhenBehind,
                          bool useAcceleration) {
  idRotationTracker::Update(referenceDirection, idealDirection, rate,
                            elapsedSeconds, shouldProjectWhenBehind,
                            useAcceleration);
}

// DIA: idRotationTracker::SetConstraints
void idRotationTracker::SetConstraints(const range_t& value) {
  constraints.yawLeft = std::max(0.0F, value.yawLeft);
  constraints.yawRight = std::max(0.0F, value.yawRight);
  constraints.pitchUp = std::max(0.0F, value.pitchUp);
  constraints.pitchDown = std::max(0.0F, value.pitchDown);
}
