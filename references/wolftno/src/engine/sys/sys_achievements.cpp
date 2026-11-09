#include "engine/sys/sys_achievements.h"
#include "engine/sys/sys_localuser.h"

idAchievementSystem::~idAchievementSystem() { users.ClearFree(); }

void idAchievementSystem::RegisterLocalUser(idLocalUser* user) {
  if (user == nullptr) return;
  for (int i = 0; i < users.Num(); ++i) if (users[i] == user) return;
  users.Append(user);
}

void idAchievementSystem::RemoveLocalUser(idLocalUser* user) {
  for (int i = 0; i < users.Num(); ++i) {
    if (users[i] != user) continue;
    for (int j = i + 1; j < users.Num(); ++j) users[j - 1] = users[j];
    --users.num;
    return;
  }
}

bool idAchievementSystem::GetRequiredStorage(std::uint64_t* requiredSizeTrophiesBytes) {
  if (requiredSizeTrophiesBytes != nullptr) *requiredSizeTrophiesBytes = 0;
  return true;
}

void idAchievementSystem::SyncAchievementBits(idLocalUser* user) {
  if (user == nullptr) {
    return;
  }
  idPlayerProfile* profile = user->GetProfile();
  if (profile == nullptr) {
    return;
  }
  std::array<bool, 128> state{};
  if (!GetAchievementState(user, state)) {
    return;
  }
  for (int i = 0; i < static_cast<int>(state.size()); ++i) {
    if (state[static_cast<std::size_t>(i)]) {
      profile->SetAchievement(i);
    } else {
      profile->ClearAchievement(i);
    }
  }
}
