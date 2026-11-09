#pragma once

#include "engine/cm/collisionmodel.h"
#include "engine/gamelib/effectphysics/effectphysicsbroadphase.h"

#include <cstddef>
#include <cstdint>

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

class idCVar;
class idEffectPhysicsProperties;

struct impactInfo_t {
  float invMass;
  idMat3 invInertiaTensor;
  idVec3 position;
  idVec3 velocity;
};

class idEffectPhysicsCollision {
public:
#if defined(WOLFTNO_RETAIL_CUTTABLE_COLLISION_CTOR_ABI)
  idEffectPhysicsCollision() noexcept
      : collisionPoint(vec3_origin), collisionNormalVelocity(vec3_origin),
        collisionSurfaceType(0) {}
#endif

  idVec3 collisionPoint;
  idVec3 collisionNormalVelocity;
  int collisionSurfaceType;
};

class idEffectPhysicsRigidBody {
public:
  struct effectRBState_t {
    idVec3 position;
    idVec3 positionFiltered;
    idMat3 orientation;
    idMat3 orientationFiltered;
    idVec3 linearMomentum;
    idVec3 angularMomentum;
    idVec3 externalForce;
    idVec3 externalTorque;
  };

  struct effectRBDelta_t {
    idVec3 translation;
    idRotation rotation;
    idVec3 linearForce;
    idVec3 angularForce;
  };

  idEffectPhysicsRigidBody();
  ~idEffectPhysicsRigidBody() noexcept(false);

  void Setup(const idEffectPhysicsProperties* props);
  void Update(float deltaTime, idEffectPhysicsCollision& collision);
  void Disable();
  void ApplyImpulse(const idVec3& point, const idVec3& impulse);
  void SetOrientation(const idMat3& orientation);

  const idEffectPhysicsProperties* properties;
  effectRBState_t currentState;
  idSPObject SPObject;
  float simulationTimeFirstContact;
  float simulationTime;
  bool active;
  bool settled;
  bool enabled;

private:
  void CapVelocity(float deltaTime, effectRBState_t& current) const;
  void Evolve(float deltaTime, float unused,
              const effectRBState_t& current, effectRBDelta_t& delta) const;
  void CollisionImpulse(trace_t& collision, const impactInfo_t& impact,
                        float extraMomentumDecay, effectRBState_t& state,
                        idVec3& normalVelocity, idVec3& impulse) const;
};

static_assert(sizeof(impactInfo_t) == 0x40, "impact info ABI drift");
static_assert(offsetof(impactInfo_t, invInertiaTensor) == 0x04,
              "impact inertia offset drift");
static_assert(sizeof(idEffectPhysicsCollision) == 0x1C,
              "effect collision ABI drift");
static_assert(sizeof(idEffectPhysicsRigidBody::effectRBState_t) == 0x90,
              "effect rigid-body state ABI drift");
static_assert(offsetof(idEffectPhysicsRigidBody::effectRBState_t,
                       orientation) == 0x18,
              "effect orientation offset drift");
static_assert(offsetof(idEffectPhysicsRigidBody::effectRBState_t,
                       linearMomentum) == 0x60,
              "effect linear momentum offset drift");
static_assert(sizeof(idEffectPhysicsRigidBody::effectRBDelta_t) == 0x68,
              "effect rigid-body delta ABI drift");
static_assert(offsetof(idEffectPhysicsRigidBody::effectRBDelta_t,
                       linearForce) == 0x50,
              "effect linear-force offset drift");
static_assert(sizeof(idEffectPhysicsRigidBody) == 0x150,
              "effect rigid-body ABI drift");
static_assert(offsetof(idEffectPhysicsRigidBody, currentState) == 0x08,
              "effect rigid-body state offset drift");
static_assert(offsetof(idEffectPhysicsRigidBody, SPObject) == 0x98,
              "effect rigid-body spatial-object offset drift");
static_assert(offsetof(idEffectPhysicsRigidBody, simulationTimeFirstContact) ==
                  0x140,
              "effect first-contact time offset drift");
static_assert(offsetof(idEffectPhysicsRigidBody, enabled) == 0x14A,
              "effect enabled offset drift");

extern idCVar fx_positionFilterCoeff;
extern idCVar fx_rotationFilterCoeff;
