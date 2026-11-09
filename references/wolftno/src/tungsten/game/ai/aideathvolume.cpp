#include "tungsten/game/ai/aideathvolume.h"

#include <stdexcept>

// DIA: idAIDeathVolume::Spawn
void idAIDeathVolume::Spawn() {
  if (referenceDir.LengthSqr() < 0.01F) {
    throw std::invalid_argument(
        "idAIDeathVolume reference direction must be non-zero");
  }
  referenceDir.Normalize();
}

// DIA: idAIDeathVolume::idAIDeathVolume
idAIDeathVolume::idAIDeathVolume() = default;

// DIA: `idAIDeathVolume::idAIDeathVolume'::`1'::dtor$0
// DIA: `idAIDeathVolume::idAIDeathVolume'::`1'::dtor$1
// DIA: `idAIDeathVolume::idAIDeathVolume'::`1'::dtor$2
// DIA: `idAIDeathVolume::idAIDeathVolume'::`1'::dtor$3

// DIA: idAIDeathVolume::`scalar deleting destructor'
idAIDeathVolume::~idAIDeathVolume() = default;

// DIA: idAIDeathVolume::Event_Touch
eventVoid* idAIDeathVolume::Event_Touch(eventVoid* result,
                                        idEntity* toucher,
                                        int clipModelId) {
  idVolume::Event_Touch(result, toucher, clipModelId);
  if (!IsTouchable()) return result;
  idAI2* ai = dynamic_cast<idAI2*>(toucher);
  if (ai == nullptr || ai->GetBaseHealth() <= 0.0F || ai->IsDeadState() ||
      ai->IsDyingState()) {
    return result;
  }
  idEntity* damageAttacker = lastActivator != nullptr ? lastActivator : this;
  ai->ApplyVolumeDamage(this, damageAttacker, damageDecl);
  return result;
}

// DIA: idAIDeathVolume::Event_StopKilling
eventVoid* idAIDeathVolume::Event_StopKilling(eventVoid* result) {
  DisableTouch();
  disabling = false;
  remainingKillTime = 0.0F;
  lastActivator = nullptr;
  return result;
}

// DIA: idAIDeathVolume::OnActivate
void idAIDeathVolume::OnActivate(idEntity* activator) {
  if (IsTouchable()) {
    Event_StopKilling(nullptr);
    return;
  }
  lastActivator = activator;
  EnableTouch();
  if (killDuration <= 0.0F) {
    Event_StopKilling(nullptr);
    return;
  }
  disabling = true;
  remainingKillTime = killDuration;
}

void idAIDeathVolume::AdvanceTime(float seconds) {
  if (!disabling || seconds <= 0.0F) return;
  remainingKillTime -= seconds;
  if (remainingKillTime <= 0.0F) Event_StopKilling(nullptr);
}

void idAIDeathVolume::SetAnimFacing(const char* forward, const char* left,
                                    const char* right, const char* back) {
  animFacingRefFwd = forward;
  animFacingRefLeft = left;
  animFacingRefRight = right;
  animFacingRefBack = back;
}
