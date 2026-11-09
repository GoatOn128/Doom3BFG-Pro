#include "engine/sys/win32/win_steamlobbysearch.h"

#include <cstdlib>

namespace {
idSteamLobbySearchPlatform* searchPlatform = nullptr;

int PropertyInt(const std::unordered_map<std::string, std::string>& properties,
                const char* key, int fallback = -1) {
  const auto found = properties.find(key);
  return found != properties.end() && !found->second.empty() ? std::atoi(found->second.c_str()) : fallback;
}
}

void Win_SetSteamLobbySearchPlatform(idSteamLobbySearchPlatform* platform) { searchPlatform = platform; }

idSteamLobbySearch::idServer& idSteamLobbySearch::idServer::operator=(const idServer& other) = default;
idSteamLobbySearch::idSteamLobbySearch() = default;
bool idSteamLobbySearch::IsBusy() const { return isBusy; }

void idSteamLobbySearch::idServer::RetrieveGameInfo() {
  const auto name = properties.find("name");
  if (info.name.IsEmpty() && name != properties.end()) info.name = name->second.c_str();
  info.map = PropertyInt(properties, "gameMap");
  info.type = PropertyInt(properties, "gameType");
  info.mode = PropertyInt(properties, "gameMode");
  info.numPlayers = PropertyInt(properties, "numPlayers");
  const int freeSlots = PropertyInt(properties, "freeSlots");
  info.maxPlayers = info.numPlayers >= 0 && freeSlots >= 0 ? info.numPlayers + freeSlots : -1;
  info.joinable = PropertyInt(properties, "joinable", 1) != 0 && freeSlots != 0;
  isGameInfoValid = id != 0 && PropertyInt(properties, "version", 0) >= 0;
}

idSteamLobbySearch::~idSteamLobbySearch() { Clear(); }
void idSteamLobbySearch::CancelSearchCallback() { delete callback; callback = nullptr; }
void idSteamLobbySearch::SetCallback(idCallback* value) { CancelSearchCallback(); callback = value; }
const char* idSteamLobbySearch::ConvertELobbyComparisonTypeToString(ELobbyComparison type) {
  switch (type) {
    case k_ELobbyComparisonEqualToOrLessThan: return "equal to or less than";
    case k_ELobbyComparisonLessThan: return "less than";
    case k_ELobbyComparisonEqual: return "equal";
    case k_ELobbyComparisonGreaterThan: return "greater than";
    case k_ELobbyComparisonEqualToOrGreaterThan: return "equal to or greater than";
    case k_ELobbyComparisonNotEqual: return "not equal";
    default: return "unknown";
  }
}
const serverInfo_t* idSteamLobbySearch::DescribeServerAtIndex(int index) const {
  return index >= 0 && index < NumServers() ? &servers[index].info : nullptr;
}
void idSteamLobbySearch::ShowServerGamerCardUI(int index) {
  if (searchPlatform == nullptr || index < 0 || index >= NumServers()) return;
  searchPlatform->ShowUserProfile(servers[index].friendSteamID);
}
std::uint64_t idSteamLobbySearch::ServerAtIndex(int index) const {
  return index >= 0 && index < NumServers() ? servers[index].id : 0;
}
void idSteamLobbySearch::SteamLobbyList_RequestNumericalFilter(
    const char* key, unsigned value, ELobbyComparison comparison) {
  if (key == nullptr) return;
  requestedFilters[key] = std::to_string(value) + ":" + std::to_string(comparison);
}
void idSteamLobbySearch::SteamLobbyList_RequestStringFilter(
    const char* key, const char* value, ELobbyComparison comparison) {
  if (key == nullptr || value == nullptr) return;
  requestedFilters[key] = std::string(value) + ":" + std::to_string(comparison);
}
idSteamLobbySearch::idServer* idSteamLobbySearch::FindServerWaitingForDataUpdate(
    std::uint64_t lobbyId) {
  for (idServer& server : servers)
    if (server.id == lobbyId && !server.isGameInfoValid) return &server;
  return nullptr;
}
void idSteamLobbySearch::OnLobbyDataUpdate(LobbyDataUpdate_t* update) {
  if (update == nullptr) return;
  idServer* server = FindServerWaitingForDataUpdate(update->lobbyId);
  if (server != nullptr) server->RetrieveGameInfo();
  for (const idServer& item : servers) if (!item.isGameInfoValid) return;
  CompleteSearch();
}
bool idSteamLobbySearch::PrepareToSearch(const idCallback* callbackValue) {
  idCallback* clone = callbackValue != nullptr ? callbackValue->Clone() : nullptr;
  if (isBusy || searchPlatform == nullptr) {
    if (clone != nullptr) { clone->Call(); delete clone; }
    return false;
  }
  SetCallback(clone); servers.clear(); requestedFilters.clear(); return true;
}
void idSteamLobbySearch::Clear() {
  isBusy = false;
  if (callback != nullptr) { idCallback* pending = callback; callback = nullptr; pending->Call(); delete pending; }
  servers.clear(); requestedFilters.clear();
}
void idSteamLobbySearch::SetupServerDataForLobby(const steamLobbySearchResult_t& result) {
  idServer server; server.id = result.lobbyId; server.friendSteamID = result.friendId;
  server.properties = result.properties; server.RetrieveGameInfo(); servers.push_back(std::move(server));
}
void idSteamLobbySearch::OnLobbyMatchList(
    const std::vector<steamLobbySearchResult_t>& matches, bool ioFailure) {
  servers.clear();
  if (!ioFailure) for (const auto& result : matches) SetupServerDataForLobby(result);
  CompleteSearch();
}
void idSteamLobbySearch::CompleteSearch() {
  isBusy = false;
  if (callback != nullptr) { idCallback* complete = callback; callback = nullptr; complete->Call(); delete complete; }
}
void idSteamLobbySearch::StartSearch(const idMatchParameters* parameters, unsigned slots,
                                     unsigned skill, bool browser) {
  if (searchPlatform == nullptr) { CompleteSearch(); return; }
  isBusy = true;
  OnLobbyMatchList(searchPlatform->Search(parameters, slots, skill, browser), false);
}
void idSteamLobbySearch::Search(const idMatchParameters* parameters, unsigned slots, unsigned skill) {
  if (isBusy || searchPlatform == nullptr) return;
  servers.clear();
  if (parameters != nullptr) idGameSpecificMatchmakingParameters::ApplyMatchParametersAsSearchFilters(*parameters);
  SteamLobbyList_RequestStringFilter("lobbyType", "gamePublic", k_ELobbyComparisonEqual);
  SteamLobbyList_RequestNumericalFilter("freeSlots", slots, k_ELobbyComparisonEqualToOrGreaterThan);
  StartSearch(parameters, slots, skill, false);
}
void idSteamLobbySearch::BrowserSearch(const idCallback* callbackValue, unsigned slots,
                                       unsigned skill) {
  if (!PrepareToSearch(callbackValue)) return;
  SteamLobbyList_RequestNumericalFilter("searchMode", ~0U, k_ELobbyComparisonEqual);
  SteamLobbyList_RequestStringFilter("lobbyType", "gamePublic", k_ELobbyComparisonEqual);
  SteamLobbyList_RequestNumericalFilter("freeSlots", slots, k_ELobbyComparisonEqualToOrGreaterThan);
  StartSearch(nullptr, slots, skill, true);
}
void idSteamLobbySearch::RequestFriendLobbyData(const steamLobbySearchResult_t& result) {
  if (result.lobbyId != 0) SetupServerDataForLobby(result);
}
void idSteamLobbySearch::FriendSearch(const idCallback* callbackValue) {
  if (!PrepareToSearch(callbackValue)) return;
  isBusy = true;
  for (const auto& result : searchPlatform->FriendLobbies()) RequestFriendLobbyData(result);
  CompleteSearch();
}
