#pragma once

#include "engine/sys/sys_achievements.h"

struct UserAchievementStored_t {
  char achievementName[128]{};
};

class idSteamUserStats {
public:
  virtual ~idSteamUserStats() = default;
  virtual bool StoreStats() = 0;
  virtual bool SetAchievement(const char* name) = 0;
  virtual bool ClearAchievement(const char* name) = 0;
  virtual bool GetAchievement(const char* name, bool* achieved) = 0;
  virtual const char* GetAchievementDisplayAttribute(const char* name, const char* key) = 0;
  virtual bool RequestCurrentStats() { return false; }
  virtual bool SetStat(const char* name, int value) { (void)name; (void)value; return false; }
};

class idAchievementSystemWin final : public idAchievementSystem {
public:
  idAchievementSystemWin();
  bool IsInitialized() const override;
  void Pump() override;
  void AchievementUnlock(idLocalUser* user, int achievementId) override;
  void AchievementLock(idLocalUser* user, int achievementId) override;
  void AchievementLockAll(idLocalUser* user, int maxId) override;
  bool GetAchievementDescription(idLocalUser* user, int achievementId,
                                 achievementDescription_t* data) override;
  bool GetAchievementState(idLocalUser* user, std::array<bool, 128>& achievements) override;
  void OnAchievementStored(UserAchievementStored_t* result);

private:
  bool cheatingDialogShown{};
  bool needsUpdate{};
};

void Sys_SetSteamUserStats(idSteamUserStats* stats);
void Sys_SetSteamInitialized(bool initialized);
void Sys_SetAchievementsAllowed(bool allowed);
idSteamUserStats* Sys_GetSteamUserStats();
