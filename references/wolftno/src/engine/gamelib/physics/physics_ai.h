#pragma once

#include "engine/gamelib/physics/physics_actor.h"

#include <cstddef>
#include <cstdint>

struct aiPState_t {
  std::int16_t atRest;
  bool onGround;
  std::uint8_t pad;
  idVec3 localOrigin;
  idVec3 worldOrigin;
  idVec3 velocity;
  idVec3 pushVelocity;
};

class idPhysics_AI : public idPhysics_Actor {
public:
  enum aiMovement_t : int {
    AI_MOVE_WALKING = 0,
    AI_MOVE_FLYING = 1,
    AI_MOVE_DROPPING = 2,
  };

  idPhysics_AI();
  ~idPhysics_AI() override;

  void SetClipModel(idClipModel* model, float density, int id,
                    bool freeOld) override;
  void SetOrigin(const idVec3& newOrigin, int id) override;
  void SetAxis(const idMat3& newAxis, int id) override;
  void Translate(const idVec3& translation, int id) override;
  void Rotate(const idRotation& rotation, int id) override;
  void SetLinearVelocity(const idVec3& newLinearVelocity, int id) override;
  idVec3 GetLinearVelocity(int id) const override;
  void GetImpactInfo(int id, const idVec3& point,
                     impactInfo_t& info) const override;
  void ApplyImpulse(int bodyId, const idVec3& point,
                    const idVec3& impulse) override;
  void Activate() override;
  void PutToRest() override;
  bool IsAtRest() const override;
  void SaveState() override;
  void RestoreState() override;
  bool Evaluate(int timeStepMSec, int endTimeMSec) override;
  void SetPushed(int deltaTime) override;
  idVec3 GetPushedLinearVelocity(int id) const override;
  void SetContents(int contents, int id) override;
  void SetMaster(bool enable, const idVec3& masterOrigin,
                 const idMat3& masterAxis, bindFlags_t flags) override;
  void SetLocalOrigin(const idVec3& newOrigin, int id) override;
  void SetLocalAxis(const idMat3& newAxis, int id) override;

  void SetMaxStepHeight(float value) { maxStepHeight = value; }
  float GetMaxStepHeight() const { return maxStepHeight; }
  void SetMinFloorCosine(float value) { minFloorCosine = value; }
  bool OnGround() const { return current.onGround; }
  void ClearOnGround() { current.onGround = false; }
  void SetMovementType(aiMovement_t value) { movementType = value; }
  void SetNoImpact(bool value) { noImpact = value; }
  void SetMaxDropVelocity(float value) { maxDropVelocity = value; }
  float GetMaxDropVelocity() const { return maxDropVelocity; }
  void SetDelta(const idVec3& delta, int timeMSec) {
    current.velocity = delta * (1000.0f / static_cast<float>(timeMSec));
  }
  idVec3 GetDelta(int timeMSec) const {
    return current.velocity * (static_cast<float>(timeMSec) * 0.001f);
  }
  bool GetMasterControlledVelocity() const {
    return masterControlledVelocity;
  }
  void SetMasterControlledVelocity(bool value) {
    masterControlledVelocity = value;
  }
  void SetAsSwimmer(bool value) { swimmer = value; }
  bool IsSwimmer() const { return swimmer; }
  bool HasFullContact() const { return fullContact; }
  void SetCrouched(bool value) { crouched = value; }
  bool IsCrouched() const { return crouched; }
  void SetCanCrouch(bool value) { canSetCrouchedPhysics = value; }
  void SetCrouchHeight(float value) { heightCrouched = value; }
  void ResetLocalOrigin(int) { current.localOrigin = vec3_origin; }
  const idVec3& GetStuckNormal() const { return stuckCollisionNormal; }
  const idVec3& GetStuckPoint() const { return stuckCollisionPoint; }

private:
  void Evolve(float timeStep);
  void ResolveCollisions();

public:
  aiPState_t current;
  aiPState_t saved;
  idClipQuery stepMoveQuery;
  idClipQuery contactsQuery;
  idVec3 lastCollisionNormal;
  idVec3 lastCollisionPoint;
  idVec3 stuckCollisionNormal;
  idVec3 stuckCollisionPoint;
  idClipModel* clipModel_standing;
  idClipModel* clipModel_crouched;
  float maxStepHeight;
  float minFloorCosine;
  float maxDropVelocity;
  float heightCrouched;
  aiMovement_t movementType;
  bool noImpact;
  bool masterControlledVelocity;
  bool swimmer;
  bool crouched;
  bool canSetCrouchedPhysics;
  bool fullContact;
  std::uint8_t recoveredTail[2];

  friend class PhysicsAIValidationAccess;
};

static_assert(sizeof(aiPState_t) == 0x34, "aiPState_t ABI drift");
static_assert(offsetof(aiPState_t, localOrigin) == 0x04,
              "AI state local-origin offset drift");
static_assert(offsetof(aiPState_t, velocity) == 0x1C,
              "AI state velocity offset drift");
static_assert(sizeof(idPhysics_AI) == 0x2C8, "idPhysics_AI ABI drift");
static_assert(offsetof(idPhysics_AI, current) == 0x1F0,
              "idPhysics_AI current-state offset drift");
static_assert(offsetof(idPhysics_AI, saved) == 0x224,
              "idPhysics_AI saved-state offset drift");
static_assert(offsetof(idPhysics_AI, stepMoveQuery) == 0x258,
              "idPhysics_AI step-query offset drift");
static_assert(offsetof(idPhysics_AI, lastCollisionNormal) == 0x268,
              "idPhysics_AI collision-normal offset drift");
static_assert(offsetof(idPhysics_AI, clipModel_standing) == 0x298,
              "idPhysics_AI standing-model offset drift");
static_assert(offsetof(idPhysics_AI, maxStepHeight) == 0x2A8,
              "idPhysics_AI step-height offset drift");
static_assert(offsetof(idPhysics_AI, movementType) == 0x2B8,
              "idPhysics_AI movement-type offset drift");
static_assert(offsetof(idPhysics_AI, noImpact) == 0x2BC,
              "idPhysics_AI flags offset drift");
