#pragma once

#include <cstdint>

class idSteamUtil {
public:
  static void SteamIpToNetadrIp(std::uint32_t steamIp, std::uint8_t* networkIp);
};
