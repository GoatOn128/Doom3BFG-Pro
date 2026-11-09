#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/atomicstring.h"

#include <cstdint>

class idAnimWebScalar;
class idMD6Node;
class idScalarMemBlock {
public:
  idScalarMemBlock() : floats(nullptr), num(0) {}
  ~idScalarMemBlock() { Free(); }
  void Alloc(int count);
#if defined(WOLFTNO_RETAIL_ANIMWEB_SCALARMEM_PRIVATE_FREE_ABI)
private:
#endif
  void Free();
#if defined(WOLFTNO_RETAIL_ANIMWEB_SCALARMEM_PRIVATE_FREE_ABI)
public:
#endif
  void Free(idList<idAnimWebScalar>& scalars);
  void Free(idList<idAnimWebScalar,33>& scalars);
  float* floats;
  int num;
};

class idAnimWebScalar {
public:
  using idListAllocReturnsReferenceMarker = void;
  enum flags_t : std::uint8_t { DECLARED = 1, READ_ONLY = 2 };
  idAnimWebScalar() : flags(0), scalarIndex(-1), scalarPtr(nullptr) {}
  void Init(const char* name, std::uint8_t flags, float* scalar = nullptr);
#if defined(WOLFTNO_RETAIL_ANIMWEB_GETSCALAR_REFERENCE_ABI)
  float GetScalar(const idScalarMemBlock& block) const;
#else
  float GetScalar(const idScalarMemBlock* block) const;
#endif
#if defined(WOLFTNO_RETAIL_ANIMWEB_SETSCALAR_REFERENCE_ABI)
  void SetScalar(idScalarMemBlock& block, float value);
#else
  void SetScalar(idScalarMemBlock* block, float value);
#endif
  void SetScalarPtr(float* scalar);
  void SetIndex(const idScalarMemBlock* block, std::int16_t index);
  idAtomicString name;
  std::uint8_t flags;
  std::int16_t scalarIndex;
  float* scalarPtr;
};
static_assert(sizeof(idAnimWebScalar)==24,"AnimWeb scalar ABI drift");
class idAnimWebScalarPair {
public:
  enum fieldFlag_t : int {
    FF_NO_OVERRIDE = 0,
  };

  int scalarIndex=-1;
  int coordinateIndex=-1;
  idMD6Node* node=nullptr;
  std::uint8_t fieldType=0;
  std::uint8_t fieldFlags=0;
  std::uint8_t data1=0;
  std::uint8_t data2=0;
};
static_assert(sizeof(idAnimWebScalarPair)==24,"AnimWeb scalar-pair ABI drift");
