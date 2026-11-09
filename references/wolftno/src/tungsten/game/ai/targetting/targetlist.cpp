#include "tungsten/game/ai/targetting/targetlist.h"

struct widgetTransition_t {
  ~widgetTransition_t();
};

// DIA: widgetTransition_t::~widgetTransition_t
widgetTransition_t::~widgetTransition_t() = default;

idTargetList::~idTargetList() = default;

// DIA: idTargetList::Filter
int idTargetList::Filter(const idEntity* owner,
                         const idTargetFilter* filter) {
  viableTargets.Clear();
  bestTargetIndex = -1;
  if (targetCache == nullptr || filter == nullptr) return 0;
  for (int index = 0; index < targetCache->NumTargets(); ++index) {
    const idTargetInfo* target = targetCache->GetTarget(index);
    if (target != nullptr && filter->Filter(owner, target) &&
        viableTargets.Num() < 8)
      viableTargets.Append(index);
  }
  return viableTargets.Num();
}

// DIA: idTargetList::FindBestTarget
int idTargetList::FindBestTarget(const idEntity* owner,
                                 const idTargetFilter* filter,
                                 const idTargetSelector* selector) {
  Filter(owner, filter);
  if (selector == nullptr) return bestTargetIndex;
  float bestWeight = 0.0F;
  for (int viableIndex = 0; viableIndex < viableTargets.Num(); ++viableIndex) {
    const idTargetInfo* target = GetViableTarget(viableIndex);
    const float weight = target != nullptr
                             ? selector->GetWeight(owner, target) : 0.0F;
    if (weight > bestWeight) {
      bestWeight = weight;
      bestTargetIndex = viableIndex;
    }
  }
  return bestTargetIndex;
}

// DIA: idTargetList::idTargetList
idTargetList::idTargetList(const idTargetCache* newTargetCache)
    : targetCache(newTargetCache) {}

const idTargetInfo* idTargetList::GetViableTarget(int index) const {
  if (targetCache == nullptr || index < 0 || index >= viableTargets.Num())
    return nullptr;
  return targetCache->GetTarget(viableTargets[index]);
}
