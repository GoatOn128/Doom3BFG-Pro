#pragma once

#include "tungsten/game/ai/targetting/targetfilter.h"

class idFriendlyTargetFilter final : public idTargetFilter {
public:
  explicit idFriendlyTargetFilter(int minimumAwareness);
  bool InternalFilter(const idEntity* owner,
                      const idTargetInfo* targetInfo) const override;
};

class idFriendlyDeadTargetFilter final : public idTargetFilter {
public:
  explicit idFriendlyDeadTargetFilter(int minimumAwareness);
  bool InternalFilter(const idEntity* owner,
                      const idTargetInfo* targetInfo) const override;
};
