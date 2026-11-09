#pragma once

#include "engine/gamelib/physics/physics.h"

class idPhysics_DynamicBase : public idPhysics {
public:
  idPhysics_DynamicBase();
  ~idPhysics_DynamicBase() override;

  void SetClipModel(idClipModel*, float, int, bool) override;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI)
  idClipModel* GetClipModel(int) const override;
  int GetNumClipModels() const override;
#else
  idClipModel* GetClipModel(int) override;
  int GetNumClipModels() override;
#endif
  void SetMass(float, int) override;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI)
  float GetMass(int) const override;
#else
  float GetMass(int) override;
#endif
  void SetContents(int, int) override;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI)
  int GetContents(int) const override;
#else
  int GetContents(int) override;
#endif
  void SetClipMask(int, int) override;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI)
  int GetClipMask(int) const override;
  const idBounds& GetBounds(int) const override;
  const idBounds& GetAbsBounds(int) const override;
#else
  int GetClipMask(int) override;
  const idBounds& GetBounds(int) override;
  const idBounds& GetAbsBounds(int) override;
#endif
  void SetOrigin(const idVec3&, int) override;
  void SetAxis(const idMat3&, int) override;
  void Translate(const idVec3&, int) override;
  void Rotate(const idRotation&, int) override;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI)
  const idVec3& GetOrigin(int) const override;
  const idMat3& GetAxis(int) const override;
#else
  const idVec3& GetOrigin(int) override;
  const idMat3& GetAxis(int) override;
#endif
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  const idVec3& GetLocalOrigin(int) const override;
  const idMat3& GetLocalAxis(int) const override;
#else
  const idVec3& GetLocalOrigin(int) override;
  const idMat3& GetLocalAxis(int) override;
#endif
  void SetLinearVelocity(const idVec3&, int) override;
  void SetAngularVelocity(const idVec3&, int) override;
  idVec3 GetLinearVelocity(int) const override;
  #if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  idVec3 GetAngularVelocity(int) const override;
#else
  idVec3 GetAngularVelocity(int) override;
#endif
  void SetGravity(const idVec3&) override;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  const idVec3& GetGravity() const override;
  const idVec3& GetGravityNormal() const override;
#else
  const idVec3& GetGravity() override;
  const idVec3& GetGravityNormal() override;
#endif
  void SetWaterLevel(float, int, bool, int, float) override;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  float GetWaterLevel(int) const override;
#else
  float GetWaterLevel(int) override;
#endif
  void SetWaterViscosity(float, int) override;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  float GetWaterViscosity(int) const override;
  int GetWaterEntNum() const override;
  float GetWaterSurfaceWrldHeight() const override;
  float GetBuoyancyScale() const override;
#else
  float GetWaterViscosity(int) override;
  int GetWaterEntNum() override;
  float GetWaterSurfaceWrldHeight() override;
  float GetBuoyancyScale() override;
#endif
  void GetImpactInfo(int, const idVec3&, impactInfo_t&) const override;
  void ApplyImpulse(int, const idVec3&, const idVec3&) override;
  void ApplyForce(int, const idVec3&, const idVec3&) override;
  void Activate() override;
  void PutToRest() override;
  bool IsAtRest() const override;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI)
  bool IsPushable(int) const override;
#else
  bool IsPushable(int) override;
#endif
  void SaveState() override;
  void RestoreState() override;
  bool Evaluate(int, int) override;
  void UpdateTime(int) override;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI)
  void ClipTranslation(trace_t&, const idVec3&,
                       const idClipModel*) const override;
  void ClipRotation(trace_t&, const idRotation&,
                    const idClipModel*) const override;
  int ClipContents(const idClipModel*, int) const override;
#else
  void ClipTranslation(trace_t&, const idVec3&, const idClipModel*) override;
  void ClipRotation(trace_t&, const idRotation&, const idClipModel*) override;
  int ClipContents(const idClipModel*, int) override;
#endif
  void DisableClip() override;
  void EnableClip() override;
  void UnlinkClip() override;
  void LinkClip() override;
  bool EvaluateContacts() override;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  int GetNumContacts() const override;
  const contactInfo_t& GetContact(int) const override;
#else
  int GetNumContacts() override;
  const contactInfo_t& GetContact(int) override;
#endif
  void ClearContacts() override;
  void AddContactPhysics(idPhysics*) override;
  void RemoveContactPhysics(idPhysics*) override;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  int GetNumContactPhysics() const override;
  idPhysics* GetContactPhysics(int) const override;
#else
  int GetNumContactPhysics() override;
  idPhysics* GetContactPhysics(int) override;
#endif
  void ActivateContactPhysics() override;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  bool HasGroundContacts() const override;
  bool IsGroundEntity(int) const override;
  bool IsGroundClipModel(int, int) const override;
#else
  bool HasGroundContacts() override;
  bool IsGroundEntity(int) override;
  bool IsGroundClipModel(int, int) override;
#endif
  void SetPushed(int) override;
  idVec3 GetPushedLinearVelocity(int) const override;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  idVec3 GetPushedAngularVelocity(int) const override;
#else
  idVec3 GetPushedAngularVelocity(int) override;
#endif
  void SetMaster(bool, const idVec3&, const idMat3&, bindFlags_t) override;
  void SetLocalOrigin(const idVec3&, int) override;
  void SetLocalAxis(const idMat3&, int) override;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  int GetBlockingEntityNum() const override;
  int GetLinearEndTime() const override;
  int GetAngularEndTime() const override;
#else
  int GetBlockingEntityNum() override;
  int GetLinearEndTime() override;
  int GetAngularEndTime() override;
#endif
  virtual bool IsOutsideWorld() const;
  bool IsGroundNormal(const idVec3& normal) const;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_GRAVITY_SLOT_ABI)
  virtual const idMat3& GetGravityAxis() const = 0;
#endif

protected:
  void AddGroundContacts(const idClipModel* model, int mask);
  void AddContactPhysicsForContacts();
  void DrawVelocity(int id, float linearScale, float angularScale) const;

  int clipMask;
  idVec3 gravityVector;
  idVec3 gravityNormal;
  idList<contactInfo_t, 77> contacts;
  idList<int, 77> contactPhysicsIds;
  float waterLevel;
  float waterViscosity;

  friend class PhysicsDynamicBaseValidationAccess;
  friend struct idPhysicsDynamicBaseABI;
};

struct idPhysicsDynamicBaseABI {
  static_assert(offsetof(idPhysics_DynamicBase, clipMask) == 0x40,
                "dynamic physics clip-mask offset drift");
  static_assert(offsetof(idPhysics_DynamicBase, gravityVector) == 0x44,
                "dynamic physics gravity-vector offset drift");
  static_assert(offsetof(idPhysics_DynamicBase, gravityNormal) == 0x50,
                "dynamic physics gravity-normal offset drift");
  static_assert(offsetof(idPhysics_DynamicBase, contacts) == 0x60,
                "dynamic physics contacts offset drift");
  static_assert(offsetof(idPhysics_DynamicBase, contactPhysicsIds) == 0x78,
                "dynamic physics contact-physics offset drift");
  static_assert(offsetof(idPhysics_DynamicBase, waterLevel) == 0x90,
                "dynamic physics water-level offset drift");
  static_assert(offsetof(idPhysics_DynamicBase, waterViscosity) == 0x94,
                "dynamic physics water-viscosity offset drift");
};

static_assert(sizeof(idPhysics_DynamicBase) == 0x98,
              "idPhysics_DynamicBase ABI drift");
