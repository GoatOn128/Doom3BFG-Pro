#pragma once

#include "shared/idlib/containers/staticlist.h"
#include "tungsten/game/ai/ai2_actions.h"

#include <cstdint>

class idAI2;

struct idJointIndex {
  short value{-1};
};

struct idDamageImpulse {
  const idDeclDamage* damageDecl{nullptr};
  idVec3 impactDir{};
  idVec3 impactPoint{};
  idJointIndex jointIndex{};
  float damage{0.0F};
  bool armorPoppedOff{false};
  bool forceInjured{false};
  bool blinded{false};
};

class idAIDamageInfo {
public:
  struct recentDamager_t {
    const idEntity* entity{nullptr};
    float recentDamage{0.0F};
  };

  idAIDamageInfo();
  ~idAIDamageInfo();

  idVec3 GetAverageImpactPoint() const;
  idVec3 GetAverageImpactDir() const;
  const idDamageImpulse* GetDeathImpulse() const;
  void DecayRecentDamagers();
  float GetRecentDamage(const idEntity* entity) const;
  void EndFrame();
  void AddRecentDamage(const idEntity* entity, float damage);
  void Update(idAI2* ai, idEntity* inflictor, idEntity* attacker,
              const idDeclDamage* damageDecl, float damage,
              float unscaledDamage, float damageScale, float armorDamage,
              const idVec3& impactPoint, const idVec3& impactDir,
              idJointIndex impactJoint, bool killed, bool armorPoppedOff,
              bool forceInjured, bool blinded);

  idEntity* GetAttacker() const;
  idEntity* GetInflictor() const;
  const idEntity* GetAttackerEntity() const { return attacker; }
  const idEntity* GetInflictorEntity() const { return inflictor; }

  int NumImpulses() const { return impulses.Num(); }
  const idDamageImpulse* GetImpulseForTest(int index) const {
    return index >= 0 && index < impulses.Num() ? &impulses[index] : nullptr;
  }
  const idDamageImpulse* GetLastImpulse() const {
    return impulses.Num() > 0 ? &impulses[impulses.Num() - 1] : nullptr;
  }
  idJointIndex GetJointIndex() const { return jointIndex; }
  void SetJointIndexForTest(short value) { jointIndex.value = value; }
  float GetFrameDamage() const { return frameDamage; }
  float GetFrameArmorDamage() const { return frameArmorDamage; }
  float GetRecentUnscaledDamage() const { return recentUnscaledDamage; }
  float GetRecentBodyDamage() const { return recentBodyDamage; }
  bool ShouldPain() const { return shouldPain; }
  void SetShouldPain(bool value) { shouldPain = value; }
  const char* GetInjuryDamageGroup() const { return injuryDamageGroup.data; }
  int GetInjuryGoreLevelIndex() const { return injuryGoreLevelIndex; }
  void SetInjuryGoreLevel(const char* groupName, int goreLevelIndex) {
    injuryDamageGroup = groupName != nullptr ? groupName : "";
    injuryGoreLevelIndex = goreLevelIndex;
  }
  void ConfigureInjuryGroupForTest(const char* groupName,
                                   int goreLevelIndex) {
    SetInjuryGoreLevel(groupName, goreLevelIndex);
  }
  int GetMostDamagedIndex() const { return mostDamagedIndex; }
  void SetMostDamagedIndex(int value) { mostDamagedIndex = value; }
  void ConfigureDeathInfoForTest(const idEntity* attackerEntity,
                                 const idEntity* inflictorEntity,
                                 const idDamageImpulse* deathImpulse) {
    attacker = attackerEntity;
    inflictor = inflictorEntity;
    impulses.Clear();
    deathImpulseIdx = -1;
    if (deathImpulse != nullptr) deathImpulseIdx = impulses.Append(*deathImpulse);
  }

private:
  idStaticList<idDamageImpulse, 8> impulses;
  const idEntity* inflictor{nullptr};
  const idEntity* attacker{nullptr};
  idStaticList<recentDamager_t, 4> recentDamagers;
  idJointIndex jointIndex{};
  int deathImpulseIdx{-1};
  float frameDamage{0.0F};
  float frameArmorDamage{0.0F};
  float recentUnscaledDamage{0.0F};
  float recentBodyDamage{0.0F};
  int lastBloodSprayTime{-1};
  int mostDamagedIndex{-1};
  bool shouldPain{false};
  idStr injuryDamageGroup;
  int injuryGoreLevelIndex{0};
};
