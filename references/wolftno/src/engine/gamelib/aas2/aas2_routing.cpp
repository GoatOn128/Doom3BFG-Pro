#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#define WOLFTNO_RETAIL_TRIVIAL_IDINDEX_ABI
#include "engine/gamelib/aas2/aas2_routing_abi.h"

#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/sys/sys_time.h"

#undef WOLFTNO_RETAIL_TRIVIAL_IDINDEX_ABI
#undef WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cstring>
#include <limits>
#include <new>
#include <xmmintrin.h>

void operator delete[](void* memory, memTag_t tag) noexcept;

enum class aas2RoutingCacheTag_t : int { Cache = TAG_AAS };

void operator delete(void* memory, aas2RoutingCacheTag_t tag) noexcept;

#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:??3@YAXPEAXW4aas2RoutingCacheTag_t@@@Z="                \
    "??_V@YAXPEAXW4memTag_t@@@Z")

__forceinline void* operator new(std::size_t size,
                                 aas2RoutingCacheTag_t tag) noexcept {
  return idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size,
      static_cast<memTag_t>(tag), false, ALIGN_16, HEAP_DEFAULTHEAP);
}

static idCVar aas_showCacheMisses(
    "aas_showCacheMisses", "0", CVAR_BOOL,
    "show cache misses and time to cache",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

__forceinline float RoutingSquareRoot(float squaredLength) {
  return _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
      _mm_set_ss(squaredLength), idMath::vector_float_smallestNonDenorm)));
}

__forceinline idVec3 Dequantize(const idQuantizedVec3& value) {
  return {static_cast<float>(value.x), static_cast<float>(value.y),
          static_cast<float>(value.z)};
}

__forceinline bool TravelFlagsAllowed(int value, int allowed) {
  return (value & allowed) != 0 && (value & ~allowed) == 0;
}

__forceinline void DestroyRoutingCache(idAASRoutingCache* cache) {
  idMem& memory = idMem::Singleton();
  memory.Free(cache->reachabilities, ALIGN_16);
  memory.Free(cache->travelTimes, ALIGN_16);
  memory.Free(cache, ALIGN_16);
}

__forceinline void LinkRoutingCache(const idAAS2Local* aas,
                                    idAASRoutingCache* cache) {
  aas->totalCacheMemory += 80 + 3 * cache->size;
  cache->time_next = nullptr;
  cache->time_prev = aas->cacheListEnd;
  if (aas->cacheListEnd != nullptr) {
    aas->cacheListEnd->time_next = cache;
  }
  aas->cacheListEnd = cache;
  if (aas->cacheListStart == nullptr) {
    aas->cacheListStart = cache;
  }
}

__forceinline idAAS2ReachabilityIndex ResolveReachability(
    const idAAS2File& file, int areaNum, int ordinal) {
  idAAS2ReachabilityIndex result = file.areas.list[areaNum].reach;
  if (result.value >= 0) {
    while (--ordinal >= 0) {
      result = file.reachabilities.list[result.value].next;
      if (result.value < 0) {
        break;
      }
    }
  }
  if (result.value < 0) {
    result.value = -1;
  }
  return result;
}

} // namespace

idAASRoutingCache::idAASRoutingCache(int cacheSize) {
  cluster = 0;
  areaNum = 0;
  prev = nullptr;
  next = nullptr;
  time_prev = nullptr;
  time_next = nullptr;
  travelFlags = 0;
  startTravelTime = 0;
  type = 0;
  size = cacheSize;

  const std::size_t count = static_cast<std::size_t>(cacheSize);
  reachabilities = static_cast<unsigned char*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\Heap.h(113) : tag", count,
          TAG_AAS, false, ALIGN_16, HEAP_DEFAULTHEAP));
  std::memset(reachabilities, 0, count);

  const std::size_t travelAllocationSize =
      count <= std::numeric_limits<std::size_t>::max() / 2
          ? count * 2
          : std::numeric_limits<std::size_t>::max();
  travelTimes = static_cast<unsigned short*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\Heap.h(113) : tag",
          travelAllocationSize, TAG_AAS, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  std::memset(travelTimes, 0, count * 2);
}

void idAAS2Local::RoutingStats() const {
  int portalCacheCount = 0;
  int areaCacheCount = 0;
  int portalCacheMemory = 0;
  int areaCacheMemory = 0;
  for (idAASRoutingCache* cache = cacheListStart; cache != nullptr;
       cache = cache->time_next) {
    if (cache->type == 1) {
      ++areaCacheCount;
      areaCacheMemory += 80 + 3 * cache->size;
    } else {
      ++portalCacheCount;
      portalCacheMemory += 80 + 3 * cache->size;
    }
  }
  idLib::Printf("%6d area cache (%d kB)\n", areaCacheCount,
                areaCacheMemory >> 10);
  idLib::Printf("%6d portal cache (%d kB)\n", portalCacheCount,
                portalCacheMemory >> 10);
  idLib::Printf("%6d total cache (%d kB)\n",
                portalCacheCount + areaCacheCount, totalCacheMemory >> 10);
  idLib::Printf("%6d area travel times (%d kB)\n", numAreaTravelTimes,
                (2 * numAreaTravelTimes) >> 10);
  idLib::Printf("%6d area cache entries (%d kB)\n", areaCacheIndexSize,
                (8 * areaCacheIndexSize) >> 10);
  idLib::Printf("%6d portal cache entries (%d kB)\n",
                portalCacheIndexSize, (8 * portalCacheIndexSize) >> 10);
}

void idAAS2Local::UnlinkCache(idAASRoutingCache* cache) const {
  if (cache == nullptr) {
    return;
  }
  totalCacheMemory -= 80 + 3 * cache->size;
  if (cache->time_next != nullptr) {
    cache->time_next->time_prev = cache->time_prev;
  } else {
    cacheListEnd = cache->time_prev;
  }
  if (cache->time_prev != nullptr) {
    cache->time_prev->time_next = cache->time_next;
  } else {
    cacheListStart = cache->time_next;
  }
  cache->time_prev = nullptr;
  cache->time_next = nullptr;
}

unsigned short idAAS2Local::CalcTravelTime(const idVec3& start,
                                            const idVec3& end,
                                            int travelFlags) const {
  const float deltaX = end.x - start.x;
  const float deltaY = end.y - start.y;
  const float deltaZ = end.z - start.z;
  const float length = RoutingSquareRoot(
      (deltaX * deltaX + deltaY * deltaY) + deltaZ * deltaZ);
  const float scaled = (travelFlags & 0x10) != 0
                           ? length * waterSpeedMultiplier
                           : length * groundSpeedMultiplier;
  if (scaled < 1.0f) {
    return 1;
  }
  return static_cast<unsigned short>(_mm_cvtt_ss2si(_mm_set_ss(scaled)));
}

bool idAAS2Local::ChangeReachabilityTravelFlags(const char* reachabilityName,
                                                 int travelFlag, bool set) {
  if (file == nullptr) {
    return false;
  }
  const idAAS2ReachabilityIndex index =
      file->FindReachabilityByName(reachabilityName);
  return index.value >= 0 &&
         ChangeReachabilityTravelFlags(index, travelFlag, set);
}

int idAAS2Local::TravelTimeToGoalArea(
    int startAreaNum, const idVec3& startOrigin, int goalAreaNum,
    const idVec3& goalOrigin, int travelFlags,
    const idAAS2TravelSpeeds&) const {
  idAAS2ReachabilityIndex reachIndex;
  reachIndex.value = -1;
  if (file == nullptr) {
    return 0;
  }
  int travelTime;
  if (!RouteToGoalArea(startAreaNum, startOrigin, goalAreaNum, goalOrigin,
                       travelFlags, travelTime, reachIndex)) {
    return 0;
  }
  return travelTime;
}

unsigned short idAAS2Local::AreaTravelTime(int areaNum,
                                            const idVec3& start,
                                            const idVec3& end) const {
  return file != nullptr
             ? CalcTravelTime(start, end, file->areas.list[areaNum].travelFlags)
             : 0;
}

void idAAS2Local::CalculateAreaTravelTimes() {
  if (file == nullptr) {
    return;
  }

  numAreaTravelTimes = 0;
  for (int areaNum = 0; areaNum < file->areas.num; ++areaNum) {
    const aas2Area_t& area = file->areas.list[areaNum];
    if ((area.flags & 2) == 0) {
      continue;
    }
    int outgoingCount = 0;
    for (idAAS2ReachabilityIndex index = area.reach; index.value >= 0;
         index = file->reachabilities.list[index.value].next) {
      ++outgoingCount;
    }
    int incomingCount = 0;
    for (idAAS2ReachabilityIndex index = area.rev_reach; index.value >= 0;
         index = file->reachabilities.list[index.value].rev_next) {
      ++incomingCount;
    }
    numAreaTravelTimes += outgoingCount * incomingCount;
  }

  areaTravelTimes = static_cast<unsigned short*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\engine\\gamelib\\aas2\\AAS2_routing.cpp(145) : TAG_AAS",
          2LL * numAreaTravelTimes, TAG_AAS, false, ALIGN_16,
          HEAP_DEFAULTHEAP));

  int travelTimeOffset = 0;
  for (int areaNum = 0; areaNum < file->areas.num; ++areaNum) {
    aas2Area_t& area = file->areas.list[areaNum];
    if ((area.flags & 2) == 0) {
      continue;
    }

    unsigned short maxAreaTravelTime = 0;
    int outgoingOrdinal = 0;
    for (idAAS2ReachabilityIndex outgoingIndex = area.reach;
         outgoingIndex.value >= 0;
         outgoingIndex =
             file->reachabilities.list[outgoingIndex.value].next) {
      aas2Reachability_t& outgoing =
          file->reachabilities.list[outgoingIndex.value];
      const idVec3 outgoingStart = Dequantize(outgoing.start);
      int incomingCount = 0;
      for (idAAS2ReachabilityIndex incomingIndex = area.rev_reach;
           incomingIndex.value >= 0;
           incomingIndex =
               file->reachabilities.list[incomingIndex.value].rev_next) {
        const aas2Reachability_t& incoming =
            file->reachabilities.list[incomingIndex.value];
        const unsigned short value =
            CalcTravelTime(outgoingStart, Dequantize(incoming.end),
                           area.travelFlags);
        if (value > maxAreaTravelTime) {
          maxAreaTravelTime = value;
        }
        areaTravelTimes[travelTimeOffset + incomingCount] =
            static_cast<unsigned short>(value + 1);
        ++incomingCount;
      }

      if (outgoingOrdinal >= 0x1000000) {
        idLib::Error("more than %d area travel times", 0x1000000);
        return;
      }
      if (outgoingOrdinal >= 256) {
        idLib::Error("area %d has more than %d reachabilities", areaNum,
                     256);
        __debugbreak();
      }
      outgoing.areaTTOfsAndNumber =
          travelTimeOffset | (outgoingOrdinal << 24);
      travelTimeOffset += incomingCount;
      ++outgoingOrdinal;
    }

    if (area.cluster < 0) {
      file->portals.list[-area.cluster].maxAreaTravelTime =
          maxAreaTravelTime;
    }
  }
}

void idAAS2Local::SetupRoutingCache() {
  areaCacheIndexSize = 0;
  for (int clusterNum = 0; clusterNum < file->clusters.num; ++clusterNum) {
    areaCacheIndexSize +=
        file->clusters.list[clusterNum].numReachableAreas;
  }

  areaCacheIndex = static_cast<idAASRoutingCache***>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\engine\\gamelib\\aas2\\AAS2_routing.cpp(223) : TAG_AAS",
          8LL * (areaCacheIndexSize + file->clusters.num), TAG_AAS, true,
          ALIGN_16, HEAP_DEFAULTHEAP));
  idAASRoutingCache** cachePointers =
      reinterpret_cast<idAASRoutingCache**>(areaCacheIndex +
                                            file->clusters.num);
  for (int clusterNum = 0; clusterNum < file->clusters.num; ++clusterNum) {
    areaCacheIndex[clusterNum] = cachePointers;
    cachePointers += file->clusters.list[clusterNum].numReachableAreas;
  }

  portalCacheIndexSize = file->areas.num;
  portalCacheIndex = static_cast<idAASRoutingCache**>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\engine\\gamelib\\aas2\\AAS2_routing.cpp(231) : TAG_AAS",
          8LL * portalCacheIndexSize, TAG_AAS, true, ALIGN_16,
          HEAP_DEFAULTHEAP));
  areaUpdate = static_cast<idRoutingUpdate*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\engine\\gamelib\\aas2\\AAS2_routing.cpp(233) : TAG_AAS",
          static_cast<std::size_t>(file->areas.num) << 6, TAG_AAS, true,
          ALIGN_16, HEAP_DEFAULTHEAP));
  portalUpdate = static_cast<idRoutingUpdate*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\engine\\gamelib\\aas2\\AAS2_routing.cpp(234) : TAG_AAS",
          static_cast<std::size_t>(file->portals.num + 1) << 6, TAG_AAS,
          true, ALIGN_16, HEAP_DEFAULTHEAP));
  goalAreaTravelTimes = static_cast<unsigned short*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\engine\\gamelib\\aas2\\AAS2_routing.cpp(235) : TAG_AAS",
          2LL * file->areas.num, TAG_AAS, true, ALIGN_16,
          HEAP_DEFAULTHEAP));
  cacheListEnd = nullptr;
  cacheListStart = nullptr;
  totalCacheMemory = 0;
}

void idAAS2Local::DeleteClusterCache(int clusterNum) {
  if (areaCacheIndex == nullptr) {
    return;
  }
  const int count = file->clusters.list[clusterNum].numReachableAreas;
  for (int clusterAreaNum = 0; clusterAreaNum < count; ++clusterAreaNum) {
    idAASRoutingCache** head =
        &areaCacheIndex[clusterNum][clusterAreaNum];
    while (*head != nullptr) {
      idAASRoutingCache* cache = *head;
      *head = cache->next;
      UnlinkCache(cache);
      DestroyRoutingCache(cache);
    }
  }
}

void idAAS2Local::DeletePortalCache() {
  for (int areaNum = 0; areaNum < file->areas.num; ++areaNum) {
    idAASRoutingCache** head = &portalCacheIndex[areaNum];
    while (*head != nullptr) {
      idAASRoutingCache* cache = *head;
      *head = cache->next;
      UnlinkCache(cache);
      DestroyRoutingCache(cache);
    }
  }
}

void idAAS2Local::ShutdownRoutingCache() {
  for (int clusterNum = 0; clusterNum < file->clusters.num; ++clusterNum) {
    DeleteClusterCache(clusterNum);
  }
  DeletePortalCache();
  idMem& memory = idMem::Singleton();
  memory.Free(areaCacheIndex, ALIGN_16);
  areaCacheIndex = nullptr;
  areaCacheIndexSize = 0;
  memory.Free(portalCacheIndex, ALIGN_16);
  portalCacheIndex = nullptr;
  portalCacheIndexSize = 0;
  memory.Free(areaUpdate, ALIGN_16);
  areaUpdate = nullptr;
  memory.Free(portalUpdate, ALIGN_16);
  portalUpdate = nullptr;
  memory.Free(goalAreaTravelTimes, ALIGN_16);
  goalAreaTravelTimes = nullptr;
  cacheListEnd = nullptr;
  cacheListStart = nullptr;
  totalCacheMemory = 0;
}

bool idAAS2Local::SetupRouting() {
  groundSpeedMultiplier = 100.0f / file->settings.groundSpeed;
  waterSpeedMultiplier = 100.0f / file->settings.waterSpeed;
  CalculateAreaTravelTimes();
  SetupRoutingCache();
  return true;
}

void idAAS2Local::ShutdownRouting() {
  idMem::Singleton().Free(areaTravelTimes, ALIGN_16);
  areaTravelTimes = nullptr;
  numAreaTravelTimes = 0;
  ShutdownRoutingCache();
}

void idAAS2Local::RemoveRoutingCacheUsingArea(int areaNum) {
  const int cluster = file->areas.list[areaNum].cluster;
  if (cluster <= 0) {
    const aas2Portal_t& portal = file->portals.list[-cluster];
    DeleteClusterCache(portal.clusters[0]);
    DeleteClusterCache(portal.clusters[1]);
  } else {
    DeleteClusterCache(cluster);
  }
  DeletePortalCache();
}

bool idAAS2Local::ChangeAreaTravelFlags_r(int nodeNum,
                                          const idBounds& bounds,
                                          int areaFlags, int travelFlags,
                                          bool set) {
  bool changedArea = false;
  if (nodeNum == 0) {
    return false;
  }

  while (nodeNum >= 0) {
    const aas2Node_t& node = file->nodes.list[nodeNum];
    const planeSide_t side =
        bounds.PlaneSide(file->planes.list[node.planeNum], 0.1f);
    if (side == PLANESIDE_BACK) {
      nodeNum = node.children[1];
    } else {
      if (side != PLANESIDE_FRONT &&
          (ChangeAreaTravelFlags_r(node.children[1], bounds, areaFlags,
                                   travelFlags, set) ||
           changedArea)) {
        changedArea = true;
      }
      nodeNum = node.children[0];
    }
    if (nodeNum == 0) {
      return changedArea;
    }
  }

  const int areaNum = -nodeNum;
  aas2Area_t& area = file->areas.list[areaNum];
  if ((area.flags & static_cast<unsigned short>(areaFlags)) == 0) {
    return changedArea;
  }
  if (set) {
    if ((travelFlags & area.travelFlags) == 0) {
      area.travelFlags |= travelFlags;
      RemoveRoutingCacheUsingArea(areaNum);
    }
  } else if ((travelFlags & area.travelFlags) != 0) {
    area.travelFlags &= ~travelFlags;
    RemoveRoutingCacheUsingArea(areaNum);
  }
  return true;
}

bool idAAS2Local::ChangeAreaTravelFlags(const idBounds& bounds,
                                         int areaFlags, int travelFlags,
                                         bool set,
                                         const char* debugName) {
  if (bounds.b[0].x > bounds.b[1].x) {
    idLib::Warning(
        "entity '%s' Trying to change area travel flags with invalid bounds",
        debugName != nullptr ? debugName : "<unknown>");
    return false;
  }
  if (file == nullptr) {
    return false;
  }

  idBounds expandedBounds;
  expandedBounds.b[0].x =
      bounds.b[0].x - file->settings.boundingBox.b[1].x;
  expandedBounds.b[0].y =
      bounds.b[0].y - file->settings.boundingBox.b[1].y;
  expandedBounds.b[0].z =
      bounds.b[0].z - file->settings.boundingBox.b[1].z;
  expandedBounds.b[1].x =
      bounds.b[1].x - file->settings.boundingBox.b[0].x;
  expandedBounds.b[1].y =
      bounds.b[1].y - file->settings.boundingBox.b[0].y;
  expandedBounds.b[1].z =
      bounds.b[1].z - file->settings.boundingBox.b[0].z;
  return ChangeAreaTravelFlags_r(1, expandedBounds, areaFlags, travelFlags,
                                 set);
}

bool idAAS2Local::ChangeReachabilityTravelFlags(
    idAAS2ReachabilityIndex index, int travelFlag, bool set) {
  if (file == nullptr || index.value < 0) {
    return false;
  }
  aas2Reachability_t& reachability = file->reachabilities.list[index.value];
  if (set) {
    reachability.travelFlags |= travelFlag;
  } else {
    reachability.travelFlags &= ~travelFlag;
  }
  RemoveRoutingCacheUsingArea(reachability.fromAreaNum);
  RemoveRoutingCacheUsingArea(reachability.toAreaNum);
  return true;
}

int idAAS2Local::ClusterAreaNum(int clusterNum, int areaNum) const {
  const aas2Area_t& area = file->areas.list[areaNum];
  if (area.cluster > 0) {
    return area.clusterAreaNum;
  }
  const aas2Portal_t& portal = file->portals.list[-area.cluster];
  return portal.clusterAreaNum[portal.clusters[0] != clusterNum];
}

bool idAAS2Local::FindNearestGoal(idAAS2Goal& goal, int startAreaNum,
                                   const idVec3& startOrigin,
                                   int travelFlags,
                                   idAAS2Callback& callback) const {
  if (file == nullptr || startAreaNum <= 0 ||
      goalAreaTravelTimes == nullptr || areaUpdate == nullptr) {
    goal.areaNum = startAreaNum;
    goal.origin = startOrigin;
    return false;
  }
  if (callback.AreaIsGoal(this, startAreaNum, startOrigin)) {
    goal.areaNum = startAreaNum;
    goal.origin = startOrigin;
    return true;
  }

  std::memset(goalAreaTravelTimes, 0,
              2LL * static_cast<std::size_t>(file->areas.num));
  idRoutingUpdate* head = &areaUpdate[startAreaNum];
  idRoutingUpdate* tail = head;
  head->areaNum = startAreaNum;
  head->tmpTravelTime = 0;
  head->start = startOrigin;
  head->next = nullptr;
  head->prev = nullptr;

  unsigned short bestTravelTime = 0;
  int bestAreaNum = 0;
  while (head != nullptr) {
    idRoutingUpdate* current = head;
    head = current->next;
    if (head != nullptr) {
      head->prev = nullptr;
    } else {
      tail = nullptr;
    }
    current->isInList = false;

    if (bestTravelTime != 0 && current->tmpTravelTime >= bestTravelTime) {
      continue;
    }

    for (idAAS2ReachabilityIndex reachIndex =
             file->areas.list[current->areaNum].reach;
         reachIndex.value >= 0;
         reachIndex = file->reachabilities.list[reachIndex.value].next) {
      const aas2Reachability_t& reachability =
          file->reachabilities.list[reachIndex.value];
      if (!TravelFlagsAllowed(reachability.travelFlags, travelFlags)) {
        continue;
      }
      const int toAreaNum = reachability.toAreaNum;
      if (!TravelFlagsAllowed(file->areas.list[toAreaNum].travelFlags,
                              travelFlags)) {
        continue;
      }

      const idVec3 reachStart = Dequantize(reachability.start);
      const idVec3 reachEnd = Dequantize(reachability.end);
      unsigned short newTravelTime = static_cast<unsigned short>(
          current->tmpTravelTime + reachability.travelTime +
          CalcTravelTime(current->start, reachStart,
                         file->areas.list[current->areaNum].travelFlags));
      newTravelTime = static_cast<unsigned short>(
          newTravelTime + callback.AdditionalTravelTimeForPath(
                              this, current->start, reachEnd));
      if ((bestTravelTime != 0 && newTravelTime >= bestTravelTime) ||
          (goalAreaTravelTimes[toAreaNum] != 0 &&
           newTravelTime >= goalAreaTravelTimes[toAreaNum])) {
        continue;
      }
      if (!callback.PathValid(this, current->start, reachEnd)) {
        continue;
      }

      goalAreaTravelTimes[toAreaNum] = newTravelTime;
      idRoutingUpdate* update = &areaUpdate[toAreaNum];
      update->areaNum = toAreaNum;
      update->tmpTravelTime = newTravelTime;
      update->start = reachEnd;
      if ((file->areas.list[toAreaNum].flags & 1) != 0) {
        update->tmpTravelTime =
            static_cast<unsigned short>(update->tmpTravelTime + 250);
      }
      if (!update->isInList) {
        update->next = nullptr;
        update->prev = tail;
        if (tail != nullptr) {
          tail->next = update;
        } else {
          head = update;
        }
        tail = update;
        update->isInList = true;
      }

      if ((file->areas.list[toAreaNum].flags & 1) != 0) {
        continue;
      }
      const idVec3 center = AreaCenter(toAreaNum);
      const unsigned short goalTravelTime = static_cast<unsigned short>(
          newTravelTime +
          CalcTravelTime(reachEnd, center,
                         file->areas.list[toAreaNum].travelFlags));
      if (bestTravelTime != 0 && goalTravelTime >= bestTravelTime) {
        continue;
      }
      if (callback.AreaIsGoal(this, toAreaNum, reachEnd)) {
        bestAreaNum = toAreaNum;
        bestTravelTime = goalTravelTime;
      }
    }
  }

  if (bestAreaNum == 0) {
    return false;
  }
  goal.areaNum = bestAreaNum;
  goal.origin = AreaCenter(bestAreaNum);
  return true;
}

bool idAAS2Local::NearestPointReachable(
    idAAS2TreeIndex tree, const idVec3& source, int sourceAreaNum,
    const idVec3& destination, const idBounds& destinationBounds,
    int travelFlags, idAAS2NearestReachable& nearestInfo) const {
  int boundsAreas[32] = {};
  const int numBoundsAreas =
      file->BoundsAreaNums(tree, destinationBounds, boundsAreas, 32);
  if (numBoundsAreas == 0 || sourceAreaNum == 0) {
    return false;
  }

  idAAS2NearestReachable nearest = {};
  float nearestDistanceSquared = FLT_MAX;
  int nearestTravelTime = INT_MAX;
  int quickestTravelTime = INT_MAX;
  for (int index = 0; index < numBoundsAreas; ++index) {
    idAAS2ReachabilityIndex reachIndex;
    reachIndex.value = -1;
    int routeTravelTime = 0;
    if (!RouteToGoalArea(sourceAreaNum, source, boundsAreas[index],
                         destination, travelFlags, routeTravelTime,
                         reachIndex)) {
      continue;
    }

    idVec3 pushedDestination = destination;
    PushPointIntoAreaNum(boundsAreas[index], pushedDestination);
    if (routeTravelTime < quickestTravelTime) {
      nearest.quickestDestArea = boundsAreas[index];
      quickestTravelTime = routeTravelTime;
      nearest.quickestDestPos = pushedDestination;
    }

    const float deltaX = destination.x - pushedDestination.x;
    const float deltaY = destination.y - pushedDestination.y;
    const float deltaZ = destination.z - pushedDestination.z;
    const float distanceSquared =
        (deltaX * deltaX + deltaY * deltaY) + deltaZ * deltaZ;
    if (distanceSquared < nearestDistanceSquared) {
      nearest.nearestDestArea = boundsAreas[index];
      nearestDistanceSquared = distanceSquared;
      nearest.nearestDestPos = pushedDestination;
      nearestTravelTime = routeTravelTime;
    }
  }

  nearest.nearestTravelTime = nearestTravelTime;
  nearest.quickestTravelTime = quickestTravelTime;
  nearestInfo = nearest;
  return nearest.quickestDestArea != 0 && nearest.nearestDestArea != 0;
}

void idAAS2Local::DeleteOldestCache() const {
  idAASRoutingCache* cache = cacheListStart;
  if (cache == nullptr) {
    return;
  }
  UnlinkCache(cache);
  if (cache->next != nullptr) {
    cache->next->prev = cache->prev;
  }
  if (cache->prev != nullptr) {
    cache->prev->next = cache->next;
  } else if (cache->type == 1) {
    if (areaCacheIndex != nullptr) {
      areaCacheIndex[cache->cluster]
                    [ClusterAreaNum(cache->cluster, cache->areaNum)] =
          cache->next;
    }
  } else if (cache->type == 2 && portalCacheIndex != nullptr) {
    portalCacheIndex[cache->areaNum] = cache->next;
  }
  DestroyRoutingCache(cache);
}

void idAAS2Local::UpdateAreaRoutingCache(
    idAASRoutingCache* areaCache) const {
  if (areaUpdate == nullptr || areaTravelTimes == nullptr) {
    return;
  }

  const int clusterNum = areaCache->cluster;
  const int numReachableAreas =
      file->clusters.list[clusterNum].numReachableAreas;
  const int startClusterAreaNum =
      ClusterAreaNum(clusterNum, areaCache->areaNum);
  if (startClusterAreaNum >= numReachableAreas) {
    return;
  }

  areaCache->travelTimes[startClusterAreaNum] = areaCache->startTravelTime;
  unsigned short startAreaTravelTimes[256] = {};
  idRoutingUpdate* head = &areaUpdate[startClusterAreaNum];
  idRoutingUpdate* tail = head;
  head->areaNum = areaCache->areaNum;
  head->areaTravelTimes = startAreaTravelTimes;
  head->tmpTravelTime = areaCache->startTravelTime;
  head->next = nullptr;
  head->prev = nullptr;

  while (head != nullptr) {
    idRoutingUpdate* current = head;
    head = current->next;
    if (head != nullptr) {
      head->prev = nullptr;
    } else {
      tail = nullptr;
    }
    current->isInList = false;

    int reverseOrdinal = 0;
    for (idAAS2ReachabilityIndex reachIndex =
             file->areas.list[current->areaNum].rev_reach;
         reachIndex.value >= 0;
         reachIndex = file->reachabilities.list[reachIndex.value].rev_next,
                                  ++reverseOrdinal) {
      const aas2Reachability_t& reachability =
          file->reachabilities.list[reachIndex.value];
      if (!TravelFlagsAllowed(reachability.travelFlags,
                              areaCache->travelFlags)) {
        continue;
      }

      if ((reachability.travelFlags & 0x1870000) != 0) {
        idStaticList<int, 32> traversalIndices;
        file->GetTraversalsForReachability(reachIndex, traversalIndices);
        if (traversalIndices.num <= 0) {
          continue;
        }
      }
      if ((reachability.travelFlags & 0x2400000) != 0 &&
          (reachability.travelFlags & 0x1870000) == 0) {
        continue;
      }

      const int fromAreaNum = reachability.fromAreaNum;
      const aas2Area_t& fromArea = file->areas.list[fromAreaNum];
      if (!TravelFlagsAllowed(fromArea.travelFlags,
                              areaCache->travelFlags)) {
        continue;
      }
      if (fromArea.cluster > 0 && fromArea.cluster != clusterNum) {
        continue;
      }

      const int fromClusterAreaNum =
          ClusterAreaNum(clusterNum, fromAreaNum);
      if (fromClusterAreaNum >= numReachableAreas) {
        continue;
      }
      const unsigned short newTravelTime =
          static_cast<unsigned short>(current->tmpTravelTime +
                                      reachability.travelTime +
                                      current->areaTravelTimes[reverseOrdinal]);
      const unsigned short oldTravelTime =
          areaCache->travelTimes[fromClusterAreaNum];
      if (oldTravelTime != 0 && newTravelTime >= oldTravelTime) {
        continue;
      }

      areaCache->travelTimes[fromClusterAreaNum] = newTravelTime;
      areaCache->reachabilities[fromClusterAreaNum] =
          static_cast<unsigned char>(reachability.areaTTOfsAndNumber >> 24);
      idRoutingUpdate* update = &areaUpdate[fromClusterAreaNum];
      update->areaNum = fromAreaNum;
      update->tmpTravelTime = newTravelTime;
      update->areaTravelTimes =
          &areaTravelTimes[reachability.areaTTOfsAndNumber & 0xffffff];
      if ((fromArea.flags & 1) != 0) {
        update->tmpTravelTime =
            static_cast<unsigned short>(newTravelTime + 250);
      }
      if (!update->isInList) {
        update->next = nullptr;
        update->prev = tail;
        if (tail != nullptr) {
          tail->next = update;
        } else {
          head = update;
        }
        tail = update;
        update->isInList = true;
      }
    }
  }
}

idAASRoutingCache* idAAS2Local::GetAreaRoutingCache(
    int clusterNum, int areaNum, int travelFlags) const {
  if (areaCacheIndex == nullptr) {
    return nullptr;
  }

  const int clusterAreaNum = ClusterAreaNum(clusterNum, areaNum);
  idAASRoutingCache* oldHead =
      areaCacheIndex[clusterNum][clusterAreaNum];
  idAASRoutingCache* cache = oldHead;
  while (cache != nullptr && cache->travelFlags != travelFlags) {
    cache = cache->next;
  }

  if (cache == nullptr) {
    std::uint64_t startTime = 0;
    if (aas_showCacheMisses.valueInteger != 0) {
      startTime = Sys_Microseconds();
    }
    cache = new (aas2RoutingCacheTag_t::Cache)
        idAASRoutingCache(
            file->clusters.list[clusterNum].numReachableAreas);
    cache->type = 1;
    cache->cluster = clusterNum;
    cache->areaNum = areaNum;
    cache->startTravelTime = 1;
    cache->travelFlags = travelFlags;
    cache->prev = nullptr;
    cache->next = oldHead;
    if (oldHead != nullptr) {
      oldHead->prev = cache;
    }
    areaCacheIndex[clusterNum][clusterAreaNum] = cache;
    UpdateAreaRoutingCache(cache);
    if (aas_showCacheMisses.valueInteger != 0) {
      const std::uint64_t elapsed = Sys_Microseconds() - startTime;
      idLib::Warning(
          "AAS cache miss. %.2f ms to build cache for area %d.",
          static_cast<float>(elapsed) / 1000.0f, areaNum);
    }
  }

  if (cache->time_next != nullptr || cache->time_prev != nullptr ||
      cacheListStart == cache) {
    UnlinkCache(cache);
  }
  LinkRoutingCache(this, cache);
  return cache;
}

void idAAS2Local::UpdatePortalRoutingCache(
    idAASRoutingCache* portalCache) const {
  if (portalUpdate == nullptr) {
    return;
  }

  idRoutingUpdate* head = &portalUpdate[file->portals.num];
  idRoutingUpdate* tail = head;
  head->cluster = portalCache->cluster;
  head->areaNum = portalCache->areaNum;
  head->tmpTravelTime = portalCache->startTravelTime;
  head->next = nullptr;
  head->prev = nullptr;

  while (head != nullptr) {
    idRoutingUpdate* current = head;
    head = current->next;
    if (head != nullptr) {
      head->prev = nullptr;
    } else {
      tail = nullptr;
    }
    current->isInList = false;

    const aas2Cluster_t& cluster =
        file->clusters.list[current->cluster];
    idAASRoutingCache* areaCache =
        GetAreaRoutingCache(current->cluster, current->areaNum,
                            portalCache->travelFlags);
    for (int portalOffset = 0; portalOffset < cluster.numPortals;
         ++portalOffset) {
      const int portalNum =
          file->portalIndex.list[cluster.firstPortal + portalOffset];
      const aas2Portal_t& portal = file->portals.list[portalNum];
      const int clusterAreaNum =
          ClusterAreaNum(current->cluster, portal.areaNum);
      if (clusterAreaNum >= cluster.numReachableAreas) {
        continue;
      }
      const unsigned short areaTravelTime =
          areaCache->travelTimes[clusterAreaNum];
      if (areaTravelTime == 0) {
        continue;
      }

      const unsigned short newTravelTime = static_cast<unsigned short>(
          current->tmpTravelTime + areaTravelTime);
      const unsigned short oldTravelTime =
          portalCache->travelTimes[portalNum];
      if (oldTravelTime != 0 && newTravelTime >= oldTravelTime) {
        continue;
      }

      portalCache->travelTimes[portalNum] = newTravelTime;
      portalCache->reachabilities[portalNum] =
          areaCache->reachabilities[clusterAreaNum];
      idRoutingUpdate* update = &portalUpdate[portalNum];
      update->cluster = portal.clusters[0];
      if (update->cluster == current->cluster) {
        update->cluster = portal.clusters[1];
      }
      update->areaNum = portal.areaNum;
      update->tmpTravelTime = static_cast<unsigned short>(
          newTravelTime + portal.maxAreaTravelTime);
      if (!update->isInList) {
        update->next = nullptr;
        update->prev = tail;
        if (tail != nullptr) {
          tail->next = update;
        } else {
          head = update;
        }
        tail = update;
        update->isInList = true;
      }
    }
  }
}

idAASRoutingCache* idAAS2Local::GetPortalRoutingCache(
    int clusterNum, int areaNum, int travelFlags) const {
  if (portalCacheIndex == nullptr) {
    return nullptr;
  }

  idAASRoutingCache* cache = portalCacheIndex[areaNum];
  while (cache != nullptr && cache->travelFlags != travelFlags) {
    cache = cache->next;
  }
  if (cache == nullptr) {
    cache = new (aas2RoutingCacheTag_t::Cache)
        idAASRoutingCache(file->portals.num);
    cache->type = 2;
    cache->cluster = clusterNum;
    cache->areaNum = areaNum;
    cache->startTravelTime = 1;
    cache->travelFlags = travelFlags;
    cache->prev = nullptr;
    cache->next = portalCacheIndex[areaNum];
    if (portalCacheIndex[areaNum] != nullptr) {
      portalCacheIndex[areaNum]->prev = cache;
    }
    portalCacheIndex[areaNum] = cache;
    UpdatePortalRoutingCache(cache);
  }

  if (cache->time_next != nullptr || cache->time_prev != nullptr ||
      cacheListStart == cache) {
    UnlinkCache(cache);
  }
  LinkRoutingCache(this, cache);
  return cache;
}

bool idAAS2Local::GetClusterRoute(
    int startAreaNum, const idVec3& startOrigin, int startClusterNum,
    int goalAreaNum, int travelFlags, int& travelTime,
    idAAS2ReachabilityIndex& bestReachIndex) const {
  idAASRoutingCache* areaCache = GetAreaRoutingCache(
      startClusterNum, goalAreaNum, travelFlags);
  const int startClusterAreaNum =
      ClusterAreaNum(startClusterNum, startAreaNum);
  if (areaCache->travelTimes[startClusterAreaNum] == 0) {
    return false;
  }

  bestReachIndex = ResolveReachability(
      *file, startAreaNum,
      areaCache->reachabilities[startClusterAreaNum]);
  const aas2Reachability_t& bestReachability =
      file->reachabilities.list[bestReachIndex.value];
  travelTime = areaCache->travelTimes[startClusterAreaNum] +
               CalcTravelTime(startOrigin,
                              Dequantize(bestReachability.start),
                              file->areas.list[startAreaNum].travelFlags);

  for (idAAS2ReachabilityIndex candidateIndex =
           file->areas.list[startAreaNum].reach;
       candidateIndex.value >= 0;
       candidateIndex =
           file->reachabilities.list[candidateIndex.value].next) {
    if (candidateIndex.value == bestReachIndex.value) {
      continue;
    }
    const aas2Reachability_t& candidate =
        file->reachabilities.list[candidateIndex.value];
    if (!TravelFlagsAllowed(candidate.travelFlags, travelFlags) ||
        ((candidate.travelFlags & 0x2400000) != 0 &&
         (candidate.travelFlags & 0x1870000) == 0)) {
      continue;
    }

    const int toAreaNum = candidate.toAreaNum;
    const aas2Area_t& toArea = file->areas.list[toAreaNum];
    if (toArea.cluster != startClusterNum) {
      continue;
    }
    const int toClusterAreaNum =
        ClusterAreaNum(startClusterNum, toAreaNum);
    const unsigned short cachedTravelTime =
        areaCache->travelTimes[toClusterAreaNum];
    if (cachedTravelTime == 0) {
      continue;
    }

    const idAAS2ReachabilityIndex nextIndex = ResolveReachability(
        *file, toAreaNum,
        areaCache->reachabilities[toClusterAreaNum]);
    if (nextIndex.value < 0) {
      continue;
    }
    const aas2Reachability_t& next =
        file->reachabilities.list[nextIndex.value];
    if (next.toAreaNum == startAreaNum) {
      continue;
    }

    const int candidateTravelTime =
        CalcTravelTime(startOrigin, Dequantize(candidate.start),
                       file->areas.list[startAreaNum].travelFlags) +
        candidate.travelTime +
        CalcTravelTime(Dequantize(candidate.end), Dequantize(next.start),
                       toArea.travelFlags) +
        cachedTravelTime;
    if (candidateTravelTime < travelTime) {
      travelTime = candidateTravelTime;
      bestReachIndex = candidateIndex;
    }
  }
  return true;
}

bool idAAS2Local::RouteToGoalArea(
    int startAreaNum, const idVec3& startOrigin, int goalAreaNum,
    const idVec3& goalOrigin, int travelFlags, int& travelTime,
    idAAS2ReachabilityIndex& reachIndex) const {
  travelTime = 0;
  reachIndex.value = -1;
  if (file == nullptr) {
    return false;
  }
  if (startAreaNum == goalAreaNum) {
    travelTime = CalcTravelTime(
        startOrigin, goalOrigin,
        file->areas.list[startAreaNum].travelFlags);
    return true;
  }
  if (startAreaNum <= 0 || startAreaNum >= file->areas.num) {
    idLib::Warning("RouteToGoalArea: startAreaNum %d out of range.",
                   startAreaNum);
    idLib::Warning("Start Origin: X: %0.f, Y: %0.f, Z: %0.f",
                   startOrigin.x, startOrigin.y, startOrigin.z);
    return false;
  }
  if (goalAreaNum <= 0 || goalAreaNum >= file->areas.num) {
    idLib::Warning("RouteToGoalArea: goalAreaNum %d out of range",
                   goalAreaNum);
    return false;
  }

  while (totalCacheMemory > 0x200000) {
    DeleteOldestCache();
  }

  const int startCluster = file->areas.list[startAreaNum].cluster;
  int goalCluster = file->areas.list[goalAreaNum].cluster;
  if (startCluster < 0) {
    if (goalCluster < 0) {
      goalCluster = file->portals.list[-goalCluster].clusters[0];
    }
    idAASRoutingCache* portalCache =
        GetPortalRoutingCache(goalCluster, goalAreaNum, travelFlags);
    reachIndex = ResolveReachability(
        *file, startAreaNum, portalCache->reachabilities[-startCluster]);
    travelTime = portalCache->travelTimes[-startCluster] +
                 CalcTravelTime(
                     startOrigin,
                     Dequantize(
                         file->reachabilities.list[reachIndex.value].start),
                     file->areas.list[startAreaNum].travelFlags);
    return true;
  }

  int bestTravelTime = 0;
  idAAS2ReachabilityIndex bestReachIndex;
  bestReachIndex.value = -1;
  int directGoalCluster = goalCluster;
  if (directGoalCluster < 0) {
    const aas2Portal_t& goalPortal =
        file->portals.list[-directGoalCluster];
    if (goalPortal.clusters[0] == startCluster ||
        goalPortal.clusters[1] == startCluster) {
      directGoalCluster = startCluster;
    }
  }
  if (startCluster > 0 && directGoalCluster > 0 &&
      startCluster == directGoalCluster) {
    GetClusterRoute(startAreaNum, startOrigin, startCluster, goalAreaNum,
                    travelFlags, bestTravelTime, bestReachIndex);
  }

  goalCluster = file->areas.list[goalAreaNum].cluster;
  if (goalCluster < 0) {
    goalCluster = file->portals.list[-goalCluster].clusters[0];
  }
  idAASRoutingCache* portalCache =
      GetPortalRoutingCache(goalCluster, goalAreaNum, travelFlags);
  const aas2Cluster_t& cluster = file->clusters.list[startCluster];
  if (ClusterAreaNum(startCluster, startAreaNum) >=
      cluster.numReachableAreas) {
    return false;
  }

  for (int portalOffset = 0; portalOffset < cluster.numPortals;
       ++portalOffset) {
    const int portalNum =
        file->portalIndex.list[cluster.firstPortal + portalOffset];
    if (portalCache->travelTimes[portalNum] == 0) {
      continue;
    }
    const aas2Portal_t& portal = file->portals.list[portalNum];
    int clusterTravelTime = 0;
    idAAS2ReachabilityIndex testReachIndex;
    testReachIndex.value = -1;
    if (GetClusterRoute(startAreaNum, startOrigin, startCluster,
                        portal.areaNum, travelFlags, clusterTravelTime,
                        testReachIndex)) {
      const int candidateTravelTime =
          clusterTravelTime + portal.maxAreaTravelTime +
          portalCache->travelTimes[portalNum];
      if (bestTravelTime == 0 || candidateTravelTime < bestTravelTime) {
        bestReachIndex = testReachIndex;
        bestTravelTime = candidateTravelTime;
      }
    }
  }

  if (bestReachIndex.value < 0) {
    return false;
  }
  reachIndex = bestReachIndex;
  travelTime = bestTravelTime;
  return true;
}
