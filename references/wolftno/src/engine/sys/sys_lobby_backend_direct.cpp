#include "engine/sys/sys_lobby_backend_direct.h"

#include "shared/idlib/networking/networking.h"

idLobbyBackend::~idLobbyBackend() = default;
bool idLobbyBackend::IsHost() const { return isHost; }
bool idLobbyBackend::IsLocal() const { return isLocal; }
bool idLobbyBackend::IsOnline() const { return !isLocal; }

idLobbyBackendDirect::idLobbyBackendDirect(idLobbyToSessionCB* value) : callbacks(value) {}

void idLobbyBackendDirect::JoinFromConnectInfo(const lobbyConnectInfo_t& connectInfo) {
  address = connectInfo.netAddr;
  state = LOBBY_BACKEND_READY;
  isLocal = false;
  isHost = false;
}

void idLobbyBackendDirect::Shutdown() { state = LOBBY_BACKEND_SHUTDOWN; }
void idLobbyBackendDirect::FinishBecomeHost() { isHost = true; }

void idLobbyBackendDirect::GetOwnerAddress(lobbyAddress_t& output) {
  output.InitFromNetadr(address);
  state = LOBBY_BACKEND_READY;
}

bool idLobbyBackendDirect::IsOwnerOfConnectInfo(const lobbyConnectInfo_t& connectInfo) const {
  return !connectInfo.usingBackendPlatform && Sys_CompareNetAdrBase(address, connectInfo.netAddr);
}

void idLobbyBackendDirect::StartHosting(const idMatchParameters& parameters, float,
                                        lobbyBackendType_t backendType) {
  parms = parameters;
  type = backendType;
  state = LOBBY_BACKEND_READY;
  isLocal = true;
  isHost = true;
}

void idLobbyBackendDirect::StartFinding(const idMatchParameters& parameters, int numPartyUsers,
                                        float) {
  parms = parameters;
  isHost = false;
  isLocal = (parameters.matchFlags & 2U) == 0;
  state = callbacks != nullptr && callbacks->CanJoinLocalHost(parameters, numPartyUsers)
              ? LOBBY_BACKEND_READY
              : LOBBY_BACKEND_FAILED;
}

lobbyConnectInfo_t idLobbyBackendDirect::GetConnectInfo() {
  if (IsHost()) {
    Sys_StringToNetAdr(Sys_GetLocalIP(0), address, false);
    address.port = boundPort;
  }
  lobbyConnectInfo_t result;
  result.usingBackendPlatform = false;
  result.netAddr = address;
  return result;
}

void idLobbyBackendDirect::GetSearchResults(
    std::vector<lobbyConnectInfo_t>& searchResults) const {
  searchResults.clear();
  searchResults.emplace_back();
}

