#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#include "engine/gamelib/animstack/coneconstraint.h"
#include "shared/idlib/math/mathlib.h"
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI

#include <cmath>
#include <xmmintrin.h>

extern "C" idMat3* ConeConstraintMat3FromHalfAngle(
    idMat3* result, const idVec3* axis, float sineHalfAngle,
    float cosineHalfAngle);
extern "C" idQuat* ConeConstraintMultiplyQuat(
    const idQuat* left, idQuat* result, const idQuat* right);

#if defined(_MSC_VER)
#pragma comment(linker,                                                        \
                "/alternatename:ConeConstraintMat3FromHalfAngle="            \
                "??0idMat3@@QEAA@AEBVidVec3@@MM@Z")
#pragma comment(linker,                                                        \
                "/alternatename:ConeConstraintMultiplyQuat="                \
                "??DidQuat@@QEBA?AV0@AEBV0@@Z")
#endif

namespace {
constexpr float kTwoPi = 6.2831855f;
constexpr float kInverseTwoPi = 0.15915494f;

float WrapAngle(float angle) {
  if (angle < 0.0f || angle >= kTwoPi) {
    angle -= ::floorf(angle * kInverseTwoPi) * kTwoPi;
  }
  return angle;
}

float RetailSquareRoot(float value) {
  return _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
      _mm_set_ss(value), idMath::vector_float_smallestNonDenorm)));
}

float RetailInverseLength(float x, float y, float z) {
  const float squaredXY = y * y + x * x;
  return 1.0f / RetailSquareRoot(squaredXY + z * z);
}

idVec3 MultiplyVectorMatrix(const idVec3& vector, const idMat3& matrix) {
  return {
      vector.y * matrix.mat[1].x + vector.x * matrix.mat[0].x +
          vector.z * matrix.mat[2].x,
      vector.y * matrix.mat[1].y + vector.x * matrix.mat[0].y +
          vector.z * matrix.mat[2].y,
      vector.y * matrix.mat[1].z + vector.x * matrix.mat[0].z +
          vector.z * matrix.mat[2].z};
}
} // namespace

idConeConstraint_Vec3::idConeConstraint_Vec3(
    const idConeConstraint_Vec3& other) {
  current.x = other.current.x;
  current.y = other.current.y;
  current.z = other.current.z;
  center.x = other.center.x;
  center.y = other.center.y;
  center.z = other.center.z;
  limitAngleDot = other.limitAngleDot;
  halfLimitAngleCosine = other.halfLimitAngleCosine;
  halfLimitAngleSine = other.halfLimitAngleSine;
}

idConeConstraint_Quat::idConeConstraint_Quat(
    const idConeConstraint_Quat& other) {
  current.x = other.current.x;
  current.y = other.current.y;
  current.z = other.current.z;
  current.w = other.current.w;
  center.x = other.center.x;
  center.y = other.center.y;
  center.z = other.center.z;
  center.w = other.center.w;
  limitAngleDot = other.limitAngleDot;
  halfLimitAngleCosine = other.halfLimitAngleCosine;
  halfLimitAngleSine = other.halfLimitAngleSine;
}

idConeConstraint_Vec3::idConeConstraint_Vec3(
    const idVec3& center_, const idRadians maxAngle_) {
  current = center_;
  center = center_;
  const float limitAngle = WrapAngle(maxAngle_.value);
  limitAngleDot = ::cosf(limitAngle);
  const float halfAngleForCosine = WrapAngle(maxAngle_.value * 0.5f);
  halfLimitAngleCosine = ::cosf(halfAngleForCosine);
  const float halfAngleForSine = WrapAngle(maxAngle_.value * 0.5f);
  halfLimitAngleSine = ::sinf(halfAngleForSine);
}

void idConeConstraint_Vec3::RotateTo(const idVec3& dir, float lerpRate) {
  float targetX = dir.x;
  float targetY = dir.y;
  float targetZ = dir.z;
  const float centerDotDirection =
      dir.y * center.y + center.x * dir.x + dir.z * center.z;
  if (limitAngleDot > centerDotDirection) {
    const float axisX = center.z * dir.y - center.y * dir.z;
    const float axisY = center.x * dir.z - center.z * dir.x;
    const float axisZ = center.y * dir.x - center.x * dir.y;
    const float inverseAxisLength = RetailInverseLength(axisX, axisY, axisZ);
    const idVec3 axis{axisX * inverseAxisLength,
                      axisY * inverseAxisLength,
                      axisZ * inverseAxisLength};
    idMat3 rotation;
    ConeConstraintMat3FromHalfAngle(&rotation, &axis, halfLimitAngleSine,
                                    halfLimitAngleCosine);
    const idVec3 constrained = MultiplyVectorMatrix(center, rotation);
    targetX = constrained.x;
    targetY = constrained.y;
    targetZ = constrained.z;
  }

  const float nextX = current.x + lerpRate * (targetX - current.x);
  const float nextY = current.y + (targetY - current.y) * lerpRate;
  const float nextZ = current.z + (targetZ - current.z) * lerpRate;
  current.y = nextY;
  current.z = nextZ;
  current.x = nextX;
  const float inverseLength = RetailInverseLength(nextX, nextY, nextZ);
  current.y = nextY * inverseLength;
  current.z = nextZ * inverseLength;
  current.x = inverseLength * nextX;
}

idConeConstraint_Quat::idConeConstraint_Quat(
    const idQuat& center_, const idRadians maxAngle_) {
  current = center_;
  center = center_;
  const float limitAngle = WrapAngle(maxAngle_.value);
  limitAngleDot = ::cosf(limitAngle);
  const float halfAngleForCosine = WrapAngle(maxAngle_.value * 0.5f);
  halfLimitAngleCosine = ::cosf(halfAngleForCosine);
  const float halfAngleForSine = WrapAngle(maxAngle_.value * 0.5f);
  halfLimitAngleSine = ::sinf(halfAngleForSine);
}

void idConeConstraint_Quat::RotateTo(const idVec3& dir, float lerpRate) {
  const idVec3 currentForward = current.ToForward();
  const float directionDotCurrent =
      dir.y * currentForward.y + dir.x * currentForward.x +
      dir.z * currentForward.z;
  if (directionDotCurrent > -1.0f && directionDotCurrent < 1.0f) {
    const float halfRate = lerpRate * 0.5f;
    const float blendedX =
        (dir.x - currentForward.x) * halfRate + currentForward.x;
    const float blendedY =
        (dir.y - currentForward.y) * halfRate + currentForward.y;
    const float blendedZ =
        (dir.z - currentForward.z) * halfRate + currentForward.z;
    const float inverseBlendedLength =
        RetailInverseLength(blendedX, blendedY, blendedZ);
    const float normalizedX = blendedX * inverseBlendedLength;
    const float normalizedY = blendedY * inverseBlendedLength;
    const float normalizedZ = blendedZ * inverseBlendedLength;
    const float normalizedDotCurrent =
        normalizedY * currentForward.y + normalizedX * currentForward.x +
        normalizedZ * currentForward.z;
    const float sine = !(normalizedDotCurrent >= 1.0f)
                           ? RetailSquareRoot(
                                 1.0f - normalizedDotCurrent *
                                            normalizedDotCurrent)
                           : 0.0f;
    const float axisX =
        normalizedY * currentForward.z - normalizedZ * currentForward.y;
    const float axisY =
        normalizedZ * currentForward.x - currentForward.z * normalizedX;
    const float axisZ =
        currentForward.y * normalizedX - normalizedY * currentForward.x;
    const float inverseAxisLength = RetailInverseLength(axisX, axisY, axisZ);
    const idQuat rotation{axisX * inverseAxisLength * sine,
                          axisY * inverseAxisLength * sine,
                          axisZ * inverseAxisLength * sine,
                          normalizedDotCurrent};
    idQuat result;
    ConeConstraintMultiplyQuat(&current, &result, &rotation);
    current = result;
    current.Normalize();
  }

  const idVec3 centerForward = center.ToForward();
  const idVec3 constrainedForward = current.ToForward();
  const float centerDotCurrent =
      centerForward.y * constrainedForward.y +
      centerForward.x * constrainedForward.x +
      centerForward.z * constrainedForward.z;
  if (limitAngleDot > centerDotCurrent) {
    const float axisX = constrainedForward.y * centerForward.z -
                        constrainedForward.z * centerForward.y;
    const float axisY = constrainedForward.z * centerForward.x -
                        centerForward.z * constrainedForward.x;
    const float axisZ = centerForward.y * constrainedForward.x -
                        constrainedForward.y * centerForward.x;
    const float inverseAxisLength = RetailInverseLength(axisX, axisY, axisZ);
    const idQuat rotation{axisX * inverseAxisLength * halfLimitAngleSine,
                          axisY * inverseAxisLength * halfLimitAngleSine,
                          axisZ * inverseAxisLength * halfLimitAngleSine,
                          halfLimitAngleCosine};
    idQuat result;
    ConeConstraintMultiplyQuat(&center, &result, &rotation);
    current = result;
    current.Normalize();
  }
}
