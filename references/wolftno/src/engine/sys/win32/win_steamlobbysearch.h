#pragma once

#include "engine/sys/sys_lobby_types.h"
#include "engine/sys/win32/win_steamlobby.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

enum ELobbyComparison {
  k_ELobbyComparisonEqualToOrLessThan = -2,
  k_ELobbyComparisonLessThan = -1,
  k_ELobbyComparisonEqual = 0,
  k_ELobbyComparisonGreaterThan = 1,
  k_ELobbyComparisonEqualToOrGreaterThan = 2,
  k_ELobbyComparisonNotEqual = 3
};

struct steamLobbySearchResult_t {
  std::uint64_t lobbyId{};
  std::uint64_t friendId{};
  std::unordered_map<std::string, std::string> properties;
};

class idSteamLobbySearchPlatform {
public:
  virtual ~idSteamLobbySearchPlatform() = default;
  virtual std::vector<steamLobbySearchResult_t> Search(const idMatchParameters* parameters,
      unsigned slots, unsigned skill, bool browser) = 0;
  virtual std::vector<steamLobbySearchResult_t> FriendLobbies() = 0;
  virtual void ShowUserProfile(std::uint64_t userId) = 0;
};

void Win_SetSteamLobbySearchPlatform(idSteamLobbySearchPlatform* platform);

class idSteamLobbySearch {
public:
  struct idServer {
    idServer() = default;
    idServer& operator=(const idServer& other);
    std::uint64_t id{};
    std::uint64_t friendSteamID{};
    serverInfo_t info;
    bool isGameInfoValid{};
    std::unordered_map<std::string, std::string> properties;
    void RetrieveGameInfo();
  };

  idSteamLobbySearch();
  ~idSteamLobbySearch();
  void CancelSearchCallback();
  bool IsBusy() const;
  void SetCallback(idCallback* callbackValue);
  static const char* ConvertELobbyComparisonTypeToString(ELobbyComparison type);
  int NumServers() const { return static_cast<int>(servers.size()); }
  const serverInfo_t* DescribeServerAtIndex(int index) const;
  void ShowServerGamerCardUI(int index);
  std::uint64_t ServerAtIndex(int index) const;
  void SteamLobbyList_RequestNumericalFilter(const char* key, unsigned value,
                                             ELobbyComparison comparison);
  void SteamLobbyList_RequestStringFilter(const char* key, const char* value,
                                          ELobbyComparison comparison);
  idServer* FindServerWaitingForDataUpdate(std::uint64_t lobbyId);
  void OnLobbyDataUpdate(LobbyDataUpdate_t* update);
  bool PrepareToSearch(const idCallback* callbackValue);
  void Clear();
  void SetupServerDataForLobby(const steamLobbySearchResult_t& result);
  void OnLobbyMatchList(const std::vector<steamLobbySearchResult_t>& matches, bool ioFailure);
  void StartSearch(const idMatchParameters* parameters = nullptr, unsigned slots = 0,
                   unsigned skill = 0, bool browser = false);
  void Search(const idMatchParameters* parameters, unsigned slots, unsigned skill);
  void BrowserSearch(const idCallback* callbackValue, unsigned slots, unsigned skill);
  void RequestFriendLobbyData(const steamLobbySearchResult_t& result);
  void FriendSearch(const idCallback* callbackValue);

private:
  void CompleteSearch();
  std::vector<idServer> servers;
  idCallback* callback{};
  bool isBusy{};
  std::unordered_map<std::string, std::string> requestedFilters;
};
