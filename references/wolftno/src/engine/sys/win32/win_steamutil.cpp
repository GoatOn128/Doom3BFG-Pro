#include "engine/sys/win32/win_steamutil.h"

void idSteamUtil::SteamIpToNetadrIp(std::uint32_t steamIp, std::uint8_t* networkIp) {
  if (networkIp == nullptr) return;
  networkIp[0] = static_cast<std::uint8_t>(steamIp >> 24U);
  networkIp[1] = static_cast<std::uint8_t>(steamIp >> 16U);
  networkIp[2] = static_cast<std::uint8_t>(steamIp >> 8U);
  networkIp[3] = static_cast<std::uint8_t>(steamIp);
}
