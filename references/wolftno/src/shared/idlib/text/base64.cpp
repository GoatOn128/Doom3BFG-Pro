#include "base64.h"

#include <algorithm>
#include <cstring>

namespace {
constexpr char encodeLookup[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
int DecodeValue(std::uint8_t value) {
  if (value >= 'A' && value <= 'Z')
    return value - 'A';
  if (value >= 'a' && value <= 'z')
    return value - 'a' + 26;
  if (value >= '0' && value <= '9')
    return value - '0' + 52;
  if (value == '+')
    return 62;
  if (value == '/')
    return 63;
  return -1;
}
} // namespace

idBase64::idBase64() : data(nullptr), len(0), alloced(0) {}
idBase64::~idBase64() {
  delete[] data;
}
void idBase64::EnsureAlloced(int amount) {
  if (amount <= alloced)
    return;
  auto* replacement = new std::uint8_t[amount];
  if (data && len)
    std::memcpy(replacement, data, len + 1);
  delete[] data;
  data = replacement;
  alloced = amount;
}

int idBase64::Decode(std::uint8_t* destination) const {
  if (!destination)
    return -1;
  int output = 0;
  std::uint32_t value = 0;
  int count = 0;
  for (int i = 0; i < len; ++i) {
    if (data[i] == '=')
      break;
    const int decoded = DecodeValue(data[i]);
    if (decoded < 0)
      continue;
    value = (value << 6) | decoded;
    ++count;
    if (count == 4) {
      destination[output++] = std::uint8_t(value >> 16);
      destination[output++] = std::uint8_t(value >> 8);
      destination[output++] = std::uint8_t(value);
      value = 0;
      count = 0;
    }
  }
  if (count == 3) {
    value <<= 6;
    destination[output++] = std::uint8_t(value >> 16);
    destination[output++] = std::uint8_t(value >> 8);
  } else if (count == 2) {
    value <<= 12;
    destination[output++] = std::uint8_t(value >> 16);
  }
  destination[output] = 0;
  return len;
}

void idBase64::Encode(const std::uint8_t* source, int size) {
  EnsureAlloced((4 * size + 12) / 3 + 2);
  len = 0;
  int index = 0;
  while (index + 3 <= size) {
    const std::uint32_t value = (std::uint32_t(source[index]) << 16) |
                                (std::uint32_t(source[index + 1]) << 8) | source[index + 2];
    data[len++] = encodeLookup[(value >> 18) & 63];
    data[len++] = encodeLookup[(value >> 12) & 63];
    data[len++] = encodeLookup[(value >> 6) & 63];
    data[len++] = encodeLookup[value & 63];
    index += 3;
  }
  const int remaining = size - index;
  if (remaining == 1) {
    const std::uint32_t value = std::uint32_t(source[index]) << 16;
    data[len++] = encodeLookup[(value >> 18) & 63];
    data[len++] = encodeLookup[(value >> 12) & 63];
    data[len++] = '=';
    data[len++] = '=';
  } else if (remaining == 2) {
    const std::uint32_t value =
        (std::uint32_t(source[index]) << 16) | (std::uint32_t(source[index + 1]) << 8);
    data[len++] = encodeLookup[(value >> 18) & 63];
    data[len++] = encodeLookup[(value >> 12) & 63];
    data[len++] = encodeLookup[(value >> 6) & 63];
    data[len++] = '=';
  }
  data[len] = 0;
}
