#include "tungsten/game/ai/aievents/aieventvoice.h"

// DIA: idAIEventVoice::GetVoiceMessageType
int idAIEventVoice::GetVoiceMessageType() const {
  const idDeclAiEvent* declaration = GetDeclaration();
  return declaration != nullptr ? declaration->voiceMessage : 7;
}

// DIA: idAIEvent_DelayedVO::GetMessageA
int idAIEvent_DelayedVO::GetMessageA() const {
  const idDeclAiEvent* declaration = GetDeclaration();
  return declaration != nullptr ? declaration->delayedMessage : 30;
}

// DIA: idAIEventVoice::idAIEventVoice
idAIEventVoice::idAIEventVoice() = default;

// DIA: idAIEventVoice::InternalIsTouching
bool idAIEventVoice::InternalIsTouching(idEntity* entity,
                                        int currentTime) const {
  if (entity == nullptr) return false;

  const void* listenerGroup = entity->GetAIGroupIdentity();
  const idEntity* sourceEntity = GetOriginator();
  if (listenerGroup != nullptr && sourceEntity != nullptr &&
      listenerGroup == sourceEntity->GetAIGroupIdentity())
    return true;

  if (entity->GetPhysics() == nullptr) return false;
  const idVec3 listenerOrigin = entity->GetPhysics()->GetOrigin();
  const float distance = (listenerOrigin - GetOrigin()).Length();
  const bool obstructed =
      visibilityQuery != nullptr && !visibilityQuery(GetOrigin(), listenerOrigin);
  if (obstructed && distance > GetRadius() * 0.5F) return false;
  return idAIEventSound::InternalIsTouching(entity, currentTime);
}
