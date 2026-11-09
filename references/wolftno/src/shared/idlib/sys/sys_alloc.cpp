#include "sys_alloc.h"

#include <Windows.h>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
struct Allocation {
  void* pointer;
  std::size_t size;
  memTag_t tag;
  bool map;
  Allocation* next;
};
std::array<const char*, 256> tagNames{};
SRWLOCK allocationLock = SRWLOCK_INIT;
Allocation* allocations = nullptr;
} // namespace
void idMemLocal_ResetMapAllocations(idMemLocal* memory) {
  std::vector<void*> pending;
  AcquireSRWLockShared(&allocationLock);
  for (Allocation* item = allocations; item != nullptr; item = item->next)
    if (item->map)
      pending.push_back(item->pointer);
  ReleaseSRWLockShared(&allocationLock);
  for (void* pointer : pending)
    memory->Free(pointer);
}
void InitTagNamesTable() {
  if (tagNames[0])
    return;
  tagNames.fill("<BAD TAG NAME>");
  tagNames[TAG_UNSET] = "unset";
  tagNames[TAG_STATIC_EXE] = "static exe";
  tagNames[TAG_DEBUG] = "debug";
  tagNames[TAG_NEW] = "new";
  tagNames[TAG_IDLIST] = "idList";
  tagNames[TAG_TEMP] = "temporary";
  tagNames[TAG_STRING] = "string";
  tagNames[TAG_ATOMIC_STRING] = "atomic string";
  tagNames[TAG_MATH] = "math";
  tagNames[TAG_LEXER] = "lexer";
  tagNames[TAG_IMAGE] = "image";
  tagNames[TAG_IDLIB] = "idlib";
  tagNames[TAG_FILE] = "file";
}
const char* GetMemTagName(int tag) {
  InitTagNamesTable();
  return tag >= 0 && tag < static_cast<int>(tagNames.size()) ? tagNames[tag] : "<BAD TAG NAME>";
}
idMemLocal::idMemLocal()
    : numAllocs(0), hashWalks(0), hashTable{}, oomCallback(nullptr), isOutOfMemory(false),
      currentHeap(nullptr), mapHeap(nullptr), pushedHeap(nullptr), mem_heapStack(0) {}
idMemLocal* idMem::Singleton() {
  static idMemLocal instance;
  return &instance;
}
void* idMemLocal::AllocWithLocation(const char* location, std::size_t size, memTag_t tag, bool zero,
                                    align_t alignment, heapType_t heap) {
  if (size == 0)
    size = 1;
  void* pointer = nullptr;
  do {
    pointer = _aligned_malloc(size, static_cast<std::size_t>(alignment));
    if (pointer)
      break;
    isOutOfMemory = true;
  } while (oomCallback && oomCallback());
  if (!pointer)
    return nullptr;
  if (zero)
    std::memset(pointer, 0, size);
  auto* allocation = static_cast<Allocation*>(
      HeapAlloc(GetProcessHeap(), 0, sizeof(Allocation)));
  if (allocation == nullptr) {
    _aligned_free(pointer);
    isOutOfMemory = true;
    return nullptr;
  }
  allocation->pointer = pointer;
  allocation->size = size;
  allocation->tag = tag;
  allocation->map =
      heap == HEAP_MAPHEAP ||
      (heap == HEAP_DEFAULTHEAP && !IsGlobalHeap());
  AcquireSRWLockExclusive(&allocationLock);
  allocation->next = allocations;
  allocations = allocation;
  ++numAllocs;
  ReleaseSRWLockExclusive(&allocationLock);
  hashTable[0] = {pointer, tag, location, static_cast<int>(size), false, heap == HEAP_MAPHEAP};
  return pointer;
}
void idMemLocal::Free(void* pointer, align_t) {
  if (!pointer)
    return;
  Allocation* removed = nullptr;
  AcquireSRWLockExclusive(&allocationLock);
  Allocation** link = &allocations;
  while (*link != nullptr) {
    if ((*link)->pointer == pointer) {
      removed = *link;
      *link = removed->next;
      --numAllocs;
      break;
    }
    link = &(*link)->next;
  }
  ReleaseSRWLockExclusive(&allocationLock);
  if (removed != nullptr)
    HeapFree(GetProcessHeap(), 0, removed);
  _aligned_free(pointer);
}
void idMemLocal::InitMapHeap() {
  if (!mapHeap)
    mapHeap = this;
}
void idMemLocal::PushMapHeap() {
  InitMapHeap();
  PushHeap(HEAP_MAPHEAP);
}
void idMemLocal::PopMapHeap() {
  PopHeap();
}
bool idMemLocal::OutOfMemory() {
  return isOutOfMemory;
}
std::uint64_t idMemLocal::BytesCurrentlyAllocated() const {
  AcquireSRWLockShared(&allocationLock);
  std::uint64_t total = 0;
  for (Allocation* item = allocations; item != nullptr; item = item->next)
    total += item->size;
  ReleaseSRWLockShared(&allocationLock);
  return total;
}
void idMemLocal::DumpMemory() const {
  std::fprintf(stderr, "idMem: %d allocations, %llu bytes\n", numAllocs,
               static_cast<unsigned long long>(BytesCurrentlyAllocated()));
}
void idMemLocal::WriteMemoryReport(const char* map, const char* version) const {
  std::fprintf(stderr, "idMem report: map=%s version=%s allocations=%d bytes=%llu\n",
               map ? map : "", version ? version : "", numAllocs,
               static_cast<unsigned long long>(BytesCurrentlyAllocated()));
}
