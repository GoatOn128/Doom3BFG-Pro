#pragma once

#include "engine/sys/sys_lobby_types.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum lobbyState_t { LOBBY_DISCONNECTED, LOBBY_CREATING, LOBBY_JOINING, LOBBY_NORMAL };
enum lobbyJoinable_t { LOBBY_NOT_JOINABLE, LOBBY_JOINABLE };
enum lobbyType_t {
  LOBBY_TYPE_UNSPECIFIED,
  LOBBY_TYPE_PARTY,
  LOBBY_TYPE_GAME_PUBLIC,
  LOBBY_TYPE_GAME_PRIVATE
};
enum lobbyDisconnectReason_t {
  LOBBY_NEVER_CONNECTED,
  LOBBY_NOT_DISCONNECTED,
  LOBBY_DISCONNECTED_KICKED,
  LOBBY_DISCONNECTED_BANNED,
  LOBBY_DISCONNECTED_QUIT_NORMALLY,
  LOBBY_DISCONNECTED_TIMEOUT,
  LOBBY_DISCONNECTED_NOTFOUND,
  LOBBY_DISCONNECTED_FULL,
  LOBBY_DISCONNECTED_ACCESS_DENIED,
  LOBBY_DISCONNECTED_STEAM_CLIENT_CRASHED,
  LOBBY_DISCONNECTED_STEAM_UNAVAILABLE,
  LOBBY_DISCONNECTED_TOO_MANY_LOBBIES,
  LOBBY_DISCONNECTED_UNKNOWN
};

struct LobbyDataUpdate_t { std::uint64_t lobbyId{}; std::uint64_t memberId{}; bool success{}; };
struct LobbyChatUpdate_t { std::uint64_t lobbyId{}; std::uint64_t userChanged{}; std::uint64_t makingChange{}; unsigned changes{}; };
struct LobbyChatMsg_t { std::uint64_t lobbyId{}; std::uint64_t user{}; unsigned entryType{}; unsigned chatId{}; };
struct LobbyKicked_t { std::uint64_t lobbyId{}; std::uint64_t adminId{}; bool disconnected{}; };
struct LobbyGameCreated_t { std::uint64_t lobbyId{}; std::uint64_t gameServerId{}; std::uint32_t ip{}; std::uint16_t port{}; };
struct LobbyCreated_t { int result{}; std::uint64_t lobbyId{}; };
struct LobbyEnter_t { std::uint64_t lobbyId{}; unsigned chatPermissions{}; bool locked{}; unsigned response{}; };

struct steamGameServer_t {
  std::uint64_t gameServerId{};
  std::uint64_t lobbyId{};
  netadr_t netadr{};
};

class idSteamLobbyPlatform {
public:
  virtual ~idSteamLobbyPlatform() = default;
  virtual std::uint64_t LocalUserId() const = 0;
  virtual const char* LocalPersonaName() const = 0;
  virtual std::uint64_t CreateLobby(unsigned privacyType, unsigned slots) = 0;
  virtual bool JoinLobby(std::uint64_t lobbyId) = 0;
  virtual void LeaveLobby(std::uint64_t lobbyId) = 0;
  virtual bool SetJoinable(std::uint64_t lobbyId, bool joinable) = 0;
  virtual bool SetProperty(std::uint64_t lobbyId, const char* key, const char* value) = 0;
  virtual bool DeleteProperty(std::uint64_t lobbyId, const char* key) = 0;
  virtual const char* GetProperty(std::uint64_t lobbyId, const char* key) const = 0;
  virtual std::uint64_t OwnerId(std::uint64_t lobbyId) const = 0;
  virtual int MemberCount(std::uint64_t lobbyId) const = 0;
};

void Win_SetSteamLobbyPlatform(idSteamLobbyPlatform* platform);

class idSteamLobby {
public:
  idSteamLobby();
  ~idSteamLobby();

  bool IsDisconnected() const;
  bool IsCreating() const;
  bool IsJoining() const;
  bool IsNormal() const;
  std::uint64_t Id() const;
  std::uint64_t OwnerId() const;
  const char* DisconnectReasonString() const;
  static bool IsLobbyValid(std::uint64_t id) { return id != 0; }
  void SetLobbyPrivacyType(bool inviteOnly);
  bool IsOwnedByLocalPlayer() const;
  void ChangeLobbyId(std::uint64_t id);
  int GetNumSteamLobbyMembers() const;
  bool CanChangeProperties() const;
  void OnLobbyDataUpdate(LobbyDataUpdate_t* update);
  void OnLobbyChatUpdate(LobbyChatUpdate_t* update);
  void OnLobbyChatMsg(LobbyChatMsg_t* message);
  void OnLobbyGameCreated(LobbyGameCreated_t* event);
  void DebugReport() const;
  void MakeJoinable();
  void MakeNotJoinable();
  void SetProperty(const char* key, const char* value);
  void DeleteProperty(const char* key);
  const char* GetProperty(const char* key) const;
  void WriteLobbyType();
  void UpdateMatchParameters(const idMatchParameters& parameters, int freeSlots, int version);
  void SetIsJoinable(bool joinableValue);
  void OnLobbyCreatedOk();
  void LeaveLobby();
  void SetIsParty();
  void SetIsPublicGame();
  void SetIsPrivateGame();
  void OnLobbyKickedLocalPlayer(LobbyKicked_t* event);
  void OnLobbyCreated(LobbyCreated_t* event, bool ioFailure);
  [[noreturn]] void LeaveLobbyAndThrowError();
  void ReadLobbyType();
  void Create(unsigned privacyType, unsigned slots);
  static idSteamLobby* CreateFriendsOnly(unsigned slots);
  static idSteamLobby* CreateInvisible(unsigned slots);
  void OnLobbyEntered(LobbyEnter_t* event, bool ioFailure);
  void Join(std::uint64_t id);

  lobbyState_t GetState() const { return state; }
  lobbyType_t GetType() const { return type; }
  lobbyDisconnectReason_t GetDisconnectReason() const { return disconnectReason; }
  const steamGameServer_t& GetGameServer() const { return gameServer; }

private:
  std::uint64_t lobbyId{};
  lobbyState_t state{LOBBY_DISCONNECTED};
  lobbyType_t type{LOBBY_TYPE_UNSPECIFIED};
  lobbyJoinable_t joinable{LOBBY_NOT_JOINABLE};
  lobbyDisconnectReason_t disconnectReason{LOBBY_NEVER_CONNECTED};
  bool wasJoined{};
  bool wasGameServerChanged{};
  bool inviteOnly{};
  steamGameServer_t gameServer{};
  mutable std::unordered_map<std::string, std::string> localProperties;
};

struct idGameSpecificMatchmakingParameters {
  static void ApplyMatchParametersAsSearchFilters(const idMatchParameters& parameters);
  static void UpdateMatchParameters(idSteamLobby& lobby, const idMatchParameters& parameters,
                                    int freeSlots, int version);
};

lobbyDisconnectReason_t EChatRoomEnterResponseToDisconnectReason(unsigned response);
