#pragma once

#include "shared/idlib/networking/bitmsg.h"

class idUDP {
public:
  idUDP();
  virtual ~idUDP();
  bool InitForPort(int portNumber, bool useBackend = false);
  void Close();
  bool GetPacket(netadr_t& from, char* data, int& size, int maxSize);
  void SendPacket(const netadr_t& to, const void* data, int size);
  bool CanSendPacket(int timeout = 0);
  int packetsRead{}, bytesRead{}, packetsWritten{}, bytesWritten{};
  netadr_t bound_to{};
  int netSocket{-1};
  bool silent{};
};

class idTCP {
public:
  idTCP();
  virtual ~idTCP();
  bool Connect(const char* host, std::uint16_t port, bool nonBlocking = false, bool silent = false,
               bool nagle = true);
  bool IsOpen() const;
  void Close();
  int Read(char* data, std::int64_t size);
  int Write(const char* data, std::int64_t size);
  int ReadBlocking(char* data, int size, int timeoutMS);
  int WriteBlocking(const char* data, int size, int timeoutMS);
  bool WriteDataBlock(char* buffer, int size, int timeoutMS);
  std::int64_t ReadDataBlock(char* buffer, int bufferSize, int timeoutMS);
  netadr_t address{};
  int fd{-1};
};

void Sys_InitNetworking();
const char* NET_ErrorString();
const char* Sys_NetAdrToString(const netadr_t& address);
bool Sys_CompareNetAdrBase(const netadr_t& a, const netadr_t& b);
bool Sys_StringToNetAdr(const char* text, netadr_t& address, bool resolve = true);
const char* Sys_GetLocalIP(int index);

static_assert(sizeof(idUDP) == 48, "UDP ABI drift");
static_assert(sizeof(idTCP) == 24, "TCP ABI drift");
