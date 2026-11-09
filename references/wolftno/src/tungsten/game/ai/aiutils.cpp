#include "tungsten/game/ai/aiutils.h"

#include <cmath>

idCVar ai_showSearchPath("ai_showSearchPath", "0", 1,
                         "shows the search path calculated by AIs");

// DIA: idAIUtils::CalcDecelerationDistance
float idAIUtils::CalcDecelerationDistance(float currentVelocity,
                                          float decelerationRate) {
  if (std::fabs(decelerationRate) < 0.00001F) return 0.0F;
  return -(currentVelocity * currentVelocity) / (2.0F * decelerationRate);
}

// DIA: idAIUtils::CalcExactDecelerationRate
float idAIUtils::CalcExactDecelerationRate(float currentVelocity,
                                           float distance) {
  if (std::fabs(distance) <= 0.00001F) return 0.0F;
  return -(currentVelocity * currentVelocity) / (2.0F * distance);
}

// DIA: `dynamic initializer for 'ai_showSearchPath''

// DIA: `dynamic atexit destructor for 'ai_showSearchPath''
