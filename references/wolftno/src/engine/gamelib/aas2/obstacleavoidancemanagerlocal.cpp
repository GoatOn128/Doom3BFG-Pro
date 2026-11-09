#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/aas2/obstacleavoidancemanagerlocal.h"

#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <algorithm>
#include <new>

namespace {

constexpr memTag_t TAG_AI_OBSTACLE = static_cast<memTag_t>(0x5F);

__forceinline void AppendObstacleBuffer(
    idList<idObstacleBuffers*, 5>& list, idObstacleBuffers* buffer) {
  void* value = buffer;
  reinterpret_cast<idList<void*, 107>&>(list).Append(value);
}

__forceinline void FreeObstacleAllocation(void* allocation,
                                          align_t alignment) {
  idMem::Singleton().Free(allocation, alignment);
}

} // namespace

idObstacleAvoidanceManagerLocal obstacleAvoidanceManagerLocal;

idObstacleAvoidanceManager::~idObstacleAvoidanceManager() = default;

__declspec(noinline)
idObstacleAvoidanceManagerLocal::idObstacleAvoidanceManagerLocal()
    : jobList(nullptr) {}

__declspec(noinline)
idObstacleAvoidanceManagerLocal::~idObstacleAvoidanceManagerLocal() = default;

void idObstacleAvoidanceManagerLocal::Shutdown() {
  FrameFreeObstacleBuffers();

  for (int index = 0; index < freeBuffers.num; ++index) {
    idObstacleBuffers* const buffer = freeBuffers.list[index];
    FreeObstacleAllocation(buffer->parms, ALIGN_128);
    FreeObstacleAllocation(buffer->route, ALIGN_128);
    FreeObstacleAllocation(buffer->obstacles, ALIGN_128);
    FreeObstacleAllocation(buffer->vertices, ALIGN_128);
    FreeObstacleAllocation(buffer->edges, ALIGN_128);
    FreeObstacleAllocation(buffer->corners, ALIGN_128);
    FreeObstacleAllocation(buffer->traces, ALIGN_128);
    FreeObstacleAllocation(buffer->traceFractions, ALIGN_128);
    FreeObstacleAllocation(buffer->path, ALIGN_128);
    FreeObstacleAllocation(buffer, ALIGN_16);
  }

  usedBuffers.ClearFree();
  freeBuffers.ClearFree();
  toBeFreedBuffers.ClearFree();
  jobList = nullptr;
}

void idObstacleAvoidanceManagerLocal::Init(idParallelJobList* newJobList) {
  if (jobList != nullptr) {
    return;
  }

  jobList = newJobList;
  if (freeBuffers.size >= 32 ||
      reinterpret_cast<idList<idMD6Anim*, 5>&>(freeBuffers).Resize(32)) {
    int count = freeBuffers.size;
    if (count > 32) {
      count = 32;
    }
    freeBuffers.num = count;
  }

  for (int index = 0; index < 32; ++index) {
    idObstacleBuffers* buffer =
        static_cast<idObstacleBuffers*>(
            idMem::Singleton().AllocWithLocation(
                "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
                sizeof(idObstacleBuffers), TAG_AI_OBSTACLE, false, ALIGN_16,
                HEAP_DEFAULTHEAP));
    buffer->parms = static_cast<obstacleParms_t*>(
        idMem::Singleton().AllocWithLocation(
            "w:\\build\\tech5\\engine\\gamelib\\aas2\\"
            "ObstacleAvoidanceManagerLocal.cpp(34) : TAG_AI_OBSTACLE",
            0xD0, TAG_AI_OBSTACLE, false, ALIGN_128, HEAP_DEFAULTHEAP));
    buffer->route = static_cast<obstacleRoute_t*>(
        idMem::Singleton().AllocWithLocation(
            "w:\\build\\tech5\\engine\\gamelib\\aas2\\"
            "ObstacleAvoidanceManagerLocal.cpp(35) : TAG_AI_OBSTACLE",
            0x150, TAG_AI_OBSTACLE, false, ALIGN_128, HEAP_DEFAULTHEAP));
    buffer->obstacles = static_cast<obstacleBox_t*>(
        idMem::Singleton().AllocWithLocation(
            "w:\\build\\tech5\\engine\\gamelib\\aas2\\"
            "ObstacleAvoidanceManagerLocal.cpp(36) : TAG_AI_OBSTACLE",
            0x2800, TAG_AI_OBSTACLE, false, ALIGN_128, HEAP_DEFAULTHEAP));
    buffer->vertices = static_cast<obstacleVertex_t*>(
        idMem::Singleton().AllocWithLocation(
            "w:\\build\\tech5\\engine\\gamelib\\aas2\\"
            "ObstacleAvoidanceManagerLocal.cpp(37) : TAG_AI_OBSTACLE",
            0x2400, TAG_AI_OBSTACLE, false, ALIGN_128, HEAP_DEFAULTHEAP));
    buffer->edges = static_cast<obstacleEdge_t*>(
        idMem::Singleton().AllocWithLocation(
            "w:\\build\\tech5\\engine\\gamelib\\aas2\\"
            "ObstacleAvoidanceManagerLocal.cpp(38) : TAG_AI_OBSTACLE",
            0x2000, TAG_AI_OBSTACLE, false, ALIGN_128, HEAP_DEFAULTHEAP));
    buffer->corners = static_cast<obstacleCorner_t*>(
        idMem::Singleton().AllocWithLocation(
            "w:\\build\\tech5\\engine\\gamelib\\aas2\\"
            "ObstacleAvoidanceManagerLocal.cpp(39) : TAG_AI_OBSTACLE",
            0x100, TAG_AI_OBSTACLE, false, ALIGN_128, HEAP_DEFAULTHEAP));
    buffer->traces = static_cast<obstacleTrace_t*>(
        idMem::Singleton().AllocWithLocation(
            "w:\\build\\tech5\\engine\\gamelib\\aas2\\"
            "ObstacleAvoidanceManagerLocal.cpp(40) : TAG_AI_OBSTACLE",
            0x1000, TAG_AI_OBSTACLE, false, ALIGN_128, HEAP_DEFAULTHEAP));
    buffer->traceFractions = static_cast<float*>(
        idMem::Singleton().AllocWithLocation(
            "w:\\build\\tech5\\engine\\gamelib\\aas2\\"
            "ObstacleAvoidanceManagerLocal.cpp(41) : TAG_AI_OBSTACLE",
            0x400, TAG_AI_OBSTACLE, false, ALIGN_128, HEAP_DEFAULTHEAP));
    void* const pathAllocation = idMem::Singleton().AllocWithLocation(
        "w:\\build\\tech5\\engine\\gamelib\\aas2\\"
        "ObstacleAvoidanceManagerLocal.cpp(42) : TAG_AI_OBSTACLE",
        sizeof(obstaclePath_t), TAG_AI_OBSTACLE, false, ALIGN_128,
        HEAP_DEFAULTHEAP);
    buffer->path = pathAllocation != nullptr
                       ? ::new (pathAllocation) obstaclePath_t
                       : nullptr;
    buffer->numObstacles = 0;
    buffer->numVertices = 0;
    buffer->numEdges = 0;
    buffer->numCorners = 0;
    buffer->numTraces = 0;
    buffer->frameCount = 0;
    buffer->user = nullptr;
    buffer->job = nullptr;
    freeBuffers.list[index] = buffer;
  }
}

void idObstacleAvoidanceManagerLocal::AllocObstacleBuffers(
    idObstacleAvoidance* obstacleAvoidance) {
  int usedIndex = 0;
  while (usedIndex < usedBuffers.num &&
         usedBuffers.list[usedIndex] != obstacleAvoidance->buffers) {
    ++usedIndex;
  }

  if (usedIndex < usedBuffers.num) {
    idObstacleBuffers* const buffer = obstacleAvoidance->buffers;
    buffer->numObstacles = 0;
    buffer->numVertices = 0;
    buffer->numEdges = 0;
    buffer->numCorners = 0;
    buffer->numTraces = 0;
    buffer->frameCount = 2;
    return;
  }

  if (freeBuffers.num <= 0) {
    idLib::Warning("out of obstacle avoidance buffers");
    return;
  }

  const int newFreeCount = freeBuffers.num - 1;
  idObstacleBuffers* const buffer = freeBuffers.list[newFreeCount];
  if (newFreeCount <= freeBuffers.size ||
      reinterpret_cast<idList<idMD6Anim*, 5>&>(freeBuffers).Resize(
          newFreeCount)) {
    int count = freeBuffers.size;
    if (newFreeCount < count) {
      count = newFreeCount;
    }
    freeBuffers.num = count;
  }
  AppendObstacleBuffer(usedBuffers, buffer);
  buffer->user = obstacleAvoidance;
  obstacleAvoidance->buffers = buffer;
  buffer->job = nullptr;
  buffer->numObstacles = 0;
  buffer->numVertices = 0;
  buffer->numEdges = 0;
  buffer->numCorners = 0;
  buffer->numTraces = 0;
  buffer->frameCount = 2;
}

void idObstacleAvoidanceManagerLocal::FreeObstacleBuffers(
    idObstacleAvoidance* obstacleAvoidance) {
  if (obstacleAvoidance->buffers == nullptr) {
    return;
  }

  int usedIndex = 0;
  while (usedIndex < usedBuffers.num &&
         usedBuffers.list[usedIndex] != obstacleAvoidance->buffers) {
    ++usedIndex;
  }
  if (usedIndex >= usedBuffers.num) {
    return;
  }

  idObstacleBuffers* const buffer = usedBuffers.list[usedIndex];
  buffer->user = nullptr;
  buffer->job = nullptr;
  AppendObstacleBuffer(toBeFreedBuffers, buffer);
  const int lastIndex = usedBuffers.num - 1;
  usedBuffers.num = lastIndex;
  if (usedIndex != lastIndex) {
    usedBuffers.list[usedIndex] = usedBuffers.list[lastIndex];
  }
  obstacleAvoidance->buffers = nullptr;
}

void idObstacleAvoidanceManagerLocal::FrameFreeObstacleBuffers() {
  for (int usedIndex = 0; usedIndex < usedBuffers.num; ++usedIndex) {
    idObstacleBuffers* const buffer = usedBuffers.list[usedIndex];
    buffer->frameCount = static_cast<int>(
        static_cast<unsigned int>(buffer->frameCount) - 1u);
    if (buffer->frameCount <= 0) {
      buffer->user->buffers = nullptr;
      buffer->user = nullptr;
      buffer->job = nullptr;
      AppendObstacleBuffer(toBeFreedBuffers, buffer);
      const int lastIndex = usedBuffers.num - 1;
      usedBuffers.num = lastIndex;
      if (usedIndex != lastIndex) {
        usedBuffers.list[usedIndex] = usedBuffers.list[lastIndex];
      }
      --usedIndex;
    }
  }

  for (int index = 0; index < toBeFreedBuffers.num; ++index) {
    AppendObstacleBuffer(freeBuffers, toBeFreedBuffers.list[index]);
  }
  toBeFreedBuffers.SetNum(0);
}
