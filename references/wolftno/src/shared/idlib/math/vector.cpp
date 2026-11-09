#include "vector.h"

#include "mathlib.h"
#include "shared/idlib/text/str.h"

#include <algorithm>
#include <cmath>

const idVec3 vec3_origin{0.0f, 0.0f, 0.0f};
const idVec3 vec3_infinity{1.0e30f, 1.0e30f, 1.0e30f};
const idVec6 vec6_infinity{{1.0e30f, 1.0e30f, 1.0e30f, 1.0e30f, 1.0e30f, 1.0e30f}};

const char* idAngles::ToString(int precision) const {
  return idStr::FloatArrayToString(&pitch, 3, precision);
}

const char* idVec2::ToString(int precision) const {
  return idStr::FloatArrayToString(&x, 2, precision);
}

const char* idVec3::ToString(int precision) const {
  return idStr::FloatArrayToString(&x, 3, precision);
}

float idVec3::ToYaw() const {
  if (std::fabs(x) <= idMath::FLT_SMALLEST_NON_DENORMAL &&
      std::fabs(y) <= idMath::FLT_SMALLEST_NON_DENORMAL) {
    return 0.0f;
  }
  float yaw = std::atan2(y, x) * idMath::M_RAD2DEG;
  return yaw < 0.0f ? yaw + 360.0f : yaw;
}

float idVec3::ToPitch() const {
  const float horizontalSqr = x * x + y * y;
  if (horizontalSqr <= idMath::FLT_SMALLEST_NON_DENORMAL) {
    return z >= 0.0f ? 270.0f : 90.0f;
  }
  float pitch = -std::atan2(z, std::sqrt(horizontalSqr)) * idMath::M_RAD2DEG;
  return pitch < 0.0f ? pitch + 360.0f : pitch;
}

idAngles idVec3::ToAngles() const {
  return {ToPitch(), ToYaw(), 0.0f};
}

idMat3 idVec3::ToMat3() const {
  idMat3 result{};
  const float horizontalSqr = x * x + y * y;
  if (horizontalSqr > idMath::FLT_SMALLEST_NON_DENORMAL) {
    result.mat[0] = *this;
    result.mat[0].Normalize();
    const float inverseHorizontal = 1.0f / std::sqrt(horizontalSqr);
    result.mat[1] = {-y * inverseHorizontal, x * inverseHorizontal, 0.0f};
    result.mat[2] = result.mat[0].Cross(result.mat[1]);
  } else {
    const float sign = z >= 0.0f ? 1.0f : -1.0f;
    result.mat[0] = {0.0f, 0.0f, sign};
    result.mat[1] = {1.0f, 0.0f, 0.0f};
    result.mat[2] = {0.0f, sign, 0.0f};
  }
  return result;
}

idMat3 idVec3::ToZAxis() const {
  idMat3 result{};
  const float horizontalSqr = x * x + y * y;
  if (horizontalSqr > idMath::FLT_SMALLEST_NON_DENORMAL) {
    result.mat[2] = *this;
    result.mat[2].Normalize();
    const float inverseHorizontal = 1.0f / std::sqrt(horizontalSqr);
    result.mat[0] = {-y * inverseHorizontal, x * inverseHorizontal, 0.0f};
    result.mat[1] = result.mat[0].Cross(result.mat[2]);
  } else {
    const float sign = z >= 0.0f ? 1.0f : -1.0f;
    result.mat[0] = {1.0f, 0.0f, 0.0f};
    result.mat[1] = {0.0f, sign, 0.0f};
    result.mat[2] = {0.0f, 0.0f, sign};
  }
  return result;
}

void idVec3::ProjectSelfOntoSphere(float radius) {
  const float radiusSqr = radius * radius;
  const float length = std::sqrt(std::max(LengthSqr(), idMath::FLT_SMALLEST_NON_DENORMAL));
  if (radiusSqr * 0.5f <= length) {
    z = length <= idMath::FLT_SMALLEST_NON_DENORMAL ? 0.0f : radiusSqr / (2.0f * std::sqrt(length));
  } else {
    z = std::sqrt(std::max(radiusSqr - length, idMath::FLT_SMALLEST_NON_DENORMAL));
  }
}

const idVec3& idVec3::RotateSelf(const idVec3& axis, float angleDegrees) {
  *this = idRotation(vec3_origin, axis, -angleDegrees) * *this;
  return *this;
}

const idVec3& idVec3::RotateSelfRadians(const idVec3& axis, float angleRadians) {
  return RotateSelf(axis, angleRadians * idMath::M_RAD2DEG);
}

float idVec3::ToYaw(const idVec3& up) const {
  const idMat3 basis = up.ToZAxis();
  const float forward = *this * basis.mat[0];
  const float right = *this * basis.mat[1];
  if (std::fabs(forward) <= idMath::FLT_SMALLEST_NON_DENORMAL &&
      std::fabs(right) <= idMath::FLT_SMALLEST_NON_DENORMAL) {
    return 0.0f;
  }
  float yaw = std::atan2(right, forward) * idMath::M_RAD2DEG;
  return yaw < 0.0f ? yaw + 360.0f : yaw;
}
void idVec3::NormalVectors(idVec3& left, idVec3& down) const {
  const float d = x * x + y * y;
  if (d == 0.0f) {
    left = {1.0f, 0.0f, 0.0f};
  } else {
    const float inverseLength = 1.0f / std::sqrt(d);
    left = {-y * inverseLength, x * inverseLength, 0.0f};
  }
  down = left.Cross(*this);
}
