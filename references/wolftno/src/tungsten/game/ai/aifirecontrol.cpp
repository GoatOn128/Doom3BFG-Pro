#include "tungsten/game/ai/aifirecontrol.h"

#include "tungsten/game/ai/aidefs.h"

#include <algorithm>

namespace {

int RandomInterval(int minimum, int maximum) {
  if (maximum <= minimum) return minimum;
  return static_cast<int>(GetRandomInterval(
      static_cast<unsigned int>(std::max(0, minimum)), maximum));
}

}  // namespace

const std::array<idAIFireControl::weaponInfo_t, 2>
    idAIFireControl_Human::weaponInfos{{{0}, {1}}};

// DIA: idAIFireControl::SetFireMode
void idAIFireControl::SetFireMode(idAI2* owner, aiFireMode_t mode) {
  if (allowFireStamp > 0) {
    savedMode = mode;
    return;
  }
  if (fireMode == mode) return;
  fireMode = mode;
  ReleaseAllWeapons(owner);
}

// DIA: idAIFireControl::SetForceFireDuration
void idAIFireControl::SetForceFireDuration(idAI2* owner, int endTime) {
  SetFireMode(owner, FIREMODE_FORCEFIRE);
  forceFireTimeStamp = endTime;
}

// DIA: idAIFireControl_Human::GetWeaponInfo
const idAIFireControl::weaponInfo_t* idAIFireControl_Human::GetWeaponInfo(
    int index) const {
  return index >= 0 && index < static_cast<int>(weaponInfos.size())
             ? &weaponInfos[index]
             : nullptr;
}

// DIA: idAIFireControl_Human::DormantEnd
void idAIFireControl_Human::DormantEnd(int timeDormant) {
  for (weaponState_t& state : weaponStates) {
    state.nextPullTriggerTime += timeDormant;
    state.endBurstTime += timeDormant;
  }
}

// DIA: idAIFireControl::ReleaseWeaponTrigger
void idAIFireControl::ReleaseWeaponTrigger(idAI2* owner, idWeapon* weapon,
                                           int curTime) {
  if (weapon == nullptr || !weapon->UsesFireControl()) return;
  weapon->ReleaseTrigger(owner, false);
  if (weapon->fireState != idWeapon::FIRESTATE_RELOADING) {
    weapon->SetFireState(idWeapon::FIRESTATE_IDLE);
  }
  SetNextPullTriggerTime(
      weapon, curTime + RandomInterval(weapon->repullMin, weapon->repullMax));
}

// DIA: idAIFireControl::StopFiringEffects
void idAIFireControl::StopFiringEffects(idAI2* owner, idWeapon* weapon,
                                        int curTime) {
  (void)owner;
  (void)curTime;
  if (weapon != nullptr) weapon->StopAllFiringEffects();
}

// DIA: idAIFireControl::InhibitFire
void idAIFireControl::InhibitFire(const idWeapon* weapon, int inhibitLength,
                                  bool script) {
  ++inhibitFireCallCount;
  lastInhibitedWeapon = weapon;
  lastInhibitLength = inhibitLength;
  lastInhibitScript = script;
  if (script) {
    allowFireStamp = std::max(allowFireStamp, currentTime + inhibitLength);
    savedMode = fireMode;
    fireMode = FIREMODE_HOLDFIRE;
  }
  if (weapon != nullptr &&
      GetNextPullTriggerTime(weapon) < currentTime + inhibitLength) {
    SetNextPullTriggerTime(weapon, currentTime + inhibitLength);
  }
}

// DIA: idAIFireControl::ClearInhibitFire
void idAIFireControl::ClearInhibitFire(const idWeapon* weapon, bool script) {
  if (script) allowFireStamp = allowFireStamp > 0 ? 1 : allowFireStamp;
  if (weapon != nullptr) SetNextPullTriggerTime(weapon, currentTime);
}

// DIA: idAIFireControl::IsFireInhibited
bool idAIFireControl::IsFireInhibited() const {
  return currentTime < allowFireStamp;
}

// DIA: idAIFireControl::GetMissTimeFractionRemaining
float idAIFireControl::GetMissTimeFractionRemaining(const idAI2* ai,
                                                     int curTime) const {
  if (ai == nullptr || ai->GetFireMissTime() == -1.0F || missTimeEnd == -1) {
    return -1.0F;
  }
  if (ai->GetFireMissTime() == 0.0F || curTime > missTimeEnd) return 0.0F;
  return std::min(1.0F, static_cast<float>(missTimeEnd - curTime) /
                            (ai->GetFireMissTime() * 1000.0F));
}

// DIA: idAIFireControl_Human::SetNextPullTriggerTime
void idAIFireControl_Human::SetNextPullTriggerTime(const idWeapon* weapon,
                                                   int time) {
  weaponStates[WeaponIndex(weapon)].nextPullTriggerTime = time;
}

// DIA: idAIFireControl_Human::GetNextPullTriggerTime
int idAIFireControl_Human::GetNextPullTriggerTime(
    const idWeapon* weapon) const {
  return weaponStates[WeaponIndex(weapon)].nextPullTriggerTime;
}

// DIA: idAIFireControl_Human::ResetNextPullTriggerTime
void idAIFireControl_Human::ResetNextPullTriggerTime(idAI2* ai, int time) {
  for (int index = 0; index < NumWeaponInfos(); ++index) {
    idWeapon* weapon = ai != nullptr ? ai->GetFireControlWeapon(index) : nullptr;
    if (weapon == nullptr) continue;
    weaponState_t& state = weaponStates[WeaponIndex(weapon)];
    state.nextPullTriggerTime = std::max(state.nextPullTriggerTime, time + 1000);
  }
}

// DIA: idAIFireControl_Human::SetEndBurstTime
void idAIFireControl_Human::SetEndBurstTime(const idWeapon* weapon, int time) {
  weaponStates[WeaponIndex(weapon)].endBurstTime = time;
}

// DIA: idAIFireControl_Human::GetEndBurstTime
int idAIFireControl_Human::GetEndBurstTime(const idWeapon* weapon) const {
  return weaponStates[WeaponIndex(weapon)].endBurstTime;
}

// DIA: idAIFireControl_Human::SetFireAtLastKnownDuration
void idAIFireControl_Human::SetFireAtLastKnownDuration(
    const idWeapon* weapon, int duration) {
  weaponStates[WeaponIndex(weapon)].fireAtLastKnownDuration = duration;
}

// DIA: idAIFireControl_Human::GetFireAtLastKnownDuration
int idAIFireControl_Human::GetFireAtLastKnownDuration(
    const idWeapon* weapon) const {
  return weaponStates[WeaponIndex(weapon)].fireAtLastKnownDuration;
}

// DIA: idAIFireControl::CanFireWeapon
bool idAIFireControl::CanFireWeapon(
    idAI2* owner, idWeapon* weapon, const idAIEntityState* entityState,
    int curTime) const {
  if (owner == nullptr || weapon == nullptr || !weapon->UsesFireControl()) {
    return false;
  }
  const bool secondary = weapon->weaponState == idWeapon::WEAPONSTATE_SECONDARY;
  if (weapon->NeedsReload(secondary) && !freeAmmo) return false;
  if (owner->HasFireTemporaryHold()) return false;
  if (entityState == nullptr || !entityState->hasFocus ||
      !entityState->aimAligned || entityState->muzzleInSolid ||
      !entityState->robotPowerAvailable) {
    return false;
  }
  if (!entityState->visible &&
      curTime - entityState->confirmedStimTime >=
          GetFireAtLastKnownDuration(weapon)) {
    return false;
  }
  return true;
}

// DIA: idAIFireControl::CheckReleaseTrigger
bool idAIFireControl::CheckReleaseTrigger(idAI2* owner, idWeapon* weapon,
                                          idAIEntityState* entityState,
                                          int curTime) {
  return weapon == nullptr || !weapon->UsesFireControl() ||
         !CanFireWeapon(owner, weapon, entityState, curTime) ||
         curTime >= GetEndBurstTime(weapon) || weapon->burstCount <= 0;
}

// DIA: idAIFireControl::PullWeaponTrigger
void idAIFireControl::PullWeaponTrigger(
    idAI2* owner, idWeapon* weapon, const idAIEntityState* entityState,
    int curTime) {
  (void)entityState;
  if (owner == nullptr || weapon == nullptr || !weapon->UsesFireControl()) {
    return;
  }
  const bool secondary = weapon->weaponState == idWeapon::WEAPONSTATE_SECONDARY;
  weapon->PullTrigger(owner, secondary);
  SetEndBurstTime(
      weapon, curTime + RandomInterval(weapon->burstMin, weapon->burstMax));
  SetFireAtLastKnownDuration(
      weapon, RandomInterval(weapon->lastKnownMin, weapon->lastKnownMax));
}

// DIA: idAIFireControl::UseMissTime
bool idAIFireControl::UseMissTime(idAI2* ai, idEntity* target,
                                  int curTime) const {
  if (ai == nullptr || target == nullptr || ai->GetFireMissTime() == 0.0F ||
      missTimeTargetVisible) {
    return false;
  }
  if (target != missTimeTarget || missTimeEnd == -1) return true;
  return curTime < missTimeEnd;
}

// DIA: idAIFireControl::UpdateMissTime
void idAIFireControl::UpdateMissTime(idAI2* ai) {
  if (ai == nullptr || ai->GetFireMissTime() == 0.0F ||
      missTimeTarget == nullptr || targetState == nullptr) {
    return;
  }
  if (missTimeTargetVisible && !targetState->visible) {
    missTimeTargetVisible = false;
    if (missTimeTarget->GetPhysics() != nullptr) {
      missTimeDisappearPos = missTimeTarget->GetPhysics()->GetOrigin();
      missTimeDisappearPosValid = true;
    }
  }
}

// DIA: idAIFireControl::Update
void idAIFireControl::Update(idAI2* owner, unsigned int curTime) {
  currentTime = static_cast<int>(curTime);
  if (allowFireStamp > 0) {
    if (currentTime <= allowFireStamp) {
      if (fireMode != FIREMODE_HOLDFIRE) fireMode = FIREMODE_HOLDFIRE;
    } else {
      fireMode = savedMode;
      ReleaseAllWeapons(owner);
      allowFireStamp = -1;
    }
  }
  UpdateMissTime(owner);
  if (owner != nullptr && owner->ShouldHoldFireState()) {
    Update_HoldFire(owner, currentTime);
    return;
  }
  if (owner != nullptr && owner->IsFireActionScriptRunning() &&
      fireMode != FIREMODE_FORCEFIRE) {
    Update_Weapon(owner, currentTime);
    return;
  }
  switch (fireMode) {
    case FIREMODE_WEAPON: Update_Weapon(owner, currentTime); break;
    case FIREMODE_HOLDFIRE: Update_HoldFire(owner, currentTime); break;
    case FIREMODE_FIREWHENREADY: Update_FireWhenReady(owner, currentTime); break;
    case FIREMODE_SPRAYWHENREADY: Update_SprayWhenReady(owner, currentTime); break;
    case FIREMODE_FORCEFIRE: Update_ForceFire(owner, currentTime); break;
  }
}

// DIA: idAIFireControl::CanFireWeapons
bool idAIFireControl::CanFireWeapons(
    idAI2* owner, const idAIEntityState* entityState, int curTime) const {
  (void)owner;
  (void)curTime;
  return entityState != nullptr && entityState->entity != nullptr &&
         entityState->hasFocus;
}

// DIA: idAIFireControl::CheckPullTrigger
bool idAIFireControl::CheckPullTrigger(idAI2* owner, idWeapon* weapon,
                                       idAIEntityState* entityState,
                                       int curTime) {
  if (weapon == nullptr || !weapon->UsesFireControl() ||
      weapon->NeedsReload(weapon->weaponState ==
                          idWeapon::WEAPONSTATE_SECONDARY) ||
      entityState == nullptr || !entityState->hasFocus ||
      curTime < GetNextPullTriggerTime(weapon)) {
    return false;
  }
  return CanFireWeapon(owner, weapon, entityState, curTime);
}

// DIA: idAIFireControl::CheckSecondaryUsage
void idAIFireControl::CheckSecondaryUsage(idAI2* owner, idWeapon* weapon) {
  if (owner == nullptr || weapon == nullptr ||
      owner->GetSecondaryFireRange() < 0.0F || targetState == nullptr ||
      targetState->entity == nullptr || owner->GetPhysics() == nullptr ||
      targetState->entity->GetPhysics() == nullptr) {
    return;
  }
  const idVec3 delta = targetState->entity->GetPhysics()->GetOrigin() -
                       owner->GetPhysics()->GetOrigin();
  const float range = owner->GetSecondaryFireRange();
  weapon->SetWeaponMode(delta.LengthSqr() <= range * range);
}

// DIA: idAIFireControl::SetMissTimeTarget
void idAIFireControl::SetMissTimeTarget(idAI2* ai, idEntity* target,
                                        int curTime) {
  if (ai == nullptr || ai->GetFireMissTime() == 0.0F) {
    missTimeTarget = nullptr;
    return;
  }
  if (target != missTimeTarget) {
    missTimeTarget = target;
    missTimeDisappearPosValid = false;
    missTimeTargetVisible = false;
    missTimeEnd = target == nullptr ? 0 : -1;
  }
  if (target != nullptr && targetState != nullptr && targetState->visible) {
    missTimeTargetVisible = true;
    missTimeEnd = ai->GetFireMissTime() == -1.0F
                      ? -1
                      : curTime + static_cast<int>(ai->GetFireMissTime() *
                                                   1000.0F);
  }
}

// DIA: idAIFireControl_Human::idAIFireControl_Human
idAIFireControl_Human::idAIFireControl_Human() = default;

// DIA: idAIFireControl::UpdateWeaponState
void idAIFireControl::UpdateWeaponState(
    idAI2* ai, idWeapon* weapon, idAIEntityState* entityState, int curTime,
    const weaponInfo_t* info, const updateWeaponStateParms_t* updateParms) {
  (void)info;
  if (weapon == nullptr || updateParms == nullptr) return;
  if (weapon->triggerState == idWeapon::TRIGGERSTATE_PULLED) {
    if (fireMode == FIREMODE_FORCEFIRE ||
        CanFireWeapon(ai, weapon, entityState, curTime)) {
      if (weapon->fireState != idWeapon::FIRESTATE_FIRING) {
        weapon->BeginFiring(false);
      }
      blockedShotsConsecutive = 0;
    } else {
      ++blockedShotsConsecutive;
      if (blockedShotsConsecutive >=
          updateParms->numBlockedShotsConsecutiveToReleaseTrigger) {
        ReleaseWeaponTrigger(ai, weapon, curTime);
      }
    }
  }
}

// DIA: `idAIFireControl::UpdateWeaponState'::`1'::dtor$0

// DIA: idAIFireControl::Update_Weapon
void idAIFireControl::Update_Weapon(idAI2* owner, int curTime) {
  updateWeaponStateParms_t parms{};
  for (int index = 0; owner != nullptr && index < NumWeaponInfos(); ++index) {
    const weaponInfo_t* info = GetWeaponInfo(index);
    idWeapon* weapon = info != nullptr
                           ? owner->GetFireControlWeapon(info->slot)
                           : nullptr;
    if (weapon == nullptr) continue;
    if (weapon->triggerState == idWeapon::TRIGGERSTATE_PULLED &&
        weapon->NeedsReload(false)) {
      ReleaseWeaponTrigger(owner, weapon, curTime);
    }
    UpdateWeaponState(owner, weapon, targetState, curTime, info, &parms);
  }
}

// DIA: idAIFireControl::Update_HoldFire
void idAIFireControl::Update_HoldFire(idAI2* owner, int curTime) {
  updateWeaponStateParms_t parms{};
  for (int index = 0; owner != nullptr && index < NumWeaponInfos(); ++index) {
    const weaponInfo_t* info = GetWeaponInfo(index);
    idWeapon* weapon = info != nullptr
                           ? owner->GetFireControlWeapon(info->slot)
                           : nullptr;
    if (weapon == nullptr) continue;
    CheckSecondaryUsage(owner, weapon);
    if (weapon->triggerState == idWeapon::TRIGGERSTATE_PULLED) {
      ReleaseWeaponTrigger(owner, weapon, curTime);
    }
    UpdateWeaponState(owner, weapon, targetState, curTime, info, &parms);
  }
}

// DIA: idAIFireControl::Update_FireWhenReady
void idAIFireControl::Update_FireWhenReady(idAI2* owner, int curTime) {
  freeAmmo = false;
  UpdateReadyWeapons(owner, curTime, WFT_SINGLE);
}

// DIA: idAIFireControl::Update_SprayWhenReady
void idAIFireControl::Update_SprayWhenReady(idAI2* owner, int curTime) {
  freeAmmo = true;
  UpdateReadyWeapons(owner, curTime, WFT_SPRAY);
}

// DIA: idAIFireControl::Update_ForceFire
void idAIFireControl::Update_ForceFire(idAI2* owner, int curTime) {
  if (forceFireTimeStamp < 0 || curTime > forceFireTimeStamp) {
    SetFireMode(owner, FIREMODE_HOLDFIRE);
    return;
  }
  updateWeaponStateParms_t parms{0.1F, WFT_SPRAY, 1};
  for (int index = 0; owner != nullptr && index < NumWeaponInfos(); ++index) {
    const weaponInfo_t* info = GetWeaponInfo(index);
    idWeapon* weapon = info != nullptr
                           ? owner->GetFireControlWeapon(info->slot)
                           : nullptr;
    if (weapon == nullptr) continue;
    CheckSecondaryUsage(owner, weapon);
    if (weapon->triggerState == idWeapon::TRIGGERSTATE_RELEASED) {
      weapon->PullTrigger(owner,
                          weapon->weaponState == idWeapon::WEAPONSTATE_SECONDARY);
      SetEndBurstTime(weapon, forceFireTimeStamp);
      weapon->BeginFiring(false);
    }
    UpdateWeaponState(owner, weapon, nullptr, curTime, info, &parms);
  }
}

void idAIFireControl::ReleaseAllWeapons(idAI2* owner) {
  for (int index = 0; owner != nullptr && index < NumWeaponInfos(); ++index) {
    const weaponInfo_t* info = GetWeaponInfo(index);
    idWeapon* weapon = info != nullptr
                           ? owner->GetFireControlWeapon(info->slot)
                           : nullptr;
    if (weapon == nullptr) continue;
    weapon->ReleaseTrigger(owner, false);
    weapon->SetFireState(idWeapon::FIRESTATE_IDLE);
  }
}

void idAIFireControl::UpdateReadyWeapons(idAI2* owner, int curTime,
                                         weaponFireType_t fireType) {
  if (!CanFireWeapons(owner, targetState, curTime)) return;
  updateWeaponStateParms_t parms{
      fireType == WFT_SPRAY ? 0.7F : 0.1F, fireType,
      fireType == WFT_SPRAY ? 5 : 1};
  for (int index = 0; owner != nullptr && index < NumWeaponInfos(); ++index) {
    const weaponInfo_t* info = GetWeaponInfo(index);
    idWeapon* weapon = info != nullptr
                           ? owner->GetFireControlWeapon(info->slot)
                           : nullptr;
    if (weapon == nullptr) continue;
    CheckSecondaryUsage(owner, weapon);
    if (weapon->triggerState == idWeapon::TRIGGERSTATE_RELEASED) {
      if (CheckPullTrigger(owner, weapon, targetState, curTime)) {
        PullWeaponTrigger(owner, weapon, targetState, curTime);
      }
    } else if (CheckReleaseTrigger(owner, weapon, targetState, curTime)) {
      ReleaseWeaponTrigger(owner, weapon, curTime);
    }
    UpdateWeaponState(owner, weapon, targetState, curTime, info, &parms);
  }
}

int idAIFireControl_Human::WeaponIndex(const idWeapon* weapon) const {
  if (weapon == nullptr) return 0;
  return std::clamp(weapon->GetEquipSlot(), 0,
                    static_cast<int>(weaponStates.size()) - 1);
}
