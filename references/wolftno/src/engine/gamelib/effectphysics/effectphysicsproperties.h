#pragma once

#include "engine/gamelib/effectphysics/effectphysicsbroadphase.h"

#include <cstddef>

class idMaterial;
class idTraceModel;

class idTraceModelCache {
public:
  int AllocTraceModel(const idTraceModel& traceModel,
                      const idMaterial* material);
  void FreeTraceModel(int traceModelIndex);
  void GetMassProperties(int traceModelIndex, float density, float& mass,
                         idVec3& centerOfMass,
                         idMat3& inertiaTensor) const;
};

class idEffectPhysicsProperties {
public:
  idEffectPhysicsProperties();
  ~idEffectPhysicsProperties() { broadPhase = nullptr; }

  void Setup(idEffectPhysicsBroadPhase* broadPhase, int traceModelIndex,
             float mass);

  idEffectPhysicsBroadPhase* broadPhase;
  int traceModelIndex;
  int clipMask;
  float linearFriction;
  float angularFriction;
  float contactFriction;
  float linearFrictionWater;
  float angularFrictionWater;
  float bouncyness;
  idVec3 gravityVector;
  bool worldCollisionOnly;
  bool simplePointCollision;
  unsigned char padding[2];
  float crazyBounceChance;
  float mass;
  float inverseMass;
  idVec3 centerOfMass;
  idMat3 inertiaTensor;
  idMat3 inverseInertiaTensor;
  float stopSpeed;
  float maxLinearVelocity;
  float maxAngularVelocity;
  int maxSimulationTime;
};

static_assert(offsetof(idEffectPhysicsBroadPhase, traceModelCache) == 8,
              "idEffectPhysicsBroadPhase ABI drift");
static_assert(offsetof(idEffectPhysicsProperties, traceModelIndex) == 0x08,
              "idEffectPhysicsProperties ABI drift");
static_assert(offsetof(idEffectPhysicsProperties, gravityVector) == 0x28,
              "idEffectPhysicsProperties ABI drift");
static_assert(offsetof(idEffectPhysicsProperties, inertiaTensor) == 0x50,
              "idEffectPhysicsProperties ABI drift");
static_assert(offsetof(idEffectPhysicsProperties, inverseInertiaTensor) == 0x74,
              "idEffectPhysicsProperties ABI drift");
static_assert(offsetof(idEffectPhysicsProperties, maxSimulationTime) == 0xA4,
              "idEffectPhysicsProperties ABI drift");
static_assert(sizeof(idEffectPhysicsProperties) == 0xA8,
              "idEffectPhysicsProperties ABI drift");
