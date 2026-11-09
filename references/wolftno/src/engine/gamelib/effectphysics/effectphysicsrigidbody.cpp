#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#define WOLFTNO_RETAIL_IDVEC3_RADIANS_ABI
#include "engine/gamelib/effectphysics/effectphysicsrigidbody.h"

#include "engine/gamelib/effectphysics/effectphysicsproperties.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/math/random.h"

#undef WOLFTNO_RETAIL_IDVEC3_RADIANS_ABI
#undef WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <cmath>
#include <cstring>
#include <xmmintrin.h>

idCVar fx_positionFilterCoeff(
    "fx_positionFilterCoeff", "0.9", CVAR_FLOAT,
    "Positional LP filtering coefficient",
    static_cast<argCompletionRef_t>(nullptr));
idCVar fx_rotationFilterCoeff(
    "fx_rotationFilterCoeff", "0.8", CVAR_FLOAT,
    "Rotational LP filtering coefficient",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

__forceinline float InverseLength(float squaredLength) {
  const __m128 squared = _mm_set_ss(squaredLength);
  const __m128 clamped =
      _mm_max_ps(squared, idMath::vector_float_smallestNonDenorm);
  return _mm_cvtss_f32(
      _mm_div_ps(_mm_set1_ps(1.0f), _mm_sqrt_ps(clamped)));
}

__forceinline float ClampedLength(float squaredLength) {
  const __m128 squared = _mm_set_ss(squaredLength);
  return _mm_cvtss_f32(
      _mm_sqrt_ps(_mm_max_ps(squared,
                             idMath::vector_float_smallestNonDenorm)));
}

__forceinline float LengthSquared(const idVec3& vector) {
  return (vector.y * vector.y + vector.x * vector.x) +
         vector.z * vector.z;
}

__forceinline float Dot(const idVec3& left, const idVec3& right) {
  return (left.x * right.x + left.y * right.y) + left.z * right.z;
}

__forceinline idVec3 Cross(const idVec3& left, const idVec3& right) {
  return idVec3{left.y * right.z - left.z * right.y,
                left.z * right.x - left.x * right.z,
                left.x * right.y - left.y * right.x};
}

__forceinline void LimitMomentum(idVec3& momentum, float oldLengthSquared) {
  const float newLengthSquared = LengthSquared(momentum);
  if (newLengthSquared > oldLengthSquared) {
    const float inverseNewLength = InverseLength(newLengthSquared);
    const float oldLength = ClampedLength(oldLengthSquared);
    momentum.x = (oldLength * momentum.x) * inverseNewLength;
    momentum.y = (oldLength * momentum.y) * inverseNewLength;
    momentum.z = (oldLength * momentum.z) * inverseNewLength;
  }
}

} // namespace

idEffectPhysicsRigidBody::idEffectPhysicsRigidBody()
    : properties(nullptr), SPObject(), simulationTime(0.0f) {}

idEffectPhysicsRigidBody::~idEffectPhysicsRigidBody() noexcept(false) {
  const idEffectPhysicsProperties* currentProperties = properties;
  if (currentProperties != nullptr &&
      currentProperties->broadPhase != nullptr) {
    if (enabled) {
      enabled = false;
      currentProperties->broadPhase->RemoveObject(&SPObject);
    }
    properties = nullptr;
  }
  active = false;
}

void idEffectPhysicsRigidBody::Disable() {
  if (enabled) {
    enabled = false;
    properties->broadPhase->RemoveObject(&SPObject);
  }
}

void idEffectPhysicsRigidBody::CapVelocity(float,
                                           effectRBState_t& current) const {
  const idEffectPhysicsProperties* currentProperties = properties;
  const float inverseMass = currentProperties->inverseMass;
  const idVec3 linearVelocity{
      inverseMass * current.linearMomentum.x,
      inverseMass * current.linearMomentum.y,
      inverseMass * current.linearMomentum.z};
  idMat3 orientationTranspose;
  orientationTranspose.mat[0] =
      idVec3{current.orientation.mat[0].x, current.orientation.mat[1].x,
             current.orientation.mat[2].x};
  orientationTranspose.mat[1] =
      idVec3{current.orientation.mat[0].y, current.orientation.mat[1].y,
             current.orientation.mat[2].y};
  orientationTranspose.mat[2] =
      idVec3{current.orientation.mat[0].z, current.orientation.mat[1].z,
             current.orientation.mat[2].z};
  const idMat3 inverseWorldInertiaTensor =
      (orientationTranspose *
       currentProperties->inverseInertiaTensor) *
      current.orientation;
  const idVec3 angularVelocity{
      (current.angularMomentum.x * inverseWorldInertiaTensor.mat[0].x +
       current.angularMomentum.y * inverseWorldInertiaTensor.mat[1].x) +
          current.angularMomentum.z * inverseWorldInertiaTensor.mat[2].x,
      (current.angularMomentum.x * inverseWorldInertiaTensor.mat[0].y +
       current.angularMomentum.y * inverseWorldInertiaTensor.mat[1].y) +
          current.angularMomentum.z * inverseWorldInertiaTensor.mat[2].y,
      (current.angularMomentum.x * inverseWorldInertiaTensor.mat[0].z +
       current.angularMomentum.y * inverseWorldInertiaTensor.mat[1].z) +
          current.angularMomentum.z * inverseWorldInertiaTensor.mat[2].z};

  const float linearLengthSquared = LengthSquared(linearVelocity);
  const float maxLinearVelocity = currentProperties->maxLinearVelocity;
  if (linearLengthSquared > maxLinearVelocity * maxLinearVelocity) {
    const float scale =
        maxLinearVelocity * InverseLength(linearLengthSquared);
    current.linearMomentum.x = scale * current.linearMomentum.x;
    current.linearMomentum.y = scale * current.linearMomentum.y;
    current.linearMomentum.z = scale * current.linearMomentum.z;
  }

  const float angularLengthSquared = LengthSquared(angularVelocity);
  const float maxAngularVelocity = currentProperties->maxAngularVelocity;
  if (angularLengthSquared > maxAngularVelocity * maxAngularVelocity) {
    const float scale =
        maxAngularVelocity * InverseLength(angularLengthSquared);
    current.angularMomentum.x = scale * current.angularMomentum.x;
    current.angularMomentum.y = scale * current.angularMomentum.y;
    current.angularMomentum.z = scale * current.angularMomentum.z;
  }
}

void idEffectPhysicsRigidBody::CollisionImpulse(
    trace_t& collision, const impactInfo_t& impact,
    float extraMomentumDecay, effectRBState_t& state,
    idVec3& normalVelocity, idVec3& impulse) const {
  const idEffectPhysicsProperties* currentProperties = properties;
  const float inverseMass = currentProperties->inverseMass;
  const idVec3 linearVelocity{
      inverseMass * state.linearMomentum.x,
      inverseMass * state.linearMomentum.y,
      inverseMass * state.linearMomentum.z};
  idMat3 orientationTranspose;
  orientationTranspose.mat[0] =
      idVec3{state.orientation.mat[0].x, state.orientation.mat[1].x,
             state.orientation.mat[2].x};
  orientationTranspose.mat[1] =
      idVec3{state.orientation.mat[0].y, state.orientation.mat[1].y,
             state.orientation.mat[2].y};
  orientationTranspose.mat[2] =
      idVec3{state.orientation.mat[0].z, state.orientation.mat[1].z,
             state.orientation.mat[2].z};
  const idMat3 inverseWorldInertiaTensor =
      (orientationTranspose *
       currentProperties->inverseInertiaTensor) *
      state.orientation;
  const idVec3 angularVelocity{
      (state.angularMomentum.x * inverseWorldInertiaTensor.mat[0].x +
       state.angularMomentum.y * inverseWorldInertiaTensor.mat[1].x) +
          state.angularMomentum.z * inverseWorldInertiaTensor.mat[2].x,
      (state.angularMomentum.x * inverseWorldInertiaTensor.mat[0].y +
       state.angularMomentum.y * inverseWorldInertiaTensor.mat[1].y) +
          state.angularMomentum.z * inverseWorldInertiaTensor.mat[2].y,
      (state.angularMomentum.x * inverseWorldInertiaTensor.mat[0].z +
       state.angularMomentum.y * inverseWorldInertiaTensor.mat[1].z) +
          state.angularMomentum.z * inverseWorldInertiaTensor.mat[2].z};

  idVec3 collisionNormal = collision.c.normal;
  if (properties->crazyBounceChance > 0.0f) {
    static idRandom random;
    if (properties->crazyBounceChance >= random.RandomFloat()) {
      idVec3 axis;
      const int positiveAxis = random.RandomInt(3);
      axis.x = positiveAxis == 0
                   ? random.RandomFloat() * 0.9f + 0.1f
                   : random.CRandomFloat();
      axis.y = positiveAxis == 1
                   ? random.RandomFloat() * 0.9f + 0.1f
                   : random.CRandomFloat();
      axis.z = positiveAxis == 2
                   ? random.RandomFloat() * 0.9f + 0.1f
                   : random.CRandomFloat();
      const float inverseAxisLength = InverseLength(LengthSquared(axis));
      axis.x *= inverseAxisLength;
      axis.y *= inverseAxisLength;
      axis.z *= inverseAxisLength;
      const float angle = random.CRandomFloat() *
                          (3.1415927f * 0.25f) *
                          (1.0f - extraMomentumDecay);
      collisionNormal.RotateSelf(axis, idRadians(angle));
    }
  }

  const idVec3 relativePosition{
      collision.c.point.x - state.position.x,
      collision.c.point.y - state.position.y,
      collision.c.point.z - state.position.z};
  const idVec3 angularPointVelocity =
      Cross(angularVelocity, relativePosition);
  const idVec3 relativeVelocity{
      (angularPointVelocity.x + linearVelocity.x) - impact.velocity.x,
      (angularPointVelocity.y + linearVelocity.y) - impact.velocity.y,
      (angularPointVelocity.z + linearVelocity.z) - impact.velocity.z};
  const float normalMagnitude = -Dot(relativeVelocity, collisionNormal);
  normalVelocity.x = collisionNormal.x * normalMagnitude;
  normalVelocity.y = collisionNormal.y * normalMagnitude;
  normalVelocity.z = collisionNormal.z * normalMagnitude;

  const bool simplePointCollision = currentProperties->simplePointCollision;
  float impulseDenominator = currentProperties->inverseMass;
  const float impulseNumerator =
      (currentProperties->bouncyness + 1.0f) * normalMagnitude;
  if (!simplePointCollision) {
    const idVec3 bodyCross = Cross(relativePosition, collisionNormal);
    const idVec3 bodyAngular{
        (bodyCross.x * inverseWorldInertiaTensor.mat[0].x +
         bodyCross.y * inverseWorldInertiaTensor.mat[1].x) +
            bodyCross.z * inverseWorldInertiaTensor.mat[2].x,
        (bodyCross.x * inverseWorldInertiaTensor.mat[0].y +
         bodyCross.y * inverseWorldInertiaTensor.mat[1].y) +
            bodyCross.z * inverseWorldInertiaTensor.mat[2].y,
        (bodyCross.x * inverseWorldInertiaTensor.mat[0].z +
         bodyCross.y * inverseWorldInertiaTensor.mat[1].z) +
            bodyCross.z * inverseWorldInertiaTensor.mat[2].z};
    impulseDenominator += Dot(Cross(bodyAngular, relativePosition),
                              collisionNormal);
  }
  if (impact.invMass != 0.0f) {
    impulseDenominator += impact.invMass;
    if (!simplePointCollision) {
      const idVec3 impactCross = Cross(impact.position, collisionNormal);
      const idVec3 impactAngular{
          (impactCross.x * impact.invInertiaTensor.mat[0].x +
           impactCross.y * impact.invInertiaTensor.mat[1].x) +
              impactCross.z * impact.invInertiaTensor.mat[2].x,
          (impactCross.x * impact.invInertiaTensor.mat[0].y +
           impactCross.y * impact.invInertiaTensor.mat[1].y) +
              impactCross.z * impact.invInertiaTensor.mat[2].y,
          (impactCross.x * impact.invInertiaTensor.mat[0].z +
           impactCross.y * impact.invInertiaTensor.mat[1].z) +
              impactCross.z * impact.invInertiaTensor.mat[2].z};
      impulseDenominator +=
          Dot(Cross(impactAngular, impact.position), collisionNormal);
    }
  }
  if (impulseDenominator > 0.0f) {
    const float magnitude = impulseNumerator / impulseDenominator;
    impulse.z = collisionNormal.z * magnitude;
    impulse.x = magnitude * collisionNormal.x;
    impulse.y = magnitude * collisionNormal.y;
  }

  idVec3 tangent{
      -(relativeVelocity.x + normalVelocity.x),
      -(relativeVelocity.y + normalVelocity.y),
      -(relativeVelocity.z + normalVelocity.z)};
  const float tangentLengthSquared = LengthSquared(tangent);
  const float inverseTangentLength = InverseLength(tangentLengthSquared);
  tangent.x *= inverseTangentLength;
  tangent.y *= inverseTangentLength;
  tangent.z *= inverseTangentLength;

  float frictionDenominator = currentProperties->inverseMass;
  if (!simplePointCollision) {
    const idVec3 bodyCross = Cross(relativePosition, tangent);
    const idVec3 bodyAngular{
        (bodyCross.x * inverseWorldInertiaTensor.mat[0].x +
         bodyCross.y * inverseWorldInertiaTensor.mat[1].x) +
            bodyCross.z * inverseWorldInertiaTensor.mat[2].x,
        (bodyCross.x * inverseWorldInertiaTensor.mat[0].y +
         bodyCross.y * inverseWorldInertiaTensor.mat[1].y) +
            bodyCross.z * inverseWorldInertiaTensor.mat[2].y,
        (bodyCross.x * inverseWorldInertiaTensor.mat[0].z +
         bodyCross.y * inverseWorldInertiaTensor.mat[1].z) +
            bodyCross.z * inverseWorldInertiaTensor.mat[2].z};
    frictionDenominator +=
        Dot(Cross(bodyAngular, relativePosition), tangent);
  }
  if (frictionDenominator > 0.0f) {
    const float magnitude =
        (inverseTangentLength * tangentLengthSquared *
         currentProperties->contactFriction) /
        frictionDenominator;
    const int normalYInteger = static_cast<int>(collisionNormal.y);
    const float normalYScale = static_cast<float>(
        normalYInteger < 0 ? -normalYInteger : normalYInteger);
    impulse.x += magnitude * tangent.x * normalYScale;
    impulse.y += magnitude * tangent.y * normalYScale;
    impulse.z += magnitude * tangent.z * normalYScale;
  }

  state.linearMomentum.x = impulse.x + state.linearMomentum.x;
  state.linearMomentum.y = impulse.y + state.linearMomentum.y;
  state.linearMomentum.z = state.linearMomentum.z + impulse.z;
  const idVec3 angularImpulse = Cross(relativePosition, impulse);
  state.angularMomentum.x = angularImpulse.x + state.angularMomentum.x;
  state.angularMomentum.y = angularImpulse.y + state.angularMomentum.y;
  state.angularMomentum.z = angularImpulse.z + state.angularMomentum.z;
}

void idEffectPhysicsRigidBody::ApplyImpulse(const idVec3& point,
                                             const idVec3& impulse) {
  settled = false;
  currentState.linearMomentum.x += impulse.x;
  currentState.linearMomentum.y =
      impulse.y + currentState.linearMomentum.y;
  currentState.linearMomentum.z += impulse.z;
  const idVec3 relativePosition{
      point.x - currentState.position.x,
      point.y - currentState.position.y,
      point.z - currentState.position.z};
  const idVec3 angularImpulse = Cross(relativePosition, impulse);
  currentState.angularMomentum.x += angularImpulse.x;
  currentState.angularMomentum.z =
      angularImpulse.z + currentState.angularMomentum.z;
  currentState.angularMomentum.y =
      angularImpulse.y + currentState.angularMomentum.y;
}

void idEffectPhysicsRigidBody::Setup(
    const idEffectPhysicsProperties* props) {
  properties = props;
  currentState.position = idVec3{0.0f, 0.0f, 0.0f};
  currentState.positionFiltered = idVec3{0.0f, 0.0f, 0.0f};
  currentState.orientation.mat[0] = idVec3{1.0f, 0.0f, 0.0f};
  currentState.orientation.mat[1] = idVec3{0.0f, 1.0f, 0.0f};
  currentState.orientation.mat[2] = idVec3{0.0f, 0.0f, 1.0f};
  currentState.orientationFiltered.mat[0] =
      idVec3{1.0f, 0.0f, 0.0f};
  currentState.orientationFiltered.mat[1] =
      idVec3{0.0f, 1.0f, 0.0f};
  currentState.orientationFiltered.mat[2] =
      idVec3{0.0f, 0.0f, 1.0f};
  currentState.linearMomentum = idVec3{0.0f, 0.0f, 0.0f};
  currentState.angularMomentum = idVec3{0.0f, 0.0f, 0.0f};
  currentState.externalForce = idVec3{0.0f, 0.0f, 0.0f};
  currentState.externalTorque = idVec3{0.0f, 0.0f, 0.0f};
  enabled = false;
  active = false;
  settled = false;
  SPObject.traceModelIndex = props->traceModelIndex;
  SPObject.motion.worldCollisionOnly = props->worldCollisionOnly;
  enabled = true;
  SPObject.clipMask = props->clipMask;
  currentState.linearMomentum = idVec3{0.0f, 0.0f, 0.0f};
  currentState.angularMomentum = idVec3{0.0f, 0.0f, 0.0f};
  currentState.externalForce = idVec3{0.0f, 0.0f, 0.0f};
  currentState.externalTorque = idVec3{0.0f, 0.0f, 0.0f};
  properties->broadPhase->AddObject(
      &SPObject, currentState.position, currentState.orientation, nullptr);
}

void idEffectPhysicsRigidBody::Evolve(float deltaTime, float,
                                       const effectRBState_t& current,
                                       effectRBDelta_t& delta) const {
  const idEffectPhysicsProperties* currentProperties = properties;
  idMat3 orientationTranspose;
  orientationTranspose.mat[0] =
      idVec3{current.orientation.mat[0].x, current.orientation.mat[1].x,
             current.orientation.mat[2].x};
  orientationTranspose.mat[1] =
      idVec3{current.orientation.mat[0].y, current.orientation.mat[1].y,
             current.orientation.mat[2].y};
  orientationTranspose.mat[2] =
      idVec3{current.orientation.mat[0].z, current.orientation.mat[1].z,
             current.orientation.mat[2].z};
  const idMat3 inverseWorldInertiaTensor =
      (orientationTranspose *
       currentProperties->inverseInertiaTensor) *
      current.orientation;
  const idVec3 angularVelocity{
      (current.angularMomentum.x * inverseWorldInertiaTensor.mat[0].x +
       current.angularMomentum.y * inverseWorldInertiaTensor.mat[1].x) +
          current.angularMomentum.z * inverseWorldInertiaTensor.mat[2].x,
      (current.angularMomentum.x * inverseWorldInertiaTensor.mat[0].y +
       current.angularMomentum.y * inverseWorldInertiaTensor.mat[1].y) +
          current.angularMomentum.z * inverseWorldInertiaTensor.mat[2].y,
      (current.angularMomentum.x * inverseWorldInertiaTensor.mat[0].z +
       current.angularMomentum.y * inverseWorldInertiaTensor.mat[1].z) +
          current.angularMomentum.z * inverseWorldInertiaTensor.mat[2].z};
  const float angularLengthSquared = LengthSquared(angularVelocity);
  const float inverseAngularLength = InverseLength(angularLengthSquared);
  const idVec3 rotationVector{
      angularVelocity.x * inverseAngularLength,
      angularVelocity.y * inverseAngularLength,
      angularVelocity.z * inverseAngularLength};
  const float rotationAngle =
      -(inverseAngularLength * angularLengthSquared * 57.295776f) *
      deltaTime;

  const float inverseMass = currentProperties->inverseMass;
  delta.translation.x =
      deltaTime * (inverseMass * current.linearMomentum.x);
  delta.translation.y =
      (inverseMass * current.linearMomentum.y) * deltaTime;
  delta.translation.z =
      (inverseMass * current.linearMomentum.z) * deltaTime;
  delta.rotation.origin = current.position;
  delta.rotation.vec = rotationAngle != 0.0f
                           ? rotationVector
                           : idVec3{0.0f, 0.0f, 1.0f};
  delta.rotation.angle = rotationAngle;
  delta.rotation.axisValid = false;

  const float waterFraction = 0.0f;
  const float linearFriction =
      (1.0f - waterFraction) * currentProperties->linearFriction +
      currentProperties->linearFrictionWater * waterFraction;
  delta.linearForce.x =
      deltaTime * (current.externalForce.x -
                   linearFriction * current.linearMomentum.x);
  delta.linearForce.y =
      (current.externalForce.y -
       linearFriction * current.linearMomentum.y) *
      deltaTime;
  delta.linearForce.z =
      (current.externalForce.z -
       linearFriction * current.linearMomentum.z) *
      deltaTime;

  const float angularFriction =
      (1.0f - waterFraction) * currentProperties->angularFriction +
      currentProperties->angularFrictionWater * waterFraction;
  delta.angularForce.x =
      (current.externalTorque.x -
       angularFriction * current.angularMomentum.x) *
      deltaTime;
  delta.angularForce.y =
      (current.externalTorque.y -
       angularFriction * current.angularMomentum.y) *
      deltaTime;
  delta.angularForce.z =
      (current.externalTorque.z -
       angularFriction * current.angularMomentum.z) *
      deltaTime;
}

void idEffectPhysicsRigidBody::Update(float deltaTime,
                                      idEffectPhysicsCollision& collision) {
  if (!active || settled || !enabled) {
    return;
  }

  float extraMomentumDecay = 0.0f;
  if (simulationTimeFirstContact >= 0.0f) {
    const float normalizedTime =
        (simulationTime - simulationTimeFirstContact) /
        (static_cast<float>(properties->maxSimulationTime) -
         simulationTimeFirstContact);
    if (normalizedTime >= 0.5f) {
      float blend = (normalizedTime - 0.5f) * 2.0f;
      if (blend < 0.0f) {
        blend = 0.0f;
      } else if (blend > 1.0f) {
        blend = 1.0f;
      }
      extraMomentumDecay = ::powf(blend, 4.0f);
    }
  }

  bool collidedWithSettlingSurface = false;
  collision.collisionPoint = idVec3{0.0f, 0.0f, 0.0f};
  collision.collisionNormalVelocity = idVec3{0.0f, 0.0f, 0.0f};
  collision.collisionSurfaceType = 0;
  const float oldLinearMomentumSquared =
      LengthSquared(currentState.linearMomentum);
  const float oldAngularMomentumSquared =
      LengthSquared(currentState.angularMomentum);

  const std::uint64_t queryOffset =
      SPObject.query.offset & 0x3FFFFFFFFFFFFFULL;
  if (queryOffset >= idCollisionQuery::finalResultsFirstSubmittedOffset &&
      queryOffset < idCollisionQuery::finalResultsLastSubmittedOffset) {
    alignas(trace_t) std::uint8_t resultStorage[sizeof(trace_t)];
    trace_t& result = *reinterpret_cast<trace_t*>(resultStorage);
    properties->broadPhase->GetMotionResult(&result, &SPObject);
    currentState.position = SPObject.origin;
    currentState.orientation = SPObject.axis;

    const float positionCoefficient = fx_positionFilterCoeff.valueFloat;
    const float oldPositionCoefficient = 1.0f - positionCoefficient;
    currentState.positionFiltered.x =
        oldPositionCoefficient * currentState.positionFiltered.x +
        positionCoefficient * currentState.position.x;
    currentState.positionFiltered.y =
        oldPositionCoefficient * currentState.positionFiltered.y +
        positionCoefficient * currentState.position.y;
    currentState.positionFiltered.z =
        oldPositionCoefficient * currentState.positionFiltered.z +
        positionCoefficient * currentState.position.z;

    idMat3 currentOrientationTranspose;
    currentOrientationTranspose.mat[0] =
        idVec3{currentState.orientation.mat[0].x,
               currentState.orientation.mat[1].x,
               currentState.orientation.mat[2].x};
    currentOrientationTranspose.mat[1] =
        idVec3{currentState.orientation.mat[0].y,
               currentState.orientation.mat[1].y,
               currentState.orientation.mat[2].y};
    currentOrientationTranspose.mat[2] =
        idVec3{currentState.orientation.mat[0].z,
               currentState.orientation.mat[1].z,
               currentState.orientation.mat[2].z};
    const idQuat currentOrientation = currentOrientationTranspose.ToQuat();
    idMat3 filteredOrientationTranspose;
    filteredOrientationTranspose.mat[0] =
        idVec3{currentState.orientationFiltered.mat[0].x,
               currentState.orientationFiltered.mat[1].x,
               currentState.orientationFiltered.mat[2].x};
    filteredOrientationTranspose.mat[1] =
        idVec3{currentState.orientationFiltered.mat[0].y,
               currentState.orientationFiltered.mat[1].y,
               currentState.orientationFiltered.mat[2].y};
    filteredOrientationTranspose.mat[2] =
        idVec3{currentState.orientationFiltered.mat[0].z,
               currentState.orientationFiltered.mat[1].z,
               currentState.orientationFiltered.mat[2].z};
    const idQuat filteredOrientation =
        filteredOrientationTranspose.ToQuat();
    idQuat blendedOrientation;
    blendedOrientation.Slerp(filteredOrientation, currentOrientation,
                             fx_rotationFilterCoeff.valueFloat);
    blendedOrientation.Normalize();
    const idMat3 blendedMatrix = blendedOrientation.ToMat3();
    currentState.orientationFiltered.mat[0] =
        idVec3{blendedMatrix.mat[0].x, blendedMatrix.mat[1].x,
               blendedMatrix.mat[2].x};
    currentState.orientationFiltered.mat[1] =
        idVec3{blendedMatrix.mat[0].y, blendedMatrix.mat[1].y,
               blendedMatrix.mat[2].y};
    currentState.orientationFiltered.mat[2] =
        idVec3{blendedMatrix.mat[0].z, blendedMatrix.mat[1].z,
               blendedMatrix.mat[2].z};

    if (result.fraction < 1.0f) {
      impactInfo_t impact;
      impact.invMass = 1.0f;
      impact.invInertiaTensor.mat[0] = idVec3{1.0f, 0.0f, 0.0f};
      impact.invInertiaTensor.mat[1] = idVec3{0.0f, 1.0f, 0.0f};
      impact.invInertiaTensor.mat[2] = idVec3{0.0f, 0.0f, 1.0f};
      impact.position = idVec3{0.0f, 0.0f, 0.0f};
      impact.velocity = idVec3{0.0f, 0.0f, 0.0f};
      idVec3 normalVelocity;
      idVec3 impulse;
      CollisionImpulse(result, impact, extraMomentumDecay, currentState,
                       normalVelocity, impulse);
      collision.collisionPoint = result.c.point;
      collision.collisionNormalVelocity = normalVelocity;
      collision.collisionSurfaceType = result.c.surfaceType;
      collidedWithSettlingSurface = result.c.physicsId != 4;
      if (collidedWithSettlingSurface &&
          simulationTimeFirstContact < 0.0f) {
        simulationTimeFirstContact = simulationTime;
      }
      properties->broadPhase->SetObjectPosition(
          &SPObject, currentState.position,
          currentState.orientationFiltered, nullptr);
    }
  }

  CapVelocity(deltaTime, currentState);
  alignas(effectRBDelta_t)
      std::uint8_t deltaStorage[sizeof(effectRBDelta_t)];
  effectRBDelta_t& delta =
      *reinterpret_cast<effectRBDelta_t*>(deltaStorage);
  std::memset(&delta, 0, 40);
  Evolve(deltaTime, extraMomentumDecay, currentState, delta);
  currentState.linearMomentum.x += delta.linearForce.x;
  currentState.linearMomentum.y += delta.linearForce.y;
  currentState.linearMomentum.z += delta.linearForce.z;
  currentState.angularMomentum.x =
      (delta.angularForce.x + currentState.angularMomentum.x) *
      (1.0f - extraMomentumDecay);
  currentState.angularMomentum.y =
      (delta.angularForce.y + currentState.angularMomentum.y) *
      (1.0f - extraMomentumDecay);
  currentState.angularMomentum.z =
      (delta.angularForce.z + currentState.angularMomentum.z) *
      (1.0f - extraMomentumDecay);

  LimitMomentum(currentState.linearMomentum, oldLinearMomentumSquared);
  LimitMomentum(currentState.angularMomentum, oldAngularMomentumSquared);

  const float gravityX = properties->mass * properties->gravityVector.x;
  const float gravityY = properties->mass * properties->gravityVector.y;
  const float gravityZ = properties->mass * properties->gravityVector.z;
  currentState.linearMomentum.x += gravityX * deltaTime;
  currentState.linearMomentum.y += gravityY * deltaTime;
  currentState.linearMomentum.z += gravityZ * deltaTime;

  const float inverseMass = properties->inverseMass;
  delta.translation.x =
      deltaTime * (inverseMass * currentState.linearMomentum.x);
  delta.translation.y =
      (inverseMass * currentState.linearMomentum.y) * deltaTime;
  delta.translation.z =
      (inverseMass * currentState.linearMomentum.z) * deltaTime;
  properties->broadPhase->ApplyMotion(&SPObject, delta.translation,
                                      delta.rotation);
  currentState.position.x += delta.translation.x;
  currentState.position.y += delta.translation.y;
  currentState.position.z += delta.translation.z;
  currentState.orientation =
      currentState.orientation * delta.rotation.ToMat3();

  if (collidedWithSettlingSurface &&
      LengthSquared(currentState.linearMomentum) < 1.0f &&
      LengthSquared(currentState.angularMomentum) < 0.25f) {
    settled = true;
  }
  if (settled) {
    SPObject.motion.valid = false;
    currentState.linearMomentum = idVec3{0.0f, 0.0f, 0.0f};
    currentState.angularMomentum = idVec3{0.0f, 0.0f, 0.0f};
    collision.collisionPoint = idVec3{0.0f, 0.0f, 0.0f};
    collision.collisionNormalVelocity = idVec3{0.0f, 0.0f, 0.0f};
    collision.collisionSurfaceType = 0;
  }
  currentState.externalForce = idVec3{0.0f, 0.0f, 0.0f};
  currentState.externalTorque = idVec3{0.0f, 0.0f, 0.0f};
  simulationTime = deltaTime + simulationTime;
}
