#pragma once

#include "tungsten/game/ai/ai2_actions.h"
#include "tungsten/game/ai/gamestate/aientitystate.h"

#include <array>

enum aiFireMode_t {
  FIREMODE_WEAPON = 0,
  FIREMODE_HOLDFIRE = 1,
  FIREMODE_FIREWHENREADY = 2,
  FIREMODE_SPRAYWHENREADY = 3,
  FIREMODE_FORCEFIRE = 5,
};

enum weaponFireType_t { WFT_SINGLE, WFT_SPRAY };

class idAIFireControl {
public:
  struct weaponInfo_t {
    int slot{0};
  };

  struct updateWeaponStateParms_t {
    float percentBlockedShotsToReleaseTrigger{0.1F};
    weaponFireType_t fireType{WFT_SINGLE};
    int numBlockedShotsConsecutiveToReleaseTrigger{1};
  };

  virtual ~idAIFireControl() = default;
  virtual const weaponInfo_t* GetWeaponInfo(int index) const = 0;
  virtual int NumWeaponInfos() const = 0;
  virtual void SetNextPullTriggerTime(const idWeapon* weapon, int time) = 0;
  virtual int GetNextPullTriggerTime(const idWeapon* weapon) const = 0;
  virtual void SetEndBurstTime(const idWeapon* weapon, int time) = 0;
  virtual int GetEndBurstTime(const idWeapon* weapon) const = 0;
  virtual void SetFireAtLastKnownDuration(const idWeapon* weapon,
                                          int duration) = 0;
  virtual int GetFireAtLastKnownDuration(const idWeapon* weapon) const = 0;
  virtual void ResetNextPullTriggerTime(idAI2* ai, int time) {
    (void)ai;
    (void)time;
  }

  void SetFireMode(idAI2* owner, aiFireMode_t mode);
  void SetForceFireDuration(idAI2* owner, int endTime);
  void ReleaseWeaponTrigger(idAI2* owner, idWeapon* weapon, int curTime);
  void StopFiringEffects(idAI2* owner, idWeapon* weapon, int curTime);
  void InhibitFire(const idWeapon* weapon, int inhibitLength, bool script);
  void ClearInhibitFire(const idWeapon* weapon, bool script);
  bool IsFireInhibited() const;
  float GetMissTimeFractionRemaining(const idAI2* ai, int curTime) const;
  bool CanFireWeapon(idAI2* owner, idWeapon* weapon,
                     const idAIEntityState* entityState, int curTime) const;
  bool CheckReleaseTrigger(idAI2* owner, idWeapon* weapon,
                           idAIEntityState* entityState, int curTime);
  void PullWeaponTrigger(idAI2* owner, idWeapon* weapon,
                         const idAIEntityState* entityState, int curTime);
  bool UseMissTime(idAI2* ai, idEntity* target, int curTime) const;
  void UpdateMissTime(idAI2* ai);
  void Update(idAI2* owner, unsigned int curTime);
  bool CanFireWeapons(idAI2* owner, const idAIEntityState* entityState,
                      int curTime) const;
  bool CheckPullTrigger(idAI2* owner, idWeapon* weapon,
                        idAIEntityState* entityState, int curTime);
  void CheckSecondaryUsage(idAI2* owner, idWeapon* weapon);
  void SetMissTimeTarget(idAI2* ai, idEntity* target, int curTime);
  void UpdateWeaponState(idAI2* ai, idWeapon* weapon,
                         idAIEntityState* entityState, int curTime,
                         const weaponInfo_t* info,
                         const updateWeaponStateParms_t* updateParms);
  void Update_Weapon(idAI2* owner, int curTime);
  void Update_HoldFire(idAI2* owner, int curTime);
  void Update_FireWhenReady(idAI2* owner, int curTime);
  void Update_SprayWhenReady(idAI2* owner, int curTime);
  void Update_ForceFire(idAI2* owner, int curTime);

  void SetTargetState(idAIEntityState* value) { targetState = value; }
  void SetCurrentTime(int value) { currentTime = value; }
  aiFireMode_t GetFireMode() const { return fireMode; }
  int GetForceFireTimeStamp() const { return forceFireTimeStamp; }
  int GetMissTimeEnd() const { return missTimeEnd; }
  bool IsMissTimeTargetVisible() const { return missTimeTargetVisible; }
  bool IsFreeAmmo() const { return freeAmmo; }
  int GetInhibitFireCallCountForTest() const {
    return inhibitFireCallCount;
  }
  const idWeapon* GetLastInhibitedWeaponForTest() const {
    return lastInhibitedWeapon;
  }
  int GetLastInhibitLengthForTest() const { return lastInhibitLength; }
  bool GetLastInhibitScriptForTest() const { return lastInhibitScript; }

protected:
  void ReleaseAllWeapons(idAI2* owner);
  void UpdateReadyWeapons(idAI2* owner, int curTime,
                          weaponFireType_t fireType);

  int allowFireStamp{-1};
  aiFireMode_t savedMode{FIREMODE_HOLDFIRE};
  aiFireMode_t fireMode{FIREMODE_HOLDFIRE};
  bool freeAmmo{false};
  bool missTimeTargetVisible{false};
  int forceFireTimeStamp{-1};
  int missTimeEnd{0};
  idEntity* missTimeTarget{nullptr};
  idVec3 missTimeDisappearPos{};
  bool missTimeDisappearPosValid{false};
  idAIEntityState* targetState{nullptr};
  int currentTime{0};
  int blockedShotsConsecutive{0};
  int inhibitFireCallCount{0};
  const idWeapon* lastInhibitedWeapon{nullptr};
  int lastInhibitLength{0};
  bool lastInhibitScript{false};
};

class idAIFireControl_Human final : public idAIFireControl {
public:
  struct weaponState_t {
    int nextPullTriggerTime{0};
    int endBurstTime{0};
    int fireAtLastKnownDuration{0};
  };

  idAIFireControl_Human();
  const weaponInfo_t* GetWeaponInfo(int index) const override;
  int NumWeaponInfos() const override { return 2; }
  void DormantEnd(int timeDormant);
  void SetNextPullTriggerTime(const idWeapon* weapon, int time) override;
  int GetNextPullTriggerTime(const idWeapon* weapon) const override;
  void ResetNextPullTriggerTime(idAI2* ai, int time) override;
  void SetEndBurstTime(const idWeapon* weapon, int time) override;
  int GetEndBurstTime(const idWeapon* weapon) const override;
  void SetFireAtLastKnownDuration(const idWeapon* weapon,
                                  int duration) override;
  int GetFireAtLastKnownDuration(const idWeapon* weapon) const override;

private:
  int WeaponIndex(const idWeapon* weapon) const;
  static const std::array<weaponInfo_t, 2> weaponInfos;
  std::array<weaponState_t, 2> weaponStates{};
};
