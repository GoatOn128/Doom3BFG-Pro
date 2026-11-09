#include "engine/sys/win32/win_achievements.h"

#include "engine/sys/sys_localuser.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/str.h"

#include <cstring>

extern idCVar achievements_Verbose;

namespace {
idSteamUserStats* steamUserStats = nullptr;
bool steamInitialized = false;
bool achievementsAllowed = true;

idStr AchievementName(int achievementId) {
  idStr name;
  name.Format("ach_%i", achievementId);
  return name;
}
}

void Sys_SetSteamUserStats(idSteamUserStats* stats) { steamUserStats = stats; }
idSteamUserStats* Sys_GetSteamUserStats() { return steamUserStats; }
void Sys_SetSteamInitialized(bool initialized) { steamInitialized = initialized; }
void Sys_SetAchievementsAllowed(bool allowed) { achievementsAllowed = allowed; }

idAchievementSystemWin::idAchievementSystemWin() { users.granularity = 1; }

bool idAchievementSystemWin::IsInitialized() const { return steamInitialized; }

void idAchievementSystemWin::Pump() {
  if (!needsUpdate) return;
  needsUpdate = false;
  if (steamUserStats != nullptr) steamUserStats->StoreStats();
}

void idAchievementSystemWin::AchievementUnlock(idLocalUser* user, int achievementId) {
  if (steamUserStats != nullptr && achievementsAllowed) {
    const idStr name = AchievementName(achievementId);
    idLib::PrintfIf(achievements_Verbose.GetBool(), "Achievement unlock: %d\n", achievementId);
    if (steamUserStats->SetAchievement(name.data)) {
      if (user != nullptr && user->GetProfile() != nullptr)
        user->GetProfile()->SetAchievement(achievementId);
      needsUpdate = true;
    }
    return;
  }
  if (!achievementsAllowed && !cheatingDialogShown) cheatingDialogShown = true;
}

void idAchievementSystemWin::AchievementLock(idLocalUser* user, int achievementId) {
  if (steamUserStats == nullptr) return;
  const idStr name = AchievementName(achievementId);
  idLib::PrintfIf(achievements_Verbose.GetBool(), "Achievement reset: %d\n", achievementId);
  steamUserStats->ClearAchievement(name.data);
  if (user != nullptr && user->GetProfile() != nullptr)
    user->GetProfile()->ClearAchievement(achievementId);
  needsUpdate = true;
}

void idAchievementSystemWin::AchievementLockAll(idLocalUser* user, int maxId) {
  idLib::PrintfIf(achievements_Verbose.GetBool(), "Resetting all achievements\n");
  if (steamUserStats == nullptr) return;
  for (int achievementId = 1; achievementId <= maxId; ++achievementId) {
    const idStr name = AchievementName(achievementId);
    if (!steamUserStats->ClearAchievement(name.data)) break;
    if (user != nullptr && user->GetProfile() != nullptr)
      user->GetProfile()->ClearAchievement(achievementId);
    needsUpdate = true;
  }
}

bool idAchievementSystemWin::GetAchievementDescription(idLocalUser*, int achievementId,
                                                       achievementDescription_t* data) {
  if (steamUserStats == nullptr || data == nullptr) return false;
  const idStr name = AchievementName(achievementId);
  const char* displayName = steamUserStats->GetAchievementDisplayAttribute(name.data, "name");
  if (displayName == nullptr || displayName[0] == '\0') return false;
  idStr::Copynz(data->name, displayName, sizeof(data->name));
  const char* description = steamUserStats->GetAchievementDisplayAttribute(name.data, "desc");
  if (description != nullptr) idStr::Copynz(data->description, description, sizeof(data->description));
  const char* hidden = steamUserStats->GetAchievementDisplayAttribute(name.data, "hidden");
  if (hidden != nullptr) data->hidden = std::strcmp(hidden, "1") == 0;
  return true;
}

bool idAchievementSystemWin::GetAchievementState(idLocalUser*,
                                                  std::array<bool, 128>& achievements) {
  achievements.fill(false);
  if (steamUserStats == nullptr) return false;
  for (int achievementId = 1; achievementId < static_cast<int>(achievements.size()); ++achievementId) {
    const idStr name = AchievementName(achievementId);
    if (!steamUserStats->GetAchievement(name.data, &achievements[achievementId])) break;
  }
  return true;
}

void idAchievementSystemWin::OnAchievementStored(UserAchievementStored_t* result) {
  idLib::PrintfIf(achievements_Verbose.GetBool(), "[%s], %s\n",
                  "idAchievementSystemWin::OnAchievementStored",
                  result != nullptr ? result->achievementName : "");
}
