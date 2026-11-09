#include "engine/sys/sys_localuser.h"

#include <algorithm>
#include <windows.h>

namespace {
bool ValidStatIndex(int index) { return index >= 0 && index < 500; }
}

void idPlayerProfile::SetAchievement(int index) {
  if (index >= 0 && index < 128) {
    achievementBits[static_cast<std::size_t>(index / 64)] |= std::uint64_t{1} << (index & 63);
  }
}

void idPlayerProfile::ClearAchievement(int index) {
  if (index >= 0 && index < 128) {
    achievementBits[static_cast<std::size_t>(index / 64)] &= ~(std::uint64_t{1} << (index & 63));
  }
}

bool idPlayerProfile::HasAchievement(int index) const {
  return index >= 0 && index < 128 &&
         (achievementBits[static_cast<std::size_t>(index / 64)] &
          (std::uint64_t{1} << (index & 63))) != 0;
}

void idPlayerProfile::StatSetInt(int index, int value) {
  if (ValidStatIndex(index)) integerStats[static_cast<std::size_t>(index)] = value;
}
void idPlayerProfile::StatSetFloat(int index, float value) {
  if (ValidStatIndex(index)) floatStats[static_cast<std::size_t>(index)] = value;
}
int idPlayerProfile::StatGetInt(int index) const {
  return ValidStatIndex(index) ? integerStats[static_cast<std::size_t>(index)] : 0;
}
float idPlayerProfile::StatGetFloat(int index) const {
  return ValidStatIndex(index) ? floatStats[static_cast<std::size_t>(index)] : 0.0f;
}

idLocalUser::idLocalUser() { profileMgr.Init(this); }
idLocalUser::~idLocalUser() = default;

bool idLocalUser::IsPersistent() { return IsProfileReady(); }

void idLocalUser::Pump() {
  profileMgr.Pump();
  PumpPlatform();
}

bool idLocalUser::IsStorageDeviceAvailable() { return true; }

bool idLocalUser::StorageSizeAvailable(std::uint64_t minimumBytes, std::int64_t* neededBytes) {
  ULARGE_INTEGER available{};
  const bool queried = GetDiskFreeSpaceExW(nullptr, &available, nullptr, nullptr) != FALSE;
  const std::uint64_t freeBytes = queried ? available.QuadPart : 0;
  const std::uint64_t shortage = minimumBytes > freeBytes ? minimumBytes - freeBytes : 0;
  if (neededBytes != nullptr) {
    *neededBytes = static_cast<std::int64_t>(shortage);
  }
  return queried && shortage == 0;
}

void idLocalUser::SetStatInt(int stat, int value) {
  if (idPlayerProfile* profile = GetProfile()) profile->StatSetInt(stat, value);
}
void idLocalUser::SetStatFloat(int stat, float value) {
  if (idPlayerProfile* profile = GetProfile()) profile->StatSetFloat(stat, value);
}
int idLocalUser::GetStatInt(int stat) {
  const idPlayerProfile* profile = GetProfile();
  return profile != nullptr ? profile->StatGetInt(stat) : 0;
}
float idLocalUser::GetStatFloat(int stat) {
  const idPlayerProfile* profile = GetProfile();
  return profile != nullptr ? profile->StatGetFloat(stat) : 0.0f;
}

void idLocalUser::LoadProfileSettings() { profileMgr.LoadSettingsAsync(); }
void idLocalUser::SaveProfileSettings() { profileMgr.SaveSettingsAsync(); }
idPlayerProfile* idLocalUser::GetProfile() { return profileMgr.GetProfile(); }

