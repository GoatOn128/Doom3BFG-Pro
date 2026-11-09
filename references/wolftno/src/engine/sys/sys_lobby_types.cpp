#include "engine/sys/sys_lobby_types.h"

#include "shared/idlib/text/str.h"

#include <cstring>

bool lobbyAddress_t::Compare(const lobbyAddress_t& other, bool) const {
  if (usingBackendPlatform != other.usingBackendPlatform) return false;
  if (usingBackendPlatform) return platformId == other.platformId;
  return netAddr.type == other.netAddr.type && netAddr.port == other.netAddr.port &&
         std::memcmp(netAddr.ip, other.netAddr.ip, sizeof(netAddr.ip)) == 0;
}

void lobbyAddress_t::InitFromNetadr(const netadr_t& address) {
  usingBackendPlatform = false;
  platformId = 0;
  netAddr = address;
}

const char* lobbyAddress_t::ToString() const {
  static thread_local char text[64];
  if (usingBackendPlatform) {
    idStr::snPrintf(text, sizeof(text), "platform:%llu",
                    static_cast<unsigned long long>(platformId));
  } else {
    idStr::snPrintf(text, sizeof(text), "%u.%u.%u.%u:%u", netAddr.ip[0], netAddr.ip[1],
                    netAddr.ip[2], netAddr.ip[3], netAddr.port);
  }
  return text;
}

void serverInfo_t::Read(idBitMsg& message) {
  joinable = message.ReadBits(1) != 1;
  message.ReadString(name);
  type = message.ReadBits(32);
  map = message.ReadBits(32);
  mode = message.ReadBits(32);
  numPlayers = message.ReadBits(32);
  maxPlayers = message.ReadBits(32);
}

void serverInfo_t::Write(idBitMsg& message) const {
  message.WriteBits(joinable ? 0 : 1, 1);
  message.WriteString(name.data);
  message.WriteBits(type, 32);
  message.WriteBits(map, 32);
  message.WriteBits(mode, 32);
  message.WriteBits(numPlayers, 32);
  message.WriteBits(maxPlayers, 32);
}

void lobbyUserID_t::WriteToMsg(idBitMsg& message) const {
  message.WriteBits(static_cast<int>(localUserHandle.handle), 32);
  message.WriteBits(lobbyType, 8);
}

void lobbyUserID_t::ReadFromMsg(idBitMsg& message) {
  localUserHandle.handle = static_cast<std::uint32_t>(message.ReadBits(32));
  lobbyType = static_cast<std::uint8_t>(message.ReadBits(8));
}
