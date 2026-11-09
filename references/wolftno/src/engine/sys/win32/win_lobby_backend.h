#pragma once

#include "engine/sys/sys_lobby_backend_direct.h"
#include "engine/sys/win32/win_steamlobby.h"
#include "engine/sys/win32/win_steamlobbysearch.h"

#include <memory>
#include <vector>

class idLobbyBackendWin final : public idLobbyBackend {
public:
  explicit idLobbyBackendWin(idLobbyToSessionCB* callbacks = nullptr);
  ~idLobbyBackendWin() override;

  void SetIsJoinable(bool joinable);
  void UpdateLobbySkill(float skill);
  void StartHosting(const idMatchParameters& parameters, float skill,
                    lobbyBackendType_t backendType);
  void JoinFromConnectInfo(const lobbyConnectInfo_t& connectInfo);
  void BecomeHost(int numInvites);
  void FinishBecomeHost();
  void GetOwnerAddress(lobbyAddress_t& output);
  lobbyConnectInfo_t GetConnectInfo();
  bool IsOwnerOfConnectInfo(const lobbyConnectInfo_t& connectInfo) const;
  void SetInGame(bool value);
  int GetGameLobbyFreeSlots() const;
  void ClearSearchResults();
  void Shutdown();
  void SetLobbySkill();
  void UnregisterUser(lobbyUser_t* user, bool isLocalUser);
  void StartFinding(const idMatchParameters& parameters, unsigned numPartyUsers, float skill);
  void GetSearchResults(std::vector<lobbyConnectInfo_t>& output) const;
  void Pump();
  void UpdateMatchParms(const idMatchParameters& parameters);
  void RegisterUser(lobbyUser_t* user, bool isLocalUser);
  lobbyBackendState_t GetState() const;

private:
  bool UsesPlatformBackend() const;
  idLobbyToSessionCB* callbacks{};
  lobbyBackendState_t state{LOBBY_BACKEND_INVALID};
  std::unique_ptr<idSteamLobby> steamLobby;
  std::unique_ptr<idSteamLobbySearch> steamLobbySearch;
  std::vector<std::uint64_t> lobbySearchResults;
  std::vector<lobbyUser_t*> allUsers;
  std::uint64_t lobbyOwner{};
  bool lobbyOwnerIsValid{};
  netadr_t nonSteamAddr{};
  bool isJoinable{true};
  bool commitIsJoinable{};
  bool commitLobbySkill{true};
  bool commitMatchParmsToSteam{};
  bool inGame{};
  int currentSkillLevel{1};
  int maxFreeSlotsOverride{-1};
};
