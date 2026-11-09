#pragma once

#include "tungsten/game/game_runtime.h"

class idWaypoint : public idEntity {
public:
  ~idWaypoint() override;
  void Spawn();
};
