#pragma once

#include "tungsten/game/ai/targetting/targetfilter.h"

class idEnemyTargetFilter final : public idTargetFilter {
public:
  explicit idEnemyTargetFilter(int minimumAwareness);
  bool InternalFilter(const idEntity* owner,
                      const idTargetInfo* targetInfo) const override;
};
