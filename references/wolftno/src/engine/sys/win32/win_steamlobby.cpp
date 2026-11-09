#include "engine/sys/win32/win_steamlobby.h"

#include "engine/sys/win32/win_steamutil.h"
#include "shared/idlib/lib.h"

#include <atomic>
#include <cstdlib>

namespace {
idSteamLobbyPlatform* steamLobbyPlatform = nullptr;
std::atomic_uint64_t nextLocalLobbyId{1};
constexpr int kResultOK = 1;
constexpr const char* kLobbyType = "lobbyType";

const char* TypeString(lobbyType_t type) {
  switch (type) {
    case LOBBY_TYPE_PARTY: return "party";
    case LOBBY_TYPE_GAME_PUBLIC: return "gamePublic";
    case LOBBY_TYPE_GAME_PRIVATE: return "gamePrivate";
    default: return "";
  }
}
}

void Win_SetSteamLobbyPlatform(idSteamLobbyPlatform* platform) { steamLobbyPlatform = platform; }

lobbyDisconnectReason_t EChatRoomEnterResponseToDisconnectReason(unsigned response) {
  switch (response) {
    case 1: return LOBBY_DISCONNECTED_UNKNOWN;
    case 2: return LOBBY_DISCONNECTED_NOTFOUND;
    case 3: return LOBBY_DISCONNECTED_ACCESS_DENIED;
    case 4: return LOBBY_DISCONNECTED_FULL;
    case 5: return LOBBY_DISCONNECTED_UNKNOWN;
    case 6: return LOBBY_DISCONNECTED_BANNED;
    default: return LOBBY_DISCONNECTED_UNKNOWN;
  }
}

idSteamLobby::idSteamLobby() = default;
idSteamLobby::~idSteamLobby() { LeaveLobby(); }
bool idSteamLobby::IsDisconnected() const { return state == LOBBY_DISCONNECTED; }
bool idSteamLobby::IsCreating() const { return state == LOBBY_CREATING; }
bool idSteamLobby::IsJoining() const { return state == LOBBY_JOINING; }
bool idSteamLobby::IsNormal() const { return state == LOBBY_NORMAL; }
std::uint64_t idSteamLobby::Id() const { return lobbyId; }

std::uint64_t idSteamLobby::OwnerId() const {
  return steamLobbyPlatform != nullptr ? steamLobbyPlatform->OwnerId(lobbyId) : (lobbyId != 0 ? 1 : 0);
}

const char* idSteamLobby::DisconnectReasonString() const {
  static const char* names[] = {"LOBBY_NEVER_CONNECTED", "LOBBY_NOT_DISCONNECTED",
    "LOBBY_DISCONNECTED_KICKED", "LOBBY_DISCONNECTED_BANNED",
    "LOBBY_DISCONNECTED_QUIT_NORMALLY", "LOBBY_DISCONNECTED_TIMEOUT",
    "LOBBY_DISCONNECTED_NOTFOUND", "LOBBY_DISCONNECTED_FULL",
    "LOBBY_DISCONNECTED_ACCESS_DENIED", "LOBBY_DISCONNECTED_STEAM_CLIENT_CRASHED",
    "LOBBY_DISCONNECTED_STEAM_UNAVAILABLE", "LOBBY_DISCONNECTED_TOO_MANY_LOBBIES"};
  if (state != LOBBY_DISCONNECTED) return names[LOBBY_NOT_DISCONNECTED];
  return disconnectReason < LOBBY_DISCONNECTED_UNKNOWN ? names[disconnectReason]
                                                        : "LOBBY_DISCONNECTED_UNKNOWN";
}

void idSteamLobby::SetLobbyPrivacyType(bool value) { inviteOnly = value; }
bool idSteamLobby::IsOwnedByLocalPlayer() const {
  if (!IsLobbyValid(lobbyId)) return false;
  const std::uint64_t local = steamLobbyPlatform != nullptr ? steamLobbyPlatform->LocalUserId() : 1;
  return OwnerId() == local;
}
void idSteamLobby::ChangeLobbyId(std::uint64_t id) {
  if (IsLobbyValid(lobbyId)) idLib::Warning("Changing lobby id when lobby is already valid");
  lobbyId = id;
}
int idSteamLobby::GetNumSteamLobbyMembers() const {
  return steamLobbyPlatform != nullptr ? steamLobbyPlatform->MemberCount(lobbyId) : (lobbyId != 0 ? 1 : 0);
}
bool idSteamLobby::CanChangeProperties() const { return IsLobbyValid(lobbyId) && IsOwnedByLocalPlayer(); }

void idSteamLobby::OnLobbyDataUpdate(LobbyDataUpdate_t* update) { (void)update; }
void idSteamLobby::OnLobbyChatUpdate(LobbyChatUpdate_t* update) { (void)update; }
void idSteamLobby::OnLobbyChatMsg(LobbyChatMsg_t* message) {
  if (message != nullptr && message->lobbyId == lobbyId && state != LOBBY_NORMAL)
    idLib::Warning("Lobby chat message received outside normal state");
}
void idSteamLobby::OnLobbyGameCreated(LobbyGameCreated_t* event) {
  if (event == nullptr || event->lobbyId != lobbyId || state != LOBBY_NORMAL) return;
  gameServer.lobbyId = event->lobbyId;
  gameServer.gameServerId = event->gameServerId;
  gameServer.netadr.type = NA_IP;
  gameServer.netadr.port = event->port;
  idSteamUtil::SteamIpToNetadrIp(event->ip, gameServer.netadr.ip);
  wasGameServerChanged = true;
}
void idSteamLobby::DebugReport() const {
  idLib::Printf("Steam lobby %llx state=%d type=%d members=%d joinable=%d\n",
                static_cast<unsigned long long>(lobbyId), state, type,
                GetNumSteamLobbyMembers(), joinable);
}
void idSteamLobby::MakeJoinable() {
  if (joinable == LOBBY_JOINABLE || !CanChangeProperties()) return;
  if (steamLobbyPlatform == nullptr || steamLobbyPlatform->SetJoinable(lobbyId, true))
    joinable = LOBBY_JOINABLE;
}
void idSteamLobby::MakeNotJoinable() {
  if (joinable == LOBBY_NOT_JOINABLE || !CanChangeProperties()) return;
  if (steamLobbyPlatform == nullptr || steamLobbyPlatform->SetJoinable(lobbyId, false))
    joinable = LOBBY_NOT_JOINABLE;
}
void idSteamLobby::SetProperty(const char* key, const char* value) {
  if (!CanChangeProperties() || key == nullptr || value == nullptr) return;
  if (steamLobbyPlatform == nullptr || steamLobbyPlatform->SetProperty(lobbyId, key, value))
    localProperties[key] = value;
}
void idSteamLobby::DeleteProperty(const char* key) {
  if (!CanChangeProperties() || key == nullptr) return;
  if (steamLobbyPlatform == nullptr || steamLobbyPlatform->DeleteProperty(lobbyId, key))
    localProperties.erase(key);
}
const char* idSteamLobby::GetProperty(const char* key) const {
  if (key == nullptr) return "";
  if (steamLobbyPlatform != nullptr) {
    const char* value = steamLobbyPlatform->GetProperty(lobbyId, key);
    if (value != nullptr) return value;
  }
  const auto found = localProperties.find(key);
  return found != localProperties.end() ? found->second.c_str() : "";
}
void idSteamLobby::WriteLobbyType() { if (type != LOBBY_TYPE_UNSPECIFIED) SetProperty(kLobbyType, TypeString(type)); }
void idSteamLobby::UpdateMatchParameters(const idMatchParameters& p, int freeSlots, int version) {
  idGameSpecificMatchmakingParameters::UpdateMatchParameters(*this, p, freeSlots, version);
}
void idSteamLobby::SetIsJoinable(bool value) { value ? MakeJoinable() : MakeNotJoinable(); }
void idSteamLobby::OnLobbyCreatedOk() {
  state = LOBBY_NORMAL;
  disconnectReason = LOBBY_NOT_DISCONNECTED;
  const char* name = steamLobbyPlatform != nullptr ? steamLobbyPlatform->LocalPersonaName() : "Player";
  SetProperty("name", name != nullptr ? name : "Player");
  WriteLobbyType();
}
void idSteamLobby::LeaveLobby() {
  if (IsLobbyValid(lobbyId) && steamLobbyPlatform != nullptr) steamLobbyPlatform->LeaveLobby(lobbyId);
  lobbyId = 0; type = LOBBY_TYPE_UNSPECIFIED; joinable = LOBBY_NOT_JOINABLE;
}
void idSteamLobby::SetIsParty() { type = LOBBY_TYPE_PARTY; WriteLobbyType(); }
void idSteamLobby::SetIsPublicGame() { type = LOBBY_TYPE_GAME_PUBLIC; WriteLobbyType(); }
void idSteamLobby::SetIsPrivateGame() { type = LOBBY_TYPE_GAME_PRIVATE; WriteLobbyType(); }
void idSteamLobby::OnLobbyKickedLocalPlayer(LobbyKicked_t* event) {
  if (event == nullptr || event->lobbyId != lobbyId || state != LOBBY_NORMAL) return;
  LeaveLobby(); state = LOBBY_DISCONNECTED; disconnectReason = LOBBY_DISCONNECTED_KICKED;
}
void idSteamLobby::OnLobbyCreated(LobbyCreated_t* event, bool ioFailure) {
  if (state != LOBBY_CREATING || ioFailure || event == nullptr || event->result != kResultOK) {
    state = LOBBY_DISCONNECTED;
    disconnectReason = ioFailure ? LOBBY_DISCONNECTED_STEAM_UNAVAILABLE : LOBBY_DISCONNECTED_UNKNOWN;
    return;
  }
  ChangeLobbyId(event->lobbyId);
  OnLobbyCreatedOk();
}
[[noreturn]] void idSteamLobby::LeaveLobbyAndThrowError() {
  LeaveLobby(); state = LOBBY_DISCONNECTED; disconnectReason = LOBBY_DISCONNECTED_UNKNOWN;
  idLib::Error("Unknown lobby type in idSteamLobby::ReadLobbyType");
}
void idSteamLobby::ReadLobbyType() {
  const char* value = GetProperty(kLobbyType);
  if (_stricmp(value, "party") == 0) type = LOBBY_TYPE_PARTY;
  else if (_stricmp(value, "gamePublic") == 0) type = LOBBY_TYPE_GAME_PUBLIC;
  else if (_stricmp(value, "gamePrivate") == 0) type = LOBBY_TYPE_GAME_PRIVATE;
  else LeaveLobbyAndThrowError();
}
void idSteamLobby::Create(unsigned privacyType, unsigned slots) {
  state = LOBBY_CREATING; wasJoined = false;
  const std::uint64_t id = steamLobbyPlatform != nullptr
                               ? steamLobbyPlatform->CreateLobby(privacyType, slots)
                               : nextLocalLobbyId.fetch_add(1);
  LobbyCreated_t event{id != 0 ? kResultOK : 2, id};
  OnLobbyCreated(&event, id == 0);
}
idSteamLobby* idSteamLobby::CreateFriendsOnly(unsigned slots) {
  auto* lobby = new idSteamLobby(); lobby->Create(1, slots); return lobby;
}
idSteamLobby* idSteamLobby::CreateInvisible(unsigned slots) {
  auto* lobby = new idSteamLobby(); lobby->Create(3, slots); return lobby;
}
void idSteamLobby::OnLobbyEntered(LobbyEnter_t* event, bool ioFailure) {
  if (state != LOBBY_JOINING || ioFailure || event == nullptr || event->response != 1 ||
      !IsLobbyValid(lobbyId)) {
    lobbyId = 0; state = LOBBY_DISCONNECTED;
    disconnectReason = event != nullptr ? EChatRoomEnterResponseToDisconnectReason(event->response)
                                        : LOBBY_DISCONNECTED_STEAM_UNAVAILABLE;
    return;
  }
  state = LOBBY_NORMAL; wasJoined = true; disconnectReason = LOBBY_NOT_DISCONNECTED; ReadLobbyType();
}
void idSteamLobby::Join(std::uint64_t id) {
  state = LOBBY_JOINING; ChangeLobbyId(id);
  const bool joined = steamLobbyPlatform == nullptr || steamLobbyPlatform->JoinLobby(id);
  LobbyEnter_t event{id, 0, false, joined ? 1U : 2U};
  OnLobbyEntered(&event, !joined);
}

void idGameSpecificMatchmakingParameters::ApplyMatchParametersAsSearchFilters(const idMatchParameters&) {}
void idGameSpecificMatchmakingParameters::UpdateMatchParameters(idSteamLobby& lobby,
    const idMatchParameters& p, int freeSlots, int version) {
  char value[32]{};
  std::snprintf(value, sizeof(value), "%d", p.gameType); lobby.SetProperty("gameType", value);
  std::snprintf(value, sizeof(value), "%d", freeSlots); lobby.SetProperty("freeSlots", value);
  std::snprintf(value, sizeof(value), "%d", version); lobby.SetProperty("version", value);
  std::snprintf(value, sizeof(value), "%d", (p.matchFlags & 8) != 0); lobby.SetProperty("private", value);
  if (p.gameType != 0) {
    std::snprintf(value, sizeof(value), "%d", p.gameMode); lobby.SetProperty("gameMode", value);
    std::snprintf(value, sizeof(value), "%d", p.searchMode); lobby.SetProperty("searchMode", value);
    std::snprintf(value, sizeof(value), "%d", p.gameMap); lobby.SetProperty("gameMap", value);
  } else {
    lobby.DeleteProperty("gameMode"); lobby.DeleteProperty("searchMode"); lobby.DeleteProperty("gameMap");
  }
}
