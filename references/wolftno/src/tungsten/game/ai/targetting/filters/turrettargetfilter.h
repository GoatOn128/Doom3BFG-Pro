#pragma once

#include "tungsten/game/ai/targetting/targetfilter.h"

class idTurretTargetFilter final : public idTargetFilter {
public:
  explicit idTurretTargetFilter(int minimumAwareness);
  ~idTurretTargetFilter() override;
  bool InternalFilter(const idEntity* owner,
                      const idTargetInfo* targetInfo) const override;
};
