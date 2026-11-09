#pragma once

#include "tungsten/game/game_runtime.h"

class idAIUtils {
public:
  static float CalcDecelerationDistance(float currentVelocity,
                                        float decelerationRate);
  static float CalcExactDecelerationRate(float currentVelocity,
                                         float distance);
};

extern idCVar ai_showSearchPath;
