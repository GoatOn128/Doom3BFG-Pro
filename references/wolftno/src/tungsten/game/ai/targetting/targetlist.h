#pragma once

#include "tungsten/game/ai/targetting/targetcache.h"
#include "tungsten/game/ai/targetting/targetfilter.h"
#include "tungsten/game/ai/targetting/targetselector.h"

class idTargetList {
public:
  explicit idTargetList(const idTargetCache* targetCache);
  ~idTargetList();
  int Filter(const idEntity* owner, const idTargetFilter* filter);
  int FindBestTarget(const idEntity* owner, const idTargetFilter* filter,
                     const idTargetSelector* selector);
  int NumViableTargets() const { return viableTargets.Num(); }
  int GetBestTargetIndex() const { return bestTargetIndex; }
  const idTargetInfo* GetViableTarget(int index) const;
  const idTargetInfo* GetBestTarget() const {
    return GetViableTarget(bestTargetIndex);
  }

private:
  const idTargetCache* targetCache{nullptr};
  idStaticList<int, 8> viableTargets;
  int bestTargetIndex{-1};
};
