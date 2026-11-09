#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#include "engine/gamelib/physics/physics_rigidbody.h"

#include "engine/framework/common.h"
#include "engine/gamelib/effectphysics/effectphysicsrigidbody.h"
#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/geometry/winding.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/text/str.h"

#undef WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#undef WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

struct contactsResult_t {
  int numContacts;
  std::uint8_t pad[12];
  contactInfo_t contacts[12];
};

static_assert(sizeof(contactsResult_t) == 0x3A0,
              "contactsResult_t ABI drift");

template <>
int idList<contactInfo_t, 77>::Append(const contactInfo_t& value) {
  EnsureCapacity(num + 1);
  if (num >= size) {
    return num - 1;
  }
  list[num] = value;
  return num++;
}

class idCollisionModelManager {
public:
  bool GetMotionContactsResult(trace_t* trace, contactsResult_t* contacts,
                               idCollisionQuery& query, bool peek);
};

extern idCollisionModelManager* collisionModelManager;
extern bool forceIdListsToReallocateEveryAppend;
extern const idVec3 vec3_up;
idRenderDebug* GetGameLibRenderDebug();

idCVar rb_showTimings("rb_showTimings", "0", CVAR_BOOL,
                      "show rigid body cpu usage",
                      static_cast<argCompletionRef_t>(nullptr));
idCVar rb_showBodies("rb_showBodies", "0", CVAR_BOOL, "show rigid bodies",
                     static_cast<argCompletionRef_t>(nullptr));
idCVar rb_showMass("rb_showMass", "0", CVAR_BOOL,
                   "show the mass of each rigid body",
                   static_cast<argCompletionRef_t>(nullptr));
idCVar rb_showInertia("rb_showInertia", "0", CVAR_BOOL,
                      "show the inertia tensor of each rigid body",
                      static_cast<argCompletionRef_t>(nullptr));
idCVar rb_showVelocity("rb_showVelocity", "0", CVAR_BOOL,
                       "show the velocity of each rigid body",
                       static_cast<argCompletionRef_t>(nullptr));
idCVar rb_showActive("rb_showActive", "0", CVAR_BOOL,
                     "show rigid bodies that are not at rest",
                     static_cast<argCompletionRef_t>(nullptr));

namespace {

__declspec(noinline) float InitialMaxAngularVelocity() {
  volatile float pi = idMath::PI;
  return pi * 4.0f;
}

} // namespace

float MAX_ANGULAR_VELOCITY = InitialMaxAngularVelocity();

namespace {

constexpr int kQueryCanceled = 1;
constexpr int kCollideStopSimulation = 1;
constexpr int kCollideIgnoreCollision = 2;
constexpr std::uint64_t kCollisionResultOffsetMask = 0x3fffffffffffffull;

float InverseLength(const idVec3& value) {
  const float squared = ((value.y * value.y) + (value.x * value.x)) +
                        (value.z * value.z);
  const __m128 clamped = _mm_max_ps(
      _mm_set_ss(squared), _mm_set1_ps(idMath::FLT_SMALLEST_NON_DENORMAL));
  return _mm_cvtss_f32(_mm_div_ps(_mm_set_ss(1.0f), _mm_sqrt_ps(clamped)));
}

float Length(const idVec3& value) {
  const float squared = ((value.y * value.y) + (value.x * value.x)) +
                        (value.z * value.z);
  return _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
      _mm_set_ss(squared), _mm_set1_ps(idMath::FLT_SMALLEST_NON_DENORMAL))));
}

idMat3 Transpose(const idMat3& matrix) {
  idMat3 result;
  result[0] = {matrix[0].x, matrix[1].x, matrix[2].x};
  result[1] = {matrix[0].y, matrix[1].y, matrix[2].y};
  result[2] = {matrix[0].z, matrix[1].z, matrix[2].z};
  return result;
}

__declspec(noinline) float RetailMultiply(float left, float right) {
  return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

__declspec(noinline) float RetailSubtract(float left, float right) {
  return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

__declspec(noinline) float RetailAdd(float left, float right) {
  return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(left), _mm_set_ss(right)));
}

idMat3 RetailMatrixMultiply(const idMat3& left, const idMat3& right) {
  idMat3 result;
  result[0].x = RetailAdd(
      RetailAdd(RetailMultiply(right[0].x, left[0].x),
                RetailMultiply(left[0].y, right[1].x)),
      RetailMultiply(left[0].z, right[2].x));
  result[0].y = RetailAdd(
      RetailAdd(RetailMultiply(left[0].y, right[1].y),
                RetailMultiply(right[0].y, left[0].x)),
      RetailMultiply(right[2].y, left[0].z));
  result[0].z = RetailAdd(
      RetailAdd(RetailMultiply(right[1].z, left[0].y),
                RetailMultiply(right[0].z, left[0].x)),
      RetailMultiply(right[2].z, left[0].z));
  result[1].x = RetailAdd(
      RetailAdd(RetailMultiply(right[0].x, left[1].x),
                RetailMultiply(left[1].y, right[1].x)),
      RetailMultiply(left[1].z, right[2].x));
  result[1].y = RetailAdd(
      RetailAdd(RetailMultiply(left[1].y, right[1].y),
                RetailMultiply(right[0].y, left[1].x)),
      RetailMultiply(right[2].y, left[1].z));
  result[1].z = RetailAdd(
      RetailAdd(RetailMultiply(left[1].y, right[1].z),
                RetailMultiply(right[0].z, left[1].x)),
      RetailMultiply(left[1].z, right[2].z));
  result[2].x = RetailAdd(
      RetailAdd(RetailMultiply(left[2].x, right[0].x),
                RetailMultiply(right[1].x, left[2].y)),
      RetailMultiply(right[2].x, left[2].z));
  result[2].y = RetailAdd(
      RetailAdd(RetailMultiply(right[0].y, left[2].x),
                RetailMultiply(right[1].y, left[2].y)),
      RetailMultiply(right[2].y, left[2].z));
  result[2].z = RetailAdd(
      RetailAdd(RetailMultiply(left[2].x, right[0].z),
                RetailMultiply(right[1].z, left[2].y)),
      RetailMultiply(right[2].z, left[2].z));
  return result;
}

idMat3 RetailMatrixMultiplyAssign(const idMat3& left,
                                  const idMat3& right) {
  idMat3 result;
  result[0].x = RetailAdd(
      RetailAdd(RetailMultiply(left[0].x, right[0].x),
                RetailMultiply(left[0].y, right[1].x)),
      RetailMultiply(left[0].z, right[2].x));
  result[0].y = RetailAdd(
      RetailAdd(RetailMultiply(left[0].x, right[0].y),
                RetailMultiply(left[0].y, right[1].y)),
      RetailMultiply(left[0].z, right[2].y));
  result[0].z = RetailAdd(
      RetailAdd(RetailMultiply(left[0].y, right[1].z),
                RetailMultiply(left[0].x, right[0].z)),
      RetailMultiply(left[0].z, right[2].z));
  result[1].x = RetailAdd(
      RetailAdd(RetailMultiply(left[1].y, right[1].x),
                RetailMultiply(left[1].x, right[0].x)),
      RetailMultiply(left[1].z, right[2].x));
  result[1].y = RetailAdd(
      RetailAdd(RetailMultiply(left[1].y, right[1].y),
                RetailMultiply(left[1].x, right[0].y)),
      RetailMultiply(left[1].z, right[2].y));
  result[1].z = RetailAdd(
      RetailAdd(RetailMultiply(left[1].x, right[0].z),
                RetailMultiply(left[1].y, right[1].z)),
      RetailMultiply(left[1].z, right[2].z));
  result[2].x = RetailAdd(
      RetailAdd(RetailMultiply(left[2].y, right[1].x),
                RetailMultiply(left[2].x, right[0].x)),
      RetailMultiply(left[2].z, right[2].x));
  result[2].y = RetailAdd(
      RetailAdd(RetailMultiply(left[2].y, right[1].y),
                RetailMultiply(left[2].x, right[0].y)),
      RetailMultiply(left[2].z, right[2].y));
  result[2].z = RetailAdd(
      RetailAdd(RetailMultiply(left[2].x, right[0].z),
                RetailMultiply(left[2].y, right[1].z)),
      RetailMultiply(left[2].z, right[2].z));
  return result;
}

float RetailAcos(float value) {
  const std::uint32_t bits = std::bit_cast<std::uint32_t>(value);
  const std::uint32_t absoluteBits = bits & 0x7fffffffu;
  const float absolute = std::bit_cast<float>(absoluteBits);
  const int exponent = static_cast<int>((absoluteBits >> 23) & 0xffu) - 127;

  if (exponent < -26) {
    return std::bit_cast<float>(0x3fc90fdbu);
  }
  if (exponent >= 0) {
    if (absoluteBits == 0x3f800000u) {
      return (bits & 0x80000000u) != 0
                 ? std::bit_cast<float>(0x40490fdbu)
                 : 0.0f;
    }
    return std::acosf(value);
  }

  const bool transformed = exponent >= -1;
  float polynomialInput;
  float leading;
  if (transformed) {
    polynomialInput = (1.0f - absolute) * 0.5f;
    leading = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(polynomialInput)));
  } else {
    polynomialInput = absolute * absolute;
    leading = absolute;
  }

  float numerator = std::bit_cast<float>(0xbc5b3fe1u) -
                    polynomialInput * std::bit_cast<float>(0x3b81ce6bu);
  numerator = numerator * polynomialInput -
              std::bit_cast<float>(0x3d678bddu);
  numerator = numerator * polynomialInput +
              std::bit_cast<float>(0x3e3c94dcu);
  numerator *= polynomialInput;
  const float denominator =
      std::bit_cast<float>(0x3f8d6fa5u) -
      polynomialInput * std::bit_cast<float>(0x3f561f0du);
  const float correction = numerator / denominator;

  constexpr double kPi = std::bit_cast<double>(0x400921fb54442d18ull);
  constexpr double kHalfPi = std::bit_cast<double>(0x3ff921fb54442d18ull);
  constexpr double kPiTail = std::bit_cast<double>(0x3c91a62633145c07ull);
  if (!transformed) {
    const float product = correction * value;
    return static_cast<float>(
        kHalfPi - (static_cast<double>(value) -
                   (kPiTail - static_cast<double>(product))));
  }
  if ((bits & 0x80000000u) != 0) {
    const float product = correction * leading;
    return static_cast<float>(
        kPi - ((static_cast<double>(product) - kPiTail +
                static_cast<double>(leading)) *
               2.0));
  }

  const float high = std::bit_cast<float>(
      std::bit_cast<std::uint32_t>(leading) & 0xffff0000u);
  const float product = (leading * 2.0f) * correction;
  const float sqrtCorrection =
      ((polynomialInput - high * high) / (high + leading)) * 2.0f;
  return (sqrtCorrection + product) + high * 2.0f;
}

idQuat MatrixToQuatRetail(const idMat3& matrix) {
  idQuat result;
  const float trace = (matrix[0].x + matrix[1].y) + matrix[2].z;
  if (trace > 0.0f) {
    const float value = trace + 1.0f;
    const float inverse =
        1.0f / _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
                   _mm_set_ss(value), idMath::vector_float_smallestNonDenorm)));
    const float halfInverse = inverse * 0.5f;
    result.x = (matrix[2].y - matrix[1].z) * halfInverse;
    result.w = halfInverse * value;
    result.z = (matrix[1].x - matrix[0].y) * halfInverse;
    result.y = (matrix[0].z - matrix[2].x) * halfInverse;
    return result;
  }

  static constexpr int next[3] = {1, 2, 0};
  int i = matrix[1].y > matrix[0].x ? 1 : 0;
  if (matrix[2].z > matrix[i][i]) {
    i = 2;
  }
  const int j = next[i];
  const int k = next[j];
  const float value =
      (matrix[i][i] - (matrix[k][k] + matrix[j][j])) + 1.0f;
  float scale =
      (1.0f / _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
                    _mm_set_ss(value),
                    idMath::vector_float_smallestNonDenorm)))) *
      0.5f;
  const float difference = matrix[k][j] - matrix[j][k];
  if (difference < 0.0f) {
    scale = -scale;
  }
  result[i] = scale * value;
  result.w = difference * scale;
  result[j] = (matrix[i][j] + matrix[j][i]) * scale;
  result[k] = (matrix[i][k] + matrix[k][i]) * scale;
  return result;
}

idVec3 MatrixToAngularVelocityRetail(const idMat3& matrix) {
  const idQuat quaternion = MatrixToQuatRetail(matrix);
  const float squared = ((quaternion.y * quaternion.y) +
                         (quaternion.x * quaternion.x)) +
                        (quaternion.z * quaternion.z);
  const float inverse =
      1.0f / _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
                 _mm_set_ss(squared), idMath::vector_float_smallestNonDenorm)));
  const float x = quaternion.x * inverse;
  const float y = quaternion.y * inverse;
  const float z = quaternion.z * inverse;
  float halfAngle;
  if (quaternion.w > -1.0f) {
    halfAngle = quaternion.w < 1.0f ? RetailAcos(quaternion.w) : 0.0f;
  } else {
    halfAngle = idMath::PI;
  }
  const float scale = halfAngle * -2.0f;
  return {scale * x, scale * y, scale * z};
}

idMat3 RotationMatrixRetail(const idVec3& axis, float angle) {
  const float halfAngle = (idMath::M_DEG2RAD * 0.5f) * angle;
  const float sine = std::sinf(halfAngle);
  const float cosine = std::cosf(halfAngle);
  const float z = sine * axis.z;
  const float x = sine * axis.x;
  const float y = sine * axis.y;
  const float zw = (z + z) * cosine;
  const float zx = (z + z) * x;
  const float zy = (z + z) * y;
  const float xx = (x + x) * x;
  const float xw = (x + x) * cosine;
  const float yw = (y + y) * cosine;
  const float xy = (y + y) * x;
  const float yy = (y + y) * y;
  const float zz = (z + z) * z;
  idMat3 result;
  result[0] = {1.0f - (zz + yy), xy - zw, yw + zx};
  result[1] = {zw + xy, 1.0f - (zz + xx), zy - xw};
  result[2] = {zx - yw, xw + zy, 1.0f - (yy + xx)};
  return result;
}

__declspec(noinline) float RetailScaleDouble(float value, double scale) {
  const __m128d converted = _mm_cvtss_sd(_mm_setzero_pd(), _mm_set_ss(value));
  return _mm_cvtss_f32(_mm_cvtsd_ss(
      _mm_setzero_ps(), _mm_mul_sd(converted, _mm_set_sd(scale))));
}

bool InverseSelfRetail(idMat3& matrix) {
  const float cofactor00 = RetailSubtract(
      RetailMultiply(matrix[1].y, matrix[2].z),
      RetailMultiply(matrix[1].z, matrix[2].y));
  const float cofactor01 = RetailSubtract(
      RetailMultiply(matrix[2].x, matrix[1].z),
      RetailMultiply(matrix[1].x, matrix[2].z));
  const float cofactor02 = RetailSubtract(
      RetailMultiply(matrix[1].x, matrix[2].y),
      RetailMultiply(matrix[2].x, matrix[1].y));
  const float determinant = RetailAdd(
      RetailAdd(RetailMultiply(matrix[0].x, cofactor00),
                RetailMultiply(matrix[0].y, cofactor01)),
      RetailMultiply(matrix[0].z, cofactor02));
  if (std::fabs(static_cast<double>(determinant)) < 1.0e-14f) {
    return false;
  }

  const double inverseDeterminant =
      1.0 / static_cast<double>(determinant);
  idMat3 inverse;
  inverse[0].x = RetailScaleDouble(cofactor00, inverseDeterminant);
  inverse[0].y = RetailScaleDouble(
      RetailSubtract(RetailMultiply(matrix[0].z, matrix[2].y),
                     RetailMultiply(matrix[0].y, matrix[2].z)),
      inverseDeterminant);
  inverse[0].z = RetailScaleDouble(
      RetailSubtract(RetailMultiply(matrix[0].y, matrix[1].z),
                     RetailMultiply(matrix[0].z, matrix[1].y)),
      inverseDeterminant);
  inverse[1].x = RetailScaleDouble(cofactor01, inverseDeterminant);
  inverse[1].y = RetailScaleDouble(
      RetailSubtract(RetailMultiply(matrix[0].x, matrix[2].z),
                     RetailMultiply(matrix[0].z, matrix[2].x)),
      inverseDeterminant);
  inverse[1].z = RetailScaleDouble(
      RetailSubtract(RetailMultiply(matrix[0].z, matrix[1].x),
                     RetailMultiply(matrix[0].x, matrix[1].z)),
      inverseDeterminant);
  inverse[2].x = RetailScaleDouble(cofactor02, inverseDeterminant);
  inverse[2].y = RetailScaleDouble(
      RetailSubtract(RetailMultiply(matrix[0].y, matrix[2].x),
                     RetailMultiply(matrix[0].x, matrix[2].y)),
      inverseDeterminant);
  inverse[2].z = RetailScaleDouble(
      RetailSubtract(RetailMultiply(matrix[0].x, matrix[1].y),
                     RetailMultiply(matrix[0].y, matrix[1].x)),
      inverseDeterminant);
  matrix = inverse;
  return true;
}

idVec3 RotateVector(const idMat3& matrix, const idVec3& vector) {
  return {
      ((vector.y * matrix[1].x) + (vector.x * matrix[0].x)) +
          (vector.z * matrix[2].x),
      ((vector.x * matrix[0].y) + (vector.y * matrix[1].y)) +
          (vector.z * matrix[2].y),
      ((vector.x * matrix[0].z) + (vector.y * matrix[1].z)) +
          (vector.z * matrix[2].z),
  };
}

idVec3 RotateVectorYFirst(const idMat3& matrix, const idVec3& vector) {
  return {
      RetailAdd(RetailAdd(RetailMultiply(vector.y, matrix[1].x),
                          RetailMultiply(vector.x, matrix[0].x)),
                RetailMultiply(vector.z, matrix[2].x)),
      RetailAdd(RetailAdd(RetailMultiply(vector.y, matrix[1].y),
                          RetailMultiply(vector.x, matrix[0].y)),
                RetailMultiply(vector.z, matrix[2].y)),
      RetailAdd(RetailAdd(RetailMultiply(vector.y, matrix[1].z),
                          RetailMultiply(vector.x, matrix[0].z)),
                RetailMultiply(vector.z, matrix[2].z)),
  };
}

idVec3 SetLocalOriginWorld(const idVec3& masterOrigin,
                           const idMat3& masterAxis,
                           const idVec3& localOrigin) {
  const idVec3 rotated{
      RetailAdd(RetailAdd(RetailMultiply(localOrigin.y, masterAxis[1].x),
                          RetailMultiply(localOrigin.x, masterAxis[0].x)),
                RetailMultiply(localOrigin.z, masterAxis[2].x)),
      RetailAdd(RetailAdd(RetailMultiply(localOrigin.x, masterAxis[0].y),
                          RetailMultiply(localOrigin.y, masterAxis[1].y)),
                RetailMultiply(localOrigin.z, masterAxis[2].y)),
      RetailAdd(RetailAdd(RetailMultiply(localOrigin.x, masterAxis[0].z),
                          RetailMultiply(localOrigin.y, masterAxis[1].z)),
                RetailMultiply(localOrigin.z, masterAxis[2].z)),
  };
  return {RetailAdd(rotated.x, masterOrigin.x),
          RetailAdd(masterOrigin.y, rotated.y),
          RetailAdd(masterOrigin.z, rotated.z)};
}

idVec3 Cross(const idVec3& left, const idVec3& right) {
  return {(right.z * left.y) - (right.y * left.z),
          (right.x * left.z) - (right.z * left.x),
          (right.y * left.x) - (right.x * left.y)};
}

float Dot(const idVec3& left, const idVec3& right) {
  return ((left.y * right.y) + (left.x * right.x)) +
         (left.z * right.z);
}

idVec3 Add(const idVec3& left, const idVec3& right) {
  return {left.x + right.x, left.y + right.y, left.z + right.z};
}

idVec3 Subtract(const idVec3& left, const idVec3& right) {
  return {left.x - right.x, left.y - right.y, left.z - right.z};
}

idVec3 Scale(const idVec3& value, float scale) {
  return {value.x * scale, value.y * scale, value.z * scale};
}

idVec3 LinearVelocity(const rigidBodyPState_t& state) {
  return {state.spatialVelocity.p[0], state.spatialVelocity.p[1],
          state.spatialVelocity.p[2]};
}

idVec3 AngularVelocity(const rigidBodyPState_t& state) {
  return {state.spatialVelocity.p[3], state.spatialVelocity.p[4],
          state.spatialVelocity.p[5]};
}

idVec3 Force(const rigidBodyPState_t& state) {
  return {state.externalForce.p[0], state.externalForce.p[1],
          state.externalForce.p[2]};
}

idVec3 Torque(const rigidBodyPState_t& state) {
  return {state.externalForce.p[3], state.externalForce.p[4],
          state.externalForce.p[5]};
}

void StoreLinearVelocity(rigidBodyPState_t& state, const idVec3& value) {
  state.spatialVelocity.p[0] = value.x;
  state.spatialVelocity.p[1] = value.y;
  state.spatialVelocity.p[2] = value.z;
}

void StoreAngularVelocity(rigidBodyPState_t& state, const idVec3& value) {
  state.spatialVelocity.p[3] = value.x;
  state.spatialVelocity.p[4] = value.y;
  state.spatialVelocity.p[5] = value.z;
}

void ClearVec6(idVec6& value) {
  std::memset(value.p, 0, sizeof(value.p));
}

void CancelQuery(idPhysics_RigidBody& body) {
  const std::uint64_t index = body.motionQuery.index;
  if (index >= body.clip->collisionQueryFirstSubmittedIndex &&
      index <= body.clip->collisionQueryLastAllocedIndex) {
    body.clip->collisionQueries[index & 0xfffu].type = kQueryCanceled;
  }
  body.motionQuery.index = 0;
}

idVec3 CenterOfMassWorld(const idPhysics_RigidBody& body) {
  return Add(body.clipModel->origin,
             RotateVector(body.clipModel->axis, body.centerOfMass));
}

idMat3 InverseWorldInertia(const idPhysics_RigidBody& body) {
  return RetailMatrixMultiply(
      RetailMatrixMultiply(Transpose(body.clipModel->axis),
                           body.inverseInertiaTensor),
      body.clipModel->axis);
}

float ClampUnit(float value) {
  if (0.0f > value) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

} // namespace

rigidBodyPState_t& rigidBodyPState_t::operator=(
    const rigidBodyPState_t& other) {
  std::memcpy(this, &other, sizeof(*this));
  return *this;
}

idPhysics_RigidBody::idPhysics_RigidBody()
    : clipModel(nullptr), linearFriction(0.0f), angularFriction(0.0f),
      contactFriction(0.0f), linearFrictionWater(0.0f),
      angularFrictionWater(0.0f), bouncyness(0.0f), motionQuery(),
      lastTimeStep(0.0f) {
  type = PHYSICS_RIGIDBODY;
  SetClipMask(1, -1);
  bouncyness = 0.6f;
  linearFriction = 0.6f;
  angularFriction = 0.6f;
  contactFriction = 0.0f;
  linearFrictionWater = 1.0f;
  angularFrictionWater = 1.0f;
  inertiaScale = {1.0f, 1.0f, 1.0f};
  waterLevel = 0.0f;
  current.atRest = -1;
  current.localOrigin = vec3_origin;
  current.localAxis = mat3_identity;
  current.worldOrigin = vec3_origin;
  current.worldAxis = mat3_identity;
  ClearVec6(current.spatialVelocity);
  ClearVec6(current.externalForce);
  ClearVec6(current.pushVelocity);
  saved = current;
  mass = 1.0f;
  inverseMass = 1.0f;
  centerOfMass = vec3_origin;
  inertiaTensor = mat3_identity;
  inverseInertiaTensor = mat3_identity;
  fl.noPushMask = 0;
  reinterpret_cast<std::uint8_t*>(&fl)[4] &= 0xe0u;
}

idPhysics_RigidBody::~idPhysics_RigidBody() {
  if (clipModel != nullptr) {
    clipModel->Delete();
    clipModel = nullptr;
  }
}

void idPhysics_RigidBody::SetFriction(float linear, float angular,
                                      float contact) {
  if (!(linear < 0.0f) && !(linear > 1.0f) && !(angular < 0.0f) &&
      !(angular > 1.0f) && !(contact < 0.0f) && !(contact > 1.0f)) {
    linearFriction = linear;
    angularFriction = angular;
    contactFriction = contact;
  }
}

void idPhysics_RigidBody::SetWaterFriction(float linear, float angular) {
  linearFrictionWater = ClampUnit(linear);
  angularFrictionWater = ClampUnit(angular);
}

void idPhysics_RigidBody::SetBouncyness(float value) {
  if (!(value < 0.0f) && !(value > 1.0f)) {
    bouncyness = value;
  }
}

void idPhysics_RigidBody::SetInertiaScale(const idVec3& scale) {
  inertiaScale = scale;
}

void idPhysics_RigidBody::SetClipModel(idClipModel* model, float density,
                                       int id, bool freeOld) {
  if (clipModel != nullptr && clipModel != model && freeOld) {
    clipModel->Delete();
  }
  clipModel = model;
  model->Link(entityNumber, physicsId, id, model->origin, model->axis);
  model->GetMassProperties(density, mass, centerOfMass, inertiaTensor);
  const std::uint32_t massBits =
      *reinterpret_cast<const std::uint32_t*>(&mass);
  if (mass <= 0.0f || std::isnan(mass) ||
      ((massBits & 0x7f800000u) == 0 && (massBits & 0x007fffffu) != 0)) {
    mass = 1.0f;
    centerOfMass = vec3_origin;
    inertiaTensor = mat3_identity;
  }

  int minimum = 0;
  if (!(inertiaTensor[1].y > inertiaTensor[0].x)) {
    minimum = inertiaTensor[2].z > inertiaTensor[1].y ? 1 : 2;
  } else if (!(inertiaTensor[2].z > inertiaTensor[0].x)) {
    minimum = 2;
  }
  idMat3 scale{};
  const float minimumInertia = (&inertiaTensor[0].x)[minimum * 4];
  scale[0].x = (inertiaTensor[0].x / minimumInertia) * inertiaScale.x;
  scale[1].y = (inertiaTensor[1].y / minimumInertia) * inertiaScale.y;
  scale[2].z = (inertiaTensor[2].z / minimumInertia) * inertiaScale.z;
  if (scale[0].x > 10.0f || scale[1].y > 10.0f ||
      scale[2].z > 10.0f) {
    const float maximum = minimumInertia * 10.0f;
    const int first = (minimum + 1) % 3;
    const int second = (minimum + 2) % 3;
    (&scale[0].x)[first * 4] =
        maximum / (&inertiaTensor[0].x)[first * 4];
    (&scale[0].x)[second * 4] =
        maximum / (&inertiaTensor[0].x)[second * 4];
    inertiaTensor = RetailMatrixMultiplyAssign(inertiaTensor, scale);
  }
  inverseMass = 1.0f / mass;
  idMat3 inverse = inertiaTensor;
  InverseSelfRetail(inverse);
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      inverseInertiaTensor[row][column] =
          inverse[row][column] * 0.16666667f;
    }
  }
  ClearVec6(current.spatialVelocity);
}

void idPhysics_RigidBody::SetMass(float value, int) {
  const float scale = value / mass;
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      inertiaTensor[row][column] =
          RetailMultiply(scale, inertiaTensor[row][column]);
    }
  }
  idMat3 inverse = inertiaTensor;
  InverseSelfRetail(inverse);
  for (int row = 0; row < 3; ++row) {
    for (int column = 0; column < 3; ++column) {
      inverseInertiaTensor[row][column] =
          inverse[row][column] * 0.16666667f;
    }
  }
  inverseMass = 1.0f / value;
  mass = value;
}

float idPhysics_RigidBody::GetMass(int) const { return mass; }

void idPhysics_RigidBody::SetContents(int contents, int) {
  if (clipModel != nullptr) {
    clipModel->SetContents(contents);
  }
}

int idPhysics_RigidBody::GetContents(int) const {
  return clipModel != nullptr ? clipModel->contents : 0;
}

const idBounds& idPhysics_RigidBody::GetBounds(int) const {
  return clipModel != nullptr ? clipModel->bounds : bounds_zero;
}

const idBounds& idPhysics_RigidBody::GetAbsBounds(int) const {
  return clipModel != nullptr ? clipModel->absBounds : bounds_zero;
}

void idPhysics_RigidBody::Activate() { current.atRest = -1; }

void idPhysics_RigidBody::PutToRest() {
  current.atRest = 1;
  ClearVec6(current.spatialVelocity);
}

bool idPhysics_RigidBody::IsAtRest() const { return current.atRest >= 0; }

bool idPhysics_RigidBody::IsPushable(int sourceContentType) const {
  return (sourceContentType & fl.noPushMask) == 0 && !fl.noImpact &&
         !fl.hasMaster;
}

void idPhysics_RigidBody::UnlinkClip() {
  if (clipModel != nullptr) {
    clipModel->Unlink();
  }
}

void idPhysics_RigidBody::LinkClip() {
  if (clipModel != nullptr) {
    clipModel->Link();
  }
}

void idPhysics_RigidBody::DisableClip() {
  if (clipModel != nullptr) {
    clipModel->flags &= ~1u;
  }
}

void idPhysics_RigidBody::CapVelocity() {
  idVec3 linear = LinearVelocity(current);
  const float linearSquared = ((linear.y * linear.y) +
                               (linear.x * linear.x)) +
                              (linear.z * linear.z);
  if (linearSquared > 25000000.0f) {
    const float scale = InverseLength(linear) * 5000.0f;
    StoreLinearVelocity(current, Scale(linear, scale));
  }
  idVec3 angular = AngularVelocity(current);
  const float angularSquared = ((angular.y * angular.y) +
                                (angular.x * angular.x)) +
                               (angular.z * angular.z);
  if (angularSquared > MAX_ANGULAR_VELOCITY * MAX_ANGULAR_VELOCITY) {
    const float scale = MAX_ANGULAR_VELOCITY * InverseLength(angular);
    StoreAngularVelocity(current, Scale(angular, scale));
  }
}

void idPhysics_RigidBody::DebugDraw() {
  if (clipModel == nullptr) {
    return;
  }
  if (rb_showBodies.valueInteger != 0 ||
      (rb_showActive.valueInteger != 0 && current.atRest < 0)) {
    clip->DrawSingleClipModel(clipModel, clipModel->origin, clipModel->axis,
                              false);
  }
  if (rb_showMass.valueInteger != 0) {
    va text("\n%1.2f", mass);
    GetGameLibRenderDebug()->DebugText(
        text, clipModel->origin, 0.08f, idColor::colorCyan, 1, 0, false,
        false);
  }
  if (rb_showInertia.valueInteger != 0) {
    va text("\n\n\n( %.1f %.1f %.1f )\n( %.1f %.1f %.1f )\n( %.1f %.1f %.1f )",
            inertiaTensor[0].x, inertiaTensor[0].y, inertiaTensor[0].z,
            inertiaTensor[1].x, inertiaTensor[1].y, inertiaTensor[1].z,
            inertiaTensor[2].x, inertiaTensor[2].y, inertiaTensor[2].z);
    GetGameLibRenderDebug()->DebugText(
        text, clipModel->origin, 0.05f, idColor::colorCyan, 1, 0, false,
        false);
  }
  if (rb_showVelocity.valueInteger != 0) {
    DrawVelocity(clipModel->bodyId, 0.1f, 4.0f);
  }
}

void idPhysics_RigidBody::SaveState() {
  if (clipModel != nullptr) {
    saved = current;
    saved.worldOrigin = clipModel->origin;
    saved.worldAxis = clipModel->axis;
  }
}

void idPhysics_RigidBody::RestoreState() {
  if (clipModel != nullptr) {
    current = saved;
    clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                    current.worldOrigin, current.worldAxis);
    EvaluateContacts();
  }
}

void idPhysics_RigidBody::SetOrigin(const idVec3& newOrigin, int) {
  if (clipModel == nullptr) {
    return;
  }
  CancelQuery(*this);
  clipModel->Link(entityNumber, physicsId, clipModel->bodyId, newOrigin,
                  clipModel->axis);
  if (fl.hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localOrigin =
        RotateVector(Transpose(masterAxis), Subtract(newOrigin, masterOrigin));
  }
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_RigidBody::SetAxis(const idMat3& newAxis, int) {
  if (clipModel == nullptr) {
    return;
  }
  CancelQuery(*this);
  clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                  clipModel->origin, newAxis);
  if (fl.hasMaster && fl.isOrientated) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localAxis = RetailMatrixMultiply(newAxis, Transpose(masterAxis));
  }
  callbacks->ActivatePhysics(physicsId);
}

const idVec3& idPhysics_RigidBody::GetOrigin(int) const {
  return clipModel != nullptr ? clipModel->origin : vec3_origin;
}

const idMat3& idPhysics_RigidBody::GetAxis(int) const {
  return clipModel != nullptr ? clipModel->axis : mat3_identity;
}

void idPhysics_RigidBody::SetLinearVelocity(const idVec3& value, int) {
  StoreLinearVelocity(current, value);
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_RigidBody::SetAngularVelocity(const idVec3& value, int) {
  StoreAngularVelocity(current, value);
  callbacks->ActivatePhysics(physicsId);
}

idVec3 idPhysics_RigidBody::GetLinearVelocity(int) const {
  return LinearVelocity(current);
}

void idPhysics_RigidBody::GetImpactInfo(int, const idVec3& point,
                                        impactInfo_t& info) const {
  if (clipModel == nullptr) {
    return;
  }
  info.invMass = inverseMass;
  info.invInertiaTensor = InverseWorldInertia(*this);
  info.position = Subtract(point, CenterOfMassWorld(*this));
  info.velocity = Add(LinearVelocity(current),
                      Cross(AngularVelocity(current), info.position));
}

void idPhysics_RigidBody::ApplyImpulse(int, const idVec3& point,
                                       const idVec3& impulse) {
  if (clipModel == nullptr || fl.noImpact) {
    return;
  }
  const idMat3 inverseWorldInertia = InverseWorldInertia(*this);
  current.spatialVelocity.p[0] = RetailAdd(
      RetailMultiply(inverseMass, impulse.x), current.spatialVelocity.p[0]);
  current.spatialVelocity.p[1] = RetailAdd(
      RetailMultiply(inverseMass, impulse.y), current.spatialVelocity.p[1]);
  current.spatialVelocity.p[2] = RetailAdd(
      RetailMultiply(inverseMass, impulse.z), current.spatialVelocity.p[2]);
  const idVec3 arm = Subtract(point, CenterOfMassWorld(*this));
  const idVec3 armCrossImpulse{
      RetailSubtract(RetailMultiply(impulse.z, arm.y),
                     RetailMultiply(impulse.y, arm.z)),
      RetailSubtract(RetailMultiply(impulse.x, arm.z),
                     RetailMultiply(impulse.z, arm.x)),
      RetailSubtract(RetailMultiply(impulse.y, arm.x),
                     RetailMultiply(impulse.x, arm.y)),
  };
  current.spatialVelocity.p[3] = RetailAdd(
      RetailAdd(
          RetailAdd(RetailMultiply(inverseWorldInertia[1].x,
                                   armCrossImpulse.y),
                    RetailMultiply(armCrossImpulse.x,
                                   inverseWorldInertia[0].x)),
          RetailMultiply(inverseWorldInertia[2].x, armCrossImpulse.z)),
      current.spatialVelocity.p[3]);
  current.spatialVelocity.p[4] = RetailAdd(
      RetailAdd(
          RetailAdd(RetailMultiply(inverseWorldInertia[1].y,
                                   armCrossImpulse.y),
                    RetailMultiply(inverseWorldInertia[0].y,
                                   armCrossImpulse.x)),
          RetailMultiply(inverseWorldInertia[2].y, armCrossImpulse.z)),
      current.spatialVelocity.p[4]);
  current.spatialVelocity.p[5] = RetailAdd(
      RetailAdd(
          RetailAdd(RetailMultiply(inverseWorldInertia[1].z,
                                   armCrossImpulse.y),
                    RetailMultiply(inverseWorldInertia[0].z,
                                   armCrossImpulse.x)),
          RetailMultiply(inverseWorldInertia[2].z, armCrossImpulse.z)),
      current.spatialVelocity.p[5]);
  CapVelocity();
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_RigidBody::ApplyForce(int, const idVec3& point,
                                     const idVec3& force) {
  if (clipModel == nullptr || fl.noImpact) {
    return;
  }
  current.externalForce.p[0] += force.x;
  current.externalForce.p[1] =
      RetailAdd(force.y, current.externalForce.p[1]);
  current.externalForce.p[2] += force.z;
  const idVec3 arm = Subtract(point, CenterOfMassWorld(*this));
  const idVec3 torque{
      RetailSubtract(RetailMultiply(force.z, arm.y),
                     RetailMultiply(force.y, arm.z)),
      RetailSubtract(RetailMultiply(force.x, arm.z),
                     RetailMultiply(force.z, arm.x)),
      RetailSubtract(RetailMultiply(force.y, arm.x),
                     RetailMultiply(force.x, arm.y)),
  };
  current.externalForce.p[3] =
      RetailAdd(torque.x, current.externalForce.p[3]);
  current.externalForce.p[4] =
      RetailAdd(torque.y, current.externalForce.p[4]);
  current.externalForce.p[5] =
      RetailAdd(torque.z, current.externalForce.p[5]);
}

void idPhysics_RigidBody::ClipTranslation(
    trace_t& results, const idVec3& translation,
    const idClipModel* model) const {
  if (clipModel == nullptr) {
    return;
  }
  const idVec3 end = Add(clipModel->origin, translation);
  if (model != nullptr) {
    clip->TranslationModel(results, clipModel->origin, end, clipModel,
                           clipModel->axis, clipMask, model, model->origin,
                           model->axis, nullptr);
  } else {
    clip->Translation(&results, clipModel->origin, end, clipModel,
                      clipModel->axis, clipMask, entityNumber, false, nullptr);
  }
}

void idPhysics_RigidBody::ClipRotation(
    trace_t& results, const idRotation& rotation,
    const idClipModel* model) const {
  if (clipModel == nullptr) {
    return;
  }
  if (model != nullptr) {
    clip->RotationModel(results, clipModel->origin, rotation, clipModel,
                        clipModel->axis, clipMask, model, model->origin,
                        model->axis, nullptr);
  } else {
    clip->Rotation(&results, clipModel->origin, rotation, clipModel,
                   clipModel->axis, clipMask, entityNumber, false, nullptr);
  }
}

int idPhysics_RigidBody::ClipContents(const idClipModel* model,
                                      int clipMask_) const {
  if (clipModel == nullptr) {
    return 0;
  }
  trace_t contents;
  const int mask = clipMask_ != 0 ? clipMask_ : clipMask;
  if (model != nullptr) {
    clip->ContentsModel(contents, clipModel->origin, clipModel,
                        clipModel->axis, mask, model, model->origin,
                        model->axis, nullptr);
  } else {
    clip->Contents(&contents, clipModel->origin, clipModel, clipModel->axis,
                   mask, 0x1fff, nullptr);
  }
  return contents.c.contentFlags;
}

bool idPhysics_RigidBody::IsGroundClipModel(int entityNum, int id) const {
  if (clipModel == nullptr || current.atRest < 0) {
    return false;
  }
  for (int index = 0; index < contacts.num; ++index) {
    const contactInfo_t& contact = contacts.list[index];
    if (contact.entityNum == entityNum && contact.bodyId == id &&
        Dot(contact.normal, gravityNormal) < -0.98480773f) {
      return true;
    }
  }
  return false;
}

void idPhysics_RigidBody::SetPushed(int deltaTime) {
  if (clipModel == nullptr) {
    return;
  }
  const float inverseTime = 1.0f / (static_cast<float>(deltaTime) * 0.001f);
  const idVec3 linear = Subtract(clipModel->origin, saved.worldOrigin);
  current.pushVelocity.p[0] = linear.x * inverseTime;
  current.pushVelocity.p[1] = linear.y * inverseTime;
  current.pushVelocity.p[2] = linear.z * inverseTime;
  const idVec3 angular = MatrixToAngularVelocityRetail(
      RetailMatrixMultiply(clipModel->axis, Transpose(saved.worldAxis)));
  current.pushVelocity.p[3] = inverseTime * angular.x;
  current.pushVelocity.p[4] = inverseTime * angular.y;
  current.pushVelocity.p[5] = inverseTime * angular.z;
}

idVec3 idPhysics_RigidBody::GetPushedLinearVelocity(int) const {
  return {current.pushVelocity.p[0], current.pushVelocity.p[1],
          current.pushVelocity.p[2]};
}

idVec3 idPhysics_RigidBody::GetPushedAngularVelocity(int) const {
  return {current.pushVelocity.p[3], current.pushVelocity.p[4],
          current.pushVelocity.p[5]};
}

void idPhysics_RigidBody::SetMaster(bool enable,
                                    const idVec3& masterOrigin,
                                    const idMat3& masterAxis,
                                    bindFlags_t flags_) {
  if (clipModel == nullptr) {
    return;
  }
  CancelQuery(*this);
  if (enable) {
    if (!fl.hasMaster) {
      current.localOrigin = RotateVectorYFirst(
          Transpose(masterAxis), Subtract(clipModel->origin, masterOrigin));
      const bool oriented = (static_cast<int>(flags_) & 1) != 0;
      current.localAxis =
          oriented
              ? RetailMatrixMultiply(clipModel->axis, Transpose(masterAxis))
              : clipModel->axis;
      fl.isOrientated = oriented;
      fl.hasMaster = true;
      ClearContacts();
    }
  } else if (fl.hasMaster) {
    fl.hasMaster = false;
    callbacks->ActivatePhysics(physicsId);
  }
}

void idPhysics_RigidBody::SetLocalOrigin(const idVec3& newOrigin, int) {
  if (clipModel == nullptr) {
    return;
  }
  CancelQuery(*this);
  if (fl.hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    const idVec3 world =
        SetLocalOriginWorld(masterOrigin, masterAxis, newOrigin);
    clipModel->Link(entityNumber, physicsId, clipModel->bodyId, world,
                    clipModel->axis);
    current.localOrigin = newOrigin;
  } else {
    clipModel->Link(entityNumber, physicsId, clipModel->bodyId, newOrigin,
                    clipModel->axis);
  }
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_RigidBody::SetLocalAxis(const idMat3& newAxis, int) {
  if (clipModel == nullptr) {
    return;
  }
  CancelQuery(*this);
  if (fl.hasMaster && fl.isOrientated) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    const idMat3 worldAxis = RetailMatrixMultiply(newAxis, masterAxis);
    clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                    clipModel->origin, worldAxis);
    current.localAxis = newAxis;
  } else {
    clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                    clipModel->origin, newAxis);
  }
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_RigidBody::Evolve(float timeStep, trace_t* collisionOverride,
                                 contactsResult_t* contactOverride) {
  const idVec3 angular{
      current.spatialVelocity.p[3] - current.pushVelocity.p[3],
      current.spatialVelocity.p[4] - current.pushVelocity.p[4],
      current.spatialVelocity.p[5] - current.pushVelocity.p[5],
  };
  const float angularSquared =
      ((angular.y * angular.y) + (angular.x * angular.x)) +
      (angular.z * angular.z);
  const float inverseAngular = InverseLength(angular);
  idVec3 rotationAxis{angular.x * inverseAngular,
                      angular.y * inverseAngular,
                      angular.z * inverseAngular};
  float angle = -((inverseAngular * angularSquared) * 57.295776f) * timeStep;
  if (angle >= 360.0f || angle < 0.0f) {
    angle -= std::floor(angle * 0.0027777778f) * 360.0f;
  }
  if (angle > 180.0f) {
    angle -= 360.0f;
  }
  if (angle == 0.0f) {
    rotationAxis = vec3_up;
  }

  idRotation rotation;
  rotation.origin = clipModel->origin;
  rotation.vec = rotationAxis;
  rotation.angle = angle;
  rotation.axis = RotationMatrixRetail(rotationAxis, angle);
  rotation.axisValid = true;
  const idMat3 nextAxis =
      RetailMatrixMultiply(clipModel->axis, rotation.axis);
  const idVec3 oldCenter = RotateVector(clipModel->axis, centerOfMass);
  const idVec3 nextCenter = RotateVector(nextAxis, centerOfMass);
  const idVec3 relativeLinear{
      current.spatialVelocity.p[0] - current.pushVelocity.p[0],
      current.spatialVelocity.p[1] - current.pushVelocity.p[1],
      current.spatialVelocity.p[2] - current.pushVelocity.p[2],
  };
  const idVec3 end{
      clipModel->origin.x +
          (((oldCenter.x + (timeStep * relativeLinear.x)) - nextCenter.x)),
      clipModel->origin.y +
          (((oldCenter.y + (relativeLinear.y * timeStep)) - nextCenter.y)),
      clipModel->origin.z +
          (((oldCenter.z + (relativeLinear.z * timeStep)) - nextCenter.z)),
  };

  constexpr float contactGatheringDepth = 0.375f;
  constexpr const char* immediateName =
      "w:\\build\\tech5\\engine\\gamelib\\physics\\Physics_RigidBody.cpp(63) : MotionContacts";
  constexpr const char* deferredName =
      "w:\\build\\tech5\\engine\\gamelib\\physics\\Physics_RigidBody.cpp(68) : MotionContacts";
  if (collisionOverride != nullptr && contactOverride != nullptr) {
    clip->MotionContacts(collisionOverride, contactOverride, clipModel->origin,
                         end, rotation, contactGatheringDepth, clipModel,
                         clipModel->axis, clipMask, entityNumber, true,
                         immediateName);
  } else {
    motionQuery = clip->MotionContacts(
        nullptr, nullptr, clipModel->origin, end, rotation,
        contactGatheringDepth, clipModel, clipModel->axis, clipMask,
        entityNumber, true, deferredName);
  }
  lastTimeStep = timeStep;
}

bool idPhysics_RigidBody::TestIfAtRest() {
  if (current.atRest >= 0) {
    return true;
  }

  int supportingContacts = 0;
  idVec3 averageNormal = vec3_origin;
  for (int index = 0; index < contacts.num; ++index) {
    const contactInfo_t& contact = contacts.list[index];
    if (Dot(contact.normal, gravityNormal) < 0.0f) {
      averageNormal.x += contact.normal.x;
      averageNormal.y += contact.normal.y;
      averageNormal.z += contact.normal.z;
      ++supportingContacts;
    }
  }
  if (supportingContacts < 3) {
    return false;
  }

  const float inverseCount = 1.0f / static_cast<float>(contacts.num);
  averageNormal.x *= inverseCount;
  averageNormal.y *= inverseCount;
  averageNormal.z *= inverseCount;
  const float inverseNormal = InverseLength(averageNormal);
  averageNormal.x *= inverseNormal;
  averageNormal.y *= inverseNormal;
  averageNormal.z *= inverseNormal;
  if (Dot(gravityNormal, averageNormal) > -0.7f) {
    return false;
  }

  idFixedWinding contactWinding;
  for (int index = 0; index < contacts.num; ++index) {
    const contactInfo_t& contact = contacts.list[index];
    if (Dot(contact.normal, gravityNormal) < 0.0f) {
      const float gravityDistance = Dot(gravityNormal, contact.point);
      const idVec3 projected{
          contact.point.x - gravityNormal.x * gravityDistance,
          contact.point.y - gravityNormal.y * gravityDistance,
          contact.point.z - gravityNormal.z * gravityDistance,
      };
      contactWinding.AddToConvexHull(projected, gravityNormal, 0.1f);
    }
  }
  if (contactWinding.GetNumPoints() < 3) {
    return false;
  }

  idVec3 point = CenterOfMassWorld(*this);
  const float gravityDistance = Dot(gravityNormal, point);
  point.x -= gravityNormal.x * gravityDistance;
  point.y -= gravityNormal.y * gravityDistance;
  point.z -= gravityNormal.z * gravityDistance;
  const idVec3 windingNormal{-gravityNormal.x, -gravityNormal.y,
                             -gravityNormal.z};
  if (!contactWinding.PointInside(windingNormal, point, 0.0f)) {
    return false;
  }

  idVec3 linear = LinearVelocity(current);
  const float vertical = Dot(gravityNormal, linear);
  linear.x -= gravityNormal.x * vertical;
  linear.y -= gravityNormal.y * vertical;
  linear.z -= gravityNormal.z * vertical;
  if (Length(linear) > 20.0f || vertical > 40.0f || vertical < -40.0f) {
    return false;
  }

  const idVec3 angular = AngularVelocity(current);
  return ((angular.y * angular.y) + (angular.x * angular.x)) +
             (angular.z * angular.z) <=
         20.0f;
}

void idPhysics_RigidBody::DropToFloorAndRest() {
  if (IsInNonResidentCollisionArea(true)) {
    return;
  }
  if (fl.testSolid) {
    fl.testSolid = false;
    trace_t contents;
    clip->Contents(
        &contents, clipModel->origin, clipModel, clipModel->axis, clipMask,
        entityNumber,
        "w:\\build\\tech5\\engine\\gamelib\\physics\\Physics_RigidBody.cpp(406) : DropToFloorAndRest");
    if (contents.c.contentFlags != 0) {
      idVec3 raised = clipModel->origin;
      raised.z += 0.5f;
      SetOrigin(raised, -1);
      clip->Contents(
          &contents, clipModel->origin, clipModel, clipModel->axis, clipMask,
          entityNumber,
          "w:\\build\\tech5\\engine\\gamelib\\physics\\Physics_RigidBody.cpp(409) : DropToFloorAndRest");
      if (contents.c.contentFlags != 0) {
        callbacks->DeactivatePhysics(physicsId);
        fl.dropToFloor = false;
        return;
      }
    }
  }

  const idVec3 down{
      clipModel->origin.x + gravityNormal.x * 128.0f,
      clipModel->origin.y + gravityNormal.y * 128.0f,
      clipModel->origin.z + gravityNormal.z * 128.0f,
  };
  trace_t trace;
  clip->Translation(
      &trace, clipModel->origin, down, clipModel, clipModel->axis, clipMask,
      entityNumber, false,
      "w:\\build\\tech5\\engine\\gamelib\\physics\\Physics_RigidBody.cpp(422) : DropToFloorAndRest");
  clipModel->Link(entityNumber, physicsId, clipModel->bodyId, trace.endpos,
                  clipModel->axis);
  bool shouldRest;
  if (trace.fraction == 0.0f) {
    EvaluateContacts();
    shouldRest = TestIfAtRest();
  } else {
    shouldRest = IsOutsideWorld();
  }
  if (shouldRest) {
    callbacks->DeactivatePhysics(physicsId);
  }
  fl.dropToFloor = false;
  ClearVec6(current.externalForce);
}

void idPhysics_RigidBody::ResolveCollisions(
    trace_t* collisionOverride, contactsResult_t* contactOverride) {
  if (collisionOverride == nullptr && contactOverride == nullptr) {
    const std::uint64_t index = motionQuery.index;
    if (index < clip->collisionQueryFirstSubmittedIndex ||
        index > clip->collisionQueryLastSubmittedIndex) {
      return;
    }
    const std::uint64_t offset =
        clip->collisionQueries[index & 0xfffu].query.offset &
        kCollisionResultOffsetMask;
    if (offset < idCollisionQuery::finalResultsFirstSubmittedOffset ||
        offset >= idCollisionQuery::finalResultsLastSubmittedOffset) {
      return;
    }
  }

  ClearVec6(current.pushVelocity);
  trace_t localCollision;
  contactsResult_t localContacts;
  trace_t* collision = &localCollision;
  contactsResult_t* contactResults = &localContacts;
  if (collisionOverride != nullptr && contactOverride != nullptr) {
    collision = collisionOverride;
    contactResults = contactOverride;
    clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                    collision->endpos, collision->endAxis);
  } else {
    const std::uint64_t savedIndex = motionQuery.index;
    motionQuery.index = 0;
    collisionModelManager->GetMotionContactsResult(
        &localCollision, &localContacts,
        clip->collisionQueries[savedIndex & 0xfffu].query, false);
  }

  UpdateCollisionResidency(collision->c);
  ClearContacts();
  for (int index = 0; index < contactResults->numContacts; ++index) {
    contacts.Append(contactResults->contacts[index]);
  }
  AddContactPhysicsForContacts();

  const idMat3 inverseWorldInertia = InverseWorldInertia(*this);
  const idVec3 massCenter = CenterOfMassWorld(*this);
  idVec3 collisionImpulse = vec3_origin;
  idPhysics* otherPhysics = nullptr;

  if (collision->fraction >= 1.0f) {
    const float water = waterLevel;
    const float linearDamping = ClampUnit(
        (1.0f - water) * linearFriction +
        GetWaterViscosity(0) * (linearFrictionWater * water));
    const float angularDamping = ClampUnit(
        (1.0f - water) * angularFriction +
        GetWaterViscosity(0) * (water * angularFrictionWater));
    const idVec3 oldLinear = LinearVelocity(current);
    const idVec3 external = Force(current);
    const idVec3 nextLinear{
        lastTimeStep *
                (((inverseMass * external.x) -
                  (linearDamping * oldLinear.x)) +
                 gravityVector.x) +
            oldLinear.x,
        lastTimeStep *
                (((inverseMass * external.y) -
                  (linearDamping * oldLinear.y)) +
                 gravityVector.y) +
            oldLinear.y,
        lastTimeStep *
                (((inverseMass * external.z) -
                  (linearDamping * oldLinear.z)) +
                 gravityVector.z) +
            oldLinear.z,
    };
    StoreLinearVelocity(current, nextLinear);
    const idVec3 oldAngular = AngularVelocity(current);
    const idVec3 angularAcceleration =
        RotateVector(inverseWorldInertia, Torque(current));
    StoreAngularVelocity(
        current,
        {((angularAcceleration.x - angularDamping * oldAngular.x) *
              lastTimeStep) +
             oldAngular.x,
         ((angularAcceleration.y - angularDamping * oldAngular.y) *
              lastTimeStep) +
             oldAngular.y,
         ((angularAcceleration.z - angularDamping * oldAngular.z) *
              lastTimeStep) +
             oldAngular.z});
  } else {
    impactInfo_t info{};
    otherPhysics = idPhysics::GetPhysicsForId(collision->c.physicsId);
    if (otherPhysics != nullptr && otherPhysics != this) {
      otherPhysics->GetImpactInfo(collision->c.bodyId, collision->c.point,
                                  info);
    } else {
      otherPhysics = nullptr;
    }
    const idVec3 arm = Subtract(collision->c.point, massCenter);
    idVec3 velocity = Add(LinearVelocity(current),
                          Cross(AngularVelocity(current), arm));
    velocity = Subtract(velocity, info.velocity);
    const int collisionAction =
        callbacks->Collide(physicsId, *collision, velocity);
    otherPhysics = idPhysics::GetPhysicsForId(collision->c.physicsId);
    if (collisionAction == kCollideStopSimulation) {
      PutToRest();
      callbacks->DeactivatePhysics(physicsId);
      return;
    }
    if (collisionAction != kCollideIgnoreCollision) {
      const float normalVelocity = Dot(velocity, collision->c.normal);
      const float numerator =
          normalVelocity <= -20.0f
              ? -(bouncyness + 1.0f) * normalVelocity
              : 20.0f;
      const idVec3 armCrossNormal = Cross(arm, collision->c.normal);
      float denominator =
          inverseMass +
          Dot(Cross(RotateVector(inverseWorldInertia, armCrossNormal), arm),
              collision->c.normal);
      if (info.invMass != 0.0f) {
        const idVec3 otherCrossNormal =
            Cross(info.position, collision->c.normal);
        denominator +=
            info.invMass +
            Dot(Cross(RotateVector(info.invInertiaTensor, otherCrossNormal),
                      info.position),
                collision->c.normal);
      }
      collisionImpulse = Scale(collision->c.normal, numerator / denominator);
      StoreLinearVelocity(
          current,
          Add(LinearVelocity(current), Scale(collisionImpulse, inverseMass)));
      StoreAngularVelocity(
          current,
          Add(AngularVelocity(current),
              RotateVector(inverseWorldInertia,
                           Cross(arm, collisionImpulse))));
      if (collision->fraction < 0.0001f) {
        const idVec3 linear = LinearVelocity(current);
        StoreLinearVelocity(
            current,
            {info.velocity.x + (linear.x - info.velocity.x) * 0.5f,
             info.velocity.y + (linear.y - info.velocity.y) * 0.5f,
             info.velocity.z + (linear.z - info.velocity.z) * 0.5f});
        StoreAngularVelocity(current, Scale(AngularVelocity(current), 0.5f));
      }
    } else {
      otherPhysics = nullptr;
    }
  }

  if (TestIfAtRest()) {
    callbacks->DeactivatePhysics(physicsId);
    return;
  }
  ActivateContactPhysics();
  if (otherPhysics != nullptr && !otherPhysics->IsAtRest()) {
    if (otherPhysics->callbacks == nullptr) {
      return;
    }
    otherPhysics->callbacks->ApplyImpulse(
        physicsId, collision->c.bodyId, collision->c.point,
        Scale(collisionImpulse, -1.0f));
  }

  for (int index = 0; index < contacts.num; ++index) {
    contactInfo_t& contact = contacts.list[index];
    if (callbacks->Contact(physicsId, contact) == kCollideIgnoreCollision) {
      continue;
    }
    const idVec3 arm = Subtract(contact.point, massCenter);
    idVec3 velocity = Add(LinearVelocity(current),
                          Cross(AngularVelocity(current), arm));
    impactInfo_t info{};
    idPhysics* contactPhysics = idPhysics::GetPhysicsForId(contact.physicsId);
    if (contactPhysics != nullptr && contactPhysics != this) {
      contactPhysics->GetImpactInfo(contact.bodyId, contact.point, info);
      velocity = Subtract(velocity, info.velocity);
    }
    const idVec3 normalVelocity = Scale(contact.normal, Dot(velocity, contact.normal));
    idVec3 direction = Scale(Subtract(velocity, normalVelocity), -1.0f);
    const float directionSquared =
        ((direction.y * direction.y) + (direction.x * direction.x)) +
        direction.z * direction.z;
    const float inverseDirection = InverseLength(direction);
    direction = Scale(direction, inverseDirection);
    const idVec3 armCrossDirection = Cross(arm, direction);
    const float frictionDenominator =
        inverseMass +
        Dot(Cross(RotateVector(inverseWorldInertia, armCrossDirection), arm),
            direction);
    const float frictionMagnitude =
        (inverseDirection * directionSquared) * contactFriction /
        frictionDenominator;
    const idVec3 frictionImpulse = Scale(direction, frictionMagnitude);
    StoreLinearVelocity(
        current,
        Add(LinearVelocity(current), Scale(frictionImpulse, inverseMass)));
    StoreAngularVelocity(
        current,
        Add(AngularVelocity(current),
            RotateVector(inverseWorldInertia,
                         Cross(arm, frictionImpulse))));

    if (Dot(normalVelocity, contact.normal) < 0.0f) {
      direction = Scale(normalVelocity, -1.0f);
      const float normalSquared =
          ((direction.y * direction.y) + (direction.x * direction.x)) +
          direction.z * direction.z;
      const float inverseNormal = InverseLength(direction);
      direction = Scale(direction, inverseNormal);
      const idVec3 armCrossDirection2 = Cross(arm, direction);
      const float denominator =
          inverseMass +
          Dot(Cross(RotateVector(inverseWorldInertia, armCrossDirection2),
                    arm),
              direction);
      const idVec3 normalImpulse =
          Scale(direction, (normalSquared * inverseNormal) / denominator);
      StoreLinearVelocity(
          current,
          Add(LinearVelocity(current), Scale(normalImpulse, inverseMass)));
      StoreAngularVelocity(
          current,
          Add(AngularVelocity(current),
              RotateVector(inverseWorldInertia,
                           Cross(arm, normalImpulse))));
    }
  }
  CapVelocity();
}

bool idPhysics_RigidBody::Evaluate(int timeStepMSec, int endTimeMSec) {
  if (clipModel == nullptr) {
    return false;
  }
  const float timeStep = static_cast<float>(timeStepMSec) * 0.001f;
  if (fl.hasMaster) {
    const idVec3 oldOrigin = clipModel->origin;
    const idMat3 oldAxis = clipModel->axis;
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    const idVec3 newOrigin =
        Add(masterOrigin, RotateVector(masterAxis, current.localOrigin));
    const idMat3 newAxis =
        fl.isOrientated
            ? RetailMatrixMultiply(current.localAxis, masterAxis)
            : current.localAxis;
    clipModel->Link(entityNumber, physicsId, clipModel->bodyId, newOrigin,
                    newAxis);
    const float inverseTime = 1.0f / timeStep;
    StoreLinearVelocity(current,
                        Scale(Subtract(newOrigin, oldOrigin), inverseTime));
    StoreAngularVelocity(
        current,
        Scale(MatrixToAngularVelocityRetail(
                  RetailMatrixMultiply(newAxis, Transpose(oldAxis))),
              inverseTime));
    ClearVec6(current.externalForce);
    return newOrigin.x != oldOrigin.x || newOrigin.y != oldOrigin.y ||
           newOrigin.z != oldOrigin.z || newAxis[0].x != oldAxis[0].x ||
           newAxis[0].y != oldAxis[0].y ||
           newAxis[0].z != oldAxis[0].z ||
           newAxis[1].x != oldAxis[1].x ||
           newAxis[1].y != oldAxis[1].y ||
           newAxis[1].z != oldAxis[1].z ||
           newAxis[2].x != oldAxis[2].x ||
           newAxis[2].y != oldAxis[2].y ||
           newAxis[2].z != oldAxis[2].z;
  }
  if (current.atRest >= 0 || timeStep <= 0.0f) {
    DebugDraw();
    return false;
  }
  if (fl.dropToFloor) {
    DropToFloorAndRest();
    return true;
  }

  idGame* const game = common->Game();
  const auto gameVtable =
      *reinterpret_cast<const void* const* const*>(game);
  const bool forceNonDeferred =
      reinterpret_cast<bool (*)(idGame*)>(
          const_cast<void*>(gameVtable[79]))(game);
  ResolveCollisions(nullptr, nullptr);
  if (forceNonDeferred) {
    trace_t collision;
    contactsResult_t contactsResult;
    Evolve(timeStep, &collision, &contactsResult);
    ResolveCollisions(&collision, &contactsResult);
  } else {
    Evolve(timeStep, nullptr, nullptr);
  }
  ClearVec6(current.externalForce);
  if (IsOutsideWorld()) {
    const char* position =
        reinterpret_cast<const idAngles&>(clipModel->origin).ToString(0);
    idLib::Warning(
        "rigid body moved outside world bounds for entity '%s' at (%s)",
        callbacks->GetName(), position);
    callbacks->DeactivatePhysics(physicsId);
  }
  DebugDraw();
  static_cast<void>(endTimeMSec);
  return true;
}

void idPhysics_RigidBody::Translate(const idVec3& translation, int) {
  if (clipModel == nullptr) {
    return;
  }
  ResolveCollisions(nullptr, nullptr);
  const idVec3 newOrigin = Add(clipModel->origin, translation);
  clipModel->Link(entityNumber, physicsId, clipModel->bodyId, newOrigin,
                  clipModel->axis);
  callbacks->ActivatePhysics(physicsId);
  if (fl.hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localOrigin = RotateVector(
        Transpose(masterAxis), Subtract(clipModel->origin, masterOrigin));
  }
}

void idPhysics_RigidBody::Rotate(const idRotation& rotation, int) {
  if (clipModel == nullptr) {
    return;
  }
  ResolveCollisions(nullptr, nullptr);
  const idVec3 newOrigin = rotation * clipModel->origin;
  const idMat3 newAxis =
      RetailMatrixMultiply(clipModel->axis, rotation.ToMat3());
  clipModel->Link(entityNumber, physicsId, clipModel->bodyId, newOrigin,
                  newAxis);
  callbacks->ActivatePhysics(physicsId);
  if (fl.hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localAxis =
        RetailMatrixMultiplyAssign(current.localAxis, rotation.ToMat3());
    current.localOrigin = RotateVector(
        Transpose(masterAxis), Subtract(clipModel->origin, masterOrigin));
  }
}

bool idPhysics_RigidBody::EvaluateContacts() {
  if (clipModel == nullptr) {
    return false;
  }
  ClearContacts();
  contactsResult_t contactResults;
  clip->Contacts(
      &contactResults, clipModel->origin, vec3_origin, 0.25f, clipModel,
      clipModel->axis, clipMask, entityNumber,
      "w:\\build\\tech5\\engine\\gamelib\\physics\\Physics_RigidBody.cpp(1229) : EvaluateContacts");
  for (int index = 0; index < contactResults.numContacts; ++index) {
    contacts.Append(contactResults.contacts[index]);
  }
  AddContactPhysicsForContacts();
  return contacts.num != 0;
}
