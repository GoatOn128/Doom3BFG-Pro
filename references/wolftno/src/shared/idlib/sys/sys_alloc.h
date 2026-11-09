#pragma once

#include <cstddef>
#include <cstdint>

enum align_t { ALIGN_16 = 16, ALIGN_128 = 128, ALIGN_1M = 1048576 };
enum heapType_t { HEAP_DEFAULTHEAP = -1, HEAP_SYSTEMHEAP = 0, HEAP_MAPHEAP = 1 };
enum guardPageStyle_t {
  GUARD_PAGES_OFF = 0,
  GUARD_PAGES_AFTER,
  GUARD_PAGES_BEFORE,
  GUARD_PAGES_BODY
};
enum memTag_t {
  TAG_UNSET = 0,
  TAG_STATIC_EXE = 1,
  TAG_RESOURCE_GRAPH = 2,
  TAG_DEBUG = 3,
  TAG_NEW = 4,
  TAG_IDLIST = 5,
  TAG_TEMP = 6,
  TAG_STRING = 7,
  TAG_ATOMIC_STRING = 8,
  TAG_BITARRAY = 9,
  TAG_MATH = 10,
  TAG_LEXER = 11,
  TAG_COLLISION = 13,
  TAG_CLIPMODEL = 15,
  TAG_IMAGE = 34,
  TAG_VIRTUALTEXTURE = 36,
  TAG_AAS = 37,
  TAG_IDLIB = 44,
  TAG_TRIANGLES = 45,
  TAG_FILE = 48,
  TAG_PAGEFILECACHE = 50,
  TAG_RENDERWORLD = 61,
  TAG_RENDERER = 62,
  TAG_PUSH = 77,
  TAG_HASHINDEX = 78,
  TAG_PARTICLE = 79,
  TAG_RENDERMODEL = 85,
  TAG_DXBUFFER = 86,
  TAG_RESOURCE = 99,
  TAG_FILE_RESOURCE = 100,
  TAG_RESOURCE_BGL = 101,
  TAG_RESOURCE_BGL_RING = 102,
  TAG_PVS = 104,
  TAG_DEFERRED_VIS = 105,
  TAG_VIDEO = 117,
  TAG_SWF = 0x48,
  TAG_GUI_MODEL = 0x4A,
  TAG_BINK = 0x7C
};
struct virtualMemStats_t {
  std::uint64_t totalPhysicalMem, usedPhysicalMem, pageIns, pageOuts, pageFaults;
};
struct memAllocData_t {
  void* ptr;
  int tag;
  const char* location;
  int bytes;
  bool mark;
  bool inMapHeap;
};
struct alloc_t {
  std::uint8_t tag, allocFlags;
  std::uint16_t offset;
  std::uint32_t crc;
  std::int64_t size;
};

class idMem {
public:
  idMem* operator->() { return this; }
  const idMem* operator->() const { return this; }
  virtual ~idMem() = default;
  virtual void* AllocWithLocation(const char* location, std::size_t size, memTag_t tag, bool zero,
                                  align_t alignment, heapType_t heap) = 0;
  virtual void Free(void* pointer, align_t alignment = ALIGN_16) = 0;
  virtual void InitMapHeap() = 0;
  virtual void ResetMapHeap() = 0;
  virtual void FreeHeaps() = 0;
  virtual void PushHeap(heapType_t heap) = 0;
  virtual void PopHeap() = 0;
  virtual void PushMapHeap() = 0;
  virtual void PopMapHeap() = 0;
  virtual void SetOutOfMemoryCallback(bool (*callback)()) = 0;
  virtual bool (*GetOutOfMemoryCallback())() = 0;
  virtual bool OutOfMemory() = 0;
  virtual std::uint64_t BytesCurrentlyAllocated() const = 0;
  virtual void DumpMemory() const = 0;
  virtual void WriteMemoryReport(const char* mapName, const char* version) const = 0;
  virtual void GetVirtualMemoryStats(virtualMemStats_t& stats) const = 0;
  virtual bool IsGlobalHeap() const = 0;
  bool IsMapHeap() const { return !IsGlobalHeap(); }
  virtual bool IsMapHeapCreated() const = 0;
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
  static idMem& Singleton();
#else
  static class idMemLocal* Singleton();
#endif
};

class idMemLocal : public idMem {
public:
  idMemLocal();
  void* AllocWithLocation(const char*, std::size_t, memTag_t, bool, align_t, heapType_t) override;
  void Free(void*, align_t = ALIGN_16) override;
  void InitMapHeap() override;
  void ResetMapHeap() override;
  void FreeHeaps() override;
  void PushHeap(heapType_t) override;
  void PopHeap() override;
  void PushMapHeap() override;
  void PopMapHeap() override;
  void SetOutOfMemoryCallback(bool (*callback)()) override { oomCallback = callback; }
  bool (*GetOutOfMemoryCallback())() override { return oomCallback; }
  bool OutOfMemory() override;
  std::uint64_t BytesCurrentlyAllocated() const override;
  void DumpMemory() const override;
  void WriteMemoryReport(const char*, const char*) const override;
  void GetVirtualMemoryStats(virtualMemStats_t&) const override;
  bool IsGlobalHeap() const override { return mapHeap == nullptr || currentHeap != mapHeap; }
  bool IsMapHeapCreated() const override { return mapHeap != nullptr; }
  int numAllocs;
  int hashWalks;
  memAllocData_t hashTable[1];
  bool (*oomCallback)();
  bool isOutOfMemory;
  void* currentHeap;
  void* mapHeap;
  void* pushedHeap;
  int mem_heapStack;
};

void InitTagNamesTable();
const char* GetMemTagName(int tagNumber);
void* Sys_Alloc(std::size_t size, memTag_t tag = TAG_UNSET, align_t alignment = ALIGN_16,
                heapType_t heap = HEAP_DEFAULTHEAP);
void Sys_Free(void* pointer);
std::uint64_t Sys_GetMemoryUsage();
void Sys_DumpMemory();
void Sys_WriteMemoryReport(const char* mapName, const char* version);
void* Mem_AllocGuarded(std::size_t size, memTag_t tag, align_t alignment, guardPageStyle_t style);
void Mem_FreeGuarded(const alloc_t* header);
void Mem_PrepForCorruptionInAllocatedPages(std::uint64_t allocation);
void Mem_CheckForCorruptionInAllocatedPages(std::uint64_t allocation);
int GetTagForMemoryBlock(const void* block);
bool GetMemoryBlockFromHeapPointer(std::uint8_t* heapPointer, void** block, std::int64_t* size,
                                   memTag_t* tag);
bool Sys_AllocWillUseMapHeap();
void ReportHeap(void* heap);
void Sys_ReportHeaps();
void ReportGlobalMemoryStatus();
void Sys_FindStalePointers();
static_assert(sizeof(alloc_t) == 16, "allocation header ABI drift");
static_assert(sizeof(memAllocData_t) == 32, "allocation metadata ABI drift");
static_assert(sizeof(idMemLocal) == 96, "idMemLocal ABI drift");
