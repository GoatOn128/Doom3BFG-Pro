#pragma once

#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <new>

class idTraceModelCache;
class idClipModel;

template <typename T, int BlockSize, int Tag> class idBlockAlloc {
public:
  union element_t {
    T* data;
    element_t* next;
    // Retail rounds each recyclable element to a 16-byte payload size while
    // retaining pointer alignment for the enclosing block metadata.
    std::uint8_t buffer[(sizeof(T) + 15u) & ~std::size_t(15u)];

    element_t() {}
    ~element_t() {}
  };

  struct idBlock {
    element_t elements[BlockSize];
    idBlock* next;
    element_t* free;
    int freeCount;
  };

  explicit idBlockAlloc(bool clear = false)
      : blocks(nullptr), free(nullptr), total(0), active(0),
        allowAllocs(true), clearAllocs(clear) {}

#if defined(WOLFTNO_RETAIL_BLOCKALLOC_DTOR_ABI)
  __forceinline ~idBlockAlloc() { Shutdown(); }
#else
  ~idBlockAlloc() { Shutdown(); }
#endif

  std::size_t Allocated() const {
    return static_cast<std::size_t>(active) * sizeof(T);
  }
  std::size_t Size() const {
    return static_cast<std::size_t>(total) * sizeof(T);
  }

#if defined(WOLFTNO_RETAIL_BLOCKALLOC_DTOR_ABI)
  __declspec(noinline) void Shutdown() {
#else
  void Shutdown() {
#endif
    while (blocks != nullptr) {
      idBlock* const block = blocks;
      blocks = block->next;
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
      idMem::Singleton().Free(block, ALIGN_16);
#else
      idMem::Singleton()->Free(block, ALIGN_16);
#endif
    }
    blocks = nullptr;
    free = nullptr;
    total = 0;
    active = 0;
  }

  T* Alloc() {
    if (free == nullptr) {
      if (!allowAllocs) {
        return nullptr;
      }
      AllocNewBlock();
    }

    element_t* const element = free;
    ++active;
    free = element->next;
    if (clearAllocs) {
      std::memset(element->buffer, 0, sizeof(element->buffer));
    }
    return ::new (static_cast<void*>(element->buffer)) T;
  }

  void Free(T* value) {
    if (value == nullptr) {
      return;
    }
    value->~T();
    element_t* const element = reinterpret_cast<element_t*>(value);
    element->next = free;
    free = element;
    --active;
  }

  int GetTotalCount() const { return total; }
  int GetAllocCount() const { return active; }
  int GetFreeCount() const { return total - active; }

private:
  friend class idTraceModelCache;
  friend class idClipModel;

  void AllocNewBlock() {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem* const memory = &idMem::Singleton();
#else
    idMem* const memory = idMem::Singleton();
#endif
    idBlock* const block = static_cast<idBlock*>(memory->AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\BlockAlloc.h(207) : _memTag_",
        sizeof(idBlock), static_cast<memTag_t>(Tag), false, ALIGN_16,
        HEAP_DEFAULTHEAP));
    block->next = blocks;
    blocks = block;
    for (int index = 0; index < BlockSize; ++index) {
      block->elements[index].next = free;
      free = &block->elements[index];
    }
    total += BlockSize;
  }

public:
  idBlock* blocks;
  element_t* free;
  int total;
  int active;
  bool allowAllocs;
  bool clearAllocs;
};

static_assert(sizeof(idBlockAlloc<void*, 64, 13>) == 0x20,
              "idBlockAlloc ABI drift");
