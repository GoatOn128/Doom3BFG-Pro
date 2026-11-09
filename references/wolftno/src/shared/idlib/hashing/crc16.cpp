#include "hashing.h"

#include <array>

namespace {
const std::array<std::uint16_t, 256>& Crc16Table() {
  static const auto table = [] {
    std::array<std::uint16_t, 256> result{};
    for (std::uint32_t i = 0; i < result.size(); ++i) {
      std::uint16_t value = static_cast<std::uint16_t>(i << 8);
      for (int bit = 0; bit < 8; ++bit) {
        value = static_cast<std::uint16_t>((value & 0x8000u) ? (value << 1) ^ 0x1021u : value << 1);
      }
      result[i] = value;
    }
    return result;
  }();
  return table;
}
} // namespace

// CRC16_InitChecksum/CRC16_UpdateChecksum, RVAs 0x00A295D0/0x00A295E0.
void CRC16_InitChecksum(std::uint16_t* crcValue) {
  *crcValue = 0xFFFFu;
}

void CRC16_UpdateChecksum(std::uint16_t* crcValue, std::uint8_t* data, int length) {
  std::uint16_t value = *crcValue;
  const auto& table = Crc16Table();
  while (length-- != 0) {
    const auto index = static_cast<std::uint8_t>(*data++ ^ (value >> 8));
    value = static_cast<std::uint16_t>((value << 8) ^ table[index]);
  }
  *crcValue = value;
}
