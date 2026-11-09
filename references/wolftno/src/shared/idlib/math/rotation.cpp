#include "vector.h"

#include "mathlib.h"

#include <cmath>

idRotation::idRotation()
    : origin(vec3_origin), vec{0.0f, 0.0f, 1.0f}, angle(0.0f), axis(mat3_identity),
      axisValid(false) {}

idRotation::idRotation(const idVec3& rotationOrigin, const idVec3& rotationAxis, float angleDegrees)
    : origin(rotationOrigin), vec(rotationAxis), angle(angleDegrees), axis(mat3_identity),
      axisValid(false) {
  vec.Normalize();
}

const idMat3& idRotation::ToMat3() const {
  if (!axisValid) {
    const float halfAngle = angle * idMath::M_DEG2RAD * 0.5f;
    axis = idMat3(vec, std::sin(halfAngle), std::cos(halfAngle));
    axisValid = true;
  }
  return axis;
}

void idRotation::Normalize180() {
  angle -= std::floor(angle / 360.0f) * 360.0f;
  if (angle > 180.0f)
    angle -= 360.0f;
  if (angle < -180.0f)
    angle += 360.0f;
  axisValid = false;
}

idAngles idRotation::ToAngles() const {
  return ToMat3().ToAngles();
}

idVec3 idRotation::ToAngularVelocity() const {
  return vec * (-angle * idMath::M_DEG2RAD);
}

idVec3 idRotation::operator*(const idVec3& point) const {
  return origin + ToMat3() * (point - origin);
}
