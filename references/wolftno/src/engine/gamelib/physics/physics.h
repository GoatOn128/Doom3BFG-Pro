#pragma once

#include "engine/gamelib/physics/clip.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>

#if defined(WOLFTNO_RETAIL_IDROTATION_CLASS_ABI)
class idRotation;
#else
struct idRotation;
#endif
struct impactInfo_t;
enum bindFlags_t : int;

enum physicsType_t : int {
  PHYSICS_INVALID = 0,
  PHYSICS_STATIC = 1,
  PHYSICS_STATICMULTI = 2,
  PHYSICS_AI = 3,
  PHYSICS_PLAYER = 4,
  PHYSICS_PARAMETRIC = 5,
  PHYSICS_PARAMETRICMM = 6,
  PHYSICS_RIGIDBODY = 7,
  PHYSICS_RIGIDBODY3DOF = 8,
  PHYSICS_AF = 9,
  PHYSICS_ROBOT = 10,
  PHYSICS_PARAMETRICDOOR = 11,
};

class idPhysicsCallbacks {
public:
  const void* const* vtable;

  template <typename Function> Function Slot(std::size_t index) const {
    return reinterpret_cast<Function>(vtable[index]);
  }

  const char* GetName() {
    return Slot<const char* (*)(idPhysicsCallbacks*)>(1)(this);
  }
  bool GetMasterTransform(idVec3& origin, idMat3& axis) {
    return Slot<bool (*)(idPhysicsCallbacks*, idVec3&, idMat3&)>(2)(
        this, origin, axis);
  }
  int Collide(int physicsId_, trace_t& collision,
              const idVec3& velocity) {
    return Slot<int (*)(idPhysicsCallbacks*, int, trace_t&, const idVec3&)>(3)(
        this, physicsId_, collision, velocity);
  }
  int Contact(int physicsId_, contactInfo_t& contact) {
    return Slot<int (*)(idPhysicsCallbacks*, int, contactInfo_t&)>(4)(
        this, physicsId_, contact);
  }
  void ApplyImpulse(int physicsId_, int bodyId, const idVec3& point,
                    const idVec3& impulse) {
    Slot<void (*)(idPhysicsCallbacks*, int, int, const idVec3&,
                  const idVec3&)>(5)(this, physicsId_, bodyId, point, impulse);
  }
  void ActivatePhysics(int physicsId_) {
    Slot<void (*)(idPhysicsCallbacks*, int)>(9)(this, physicsId_);
  }
  void DeactivatePhysics(int physicsId_) {
    Slot<void (*)(idPhysicsCallbacks*, int)>(10)(this, physicsId_);
  }
};

class idPhysics {
public:
  enum residency_t : int {
    RESIDENCY_NOT_TESTED = 0,
    RESIDENCY_YES = 1,
    RESIDENCY_NO = 2,
  };

  idPhysics();
  virtual ~idPhysics();

  static idPhysics* GetPhysicsForId(int physicsId_);
  static void ClearHash();

  void InitPhysics(idClip* clip_, idPhysicsCallbacks* callbacks_,
                   int entityNumber_);

  physicsType_t GetType() const { return type; }
  int GetPhysicsId() const { return physicsId; }
  int GetEntityNumber() const { return entityNumber; }
  idClip* GetClip() const { return clip; }
  idPhysicsCallbacks* GetCallbacks() const { return callbacks; }

  virtual void SetClipModel(idClipModel*, float, int, bool) = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI) ||                    \
    defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual idClipModel* GetClipModel(int) const = 0;
  virtual int GetNumClipModels() const = 0;
#else
  virtual idClipModel* GetClipModel(int) = 0;
  virtual int GetNumClipModels() = 0;
#endif
  virtual void SetMass(float, int) = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI) ||                    \
    defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual float GetMass(int) const = 0;
#else
  virtual float GetMass(int) = 0;
#endif
  virtual void SetContents(int, int) = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI) ||                    \
    defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual int GetContents(int) const = 0;
#else
  virtual int GetContents(int) = 0;
#endif
  virtual void SetClipMask(int, int) = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI) ||                    \
    defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual int GetClipMask(int) const = 0;
  virtual const idBounds& GetBounds(int) const = 0;
  virtual const idBounds& GetAbsBounds(int) const = 0;
#else
  virtual int GetClipMask(int) = 0;
  virtual const idBounds& GetBounds(int) = 0;
  virtual const idBounds& GetAbsBounds(int) = 0;
#endif
  virtual void SetOrigin(const idVec3&, int) = 0;
  virtual void SetAxis(const idMat3&, int) = 0;
  virtual void Translate(const idVec3&, int) = 0;
  virtual void Rotate(const idRotation&, int) = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI) ||                    \
    defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual const idVec3& GetOrigin(int) const = 0;
  virtual const idMat3& GetAxis(int) const = 0;
#else
  virtual const idVec3& GetOrigin(int) = 0;
  virtual const idMat3& GetAxis(int) = 0;
#endif
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual const idVec3& GetLocalOrigin(int) const = 0;
  virtual const idMat3& GetLocalAxis(int) const = 0;
#else
  virtual const idVec3& GetLocalOrigin(int) = 0;
  virtual const idMat3& GetLocalAxis(int) = 0;
#endif
  virtual void SetLinearVelocity(const idVec3&, int) = 0;
  virtual void SetAngularVelocity(const idVec3&, int) = 0;
  virtual idVec3 GetLinearVelocity(int) const = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual idVec3 GetAngularVelocity(int) const = 0;
#else
  virtual idVec3 GetAngularVelocity(int) = 0;
#endif
  virtual void SetGravity(const idVec3&) = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual const idVec3& GetGravity() const = 0;
  virtual const idVec3& GetGravityNormal() const = 0;
#else
  virtual const idVec3& GetGravity() = 0;
  virtual const idVec3& GetGravityNormal() = 0;
#endif
  virtual void SetWaterLevel(float, int, bool, int, float) = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual float GetWaterLevel(int) const = 0;
#else
  virtual float GetWaterLevel(int) = 0;
#endif
  virtual void SetWaterViscosity(float, int) = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual float GetWaterViscosity(int) const = 0;
  virtual int GetWaterEntNum() const = 0;
  virtual float GetWaterSurfaceWrldHeight() const = 0;
  virtual float GetBuoyancyScale() const = 0;
#else
  virtual float GetWaterViscosity(int) = 0;
  virtual int GetWaterEntNum() = 0;
  virtual float GetWaterSurfaceWrldHeight() = 0;
  virtual float GetBuoyancyScale() = 0;
#endif
  virtual void GetImpactInfo(int, const idVec3&, impactInfo_t&) const = 0;
  virtual void ApplyImpulse(int, const idVec3&, const idVec3&) = 0;
  virtual void ApplyForce(int, const idVec3&, const idVec3&) = 0;
  virtual void Activate() = 0;
  virtual void PutToRest() = 0;
  virtual bool IsAtRest() const = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI) ||                    \
    defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual bool IsPushable(int) const = 0;
#else
  virtual bool IsPushable(int) = 0;
#endif
  virtual void SaveState() = 0;
  virtual void RestoreState() = 0;
  virtual bool Evaluate(int, int) = 0;
  virtual void UpdateTime(int) = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_ACTOR_CONST_ABI) ||                    \
    defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual void ClipTranslation(trace_t&, const idVec3&,
                               const idClipModel*) const = 0;
  virtual void ClipRotation(trace_t&, const idRotation&,
                            const idClipModel*) const = 0;
  virtual int ClipContents(const idClipModel*, int) const = 0;
#else
  virtual void ClipTranslation(trace_t&, const idVec3&,
                               const idClipModel*) = 0;
  virtual void ClipRotation(trace_t&, const idRotation&,
                            const idClipModel*) = 0;
  virtual int ClipContents(const idClipModel*, int) = 0;
#endif
  virtual void DisableClip() = 0;
  virtual void EnableClip() = 0;
  virtual void UnlinkClip() = 0;
  virtual void LinkClip() = 0;
  virtual bool EvaluateContacts() = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual int GetNumContacts() const = 0;
  virtual const contactInfo_t& GetContact(int) const = 0;
#else
  virtual int GetNumContacts() = 0;
  virtual const contactInfo_t& GetContact(int) = 0;
#endif
  virtual void ClearContacts() = 0;
  virtual void AddContactPhysics(idPhysics*) = 0;
  virtual void RemoveContactPhysics(idPhysics*) = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual int GetNumContactPhysics() const = 0;
  virtual idPhysics* GetContactPhysics(int) const = 0;
#else
  virtual int GetNumContactPhysics() = 0;
  virtual idPhysics* GetContactPhysics(int) = 0;
#endif
  virtual void ActivateContactPhysics() = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual bool HasGroundContacts() const = 0;
  virtual bool IsGroundEntity(int) const = 0;
  virtual bool IsGroundClipModel(int, int) const = 0;
#else
  virtual bool HasGroundContacts() = 0;
  virtual bool IsGroundEntity(int) = 0;
  virtual bool IsGroundClipModel(int, int) = 0;
#endif
  virtual void SetPushed(int) = 0;
  virtual idVec3 GetPushedLinearVelocity(int) const = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual idVec3 GetPushedAngularVelocity(int) const = 0;
#else
  virtual idVec3 GetPushedAngularVelocity(int) = 0;
#endif
  virtual void SetMaster(bool, const idVec3&, const idMat3&,
                         bindFlags_t) = 0;
  virtual void SetLocalOrigin(const idVec3&, int) = 0;
  virtual void SetLocalAxis(const idMat3&, int) = 0;
#if defined(WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI)
  virtual int GetBlockingEntityNum() const = 0;
  virtual int GetLinearEndTime() const = 0;
  virtual int GetAngularEndTime() const = 0;
#else
  virtual int GetBlockingEntityNum() = 0;
  virtual int GetLinearEndTime() = 0;
  virtual int GetAngularEndTime() = 0;
#endif
  virtual bool IsInNonResidentCollisionArea(bool assumeNonResident);

  void SetType(physicsType_t physicsType) { type = physicsType; }

protected:
  void UpdateCollisionResidency(const contactInfo_t& contact);

public:
  physicsType_t type;
  int physicsId;
  int entityNumber;
  idPhysics* nextInHash;
  idClip* clip;
  idPhysicsCallbacks* callbacks;
  idResidencyQuery residencyQuery;
  residency_t collisionResidency;

  static
#if defined(WOLFTNO_RETAIL_IDPHYSICS_FORCEINLINE_DELETE_ABI)
      __forceinline
#endif
      void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }

private:
  static int currentPhysicsId;
  static idPhysics* hash[1024];

  void Link();

  friend class PhysicsValidationAccess;
};

static_assert(sizeof(idPhysics) == 0x40, "idPhysics ABI drift");
static_assert(offsetof(idPhysics, type) == 0x08,
              "idPhysics type offset drift");
static_assert(offsetof(idPhysics, physicsId) == 0x0C,
              "idPhysics id offset drift");
static_assert(offsetof(idPhysics, entityNumber) == 0x10,
              "idPhysics entity offset drift");
static_assert(offsetof(idPhysics, nextInHash) == 0x18,
              "idPhysics hash-link offset drift");
static_assert(offsetof(idPhysics, clip) == 0x20,
              "idPhysics clip offset drift");
static_assert(offsetof(idPhysics, callbacks) == 0x28,
              "idPhysics callback offset drift");
static_assert(offsetof(idPhysics, residencyQuery) == 0x30,
              "idPhysics residency-query offset drift");
static_assert(offsetof(idPhysics, collisionResidency) == 0x38,
              "idPhysics residency-state offset drift");
