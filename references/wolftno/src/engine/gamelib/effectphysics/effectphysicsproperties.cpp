#include "engine/gamelib/effectphysics/effectphysicsproperties.h"

namespace {
constexpr float MAX_INERTIA_SCALE = 4.0f;
constexpr float INVERSE_INERTIA_SCALE = 1.0f / 6.0f;
}

idEffectPhysicsProperties::idEffectPhysicsProperties()
    : broadPhase(nullptr), traceModelIndex(-1), clipMask(0),
      linearFriction(0.0f), angularFriction(0.0f), contactFriction(0.0f),
      linearFrictionWater(0.0f), angularFrictionWater(0.0f),
      bouncyness(0.0f), gravityVector{}, worldCollisionOnly(false),
      simplePointCollision(false), crazyBounceChance(0.0f),
      mass(1.0f), inverseMass(1.0f), centerOfMass{},
      inertiaTensor(mat3_identity), inverseInertiaTensor(mat3_identity),
      stopSpeed(0.0f), maxLinearVelocity(0.0f), maxAngularVelocity(0.0f),
      maxSimulationTime(2) {}

void idEffectPhysicsProperties::Setup(idEffectPhysicsBroadPhase* broadPhase_,
                                      int traceModelIndex_, float mass_) {
  clipMask = 1;
  linearFriction = 0.6f;
  angularFriction = 0.6f;
  contactFriction = 0.5f;
  linearFrictionWater = 1.0f;
  angularFrictionWater = 1.0f;
  bouncyness = 0.6f;
  gravityVector = idVec3{0.0f, 0.0f, -250.0f};
  worldCollisionOnly = false;
  simplePointCollision = false;
  crazyBounceChance = 0.0f;
  inverseMass = 1.0f;
  centerOfMass = idVec3{};
  inertiaTensor = mat3_identity;
  inverseInertiaTensor = mat3_identity;
  stopSpeed = 10.0f;
  maxLinearVelocity = 5000.0f;
  maxAngularVelocity = 4.0f * 3.1415927f;
  broadPhase = broadPhase_;
  traceModelIndex = traceModelIndex_;

  broadPhase_->traceModelCache->GetMassProperties(
      traceModelIndex_, 1.0f, mass, centerOfMass, inertiaTensor);
  if (mass <= 0.0f) {
    mass = 1.0f;
    centerOfMass = idVec3{};
    inertiaTensor = mat3_identity;
  }

  const float massScale = mass_ / mass;
  for (idVec3& row : inertiaTensor.mat) {
    row *= massScale;
  }
  mass = mass_;

  int smallestAxis = 0;
  if (inertiaTensor[1][1] <= inertiaTensor[0][0]) {
    smallestAxis = 1;
  }
  if (inertiaTensor[2][2] <= inertiaTensor[smallestAxis][smallestAxis]) {
    smallestAxis = 2;
  }

  const float minimumInertia = inertiaTensor[smallestAxis][smallestAxis];
  idMat3 inertiaScale{};
  inertiaScale[0][0] = inertiaTensor[0][0] / minimumInertia;
  inertiaScale[1][1] = inertiaTensor[1][1] / minimumInertia;
  inertiaScale[2][2] = inertiaTensor[2][2] / minimumInertia;

  if (inertiaScale[0][0] > MAX_INERTIA_SCALE ||
      inertiaScale[1][1] > MAX_INERTIA_SCALE ||
      inertiaScale[2][2] > MAX_INERTIA_SCALE) {
    const int nextAxis = (smallestAxis + 1) % 3;
    const int lastAxis = (smallestAxis + 2) % 3;
    inertiaScale[smallestAxis][smallestAxis] = 1.0f;
    inertiaScale[nextAxis][nextAxis] =
        minimumInertia * MAX_INERTIA_SCALE /
        inertiaTensor[nextAxis][nextAxis];
    inertiaScale[lastAxis][lastAxis] =
        minimumInertia * MAX_INERTIA_SCALE /
        inertiaTensor[lastAxis][lastAxis];
    inertiaTensor = inertiaTensor * inertiaScale;
  }

  inverseMass = 1.0f / mass;
  inverseInertiaTensor = inertiaTensor;
  inverseInertiaTensor.InverseSelf();
  for (idVec3& row : inverseInertiaTensor.mat) {
    row *= INVERSE_INERTIA_SCALE;
  }
}
