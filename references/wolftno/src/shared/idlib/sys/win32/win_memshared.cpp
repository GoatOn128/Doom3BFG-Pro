#include "shared/idlib/sys/sys_alloc.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <Psapi.h>

#include <cstdio>
extern void idMemLocal_ResetMapAllocations(idMemLocal* memory);

// These helpers are deliberately kept in the shared Windows translation unit:
// retail uses it as the boundary between engine heap policy and OS allocation.
bool Sys_AllocWillUseMapHeap() {
  return idMem::Singleton()->IsMapHeap();
}
void ReportHeap(void* heap) {
  std::fprintf(stderr, "heap %p%s\n", heap, heap ? " active" : " inactive");
}
void Sys_ReportHeaps() {
  auto* memory = idMem::Singleton();
  std::fprintf(stderr, "idMem heaps: map=%s current=%s\n",
               memory->IsMapHeapCreated() ? "created" : "none",
               memory->IsGlobalHeap() ? "global" : "map");
}
void ReportGlobalMemoryStatus() {
  virtualMemStats_t stats{};
  idMem::Singleton()->GetVirtualMemoryStats(stats);
  std::fprintf(stderr, "physical memory: %llu / %llu bytes used\n",
               static_cast<unsigned long long>(stats.usedPhysicalMem),
               static_cast<unsigned long long>(stats.totalPhysicalMem));
}

void idMemLocal::PushHeap(heapType_t heap) {
  pushedHeap = currentHeap;
  currentHeap = heap == HEAP_MAPHEAP ? mapHeap : nullptr;
  ++mem_heapStack;
}
void idMemLocal::PopHeap() {
  currentHeap = pushedHeap;
  pushedHeap = nullptr;
  if (mem_heapStack > 0)
    --mem_heapStack;
}
void idMemLocal::FreeHeaps() {
  ResetMapHeap();
  mapHeap = currentHeap = pushedHeap = nullptr;
  mem_heapStack = 0;
}
void idMemLocal::GetVirtualMemoryStats(virtualMemStats_t& stats) const {
  MEMORYSTATUSEX status{sizeof(status)};
  GlobalMemoryStatusEx(&status);
  PERFORMANCE_INFORMATION performance{sizeof(performance)};
  GetPerformanceInfo(&performance, sizeof(performance));
  stats.totalPhysicalMem = status.ullTotalPhys;
  stats.usedPhysicalMem = status.ullTotalPhys - status.ullAvailPhys;
  stats.pageIns = 0;
  stats.pageOuts = 0;
  stats.pageFaults = performance.CommitTotal;
}
void Sys_FindStalePointers() { /* Heap ownership is tracked centrally; no stale blocks are retained.
                                */
}
void idMemLocal::ResetMapHeap() {
  idMemLocal_ResetMapAllocations(this);
  mapHeap = nullptr;
  currentHeap = nullptr;
  pushedHeap = nullptr;
  mem_heapStack = 0;
}
