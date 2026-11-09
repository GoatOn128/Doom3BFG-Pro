#pragma once

#include "tungsten/game/ai/gamestate/aientitystate.h"

#include <cmath>

struct idTargetInfo {
  idTargetInfo();
  void Cache(const aiLatchedState_t* ownerState,
             const idAIEntityState* entityState);

  const idAIEntityState* es{nullptr};
  float distSqr{0.0F};
  float distTo{0.0F};
  idVec3 delta{};
  idVec3 faceDir{1.0F, 0.0F, 0.0F};
  idVec3 dirTo{1.0F, 0.0F, 0.0F};
  int attitude{-1};
  std::uint8_t healthRatio{0};
};
