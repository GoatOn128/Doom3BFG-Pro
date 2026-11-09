#include "tungsten/game/ai/aidamageinfo.h"

#include <algorithm>

// DIA: idAIDamageInfo::GetAverageImpactPoint
idVec3 idAIDamageInfo::GetAverageImpactPoint() const {
  idVec3 average{};
  if (impulses.Num() == 0) return average;
  for (int index = 0; index < impulses.Num(); ++index) {
    average += impulses[index].impactPoint;
  }
  return average * (1.0F / static_cast<float>(impulses.Num()));
}

// DIA: idAIDamageInfo::GetAverageImpactDir
idVec3 idAIDamageInfo::GetAverageImpactDir() const {
  idVec3 average{};
  if (impulses.Num() == 0) return average;
  for (int index = 0; index < impulses.Num(); ++index) {
    average += impulses[index].impactDir;
  }
  average *= 1.0F / static_cast<float>(impulses.Num());
  average.Normalize();
  return average;
}

// DIA: idAIDamageInfo::GetDeathImpulse
const idDamageImpulse* idAIDamageInfo::GetDeathImpulse() const {
  return deathImpulseIdx >= 0 && deathImpulseIdx < impulses.Num()
             ? &impulses[deathImpulseIdx]
             : nullptr;
}

// DIA: idAIDamageInfo::DecayRecentDamagers
void idAIDamageInfo::DecayRecentDamagers() {
  for (int index = 0; index < recentDamagers.Num();) {
    recentDamager_t& recent = recentDamagers[index];
    if (recent.recentDamage >= 0.01F) {
      recent.recentDamage *= 0.99F;
      ++index;
    } else {
      recentDamagers.RemoveIndexFast(index);
    }
  }
}

// DIA: idAIDamageInfo::GetRecentDamage
float idAIDamageInfo::GetRecentDamage(const idEntity* entity) const {
  for (int index = 0; index < recentDamagers.Num(); ++index) {
    if (recentDamagers[index].entity == entity) {
      return recentDamagers[index].recentDamage;
    }
  }
  return 0.0F;
}

// DIA: idAIDamageInfo::EndFrame
void idAIDamageInfo::EndFrame() {
  impulses.Clear();
  inflictor = nullptr;
  attacker = nullptr;
  jointIndex.value = -1;
  deathImpulseIdx = -1;
  frameDamage = 0.0F;
  frameArmorDamage = 0.0F;
  recentUnscaledDamage *= 0.95F;
  if (recentUnscaledDamage < 0.01F) recentUnscaledDamage = 0.0F;
  recentBodyDamage *= 0.95F;
  if (recentBodyDamage < 0.01F) recentBodyDamage = 0.0F;
  DecayRecentDamagers();
  shouldPain = false;
}

// DIA: idStaticList<idDamageImpulse,8>::idStaticList<idDamageImpulse,8>
template idStaticList<idDamageImpulse, 8>::idStaticList() noexcept;

// DIA: idAIDamageInfo::idAIDamageInfo
idAIDamageInfo::idAIDamageInfo() = default;

// DIA: idList<idAIDamageInfo::recentDamager_t,5>::Resize
template bool idList<idAIDamageInfo::recentDamager_t, 5>::Resize(int);

// DIA: idList<idDamageImpulse,5>::Resize
template bool idList<idDamageImpulse, 5>::Resize(int);

// DIA: idList<idAIDamageInfo::recentDamager_t,5>::Append
template int idList<idAIDamageInfo::recentDamager_t, 5>::Append(
    const idAIDamageInfo::recentDamager_t&);

// DIA: idAIDamageInfo::AddRecentDamage
void idAIDamageInfo::AddRecentDamage(const idEntity* entity, float damage) {
  if (entity == nullptr) return;
  for (int index = 0; index < recentDamagers.Num(); ++index) {
    if (recentDamagers[index].entity == entity) {
      recentDamagers[index].recentDamage += damage;
      return;
    }
  }
  recentDamager_t recent{entity, damage};
  if (recentDamagers.Num() < 4) {
    recentDamagers.Append(recent);
    return;
  }
  int replacement = 0;
  for (int index = 1; index < recentDamagers.Num(); ++index) {
    if (recentDamagers[index].recentDamage <
        recentDamagers[replacement].recentDamage) {
      replacement = index;
    }
  }
  recentDamagers[replacement] = recent;
}

// DIA: idList<idDamageImpulse,5>::Alloc
template idDamageImpulse* idList<idDamageImpulse, 5>::Alloc();

// DIA: idAIDamageInfo::Update
void idAIDamageInfo::Update(
    idAI2* ai, idEntity* newInflictor, idEntity* newAttacker,
    const idDeclDamage* damageDecl, float damage, float unscaledDamage,
    float damageScale, float armorDamage, const idVec3& impactPoint,
    const idVec3& impactDir, idJointIndex impactJoint, bool killed,
    bool armorPoppedOff, bool forceInjured, bool blinded) {
  (void)ai;
  (void)damageScale;
  idDamageImpulse* impulse = impulses.Alloc();
  frameDamage += damage;
  frameArmorDamage += armorDamage;
  recentUnscaledDamage += unscaledDamage;
  recentBodyDamage += damage;
  *impulse = {damageDecl, impactDir, impactPoint, impactJoint, damage,
              armorPoppedOff, forceInjured, blinded};
  if (newAttacker != nullptr) AddRecentDamage(newAttacker, damage);
  if (deathImpulseIdx < 0) {
    inflictor = newInflictor;
    attacker = newAttacker;
    if (killed) deathImpulseIdx = impulses.Num() - 1;
  }
  jointIndex = impactJoint;
}

// Header-emitted damage-info support used by AI2.
idEntity* idAIDamageInfo::GetInflictor() const {
  return const_cast<idEntity*>(inflictor);
}

idEntity* idAIDamageInfo::GetAttacker() const {
  return const_cast<idEntity*>(attacker);
}

float idDamageGroup::HealthRatio() const {
  return maxHealth > 0.0F ? health / maxHealth : 0.0F;
}
