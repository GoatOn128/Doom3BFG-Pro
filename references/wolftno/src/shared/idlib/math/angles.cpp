#include "vector.h"

#include "mathlib.h"

#include <cmath>

namespace {
float NormalizeComponent360(float value) {
  value -= std::floor(value / 360.0f) * 360.0f;
  return value >= 360.0f ? value - 360.0f : value;
}
} // namespace

idAngles& idAngles::Normalize360() {
  pitch = NormalizeComponent360(pitch);
  yaw = NormalizeComponent360(yaw);
  roll = NormalizeComponent360(roll);
  return *this;
}

idAngles& idAngles::Normalize180() {
  Normalize360();
  if (pitch > 180.0f)
    pitch -= 360.0f;
  if (yaw > 180.0f)
    yaw -= 360.0f;
  if (roll > 180.0f)
    roll -= 360.0f;
  return *this;
}

void idAngles::ToVectors(idVec3* forward, idVec3* right, idVec3* up) const {
  const float pitchRadians = pitch * idMath::M_DEG2RAD;
  const float yawRadians = yaw * idMath::M_DEG2RAD;
  const float rollRadians = roll * idMath::M_DEG2RAD;
  const float sp = std::sin(pitchRadians), cp = std::cos(pitchRadians);
  const float sy = std::sin(yawRadians), cy = std::cos(yawRadians);
  const float sr = std::sin(rollRadians), cr = std::cos(rollRadians);
  if (forward)
    *forward = {cp * cy, cp * sy, -sp};
  if (right)
    *right = {-sr * sp * cy + cr * sy, -sr * sp * sy - cr * cy, -sr * cp};
  if (up)
    *up = {cr * sp * cy + sr * sy, cr * sp * sy - sr * cy, cr * cp};
}

idVec3 idAngles::ToForward() const {
  idVec3 result;
  ToVectors(&result);
  return result;
}

idQuat idAngles::ToQuat() const {
  const float halfPitch = pitch * idMath::M_DEG2RAD * 0.5f;
  const float halfYaw = yaw * idMath::M_DEG2RAD * 0.5f;
  const float halfRoll = roll * idMath::M_DEG2RAD * 0.5f;
  const float sp = std::sin(halfPitch), cp = std::cos(halfPitch);
  const float sy = std::sin(halfYaw), cy = std::cos(halfYaw);
  const float sr = std::sin(halfRoll), cr = std::cos(halfRoll);
  return {cr * sp * sy - sr * cp * cy, -cr * sp * cy - sr * cp * sy, sr * sp * cy - cr * cp * sy,
          cr * cp * cy + sr * sp * sy};
}

idRotation idAngles::ToRotation() const {
  return ToQuat().ToRotation();
}

idMat3 idAngles::ToMat3() const {
  idVec3 forward, right, up;
  ToVectors(&forward, &right, &up);
  idMat3 result;
  result.mat[0] = forward;
  result.mat[1] = -right;
  result.mat[2] = up;
  return result;
}

idVec3 idAngles::ToAngularVelocity() const {
  return ToRotation().ToAngularVelocity();
}
