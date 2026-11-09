#pragma once

#include <cstdint>

class idBase64 {
public:
  std::uint8_t* data;
  int len;
  int alloced;

  idBase64();
  ~idBase64();
  idBase64(const idBase64&) = delete;
  idBase64& operator=(const idBase64&) = delete;
  void EnsureAlloced(int amount);
  int Decode(std::uint8_t* destination) const;
  void Encode(const std::uint8_t* source, int size);
};

static_assert(sizeof(idBase64) == 16, "idBase64 ABI drift");
