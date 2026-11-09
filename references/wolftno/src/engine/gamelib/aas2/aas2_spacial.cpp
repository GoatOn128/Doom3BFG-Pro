#include "engine/gamelib/aas2/aas2.h"

#include "shared/idlib/lib.h"

#include <cstdint>
#include <cstring>
#include <malloc.h>
#include <xmmintrin.h>

namespace {

constexpr std::size_t REACHABILITIES_OFFSET = 0x218;
constexpr std::size_t AREAS_OFFSET = 0x230;
constexpr std::size_t COVER_OFFSET = 0x338;
constexpr std::size_t AREA_COVER_INDEX_OFFSET = 0x350;
constexpr std::size_t CHOKE_POINTS_OFFSET = 0x380;
constexpr std::size_t HINT_NODES_OFFSET = 0x3B0;
constexpr std::size_t AREA_BOUNDS_OFFSET = 0x3C8;

template <typename Type> struct listABI_t {
  Type* list;
  int num;
  int size;
  std::uint16_t granularity;
  std::uint8_t memTag;
  std::uint8_t listStatic;
  std::uint8_t padding[4];
};

struct areaABI_t {
  std::uint8_t prefix[0x14];
  std::int16_t reach;
  std::uint8_t middle[6];
  std::uint16_t firstCover;
  std::uint16_t numCover;
  std::uint8_t tail[8];
};

struct reachabilityABI_t {
  int travelFlags;
  std::uint8_t prefix[4];
  std::uint16_t toAreaNum;
  std::uint8_t middle[0x12];
  std::int16_t next;
  std::uint8_t tail[2];
};

struct coverABI_t {
  idVec3 origin;
  std::uint8_t prefix[0x0E];
  std::int16_t flags;
  std::uint8_t tail[0x1C];
};

struct areaBoundsABI_t {
  std::int16_t min[3];
  std::int16_t max[3];
};

static_assert(sizeof(listABI_t<int>) == 0x18, "retail list ABI drift");
static_assert(sizeof(areaABI_t) == 0x28, "retail AAS area ABI drift");
static_assert(offsetof(areaABI_t, reach) == 0x14,
              "retail AAS area reach offset drift");
static_assert(offsetof(areaABI_t, firstCover) == 0x1C,
              "retail AAS area cover offset drift");
static_assert(sizeof(reachabilityABI_t) == 0x20,
              "retail AAS reachability ABI drift");
static_assert(offsetof(reachabilityABI_t, toAreaNum) == 0x08,
              "retail reachability destination offset drift");
static_assert(offsetof(reachabilityABI_t, next) == 0x1C,
              "retail reachability link offset drift");
static_assert(sizeof(coverABI_t) == 0x38, "retail AAS cover ABI drift");
static_assert(offsetof(coverABI_t, flags) == 0x1A,
              "retail AAS cover flags offset drift");
static_assert(sizeof(areaBoundsABI_t) == 0x0C,
              "retail AAS area bounds ABI drift");

const std::uint8_t* FileBytes(const idAAS2Local& aas) {
  return reinterpret_cast<const std::uint8_t*>(aas.file);
}

template <typename Type>
const listABI_t<Type>& FileList(const idAAS2Local& aas,
                                std::size_t offset) {
  return *reinterpret_cast<const listABI_t<Type>*>(FileBytes(aas) + offset);
}

__forceinline int GetNumAreasVirtual(const idAAS2Local& aas) {
  return aas.GetNumAreas();
}

float RetailMinimum(float first, float second) {
  return _mm_cvtss_f32(_mm_min_ss(_mm_set_ss(first), _mm_set_ss(second)));
}

float RetailMaximum(float first, float second) {
  return _mm_cvtss_f32(_mm_max_ss(_mm_set_ss(first), _mm_set_ss(second)));
}

std::int16_t QuantizedMinimum(float value) {
  const int converted = static_cast<int>(value + 65535.0f);
  return static_cast<std::int16_t>(
      static_cast<std::uint16_t>(converted) + std::uint16_t{1});
}

std::int16_t QuantizedMaximum(float value) {
  const int converted = static_cast<int>(value - 65535.0f);
  return static_cast<std::int16_t>(
      static_cast<std::uint16_t>(converted) - std::uint16_t{1});
}

bool BoundsIntersect(const std::int16_t queryMin[3],
                     const std::int16_t queryMax[3],
                     const areaBoundsABI_t& areaBounds) {
  return queryMin[0] <= areaBounds.max[0] &&
         queryMax[0] >= areaBounds.min[0] &&
         queryMin[1] <= areaBounds.max[1] &&
         queryMax[1] >= areaBounds.min[1] &&
         queryMin[2] <= areaBounds.max[2] &&
         queryMax[2] >= areaBounds.min[2];
}

} // namespace

int idAAS2Local::GetNumCoverPoints() const {
  return FileList<coverABI_t>(*this, COVER_OFFSET).num;
}

const aas2Cover_t* idAAS2Local::GetCoverPoint(int coverIndex) const {
  const coverABI_t* cover = FileList<coverABI_t>(*this, COVER_OFFSET).list;
  return reinterpret_cast<const aas2Cover_t*>(&cover[coverIndex]);
}

int idAAS2Local::GetNumAreaCoverPoints(int areaNum) const {
  if (areaNum <= 0) {
    return 0;
  }
  return FileList<areaABI_t>(*this, AREAS_OFFSET).list[areaNum].numCover;
}

int idAAS2Local::GetAreaCoverPoints(int areaNum, int* coverPoints,
                                    int maxCover, int& numCover) const {
  numCover = 0;
  if (areaNum <= 0) {
    return 0;
  }

  const areaABI_t* areas = FileList<areaABI_t>(*this, AREAS_OFFSET).list;
  const areaABI_t& area = areas[areaNum];
  for (int index = 0; index < area.numCover; ++index) {
    if (numCover >= maxCover) {
      idLib::Warning(
          "idAAS2Local::GetAreaCoverPoints - search list overflowed.");
    } else {
      const int* areaCoverIndex =
          FileList<int>(*this, AREA_COVER_INDEX_OFFSET).list;
      coverPoints[numCover] = areaCoverIndex[area.firstCover + index];
      ++numCover;
    }
  }
  return numCover;
}

void idAAS2Local::FindCoverPoints_Internal(
    int startAreaNum, const idVec3& origin, const idBounds& bounds,
    float maxDistSqr, int requiredFlags, int excludeFlags, int travelFlags,
    int* coverPoints, int maxCover, int& numCover) const {
  const int roundedAreaCount = (GetNumAreasVirtual(*this) + 127) & ~127;
  const int visitedStorageSize = ((roundedAreaCount + 127) & ~127) + 128;
  auto* visitedStorage = static_cast<std::uint8_t*>(
      _alloca(static_cast<std::size_t>(visitedStorageSize)));
  auto* visited = reinterpret_cast<std::uint8_t*>(
      (reinterpret_cast<std::uintptr_t>(visitedStorage) + 127u) &
      ~std::uintptr_t{127});
  if (roundedAreaCount > 0) {
    std::memset(visited, 0, static_cast<std::size_t>(roundedAreaCount));
  }

  int searchAreas[1280];
  int queuedAreaCount = 1;
  int searchIndex = 0;
  searchAreas[0] = startAreaNum;
  visited[startAreaNum] = 1;

  const std::int16_t queryMin[3] = {
      QuantizedMinimum(bounds[0].x), QuantizedMinimum(bounds[0].y),
      QuantizedMinimum(bounds[0].z)};
  const std::int16_t queryMax[3] = {
      QuantizedMaximum(bounds[1].x), QuantizedMaximum(bounds[1].y),
      QuantizedMaximum(bounds[1].z)};

  do {
    if (queuedAreaCount >= 1280) {
      return;
    }

    const int areaNum = searchAreas[searchIndex];
    const areaABI_t* areas = FileList<areaABI_t>(*this, AREAS_OFFSET).list;
    const areaABI_t& area = areas[areaNum];

    for (int coverOffset = 0; coverOffset < area.numCover; ++coverOffset) {
      const int* areaCoverIndex =
          FileList<int>(*this, AREA_COVER_INDEX_OFFSET).list;
      const int coverIndex = areaCoverIndex[area.firstCover + coverOffset];
      const coverABI_t& cover =
          FileList<coverABI_t>(*this, COVER_OFFSET).list[coverIndex];
      if ((requiredFlags == 0 || (cover.flags & requiredFlags) != 0) &&
          (cover.flags & excludeFlags) == 0) {
        const float deltaX = cover.origin.x - origin.x;
        const float deltaY = cover.origin.y - origin.y;
        const float deltaZ = cover.origin.z - origin.z;
        const float distanceSqr =
            ((deltaX * deltaX) + (deltaY * deltaY)) +
            (deltaZ * deltaZ);
        if (maxDistSqr > distanceSqr) {
          if (numCover >= maxCover) {
            idLib::Warning(
                "idAAS2Local::FindCoverPoints_Internal - search list "
                "overflowed.");
            return;
          }
          coverPoints[numCover] = coverIndex;
          ++numCover;
        }
      }
    }

    std::int16_t reachabilityIndex = area.reach;
    while (reachabilityIndex >= 0) {
      const reachabilityABI_t* reachabilities =
          FileList<reachabilityABI_t>(*this, REACHABILITIES_OFFSET).list;
      const reachabilityABI_t& reachability =
          reachabilities[reachabilityIndex];
      const int destinationArea = reachability.toAreaNum;
      if (visited[destinationArea] != 1 &&
          (travelFlags & reachability.travelFlags) != 0) {
        visited[destinationArea] = 1;
        const areaBoundsABI_t* areaBounds =
            FileList<areaBoundsABI_t>(*this, AREA_BOUNDS_OFFSET).list;
        if (BoundsIntersect(queryMin, queryMax,
                            areaBounds[destinationArea])) {
          ++queuedAreaCount;
          searchAreas[queuedAreaCount - 1] = destinationArea;
          if (queuedAreaCount >= 1280) {
            break;
          }
        }
      }
      reachabilityIndex = reachabilities[reachabilityIndex].next;
    }
    ++searchIndex;
  } while (searchIndex < queuedAreaCount);
}

int idAAS2Local::FindCoverPoints(int areaNum, const idVec3& origin,
                                 float maxDist, int requiredFlags,
                                 int excludeFlags, int travelFlags,
                                 int* coverPoints, int maxCover,
                                 int& numCover) const {
  numCover = 0;
  if (areaNum <= 0) {
    return 0;
  }

  idBounds bounds;
  const float plusX = origin.x + maxDist;
  const float plusY = origin.y + maxDist;
  const float plusZ = origin.z + maxDist;
  const float minusX = origin.x - maxDist;
  const float minusY = origin.y - maxDist;
  const float minusZ = origin.z - maxDist;
  bounds[0].x = RetailMinimum(plusX, minusX);
  bounds[0].y = RetailMinimum(plusY, minusY);
  bounds[0].z = RetailMinimum(plusZ, minusZ);
  bounds[1].x = RetailMaximum(plusX, minusX);
  bounds[1].y = RetailMaximum(plusY, minusY);
  bounds[1].z = RetailMaximum(plusZ, minusZ);
  FindCoverPoints_Internal(areaNum, origin, bounds, maxDist * maxDist,
                           requiredFlags, excludeFlags, travelFlags,
                           coverPoints, maxCover, numCover);
  return numCover;
}

int idAAS2Local::GetNumChokePoints() const {
  return FileList<std::uint8_t>(*this, CHOKE_POINTS_OFFSET).num;
}

const aas2ChokePoint_t* idAAS2Local::GetChokePoint(int chokeIndex) const {
  const std::uint8_t* chokePoints =
      FileList<std::uint8_t>(*this, CHOKE_POINTS_OFFSET).list;
  const std::ptrdiff_t offset =
      static_cast<std::ptrdiff_t>(chokeIndex) * 0x1C;
  return reinterpret_cast<const aas2ChokePoint_t*>(chokePoints + offset);
}

int idAAS2Local::GetNumHintNodes() const {
  return FileList<std::uint8_t>(*this, HINT_NODES_OFFSET).num;
}

const aas2HintNode_t* idAAS2Local::GetHintNode(int hintIndex) const {
  const std::uint8_t* hintNodes =
      FileList<std::uint8_t>(*this, HINT_NODES_OFFSET).list;
  const std::ptrdiff_t offset =
      static_cast<std::ptrdiff_t>(hintIndex) * 0x18;
  return reinterpret_cast<const aas2HintNode_t*>(hintNodes + offset);
}
