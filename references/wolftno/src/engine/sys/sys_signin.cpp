#include "engine/sys/sys_signin.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/hashing/md.h"
#include "shared/idlib/sys/sys_time.h"

#include <cstdint>
#include <cstring>

idCVar com_requireNonProductionSignIn(
    "com_requireNonProductionSignIn", "1", CVAR_BOOL | CVAR_ARCHIVE,
    "If true, require sign-in even on non-production builds.");

namespace {
idSignInManagerBase* activeSignInManager = nullptr;
}

idSignInManagerBase* Sys_GetActiveSignInManager() { return activeSignInManager; }
void Sys_SetActiveSignInManager(idSignInManagerBase* manager) { activeSignInManager = manager; }

idSignInManagerBase::~idSignInManagerBase() { delete defaultProfile; }

bool idSignInManagerBase::ProcessInputEvent(const sysEvent_t* event) {
  if (event == nullptr || GetNumLocalUsers() >= maxDesiredLocalUsers ||
      IsAnyDeviceBeingRegistered() || (event->evValue2 & 1) == 0) {
    return false;
  }
  constexpr int kJoy1 = 0x100;
  constexpr int kJoy9 = 0x108;
  constexpr int kEnter = 0x1c;
  constexpr int kKeypadEnter = 0x9c;
  const int primaryJoyKey = IsDialogActive() ? kJoy1 + 1 : kJoy1;
  if (event->evValue != primaryJoyKey && event->evValue != kJoy9 &&
      event->evValue != kEnter && event->evValue != kKeypadEnter) {
    return false;
  }
  if (IsCreditsInputLocked() || GetLocalUserByInputDevice(event->inputDevice) != nullptr) {
    return false;
  }
  RegisterLocalUser(event->inputDevice);
  return true;
}

idPlayerProfile* idSignInManagerBase::GetDefaultProfile() {
  if (defaultProfile == nullptr) defaultProfile = new idPlayerProfile();
  return defaultProfile;
}

idLocalUser* idSignInManagerBase::GetLocalUserByInputDevice(int inputDevice) {
  for (int i = 0; i < GetNumLocalUsers(); ++i) {
    idLocalUser* user = GetLocalUserByIndex(i);
    if (user != nullptr && user->GetInputDevice() == inputDevice) return user;
  }
  return nullptr;
}

idPlayerProfile* idSignInManagerBase::GetPlayerProfileByInputDevice(int inputDevice) {
  idLocalUser* user = GetLocalUserByInputDevice(inputDevice);
  return user != nullptr ? user->GetProfile() : nullptr;
}

idLocalUser* idSignInManagerBase::GetLocalUserByHandle(localUserHandle_t handle) {
  for (int i = 0; i < GetNumLocalUsers(); ++i) {
    idLocalUser* user = GetLocalUserByIndex(i);
    if (user != nullptr && user->GetLocalUserHandle() == handle) return user;
  }
  return nullptr;
}

bool idSignInManagerBase::RemoveLocalUserByHandle(localUserHandle_t handle) {
  for (int i = 0; i < GetNumLocalUsers(); ++i) {
    idLocalUser* user = GetLocalUserByIndex(i);
    if (user != nullptr && user->GetLocalUserHandle() == handle) {
      RemoveLocalUserByIndex(i);
      return true;
    }
  }
  return false;
}

void idSignInManagerBase::RemoveAllLocalUsers() {
  while (GetNumLocalUsers() > 0) RemoveLocalUserByIndex(0);
}

void idSignInManagerBase::ValidateLocalUsers(bool requireOnline) {
  for (int i = GetNumLocalUsers() - 1; i >= 0; --i) {
    idLocalUser* user = GetLocalUserByIndex(i);
    if (user == nullptr || !user->IsProfileReady() || (requireOnline && !user->IsOnline())) {
      RemoveLocalUserByIndex(i);
    }
  }
}

localUserHandle_t idSignInManagerBase::GetUniqueLocalUserHandle(const char* name) {
  MD5_CTX context{};
  std::uint8_t digest[16]{};
  const std::int64_t ticks = Sys_GetClockTicks();
  MD5_Init(&context);
  if (name != nullptr) MD5_Update(&context, reinterpret_cast<const std::uint8_t*>(name), std::strlen(name));
  MD5_Update(&context, reinterpret_cast<const std::uint8_t*>(&ticks), sizeof(ticks));
  MD5_Final(&context, digest);
  std::uint32_t handle = 0;
  for (int i = 0; i < 4; ++i) {
    const int offset = i * 4;
    handle ^= static_cast<std::uint32_t>(digest[offset]) |
              (static_cast<std::uint32_t>(digest[offset + 1]) << 8U) |
              (static_cast<std::uint32_t>(digest[offset + 2]) << 16U) |
              (static_cast<std::uint32_t>(digest[offset + 3]) << 24U);
  }
  return {handle};
}

void idSignInManagerBase::UpdateLocalDlcInformation() {
  if (dlcVersionChecked || GetNumLocalUsers() <= 0 || !IsTitleStorageLoaded()) return;
  idLocalUser* user = GetLocalUserByIndex(0);
  idPlayerProfile* profile = user != nullptr ? user->GetProfile() : nullptr;
  if (profile == nullptr || profile->state != idPlayerProfile::IDLE) return;
  const int releaseVersion = GetTitleStorageDlcReleaseVersion();
  if (releaseVersion > profile->dlcReleaseVersion) {
    SetMarketplaceHasNewContent(true);
    ShowNewContentAvailable();
    profile->dlcReleaseVersion = releaseVersion;
    user->SaveProfileSettings();
  }
  dlcVersionChecked = true;
}
