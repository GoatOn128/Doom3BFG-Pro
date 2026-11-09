#pragma once

#include "tungsten/game/ai/targetting/targetselector.h"

class idTurretTargetSelector final : public idTargetSelector {
public:
  float GetWeight(const idEntity* owner,
                  const idTargetInfo* targetInfo) const override;
  void SetTrackOnlyPlayer(bool value) { trackOnlyPlayer = value; }
  void SetDistanceRange(float minimum, float maximum) {
    minTargetDistance = minimum;
    maxTargetDistance = maximum;
  }
  void SetSearchCone(const idVec3& direction, float angleDegrees) {
    forwardSearchDirection = direction;
    forwardSearchDirection.Normalize();
    maxSearchAngle = angleDegrees;
  }
  void SetLastTarget(const idEntity* value) { lastTarget = value; }
  void SetLastAttacker(const idEntity* value) { lastAttacker = value; }

private:
  bool trackOnlyPlayer{false};
  float minTargetDistance{0.0F};
  float maxTargetDistance{4096.0F};
  idVec3 forwardSearchDirection{1.0F, 0.0F, 0.0F};
  float maxSearchAngle{180.0F};
  const idEntity* lastTarget{nullptr};
  const idEntity* lastAttacker{nullptr};
};

extern idCVar turret_debugTargeting;
