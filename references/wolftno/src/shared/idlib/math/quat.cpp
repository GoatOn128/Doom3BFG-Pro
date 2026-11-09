#include "vector.h"

#include "mathlib.h"

#include <algorithm>
#include <cmath>

float idMath::ATan16(float y, float x) {
  return std::atan2(y, x);
}

idQuat& idQuat::Normalize() {
  const float lengthSqr = *this * *this;
  if (lengthSqr > idMath::FLT_SMALLEST_NON_DENORMAL) {
    const float scale = 1.0f / std::sqrt(lengthSqr);
    x *= scale;
    y *= scale;
    z *= scale;
    w *= scale;
  }
  return *this;
}

idRotation idQuat::ToRotation() const {
  const float boundedW = std::clamp(w, -1.0f, 1.0f);
  if (boundedW >= 1.0f - 4.0f * idMath::FLOAT_EPSILON) {
    return idRotation(vec3_origin, {0.0f, 0.0f, 1.0f}, 0.0f);
  }
  idVec3 rotationAxis{x, y, z};
  rotationAxis.Normalize();
  const float angleDegrees = 2.0f * std::acos(boundedW) * idMath::M_RAD2DEG;
  return idRotation(vec3_origin, rotationAxis, angleDegrees);
}

idMat3 idQuat::ToMat3() const {
  const float x2 = x + x;
  const float y2 = y + y;
  const float z2 = z + z;
  const float xx = x * x2;
  const float xy = x * y2;
  const float xz = x * z2;
  const float yy = y * y2;
  const float yz = y * z2;
  const float zz = z * z2;
  const float wx = w * x2;
  const float wy = w * y2;
  const float wz = w * z2;
  idMat3 result;
  result.mat[0] = {1.0f - (yy + zz), xy - wz, xz + wy};
  result.mat[1] = {xy + wz, 1.0f - (xx + zz), yz - wx};
  result.mat[2] = {xz - wy, yz + wx, 1.0f - (xx + yy)};
  return result;
}

idVec3 idQuat::ToForward() const {
  return {1.0f - 2.0f * (y * y + z * z), 2.0f * (x * y - w * z), 2.0f * (w * y + x * z)};
}

idVec3 idQuat::ToAngularVelocity() const {
  idVec3 axis{x, y, z};
  axis.Normalize();
  const float halfAngle = std::acos(std::clamp(w, -1.0f, 1.0f));
  return axis * (-2.0f * halfAngle);
}

idQuat& idQuat::Slerp(const idQuat& from, const idQuat& to, float t) {
  if (t <= 0.0f)
    return *this = from;
  if (t >= 1.0f)
    return *this = to;
  float cosine = from * to;
  float toScaleSign = 1.0f;
  if (cosine < 0.0f) {
    cosine = -cosine;
    toScaleSign = -1.0f;
  }
  float fromScale;
  float toScale;
  if (1.0f - cosine <= 1.0e-6f) {
    fromScale = 1.0f - t;
    toScale = t;
  } else {
    const float angle = std::acos(std::clamp(cosine, -1.0f, 1.0f));
    const float inverseSine = 1.0f / std::sin(angle);
    fromScale = std::sin((1.0f - t) * angle) * inverseSine;
    toScale = std::sin(t * angle) * inverseSine;
  }
  toScale *= toScaleSign;
  *this = from * fromScale + to * toScale;
  return *this;
}

idQuat& idQuat::Lerp(const idQuat& from, const idQuat& to, float t) {
  if (t <= 0.0f)
    return *this = from;
  if (t >= 1.0f)
    return *this = to;
  const float signedT = (from * to) < 0.0f ? -t : t;
  *this = from * (1.0f - t) + to * signedT;
  return Normalize();
}

idAngles idQuat::ToAngles() const {
  return ToMat3().ToAngles();
}
