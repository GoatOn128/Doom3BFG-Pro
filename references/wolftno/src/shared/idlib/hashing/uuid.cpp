#include "uuid.h"

#include <Windows.h>

#include <cstdlib>

void idUUID::GenerateHash() {
  hash.Empty();
  for (const auto byte : data) {
    char encoded[3];
    idStr::snPrintf(encoded, sizeof(encoded), "%02x", byte);
    hash.Append(encoded);
  }
}
idUUID::idUUID() : version(0), hash(), generator(0), data{} {}
void idUUID::Nil() {
  data.fill(0);
  hash.Clear();
}
idUUID& idUUID::operator=(const idUUID& other) {
  if (this != &other) {
    version = other.version;
    generator = other.generator;
    data = other.data;
    GenerateHash();
  }
  return *this;
}
idUUID4::idUUID4() {
  version = 4;
  Nil();
}

void idUUID4::SetData(const idStr& text) {
  if (text.len != 32)
    return;
  Nil();
  for (int i = 0; i < 16; ++i) {
    char pair[3] = {text.data[i * 2], text.data[i * 2 + 1], 0};
    char* end = nullptr;
    const unsigned long value = std::strtoul(pair, &end, 16);
    if (end != pair + 2) {
      Nil();
      return;
    }
    data[i] = static_cast<std::uint8_t>(value);
  }
  GenerateHash();
}

void idUUID4::Generate() {
  static std::uint32_t sharedGenerator = 0;
  if (sharedGenerator == 0)
    sharedGenerator = static_cast<std::uint32_t>(::GetTickCount64()) & 0x7FFFFFFFu;
  Nil();
  for (auto& byte : data) {
    sharedGenerator = 1664525u * sharedGenerator + 1013904223u;
    byte = static_cast<std::uint8_t>(sharedGenerator >> 10);
  }
  data[6] = static_cast<std::uint8_t>((data[6] & 0x0F) | (version << 4));
  data[8] = static_cast<std::uint8_t>((data[8] & 0x3F) | 0x80);
  GenerateHash();
}
