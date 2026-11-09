#pragma once

#include "engine/sys/sys_lobby_types.h"

#include <vector>

class idLobbyToSessionCB {
public:
  virtual ~idLobbyToSessionCB() = default;
  virtual class idLobbyBackend* GetLobbyBackend(int type) { (void)type; return nullptr; }
  virtual bool CanJoinLocalHost(const idMatchParameters& parameters, int numPartyUsers) = 0;
};

enum lobbyBackendState_t {
  LOBBY_BACKEND_INVALID = 0,
  LOBBY_BACKEND_READY = 1,
  LOBBY_BACKEND_CREATING = 2,
  LOBBY_BACKEND_SEARCHING = 3,
  LOBBY_BACKEND_OBTAINING_ADDRESS = 4,
  LOBBY_BACKEND_FAILED = 7,
  LOBBY_BACKEND_SHUTDOWN = 8
};

class idLobbyBackend {
public:
  enum lobbyBackendType_t { TYPE_PARTY = 0, TYPE_GAME = 1, TYPE_GAME_STATE = 2, TYPE_INVALID = 255 };
  virtual ~idLobbyBackend();
  bool IsHost() const;
  bool IsLocal() const;
  bool IsOnline() const;

protected:
  lobbyBackendType_t type{TYPE_INVALID};
  idMatchParameters parms;
  bool isLocal{};
  bool isHost{};
};

class idLobbyBackendDirect final : public idLobbyBackend {
public:
  explicit idLobbyBackendDirect(idLobbyToSessionCB* callbacks = nullptr);
  void StartHosting(const idMatchParameters& parameters, float skillLevel, lobbyBackendType_t type);
  void StartFinding(const idMatchParameters& parameters, int numPartyUsers, float skillLevel);
  void JoinFromConnectInfo(const lobbyConnectInfo_t& connectInfo);
  void GetSearchResults(std::vector<lobbyConnectInfo_t>& searchResults) const;
  lobbyConnectInfo_t GetConnectInfo();
  bool IsOwnerOfConnectInfo(const lobbyConnectInfo_t& connectInfo) const;
  void Shutdown();
  void FinishBecomeHost();
  void GetOwnerAddress(lobbyAddress_t& output);
  lobbyBackendState_t GetState() const { return state; }
  void SetBoundPort(std::uint16_t value) { boundPort = value; }

private:
  idLobbyToSessionCB* callbacks{};
  lobbyBackendState_t state{LOBBY_BACKEND_INVALID};
  netadr_t address{};
  std::uint16_t boundPort{};
};
