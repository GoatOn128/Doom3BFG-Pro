#include "tungsten/game/ai/ai2_core_support.h"

// DIA: idAnimatedEntity::SetModelByName
idRecoveredValue idAnimatedEntity::SetModelByName(...) { return {}; }

// Header-emitted animated-entity support used by AI.
idTreeAnimator* idAnimatedEntity::GetTreeAnimatorFromPresentable() const {
  if (presentable == nullptr ||
      presentable->GetAnimatedEntityInterface() == nullptr) {
    return nullptr;
  }
  return static_cast<idTreeAnimator*>(
      presentable->GetAnimatedEntityInterface()->GetAnimatedTreeAnimator());
}
