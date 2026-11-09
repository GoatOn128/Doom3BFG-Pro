#include "tungsten/game/ai/ai2_core_support.h"

// Header-emitted scene-point support used by AI.
void idScenePoint::Disable(int currentTime, float disableSeconds) {
  currentUser = nullptr;
  requestTime = disableSeconds < 0.0F
                    ? -1
                    : currentTime -
                          static_cast<int>(disableSeconds * -1000.0F);
}

void idScenePoint::DebugDrawScenepoint(const idVec3& viewOrigin, float score,
                                       bool drawText,
                                       bool drawConnection) {
  lastDebugDrawOrigin = viewOrigin;
  lastDebugDrawScore = score;
  lastDebugDrawText = drawText;
  lastDebugDrawConnection = drawConnection;
  ++debugDrawCount;
}

bool idScenePoint_Combat::HasEnterAnimation() const {
  return !enterAnimation.IsEmpty() || extraEnterAnimationCount > 0;
}
