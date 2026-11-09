#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#include "engine/gamelib/physics/physics_rigidbody3dof.h"

#include "engine/gamelib/effectphysics/effectphysicsrigidbody.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"

#undef WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#undef WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <bit>
#include <cmath>
#include <cstring>
#include <xmmintrin.h>

namespace {

float ClampUnitRetail(float value) {
  if (0.0f > value) {
    return 0.0f;
  }
  if (value > 1.0f) {
    return 1.0f;
  }
  return value;
}

float RetailLength(const idVec3& value) {
  const float squared =
      ((value.x * value.x) + (value.y * value.y)) + value.z * value.z;
  return _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
      _mm_set_ss(squared), idMath::vector_float_smallestNonDenorm)));
}

float RetailInverseLength(const idVec3& value) {
  const float squared =
      ((value.x * value.x) + (value.y * value.y)) + value.z * value.z;
  return 1.0f /
         _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
             _mm_set_ss(squared), idMath::vector_float_smallestNonDenorm)));
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
    leading = _mm_cvtss_f32(
        _mm_sqrt_ss(_mm_set_ss(polynomialInput)));
  } else {
    polynomialInput = absolute * absolute;
    leading = absolute;
  }

  float numerator =
      std::bit_cast<float>(0xbc5b3fe1u) -
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
  constexpr double kHalfPi =
      std::bit_cast<double>(0x3ff921fb54442d18ull);
  constexpr double kPiTail =
      std::bit_cast<double>(0x3c91a62633145c07ull);
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

idMat3 Transpose(const idMat3& matrix) {
  idMat3 result;
  result.mat[0] = {matrix.mat[0].x, matrix.mat[1].x, matrix.mat[2].x};
  result.mat[1] = {matrix.mat[0].y, matrix.mat[1].y, matrix.mat[2].y};
  result.mat[2] = {matrix.mat[0].z, matrix.mat[1].z, matrix.mat[2].z};
  return result;
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
  result.mat[0] = {1.0f - (zz + yy), xy - zw, yw + zx};
  result.mat[1] = {zw + xy, 1.0f - (zz + xx), zy - xw};
  result.mat[2] = {zx - yw, xw + zy, 1.0f - (yy + xx)};
  return result;
}

idMat3 MatrixMultiplyRetail(const idMat3& left, const idMat3& right) {
  idMat3 result;
  for (int row = 0; row < 3; ++row) {
    result.mat[row].x =
        ((right.mat[0].x * left.mat[row].x) +
         (left.mat[row].y * right.mat[1].x)) +
        (left.mat[row].z * right.mat[2].x);
    result.mat[row].y =
        ((left.mat[row].y * right.mat[1].y) +
         (right.mat[0].y * left.mat[row].x)) +
        (right.mat[2].y * left.mat[row].z);
    result.mat[row].z =
        ((right.mat[1].z * left.mat[row].y) +
         (right.mat[0].z * left.mat[row].x)) +
        (right.mat[2].z * left.mat[row].z);
  }
  return result;
}

idQuat MatrixToQuatRetail(const idMat3& matrix) {
  idQuat result;
  const float trace =
      (matrix.mat[0].x + matrix.mat[1].y) + matrix.mat[2].z;
  if (trace > 0.0f) {
    const float value = trace + 1.0f;
    const float inverse =
        1.0f / _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
                   _mm_set_ss(value),
                   idMath::vector_float_smallestNonDenorm)));
    const float halfInverse = inverse * 0.5f;
    result.x = (matrix.mat[2].y - matrix.mat[1].z) * halfInverse;
    result.w = halfInverse * value;
    result.z = (matrix.mat[1].x - matrix.mat[0].y) * halfInverse;
    result.y = (matrix.mat[0].z - matrix.mat[2].x) * halfInverse;
    return result;
  }

  static constexpr int next[3] = {1, 2, 0};
  int i = matrix.mat[1].y > matrix.mat[0].x ? 1 : 0;
  if (matrix.mat[2].z > matrix.mat[i][i]) {
    i = 2;
  }
  const int j = next[i];
  const int k = next[j];
  const float value =
      (matrix.mat[i][i] - (matrix.mat[k][k] + matrix.mat[j][j])) + 1.0f;
  float scale =
      (1.0f / _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
                    _mm_set_ss(value),
                    idMath::vector_float_smallestNonDenorm)))) *
      0.5f;
  const float difference = matrix.mat[k][j] - matrix.mat[j][k];
  if (difference < 0.0f) {
    scale = -scale;
  }
  result[i] = scale * value;
  result.w = difference * scale;
  result[j] = (matrix.mat[i][j] + matrix.mat[j][i]) * scale;
  result[k] = (matrix.mat[i][k] + matrix.mat[k][i]) * scale;
  return result;
}

idVec3 MatrixToAngularVelocityRetail(const idMat3& matrix) {
  const idQuat quaternion = MatrixToQuatRetail(matrix);
  const float squared =
      ((quaternion.y * quaternion.y) +
       (quaternion.x * quaternion.x)) +
      (quaternion.z * quaternion.z);
  const float inverse =
      1.0f /
      _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
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

idVec3 LocalToWorld(const idVec3& localOrigin,
                    const idVec3& masterOrigin,
                    const idMat3& masterAxis) {
  return {
      ((localOrigin.y * masterAxis.mat[1].x +
        localOrigin.x * masterAxis.mat[0].x) +
       localOrigin.z * masterAxis.mat[2].x) +
          masterOrigin.x,
      masterOrigin.y +
          ((localOrigin.y * masterAxis.mat[1].y +
            localOrigin.x * masterAxis.mat[0].y) +
           localOrigin.z * masterAxis.mat[2].y),
      masterOrigin.z +
          ((localOrigin.y * masterAxis.mat[1].z +
            localOrigin.x * masterAxis.mat[0].z) +
           localOrigin.z * masterAxis.mat[2].z),
  };
}

idVec3 WorldToLocal(const idVec3& worldOrigin,
                    const idVec3& masterOrigin,
                    const idMat3& masterAxis) {
  const float x = worldOrigin.x - masterOrigin.x;
  const float y = worldOrigin.y - masterOrigin.y;
  const float z = worldOrigin.z - masterOrigin.z;
  return {
      (masterAxis.mat[0].y * y + masterAxis.mat[0].x * x) +
          masterAxis.mat[0].z * z,
      (masterAxis.mat[1].y * y + masterAxis.mat[1].x * x) +
          masterAxis.mat[1].z * z,
      (masterAxis.mat[2].y * y + masterAxis.mat[2].x * x) +
          masterAxis.mat[2].z * z,
  };
}

std::uint8_t& FlagsByte(idPhysics_RigidBody3DOF::simpleBodyFlags_t& flags) {
  return *reinterpret_cast<std::uint8_t*>(&flags);
}

} // namespace

idPhysics_RigidBody3DOF::bodyState_t::bodyState_t()
    : atRest(-1), lastTimeStep(0.0f), localOrigin{},
      localAxis(mat3_identity), linearPushVelocity{}, angularPushVelocity{},
      externalForce{}, i{} {
  i.position = vec3_origin;
  i.orientation = mat3_identity;
  i.linearVelocity = vec3_origin;
  i.angularVelocity = vec3_origin;
}

idPhysics_RigidBody3DOF::idPhysics_RigidBody3DOF()
    : clipModel(nullptr), mass(1.0f), inverseMass(1.0f),
      linearFriction(1.0f), angularFriction(1.0f), contactFriction(1.0f),
      linearFrictionWater(1.0f), angularFrictionWater(1.0f),
      coefficientOfRestitution(1.0f), current(), saved(), random(0),
      translationQuery() {
  FlagsByte(flags) &= 0xe0u;
  type = PHYSICS_RIGIDBODY3DOF;
  random.SetSeed(0);
}

idPhysics_RigidBody3DOF::~idPhysics_RigidBody3DOF() {
  if (clipModel != nullptr) {
    clipModel->Delete();
    clipModel = nullptr;
  }
}

void idPhysics_RigidBody3DOF::SetFriction(float linear, float angular,
                                          float contact) {
  linearFriction = ClampUnitRetail(linear);
  angularFriction = ClampUnitRetail(angular);
  contactFriction = ClampUnitRetail(contact);
}

void idPhysics_RigidBody3DOF::SetWaterFriction(float linear,
                                               float angular) {
  linearFrictionWater = ClampUnitRetail(linear);
  angularFrictionWater = ClampUnitRetail(angular);
}

void idPhysics_RigidBody3DOF::SetCoefficientOfRestitution(float value) {
  coefficientOfRestitution = value;
}

void idPhysics_RigidBody3DOF::SetClipModel(idClipModel* model, float density,
                                           int id, bool freeOld) {
  if (clipModel != nullptr && clipModel != model && freeOld) {
    clipModel->Delete();
  }
  clipModel = model;
  model->Link(entityNumber, physicsId, id, current.i.position,
              current.i.orientation);
  const idVec3 size = model->bounds.b[1] - model->bounds.b[0];
  float volume = 0.0f;
  if (model->bounds.b[0].x < model->bounds.b[1].x &&
      model->bounds.b[0].y < model->bounds.b[1].y &&
      model->bounds.b[0].z < model->bounds.b[1].z) {
    volume = (size.x * size.y) * size.z;
  }
  SetMass(volume * density, -1);
  if (!(mass > 0.0f)) {
    idLib::Warning(
        "idPhysics_RigidBody3DOF::SetClipModel - invalid mass for entity '%s'.",
        callbacks->GetName());
    mass = 1.0f;
  }
  current.i.linearVelocity = vec3_origin;
  current.i.angularVelocity = vec3_origin;
}

void idPhysics_RigidBody3DOF::SetMass(float value, int) {
  mass = value;
  inverseMass = 1.0f / value;
}

float idPhysics_RigidBody3DOF::GetMass(int) const { return mass; }

void idPhysics_RigidBody3DOF::SetContents(int contents, int) {
  clipModel->SetContents(contents);
}

int idPhysics_RigidBody3DOF::GetContents(int) const {
  return clipModel->contents;
}

const idBounds& idPhysics_RigidBody3DOF::GetBounds(int) const {
  return clipModel->bounds;
}

const idBounds& idPhysics_RigidBody3DOF::GetAbsBounds(int) const {
  return clipModel->absBounds;
}

void idPhysics_RigidBody3DOF::Activate() { current.atRest = -1; }

void idPhysics_RigidBody3DOF::PutToRest() {
  current.atRest = 1;
  current.i.linearVelocity = vec3_origin;
  current.i.angularVelocity = vec3_origin;
}

bool idPhysics_RigidBody3DOF::IsAtRest() const {
  return current.atRest >= 0;
}

bool idPhysics_RigidBody3DOF::IsPushable(int) const {
  return !flags.hasMaster;
}

const idVec3& idPhysics_RigidBody3DOF::GetOrigin(int) const {
  return current.i.position;
}

const idMat3& idPhysics_RigidBody3DOF::GetAxis(int) const {
  return current.i.orientation;
}

idVec3 idPhysics_RigidBody3DOF::GetLinearVelocity(int) const {
  return current.i.linearVelocity;
}

idVec3 idPhysics_RigidBody3DOF::GetAngularVelocity(int) const {
  return current.i.angularVelocity;
}

void idPhysics_RigidBody3DOF::SetLinearVelocity(const idVec3& velocity,
                                                int) {
  current.i.linearVelocity = velocity;
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_RigidBody3DOF::SetAngularVelocity(const idVec3& velocity,
                                                 int) {
  current.i.angularVelocity = velocity;
  callbacks->ActivatePhysics(physicsId);
}

idVec3 idPhysics_RigidBody3DOF::GetPushedLinearVelocity(int) const {
  return current.linearPushVelocity;
}

idVec3 idPhysics_RigidBody3DOF::GetPushedAngularVelocity(int) const {
  return current.angularPushVelocity;
}

void idPhysics_RigidBody3DOF::UnlinkClip() { clipModel->Unlink(); }

void idPhysics_RigidBody3DOF::DisableClip() { clipModel->flags &= ~1u; }

void idPhysics_RigidBody3DOF::EnableClip() { clipModel->flags |= 1u; }

void idPhysics_RigidBody3DOF::LinkClip() {
  clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                  current.i.position, current.i.orientation);
}

void idPhysics_RigidBody3DOF::SetOrigin(const idVec3& newOrigin, int) {
  current.i.position = newOrigin;
  if (flags.hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localOrigin = WorldToLocal(newOrigin, masterOrigin, masterAxis);
  } else {
    current.localOrigin = newOrigin;
  }
  clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                  current.i.position, clipModel->axis);
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_RigidBody3DOF::SetAxis(const idMat3& newAxis, int) {
  current.i.orientation = newAxis;
  if (flags.hasMaster && flags.isOriented) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localAxis = newAxis * Transpose(masterAxis);
  } else {
    current.localAxis = newAxis;
  }
}

void idPhysics_RigidBody3DOF::Translate(const idVec3& translation, int) {
  if (clipModel == nullptr) {
    return;
  }
  const idVec3 newOrigin = clipModel->origin + translation;
  current.i.position = newOrigin;
  if (flags.hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localOrigin = WorldToLocal(newOrigin, masterOrigin, masterAxis);
  } else {
    current.localOrigin = newOrigin;
  }
  clipModel->Link(entityNumber, physicsId, clipModel->bodyId, newOrigin,
                  clipModel->axis);
}

void idPhysics_RigidBody3DOF::Rotate(const idRotation& rotation, int) {
  if (clipModel == nullptr) {
    return;
  }
  idMat3 newAxis = clipModel->axis;
  newAxis *= rotation.ToMat3();
  const idVec3 newOrigin = rotation * clipModel->origin;
  current.i.orientation = newAxis;
  current.i.position = newOrigin;
  if (flags.hasMaster && flags.isOriented) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.localAxis = newAxis * Transpose(masterAxis);
    current.localOrigin = WorldToLocal(newOrigin, masterOrigin, masterAxis);
  } else {
    current.localAxis = newAxis;
    current.localOrigin = newOrigin;
  }
  clipModel->Link(entityNumber, physicsId, clipModel->bodyId, newOrigin,
                  newAxis);
}

void idPhysics_RigidBody3DOF::SetMaster(bool enable,
                                        const idVec3& masterOrigin,
                                        const idMat3& masterAxis,
                                        bindFlags_t flags_) {
  if (enable) {
    if (!flags.hasMaster) {
      current.localOrigin =
          WorldToLocal(current.i.position, masterOrigin, masterAxis);
      if ((static_cast<int>(flags_) & 1) != 0) {
        current.localAxis = current.i.orientation * Transpose(masterAxis);
      } else {
        current.localAxis = current.i.orientation;
      }
      flags.isOriented = (static_cast<int>(flags_) & 1) != 0;
      flags.hasMaster = true;
      ClearContacts();
    }
  } else if (flags.hasMaster) {
    flags.hasMaster = false;
    callbacks->ActivatePhysics(physicsId);
  }
}

void idPhysics_RigidBody3DOF::SetLocalOrigin(const idVec3& newOrigin,
                                             int) {
  current.localOrigin = newOrigin;
  if (flags.hasMaster) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.i.position = LocalToWorld(newOrigin, masterOrigin, masterAxis);
  } else {
    current.i.position = newOrigin;
  }
  clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                  current.i.position, clipModel->axis);
  callbacks->ActivatePhysics(physicsId);
}

void idPhysics_RigidBody3DOF::SetLocalAxis(const idMat3& newAxis, int) {
  current.localAxis = newAxis;
  if (flags.hasMaster && flags.isOriented) {
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.i.orientation = newAxis * masterAxis;
  } else {
    current.i.orientation = newAxis;
  }
}

void idPhysics_RigidBody3DOF::GetImpactInfo(int, const idVec3&,
                                            impactInfo_t& info) const {
  info.invMass = inverseMass;
  info.invInertiaTensor = mat3_identity;
  info.position = current.i.position;
  info.velocity = current.i.linearVelocity;
}

void idPhysics_RigidBody3DOF::ApplyImpulse(int, const idVec3& point,
                                           const idVec3& impulse) {
  current.i.linearVelocity += impulse;
  callbacks->ActivatePhysics(physicsId);
  static_cast<void>(point);
}

void idPhysics_RigidBody3DOF::ApplyForce(int, const idVec3& point,
                                         const idVec3& force) {
  current.externalForce += force;
  callbacks->ActivatePhysics(physicsId);
  static_cast<void>(point);
}

void idPhysics_RigidBody3DOF::ClipTranslation(
    trace_t& results, const idVec3& translation,
    const idClipModel* model) const {
  const idVec3 end = clipModel->origin + translation;
  if (model != nullptr) {
    clip->TranslationModel(results, clipModel->origin, end, clipModel,
                           clipModel->axis, clipMask, model, model->origin,
                           model->axis, nullptr);
  } else {
    clip->Translation(&results, clipModel->origin, end, clipModel,
                      clipModel->axis, clipMask, entityNumber, false,
                      nullptr);
  }
}

void idPhysics_RigidBody3DOF::ClipRotation(
    trace_t& results, const idRotation& rotation,
    const idClipModel* model) const {
  if (model != nullptr) {
    clip->RotationModel(results, clipModel->origin, rotation, clipModel,
                        clipModel->axis, clipMask, model, model->origin,
                        model->axis, nullptr);
  } else {
    clip->Rotation(&results, clipModel->origin, rotation, clipModel,
                   clipModel->axis, clipMask, entityNumber, false, nullptr);
  }
}

int idPhysics_RigidBody3DOF::ClipContents(const idClipModel* model,
                                          int clipMask_) const {
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

void idPhysics_RigidBody3DOF::SaveState() { saved = current; }

void idPhysics_RigidBody3DOF::RestoreState() {
  current = saved;
  clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                  current.i.position, current.i.orientation);
}

void idPhysics_RigidBody3DOF::SetPushed(int deltaTime) {
  const idMat3 difference =
      MatrixMultiplyRetail(saved.i.orientation, current.i.orientation);
  const idQuat quaternion = MatrixToQuatRetail(difference);
  idVec3 axis;
  float angle;
  if (quaternion.w >= 1.0f - idMath::FLOAT_EPSILON * 4.0f) {
    angle = 0.0f;
    axis = {0.0f, 0.0f, 1.0f};
  } else {
    float halfAngle;
    if (quaternion.w > -1.0f) {
      halfAngle = quaternion.w < 1.0f ? RetailAcos(quaternion.w) : 0.0f;
    } else {
      halfAngle = idMath::PI;
    }
    angle = (halfAngle * 57.295776f) * 2.0f;
    axis = {quaternion.x, quaternion.y, quaternion.z};
    const float inverse = RetailInverseLength(axis);
    axis.x *= inverse;
    axis.y *= inverse;
    axis.z *= inverse;
  }
  const float angularScale = -(angle * idMath::M_DEG2RAD);
  const float inverseTime = 1.0f / (static_cast<float>(deltaTime) * 0.001f);
  current.angularPushVelocity.x =
      (axis.x * angularScale) * inverseTime + current.angularPushVelocity.x;
  current.angularPushVelocity.y =
      (axis.y * angularScale) * inverseTime + current.angularPushVelocity.y;
  current.angularPushVelocity.z =
      (axis.z * angularScale) * inverseTime + current.angularPushVelocity.z;
  current.linearPushVelocity.x =
      (current.i.position.x - saved.i.position.x) * inverseTime +
      current.linearPushVelocity.x;
  current.linearPushVelocity.y =
      (current.i.position.y - saved.i.position.y) * inverseTime +
      current.linearPushVelocity.y;
  current.linearPushVelocity.z =
      (current.i.position.z - saved.i.position.z) * inverseTime +
      current.linearPushVelocity.z;
}

bool idPhysics_RigidBody3DOF::CheckForCollisions(
    float, const bodyState_t& cur, bodyState_t& next,
    trace_t& collision) {
  clip->Translation(&collision, current.i.position, next.i.position,
                    clipModel, mat3_identity, clipMask, entityNumber, false,
                    nullptr);
  const float fraction = collision.fraction;
  if (fraction == 1.0f) {
    return false;
  }

  idVec3 change = next.i.linearVelocity - cur.i.linearVelocity;
  if (std::fabs(change.x) <= idMath::FLT_SMALLEST_NON_DENORMAL) {
    change.x = 0.0f;
  }
  if (std::fabs(change.y) <= idMath::FLT_SMALLEST_NON_DENORMAL) {
    change.y = 0.0f;
  }
  if (std::fabs(change.z) <= idMath::FLT_SMALLEST_NON_DENORMAL) {
    change.z = 0.0f;
  }
  next.i.position = collision.endpos;
  next.i.linearVelocity = cur.i.linearVelocity + change * fraction;

  if (!flags.noImpact) {
    float angularSpeed = RetailLength(cur.i.angularVelocity);
    if (angularSpeed <= 0.1f) {
      angularSpeed = random.RandomFloat() * 100.0f;
    }
    const float randomZ = random.RandomFloat() - 0.5f;
    const float randomY = random.RandomFloat() - 0.5f;
    const float randomX = random.RandomFloat() - 0.5f;
    idVec3 direction{randomX, randomY, randomZ};
    const float inverse = RetailInverseLength(direction);
    next.i.angularVelocity.x = (direction.x * inverse) * angularSpeed;
    next.i.angularVelocity.y = (direction.y * inverse) * angularSpeed;
    next.i.angularVelocity.z = (direction.z * inverse) * angularSpeed;
  }
  return true;
}

bool idPhysics_RigidBody3DOF::CollisionImpulse(
    trace_t& collision, float timeStep, bodyState_t& postImpactState,
    idVec3& impulse) {
  idVec3 otherVelocity = vec3_origin;
  idPhysics* other = idPhysics::GetPhysicsForId(collision.c.physicsId);
  if (other != nullptr && other != this) {
    impactInfo_t info{};
    other->GetImpactInfo(collision.c.bodyId, collision.c.point, info);
    otherVelocity = info.velocity;
  }

  const idVec3 relativeVelocity =
      postImpactState.i.linearVelocity - otherVelocity;
  const float normalSpeed = -(relativeVelocity * collision.c.normal);
  const idVec3 normalVelocity = collision.c.normal * normalSpeed;
  const float bounce = normalSpeed >= 10.0f
                           ? (coefficientOfRestitution + 1.0f) * normalSpeed
                           : 5.0f;
  impulse = collision.c.normal * bounce;
  impulse -= (normalVelocity + relativeVelocity) * contactFriction;
  impulse.FixDenormals();
  postImpactState.i.linearVelocity += impulse;
  postImpactState.i.linearVelocity += gravityVector * timeStep;
  postImpactState.i.angularVelocity =
      postImpactState.i.angularVelocity * coefficientOfRestitution;
  return callbacks->Collide(physicsId, collision, relativeVelocity) == 1;
}

void idPhysics_RigidBody3DOF::Integrate(float deltaTime,
                                        const bodyState_t& cur,
                                        bodyState_t& next) {
  next = cur;
  next.i.position.x =
      ((deltaTime * cur.i.linearVelocity.x) +
       (deltaTime * current.externalForce.x)) +
      next.i.position.x;
  next.i.position.y =
      ((deltaTime * cur.i.linearVelocity.y) +
       (deltaTime * current.externalForce.y)) +
      next.i.position.y;
  next.i.position.z =
      ((deltaTime * cur.i.linearVelocity.z) +
       (deltaTime * current.externalForce.z)) +
      next.i.position.z;

  const float angularSquared =
      ((cur.i.angularVelocity.y * cur.i.angularVelocity.y) +
       (cur.i.angularVelocity.x * cur.i.angularVelocity.x)) +
      (cur.i.angularVelocity.z * cur.i.angularVelocity.z);
  const float inverseAngular = RetailInverseLength(cur.i.angularVelocity);
  const float angularSpeed = inverseAngular * angularSquared;
  const idVec3 rotationAxis = cur.i.angularVelocity * inverseAngular;
  const float angle =
      (idMath::M_RAD2DEG * angularSpeed) *
      deltaTime;
  next.i.orientation *= RotationMatrixRetail(rotationAxis, angle);

  next.i.linearVelocity += gravityVector * deltaTime;
  next.i.linearVelocity -= next.i.linearVelocity * linearFriction;
  const float dampedAngular =
      angularSpeed - angularSpeed * angularFriction;
  next.i.angularVelocity = rotationAxis * dampedAngular;
}

bool idPhysics_RigidBody3DOF::Evaluate(int timeStepMSec, int) {
  const float deltaTime = static_cast<float>(timeStepMSec) * 0.001f;
  current.lastTimeStep = deltaTime;

  if (flags.hasMaster) {
    const idVec3 oldOrigin = current.i.position;
    const idMat3 oldAxis = current.i.orientation;
    idVec3 masterOrigin;
    idMat3 masterAxis;
    callbacks->GetMasterTransform(masterOrigin, masterAxis);
    current.i.position =
        LocalToWorld(current.localOrigin, masterOrigin, masterAxis);
    current.i.orientation = flags.isOriented
                                ? current.localAxis * masterAxis
                                : current.localAxis;
    clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                    current.i.position, current.i.orientation);
    const float inverseTime = 1.0f / deltaTime;
    current.i.linearVelocity =
        (current.i.position - oldOrigin) * inverseTime;
    current.i.angularVelocity = MatrixToAngularVelocityRetail(
                                    current.i.orientation * Transpose(oldAxis)) *
                                inverseTime;
    current.externalForce = vec3_origin;
    return current.i.position.x != oldOrigin.x ||
           current.i.position.y != oldOrigin.y ||
           current.i.position.z != oldOrigin.z ||
           !current.i.orientation.Compare(oldAxis, 0.0f);
  }

  if (current.atRest >= 0 || deltaTime <= 0.0f) {
    return false;
  }
  if (flags.dropToFloor) {
    callbacks->DeactivatePhysics(physicsId);
    current.externalForce = vec3_origin;
    return true;
  }

  clipModel->Unlink();
  bodyState_t next;
  Integrate(deltaTime, current, next);
  trace_t collision;
  const bool collided =
      CheckForCollisions(deltaTime, current, next, collision);
  current = next;
  idVec3 impulse = vec3_origin;
  if (collided) {
    const bool stop = CollisionImpulse(collision, deltaTime, current, impulse);
    const float vertical = current.i.linearVelocity * gravityNormal;
    const idVec3 horizontal =
        current.i.linearVelocity - gravityNormal * vertical;
    if (stop || (RetailLength(horizontal) <= 10.0f &&
                 std::fabs(vertical) < 20.0f)) {
      callbacks->DeactivatePhysics(physicsId);
    }
  }

  clipModel->Link(entityNumber, physicsId, clipModel->bodyId,
                  current.i.position, current.i.orientation);
  if (collided) {
    idPhysics* other = idPhysics::GetPhysicsForId(collision.c.physicsId);
    if (other != nullptr) {
      other->callbacks->ApplyImpulse(physicsId, collision.c.bodyId,
                                     collision.c.point, impulse * -1.0f);
    }
  }
  current.linearPushVelocity = vec3_origin;
  current.angularPushVelocity = vec3_origin;
  current.externalForce = vec3_origin;
  if (IsOutsideWorld()) {
    callbacks->DeactivatePhysics(physicsId);
  }
  return true;
}
