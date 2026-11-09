#pragma once

#include "engine/sys/sys_lobby_types.h"

#include <memory>
#include <vector>

class idDedicatedServerSearch {
public:
  struct serverInfoDedicated_t {
    lobbyAddress_t addr;
    serverInfo_t serverInfo;
    std::vector<idStr> connectedPlayers;
  };

  idDedicatedServerSearch();
  ~idDedicatedServerSearch();
  void StartSearch(const idCallback& callback);
  void Clear();
  void HandleQueryAck(const lobbyAddress_t& address, idBitMsg& message);
  bool GetAddrAtIndex(netadr_t& address, int index) const;
  const serverInfo_t* DescribeServerAtIndex(int index) const;
  const std::vector<idStr>* GetServerPlayersAtIndex(int index) const;
  int NumServers() const { return static_cast<int>(servers.size()); }

private:
  std::vector<serverInfoDedicated_t> servers;
  std::unique_ptr<idCallback> callback;
};

