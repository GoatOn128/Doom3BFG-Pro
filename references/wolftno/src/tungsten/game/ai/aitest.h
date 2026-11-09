#pragma once

#include "tungsten/game/ai/rotationtracker.h"
#include "tungsten/game/game_runtime.h"

class idAITest_Rotation : public idEntity {
public:
  idAITest_Rotation();
  ~idAITest_Rotation() override;

  void Think();

  const idVec3& GetLastDirection() const { return lastDir; }
  const idRotationTracker& GetTracker() const { return tracker; }
  void SetRate(const idAngles& value) { rate = value; }

private:
  idRotationTracker tracker;
  idVec3 lastDir{1.0F, 0.0F, 0.0F};
  idAngles rate{360.0F, 360.0F, 0.0F};
};

extern idCVar aitest_freezeTracker;
