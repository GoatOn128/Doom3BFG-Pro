#pragma once

#include "tungsten/game/ai/targetting/targetselector.h"

class idNeutralTargetSelector final : public idTargetSelector {
public:
  float GetWeight(const idEntity* owner,
                  const idTargetInfo* targetInfo) const override;
  void SetOwnerDirection(const idVec3& value) {
    ownerDirection = value;
    ownerDirection.Normalize();
  }

private:
  idVec3 ownerDirection{1.0F, 0.0F, 0.0F};
};
