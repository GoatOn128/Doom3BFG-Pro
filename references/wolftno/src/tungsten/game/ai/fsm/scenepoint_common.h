#pragma once

#include "tungsten/game/ai/ai2_actions.h"

#include <array>

struct idScenePointSelectionContext {
  idVec3 actorOrigin{};
  idVec3 playerOrigin{};
  int lastDamageTime{0};
  int scenePointTakeTime{0};
  int scenePointActivateTime{0};
  int enemyAwareness{0};
  bool enemyVisible{false};
  std::array<idScenePoint*, 64> points{};
  int pointCount{0};
};

bool IsScriptScenePointValid(const idScenePointSelectionContext& context,
                             const idScenePoint* scriptScenePoint);
idScenePoint* GetBestScenePoint(const idScenePointSelectionContext& context,
                                float minRange, float maxRange,
                                const idVec3& focus, scenePointType_t type,
                                unsigned int lookFlags = 0);
