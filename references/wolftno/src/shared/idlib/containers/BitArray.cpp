#include "shared/idlib/containers/BitArray.h"

#include "shared/idlib/sys/sys_alloc.h"

idBitArray::~idBitArray() {
  if (buffer != nullptr && free) {
    idMem::Singleton()->Free(buffer, ALIGN_16);
  }
  buffer = nullptr;
  bits = 0;
  free = false;
}

void idBitArray::Alloc(std::uint64_t numBits) {
  if (buffer != nullptr && free) {
    idMem::Singleton()->Free(buffer, ALIGN_16);
  }
  bits = static_cast<std::size_t>(numBits);
  const std::size_t bytes = (bits + 7U) >> 3U;
  buffer = bytes != 0
               ? static_cast<unsigned char*>(idMem::Singleton()->AllocWithLocation(
                     "w:\\build\\tech5\\shared\\idlib\\containers\\BitArray.h(40) : tag",
                     bytes, static_cast<memTag_t>(memTag), false, ALIGN_16,
                     HEAP_DEFAULTHEAP))
               : nullptr;
  free = buffer != nullptr;
}
