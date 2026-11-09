#pragma once

#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>

template <typename T> struct idHeapArray {
  T* buffer{};
  unsigned int num{};
  bool bufferOwnedByObject{};

  ~idHeapArray() {
    if (buffer != nullptr && bufferOwnedByObject) {
      idMem::Singleton()->Free(buffer, ALIGN_16);
    }
    num = 0;
    buffer = nullptr;
    bufferOwnedByObject = false;
  }

  void Alloc(unsigned int count) {
    if (buffer != nullptr) {
      idMem::Singleton()->Free(buffer, ALIGN_16);
    }
    num = count;
    buffer = static_cast<T*>(idMem::Singleton()->AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\containers/Array.h(105) : TAG_TEMP",
        sizeof(T) * static_cast<std::size_t>(count), TAG_TEMP, false,
        ALIGN_16, HEAP_DEFAULTHEAP));
    bufferOwnedByObject = true;
  }
};

static_assert(sizeof(idHeapArray<unsigned char>) == 0x10,
              "idHeapArray ABI drift");
