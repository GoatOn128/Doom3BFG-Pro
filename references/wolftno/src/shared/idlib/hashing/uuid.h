#pragma once

#include "shared/idlib/text/str.h"
#include "shared/idlib/containers/staticlist.h"

#include <array>
#include <cstdint>

class idUUID {
public:
  virtual ~idUUID() = default;
  int version;
  std::byte padding0[4];
  idStrStatic<33> hash;
  std::uint32_t generator;
  std::byte padding1[4];
  idStaticList<std::uint8_t,16> data;

  idUUID();
  idUUID(const idUUID& other);
  idUUID& operator=(const idUUID& other);
  void GenerateHash();
  void Nil();
  const char* ToString() const { return hash.data; }
};

static_assert(sizeof(idUUID)==144,"idUUID ABI drift");

class idUUID4 : public idUUID {
public:
  idUUID4();
  void SetData(const idStr& text);
  void Generate();
};

static_assert(sizeof(idUUID4)==144,"idUUID4 ABI drift");
