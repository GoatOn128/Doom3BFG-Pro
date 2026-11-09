#pragma once

#define WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI
#define WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#include "engine/gamelib/physics/physics_dynamicbase.h"
#undef WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#undef WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI

#include <cstddef>
#include <cstdint>

struct contactsResult_t;

struct rigidBodyPState_t {
  rigidBodyPState_t() = default;
  rigidBodyPState_t& operator=(const rigidBodyPState_t&);

  int atRest;
  idVec3 localOrigin;
  idMat3 localAxis;
  idVec3 worldOrigin;
  idMat3 worldAxis;
  idVec6 spatialVelocity;
  idVec6 externalForce;
  idVec6 pushVelocity;
};

class idPhysics_RigidBody : public idPhysics_DynamicBase {
public:
  struct rigidBodyFlags_t {
    int noPushMask;
    std::uint8_t dropToFloor : 1;
    std::uint8_t testSolid : 1;
    std::uint8_t noImpact : 1;
    std::uint8_t hasMaster : 1;
    std::uint8_t isOrientated : 1;
    std::uint8_t reserved : 3;
    std::uint8_t padding[3];
  };

  idPhysics_RigidBody();
  ~idPhysics_RigidBody() override;

  void SetFriction(float linear, float angular, float contact);
  void SetWaterFriction(float linear, float angular);
  void SetBouncyness(float bouncyness_);
  void SetInertiaScale(const idVec3& scale);
  void DropToFloor(bool enable) {
    fl.dropToFloor = enable;
    fl.testSolid = enable;
  }
  void SetNoPushMask(int mask) { fl.noPushMask = mask; }
  void SetNoImpact(bool value) { fl.noImpact = value; }

  void SetClipModel(idClipModel* model, float density, int id,
                    bool freeOld) override;
  idClipModel* GetClipModel(int) const override { return clipModel; }
  int GetNumClipModels() const override { return 1; }
  void SetMass(float mass_, int id) override;
  float GetMass(int id) const override;
  void SetContents(int contents, int id) override;
  int GetContents(int id) const override;
  const idBounds& GetBounds(int id) const override;
  const idBounds& GetAbsBounds(int id) const override;
  void SetOrigin(const idVec3& newOrigin, int id) override;
  void SetAxis(const idMat3& newAxis, int id) override;
  void Translate(const idVec3& translation, int id) override;
  void Rotate(const idRotation& rotation, int id) override;
  const idVec3& GetOrigin(int id) const override;
  const idMat3& GetAxis(int id) const override;
  void SetLinearVelocity(const idVec3& velocity, int id) override;
  void SetAngularVelocity(const idVec3& velocity, int id) override;
  idVec3 GetLinearVelocity(int id) const override;
  idVec3 GetAngularVelocity(int) const override {
    return {current.spatialVelocity.p[3], current.spatialVelocity.p[4],
            current.spatialVelocity.p[5]};
  }
  void GetImpactInfo(int id, const idVec3& point,
                     impactInfo_t& info) const override;
  void ApplyImpulse(int id, const idVec3& point,
                    const idVec3& impulse) override;
  void ApplyForce(int id, const idVec3& point,
                  const idVec3& force) override;
  void Activate() override;
  void PutToRest() override;
  bool IsAtRest() const override;
  bool IsPushable(int sourceContentType) const override;
  void SaveState() override;
  void RestoreState() override;
  bool Evaluate(int timeStepMSec, int endTimeMSec) override;
  void ClipTranslation(trace_t& results, const idVec3& translation,
                       const idClipModel* model) const override;
  void ClipRotation(trace_t& results, const idRotation& rotation,
                    const idClipModel* model) const override;
  int ClipContents(const idClipModel* model, int clipMask_) const override;
  void DisableClip() override;
  void EnableClip() override {
    if (clipModel != nullptr) {
      clipModel->flags |= 1u;
    }
  }
  void UnlinkClip() override;
  void LinkClip() override;
  bool EvaluateContacts() override;
  bool IsGroundClipModel(int entityNum, int id) const override;
  void SetPushed(int deltaTime) override;
  idVec3 GetPushedLinearVelocity(int id) const override;
  idVec3 GetPushedAngularVelocity(int id) const override;
  void SetMaster(bool enable, const idVec3& masterOrigin,
                 const idMat3& masterAxis, bindFlags_t flags_) override;
  void SetLocalOrigin(const idVec3& newOrigin, int id) override;
  void SetLocalAxis(const idMat3& newAxis, int id) override;

  void Evolve(float timeStep, trace_t* collisionOverride,
              contactsResult_t* contactOverride);
  void ResolveCollisions(trace_t* collisionOverride,
                         contactsResult_t* contactOverride);
  void CapVelocity();
  bool TestIfAtRest();
  void DropToFloorAndRest();
  void DebugDraw();

public:
  idClipModel* clipModel;
  float linearFriction;
  float angularFriction;
  float contactFriction;
  float linearFrictionWater;
  float angularFrictionWater;
  float bouncyness;
  idVec3 inertiaScale;
  float mass;
  float inverseMass;
  idVec3 centerOfMass;
  idMat3 inertiaTensor;
  idMat3 inverseInertiaTensor;
  rigidBodyFlags_t fl;
  rigidBodyPState_t current;
  rigidBodyPState_t saved;
  idClipQuery motionQuery;
  float lastTimeStep;

  friend class PhysicsRigidBodyValidationAccess;
};

static_assert(sizeof(rigidBodyPState_t) == 0xAC,
              "rigid-body state ABI drift");
static_assert(sizeof(idPhysics_RigidBody::rigidBodyFlags_t) == 8,
              "rigid-body flags ABI drift");
static_assert(sizeof(idPhysics_RigidBody) == 0x290,
              "idPhysics_RigidBody ABI drift");
static_assert(offsetof(idPhysics_RigidBody, clipModel) == 0x98,
              "rigid-body clip-model offset drift");
static_assert(offsetof(idPhysics_RigidBody, fl) == 0x120,
              "rigid-body flags offset drift");
static_assert(offsetof(idPhysics_RigidBody, current) == 0x128,
              "rigid-body current-state offset drift");
static_assert(offsetof(idPhysics_RigidBody, saved) == 0x1D4,
              "rigid-body saved-state offset drift");
static_assert(offsetof(idPhysics_RigidBody, motionQuery) == 0x280,
              "rigid-body motion-query offset drift");
static_assert(offsetof(idPhysics_RigidBody, lastTimeStep) == 0x288,
              "rigid-body time-step offset drift");
