#include "hashing.h"

#include <cstring>

namespace {
constexpr std::uint32_t kM = 0x5BD1E995u;

std::uint32_t Load32(const std::uint8_t* bytes) {
  std::uint32_t value;
  std::memcpy(&value, bytes, sizeof(value));
  return value;
}

std::uint32_t Mix(std::uint32_t value) {
  value *= kM;
  value ^= value >> 24;
  return value * kM;
}
} // namespace

std::uint32_t MurMur32_HashData(std::uint8_t* key, std::uint32_t length, std::uint32_t seed) {
  const std::uint8_t* cursor = key;
  std::uint32_t hash = seed ^ length;
  std::uint32_t remaining = length;
  while (remaining >= 4) {
    hash = (hash * kM) ^ Mix(Load32(cursor));
    cursor += 4;
    remaining -= 4;
  }
  switch (remaining) {
  case 3:
    hash ^= static_cast<std::uint32_t>(cursor[2]) << 16;
    [[fallthrough]];
  case 2:
    hash ^= static_cast<std::uint32_t>(cursor[1]) << 8;
    [[fallthrough]];
  case 1:
    hash = kM * (hash ^ cursor[0]);
    break;
  default:
    break;
  }
  hash ^= hash >> 13;
  hash *= kM;
  return hash ^ (hash >> 15);
}

std::uint64_t MurMur64_HashData(int* key, std::uint32_t length, std::uint32_t seed) {
  const auto* cursor = reinterpret_cast<const std::uint8_t*>(key);
  std::uint32_t low = seed ^ length;
  std::uint32_t high = 0;
  std::uint32_t remaining = length;
  while (remaining >= 8) {
    low = (low * kM) ^ Mix(Load32(cursor));
    high = (high * kM) ^ Mix(Load32(cursor + 4));
    cursor += 8;
    remaining -= 8;
  }
  if (remaining >= 4) {
    low = (low * kM) ^ Mix(Load32(cursor));
    cursor += 4;
    remaining -= 4;
  }
  switch (remaining) {
  case 3:
    high ^= static_cast<std::uint32_t>(cursor[2]) << 16;
    [[fallthrough]];
  case 2:
    high ^= static_cast<std::uint32_t>(cursor[1]) << 8;
    [[fallthrough]];
  case 1:
    high = kM * (high ^ cursor[0]);
    break;
  default:
    break;
  }
  const std::uint32_t a = kM * (low ^ (high >> 18));
  const std::uint32_t b = kM * (high ^ (a >> 22));
  const std::uint32_t upper = kM * (a ^ (b >> 17));
  const std::uint32_t lower = kM * (b ^ (upper >> 19));
  return (static_cast<std::uint64_t>(upper) << 32) | lower;
}
