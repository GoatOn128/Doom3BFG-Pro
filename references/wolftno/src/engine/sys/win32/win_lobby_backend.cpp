#include "engine/sys/win32/win_lobby_backend.h"

#include "engine/sys/sys_signin.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/networking/networking.h"

#include <algorithm>
#include <cstdio>

idCVar lob_freeSlotsOverride(
    "lob_freeSlotsOverride", "-1", CVAR_INTEGER,
    "Specifies how many free slots there are in your lobby");
idCVar net_debugSteamLobby(
    "net_debugSteamLobby", "0", CVAR_BOOL,
    "print the current Steam lobby state while pumping");

idLobbyBackendWin::idLobbyBackendWin(idLobbyToSessionCB* value) : callbacks(value) {}
idLobbyBackendWin::~idLobbyBackendWin() { Shutdown(); }
lobbyBackendState_t idLobbyBackendWin::GetState() const { return state; }

bool idLobbyBackendWin::UsesPlatformBackend() const {
  return (parms.matchFlags & 2U) != 0 && type != TYPE_GAME_STATE;
}
void idLobbyBackendWin::SetIsJoinable(bool value) {
  if (isJoinable != value) { isJoinable = value; commitIsJoinable = true; }
}
void idLobbyBackendWin::UpdateLobbySkill(float skill) {
  currentSkillLevel = static_cast<int>(skill); commitLobbySkill = true;
}
void idLobbyBackendWin::StartHosting(const idMatchParameters& parameters, float skill,
                                     lobbyBackendType_t backendType) {
  maxFreeSlotsOverride = -1; isHost = true; parms = parameters; type = backendType;
  UpdateLobbySkill(skill);
  if (!UsesPlatformBackend()) { isLocal = true; state = LOBBY_BACKEND_READY; return; }
  isLocal = false;
  steamLobby.reset(type == TYPE_PARTY ? idSteamLobby::CreateFriendsOnly(parameters.numSlots)
                                     : idSteamLobby::CreateInvisible(parameters.numSlots));
  if (steamLobby != nullptr) {
    if (type == TYPE_PARTY) steamLobby->SetIsParty();
    else if ((parameters.matchFlags & 8U) != 0) steamLobby->SetIsPrivateGame();
    else steamLobby->SetIsPublicGame();
    state = steamLobby->IsNormal() ? LOBBY_BACKEND_READY : LOBBY_BACKEND_CREATING;
  } else state = LOBBY_BACKEND_FAILED;
}
void idLobbyBackendWin::JoinFromConnectInfo(const lobbyConnectInfo_t& info) {
  isLocal = false; isHost = false;
  if (!info.usingBackendPlatform || type == TYPE_GAME_STATE) {
    if (callbacks != nullptr && callbacks->CanJoinLocalHost(parms, 0)) {
      Sys_StringToNetAdr("localhost", nonSteamAddr, true);
    } else {
      nonSteamAddr = info.netAddr;
    }
    state = LOBBY_BACKEND_READY;
    return;
  }
  lobbyOwner = info.platformId; lobbyOwnerIsValid = lobbyOwner != 0;
  steamLobby = std::make_unique<idSteamLobby>(); steamLobby->Join(lobbyOwner);
  state = steamLobby->IsNormal() ? LOBBY_BACKEND_READY : LOBBY_BACKEND_FAILED;
}
void idLobbyBackendWin::BecomeHost(int numInvites) {
  const idMatchParameters saved = parms; const lobbyBackendType_t savedType = type;
  Shutdown(); StartHosting(saved, static_cast<float>(currentSkillLevel), savedType);
  maxFreeSlotsOverride = saved.numSlots - numInvites - 1; commitMatchParmsToSteam = true;
}
void idLobbyBackendWin::FinishBecomeHost() { maxFreeSlotsOverride = -1; commitMatchParmsToSteam = true; }
void idLobbyBackendWin::GetOwnerAddress(lobbyAddress_t& output) {
  if (!UsesPlatformBackend()) { output.InitFromNetadr(nonSteamAddr); state = LOBBY_BACKEND_READY; return; }
  if (!lobbyOwnerIsValid) { state = LOBBY_BACKEND_FAILED; return; }
  output.usingBackendPlatform = true; output.platformId = lobbyOwner; state = LOBBY_BACKEND_READY;
}
lobbyConnectInfo_t idLobbyBackendWin::GetConnectInfo() {
  lobbyConnectInfo_t result;
  if (!UsesPlatformBackend()) {
    if (IsHost()) { Sys_StringToNetAdr(Sys_GetLocalIP(0), nonSteamAddr, false); }
    result.usingBackendPlatform = false; result.netAddr = nonSteamAddr;
  } else if (steamLobby != nullptr) {
    result.usingBackendPlatform = true; result.platformId = steamLobby->Id();
  }
  return result;
}
bool idLobbyBackendWin::IsOwnerOfConnectInfo(const lobbyConnectInfo_t& info) const {
  if (info.usingBackendPlatform != UsesPlatformBackend()) return false;
  if (UsesPlatformBackend()) return steamLobby != nullptr && info.platformId == steamLobby->Id();
  return Sys_CompareNetAdrBase(nonSteamAddr, info.netAddr);
}
void idLobbyBackendWin::SetInGame(bool value) {
  if (UsesPlatformBackend() && inGame != value) { inGame = value; commitMatchParmsToSteam = true; }
}
int idLobbyBackendWin::GetGameLobbyFreeSlots() const {
  const idLobbyBackendWin* source = this;
  if (type == TYPE_PARTY && callbacks != nullptr) {
    source = dynamic_cast<const idLobbyBackendWin*>(callbacks->GetLobbyBackend(TYPE_GAME));
    if (source == nullptr || source->type == TYPE_PARTY) source = this;
  }
  const int available = (std::max)(0, source->parms.numSlots - static_cast<int>(source->allUsers.size()));
  if (source->maxFreeSlotsOverride >= 0) {
    return (std::min)(available, source->maxFreeSlotsOverride);
  }
  return lob_freeSlotsOverride.GetInteger() == -1
             ? available
             : (std::max)(0, lob_freeSlotsOverride.GetInteger());
}
void idLobbyBackendWin::ClearSearchResults() { lobbySearchResults.clear(); }
void idLobbyBackendWin::Shutdown() {
  steamLobby.reset(); steamLobbySearch.reset(); ClearSearchResults(); state = LOBBY_BACKEND_SHUTDOWN;
}
void idLobbyBackendWin::SetLobbySkill() {
  if (steamLobby == nullptr) return; char value[16]{};
  std::snprintf(value, sizeof(value), "%d", currentSkillLevel); steamLobby->SetProperty("skill", value);
}
void idLobbyBackendWin::UnregisterUser(lobbyUser_t* user, bool) {
  allUsers.erase(std::remove(allUsers.begin(), allUsers.end(), user), allUsers.end());
  commitMatchParmsToSteam = true;
  if (type == TYPE_GAME && callbacks != nullptr) {
    auto* party = dynamic_cast<idLobbyBackendWin*>(callbacks->GetLobbyBackend(TYPE_PARTY));
    if (party != nullptr) party->commitMatchParmsToSteam = true;
  }
}
void idLobbyBackendWin::StartFinding(const idMatchParameters& parameters, unsigned numPartyUsers,
                                     float skill) {
  parms = parameters; isLocal = (parameters.matchFlags & 2U) == 0; isHost = false;
  ClearSearchResults();
  if (!UsesPlatformBackend()) {
    if (callbacks != nullptr && callbacks->CanJoinLocalHost(parameters, static_cast<int>(numPartyUsers))) {
      lobbySearchResults.push_back(0); state = LOBBY_BACKEND_READY;
    } else state = LOBBY_BACKEND_FAILED;
    return;
  }
  steamLobbySearch = std::make_unique<idSteamLobbySearch>();
  steamLobbySearch->Search(&parameters, numPartyUsers, static_cast<unsigned>(skill));
  for (int index = 0; index < steamLobbySearch->NumServers(); ++index)
    lobbySearchResults.push_back(steamLobbySearch->ServerAtIndex(index));
  state = LOBBY_BACKEND_READY;
}
void idLobbyBackendWin::GetSearchResults(std::vector<lobbyConnectInfo_t>& output) const {
  output.clear();
  for (std::uint64_t id : lobbySearchResults) {
    lobbyConnectInfo_t info; info.usingBackendPlatform = UsesPlatformBackend(); info.platformId = id;
    output.push_back(info);
  }
}
void idLobbyBackendWin::Pump() {
  if (steamLobby != nullptr) {
    if (net_debugSteamLobby.GetBool()) steamLobby->DebugReport();
    if (steamLobby->IsDisconnected()) { state = LOBBY_BACKEND_FAILED; steamLobby.reset(); return; }
    if (steamLobby->IsNormal()) state = LOBBY_BACKEND_READY;
  }
  if (state == LOBBY_BACKEND_READY && steamLobby != nullptr) {
    if (commitIsJoinable) { steamLobby->SetIsJoinable(isJoinable); commitIsJoinable = false; }
    if (commitLobbySkill) { SetLobbySkill(); commitLobbySkill = false; }
    if (commitMatchParmsToSteam) {
      steamLobby->UpdateMatchParameters(parms, GetGameLobbyFreeSlots(), 1);
      if (type == TYPE_PARTY) steamLobby->SetLobbyPrivacyType((parms.matchFlags & 0x10U) != 0);
      commitMatchParmsToSteam = false;
    }
  }
}
void idLobbyBackendWin::UpdateMatchParms(const idMatchParameters& parameters) {
  parms = parameters; commitMatchParmsToSteam = true;
}
void idLobbyBackendWin::RegisterUser(lobbyUser_t* user, bool) {
  if (user != nullptr && std::find(allUsers.begin(), allUsers.end(), user) == allUsers.end())
    allUsers.push_back(user);
  commitMatchParmsToSteam = true;
  if (type == TYPE_GAME && callbacks != nullptr) {
    auto* party = dynamic_cast<idLobbyBackendWin*>(callbacks->GetLobbyBackend(TYPE_PARTY));
    if (party != nullptr) party->commitMatchParmsToSteam = true;
  }
}
