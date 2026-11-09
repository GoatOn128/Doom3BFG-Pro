#pragma once

#include "engine/gamelib/physics/physics_dynamicbase.h"

#include <cstddef>
#include <cstdint>

class idPush;

struct parametricPState_t {
  int time;
  int atRest;
  idVec3 worldOrigin;
  idAngles worldAngles;
  idMat3 worldAxis;
  idVec3 localOrigin;
  idAngles localAngles;
  std::uint8_t recoveredMotion[0x154];
  void* spline;
  void* angularSpline;
  std::uint8_t recoveredSplineInterpolation[0x38];
  bool useSplineAngles;
  std::uint8_t recoveredTail[7];
};

class idPhysics_Parametric : public idPhysics_DynamicBase {
public:
  idPhysics_Parametric();
  ~idPhysics_Parametric() override;

  void SetClipModel(idClipModel*, float, int, bool) override;
  idClipModel* GetClipModel(int) override;
  int GetNumClipModels() override;
  void SetMass(float, int) override;
  float GetMass(int) override;
  void SetContents(int, int) override;
  int GetContents(int) override;
  const idBounds& GetBounds(int) override;
  const idBounds& GetAbsBounds(int) override;
  void SetOrigin(const idVec3&, int) override;
  void SetAxis(const idMat3&, int) override;
  void Translate(const idVec3&, int) override;
  void Rotate(const idRotation&, int) override;
  const idVec3& GetOrigin(int) override;
  const idMat3& GetAxis(int) override;
  const idVec3& GetLocalOrigin(int) override;
  void SetLinearVelocity(const idVec3&, int) override;
  void SetAngularVelocity(const idVec3&, int) override;
  idVec3 GetLinearVelocity(int) const override;
  idVec3 GetAngularVelocity(int) override;
  void GetImpactInfo(int, const idVec3&, impactInfo_t&) const override;
  void Activate() override;
  void PutToRest() override;
  bool IsAtRest() const override;
  bool IsPushable(int) override;
  void SaveState() override;
  void RestoreState() override;
  bool Evaluate(int, int) override;
  void UpdateTime(int) override;
  void DisableClip() override;
  void EnableClip() override;
  void UnlinkClip() override;
  void LinkClip() override;
  void SetMaster(bool, const idVec3&, const idMat3&, bindFlags_t) override;
  void SetLocalOrigin(const idVec3&, int) override;
  void SetLocalAxis(const idMat3&, int) override;
  int GetBlockingEntityNum() override;
  int GetLinearEndTime() override;
  int GetAngularEndTime() override;

protected:
  parametricPState_t current;
  parametricPState_t saved;
  idVec6 spatialVelocity;
  int blockingPhysicsId;
  idBounds absBounds;
  idPush* pusher;
  bool isPusher;
  std::uint8_t recoveredPusherPad[7];
  idClipModel* clipModel;
  int pushFlags;
  bool hasMaster;
  bool isOrientated;
  bool hasWorldOrientation;
  std::uint8_t recoveredOrientationPad;
  idMat3 worldAxis;
  idVec3 worldOrigin;

  friend class ParametricDoorValidationAccess;
};

static_assert(sizeof(parametricPState_t) == 0x200,
              "parametricPState_t ABI drift");
static_assert(offsetof(parametricPState_t, worldOrigin) == 0x08,
              "parametric state origin offset drift");
static_assert(offsetof(parametricPState_t, worldAxis) == 0x20,
              "parametric state axis offset drift");
static_assert(sizeof(idPhysics_Parametric) == 0x520,
              "idPhysics_Parametric ABI drift");
