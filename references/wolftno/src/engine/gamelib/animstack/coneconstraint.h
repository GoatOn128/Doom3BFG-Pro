#pragma once

#include "shared/idlib/math/vector.h"

#include <cstddef>

#ifndef ID_RADIANS_UNIQUE_T_DEFINED
#define ID_RADIANS_UNIQUE_T_DEFINED
enum RadiansUnique_t : int { RADIANS_UNIQUE };
#endif

#ifndef ID_TYPESAFE_NUMBER_DEFINED
#define ID_TYPESAFE_NUMBER_DEFINED
template <typename T, typename UniqueTag> class idTypesafeNumber {
public:
  constexpr idTypesafeNumber() : value{} {}
  constexpr idTypesafeNumber(T input) : value(input) {}
  constexpr idTypesafeNumber(const idTypesafeNumber& other)
      : value(other.value) {}
  constexpr operator T() const { return value; }

  T value;
};
#endif

using idRadians = idTypesafeNumber<float, RadiansUnique_t>;

class idConeConstraint_Vec3 {
public:
  idConeConstraint_Vec3() = default;
  idConeConstraint_Vec3(const idConeConstraint_Vec3& other);
  idConeConstraint_Vec3(const idVec3& center_, const idRadians maxAngle_);

  void RotateTo(const idVec3& dir, float lerpRate);

  void SetCurrent(const idVec3& value) { current = value; }
  const idVec3& GetCurrent() const { return current; }
  void SetCenter(const idVec3& value) { center = value; }
  const idVec3& GetCenter() const { return center; }
  float GetLimitAngleDot() const { return limitAngleDot; }

  idVec3 current;
  idVec3 center;
  float limitAngleDot;
  float halfLimitAngleCosine;
  float halfLimitAngleSine;
};

class idConeConstraint_Quat {
public:
  idConeConstraint_Quat() = default;
  idConeConstraint_Quat(const idConeConstraint_Quat& other);
  idConeConstraint_Quat(const idQuat& center_, const idRadians maxAngle_);

  void RotateTo(const idVec3& dir, float lerpRate);
  void SetLimitAngle(idRadians angle);

  void SetCurrent(const idQuat& value) { current = value; }
  const idQuat& GetCurrent() const { return current; }
  void SetCenter(const idQuat& value) { center = value; }
  const idQuat& GetCenter() const { return center; }
  float GetLimitAngleDot() const { return limitAngleDot; }

  idQuat current;
  idQuat center;
  float limitAngleDot;
  float halfLimitAngleCosine;
  float halfLimitAngleSine;
};

static_assert(sizeof(idRadians) == 0x04, "idRadians ABI drift");
static_assert(offsetof(idConeConstraint_Vec3, current) == 0x00,
              "vector constraint current offset drift");
static_assert(offsetof(idConeConstraint_Vec3, center) == 0x0c,
              "vector constraint center offset drift");
static_assert(offsetof(idConeConstraint_Vec3, limitAngleDot) == 0x18,
              "vector constraint dot offset drift");
static_assert(offsetof(idConeConstraint_Vec3, halfLimitAngleCosine) == 0x1c,
              "vector constraint half-cosine offset drift");
static_assert(offsetof(idConeConstraint_Vec3, halfLimitAngleSine) == 0x20,
              "vector constraint half-sine offset drift");
static_assert(sizeof(idConeConstraint_Vec3) == 0x24,
              "idConeConstraint_Vec3 ABI drift");
static_assert(offsetof(idConeConstraint_Quat, current) == 0x00,
              "quaternion constraint current offset drift");
static_assert(offsetof(idConeConstraint_Quat, center) == 0x10,
              "quaternion constraint center offset drift");
static_assert(offsetof(idConeConstraint_Quat, limitAngleDot) == 0x20,
              "quaternion constraint dot offset drift");
static_assert(offsetof(idConeConstraint_Quat, halfLimitAngleCosine) == 0x24,
              "quaternion constraint half-cosine offset drift");
static_assert(offsetof(idConeConstraint_Quat, halfLimitAngleSine) == 0x28,
              "quaternion constraint half-sine offset drift");
static_assert(sizeof(idConeConstraint_Quat) == 0x2c,
              "idConeConstraint_Quat ABI drift");
