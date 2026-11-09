#pragma once

#include "engine/sys/sys_lobby_backend_direct.h"
#include "engine/sys/win32/win_achievements.h"
#include "engine/sys/win32/win_lobby_backend.h"
#include "engine/sys/win32/win_signin.h"
#include "engine/sys/win32/win_steamleaderboards.h"
#include "engine/sys/win32/win_steamstats.h"
#include "engine/sys/win32/win_steamvoicechat.h"
#include "shared/idlib/text/cmdargs.h"

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

class idFile_Memory;

struct P2PSessionRequest_t { std::uint64_t remoteId{}; };
struct P2PSessionConnectFail_t { std::uint64_t remoteId{}; std::uint8_t error{}; };
struct GameLobbyJoinRequested_t { std::uint64_t lobbyId{}; std::uint64_t friendId{}; };

struct idLobbyPort {
  bool usingBackendPlatform{};
  std::uint16_t boundPort{};
  bool initialized{};
};

struct winContentPackage_t {
  unsigned appId{};
  idStr path;
};

struct winSessionHooks_t {
  bool (*steamInitialize)(){};
  void (*steamShutdown)(){};
  void (*steamPump)(){};
  bool (*acceptP2PSession)(std::uint64_t remoteId){};
  void (*closeP2PSession)(std::uint64_t remoteId){};
  void (*showInviteUI)(std::uint64_t lobbyId){};
  void (*showUserProfile)(std::uint64_t userId){};
  void (*showMarketplace)(){};
  std::vector<winContentPackage_t> (*enumerateContent)(){};
  void (*connectToLobby)(const lobbyConnectInfo_t& info){};
  void (*createMatch)(const idMatchParameters& parameters){};
  void (*startServer)(const idCmdArgs& args){};
};

void Win_SetSessionHooks(const winSessionHooks_t& hooks);
void SteamAPIDebugTextHook(int severity, const char* debugText);

class idSessionLocalWin;

class idLobbyToSessionCBLocal final : public idLobbyToSessionCB {
public:
  explicit idLobbyToSessionCBLocal(idSessionLocalWin* sessionValue) : sessionLocalWin(sessionValue) {}
  idLobbyBackend* GetLobbyBackend(int type) override;
  bool CanJoinLocalHost(const idMatchParameters& parameters, int numPartyUsers) override;
private:
  idSessionLocalWin* sessionLocalWin{};
};

class idSessionLocalWin {
public:
  idSessionLocalWin();
  ~idSessionLocalWin();
  void InitializeSoundRelatedSystems();
  void ShutdownSoundRelatedSystems();
  void ShowServerGamerCardUI(int index);
  bool ProcessInputEvent(const sysEvent_t* event);
  void SetSystemUIShowing(bool show);
  void LoadInvite(std::uint64_t lobbyId);
  void ClearBootableInvite();
  void HandleBootableInvite(std::uint64_t lobbyId);
  bool HasPendingBootableInvite() const;
  void LeaderboardDownloadAttachment(int sessionUserIndex,
      const leaderboardDefinition_t* leaderboard, std::int64_t attachmentId);
  void Initialize();
  void InviteFriends();
  void ShowSystemMarketplaceUI();
  void DestroySteamObjects();
  void OnP2PSessionConnectFail(P2PSessionConnectFail_t* info);
  void OnStatsReceived(UserStatsReceived_t* result);
  void PumpSteam();
  void ListServers(const idCallback* callback);
  void CancelListServers();
  int NumServers() const;
  const serverInfo_t* ServerInfo(int index) const;
  void ConnectToServer(int index);
  void ShowLobbyUserGamerCardUI(lobbyUserID_t userId);
  void OnGameLobbyJoinRequested(GameLobbyJoinRequested_t* request);
  void HandleServerQueryRequest(lobbyAddress_t* remoteAddress, idBitMsg* message, int lobbyType);
  void HandleServerQueryAck(lobbyAddress_t* remoteAddress, idBitMsg* message);
  void StatsStore();
  void StatsUpload(lobbyUserID_t userId, const std::int64_t* stats,
                   const std::int64_t* values, int numStats);
  void LeaderboardUpload(lobbyUserID_t userId, const leaderboardDefinition_t* leaderboard,
                         const column_t* stats, const idFile_Memory* attachment);
  void EnsurePort();
  idLobbyPort* GetPort(bool dedicated);
  void DestroyLobbyBackend(idLobbyBackend* backend);
  void PumpLobbies();
  idLobbyBackendWin* CreateLobbyInternal(idLobbyBackend::lobbyBackendType_t type);
  void JoinAfterSwap(const std::uint64_t* inviteInfo);
  void Shutdown();
  void PlatformPump();
  void ConstructSteamObjects();
  void OnP2PSessionRequest(P2PSessionRequest_t* request);
  void Connect_f(const idCmdArgs* args);
  void LeaderboardDownload(int sessionUserIndex, const leaderboardDefinition_t* leaderboard,
                           int startingRank, int numRows,
                           const idLeaderboardCallback* callback);
  idLobbyBackendWin* CreateLobbyBackend(const idMatchParameters* parameters, float skill,
      idLobbyBackend::lobbyBackendType_t type);
  idLobbyBackendWin* FindLobbyBackend(const idMatchParameters* parameters,
      unsigned numPartyUsers, float skill, idLobbyBackend::lobbyBackendType_t type);
  idLobbyBackendWin* JoinFromConnectInfo(const lobbyConnectInfo_t* connectInfo,
      idLobbyBackend::lobbyBackendType_t type);
  void InitSteam();
  bool IsSteamInitialized() const;
  bool IsSteamStatsInitialized() const;
  void StartServer_f(const idCmdArgs* args);
  void EnumerateDownloadableContent();

  idLobbyBackend* GetLobbyBackend(int type) const;
  bool CanJoinLocalHost() const { return canJoinLocalHost; }
  const std::vector<winContentPackage_t>& ContentPackages() const { return contentPackages; }

private:
  struct bootGameInvite_t { bool pending{}; std::uint64_t steamId{}; };
  bool steamInitialized{};
  bool steamStatsInitialized{};
  bool steamFailed{};
  bool systemUIShowing{};
  bool canJoinLocalHost{true};
  bool initialized{};
  bool flushedStats{};
  std::unique_ptr<idAchievementSystemWin> achievementSystem;
  std::unique_ptr<idSignInManagerWin> signInManager;
  std::unique_ptr<idSteamLeaderboards> steamLeaderboards;
  std::unique_ptr<idSteamLobbySearch> steamLobbySearch;
  std::unique_ptr<idSteamStats> steamStats;
  std::unique_ptr<idVoiceChatMgrSteam> voiceChat;
  std::array<std::unique_ptr<idLobbyBackendWin>, 3> lobbyBackends;
  idLobbyPort port;
  idLobbyPort dedicatedPort;
  std::unique_ptr<idLobbyToSessionCBLocal> lobbyToSessionCB;
  bootGameInvite_t pendingBootGameInviteInfo;
  std::vector<std::uint64_t> deferredCloseP2PWithUser;
  std::vector<winContentPackage_t> contentPackages;
};

extern idSessionLocalWin sessionLocalWin;
void Connect_f(const idCmdArgs* args);
void StartServer_f(const idCmdArgs* args);
