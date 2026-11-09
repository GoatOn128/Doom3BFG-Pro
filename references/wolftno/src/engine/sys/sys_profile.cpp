#include "engine/sys/sys_localuser.h"

#include "engine/framework/file_savegame.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"

idCVar profile_verbose("profile_verbose", "0", CVAR_BOOL, "Print profile save/load diagnostics");

idFile_SaveGame::idFile_SaveGame() = default;
idFile_SaveGame::idFile_SaveGame(const char* fileName) : idFile_Memory(fileName) {}
idFile_SaveGame::idFile_SaveGame(const char* fileName, int fileType)
    : idFile_Memory(fileName), type(fileType) {}

void idFile_SaveGame::Clear(bool freeMemory) {
  error = false;
  idFile_Memory::Clear(freeMemory);
}

void idFile_SaveGame::SetNameAndType(const char* newName, int newType) {
  name = newName != nullptr ? newName : "";
  type = newType;
}

bool idFile_SaveGame::operator==(const idFile_SaveGame& other) const {
  return idStr::Icmp(GetName(), other.GetName()) == 0;
}

bool idFile_SaveGame::operator==(const char* otherName) const {
  return idStr::Icmp(GetName(), otherName != nullptr ? otherName : "") == 0;
}

idProfileMgr::idProfileMgr() = default;

idProfileMgr::~idProfileMgr() {
  if (ownsProfile) delete profile;
}

void idProfileMgr::Init(idLocalUser* value) {
  owner = value;
}

void idProfileMgr::Pump() {
  // The retail manager waits for an asynchronous save-game processor. The
  // standalone recovery completes the local operation in the initiating call.
}

idPlayerProfile* idProfileMgr::GetProfile() {
  if (profile == nullptr && owner != nullptr) {
    profile = new idPlayerProfile();
    ownsProfile = true;
  }
  return profile != nullptr && profile->state != idPlayerProfile::LOADING ? profile : nullptr;
}

const idPlayerProfile* idProfileMgr::GetProfile() const {
  return profile != nullptr && profile->state != idPlayerProfile::LOADING ? profile : nullptr;
}

void idProfileMgr::SetProfile(idPlayerProfile* value) {
  if (ownsProfile) delete profile;
  profile = value;
  ownsProfile = false;
}

bool idProfileMgr::LoadSettingsAsync() {
  if (profile == nullptr) GetProfile();
  if (profile == nullptr) return false;
  profile->state = idPlayerProfile::LOADING;
  profile->state = idPlayerProfile::IDLE;
  return true;
}

bool idProfileMgr::SaveSettingsAsync() {
  idPlayerProfile* current = GetProfile();
  if (current == nullptr) return false;
  current->state = idPlayerProfile::SAVING;
  current->state = idPlayerProfile::IDLE;
  return true;
}

bool Sys_SaveGameProfileCheck() { return false; }
