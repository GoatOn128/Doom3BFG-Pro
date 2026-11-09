#pragma once

#include "tungsten/game/ai/targetting/targetfilter.h"

class idNonEnemyTargetFilter final : public idTargetFilter {
public:
  explicit idNonEnemyTargetFilter(int minimumAwareness);
  bool InternalFilter(const idEntity* owner,
                      const idTargetInfo* targetInfo) const override;
};
