#pragma once

#include "tungsten/game/ai/targetting/targetinfo.h"

class idTargetSelector {
public:
  virtual ~idTargetSelector() = default;
  virtual float GetWeight(const idEntity* owner,
                          const idTargetInfo* targetInfo) const = 0;
};
