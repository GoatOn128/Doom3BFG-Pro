#include "engine/gamelib/animstack/animweb/animwebutils.h"

#include "shared/idlib/math/mathlib.h"

#include <cmath>
#include <xmmintrin.h>

float LerpToWithScale(float cur, float dest, float scale) {
  constexpr float epsilon = 0.000001f;
  const float difference = dest - cur;
  if (!(difference > -epsilon && difference < epsilon)) {
    return difference * scale + cur;
  }
  return dest;
}

idVec3 idQuat::operator*(const idVec3& vector) const {
  const float zz = z * z;
  const float zx2 = (z * x) * 2.0f;
  const float yy = y * y;
  const float xy2 = (x * y) * 2.0f;
  const float yz2 = (y * z) * 2.0f;
  const float xx = x * x;
  const float wx2 = (w * x) * 2.0f;
  const float wy2 = (w * y) * 2.0f;
  const float ww = w * w;
  const float wz2 = (w * z) * 2.0f;
  const float yFromZ = (yz2 + wx2) * vector.z;
  const float zFromZ = ((zz - (yy + xx)) + ww) * vector.z;

  idVec3 result;
  result.x = ((((xx - yy) - zz) + ww) * vector.x +
              (wz2 + xy2) * vector.y) +
             (zx2 - wy2) * vector.z;
  result.z = ((wy2 + zx2) * vector.x + (yz2 - wx2) * vector.y) +
             zFromZ;
  result.y = ((((yy - xx) - zz) + ww) * vector.y +
              (xy2 - wz2) * vector.x) +
             yFromZ;
  return result;
}

float LerpToWithRate(float cur, float dest, float absRate) {
  if (dest > cur) {
    const float next = cur + absRate;
    if (cur > next) return cur;
    if (!(next > dest)) return next;
    return dest;
  }
  if (!(cur > dest)) return cur;
  const float next = cur - absRate;
  if (dest > next) return dest;
  if (!(next > cur)) return next;
  return cur;
}

bool UpdateWalkBlend(const idVec3& velocity, const idMat3& axis,
                     float lerpScale, float& blendBack, float& blendRight,
                     float& blendAngle, float& sync8index1,
                     float& sync8index2, float& sync8blendAngle,
                     float& fusionMoveAngle) {
  const float speedSquared = velocity.x * velocity.x +
                             velocity.y * velocity.y +
                             velocity.z * velocity.z;
  const __m128 speedVector = _mm_sqrt_ps(_mm_max_ps(
      _mm_set1_ps(speedSquared),
      _mm_set1_ps(idMath::FLT_SMALLEST_NON_DENORMAL)));
  const float speed = _mm_cvtss_f32(speedVector);
  if (speed <= 0.1f) return false;

  const float inverseSpeed = 1.0f / speed;
  float forward = (velocity.y * axis.mat[0].y +
                   velocity.x * axis.mat[0].x +
                   velocity.z * axis.mat[0].z) *
                  inverseSpeed;
  float right = (velocity.y * axis.mat[1].y +
                 velocity.x * axis.mat[1].x +
                 velocity.z * axis.mat[1].z) *
                inverseSpeed;
  if (forward < -1.0f) {
    forward = -1.0f;
  } else if (forward > 1.0f) {
    forward = 1.0f;
  }
  if (right < -1.0f) {
    right = -1.0f;
  } else if (right > 1.0f) {
    right = 1.0f;
  }

  float moveAngle = 0.0f;
  const float directionMagnitude = static_cast<float>(std::fabs(
      static_cast<double>(right * right + forward * forward)));
  if (!(directionMagnitude <= idMath::FLT_SMALLEST_NON_DENORMAL)) {
    moveAngle = -::atan2f(right, forward);
    if (moveAngle < 0.0f) moveAngle += 2.0f * idMath::PI;
  }

  float fusionDelta = moveAngle * (1.0f / (2.0f * idMath::PI)) -
                      fusionMoveAngle;
  if (fusionDelta < -0.5f) {
    fusionDelta += 1.0f;
  } else if (fusionDelta > 0.5f) {
    fusionDelta -= 1.0f;
  }
  fusionMoveAngle += fusionDelta * lerpScale;
  if (fusionMoveAngle < 0.0f || fusionMoveAngle > 1.0f) {
    fusionMoveAngle -= ::floorf(fusionMoveAngle);
  }

  const float angle = ::acosf(forward) * idMath::M_RAD2DEG;
  float backTarget;
  float angleTarget;
  const bool forwardIsNonnegative = forward >= 0.0f;
  if (!forwardIsNonnegative) {
    backTarget = 1.0f;
    angleTarget = (angle - 90.0f) / 90.0f;
  } else {
    backTarget = 0.0f;
    angleTarget = 1.0f - angle / 90.0f;
  }
  const bool rightIsNonnegative = right >= 0.0f;
  const float rightTarget = rightIsNonnegative ? 0.0f : 1.0f;
  angleTarget = 1.0f - angleTarget;

  blendBack = LerpToWithScale(blendBack, backTarget, lerpScale);
  blendRight = LerpToWithScale(blendRight, rightTarget, lerpScale);
  blendAngle = LerpToWithScale(blendAngle, angleTarget, lerpScale);

  const int angleSegment = static_cast<int>(angle / 45.0f);
  int firstIndex = 7 - angleSegment;
  const float segmentBlend =
      (angle - static_cast<float>(angleSegment) * 45.0f) / 45.0f;
  sync8blendAngle = segmentBlend;
  if (!rightIsNonnegative) {
    firstIndex = 7 - firstIndex;
  } else {
    sync8blendAngle = 1.0f - segmentBlend;
  }
  sync8index1 = static_cast<float>(firstIndex);
  sync8index2 = static_cast<float>((firstIndex + 1) & 7);
  return true;
}
