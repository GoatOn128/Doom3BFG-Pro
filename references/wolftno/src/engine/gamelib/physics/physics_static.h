#pragma once

#ifndef WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#define WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#define WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI_LOCAL
#endif
#include "engine/gamelib/physics/physics.h"

#include <cstddef>

struct staticPState_t {
  idVec3 worldOrigin;
  idMat3 worldAxis;
  idVec3 localOrigin;
  idMat3 localAxis;
};

class idPhysics_Static : public idPhysics {
public:
  idPhysics_Static();
  ~idPhysics_Static() override;

  void SetClipModel(idClipModel*, float, int, bool) override;
  idClipModel* GetClipModel(int) const override;
  int GetNumClipModels() const override;
  void SetMass(float, int) override {}
  float GetMass(int) const override { return 0.0f; }
  void SetContents(int, int) override;
  int GetContents(int) const override;
  void SetClipMask(int, int) override {}
  int GetClipMask(int) const override { return 0; }
  const idBounds& GetBounds(int) const override;
  const idBounds& GetAbsBounds(int) const override;
  void SetOrigin(const idVec3&, int) override;
  void SetAxis(const idMat3&, int) override;
  void Translate(const idVec3&, int) override;
  void Rotate(const idRotation&, int) override;
  const idVec3& GetOrigin(int) const override;
  const idMat3& GetAxis(int) const override { return current.worldAxis; }
  const idVec3& GetLocalOrigin(int) const override;
  const idMat3& GetLocalAxis(int) const override { return current.localAxis; }
  void SetLinearVelocity(const idVec3&, int) override {}
  void SetAngularVelocity(const idVec3&, int) override {}
  idVec3 GetLinearVelocity(int) const override { return vec3_origin; }
  idVec3 GetAngularVelocity(int) const override { return vec3_origin; }
  void SetGravity(const idVec3&) override {}
  const idVec3& GetGravity() const override;
  const idVec3& GetGravityNormal() const override { return GetGravity(); }
  void SetWaterLevel(float, int, bool, int, float) override {}
  float GetWaterLevel(int) const override { return 0.0f; }
  void SetWaterViscosity(float, int) override {}
  float GetWaterViscosity(int) const override { return 0.0f; }
  int GetWaterEntNum() const override { return 0; }
  float GetWaterSurfaceWrldHeight() const override { return 0.0f; }
  float GetBuoyancyScale() const override { return 0.0f; }
  void GetImpactInfo(int, const idVec3&, impactInfo_t&) const override;
  void ApplyImpulse(int, const idVec3&, const idVec3&) override {}
  void ApplyForce(int, const idVec3&, const idVec3&) override {}
  void Activate() override {}
  void PutToRest() override {}
  bool IsAtRest() const override { return true; }
  bool IsPushable(int) const override { return false; }
  void SaveState() override {}
  void RestoreState() override {}
  bool Evaluate(int, int) override;
  void UpdateTime(int) override {}
  void ClipTranslation(trace_t&, const idVec3&,
                       const idClipModel*) const override;
  void ClipRotation(trace_t&, const idRotation&,
                    const idClipModel*) const override;
  int ClipContents(const idClipModel*, int) const override;
  void DisableClip() override;
  void EnableClip() override;
  void UnlinkClip() override;
  void LinkClip() override;
  bool EvaluateContacts() override { return false; }
  int GetNumContacts() const override { return 0; }
  const contactInfo_t& GetContact(int) const override;
  void ClearContacts() override {}
  void AddContactPhysics(idPhysics*) override;
  void RemoveContactPhysics(idPhysics*) override;
  int GetNumContactPhysics() const override;
  idPhysics* GetContactPhysics(int) const override;
  void ActivateContactPhysics() override;
  bool HasGroundContacts() const override { return false; }
  bool IsGroundEntity(int) const override { return false; }
  bool IsGroundClipModel(int, int) const override { return false; }
  void SetPushed(int) override {}
  idVec3 GetPushedLinearVelocity(int) const override { return vec3_origin; }
  idVec3 GetPushedAngularVelocity(int) const override { return vec3_origin; }
  void SetMaster(bool, const idVec3&, const idMat3&, bindFlags_t) override;
  void SetLocalOrigin(const idVec3&, int) override;
  void SetLocalAxis(const idMat3&, int) override;
  int GetBlockingEntityNum() const override;
  int GetLinearEndTime() const override { return 0; }
  int GetAngularEndTime() const override { return 0; }

protected:
  idClipModel* clipModel;
  idList<int, 77> contactPhysicsIds;
  bool hasMaster : 1;
  bool isOrientated : 1;
  staticPState_t current;

  friend class PhysicsStaticValidationAccess;
};

static_assert(sizeof(staticPState_t) == 0x60,
              "staticPState_t ABI drift");
static_assert(offsetof(staticPState_t, worldOrigin) == 0x00,
              "static state world-origin offset drift");
static_assert(offsetof(staticPState_t, worldAxis) == 0x0C,
              "static state world-axis offset drift");
static_assert(offsetof(staticPState_t, localOrigin) == 0x30,
              "static state local-origin offset drift");
static_assert(offsetof(staticPState_t, localAxis) == 0x3C,
              "static state local-axis offset drift");
static_assert(sizeof(idPhysics_Static) == 0xC8,
              "idPhysics_Static ABI drift");

#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI_LOCAL)
#undef WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI_LOCAL
#undef WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#endif
