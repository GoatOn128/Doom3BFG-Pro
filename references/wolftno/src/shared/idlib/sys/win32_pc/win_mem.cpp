#include "shared/idlib/sys/sys_alloc.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace {
constexpr std::uint32_t kAllocationMagic = 0x574D454Du;
struct guardedAllocation_t {
  alloc_t header;
  void* base;
  std::size_t reserved;
};
std::uint32_t HeaderCRC(const alloc_t& header) {
  return kAllocationMagic ^ header.tag ^ header.allocFlags ^ header.offset ^
         static_cast<std::uint32_t>(header.size) ^ static_cast<std::uint32_t>(header.size >> 32);
}
} // namespace

void Mem_PrepForCorruptionInAllocatedPages(std::uint64_t allocation) {
  if (allocation)
    std::memset(reinterpret_cast<void*>(allocation), 0xCD, 16);
}
void Mem_CheckForCorruptionInAllocatedPages(std::uint64_t allocation) {
  if (!allocation)
    return;
  const auto* header = reinterpret_cast<const alloc_t*>(allocation);
  if (header->crc != HeaderCRC(*header))
    OutputDebugStringA("idMem guarded allocation header corruption\n");
}

void* Mem_AllocGuarded(std::size_t size, memTag_t tag, align_t alignment, guardPageStyle_t style) {
  SYSTEM_INFO info{};
  GetSystemInfo(&info);
  const std::size_t page = info.dwPageSize;
  const std::size_t alignmentValue = std::max<std::size_t>(static_cast<std::size_t>(alignment), 16);
  const std::size_t metadata = sizeof(guardedAllocation_t) + alignmentValue;
  const std::size_t body = ((metadata + size + page - 1) / page) * page;
  const bool before = style == GUARD_PAGES_BEFORE || style == GUARD_PAGES_BODY;
  const bool after = style == GUARD_PAGES_AFTER || style == GUARD_PAGES_BODY;
  const std::size_t reserved = body + (before ? page : 0) + (after ? page : 0);
  auto* base = static_cast<std::uint8_t*>(
      VirtualAlloc(nullptr, reserved, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
  if (!base)
    return nullptr;
  if (before) {
    DWORD old = 0;
    VirtualProtect(base, page, PAGE_NOACCESS, &old);
  }
  if (after) {
    DWORD old = 0;
    VirtualProtect(base + reserved - page, page, PAGE_NOACCESS, &old);
  }
  std::uint8_t* bodyStart = base + (before ? page : 0);
  std::uintptr_t value = reinterpret_cast<std::uintptr_t>(bodyStart + sizeof(guardedAllocation_t) +
                                                          alignmentValue - 1) &
                         ~(alignmentValue - 1);
  auto* user = reinterpret_cast<std::uint8_t*>(value);
  auto* metadataBlock = reinterpret_cast<guardedAllocation_t*>(user) - 1;
  metadataBlock->base = base;
  metadataBlock->reserved = reserved;
  metadataBlock->header = {static_cast<std::uint8_t>(tag), static_cast<std::uint8_t>(style),
                           static_cast<std::uint16_t>(user - bodyStart), 0,
                           static_cast<std::int64_t>(size)};
  metadataBlock->header.crc = HeaderCRC(metadataBlock->header);
  std::memset(user, 0, size);
  return user;
}

void Mem_FreeGuarded(const alloc_t* header) {
  if (!header)
    return;
  auto* guarded = reinterpret_cast<const guardedAllocation_t*>(header);
  if (header->crc == HeaderCRC(*header))
    VirtualFree(guarded->base, 0, MEM_RELEASE);
}
int GetTagForMemoryBlock(const void* block) {
  if (!block)
    return TAG_UNSET;
  const auto* guarded = reinterpret_cast<const guardedAllocation_t*>(block) - 1;
  return guarded->header.crc == HeaderCRC(guarded->header) ? guarded->header.tag : TAG_UNSET;
}
bool GetMemoryBlockFromHeapPointer(std::uint8_t* heapPointer, void** block, std::int64_t* size,
                                   memTag_t* tag) {
  if (!heapPointer)
    return false;
  auto* guarded = reinterpret_cast<guardedAllocation_t*>(heapPointer) - 1;
  if (guarded->header.crc != HeaderCRC(guarded->header))
    return false;
  if (block)
    *block = heapPointer;
  if (size)
    *size = guarded->header.size;
  if (tag)
    *tag = static_cast<memTag_t>(guarded->header.tag);
  return true;
}

void* Sys_Alloc(std::size_t size, memTag_t tag, align_t alignment, heapType_t heap) {
  return idMem::Singleton()->AllocWithLocation("Sys_Alloc", size, tag, false, alignment, heap);
}
void Sys_Free(void* pointer) {
  idMem::Singleton()->Free(pointer);
}
std::uint64_t Sys_GetMemoryUsage() {
  PROCESS_MEMORY_COUNTERS_EX counters{};
  counters.cb = sizeof(counters);
  return GetProcessMemoryInfo(GetCurrentProcess(),
                              reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&counters),
                              sizeof(counters))
             ? counters.PrivateUsage
             : 0;
}
void Sys_WriteMemoryReport(const char* mapName, const char* version) {
  idMem::Singleton()->WriteMemoryReport(mapName, version);
}
void Sys_DumpMemory() {
  idMem::Singleton()->DumpMemory();
}
