#pragma once

#include "engine/gamelib/physics/physics_parametric.h"
#include "shared/idlib/containers/array.h"

class idPhysics_ParametricDoor : public idPhysics_Parametric {
public:
  idPhysics_ParametricDoor();
  ~idPhysics_ParametricDoor() override;

  void SetClipModel(idClipModel* model, float density, int id,
                    bool freeOld) override;
  idClipModel* GetClipModel(int id) override;
  int GetNumClipModels() override;
  void SetContents(int contents, int id) override;
  int GetContents(int id) override;
  void RestoreState() override;
  void SetOrigin(const idVec3& newOrigin, int id) override;
  void SetAxis(const idMat3& newAxis, int id) override;
  void DisableClip() override;
  void EnableClip() override;
  void UnlinkClip() override;
  void LinkClip() override;
  void SetLocalOrigin(const idVec3& newOrigin, int id) override;
  void SetLocalAxis(const idMat3& newAxis, int id) override;
  bool Evaluate(int timeStepMSec, int endTimeMSec) override;

  idArray<idClipModel*, 2> clipModels;
};

static_assert(sizeof(idPhysics_ParametricDoor) == 0x530,
              "idPhysics_ParametricDoor ABI drift");
static_assert(offsetof(idPhysics_ParametricDoor, clipModels) == 0x520,
              "idPhysics_ParametricDoor clip-model offset drift");
