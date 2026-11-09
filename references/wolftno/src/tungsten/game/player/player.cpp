#include "tungsten/game/ai/ai2_core_support.h"

// Header-emitted player support used by AI.
idRecoveredValue idPlayer::GetUCmdTracker1(...) { return {}; }

bool idPlayer::GetIsSprinting() const { return sprinting; }

idRecoveredValue idPlayer::GetFocusTracker(...) { return {}; }

idRecoveredValue idPlayer::IsZoomedIn(...) { return {}; }

idProxyMove* idPlayer::GetProxyMove() const {
  if (proxyMoveEntity == nullptr) return nullptr;
  const int entityNumber = proxyMoveEntity->GetEntityNumber();
  if (entityNumber >= 0 && gameLocal != nullptr &&
      gameLocal->GetEntityByNumber(entityNumber) != proxyMoveEntity) {
    return nullptr;
  }
  return dynamic_cast<idProxyMove*>(proxyMoveEntity);
}
