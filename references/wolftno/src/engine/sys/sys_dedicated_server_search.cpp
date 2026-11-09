#include "engine/sys/sys_dedicated_server_search.h"

#include <algorithm>

idDedicatedServerSearch::idDedicatedServerSearch() = default;
idDedicatedServerSearch::~idDedicatedServerSearch() = default;

void idDedicatedServerSearch::StartSearch(const idCallback& completion) {
  Clear();
  callback.reset(completion.Clone());
}

void idDedicatedServerSearch::Clear() {
  callback.reset();
  servers.clear();
}

bool idDedicatedServerSearch::GetAddrAtIndex(netadr_t& address, int index) const {
  if (index < 0 || index >= NumServers()) return false;
  address = servers[static_cast<std::size_t>(index)].addr.netAddr;
  return true;
}

const serverInfo_t* idDedicatedServerSearch::DescribeServerAtIndex(int index) const {
  return index >= 0 && index < NumServers()
             ? &servers[static_cast<std::size_t>(index)].serverInfo
             : nullptr;
}

const std::vector<idStr>* idDedicatedServerSearch::GetServerPlayersAtIndex(int index) const {
  return index >= 0 && index < NumServers()
             ? &servers[static_cast<std::size_t>(index)].connectedPlayers
             : nullptr;
}

void idDedicatedServerSearch::HandleQueryAck(const lobbyAddress_t& address, idBitMsg& message) {
  auto found = std::find_if(servers.begin(), servers.end(), [&](const serverInfoDedicated_t& item) {
    return item.addr.Compare(address);
  });

  const bool canJoin = message.ReadBits(1) == 1;
  if (found != servers.end()) {
    if (!canJoin) {
      servers.erase(found);
    } else {
      found->serverInfo.Read(message);
      found->connectedPlayers.clear();
      for (int i = 0; i < found->serverInfo.numPlayers; ++i) {
        idStr player;
        message.ReadString(player);
        found->connectedPlayers.push_back(player);
      }
    }
  } else if (canJoin) {
    serverInfoDedicated_t server;
    server.addr = address;
    server.serverInfo.Read(message);
    if (idStr::Find(server.serverInfo.name.data, "HeadlessServer") >= 0) {
      server.serverInfo.name = address.ToString();
    }
    for (int i = 0; i < server.serverInfo.numPlayers; ++i) {
      idStr player;
      message.ReadString(player);
      server.connectedPlayers.push_back(player);
    }
    servers.push_back(server);
  }

  if (callback) callback->Call();
}

