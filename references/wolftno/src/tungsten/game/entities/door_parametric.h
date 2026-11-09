#pragma once

#include "tungsten/game/game_runtime.h"

// Shared entity contract for the parametric door.  Its implementation belongs
// to entities/door_parametric.cpp when that translation unit is ported.
class idDoor_Parametric : public idEntity {
public:
  virtual void KickOpen(idEntity* kicker) {
    lastKicker = kicker;
    ++kickOpenCount;
  }
  int GetKickOpenCountForTest() const { return kickOpenCount; }
  idEntity* GetLastKickerForTest() const { return lastKicker; }

private:
  idEntity* lastKicker{nullptr};
  int kickOpenCount{0};
};
