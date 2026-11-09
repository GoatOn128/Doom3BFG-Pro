#include "tungsten/game/ai/cover/coverquery.h"

#include <cmath>

namespace {

float CoverArcCosine() {
  volatile float degrees = 55.0F;
  return std::cos(degrees * 0.01745329251994329577F);
}

}  // namespace

// DIA: `dynamic initializer for 'idCoverQuery::MAX_COVER_ARC_ACOS_DEGREES''
float idCoverQuery::MAX_COVER_ARC_ACOS_DEGREES = CoverArcCosine();
