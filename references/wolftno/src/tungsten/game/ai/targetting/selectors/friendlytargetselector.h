#pragma once

#include "tungsten/game/ai/targetting/targetselector.h"

class idFriendlyTargetSelector final : public idTargetSelector {
public:
  float GetWeight(const idEntity* owner,
                  const idTargetInfo* targetInfo) const override;
  void SetOwnerDirection(const idVec3& value) {
    ownerDirection = value;
    ownerDirection.Normalize();
  }
  void SetEnemyFocus(const idVec3& direction, float dotThreshold) {
    enemyDirection = direction;
    enemyDirection.Normalize();
    enemyFocusDot = dotThreshold;
    hasEnemy = true;
  }
  void ClearEnemyFocus() { hasEnemy = false; }
  void SetOwnerDying(bool value) { ownerDying = value; }

private:
  idVec3 ownerDirection{1.0F, 0.0F, 0.0F};
  idVec3 enemyDirection{1.0F, 0.0F, 0.0F};
  float enemyFocusDot{0.5F};
  bool hasEnemy{false};
  bool ownerDying{false};
};
