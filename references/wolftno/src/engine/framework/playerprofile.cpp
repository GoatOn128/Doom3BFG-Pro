#include "engine/framework/playerprofile.h"

idPlayerProfile::idPlayerProfile() {
  SetDefaultsImpl();
  state = IDLE;
  requestedState = IDLE;
  deviceNum = -1;
  clearExistingOnWrite = false;
  resetFileOwner = false;
  mouseXFrac = 0.0f;
  mouseYFrac = 0.0f;
}

idPlayerProfile::~idPlayerProfile() = default;

void idPlayerProfile::SetAchievement(int id) {
  if (id >= 128)
    return;
  if (id >= 64)
    achievementBits2 |= std::uint64_t{1} <<
                        ((static_cast<unsigned>(id) - 64u) & 63u);
  else
    achievementBits |= std::uint64_t{1} <<
                       (static_cast<unsigned>(id) & 63u);
}

void idPlayerProfile::ClearAchievement(int id) {
  if (id >= 128)
    return;
  if (id >= 64)
    achievementBits2 &= ~(std::uint64_t{1} <<
                          ((static_cast<unsigned>(id) - 64u) & 63u));
  else
    achievementBits &= ~(std::uint64_t{1} <<
                         (static_cast<unsigned>(id) & 63u));
}

bool idPlayerProfile::GetAchievement(int id) const {
  if (id >= 128)
    return false;
  if (id >= 64)
    return ((achievementBits2 >>
             ((static_cast<unsigned>(id) - 64u) & 63u)) & 1u) != 0;
  return ((achievementBits >> (static_cast<unsigned>(id) & 63u)) & 1u) != 0;
}

void idPlayerProfile::StatSetInt(int stat, int value) {
  stats.list[stat].i = value;
}

void idPlayerProfile::StatSetFloat(int stat, float value) {
  stats.list[stat].f = value;
}

int idPlayerProfile::StatGetInt(int stat) const { return stats.list[stat].i; }

float idPlayerProfile::StatGetFloat(int stat) const { return stats.list[stat].f; }

void idPlayerProfile::SaveSettings(bool) {
  if (state != SAVING && requestedState == IDLE)
    requestedState = SAVE_REQUESTED;
}

void idPlayerProfile::LoadSettings() {
  if (hasUser && state != LOADING && requestedState == IDLE)
    requestedState = LOAD_REQUESTED;
}

void idPlayerProfile::SetDefaultsImpl() {
  hasUser = true;
  achievementBits = 0;
  achievementBits2 = 0;
  dlcReleaseVersion = 0;
  gameModeType = 0;
  stats.SetNum(MAX_PLAYER_PROFILE_STATS);
  for (int index = 0; index < MAX_PLAYER_PROFILE_STATS; ++index)
    stats.list[index].i = 0;
  seenDLCItems.Clear();
}
