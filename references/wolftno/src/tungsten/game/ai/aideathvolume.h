#pragma once

#include "shared/idlib/text/str.h"
#include "tungsten/game/ai/ai2_actions.h"

class idVolume : public idEntity {
public:
  ~idVolume() override = default;

  virtual eventVoid* Event_Touch(eventVoid* result, idEntity* toucher,
                                 int clipModelId) {
    (void)toucher;
    (void)clipModelId;
    return result;
  }

  void EnableTouch() { touchable = true; }
  void DisableTouch() { touchable = false; }
  bool IsTouchable() const { return touchable; }

private:
  bool touchable{false};
};

class idAIDeathVolume final : public idVolume {
public:
  idAIDeathVolume();
  ~idAIDeathVolume() override;

  void Spawn();
  eventVoid* Event_Touch(eventVoid* result, idEntity* toucher,
                         int clipModelId) override;
  eventVoid* Event_StopKilling(eventVoid* result);
  void OnActivate(idEntity* activator) override;
  void AdvanceTime(float seconds);

  void SetReferenceDirection(const idVec3& value) { referenceDir = value; }
  const idVec3& GetReferenceDirection() const { return referenceDir; }
  void SetKillDuration(float value) { killDuration = value; }
  float GetKillDuration() const { return killDuration; }
  void SetDamageDecl(const idDeclDamage* value) { damageDecl = value; }
  void SetAnimFacing(const char* forward, const char* left,
                     const char* right, const char* back);
  const idEntity* GetLastActivator() const { return lastActivator; }

private:
  idVec3 referenceDir{1.0F, 0.0F, 0.0F};
  bool unidirectional{true};
  bool disabling{false};
  float killDuration{3.0F};
  const idDeclDamage* damageDecl{nullptr};
  idStr animFacingRefFwd;
  idStr animFacingRefLeft;
  idStr animFacingRefRight;
  idStr animFacingRefBack;
  idEntity* lastActivator{nullptr};
  float remainingKillTime{0.0F};
};
