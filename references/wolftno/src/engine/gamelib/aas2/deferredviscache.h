#pragma once

#include "engine/gamelib/aas2/aas2_interface.h"
#include "engine/gamelib/physics/clip.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

class idClip;
class idColor;

class idDeferredVisQuery {
public:
  idDeferredVisQuery() { Reset(); }

  void Reset() {
    nextInHash = nullptr;
    prev = nullptr;
    next = nullptr;
    frameCounter = -1;
    state = 0;
  }

  bool Compare(short* queryStart, short* queryEnd) {
    return start[0] == queryStart[0] && start[1] == queryStart[1] &&
           start[2] == queryStart[2] && end[0] == queryEnd[0] &&
           end[1] == queryEnd[1] && end[2] == queryEnd[2];
  }

  void SetPoints(short* queryStart, short* queryEnd) {
    start[0] = queryStart[0];
    start[1] = queryStart[1];
    start[2] = queryStart[2];
    end[0] = queryEnd[0];
    end[1] = queryEnd[1];
    end[2] = queryEnd[2];
  }

  idDeferredVisQuery* nextInHash;
  idDeferredVisQuery* prev;
  idDeferredVisQuery* next;
  short start[3];
  short end[3];
  short frameCounter;
  unsigned char queryIdx;
  unsigned char state;
  int pad;
};

struct deferredVisList_t {
  deferredVisList_t()
      : headNode(nullptr), tailNode(nullptr), numElements(0) {}

  idDeferredVisQuery* headNode;
  idDeferredVisQuery* tailNode;
  int numElements;
};

class idDeferredVisCache {
public:
  idDeferredVisCache();
  ~idDeferredVisCache() noexcept(false);

  void Init(idClip* inClip);
  void Shutdown();
  void UpdateCollisionQueries();
  void SubmitCollisionQueries();
  unsigned char GetVisQuery(short* const start, short* const end, int flags);
  void PrintDebugInfo();

  idList<idDeferredVisQuery*, TAG_DEFERRED_VIS> hashTable;
  idList<idClipQuery, TAG_DEFERRED_VIS> clipQueries;
  int curClipQueryIdx;
  std::uint32_t alignmentPadding;
  deferredVisList_t leastRecentlyUsedList;
  deferredVisList_t priorityList;
  idClip* clip;
  idDeferredVisQuery* queryList;
  idDeferredVisQuery* queryPool;
  int expiredIndex;
  int numFrames;
  int numCreatedThisFrame;
  int numBumped;
  int numQueriesIssued;
  int totalQueriesIssued;
  int numPurged;
  int numExpired;
  int numCacheHits;
  int totalCacheHits;
  int maxCreatedInAFrame;
  int recentNumFrames;
  int recentTotalCacheHits;
  int recentTotalQueriesIssued;
  float recentCacheHitRatio;
  float recentAvgHitRatio;
  bool initialized;
  bool alreadyWarned;
  std::uint8_t tailPadding[6];

private:
  void AddToFront(deferredVisList_t& list, idDeferredVisQuery* newNode);
  void RemoveFromHash(idDeferredVisQuery* removeNode);
};

class idDeferredVisGrid {
public:
  static constexpr int DIMENSION = 8;
  static constexpr int CELL_SIZE = 32;
  static constexpr int DOUBLE_CELL_SIZE = 64;
  static constexpr int NUM_CELLS = DIMENSION * DIMENSION;
  static constexpr int HALF_GRID_SIZE = DIMENSION * CELL_SIZE / 2;

  idDeferredVisGrid();
  void Init(const idVec3& entityPos);
  void Update(const idVec3& entityPos, const idAAS2* aas, int areaNum,
              int travelFlags);
  float GetGridVisibility(const idVec3& worldPoint,
                          idDeferredVisCache& deferredVisCache,
                          const float* staggerZ, int flags,
                          idColor* missDebugColor,
                          idColor* hitDebugColor);

  unsigned char cells[NUM_CELLS];
  short xyz[3];

private:
  void DebugDrawGrid() const;
};

static_assert(sizeof(idDeferredVisQuery) == 0x30,
              "deferred visibility query ABI drift");
static_assert(offsetof(idDeferredVisQuery, start) == 0x18,
              "deferred query start offset drift");
static_assert(offsetof(idDeferredVisQuery, frameCounter) == 0x24,
              "deferred query frame offset drift");
static_assert(sizeof(deferredVisList_t) == 0x18,
              "deferred visibility list ABI drift");
static_assert(sizeof(idDeferredVisGrid) == 0x46,
              "deferred visibility grid ABI drift");
static_assert(offsetof(idDeferredVisGrid, xyz) == 0x40,
              "deferred grid position offset drift");
static_assert(sizeof(idDeferredVisCache) == 0xC8,
              "deferred visibility cache ABI drift");
static_assert(offsetof(idDeferredVisCache, clipQueries) == 0x18,
              "deferred clip-query list offset drift");
static_assert(offsetof(idDeferredVisCache, leastRecentlyUsedList) == 0x38,
              "deferred LRU offset drift");
static_assert(offsetof(idDeferredVisCache, clip) == 0x68,
              "deferred clip offset drift");
static_assert(offsetof(idDeferredVisCache, queryPool) == 0x78,
              "deferred query-pool offset drift");
static_assert(offsetof(idDeferredVisCache, initialized) == 0xC0,
              "deferred initialized offset drift");

extern int numQueriesAlloc;
