#pragma once

#ifndef WOLFTNO_RETAIL_IDLIST_EXACT_SETNUM_ABI
#define WOLFTNO_RETAIL_IDLIST_EXACT_SETNUM_ABI
#define WOLFTNO_RETAIL_IDLIST_EXACT_SETNUM_ABI_STATICMULTI_LOCAL
#endif
#include "engine/gamelib/physics/physics_static.h"

#include <cstddef>
#include <cstring>

class idPhysics_StaticMulti : public idPhysics {
public:
  idPhysics_StaticMulti();
  ~idPhysics_StaticMulti() override;

  void InitPhysics(idClip* clip_, idPhysicsCallbacks* callbacks_,
                   int entityNumber_, bool enableBroadphase);
  void RemoveIndex(int id, bool freeClipModel);

  void SetClipModel(idClipModel*, float, int, bool) override;
  idClipModel* GetClipModel(int) const override;
  int GetNumClipModels() const override { return clipModels.num; }
  void SetMass(float, int) override {}
  float GetMass(int) const override;
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
  const idMat3& GetAxis(int) const override;
  const idVec3& GetLocalOrigin(int) const override;
  const idMat3& GetLocalAxis(int) const override;
  void SetLinearVelocity(const idVec3&, int) override {}
  void SetAngularVelocity(const idVec3&, int) override {}
  idVec3 GetLinearVelocity(int) const override { return vec3_origin; }
  idVec3 GetAngularVelocity(int) const override { return vec3_origin; }
  void SetGravity(const idVec3&) override {}
  const idVec3& GetGravity() const override { return vec3_down; }
  const idVec3& GetGravityNormal() const override { return vec3_down; }
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
  void AddContactPhysics(idPhysics*) override {}
  void RemoveContactPhysics(idPhysics*) override {}
  int GetNumContactPhysics() const override { return 0; }
  idPhysics* GetContactPhysics(int) const override { return nullptr; }
  void ActivateContactPhysics() override {}
  bool HasGroundContacts() const override { return false; }
  bool IsGroundEntity(int) const override { return false; }
  bool IsGroundClipModel(int, int) const override { return false; }
  void SetPushed(int) override {}
  idVec3 GetPushedLinearVelocity(int) const override { return vec3_origin; }
  idVec3 GetPushedAngularVelocity(int) const override { return vec3_origin; }
  void SetMaster(bool, const idVec3&, const idMat3&, bindFlags_t) override;
  void SetLocalOrigin(const idVec3&, int) override;
  void SetLocalAxis(const idMat3&, int) override;
  int GetBlockingEntityNum() const override { return 0x1FFF; }
  int GetLinearEndTime() const override { return 0; }
  int GetAngularEndTime() const override { return 0; }

protected:
  void Link(int id, int entityNumber_, int physicsId_, int bodyId,
            const idVec3& origin, const idMat3& axis);

public:
  idClipModel* parent;
  idList<idClipModel*, 77> clipModels;
  idList<float, 77> mass;
  idBounds bounds;
  mutable idBounds absBounds;
  bool hasMaster;
  bool isOrientated;
  idList<staticPState_t, 77> current;

  friend class PhysicsStaticMultiValidationAccess;
};

static_assert(sizeof(idPhysics_StaticMulti) == 0xC8,
              "idPhysics_StaticMulti ABI drift");
static_assert(offsetof(idPhysics_StaticMulti, parent) == 0x40,
              "static-multi parent offset drift");
static_assert(offsetof(idPhysics_StaticMulti, clipModels) == 0x48,
              "static-multi clip-list offset drift");
static_assert(offsetof(idPhysics_StaticMulti, mass) == 0x60,
              "static-multi mass-list offset drift");
static_assert(offsetof(idPhysics_StaticMulti, bounds) == 0x78,
              "static-multi bounds offset drift");
static_assert(offsetof(idPhysics_StaticMulti, absBounds) == 0x90,
              "static-multi absolute-bounds offset drift");
static_assert(offsetof(idPhysics_StaticMulti, hasMaster) == 0xA8,
              "static-multi master flag offset drift");
static_assert(offsetof(idPhysics_StaticMulti, isOrientated) == 0xA9,
              "static-multi orientation flag offset drift");
static_assert(offsetof(idPhysics_StaticMulti, current) == 0xB0,
              "static-multi state-list offset drift");

#if defined(WOLFTNO_RETAIL_IDLIST_EXACT_SETNUM_ABI_STATICMULTI_LOCAL)
#undef WOLFTNO_RETAIL_IDLIST_EXACT_SETNUM_ABI_STATICMULTI_LOCAL
#undef WOLFTNO_RETAIL_IDLIST_EXACT_SETNUM_ABI
#endif
