#include "engine/sys/win32/win_signin.h"

#include "shared/idlib/sys/win32_pc/win_utils.h"
#include "shared/idlib/text/base64.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace {
winSignInCallbacks_t signInCallbacks{};
}

void Win_SetSignInCallbacks(const winSignInCallbacks_t& callbacks) {
  signInCallbacks = callbacks;
}

idLocalUserWin& idLocalUserWin::operator=(const idLocalUserWin& other) {
  if (this == &other) return *this;
  joiningLobby[0] = other.joiningLobby[0]; joiningLobby[1] = other.joiningLobby[1];
  localUserHandle = other.localUserHandle;
  syncAchievementsRequested = other.syncAchievementsRequested;
  onlineWarningDisplayed = other.onlineWarningDisplayed;
  gamertag = other.gamertag; inputDevice = other.inputDevice;
  return *this;
}

void idSignInManagerWin::OnDLCInstalled(DlcInstalled_t* event) {
  if (event != nullptr && signInCallbacks.onDlcInstalled != nullptr) {
    signInCallbacks.onDlcInstalled(*event);
  }
}

int idSignInManagerWin::GetNumLocalUsers() const {
  return static_cast<int>(localUsers.size());
}

void idSignInManagerWin::RemoveLocalUserByIndex(int index) {
  if (index < 0 || index >= GetNumLocalUsers()) return;
  if (signInCallbacks.onLocalUserSignout != nullptr) {
    signInCallbacks.onLocalUserSignout(localUsers[index].get());
  }
  localUsers.erase(localUsers.begin() + index);
}

void idSignInManagerWin::EncodeUser(idLocalUser* localUser, idStr& outputBuffer) const {
  auto* winUser = dynamic_cast<idLocalUserWin*>(localUser);
  if (winUser == nullptr) {
    outputBuffer.Clear();
    return;
  }
  const winUserState_t state = winUser->GetUserState();
  idBase64 base64;
  base64.Encode(reinterpret_cast<const std::uint8_t*>(&state), sizeof(state));
  outputBuffer = reinterpret_cast<const char*>(base64.data);
  outputBuffer.ReplaceChar('+', '_');
}

bool idSignInManagerWin::CreateNewUserFromEncodedSwapData(const idStr& encodedSwapData) {
  idStr encoded = encodedSwapData;
  encoded.ReplaceChar('_', '+');
  idBase64 base64;
  base64.EnsureAlloced(encoded.Length() + 1);
  std::memcpy(base64.data, encoded.data, encoded.Length() + 1);
  base64.len = encoded.Length();
  std::array<std::uint8_t, 1024> decoded{};
  base64.Decode(decoded.data());
  winUserState_t state{};
  std::memcpy(&state, decoded.data(), sizeof(state));
  RemoveAllLocalUsers();
  RegisterLocalUser(state.inputDevice);
  if (localUsers.empty() || !localUsers.front()->VerifyUserState(&state)) {
    RemoveAllLocalUsers();
  }
  // Retail reports that the swap payload was consumed even when verification
  // removes the reconstructed user.
  return true;
}

void idSignInManagerWin::Pump() {
  while (localUsers.size() > 1 &&
         static_cast<int>(localUsers.size()) > maxDesiredLocalUsers) {
    RemoveLocalUserByIndex(static_cast<int>(localUsers.size()) - 1);
  }
  for (const auto& user : localUsers) user->Pump();
  UpdateLocalDlcInformation();
}

void idSignInManagerWin::RegisterLocalUser(int inputDevice) {
  if (GetLocalUserByInputDevice(inputDevice) != nullptr || localUsers.size() >= 4) return;
  const char* name = signInCallbacks.platformPersonaName != nullptr
                         ? signInCallbacks.platformPersonaName()
                         : Sys_GetMachineName();
  if (name == nullptr || name[0] == '\0') name = "Player";
  idStr truncatedName(name);
  const int byteLength = truncatedName.Length();
  if (idStr::IsValidUTF8(reinterpret_cast<const std::uint8_t*>(name), byteLength)) {
    truncatedName.Empty();
    int byteIndex = 0;
    for (int character = 0; character < 16 && byteIndex < byteLength; ++character)
      truncatedName.AppendUTF8Char(idStr::UTF8Char(
          reinterpret_cast<const std::uint8_t*>(name), &byteIndex));
  }
  auto user = std::make_unique<idLocalUserWin>();
  user->Init(inputDevice, truncatedName.data, static_cast<int>(localUsers.size()) + 1);
  user->localUserHandle = GetUniqueLocalUserHandle(user->GetGamerTag());
  idLocalUserWin* signedIn = user.get();
  localUsers.push_back(std::move(user));
  if (signInCallbacks.onLocalUserSignin != nullptr) signInCallbacks.onLocalUserSignin(signedIn);
}

void idSignInManagerWin::Shutdown() {
  while (!localUsers.empty()) RemoveLocalUserByIndex(static_cast<int>(localUsers.size()) - 1);
}
