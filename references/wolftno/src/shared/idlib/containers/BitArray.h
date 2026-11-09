#pragma once

#include <cstddef>
#include <cstdint>

// Shared PDB-derived storage for the engine bit array. The implementation is
// recovered with idlib; consumers need its concrete embedded-object layout.
class idBitArray {
public:
  idBitArray() : buffer(nullptr), bits(0), memTag(0), free(false) {}
  ~idBitArray();
  void Alloc(std::uint64_t numBits);

  unsigned char* buffer;
  std::size_t bits;
  short memTag;
  bool free;
};

static_assert(sizeof(idBitArray) == 24, "idBitArray ABI drift");
