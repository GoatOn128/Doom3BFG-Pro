#pragma once

#include "engine/gamelib/physics/clip.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/spatialmat.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/atomicstring.h"

#include <cstddef>
#include <cstdint>

class idAFConstraint;
class idAFTree;
class idCVar;
class idSerializer;

struct AFBodyPState_t {
  idVec3 worldOrigin;
  idMat3 worldAxis;
  idVec3 atRestOrigin;
  idMat3 atRestAxis;
  idVec6 spatialVelocity;
  idVec6 externalForce;

  AFBodyPState_t& operator=(const AFBodyPState_t& other);
};

class alignas(16) idAFBody {
public:
  idAFBody();
  idAFBody(const char* bodyName, idClipModel* bodyClipModel,
           float density);
  virtual ~idAFBody();

  static void operator delete(void* pointer) noexcept {
    if (pointer == nullptr) {
      return;
    }
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(pointer, ALIGN_16);
#else
    idMem::Singleton()->Free(pointer, ALIGN_16);
#endif
  }

  void Init();
  const char* GetName() const { return name.str; }
  void SetName(const char* bodyName) { name.Set(bodyName); }
  void SetClipModel(idClipModel* bodyClipModel);
  idClipModel* GetClipModel() const { return clipModel; }
  void SetClipMask(const int mask) {
    clipMask = mask;
    fl |= 0x01u;
  }
  int GetClipMask() const { return clipMask; }
  void SetSelfCollision(const bool enable) {
    fl = static_cast<std::uint8_t>((fl & ~0x02u) |
                                   (enable ? 0x02u : 0u));
  }

  void SetFriction(float linear, float angular, float contact);
  float GetContactFriction() { return contactFriction; }
  float GetContactFriction() const { return contactFriction; }
  void SetBouncyness(float bounce);
  float GetBouncyness() const { return bouncyness; }
  void SetDensity(float density,
                  const idMat3& inertiaScale = mat3_identity);
  float GetInverseMass() const { return invMass; }
  const idMat3 GetInverseWorldInertia() const {
    idMat3 transpose;
    transpose[0] = {current.worldAxis[0].x, current.worldAxis[1].x,
                    current.worldAxis[2].x};
    transpose[1] = {current.worldAxis[0].y, current.worldAxis[1].y,
                    current.worldAxis[2].y};
    transpose[2] = {current.worldAxis[0].z, current.worldAxis[1].z,
                    current.worldAxis[2].z};
    return transpose * inverseInertiaTensor * current.worldAxis;
  }

  void SetFrictionDirection(const idVec3& direction);
  bool GetFrictionDirection(idVec3& direction) const;
  void SetContactMotorDirection(const idVec3& direction);
  bool GetContactMotorDirection(idVec3& direction) const;
  void SetContactMotorVelocity(float velocity) {
    contactMotorVelocity = velocity;
  }
  float GetContactMotorVelocity() const { return contactMotorVelocity; }
  void SetContactMotorForce(float force) { contactMotorForce = force; }
  float GetContactMotorForce() const { return contactMotorForce; }

  void SetWorldPosition(const idVec3& origin, const idMat3& axis) {
    current.worldOrigin = origin;
    current.worldAxis = axis;
  }
  void SetWorldOrigin(const idVec3& origin) { current.worldOrigin = origin; }
  void SetWorldAxis(const idMat3& axis) { current.worldAxis = axis; }
  const idVec3& GetWorldOrigin() const { return current.worldOrigin; }
  const idMat3& GetWorldAxis() const { return current.worldAxis; }
  void SetLinearVelocity(const idVec3& velocity) {
    *reinterpret_cast<idVec3*>(&current.spatialVelocity.p[0]) = velocity;
  }
  void SetAngularVelocity(const idVec3& velocity) {
    *reinterpret_cast<idVec3*>(&current.spatialVelocity.p[3]) = velocity;
  }
  const idVec3& GetLinearVelocity() const {
    return *reinterpret_cast<const idVec3*>(&current.spatialVelocity.p[0]);
  }
  const idVec3& GetAngularVelocity() const {
    return *reinterpret_cast<const idVec3*>(&current.spatialVelocity.p[3]);
  }
  const idVec3 GetPointVelocity(const idVec3& point) const;

  void AddForce(const idVec3& point, const idVec3& force);
  const idVec3& GetLinearForce() const {
    return *reinterpret_cast<const idVec3*>(&current.externalForce.p[0]);
  }
  const idVec3& GetAngularForce() const {
    return *reinterpret_cast<const idVec3*>(&current.externalForce.p[3]);
  }
  void AddLinearForce(const idVec3& force) {
    current.externalForce.p[0] += force.x;
    current.externalForce.p[1] += force.y;
    current.externalForce.p[2] += force.z;
  }
  void AddAngularForce(const idVec3& force) {
    current.externalForce.p[3] += force.x;
    current.externalForce.p[4] += force.y;
    current.externalForce.p[5] += force.z;
  }

  void InverseWorldSpatialInertiaMultiply(idSpatialVec& destination,
                                           const idSpatialVec& vector) const;

  float GetLinearFriction() const { return linearFriction; }
  float GetAngularFriction() const { return angularFriction; }
  float GetMass() const { return mass; }
  const idMat3& GetInertiaTensor() const { return inertiaTensor; }
  void SetLinearFriction(float value);
  void SetAngularFriction(float value);
  void SetContactFriction(float value) { contactFriction = value; }
  void SetMass(float value) {
    mass = value;
    invMass = 1.0f / value;
  }
  void SetNoSyncCollide(const bool enable) {
    fl = static_cast<std::uint8_t>((fl & ~0x40u) |
                                   (enable ? 0x40u : 0u));
  }
  bool GetNoSyncCollide() const { return (fl & 0x40u) != 0u; }
  void SetClearClipMaskInSolid(const bool enable) {
    fl = static_cast<std::uint8_t>((fl & ~0x80u) |
                                   (enable ? 0x80u : 0u));
  }
  bool GetClearClipMaskInSolid() const { return (fl & 0x80u) != 0u; }
  idClipQuery GetMotionQuery() const { return motionQuery; }
  virtual void Serialize(idSerializer& serializer);

  idAFBody* GetParent() { return parent; }
  const idAFBody* GetParent() const { return parent; }
  idAFConstraint* GetPrimaryConstraint() { return primaryConstraint; }
  const idAFConstraint* GetPrimaryConstraint() const {
    return primaryConstraint;
  }

  idAtomicString name;
  idClipModel* clipModel;
  int clipMask;
  float linearFrictionWater;
  float angularFrictionWater;
  float linearFriction;
  float angularFriction;
  float contactFriction;
  float bouncyness;
  idVec3 frictionDir;
  idVec3 contactMotorDir;
  float contactMotorVelocity;
  float contactMotorForce;
  float mass;
  float invMass;
  idVec3 centerOfMass;
  idMat3 inertiaTensor;
  idMat3 inverseInertiaTensor;
  idAFBody* parent;
  idList<idAFBody*, 71> children;
  idAFConstraint* primaryConstraint;
  idList<idAFConstraint*, 71> constraints;
  idAFTree* tree;
  std::uint8_t fl;
  std::uint8_t recoveredPadF9[3];
  AFBodyPState_t current;
  AFBodyPState_t saved;
  std::uint8_t recoveredPad21C[4];
  idClipQuery motionQuery;
  idSpatialMat inverseWorldSpatialInertia;
  idSpatialMat I;
  idSpatialMat invI;
  idSpatialMat J;
  std::uint8_t recoveredPad288[8];
  idStaticSpatialVec s;
  idStaticSpatialVec totalForce;
  idStaticSpatialVec auxForce;
  idStaticSpatialVec acceleration;
  float* response;
  int* responseIndex;
  int numResponses;
  int maxAuxiliaryIndex;
  int maxSubTreeAuxiliaryIndex;
  std::uint8_t recoveredPad36C[4];
};

class idAFConstraint {
public:
  const void* const* vtable;
  int type;
  std::uint8_t recoveredPad0C[4];
  idAtomicString name;
  idAFBody* body1;
  idAFBody* body2;
  void* physics;
  idStaticSpatialVec lm;
  idSpatialMat J1;
  idSpatialMat J2;
  idStaticSpatialVec c1;
  idStaticSpatialVec c2;
  idStaticSpatialVec lo;
  idStaticSpatialVec hi;
  idStaticSpatialVec e;
  idAFConstraint* boxConstraint;
  int boxIndex[6];
  float boxScale[6];
  idSpatialMat invI;
  idSpatialMat J;
  int firstIndex;
  std::uint8_t fl;
  std::uint8_t recoveredPad1ED[3];
};

class idAFTree {
public:
  void Factor() const;
  void Solve(int auxiliaryIndex) const;
  void Response(const idAFConstraint* constraint, int row,
                int auxiliaryIndex) const;
  void CalculateForces(float timeStep) const;
  void SetMaxSubTreeAuxiliaryIndex();
  void SortBodies();
  void DebugDraw(const idVec4& color) const;

  const idList<idAFBody*, 71>& GetSortedBodies() const {
    return sortedBodies;
  }

  idList<idAFBody*, 71> sortedBodies;

private:
  void SortBodies_r(idList<idAFBody*, 71>& sortedList,
                    idAFBody* body);
};

extern idCVar af_useSymmetry;

static_assert(sizeof(AFBodyPState_t) == 0x90,
              "AF body state ABI drift");
static_assert(sizeof(idAFBody) == 0x370, "idAFBody ABI drift");
static_assert(sizeof(idAFConstraint) == 0x1F0,
              "idAFConstraint ABI drift");
static_assert(sizeof(idAFTree) == 0x18, "idAFTree ABI drift");
static_assert(offsetof(idAFBody, parent) == 0xB0,
              "idAFBody parent offset drift");
static_assert(offsetof(idAFBody, children) == 0xB8,
              "idAFBody children offset drift");
static_assert(offsetof(idAFBody, primaryConstraint) == 0xD0,
              "idAFBody primary constraint offset drift");
static_assert(offsetof(idAFBody, tree) == 0xF0,
              "idAFBody tree offset drift");
static_assert(offsetof(idAFBody, fl) == 0xF8,
              "idAFBody flags offset drift");
static_assert(offsetof(idAFBody, current) == 0xFC,
              "idAFBody current-state offset drift");
static_assert(offsetof(idAFBody, inverseWorldSpatialInertia) == 0x228,
              "idAFBody world inertia offset drift");
static_assert(offsetof(idAFBody, I) == 0x240,
              "idAFBody articulated inertia offset drift");
static_assert(offsetof(idAFBody, invI) == 0x258,
              "idAFBody inverse inertia offset drift");
static_assert(offsetof(idAFBody, J) == 0x270,
              "idAFBody Jacobian offset drift");
static_assert(offsetof(idAFBody, s) == 0x290,
              "idAFBody spatial response offset drift");
static_assert(offsetof(idAFBody, response) == 0x350,
              "idAFBody response buffer offset drift");
static_assert(offsetof(idAFConstraint, lm) == 0x30,
              "idAFConstraint multiplier offset drift");
static_assert(offsetof(idAFConstraint, J1) == 0x60,
              "idAFConstraint J1 offset drift");
static_assert(offsetof(idAFConstraint, invI) == 0x1B8,
              "idAFConstraint inverse offset drift");
static_assert(offsetof(idAFConstraint, fl) == 0x1EC,
              "idAFConstraint flags offset drift");
