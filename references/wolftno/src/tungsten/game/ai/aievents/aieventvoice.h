#pragma once

#include "tungsten/game/ai/aievents/aieventsound.h"

class idAIEventVoice : public idAIEventSound {
public:
  using VisibilityQuery = bool (*)(const idVec3& soundOrigin,
                                    const idVec3& listenerOrigin);

  idAIEventVoice();
  int GetVoiceMessageType() const;
  bool InternalIsTouching(idEntity* entity, int currentTime) const;
  static void SetVisibilityQuery(VisibilityQuery value) {
    visibilityQuery = value;
  }

private:
  inline static VisibilityQuery visibilityQuery{nullptr};
};

class idAIEvent_DelayedVO : public idAIEventVoice {
public:
  int GetMessageA() const;
};
