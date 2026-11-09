#pragma once

#define WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI
#define WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#include "engine/gamelib/physics/physics_dynamicbase.h"
#undef WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#undef WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI

#include "shared/idlib/math/random.h"

#include <cstddef>
#include <cstdint>

class idPhysics_RigidBody3DOF : public idPhysics_DynamicBase {
public:
  struct bodyIntState_t {
    bodyIntState_t() = default;

    idVec3 position;
    idMat3 orientation;
    idVec3 linearVelocity;
    idVec3 angularVelocity;
  };

  struct bodyState_t {
    bodyState_t();
    bodyState_t& operator=(const bodyState_t&) = default;

    int atRest;
    float lastTimeStep;
    idVec3 localOrigin;
    idMat3 localAxis;
    idVec3 linearPushVelocity;
    idVec3 angularPushVelocity;
    idVec3 externalForce;
    bodyIntState_t i;
  };

  struct simpleBodyFlags_t {
    std::uint8_t dropToFloor : 1;
    std::uint8_t testSolid : 1;
    std::uint8_t hasMaster : 1;
    std::uint8_t isOriented : 1;
    std::uint8_t noImpact : 1;
    std::uint8_t reserved : 3;
  };

  idPhysics_RigidBody3DOF();
  ~idPhysics_RigidBody3DOF() override;

  void SetFriction(float linear, float angular, float contact);
  void SetWaterFriction(float linear, float angular);
  void SetCoefficientOfRestitution(float value);
  void SetBouncyness(float value) { SetCoefficientOfRestitution(value); }
  void SetNoImpact(bool value) { flags.noImpact = value; }

  void SetClipModel(idClipModel* model, float density, int id,
                    bool freeOld) override;
  idClipModel* GetClipModel(int) const override { return clipModel; }
  int GetNumClipModels() const override { return 1; }
  void SetMass(float value, int id) override;
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
  idVec3 GetAngularVelocity(int id) const override;
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
  void EnableClip() override;
  void UnlinkClip() override;
  void LinkClip() override;
  void SetMaster(bool enable, const idVec3& masterOrigin,
                 const idMat3& masterAxis, bindFlags_t flags_) override;
  void SetLocalOrigin(const idVec3& newOrigin, int id) override;
  void SetLocalAxis(const idMat3& newAxis, int id) override;
  void SetPushed(int deltaTime) override;
  idVec3 GetPushedLinearVelocity(int id) const override;
  idVec3 GetPushedAngularVelocity(int id) const override;

  void Integrate(float deltaTime, const bodyState_t& cur,
                 bodyState_t& next);
  bool CheckForCollisions(float deltaTime, const bodyState_t& cur,
                          bodyState_t& next, trace_t& collision);
  bool CollisionImpulse(trace_t& collision, float timeStep,
                        bodyState_t& postImpactState, idVec3& impulse);

public:
  idClipModel* clipModel;
  float mass;
  float inverseMass;
  float linearFriction;
  float angularFriction;
  float contactFriction;
  float linearFrictionWater;
  float angularFrictionWater;
  float coefficientOfRestitution;
  bodyState_t current;
  bodyState_t saved;
  idRandom random;
  idClipQuery translationQuery;
  simpleBodyFlags_t flags;
};

static_assert(sizeof(idPhysics_RigidBody3DOF::bodyIntState_t) == 0x48,
              "3DOF internal-state ABI drift");
static_assert(sizeof(idPhysics_RigidBody3DOF::bodyState_t) == 0xA4,
              "3DOF body-state ABI drift");
static_assert(sizeof(idPhysics_RigidBody3DOF::simpleBodyFlags_t) == 1,
              "3DOF flags ABI drift");
static_assert(sizeof(idPhysics_RigidBody3DOF) == 0x220,
              "idPhysics_RigidBody3DOF ABI drift");
static_assert(offsetof(idPhysics_RigidBody3DOF, clipModel) == 0x98,
              "3DOF clip-model offset drift");
static_assert(offsetof(idPhysics_RigidBody3DOF, current) == 0xC0,
              "3DOF current-state offset drift");
static_assert(offsetof(idPhysics_RigidBody3DOF, saved) == 0x164,
              "3DOF saved-state offset drift");
static_assert(offsetof(idPhysics_RigidBody3DOF, random) == 0x208,
              "3DOF random offset drift");
static_assert(offsetof(idPhysics_RigidBody3DOF, translationQuery) == 0x210,
              "3DOF query offset drift");
static_assert(offsetof(idPhysics_RigidBody3DOF, flags) == 0x218,
              "3DOF flags offset drift");
