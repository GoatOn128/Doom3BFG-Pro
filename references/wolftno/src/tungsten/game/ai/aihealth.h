#pragma once

class idAIHealth {
public:
  void ClearDeadFlag() {
    dead = false;
    ++clearDeadFlagCount;
  }
  void SetCurBaseHealth_Impl(float health) {
    currentBaseHealth = health;
    ++setCurBaseHealthCount;
  }
  float GetBaseHealthRatio_Impl() const {
    return maximumBaseHealth > 0.0F
               ? currentBaseHealth / maximumBaseHealth
               : 0.0F;
  }
  float GetMaximumBaseHealthForTest() const { return maximumBaseHealth; }
  void SetMaximumBaseHealthForTest(float health) {
    maximumBaseHealth = health;
  }
  void Kill_Impl(float killingBlowDamage = 0.0F) {
    currentBaseHealth = 0.0F;
    lastKillingBlowDamage = killingBlowDamage;
  }
  void SetDeadFlag() { dead = true; }
  float GetTotalCurHealth_Impl() const { return currentBaseHealth; }
  float GetTotalMaxHealth_Impl() const { return maximumBaseHealth; }
  bool IsDead_Impl() const { return dead; }
  float GetCurBaseHealthForTest() const { return currentBaseHealth; }
  float GetLastKillingBlowDamageForTest() const {
    return lastKillingBlowDamage;
  }
  int GetClearDeadFlagCountForTest() const { return clearDeadFlagCount; }
  int GetSetCurBaseHealthCountForTest() const {
    return setCurBaseHealthCount;
  }
  void ResetCallCountsForTest() {
    clearDeadFlagCount = 0;
    setCurBaseHealthCount = 0;
  }

private:
  float currentBaseHealth{100.0F};
  float maximumBaseHealth{100.0F};
  float lastKillingBlowDamage{0.0F};
  bool dead{false};
  int clearDeadFlagCount{0};
  int setCurBaseHealthCount{0};
};

