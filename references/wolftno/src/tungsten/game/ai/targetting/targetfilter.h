#pragma once

#include "tungsten/game/ai/targetting/targetinfo.h"

class idTargetFilter {
public:
  explicit idTargetFilter(int minimumAwareness = 0)
      : minAwareness(minimumAwareness) {}
  virtual ~idTargetFilter() = default;
  bool Filter(const idEntity* owner, const idTargetInfo* targetInfo) const;
  virtual bool InternalFilter(const idEntity* owner,
                              const idTargetInfo* targetInfo) const = 0;
  int GetMinimumAwareness() const { return minAwareness; }

protected:
  int minAwareness{0};
};
