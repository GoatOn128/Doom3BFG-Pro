#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/aas2/deferredviscache.h"

#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "shared/idlib/bv/box.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

class idMD6Anim;
class idCollisionModelManager {
public:
  bool GetStepMoveResult(trace_t* result, idCollisionQuery& query,
                         bool peek);
};

template <>
bool idList<idMD6Anim*, 5>::Resize(int newSize);

idRenderDebug* GetGameLibRenderDebug();
extern idCollisionModelManager* collisionModelManager;

#pragma warning(disable : 4996)

idCVar vis_showInfo("vis_showInfo", "0", CVAR_BOOL,
                    "show deferred vis cache info",
                    static_cast<argCompletionRef_t>(nullptr));
idCVar vis_showGrid("vis_showGrid", "0", CVAR_BOOL,
                    "draw the deferred vis grid to the screen",
                    static_cast<argCompletionRef_t>(nullptr));
idCVar vis_skipQueries("vis_skipQueries", "0", CVAR_BOOL,
                       "skip deferred vis cache collision queries",
                       static_cast<argCompletionRef_t>(nullptr));
idCVar vis_checkLRU(
    "vis_checkLRU", "0", CVAR_BOOL,
    "check that the lru has the right number of elements in it",
    static_cast<argCompletionRef_t>(nullptr));

int numQueriesAlloc = 0;

namespace {

constexpr int kHashSize = 4096;
constexpr int kQueryPoolSize = 5120;
constexpr int kClipQueryCount = 256;
constexpr int kQueriesPerSubmit = 128;
constexpr int kExpiryBatchSize = 512;

short ClampToShort(float value) {
  int integerValue = static_cast<int>(value);
  if (integerValue < -32768) {
    integerValue = -32768;
  } else if (integerValue > 32767) {
    integerValue = 32767;
  }
  return static_cast<short>(integerValue);
}

int GenerateKey(const short* start, const short* end) {
  return (end[0] + start[0] + end[1] + end[2] + start[1] + start[2]) &
         (kHashSize - 1);
}

void RemoveNoDelete(deferredVisList_t& list, idDeferredVisQuery* node) {
  idDeferredVisQuery* const previous = node->prev;
  idDeferredVisQuery* const next = node->next;
  if (previous != nullptr) {
    previous->next = next;
  } else {
    list.headNode = next;
  }
  if (next != nullptr) {
    next->prev = previous;
  } else {
    list.tailNode = previous;
  }
  --list.numElements;
  node->next = nullptr;
  node->prev = nullptr;
}

void ReturnToPool(idDeferredVisCache& cache, idDeferredVisQuery* node) {
  node->state = 0;
  node->frameCounter = -1;
  node->next = cache.queryList;
  cache.queryList = node;
  --numQueriesAlloc;
}

}  // namespace

idDeferredVisGrid::idDeferredVisGrid() : cells{}, xyz{} {}

void idDeferredVisCache::AddToFront(deferredVisList_t& list,
                                    idDeferredVisQuery* newNode) {
  idDeferredVisQuery* const headNode = list.headNode;
  if (headNode != nullptr) {
    idDeferredVisQuery* const previous = headNode->prev;
    newNode->next = headNode;
    newNode->prev = previous;
    if (previous != nullptr) {
      previous->next = nullptr;
    } else {
      list.headNode = newNode;
    }
    headNode->prev = newNode;
    ++list.numElements;
  } else {
    list.headNode = newNode;
    list.tailNode = newNode;
    newNode->next = nullptr;
    newNode->prev = nullptr;
    ++list.numElements;
  }
}

void idDeferredVisCache::Init(idClip* inClip) {
  clip = inClip;
  initialized = true;
}

void idDeferredVisGrid::DebugDrawGrid() const {
  int cellIndex = 0;
  int cellY = xyz[1] - 144;
  for (int y = 0; y < DIMENSION; ++y, cellY += CELL_SIZE) {
    int cellX = xyz[0] - 144;
    for (int x = 0; x < DIMENSION;
         ++x, ++cellIndex, cellX += CELL_SIZE) {
      idVec3 corners[4];
      corners[0] = {static_cast<float>(cellX), static_cast<float>(cellY),
                    static_cast<float>(xyz[2])};
      corners[1] = {static_cast<float>(cellX + CELL_SIZE),
                    static_cast<float>(cellY),
                    static_cast<float>(xyz[2])};
      corners[2] = {static_cast<float>(cellX + CELL_SIZE),
                    static_cast<float>(cellY + CELL_SIZE),
                    static_cast<float>(xyz[2])};
      corners[3] = {static_cast<float>(cellX),
                    static_cast<float>(cellY + CELL_SIZE),
                    static_cast<float>(xyz[2])};
      const idVec4 color = cells[cellIndex] != 0
                               ? idVec4{0.0f, 1.0f, 0.0f, 1.0f}
                               : idVec4{1.0f, 0.0f, 1.0f, 1.0f};
      GetGameLibRenderDebug()->DebugLine(color, corners[0], corners[1], 0,
                                         false);
      GetGameLibRenderDebug()->DebugLine(color, corners[1], corners[2], 0,
                                         false);
      GetGameLibRenderDebug()->DebugLine(color, corners[2], corners[3], 0,
                                         false);
      GetGameLibRenderDebug()->DebugLine(color, corners[3], corners[0], 0,
                                         false);
      GetGameLibRenderDebug()->DebugLine(color, corners[0], corners[2], 0,
                                         false);
      GetGameLibRenderDebug()->DebugLine(color, corners[1], corners[3], 0,
                                         false);
    }
  }

  idBox box;
  box.center = {static_cast<float>(xyz[0] - 16),
                static_cast<float>(xyz[1] - 16),
                static_cast<float>(xyz[2])};
  box.extents = {1.0f, 1.0f, 1.0f};
  box.axis = mat3_identity;
  GetGameLibRenderDebug()->DebugBox({1.0f, 0.0f, 1.0f, 1.0f}, box, 0,
                                    false);
}

void idDeferredVisCache::RemoveFromHash(idDeferredVisQuery* removeNode) {
  const int key = GenerateKey(removeNode->start, removeNode->end);
  idDeferredVisQuery* node = hashTable.list[key];
  if (node != nullptr) {
    if (node == removeNode) {
      hashTable.list[key] = node->nextInHash;
    } else {
      while (node->nextInHash != removeNode) {
        node = node->nextInHash;
      }
      node->nextInHash = removeNode->nextInHash;
    }
  }
}

unsigned char idDeferredVisCache::GetVisQuery(short* const start,
                                               short* const end, int flags) {
  if (vis_skipQueries.valueInteger != 0) {
    return 0;
  }

  unsigned char bumpedState = 0;
  idDeferredVisQuery* previousInHash = nullptr;
  const int key = GenerateKey(start, end);
  for (idDeferredVisQuery* query = hashTable.list[key]; query != nullptr;
       query = query->nextInHash) {
    if (query->start[0] == start[0] && query->start[1] == start[1] &&
        query->start[2] == start[2] && query->end[0] == end[0] &&
        query->end[1] == end[1] && query->end[2] == end[2]) {
      if (query->state >= 3) {
        ++numCacheHits;
        if (query != leastRecentlyUsedList.headNode) {
          idDeferredVisQuery* const next = query->next;
          idDeferredVisQuery* const previous = query->prev;
          if (next != nullptr) {
            next->prev = previous;
          } else {
            leastRecentlyUsedList.tailNode = previous;
          }
          query->prev->next = query->next;
          query->prev = nullptr;
          query->next = leastRecentlyUsedList.headNode;
          leastRecentlyUsedList.headNode->prev = query;
          leastRecentlyUsedList.headNode = query;
        }
      }
      if (query->frameCounter != 1 || (flags & 2) == 0) {
        return query->state;
      }
      bumpedState = query->state;
      query->frameCounter = 0;
      ++numBumped;
    }
    previousInHash = query;
  }

  idDeferredVisQuery* const newQuery = queryList;
  if (newQuery == nullptr) {
    if (!alreadyWarned) {
      idLib::Warning(
          "idDeferredVisCache::GetVisQuery() ran out of query nodes");
      alreadyWarned = true;
    }
    return 0;
  }

  queryList = newQuery->next;
  newQuery->Reset();
  ++numQueriesAlloc;
  ++numCreatedThisFrame;
  newQuery->SetPoints(start, end);
  newQuery->state = 1;
  if (previousInHash != nullptr) {
    previousInHash->nextInHash = newQuery;
  } else {
    hashTable.list[key] = newQuery;
  }
  AddToFront(priorityList, newQuery);
  return bumpedState != 0 ? bumpedState : 1;
}

void idDeferredVisCache::PrintDebugInfo() {
  const int issuedThisFrame = numQueriesIssued;
  const int hitsThisFrame = numCacheHits;
  ++numFrames;
  totalQueriesIssued += issuedThisFrame;
  totalCacheHits += hitsThisFrame;
  recentTotalCacheHits += hitsThisFrame;
  ++recentNumFrames;
  recentTotalQueriesIssued += issuedThisFrame;

  if (recentNumFrames > 300) {
    const int recentTotal =
        recentTotalQueriesIssued + recentTotalCacheHits;
    recentCacheHitRatio =
        recentTotal <= 0
            ? -1.0f
            : static_cast<float>(recentTotalCacheHits) /
                  (static_cast<float>(recentTotalQueriesIssued) +
                   static_cast<float>(recentTotalCacheHits));
    recentNumFrames = 0;
    recentTotalCacheHits = 0;
    recentTotalQueriesIssued = 0;
  }

  const int frameAttempts = numCreatedThisFrame + hitsThisFrame;
  const float frameHitRatio =
      frameAttempts <= 0
          ? -1.0f
          : static_cast<float>(hitsThisFrame) /
                static_cast<float>(frameAttempts);
  const int totalAttempts = totalCacheHits + totalQueriesIssued;
  const float averageHitRatio =
      totalAttempts > 0
          ? static_cast<float>(totalCacheHits) /
                (static_cast<float>(totalCacheHits) +
                 static_cast<float>(totalQueriesIssued))
          : -1.0f;

  if (vis_showInfo.valueInteger != 0) {
    idLib::Printf(
        "LRU:%d  Priority:%d\t Bumped:%d Purged:%d  Expired:%d  # Hits = "
        "%d Created:%d  Avg Issued = %5.2f  Alloc:%d  Max Created:%d ",
        leastRecentlyUsedList.numElements, priorityList.numElements,
        numBumped, numPurged, numExpired, hitsThisFrame,
        numCreatedThisFrame,
        static_cast<float>(totalQueriesIssued) /
            static_cast<float>(numFrames),
        numQueriesAlloc, maxCreatedInAFrame);

    char text[16];
    if (frameHitRatio >= 0.0f) {
      const char* prefix = frameHitRatio > 0.75f
                               ? "^2"
                               : (frameHitRatio > 0.5f ? "^3" : "^1");
      std::sprintf(text, prefix);
      idLib::Printf("Hit %% = %s%5.2f^0 ", text, frameHitRatio);
    } else {
      idLib::Printf("Hit %% = N/A ");
    }

    if (recentCacheHitRatio >= 0.0f) {
      const char* prefix = recentCacheHitRatio > 0.75f
                               ? "^2"
                               : (recentCacheHitRatio > 0.5f ? "^3" : "^1");
      std::sprintf(text, prefix);
      idLib::Printf("Recent Hit %% = %s%5.2f^0 ", text,
                    recentCacheHitRatio);
    } else {
      idLib::Printf("Recent Hit %% = N/A ");
    }

    if (averageHitRatio >= 0.0f) {
      const char* prefix = averageHitRatio > 0.75f
                               ? "^2"
                               : (averageHitRatio > 0.5f ? "^3" : "^1");
      std::sprintf(text, prefix);
      idLib::Printf("Avg Hit %% = %s%5.2f^0", text, averageHitRatio);
    } else {
      idLib::Printf("Avg Hit %% =  N/A");
    }
    idLib::Printf("\n");
  }

  if (vis_checkLRU.valueInteger != 0) {
    for (idDeferredVisQuery* query = leastRecentlyUsedList.headNode;
         query != nullptr; query = query->next) {
    }
  }
}

void idDeferredVisGrid::Update(const idVec3& entityPos, const idAAS2* aas,
                               int areaNum, int travelFlags) {
  const short clampedX = ClampToShort(entityPos.x);
  const short clampedY = ClampToShort(entityPos.y);
  const short clampedZ = ClampToShort(entityPos.z);
  const short roundedX =
      static_cast<short>((static_cast<int>(clampedX) + 31) & ~31);
  const short roundedY =
      static_cast<short>((static_cast<int>(clampedY) + 31) & ~31);

  if (roundedX != xyz[0] || roundedY != xyz[1]) {
    xyz[0] = roundedX;
    xyz[1] = roundedY;
    xyz[2] = clampedZ;
    std::memset(cells, 1, sizeof(cells));

    unsigned char reachable[NUM_CELLS];
    if (aas != nullptr) {
      const idVec3 gridOrigin{static_cast<float>(xyz[0]),
                              static_cast<float>(xyz[1]),
                              static_cast<float>(xyz[2])};
      aas->ClipGridToAreas(gridOrigin, areaNum, travelFlags, CELL_SIZE,
                           DIMENSION, reachable);
    } else {
      std::memset(reachable, 1, sizeof(reachable));
    }
    for (int index = 0; index < NUM_CELLS; ++index) {
      if (reachable[index] == 0) {
        cells[index] = 0;
      }
    }
  }

  if (vis_showGrid.valueInteger != 0) {
    DebugDrawGrid();
  }
}

float idDeferredVisGrid::GetGridVisibility(
    const idVec3& worldPoint, idDeferredVisCache& deferredVisCache,
    const float* staggerZ, int flags, idColor* missDebugColor,
    idColor* hitDebugColor) {
  (void)missDebugColor;
  (void)hitDebugColor;

  static constexpr short TARGET_CELLS[3][21] = {
      {0, 7, 27, 28, 35, 36, 56, 63, 64},
      {0, 7, 11, 12, 25, 27, 28, 30, 33, 35, 36, 38, 51, 52, 56, 63,
       64},
      {0, 7, 11, 12, 18, 21, 25, 27, 28, 30, 33, 35, 36, 38, 42, 45,
       51, 52, 56, 63, 64},
  };

  short testPoint[2][3];
  testPoint[0][0] = ClampToShort(worldPoint.x);
  testPoint[0][1] = ClampToShort(worldPoint.y);
  testPoint[0][2] = ClampToShort(worldPoint.z);
  std::memcpy(testPoint[1], testPoint[0], sizeof(testPoint[0]));

  const int deltaX = xyz[0] - testPoint[0][0];
  const int deltaY = xyz[1] - testPoint[0][1];
  const int deltaZ = xyz[2] - testPoint[0][2];
  const int distanceSquared =
      deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;

  int targetSet = -1;
  if (distanceSquared > 4000000 || (flags & 1) != 0) {
    targetSet = 0;
  } else if (distanceSquared > 1690000) {
    targetSet = 1;
  } else if (distanceSquared > 1000000) {
    targetSet = 2;
  }

  int stagger = 0;
  if (staggerZ != nullptr) {
    testPoint[1][2] = ClampToShort(*staggerZ);
    if (std::abs(static_cast<int>(xyz[0]) & 0x3F) == 32) {
      stagger = 1;
    }
    if ((static_cast<int>(xyz[1]) & 0x3F) == 0) {
      stagger ^= 1;
    }
  }

  const short gridStartX = static_cast<short>(xyz[0] - HALF_GRID_SIZE);
  const short gridStartY = static_cast<short>(xyz[1] - HALF_GRID_SIZE);
  int visibleCount = 0;
  int completedCount = 0;
  int targetIndex = 1;
  int cellIndex = 0;
  while (cellIndex < NUM_CELLS) {
    if (cells[cellIndex] == 1) {
      short cellPoint[3];
      cellPoint[0] =
          static_cast<short>(gridStartX + CELL_SIZE * (cellIndex & 7));
      cellPoint[1] =
          static_cast<short>(gridStartY + CELL_SIZE * (cellIndex >> 3));
      cellPoint[2] = xyz[2];
      const int parity = ((cellIndex & 7) ^ (cellIndex >> 3)) & 1;
      const unsigned char state = deferredVisCache.GetVisQuery(
          testPoint[stagger ^ parity], cellPoint, flags);
      if (state == 4) {
        ++visibleCount;
        ++completedCount;
      } else if (state == 3) {
        ++completedCount;
      }
    }

    if (targetSet == -1) {
      ++cellIndex;
    } else {
      cellIndex = TARGET_CELLS[targetSet][targetIndex++];
    }
  }

  return completedCount != 0
             ? static_cast<float>(visibleCount) /
                   static_cast<float>(completedCount)
             : 0.0f;
}

void idDeferredVisCache::SubmitCollisionQueries() {
  if (!initialized || vis_skipQueries.valueInteger != 0 || clip == nullptr) {
    return;
  }

  int issued = 0;
  for (idDeferredVisQuery* query = priorityList.headNode;
       query != nullptr && issued < kQueriesPerSubmit; query = query->next) {
    if (query->state != 1) {
      continue;
    }
    const idVec3 end{static_cast<float>(query->end[0]),
                     static_cast<float>(query->end[1]),
                     static_cast<float>(query->end[2])};
    const idVec3 start{static_cast<float>(query->start[0]),
                       static_cast<float>(query->start[1]),
                       static_cast<float>(query->start[2])};
    clipQueries.list[curClipQueryIdx] = clip->Translation(
        nullptr, start, end, nullptr, mat3_identity, 1, 0x1FFF, false,
        "w:\\build\\tech5\\engine\\gamelib\\aas2\\DeferredVisCache.cpp(623) : Translation");
    query->queryIdx = static_cast<unsigned char>(curClipQueryIdx);
    ++issued;
    curClipQueryIdx = static_cast<unsigned char>(curClipQueryIdx + 1);
    query->state = 2;
  }
  numQueriesIssued = issued;

  while (leastRecentlyUsedList.numElements > kHashSize) {
    idDeferredVisQuery* const node = leastRecentlyUsedList.tailNode;
    RemoveNoDelete(leastRecentlyUsedList, node);
    RemoveFromHash(node);
    ReturnToPool(*this, node);
    ++numPurged;
  }

  const int firstExpired = expiredIndex;
  int endExpired = firstExpired + kExpiryBatchSize;
  if (endExpired < kQueryPoolSize) {
    expiredIndex = endExpired;
  } else {
    expiredIndex = 0;
    endExpired = kQueryPoolSize;
  }

  for (int index = firstExpired; index < endExpired; ++index) {
    idDeferredVisQuery* const node = &queryPool[index];
    if (node->frameCounter >= 0) {
      --node->frameCounter;
      if (node->frameCounter <= 0) {
        RemoveNoDelete(leastRecentlyUsedList, node);
        RemoveFromHash(node);
        ReturnToPool(*this, node);
        ++numExpired;
      }
    }
  }
  alreadyWarned = false;
}

void idDeferredVisGrid::Init(const idVec3& entityPos) {
  xyz[0] = static_cast<short>(ClampToShort(entityPos.x) + CELL_SIZE);
  Update(entityPos, nullptr, -1, 0);
}

void idDeferredVisCache::UpdateCollisionQueries() {
  if (!initialized || clip == nullptr) {
    return;
  }

  if (numCreatedThisFrame > maxCreatedInAFrame) {
    maxCreatedInAFrame = numCreatedThisFrame;
  }
  idDeferredVisQuery* query = priorityList.headNode;
  numPurged = 0;
  numExpired = 0;
  numCreatedThisFrame = 0;
  numBumped = 0;
  numCacheHits = 0;

  while (query != nullptr) {
    if (query->state != 2) {
      query = query->next;
      continue;
    }

    const unsigned char queryIndex = query->queryIdx;
    idClipQuery& clipQuery = clipQueries.list[queryIndex];
    const std::uint64_t collisionQueryIndex = clipQuery.index & 0xFFFu;
    clipQuery.index = 0;
    trace_t newTrace;
    collisionModelManager->GetStepMoveResult(
        &newTrace, clip->collisionQueries[collisionQueryIndex].query, false);
    clipQueries.list[query->queryIdx].index = 0;

    if (newTrace.fraction >= 1.0f) {
      query->state = 4;
      query->frameCounter = 30;
    } else {
      query->state = 3;
      query->frameCounter = newTrace.c.entityNum != 8190 ? 18 : 30;
    }

    idDeferredVisQuery* const completed = query;
    query = query->next;
    RemoveNoDelete(priorityList, completed);
    AddToFront(leastRecentlyUsedList, completed);
  }
}

idDeferredVisCache::idDeferredVisCache()
    : hashTable(), clipQueries(), curClipQueryIdx(0),
      leastRecentlyUsedList(), priorityList(), clip(nullptr),
      queryList(nullptr), queryPool(nullptr), expiredIndex(0), numFrames(0),
      numCreatedThisFrame(0), numBumped(0), numQueriesIssued(0),
      totalQueriesIssued(0), numPurged(0), numExpired(0), numCacheHits(0),
      totalCacheHits(0), maxCreatedInAFrame(0), recentNumFrames(0),
      recentTotalCacheHits(0), recentTotalQueriesIssued(0),
      recentCacheHitRatio(0.0f), recentAvgHitRatio(0.0f), initialized(false),
      alreadyWarned(false) {
  queryPool = static_cast<idDeferredVisQuery*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\engine\\gamelib\\aas2\\DeferredVisCache.cpp(464) : TAG_DEFERRED_VIS",
          sizeof(idDeferredVisQuery) * kQueryPoolSize, TAG_DEFERRED_VIS, false,
          ALIGN_128, HEAP_DEFAULTHEAP));
  queryList = queryPool;
  for (int index = 0; index < kQueryPoolSize; ++index) {
    queryPool[index].Reset();
    if (index + 1 < kQueryPoolSize) {
      queryPool[index].next = &queryPool[index + 1];
    }
  }

  auto* const newClipQueries = static_cast<idClipQuery*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag",
          sizeof(idClipQuery) * kClipQueryCount, TAG_DEFERRED_VIS, false,
          ALIGN_16, HEAP_DEFAULTHEAP));
  if (newClipQueries != nullptr) {
    for (int index = 0; index < kClipQueryCount; ++index) {
      newClipQueries[index].index = 0;
    }
    idMem::Singleton().Free(clipQueries.list, ALIGN_16);
  }
  clipQueries.list = newClipQueries;
  if (newClipQueries != nullptr) {
    clipQueries.size = kClipQueryCount;
    clipQueries.num = kClipQueryCount;
  }

  auto& retailHashTable =
      reinterpret_cast<idList<idMD6Anim*, 5>&>(hashTable);
  if (hashTable.size >= kHashSize || retailHashTable.Resize(kHashSize)) {
    int count = hashTable.size;
    if (count > kHashSize) {
      count = kHashSize;
    }
    for (int index = hashTable.num; index < count; ++index) {
      hashTable.list[index] = nullptr;
    }
    hashTable.num = count;
  }
}

void idDeferredVisCache::Shutdown() {
  clip = nullptr;
  for (idDeferredVisQuery* node = leastRecentlyUsedList.headNode;
       node != nullptr;) {
    idDeferredVisQuery* const returned = node;
    node = node->next;
    returned->next = queryList;
    returned->state = 0;
    returned->frameCounter = -1;
    queryList = returned;
    --numQueriesAlloc;
  }
  leastRecentlyUsedList.headNode = nullptr;
  leastRecentlyUsedList.tailNode = nullptr;
  leastRecentlyUsedList.numElements = 0;

  for (idDeferredVisQuery* node = priorityList.headNode; node != nullptr;) {
    idDeferredVisQuery* const returned = node;
    node = node->next;
    returned->next = queryList;
    returned->state = 0;
    returned->frameCounter = -1;
    queryList = returned;
    --numQueriesAlloc;
  }
  priorityList.headNode = nullptr;
  priorityList.tailNode = nullptr;
  priorityList.numElements = 0;

  auto& retailHashTable =
      reinterpret_cast<idList<idMD6Anim*, 5>&>(hashTable);
  if (hashTable.size >= kHashSize || retailHashTable.Resize(kHashSize)) {
    int count = hashTable.size;
    if (count > kHashSize) {
      count = kHashSize;
    }
    for (int index = hashTable.num; index < count; ++index) {
      hashTable.list[index] = nullptr;
    }
    hashTable.num = count;
  }
  initialized = false;
  numCreatedThisFrame = 0;
  numBumped = 0;

  int freeCount = 0;
  for (idDeferredVisQuery* node = queryList; node != nullptr;
       node = node->next) {
    ++freeCount;
  }
  numQueriesAlloc = 0;
  if (freeCount != kQueryPoolSize) {
    idLib::Warning(
        "idDeferredVisCache::Shutdown() - Not all vis queries returned back "
        "to the pool!");
  }
  expiredIndex = 0;
  numFrames = 0;
  totalQueriesIssued = 0;
  totalCacheHits = 0;
}

idDeferredVisCache::~idDeferredVisCache() noexcept(false) {
  Shutdown();
  idMem::Singleton().Free(queryPool, ALIGN_128);
  queryPool = nullptr;
  queryList = nullptr;
  clip = nullptr;
}
