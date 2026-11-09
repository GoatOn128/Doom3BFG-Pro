#include "hashing.h"

#include <array>

namespace {
const std::array<std::uint32_t, 256>& Crc32Table() {
  static const auto table = [] {
    std::array<std::uint32_t, 256> result{};
    for (std::uint32_t i = 0; i < result.size(); ++i) {
      std::uint32_t value = i;
      for (int bit = 0; bit < 8; ++bit) {
        value = (value & 1u) ? (value >> 1) ^ 0xEDB88320u : value >> 1;
      }
      result[i] = value;
    }
    return result;
  }();
  return table;
}
} // namespace

void CRC32_InitChecksum(std::uint32_t* crcValue) {
  *crcValue = 0xFFFFFFFFu;
}

void CRC32_UpdateChecksum(std::uint32_t* crcValue, char* data, int length) {
  std::uint32_t value = *crcValue;
  const auto& table = Crc32Table();
  while (length-- != 0) {
    const auto index = static_cast<std::uint8_t>(value ^ static_cast<std::uint8_t>(*data++));
    value = (value >> 8) ^ table[index];
  }
  *crcValue = value;
}

void CRC32_FinishChecksum(std::uint32_t* crcValue) {
  *crcValue = ~*crcValue;
}

std::uint32_t CRC32_BlockChecksum(char* data, int length) {
  std::uint32_t value;
  CRC32_InitChecksum(&value);
  CRC32_UpdateChecksum(&value, data, length);
  CRC32_FinishChecksum(&value);
  return value;
}
