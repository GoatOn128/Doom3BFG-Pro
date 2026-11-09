#include "shared/idlib/networking/networking.h"

#define WIN32_LEAN_AND_MEAN
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <Windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace {
std::once_flag winsockOnce;
std::vector<std::string> localAddresses;

SOCKET Socket(int fd) {
  return static_cast<SOCKET>(static_cast<std::uintptr_t>(fd));
}
int Descriptor(SOCKET socket) {
  return static_cast<int>(socket);
}

void EnsureWinsock() {
  std::call_once(winsockOnce, [] {
    WSADATA data{};
    WSAStartup(MAKEWORD(2, 2), &data);
  });
}

sockaddr_in ToSockaddr(const netadr_t& address) {
  sockaddr_in result{};
  result.sin_family = AF_INET;
  result.sin_port = htons(address.port);
  if (address.type == NA_BROADCAST)
    result.sin_addr.s_addr = INADDR_BROADCAST;
  else if (address.type == NA_LOOPBACK)
    result.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  else
    std::memcpy(&result.sin_addr, address.ip, 4);
  return result;
}

netadr_t FromSockaddr(const sockaddr_in& address) {
  netadr_t result{};
  result.type = NA_IP;
  std::memcpy(result.ip, &address.sin_addr, 4);
  result.port = ntohs(address.sin_port);
  if (address.sin_addr.s_addr == htonl(INADDR_LOOPBACK))
    result.type = NA_LOOPBACK;
  if (address.sin_addr.s_addr == INADDR_BROADCAST)
    result.type = NA_BROADCAST;
  return result;
}

bool Resolve(const char* text, sockaddr_in& address, bool allowDns) {
  EnsureWinsock();
  if (!text || !*text)
    return false;
  std::string host = text;
  std::string service;
  if (host.front() == '[') {
    const auto end = host.find(']');
    if (end != std::string::npos) {
      if (end + 1 < host.size() && host[end + 1] == ':')
        service = host.substr(end + 2);
      host = host.substr(1, end - 1);
    }
  } else {
    const auto colon = host.rfind(':');
    if (colon != std::string::npos && host.find(':') == colon) {
      service = host.substr(colon + 1);
      host.resize(colon);
    }
  }
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_flags = allowDns ? 0 : AI_NUMERICHOST;
  addrinfo* results = nullptr;
  if (getaddrinfo(host.c_str(), service.empty() ? nullptr : service.c_str(), &hints, &results) != 0)
    return false;
  address = *reinterpret_cast<sockaddr_in*>(results->ai_addr);
  freeaddrinfo(results);
  return true;
}

bool WaitSocket(SOCKET socket, bool write, int timeout) {
  fd_set set;
  FD_ZERO(&set);
  FD_SET(socket, &set);
  timeval tv{timeout / 1000, (timeout % 1000) * 1000};
  return select(0, write ? nullptr : &set, write ? &set : nullptr, nullptr,
                timeout < 0 ? nullptr : &tv) > 0;
}
} // namespace

const char* NET_ErrorString() {
  static thread_local char buffer[128];
  const int error = WSAGetLastError();
  std::snprintf(buffer, sizeof(buffer), "Winsock error %d", error);
  return buffer;
}

const char* Sys_NetAdrToString(const netadr_t& a) {
  static thread_local char text[64];
  if (a.type == NA_LOOPBACK)
    std::snprintf(text, sizeof(text), "localhost:%u", a.port);
  else
    std::snprintf(text, sizeof(text), "%u.%u.%u.%u:%u", a.ip[0], a.ip[1], a.ip[2], a.ip[3], a.port);
  return text;
}

bool Sys_CompareNetAdrBase(const netadr_t& a, const netadr_t& b) {
  return a.type == b.type && std::memcmp(a.ip, b.ip, 4) == 0;
}

bool Sys_StringToNetAdr(const char* text, netadr_t& address, bool resolve) {
  sockaddr_in native{};
  if (!Resolve(text, native, resolve)) {
    address = {};
    return false;
  }
  address = FromSockaddr(native);
  return true;
}

void Sys_InitNetworking() {
  EnsureWinsock();
  localAddresses.clear();
  char host[256]{};
  if (gethostname(host, sizeof(host)) != 0)
    return;
  addrinfo hints{};
  hints.ai_family = AF_INET;
  addrinfo* results = nullptr;
  if (getaddrinfo(host, nullptr, &hints, &results) != 0)
    return;
  for (auto* it = results; it; it = it->ai_next) {
    char ip[INET_ADDRSTRLEN]{};
    auto* in = reinterpret_cast<sockaddr_in*>(it->ai_addr);
    if (inet_ntop(AF_INET, &in->sin_addr, ip, sizeof(ip)) &&
        std::find(localAddresses.begin(), localAddresses.end(), ip) == localAddresses.end())
      localAddresses.emplace_back(ip);
  }
  freeaddrinfo(results);
}

const char* Sys_GetLocalIP(int index) {
  if (localAddresses.empty())
    Sys_InitNetworking();
  return index >= 0 && index < static_cast<int>(localAddresses.size())
             ? localAddresses[index].c_str()
             : nullptr;
}

void Net_SendUDPPacket(int netSocket, int length, const void* data, const netadr_t& to) {
  const sockaddr_in address = ToSockaddr(to);
  sendto(Socket(netSocket), static_cast<const char*>(data), length, 0,
         reinterpret_cast<const sockaddr*>(&address), sizeof(address));
}

int NET_IPSocket(const char* interfaceName, int port, netadr_t& boundTo) {
  EnsureWinsock();
  SOCKET socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (socket == INVALID_SOCKET)
    return -1;
  BOOL broadcast = TRUE;
  setsockopt(socket, SOL_SOCKET, SO_BROADCAST, reinterpret_cast<const char*>(&broadcast),
             sizeof(broadcast));
  u_long nonBlocking = 1;
  ioctlsocket(socket, FIONBIO, &nonBlocking);
  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(static_cast<u_short>(port));
  address.sin_addr.s_addr = INADDR_ANY;
  if (interfaceName && *interfaceName && std::strcmp(interfaceName, "localhost") != 0)
    inet_pton(AF_INET, interfaceName, &address.sin_addr);
  if (bind(socket, reinterpret_cast<const sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR) {
    closesocket(socket);
    return -1;
  }
  int length = sizeof(address);
  getsockname(socket, reinterpret_cast<sockaddr*>(&address), &length);
  boundTo = FromSockaddr(address);
  return Descriptor(socket);
}

bool Net_WaitForData(unsigned netSocket, int timeout) {
  return WaitSocket(Socket(static_cast<int>(netSocket)), false, timeout);
}
bool Net_CanSendData(unsigned netSocket, int timeout) {
  return WaitSocket(Socket(static_cast<int>(netSocket)), true, timeout);
}

bool Net_GetUDPPacket(unsigned netSocket, netadr_t& from, char* data, int& size, int maxSize) {
  sockaddr_in address{};
  int addressSize = sizeof(address);
  const int received = recvfrom(Socket(static_cast<int>(netSocket)), data, maxSize, 0,
                                reinterpret_cast<sockaddr*>(&address), &addressSize);
  if (received == SOCKET_ERROR) {
    const int error = WSAGetLastError();
    if (error == WSAEWOULDBLOCK || error == WSAECONNRESET)
      return false;
    return false;
  }
  from = FromSockaddr(address);
  size = received;
  return true;
}

idUDP::idUDP() = default;
idUDP::~idUDP() {
  Close();
}
bool idUDP::InitForPort(int portNumber, bool) {
  Close();
  netSocket = NET_IPSocket(nullptr, portNumber, bound_to);
  return netSocket != -1;
}
void idUDP::Close() {
  if (netSocket != -1) {
    closesocket(Socket(netSocket));
    netSocket = -1;
  }
  bound_to = {};
}
bool idUDP::GetPacket(netadr_t& from, char* data, int& size, int maxSize) {
  if (netSocket == -1)
    return false;
  if (!Net_GetUDPPacket(static_cast<unsigned>(netSocket), from, data, size, maxSize))
    return false;
  ++packetsRead;
  bytesRead += size;
  return true;
}
void idUDP::SendPacket(const netadr_t& to, const void* data, int size) {
  if (netSocket == -1)
    return;
  Net_SendUDPPacket(netSocket, size, data, to);
  ++packetsWritten;
  bytesWritten += size;
}
bool idUDP::CanSendPacket(int timeout) {
  return netSocket != -1 && Net_CanSendData(static_cast<unsigned>(netSocket), timeout);
}

idTCP::idTCP() = default;
idTCP::~idTCP() {
  Close();
}
bool idTCP::IsOpen() const {
  return fd != -1;
}
void idTCP::Close() {
  if (fd != -1) {
    shutdown(Socket(fd), SD_BOTH);
    closesocket(Socket(fd));
    fd = -1;
  }
  address = {};
}
bool idTCP::Connect(const char* host, std::uint16_t port, bool nonBlocking, bool, bool nagle) {
  Close();
  EnsureWinsock();
  sockaddr_in native{};
  if (!Resolve(host, native, true))
    return false;
  native.sin_port = htons(port);
  SOCKET socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
  if (socket == INVALID_SOCKET)
    return false;
  BOOL noDelay = nagle ? FALSE : TRUE;
  setsockopt(socket, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<const char*>(&noDelay),
             sizeof(noDelay));
  if (::connect(socket, reinterpret_cast<const sockaddr*>(&native), sizeof(native)) ==
      SOCKET_ERROR) {
    const int error = WSAGetLastError();
    if (!nonBlocking || error != WSAEWOULDBLOCK) {
      closesocket(socket);
      return false;
    }
  }
  if (nonBlocking) {
    u_long mode = 1;
    ioctlsocket(socket, FIONBIO, &mode);
  }
  fd = Descriptor(socket);
  address = FromSockaddr(native);
  return true;
}
int idTCP::Read(char* data, std::int64_t size) {
  if (fd == -1)
    return -1;
  return recv(Socket(fd), data, static_cast<int>(std::min<std::int64_t>(size, INT_MAX)), 0);
}
int idTCP::Write(const char* data, std::int64_t size) {
  if (fd == -1)
    return -1;
  return send(Socket(fd), data, static_cast<int>(std::min<std::int64_t>(size, INT_MAX)), 0);
}
int idTCP::ReadBlocking(char* data, int size, int timeoutMS) {
  int total = 0;
  while (total < size) {
    if (!WaitSocket(Socket(fd), false, timeoutMS))
      break;
    const int count = Read(data + total, size - total);
    if (count <= 0)
      break;
    total += count;
  }
  return total;
}
int idTCP::WriteBlocking(const char* data, int size, int timeoutMS) {
  int total = 0;
  while (total < size) {
    if (!WaitSocket(Socket(fd), true, timeoutMS))
      break;
    const int count = Write(data + total, size - total);
    if (count <= 0)
      break;
    total += count;
  }
  return total;
}
