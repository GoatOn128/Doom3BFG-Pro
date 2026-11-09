#pragma once

#include "tungsten/game/ai/cover/aicover.h"

class idAICoverManager {
public:
  idAICoverManager();

  idList<idAICover, 5>& GetKnownCover() { return knownCover; }
  const idList<idAICover, 5>& GetKnownCover() const { return knownCover; }
  int GetLastUpdateTime() const { return lastUpdateTime; }
  const idVec3& GetLastUpdatePosition() const { return lastUpdatePosition; }
  bool IsForceUpdate() const { return forceUpdate; }
  void RequestUpdate() { forceUpdate = true; }
  void MarkUpdated(int time, const idVec3& position) {
    lastUpdateTime = time;
    lastUpdatePosition = position;
    forceUpdate = false;
  }

private:
  idList<idAICover, 5> knownCover;
  int lastUpdateTime{0};
  idVec3 lastUpdatePosition{};
  bool forceUpdate{false};
};

extern idCVar ai_coverRadiusOverride;
extern idCVar ai_useFormationCover;
extern idCVar ai_useVehicleCover;
extern idCVar ai_forceVehicleCoverFlag;
