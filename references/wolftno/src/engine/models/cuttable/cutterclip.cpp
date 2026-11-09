#include "engine/models/cuttable/cutterclip.h"

#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace {

bool SamePoint(const idVec2i& a, const idVec2i& b) {
  return a.x == b.x && a.y == b.y;
}

std::int32_t WrapAdd(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) +
                                   static_cast<std::uint32_t>(right));
}

std::int32_t WrapSubtract(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) -
                                   static_cast<std::uint32_t>(right));
}

std::int32_t WrapMultiply(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) *
                                   static_cast<std::uint32_t>(right));
}

std::int32_t WrapAbs(std::int32_t value) {
  return value < 0
             ? static_cast<std::int32_t>(0u - static_cast<std::uint32_t>(value))
             : value;
}

constexpr memTag_t kGameTag = static_cast<memTag_t>(0x4D);

template <typename T, int Count>
T* AllocatePoolBlock(idList<void*>& allocations, const char* location) {
  T* block = static_cast<T*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(T) * Count, kGameTag, false, ALIGN_16,
      HEAP_DEFAULTHEAP));
  allocations.Append(block);
  return block;
}

} // namespace

template <typename T>
class idSort {
public:
  virtual ~idSort() = default;
  virtual void Sort(T* base, unsigned int num) const = 0;
};

template <typename T, typename Derived>
class idSort_Quick : public idSort<T> {
public:
  void Sort(T* base, unsigned int num) const override {
    if (num <= 1)
      return;
    constexpr std::int64_t maxLevels = 128;
    std::int64_t lo[maxLevels];
    std::int64_t hi[maxLevels];
    std::int64_t low = 0;
    std::int64_t high = num - 1;
    std::int64_t level = 0;
    for (;;) {
      std::int64_t i = low;
      std::int64_t j = high;
      if (j - i >= 4 && level < maxLevels - 1) {
        const std::int64_t pivotIndex = (i + j) / 2;
        std::swap(base[j], base[pivotIndex]);
        T& pivot = base[j--];
        do {
          while (static_cast<const Derived*>(this)->Compare(base[i], pivot) < 0) {
            if (++i >= j)
              break;
          }
          while (static_cast<const Derived*>(this)->Compare(base[j], pivot) > 0) {
            if (--j <= i)
              break;
          }
          if (i >= j)
            break;
          std::swap(base[i], base[j]);
        } while (++i < --j);
        while (static_cast<const Derived*>(this)->Compare(base[i], pivot) <= 0 &&
               i < high) {
          ++i;
        }
        while (static_cast<const Derived*>(this)->Compare(base[j], pivot) >= 0 &&
               low < j) {
          --j;
        }
        std::swap(pivot, base[i]);
        if (high - i <= j - low) {
          lo[level] = low;
          hi[level++] = j;
          low = i;
        } else {
          lo[level] = i;
          hi[level++] = high;
          high = j;
        }
      } else {
        for (; i < j; --j) {
          std::int64_t maximum = i;
          for (std::int64_t index = i + 1; index <= j; ++index) {
            if (static_cast<const Derived*>(this)->Compare(base[index],
                                                           base[maximum]) > 0) {
              maximum = index;
            }
          }
          std::swap(base[maximum], base[j]);
        }
        if (--level < 0)
          return;
        low = lo[level];
        high = hi[level];
      }
    }
  }
};

class idCutterClip::idSort_Polygons
    : public idSort_Quick<Polygon_t*, idSort_Polygons> {
public:
  ~idSort_Polygons() override;

  int Compare(Polygon_t* const& left, Polygon_t* const& right) const {
    if (left == right)
      return 0;
    if (left->points != nullptr && right->points != nullptr) {
      const Polygon_t* leftOuter = left->hole ? left->firstLeft : left;
      const Polygon_t* rightOuter = right->hole ? right->firstLeft : right;
      const int difference = WrapSubtract(leftOuter->index, rightOuter->index);
      if (difference != 0)
        return difference;
      if (left->hole == right->hole)
        return 0;
      return left->hole ? 1 : -1;
    }
    if ((left->points == nullptr) == (right->points == nullptr))
      return 0;
    return left->points == nullptr ? 1 : -1;
  }
};

idCutterClip::idSort_Polygons::~idSort_Polygons() = default;

int idCutterClip::precisionModifier = 10000;

idCutterClip::idCutterClip()
    : clipMode(CLIPMODE_INTERSECT), running(false) {}

idCutterClip::~idCutterClip() {
  Prepare();
  for (int index = 0; index < poolAllocations.Num(); ++index) {
    idMem::Singleton()->Free(poolAllocations[index], ALIGN_16);
    poolAllocations[index] = nullptr;
  }
  poolAllocations.ClearFree();
}

void idCutterClip::Prepare() {
  ClearNestedPolygons();
  ClearPolygons();
  ClearPolygonPoints();
  ClearHorizontalJoinRecords();
  ClearJoinRecords();
  for (int index = 0; index < edges.Num(); ++index) {
    edges[index]->next = edgesPool;
    --numEdges;
    edgesPool = edges[index];
  }
  edges.ClearFree();
  while (localMinimaList != nullptr) {
    LocalMinima_t* next = localMinimaList->next;
    localMinimaList->next = localMinimasPool;
    --numLocalMinima;
    localMinimasPool = localMinimaList;
    localMinimaList = next;
  }
  while (intersectNodes != nullptr) {
    IntersectNode_t* next = intersectNodes->next;
    intersectNodes->next = intersectNodesPool;
    --numIntersectNode;
    intersectNodesPool = intersectNodes;
    intersectNodes = next;
  }
  while (scanbeams != nullptr) {
    Scanbeam_t* next = scanbeams->next;
    scanbeams->next = scanbeamsPool;
    --numScanbeams;
    scanbeamsPool = scanbeams;
    scanbeams = next;
  }
  activeEdges = nullptr;
  sortedEdges = nullptr;
  currentLocalMinima = nullptr;
}

void idCutterClip::PreAllocate() {
  Edge_t* edge = AllocEdge();
  edge->next = edgesPool;
  --numEdges;
  edgesPool = edge;
  LocalMinima_t* minima = AllocLocalMinima();
  minima->next = localMinimasPool;
  --numLocalMinima;
  localMinimasPool = minima;
  Scanbeam_t* scanbeam = AllocScanbeam();
  scanbeam->next = scanbeamsPool;
  --numScanbeams;
  scanbeamsPool = scanbeam;
  NestedPolygon_t* nested = AllocNestedPolygon();
  nested->next = nestedPolygonsPool;
  --numNestedPolygons;
  nestedPolygonsPool = nested;
  Polygon_t* polygon = AllocPolygon();
  polygon->appendLink = polygonsPool;
  --numPolygons;
  polygonsPool = polygon;
  PolygonPoint_t* point = AllocPolygonPoint();
  point->prev = nullptr;
  point->next = polygonPointsPool;
  --numPolygonPoints;
  polygonPointsPool = point;
  JoinRecord_t* join = AllocJoinRecord();
  join->next = joinRecordsPool;
  --numJoinRecords;
  joinRecordsPool = join;
  HorizontalJoinRecord_t* horizontal = AllocHorizontalJoinRecord();
  horizontal->next = horizontalJoinRecordsPool;
  --numHorizontalJoinRecords;
  horizontalJoinRecordsPool = horizontal;
  IntersectNode_t* intersect = AllocIntersectNode();
  intersect->next = intersectNodesPool;
  --numIntersectNode;
  intersectNodesPool = intersect;
}

void idCutterClip::Reset() {
  ClearJoinRecords();
  ClearHorizontalJoinRecords();
  ClearNestedPolygons();
  ClearPolygons();
  ClearPolygonPoints();
  for (LocalMinima_t* minima = localMinimaList; minima != nullptr;
       minima = minima->next) {
    for (Edge_t* edge = minima->leftBound; edge != nullptr;
         edge = edge->nextLML) {
      edge->side = EDGESIDE_LEFT;
      edge->outIndex = -1;
      edge->current = edge->bottom;
    }
    for (Edge_t* edge = minima->rightBound; edge != nullptr;
         edge = edge->nextLML) {
      edge->side = EDGESIDE_RIGHT;
      edge->outIndex = -1;
      edge->current = edge->bottom;
    }
    AddScanbeam(minima->y);
    AddScanbeam(minima->leftBound->top.y);
  }
  if (localMinimaList != nullptr) {
    currentLocalMinima = localMinimaList;
    currentNestedPolygon = nullptr;
    currentInnerPolygon = nullptr;
  }
}

void idCutterClip::AddContour(const Contour* contours, polygonType_t polyType) {
  for (const Contour* contour = contours; contour != nullptr; contour = contour->next) {
    if (contour->edges == nullptr)
      break;

    const int firstIndex = edges.Num();
    const int edgeCount = contour->numEdges;
    edges.SetNum(firstIndex + edgeCount);
    for (int index = 0; index < edgeCount; ++index)
      edges[firstIndex + index] = AllocEdge();

    ContourEdge_t* contourEdge = contour->edges;
    ContourEdge_t* nextContourEdge = contour->edges->next;
    Edge_t* lowest = nullptr;
    int previousIndex = edgeCount - 1;
    for (int index = 0; index < edgeCount; ++index) {
      ContourEdge_t* previousContourEdge = contourEdge;
      contourEdge = nextContourEdge;
      ContourNode_t* nextNode = nextContourEdge->node;
      nextContourEdge = nextContourEdge->next;
      if (nextContourEdge == nullptr)
        nextContourEdge = contour->edges;

      Edge_t* edge = edges[firstIndex + index];
      edge->next = edges[firstIndex + ((index + 1) % edgeCount)];
      edge->prev = edges[firstIndex + previousIndex];
      edge->prevLML = nullptr;
      edge->nextLML = nullptr;
      edge->prevAEL = nullptr;
      edge->nextAEL = nullptr;
      edge->prevSEL = nullptr;
      edge->nextSEL = nullptr;
      edge->ID = firstIndex + previousIndex;
      edge->polyType = static_cast<std::uint16_t>(polyType);
      edge->outIndex = -1;
      edge->windingDelta = -1;
      edge->tmpX = 0;
      edge->top = previousContourEdge->node->pos;
      edge->bottom = nextNode->pos;
      if (edge->top.y >= edge->bottom.y) {
        edge->bottom = previousContourEdge->node->pos;
        edge->top = nextNode->pos;
        edge->windingDelta = 1;
      }
      edge->current = edge->bottom;
      edge->horizontal = edge->bottom.y == edge->top.y;
      edge->deltaX = edge->horizontal
                         ? -1.0e30f
                         : static_cast<float>(WrapSubtract(edge->top.x,
                                                           edge->bottom.x)) /
                               static_cast<float>(WrapSubtract(edge->top.y,
                                                               edge->bottom.y));
      if (lowest == nullptr || edge->bottom.y < lowest->bottom.y)
        lowest = edge;
      previousIndex = index;
    }

    if (lowest->windingDelta > 0)
      lowest = lowest->next;
    if (lowest->horizontal)
      lowest = lowest->next;
    Edge_t* edge = lowest;
    do {
      edge = AddBoundToLML(edge);
    } while (edge != lowest);
  }
}

bool idCutterClip::Clip(clipMode_t clipMode_, fillType_t fillType) {
  clipMode = clipMode_;
  fillTypeClip = fillType;
  fillTypeSubject = fillType;
  Reset();
  if (currentLocalMinima == nullptr)
    return true;

  Scanbeam_t* scanbeam = scanbeams;
  scanbeams = scanbeam->next;
  int yBottom = scanbeam->y;
  for (;;) {
    AddLocalMinimaToAEL(yBottom);
    ClearHorizontalJoinRecords();
    while (sortedEdges != nullptr) {
      Edge_t* horizontal = sortedEdges;
      Edge_t* previous = horizontal->prevSEL;
      Edge_t* next = horizontal->nextSEL;
      horizontal->prevSEL = nullptr;
      horizontal->nextSEL = nullptr;
      if (previous != nullptr)
        previous->nextSEL = next;
      if (next != nullptr)
        next->prevSEL = previous;
      if (previous == nullptr)
        sortedEdges = next;
      if (!ProcessHorizontal(horizontal))
        return false;
    }

    scanbeam = scanbeams;
    scanbeams = scanbeam->next;
    const int yTop = scanbeam->y;
    if (activeEdges != nullptr) {
      BuildIntersectList(yBottom, yTop);
      if (intersectNodes != nullptr) {
        if (!FixupIntersections())
          return true;
        ProcessIntersectList();
      }
    }
    if (!ProcessEdgesAtTopOfScanbeam(yTop))
      return false;
    yBottom = yTop;
    if (scanbeams != nullptr)
      continue;

    for (int index = 0; index < polygons.Num(); ++index) {
      Polygon_t* polygon = polygons[index];
      if (polygon->points == nullptr)
        continue;
      FixupPolygon(polygon);
      if (polygon->points == nullptr)
        continue;
      if (polygon->hole && !FixHoleLinkage_r(polygon))
        return false;
      PolygonPoint_t* points = polygon->points;
      if (polygon->hole == IsClockwise(points))
        ReverseLinks(points);
    }

    JoinEdges();
    idSort_Polygons sorter;
    if (polygons.list != nullptr)
      sorter.Sort(polygons.list, polygons.num);
    BuildResult();
    return true;
  }
}

int idCutterClip::GetNumPolygons() const {
  return nestedPolygons.num;
}

void idCutterClip::GetPolygon(int index, idList<idVec2i>& list) {
  list.Clear();
  currentNestedPolygon = nestedPolygons[index];
  if (currentNestedPolygon != nullptr) {
    GetPolygon(currentNestedPolygon->outer, list);
    currentInnerPolygon = currentNestedPolygon->inner;
  } else {
    currentInnerPolygon = nullptr;
  }
}

void idCutterClip::GetInnerPolygon(idList<idVec2i>& list) {
  list.Clear();
  if (currentInnerPolygon == nullptr)
    return;
  GetPolygon(currentInnerPolygon, list);
  currentInnerPolygon = currentInnerPolygon->next;
}

void idCutterClip::GetConnectedPolygon(int index, idList<idVec2i>& list) {
  list.Clear();
  Polygon_t* polygon = polygons[index];
  if (polygon->connected)
    GetPolygon(polygon, list);
}

bool idCutterClip::HasInnerPolygon() const {
  return currentInnerPolygon != nullptr;
}

void idCutterClip::GetPolygon(const Polygon_t* polygon, idList<idVec2i>& list) {
  if (polygon != nullptr) {
    const PolygonPoint_t* points = polygon->points;
    if (points != nullptr) {
      const PolygonPoint_t* point = points;
      do {
        list.Append(point->pos);
        point = point->next;
      } while (point != points);
    }
  }
}

bool idCutterClip::IsClockwise(const PolygonPoint_t* points) {
  PolygonPoint_t* next = points->next;
  if (points == next || next->next == points)
    return false;
  std::int32_t area = 0;
  const PolygonPoint_t* current = points;
  do {
    area = WrapAdd(area,
                   WrapSubtract(WrapMultiply(current->pos.x, next->pos.y),
                                WrapMultiply(current->pos.y, next->pos.x)));
    current = next;
    next = next->next;
  } while (current != points);
  return area > 0;
}

bool idCutterClip::PointInPolygon(const idVec2i& pos,
                                  const PolygonPoint_t* point) {
  bool inside = false;
  const PolygonPoint_t* current = point;
  do {
    const idVec2i currentPos = current->pos;
    const idVec2i previousPos = current->prev->pos;
    if ((currentPos.y <= pos.y && pos.y < previousPos.y) ||
        (previousPos.y <= pos.y && pos.y < currentPos.y)) {
      const int left = WrapSubtract(pos.x, currentPos.x);
      const int right = WrapMultiply(WrapSubtract(pos.y, currentPos.y),
                                     WrapSubtract(previousPos.x,
                                                  currentPos.x)) /
                        WrapSubtract(previousPos.y, currentPos.y);
      if (left < right)
        inside = !inside;
    }
    current = current->next;
  } while (current != point);
  return inside;
}

bool idCutterClip::LineLineIntersection(const idVec2i& p1a,
                                        const idVec2i& p1b,
                                        const idVec2i& p2a,
                                        const idVec2i& p2b,
                                        idVec2i* pos) {
  const int delta1X = WrapSubtract(p1b.x, p1a.x);
  const int delta1Y = WrapSubtract(p1b.y, p1a.y);
  const int delta2X = WrapSubtract(p2b.x, p2a.x);
  const int delta2Y = WrapSubtract(p2b.y, p2a.y);
  const float denominator = static_cast<float>(WrapSubtract(
      WrapMultiply(delta2Y, delta1X), WrapMultiply(delta2X, delta1Y)));
  if (std::fabs(denominator) < 1.192092896e-7f)
    return false;
  const int originX = WrapSubtract(p1a.x, p2a.x);
  const int originY = WrapSubtract(p1a.y, p2a.y);
  const float inverse = 1.0f / denominator;
  const float along2 = static_cast<float>(WrapSubtract(
                           WrapMultiply(delta1X, originY),
                           WrapMultiply(delta1Y, originX))) *
                       inverse;
  const float along1 = static_cast<float>(WrapSubtract(
                           WrapMultiply(delta2X, originY),
                           WrapMultiply(delta2Y, originX))) *
                       inverse;
  if (along1 < 0.0f || along1 > 1.0f || along2 < 0.0f || along2 > 1.0f)
    return false;
  if (pos != nullptr) {
    pos->x = WrapAdd(p1a.x, static_cast<int>(static_cast<float>(delta1X) *
                                             along1));
    pos->y = WrapAdd(p1a.y, static_cast<int>(static_cast<float>(delta1Y) *
                                             along1));
  }
  return true;
}

bool idCutterClip::EdgeEdgeIntersection(Edge_t* edge1, Edge_t* edge2, idVec2i* pos) {
  if (SlopesEqual(edge1, edge2))
    return false;
  int x;
  int y = edge1->bottom.y;
  if (std::fabs(edge1->deltaX) < 1.192092896e-7f) {
    x = edge1->bottom.x;
    if (edge2->horizontal)
      y = edge2->bottom.y;
    else
      y = static_cast<int>(std::floor(
          static_cast<float>(edge2->bottom.y) -
              static_cast<float>(edge2->bottom.x) / edge2->deltaX +
              static_cast<float>(edge1->bottom.x) / edge2->deltaX +
              0.5f));
  } else if (std::fabs(edge2->deltaX) < 1.192092896e-7f) {
    x = edge2->bottom.x;
    if (!edge1->horizontal) {
      y = static_cast<int>(std::floor(
          static_cast<float>(edge1->bottom.y) -
              static_cast<float>(edge1->bottom.x) / edge1->deltaX +
              static_cast<float>(edge2->bottom.x) / edge1->deltaX +
              0.5f));
    }
  } else {
    const float intercept1 = static_cast<float>(edge1->bottom.x) -
                             static_cast<float>(edge1->bottom.y) *
                                 edge1->deltaX;
    const float intersectionY =
        (static_cast<float>(edge2->bottom.x) -
             static_cast<float>(edge2->bottom.y) * edge2->deltaX -
         intercept1) /
        (edge1->deltaX - edge2->deltaX);
    x = static_cast<int>(
        std::floor(edge1->deltaX * intersectionY + intercept1 + 0.5f));
    y = static_cast<int>(std::floor(intersectionY + 0.5f));
  }
  if (pos != nullptr)
    *pos = {x, y};
  return (y == edge1->top.y && y >= edge2->top.y &&
          edge1->tmpX > edge2->tmpX) ||
         (y == edge2->top.y && y >= edge1->top.y &&
          edge1->tmpX > edge2->tmpX) ||
         (y > edge1->top.y && y > edge2->top.y);
}

bool idCutterClip::IsSegmentOverlapping(const idVec2i& p1a,
                                        const idVec2i& p1b,
                                        const idVec2i& p2a,
                                        const idVec2i& p2b,
                                        idVec2i* overlap1, idVec2i* overlap2) {
  const int deltaY = WrapSubtract(p1a.y, p1b.y);
  const int slope = deltaY == 0
                        ? 2
                        : std::abs(WrapSubtract(p1a.x, p1b.x) / deltaY);
  const idVec2i* first;
  const idVec2i* second;
  if (deltaY == 0 || slope > 1) {
    const idVec2i* p1Minimum = p1a.x < p1b.x ? &p1a : &p1b;
    const idVec2i* p2Minimum = p2a.x < p2b.x ? &p2a : &p2b;
    first = p1Minimum->x <= p2Minimum->x ? p2Minimum : p1Minimum;
    const idVec2i* p1Maximum = p1a.x >= p1b.x ? &p1a : &p1b;
    const idVec2i* p2Maximum = p2a.x >= p2b.x ? &p2a : &p2b;
    second = p1Maximum->x >= p2Maximum->x ? p2Maximum : p1Maximum;
    if (overlap1 != nullptr)
      *overlap1 = *first;
    if (overlap2 != nullptr)
      *overlap2 = *second;
    return first->x < second->x;
  }
  const idVec2i* p1Maximum = p1a.y >= p1b.y ? &p1a : &p1b;
  const idVec2i* p2Maximum = p2a.y >= p2b.y ? &p2a : &p2b;
  first = p1Maximum->y >= p2Maximum->y ? p2Maximum : p1Maximum;
  const idVec2i* p1Minimum = p1a.y < p1b.y ? &p1a : &p1b;
  const idVec2i* p2Minimum = p2a.y < p2b.y ? &p2a : &p2b;
  second = p1Minimum->y <= p2Minimum->y ? p2Minimum : p1Minimum;
  if (overlap1 != nullptr)
    *overlap1 = *first;
  if (overlap2 != nullptr)
    *overlap2 = *second;
  return first->y > second->y;
}

bool idCutterClip::SlopesEqual(Edge_t* edge1, Edge_t* edge2) {
  if (edge1->bottom.y == edge1->top.y)
    return edge2->bottom.y == edge2->top.y;
  if (edge1->bottom.x == edge1->top.x)
    return edge2->bottom.x == edge2->top.x;
  return WrapMultiply(WrapSubtract(edge1->top.y, edge1->bottom.y),
                      WrapSubtract(edge2->top.x, edge2->bottom.x)) ==
         WrapMultiply(WrapSubtract(edge1->top.x, edge1->bottom.x),
                      WrapSubtract(edge2->top.y, edge2->bottom.y));
}

bool idCutterClip::IsContributing(clipMode_t mode, Edge_t* edge) {
  const int winding = std::abs(static_cast<int>(edge->windingCount));
  const int winding2 = std::abs(static_cast<int>(edge->windingCount2));
  switch (mode) {
  case CLIPMODE_INTERSECT:
    return winding == 1 && winding2 != 0;
  case CLIPMODE_UNION:
    return winding == 1 && winding2 == 0;
  case CLIPMODE_DIFFERENCE:
    return winding == 1 && (edge->polyType == POLYTYPE_SUBJECT ? winding2 == 0 : winding2 != 0);
  case CLIPMODE_XOR:
    return winding == 1;
  }
  return false;
}

const idCutterClip::Polygon_t* idCutterClip::FindBottomPolygon(const Polygon_t* polygon1,
                                                                const Polygon_t* polygon2) {
  const idVec2i first = polygon1->bottomPoint->pos;
  const idVec2i second = polygon2->bottomPoint->pos;
  if (first.y > second.y)
    return polygon1;
  if (first.y < second.y)
    return polygon2;
  if (first.x < second.x)
    return polygon1;
  if (first.x > second.x)
    return polygon2;
  if (polygon1->bottomEdge2 == nullptr)
    return polygon2;
  if (polygon2->bottomEdge2 == nullptr)
    return polygon1;
  const float firstSlope =
      std::fmax(polygon1->bottomEdge1->deltaX,
                polygon1->bottomEdge2->deltaX);
  const float secondSlope =
      std::fmax(polygon2->bottomEdge1->deltaX,
                polygon2->bottomEdge2->deltaX);
  return secondSlope <= firstSlope ? polygon1 : polygon2;
}

idCutterClip::PolygonPoint_t* idCutterClip::FindBottom(PolygonPoint_t* point) {
  PolygonPoint_t* result = point;
  for (PolygonPoint_t* current = point->next; current != point;
       current = current->next) {
    if (current->pos.y > result->pos.y ||
        (current->pos.y == result->pos.y && current->pos.x < result->pos.x))
      result = current;
  }
  return result;
}

void idCutterClip::ReverseLinks(PolygonPoint_t* point) {
  PolygonPoint_t* current = point;
  do {
    PolygonPoint_t* next = current->next;
    current->next = current->prev;
    current->prev = next;
    current = next;
  } while (current != point);
}

void idCutterClip::SwapEdgeInAEL(Edge_t* edge1, Edge_t* edge2) {
  Edge_t* edge1Next = edge1->nextAEL;
  if (edge1->prevAEL == nullptr && edge1Next == nullptr)
    return;
  Edge_t* edge2Next = edge2->nextAEL;
  if (edge2->prevAEL == nullptr && edge2Next == nullptr)
    return;
  if (edge1Next == edge2) {
    if (edge2Next != nullptr)
      edge2Next->prevAEL = edge1;
    Edge_t* edge1Previous = edge1->prevAEL;
    if (edge1Previous != nullptr)
      edge1Previous->nextAEL = edge2;
    edge2->prevAEL = edge1Previous;
    edge2->nextAEL = edge1;
    edge1->prevAEL = edge2;
    edge1->nextAEL = edge2Next;
  } else if (edge2Next == edge1) {
    if (edge1Next != nullptr)
      edge1Next->prevAEL = edge2;
    Edge_t* edge2Previous = edge2->prevAEL;
    if (edge2Previous != nullptr)
      edge2Previous->nextAEL = edge1;
    edge1->prevAEL = edge2Previous;
    edge1->nextAEL = edge2;
    edge2->prevAEL = edge1;
    edge2->nextAEL = edge1Next;
  } else {
    Edge_t* edge1Previous = edge1->prevAEL;
    edge1->nextAEL = edge2Next;
    if (edge2Next != nullptr)
      edge2Next->prevAEL = edge1;
    Edge_t* edge2Previous = edge2->prevAEL;
    edge1->prevAEL = edge2Previous;
    if (edge2Previous != nullptr)
      edge2Previous->nextAEL = edge1;
    edge2->nextAEL = edge1Next;
    if (edge1Next != nullptr)
      edge1Next->prevAEL = edge2;
    edge2->prevAEL = edge1Previous;
    if (edge1Previous != nullptr)
      edge1Previous->nextAEL = edge2;
  }
  if (edge1->prevAEL != nullptr) {
    if (edge2->prevAEL == nullptr)
      activeEdges = edge2;
  } else {
    activeEdges = edge1;
  }
}

void idCutterClip::SwapEdgeInSEL(Edge_t* edge1, Edge_t* edge2) {
  Edge_t* edge1Next = edge1->nextSEL;
  if (edge1->prevSEL == nullptr && edge1Next == nullptr)
    return;
  Edge_t* edge2Next = edge2->nextSEL;
  if (edge2->prevSEL == nullptr && edge2Next == nullptr)
    return;
  if (edge1Next == edge2) {
    if (edge2Next != nullptr)
      edge2Next->prevSEL = edge1;
    Edge_t* edge1Previous = edge1->prevSEL;
    if (edge1Previous != nullptr)
      edge1Previous->nextSEL = edge2;
    edge2->prevSEL = edge1Previous;
    edge2->nextSEL = edge1;
    edge1->prevSEL = edge2;
    edge1->nextSEL = edge2Next;
  } else if (edge2Next == edge1) {
    if (edge1Next != nullptr)
      edge1Next->prevSEL = edge2;
    Edge_t* edge2Previous = edge2->prevSEL;
    if (edge2Previous != nullptr)
      edge2Previous->nextSEL = edge1;
    edge1->prevSEL = edge2Previous;
    edge1->nextSEL = edge2;
    edge2->prevSEL = edge1;
    edge2->nextSEL = edge1Next;
  } else {
    Edge_t* edge1Previous = edge1->prevSEL;
    edge1->nextSEL = edge2Next;
    if (edge2Next != nullptr)
      edge2Next->prevSEL = edge1;
    Edge_t* edge2Previous = edge2->prevSEL;
    edge1->prevSEL = edge2Previous;
    if (edge2Previous != nullptr)
      edge2Previous->nextSEL = edge1;
    edge2->nextSEL = edge1Next;
    if (edge1Next != nullptr)
      edge1Next->prevSEL = edge2;
    edge2->prevSEL = edge1Previous;
    if (edge1Previous != nullptr)
      edge1Previous->nextSEL = edge2;
  }
  if (edge1->prevSEL != nullptr) {
    if (edge2->prevSEL == nullptr)
      sortedEdges = edge2;
  } else {
    sortedEdges = edge1;
  }
}

void idCutterClip::RemoveEdgeFromAEL(Edge_t* edge) {
  Edge_t* previous = edge->prevAEL;
  Edge_t* next = edge->nextAEL;
  if (previous == nullptr && next == nullptr && edge != activeEdges)
    return;
  edge->nextAEL = nullptr;
  edge->prevAEL = nullptr;
  if (previous != nullptr)
    previous->nextAEL = next;
  if (next != nullptr)
    next->prevAEL = previous;
  if (previous == nullptr)
    activeEdges = next;
}

bool idCutterClip::FixupIntersections() {
  if (intersectNodes->next == nullptr)
    return true;
  sortedEdges = activeEdges;
  Edge_t* previous = activeEdges;
  if (previous != nullptr) {
    for (Edge_t* edge = previous->nextAEL; edge != nullptr;
         edge = edge->nextAEL) {
      edge->prevSEL = previous;
      previous->nextSEL = edge;
      previous = edge;
    }
    sortedEdges->prevSEL = nullptr;
    previous->nextSEL = nullptr;
  }
  IntersectNode_t* node = intersectNodes;
  while (node->next != nullptr) {
    Edge_t* edge1 = node->edge1;
    Edge_t* edge2 = node->edge2;
    if (edge1->prevSEL != edge2 && edge1->nextSEL != edge2) {
      IntersectNode_t* adjacent = node->next;
      while (adjacent != nullptr && adjacent->edge1->nextSEL != adjacent->edge2 &&
             adjacent->edge1->prevSEL != adjacent->edge2)
        adjacent = adjacent->next;
      if (adjacent == nullptr)
        return false;
      std::swap(node->edge1, adjacent->edge1);
      std::swap(node->edge2, adjacent->edge2);
      std::swap(node->pos, adjacent->pos);
      edge1 = node->edge1;
      edge2 = node->edge2;
    }
    SwapEdgeInSEL(edge1, edge2);
    node = node->next;
  }
  sortedEdges = nullptr;
  return node->edge1->prevSEL == node->edge2 ||
         node->edge1->nextSEL == node->edge2;
}

bool idCutterClip::FixHoleLinkage_r(Polygon_t* polygon) {
  if (polygon->bottomPoint == nullptr)
    return false;
  Polygon_t* firstLeft = polygons[polygon->bottomPoint->index]->firstLeft;
  if (polygon == firstLeft)
    return false;
  if (firstLeft != nullptr) {
    while (firstLeft->appendLink != nullptr)
      firstLeft = firstLeft->appendLink;
    if (firstLeft == polygon)
      firstLeft = nullptr;
    else if (firstLeft->hole) {
      if (!FixHoleLinkage_r(firstLeft))
        return false;
      firstLeft = firstLeft->firstLeft;
    }
  }
  polygon->firstLeft = firstLeft;
  if (firstLeft == nullptr)
    polygon->hole = false;
  polygon->appendLink = nullptr;
  return true;
}

void idCutterClip::AddEdgeToAEL(Edge_t* edge) {
  edge->prevAEL = nullptr;
  edge->nextAEL = nullptr;
  Edge_t* current = activeEdges;
  if (current == nullptr) {
    activeEdges = edge;
    return;
  }
  const int edgeX = edge->current.x;
  const auto sortsBefore = [edgeX, edge](const Edge_t* other) {
    return edgeX == other->current.x
               ? static_cast<int>(edge->deltaX) >
                     static_cast<int>(other->deltaX)
               : edgeX < other->current.x;
  };
  if (sortsBefore(current)) {
    edge->nextAEL = current;
    current->prevAEL = edge;
    activeEdges = edge;
    return;
  }
  while (current->nextAEL != nullptr && !sortsBefore(current->nextAEL))
    current = current->nextAEL;
  edge->nextAEL = current->nextAEL;
  if (current->nextAEL != nullptr)
    current->nextAEL->prevAEL = edge;
  edge->prevAEL = current;
  current->nextAEL = edge;
}

void idCutterClip::AppendPolygon(Edge_t* edge1, Edge_t* edge2) {
  Polygon_t* first = polygons[edge1->outIndex];
  Polygon_t* second = polygons[edge2->outIndex];
  const Polygon_t* bottom = FindBottomPolygon(first, second);
  if (first->hole != second->hole) {
    if (bottom == second)
      first->hole = bottom->hole;
    else
      second->hole = bottom->hole;
  }
  PolygonPoint_t* firstPoints = first->points;
  PolygonPoint_t* secondPoints = second->points;
  PolygonPoint_t* firstPrevious = firstPoints->prev;
  PolygonPoint_t* secondPrevious = secondPoints->prev;
  const std::uint16_t side = edge1->side;
  if (side == edge2->side) {
    ReverseLinks(secondPoints);
    secondPoints->next = firstPoints;
    firstPoints->prev = secondPoints;
    firstPrevious->next = secondPrevious;
    secondPrevious->prev = firstPrevious;
    if (side == EDGESIDE_LEFT)
      first->points = secondPrevious;
  } else {
    secondPrevious->next = firstPoints;
    firstPoints->prev = secondPrevious;
    secondPoints->prev = firstPrevious;
    firstPrevious->next = secondPoints;
    if (side == EDGESIDE_LEFT)
      first->points = secondPoints;
  }
  if (bottom == second) {
    first->bottomPoint = second->bottomPoint;
    first->bottomPoint->index = first->index;
    first->bottomEdge1 = second->bottomEdge1;
    first->bottomEdge2 = second->bottomEdge2;
    if (second->firstLeft != first)
      first->firstLeft = second->firstLeft;
  }
  second->points = nullptr;
  second->bottomPoint = nullptr;
  second->appendLink = nullptr;
  second->hole = false;
  const std::int16_t firstIndex = edge1->outIndex;
  const std::int16_t secondIndex = edge2->outIndex;
  edge1->outIndex = -1;
  edge2->outIndex = -1;
  for (Edge_t* edge = activeEdges; edge != nullptr; edge = edge->nextAEL) {
    if (edge->outIndex == secondIndex) {
      edge->outIndex = firstIndex;
      edge->side = side;
      break;
    }
  }
  for (int index = 0; index < joins.Num(); ++index) {
    if (joins[index]->polyIndex1 == secondIndex)
      joins[index]->polyIndex1 = firstIndex;
    if (joins[index]->polyIndex2 == secondIndex)
      joins[index]->polyIndex2 = firstIndex;
  }
  for (int index = 0; index < horizontalJoins.Num(); ++index) {
    if (horizontalJoins[index]->savedIndex == secondIndex)
      horizontalJoins[index]->savedIndex = firstIndex;
  }
}

void idCutterClip::SetWindingCount(Edge_t* edge) {
  Edge_t* previous = edge->prevAEL;
  while (previous != nullptr && previous->polyType != edge->polyType)
    previous = previous->prevAEL;
  Edge_t* scan;
  if (previous != nullptr) {
    const fillType_t ownFill = edge->polyType == POLYTYPE_SUBJECT
                                   ? fillTypeSubject
                                   : fillTypeClip;
    if (ownFill == FILLTYPE_EVEN_ODD) {
      edge->windingCount = 1;
    } else if (ownFill == FILLTYPE_NON_ZERO) {
      std::int16_t winding = previous->windingCount;
      const int previousDelta = previous->windingDelta;
      const int magnitude = std::abs(static_cast<int>(winding));
      if (previousDelta * winding >= 0) {
        if (magnitude <= 1 || previousDelta * edge->windingDelta >= 0) {
          if (previous->windingCount + edge->windingDelta != 0)
            winding = static_cast<std::int16_t>(winding + edge->windingDelta);
          edge->windingCount = winding;
        } else {
          edge->windingCount = winding;
        }
      } else if (magnitude <= 1) {
        edge->windingCount = static_cast<std::int16_t>(
            previousDelta + winding + edge->windingDelta);
      } else {
        if (previousDelta * edge->windingDelta >= 0)
          winding = static_cast<std::int16_t>(winding * edge->windingDelta);
        edge->windingCount = winding;
      }
    }
    edge->windingCount2 = previous->windingCount2;
    scan = previous->nextAEL;
  } else {
    edge->windingCount = edge->windingDelta;
    edge->windingCount2 = 0;
    scan = activeEdges;
  }
  const fillType_t otherFill = edge->polyType == POLYTYPE_SUBJECT
                                   ? fillTypeClip
                                   : fillTypeSubject;
  if (otherFill == FILLTYPE_NON_ZERO) {
    while (scan != edge) {
      edge->windingCount2 = static_cast<std::int16_t>(
          edge->windingCount2 + scan->windingDelta);
      scan = scan->nextAEL;
    }
  } else if (otherFill == FILLTYPE_EVEN_ODD) {
    while (scan != edge) {
      edge->windingCount2 = edge->windingCount2 == 0;
      scan = scan->nextAEL;
    }
    if (edge->windingCount2 != 0)
      edge->windingCount2 = 1;
  }
}

void idCutterClip::ClearHorizontalJoinRecords() {
  for (int index = 0; index < horizontalJoins.Num(); ++index) {
    HorizontalJoinRecord_t* record = horizontalJoins[index];
    record->next = horizontalJoinRecordsPool;
    --numHorizontalJoinRecords;
    horizontalJoinRecordsPool = record;
    horizontalJoins[index] = nullptr;
  }
  horizontalJoins.ClearFree();
}

void idCutterClip::ClearJoinRecords() {
  for (int index = 0; index < joins.Num(); ++index) {
    JoinRecord_t* record = joins[index];
    record->next = joinRecordsPool;
    --numJoinRecords;
    joinRecordsPool = record;
    joins[index] = nullptr;
  }
  joins.ClearFree();
}

void idCutterClip::ClearNestedPolygons() {
  for (int index = 0; index < nestedPolygons.Num(); ++index) {
    NestedPolygon_t* polygon = nestedPolygons[index];
    if (polygon != nullptr) {
      polygon->next = nestedPolygonsPool;
      --numNestedPolygons;
      nestedPolygonsPool = polygon;
      nestedPolygons[index] = nullptr;
    }
  }
  nestedPolygons.ClearFree();
}

void idCutterClip::ClearPolygons() {
  for (int index = 0; index < polygons.Num(); ++index) {
    Polygon_t* polygon = polygons[index];
    if (polygon == nullptr)
      continue;
    PolygonPoint_t* point = polygon->points;
    if (point != nullptr) {
      point->prev->next = nullptr;
      while (point != nullptr) {
        PolygonPoint_t* released = point;
        point = point->next;
        released->next = nullptr;
        released->prev = nullptr;
        released->next = polygonPointsPool;
        --numPolygonPoints;
        polygonPointsPool = released;
      }
    }
    polygon->points = nullptr;
    polygon->bottomPoint = nullptr;
    polygon->appendLink = polygonsPool;
    --numPolygons;
    polygonsPool = polygon;
    polygons[index] = nullptr;
  }
  polygons.ClearFree();
}

void idCutterClip::ClearPolygonPoints() {
  for (int index = 0; index < polygonPoints.Num(); ++index) {
    PolygonPoint_t* point = polygonPoints[index];
    if (point != nullptr) {
      point->prev->next = nullptr;
      while (point != nullptr) {
        PolygonPoint_t* released = point;
        point = point->next;
        released->next = nullptr;
        released->prev = nullptr;
        released->next = polygonPointsPool;
        --numPolygonPoints;
        polygonPointsPool = released;
      }
      polygonPoints[index] = nullptr;
    }
  }
  polygonPoints.ClearFree();
}

void idCutterClip::FixupPolygon(Polygon_t* polygon) {
  PolygonPoint_t* point = polygon->bottomPoint;
  PolygonPoint_t* firstValid = nullptr;
  if (point != point->prev) {
    while (point->prev != point->next) {
      PolygonPoint_t* previous = point->prev;
      PolygonPoint_t* next = point->next;
      if (SamePoint(point->pos, next->pos) ||
          Contour::SlopesEqual(previous->pos, point->pos, next->pos)) {
        firstValid = nullptr;
        if (point == polygon->bottomPoint) {
          PolygonPoint_t* bottom = next;
          if (previous->pos.y > next->pos.y)
            bottom = previous;
          polygon->bottomPoint = bottom;
          bottom->index = polygon->index;
        }
        previous->next = next;
        next->prev = previous;
        point = previous;
      } else {
        if (point == firstValid) {
          polygon->points = polygon->bottomPoint;
          return;
        }
        if (firstValid == nullptr)
          firstValid = point;
        point = next;
      }
      if (point == point->prev)
        break;
    }
  }
  if (point != nullptr) {
    point->prev->next = nullptr;
    while (point != nullptr) {
      PolygonPoint_t* released = point;
      point = point->next;
      released->next = nullptr;
      released->prev = nullptr;
      released->next = polygonPointsPool;
      --numPolygonPoints;
      polygonPointsPool = released;
    }
  }
  polygon->bottomPoint = nullptr;
  polygon->points = nullptr;
}

idCutterClip::Edge_t* idCutterClip::AllocEdge() {
  if (edgesPool == nullptr) {
    Edge_t* block = AllocatePoolBlock<Edge_t, 1024>(
        poolAllocations,
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterClip.cpp(193) : TAG_GAME");
    edgesPool = block;
    for (int index = 0; index < 1023; ++index)
      block[index].next = &block[index + 1];
    block[1023].next = nullptr;
  }
  ++numEdges;
  maxEdges = std::max(maxEdges, numEdges);
  Edge_t* result = edgesPool;
  edgesPool = result->next;
  return result;
}

idCutterClip::LocalMinima_t* idCutterClip::AllocLocalMinima() {
  if (localMinimasPool == nullptr) {
    LocalMinima_t* block = AllocatePoolBlock<LocalMinima_t, 256>(
        poolAllocations,
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterClip.cpp(226) : TAG_GAME");
    localMinimasPool = block;
    for (int index = 0; index < 255; ++index)
      block[index].next = &block[index + 1];
    block[255].next = nullptr;
  }
  ++numLocalMinima;
  maxLocalMinima = std::max(maxLocalMinima, numLocalMinima);
  LocalMinima_t* result = localMinimasPool;
  localMinimasPool = result->next;
  return result;
}

idCutterClip::Scanbeam_t* idCutterClip::AllocScanbeam() {
  if (scanbeamsPool == nullptr) {
    Scanbeam_t* block = AllocatePoolBlock<Scanbeam_t, 256>(
        poolAllocations,
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterClip.cpp(259) : TAG_GAME");
    scanbeamsPool = block;
    for (int index = 0; index < 256; ++index) {
      block[index].next = index < 255 ? &block[index + 1] : nullptr;
      block[index].y = 0;
    }
  }
  ++numScanbeams;
  maxScanbeams = std::max(maxScanbeams, numScanbeams);
  Scanbeam_t* result = scanbeamsPool;
  scanbeamsPool = result->next;
  return result;
}

idCutterClip::Polygon_t* idCutterClip::AllocPolygon() {
  if (polygonsPool == nullptr) {
    Polygon_t* block = AllocatePoolBlock<Polygon_t, 32>(
        poolAllocations,
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterClip.cpp(291) : TAG_GAME");
    polygonsPool = block;
    for (int index = 0; index < 32; ++index) {
      block[index].firstLeft = nullptr;
      block[index].appendLink = index < 31 ? &block[index + 1] : nullptr;
    }
  }
  ++numPolygons;
  maxPolygons = std::max(maxPolygons, numPolygons);
  Polygon_t* result = polygonsPool;
  polygonsPool = result->appendLink;
  result->firstLeft = nullptr;
  result->appendLink = nullptr;
  result->bottomPoint = nullptr;
  result->points = nullptr;
  result->connected = false;
  result->hole = false;
  result->index = -1;
  return result;
}

idCutterClip::PolygonPoint_t* idCutterClip::AllocPolygonPoint() {
  if (polygonPointsPool == nullptr) {
    PolygonPoint_t* block = AllocatePoolBlock<PolygonPoint_t, 1024>(
        poolAllocations,
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterClip.cpp(336) : TAG_GAME");
    polygonPointsPool = block;
    for (int index = 0; index < 1024; ++index) {
      block[index].next = index < 1023 ? &block[index + 1] : nullptr;
      block[index].prev = nullptr;
    }
  }
  ++numPolygonPoints;
  maxPolygonPoints = std::max(maxPolygonPoints, numPolygonPoints);
  PolygonPoint_t* result = polygonPointsPool;
  polygonPointsPool = result->next;
  result->next = nullptr;
  result->prev = nullptr;
  return result;
}

idCutterClip::NestedPolygon_t* idCutterClip::AllocNestedPolygon() {
  if (nestedPolygonsPool == nullptr) {
    NestedPolygon_t* block = AllocatePoolBlock<NestedPolygon_t, 32>(
        poolAllocations,
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterClip.cpp(375) : TAG_GAME");
    nestedPolygonsPool = block;
    for (int index = 0; index < 32; ++index)
      block[index].next = index < 31 ? &block[index + 1] : nullptr;
  }
  ++numNestedPolygons;
  maxNestedPolygons = std::max(maxNestedPolygons, numNestedPolygons);
  NestedPolygon_t* result = nestedPolygonsPool;
  nestedPolygonsPool = result->next;
  result->next = nullptr;
  result->outer = nullptr;
  result->inner = nullptr;
  return result;
}

idCutterClip::JoinRecord_t* idCutterClip::AllocJoinRecord() {
  if (joinRecordsPool == nullptr) {
    JoinRecord_t* block = AllocatePoolBlock<JoinRecord_t, 128>(
        poolAllocations,
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterClip.cpp(412) : TAG_GAME");
    joinRecordsPool = block;
    for (int index = 0; index < 128; ++index)
      block[index].next = index < 127 ? &block[index + 1] : nullptr;
  }
  ++numJoinRecords;
  maxJoinRecords = std::max(maxJoinRecords, numJoinRecords);
  JoinRecord_t* result = joinRecordsPool;
  joinRecordsPool = result->next;
  return result;
}

idCutterClip::HorizontalJoinRecord_t* idCutterClip::AllocHorizontalJoinRecord() {
  if (horizontalJoinRecordsPool == nullptr) {
    HorizontalJoinRecord_t* block =
        AllocatePoolBlock<HorizontalJoinRecord_t, 128>(
            poolAllocations,
            "w:\\build\\tech5\\engine\\models\\cuttable\\CutterClip.cpp(444) : TAG_GAME");
    horizontalJoinRecordsPool = block;
    for (int index = 0; index < 128; ++index)
      block[index].next = index < 127 ? &block[index + 1] : nullptr;
  }
  ++numHorizontalJoinRecords;
  maxHorizontalJoinRecords = std::max(maxHorizontalJoinRecords, numHorizontalJoinRecords);
  HorizontalJoinRecord_t* result = horizontalJoinRecordsPool;
  horizontalJoinRecordsPool = result->next;
  return result;
}

idCutterClip::IntersectNode_t* idCutterClip::AllocIntersectNode() {
  if (intersectNodesPool == nullptr) {
    IntersectNode_t* block = AllocatePoolBlock<IntersectNode_t, 128>(
        poolAllocations,
        "w:\\build\\tech5\\engine\\models\\cuttable\\CutterClip.cpp(477) : TAG_GAME");
    intersectNodesPool = block;
    for (int index = 0; index < 128; ++index)
      block[index].next = index < 127 ? &block[index + 1] : nullptr;
  }
  ++numIntersectNode;
  maxIntersectNode = std::max(maxIntersectNode, numIntersectNode);
  IntersectNode_t* result = intersectNodesPool;
  intersectNodesPool = result->next;
  return result;
}

void idCutterClip::BuildResult() {
  const int count = polygons.Num();
  int index = 0;
  while (index < count) {
    Polygon_t* outer = polygons[index++];
    if (outer->points == nullptr)
      break;
    int pointCount = 0;
    PolygonPoint_t* point = outer->points;
    do {
      point = point->next;
      ++pointCount;
    } while (point != outer->points);
    if (pointCount < 3)
      continue;
    NestedPolygon_t* nested = AllocNestedPolygon();
    nested->outer = outer;
    nested->inner = nullptr;
    Polygon_t* previousInner = nullptr;
    while (index < count) {
      Polygon_t* inner = polygons[index++];
      if (inner->points == nullptr)
        break;
      if (!inner->hole && nested->outer != inner) {
        inner->connected = true;
        break;
      }
      if (previousInner == nullptr)
        nested->inner = inner;
      else
        previousInner->next = inner;
      inner->next = nullptr;
      previousInner = inner;
    }
    nestedPolygons.Append(nested);
  }
}

idCutterClip::Edge_t* idCutterClip::AddBoundToLML(Edge_t* edge) {
  Edge_t* next = edge->next;
  edge->nextLML = nullptr;

  for (;;) {
    while (!next->horizontal) {
      if (next->current.y == next->prev->current.y)
        goto localMinimum;
      next->nextLML = next->prev;
      next = next->next;
    }

    if (next->next->top.y < next->top.y &&
        next->next->bottom.x > next->prev->bottom.x) {
      break;
    }
    if (next->top.x == next->prev->bottom.x) {
      next->nextLML = next->prev;
      next = next->next;
      continue;
    }
    std::swap(next->top, next->bottom);
    next->current = next->bottom;
    next->nextLML = next->prev;
    next = next->next;
  }

localMinimum:
  LocalMinima_t* minimum = AllocLocalMinima();
  minimum->next = nullptr;
  minimum->y = next->prev->bottom.y;
  if (next->horizontal) {
    if (next->bottom.x != next->prev->bottom.x) {
      std::swap(next->top, next->bottom);
      next->current = next->bottom;
    }
  }
  if (next->horizontal || next->prev->deltaX > next->deltaX) {
    minimum->rightBound = next;
    minimum->leftBound = next->prev;
  } else {
    minimum->leftBound = next;
    minimum->rightBound = next->prev;
  }
  minimum->leftBound->side = EDGESIDE_LEFT;
  minimum->rightBound->side = EDGESIDE_RIGHT;

  LocalMinima_t* current = localMinimaList;
  LocalMinima_t* previous = nullptr;
  while (current != nullptr && minimum->y < current->y) {
    previous = current;
    current = current->next;
  }
  minimum->next = current;
  if (previous != nullptr)
    previous->next = minimum;
  else
    localMinimaList = minimum;

  for (;;) {
    Edge_t* following = next->next;
    if (following->top.y == next->top.y && !following->horizontal)
      break;
    next->nextLML = following;
    next = following;
    if (following->horizontal && following->bottom.x != following->prev->top.x) {
      std::swap(following->top, following->bottom);
      following->current = following->bottom;
    }
  }
  return next->next;
}

void idCutterClip::AddScanbeam(int y) {
  Scanbeam_t* current = scanbeams;
  Scanbeam_t* previous = nullptr;
  while (current != nullptr) {
    if (y > current->y)
      break;
    previous = current;
    current = current->next;
  }
  if (previous == nullptr || y != previous->y) {
    Scanbeam_t* value = AllocScanbeam();
    value->y = y;
    value->next = current;
    if (previous != nullptr)
      previous->next = value;
    else
      scanbeams = value;
  }
}

void idCutterClip::AddJoinRecord(Edge_t* edge1, Edge_t* edge2,
                                 std::int16_t outIndexEdge1, std::int16_t outIndexEdge2) {
  JoinRecord_t* record = AllocJoinRecord();
  record->point1a = edge1->current;
  record->point1b = edge1->top;
  record->point2a = edge2->current;
  record->point2b = edge2->top;
  record->polyIndex1 =
      outIndexEdge1 == -1 ? edge1->outIndex : outIndexEdge1;
  record->polyIndex2 =
      outIndexEdge2 == -1 ? edge2->outIndex : outIndexEdge2;
  joins.Append(record);
}

idCutterClip::PolygonPoint_t* idCutterClip::AddPolygonPoint(Edge_t* edge1, Edge_t* edge2,
                                                            const idVec2i& pos) {
  const std::int16_t outIndex = edge1->outIndex;
  if (outIndex == -1) {
    const int polygonIndex = polygons.Num();
    PolygonPoint_t* point = AllocPolygonPoint();
    point->index = polygonIndex;
    point->next = point;
    point->prev = point;
    point->pos = pos;
    Polygon_t* polygon = AllocPolygon();
    polygon->index = polygonIndex;
    polygon->points = point;
    polygon->bottomPoint = point;
    polygon->bottomEdge1 = edge1;
    polygon->bottomEdge2 = edge2;
    bool hole = false;
    for (Edge_t* previous = edge1->prevAEL; previous != nullptr;
         previous = previous->prevAEL) {
      if (previous->outIndex != -1) {
        hole = !hole;
        if (polygon->firstLeft == nullptr)
          polygon->firstLeft = polygons[previous->outIndex];
      }
    }
    polygon->hole = hole;
    edge1->outIndex = static_cast<std::int16_t>(polygonIndex);
    polygons.Append(polygon);
  } else {
    Polygon_t* polygon = polygons[outIndex];
    PolygonPoint_t* points = polygon->points;
    PolygonPoint_t* bottom = polygon->bottomPoint;
    PolygonPoint_t* adjacent = edge1->side == EDGESIDE_RIGHT
                                  ? points->prev
                                  : points;
    if (!SamePoint(pos, adjacent->pos)) {
      PolygonPoint_t* point = AllocPolygonPoint();
      point->index = polygon->index;
      point->pos = pos;
      if (pos.y == bottom->pos.y && pos.x < bottom->pos.x) {
        polygon->bottomPoint = point;
        polygon->bottomEdge1 = edge1;
        polygon->bottomEdge2 = edge2;
      }
      point->next = points;
      point->prev = points->prev;
      points->prev->next = point;
      points->prev = point;
      if (edge1->side == EDGESIDE_LEFT)
        polygon->points = point;
    }
  }
  return nullptr;
}

void idCutterClip::AddLocalMinPolygon(Edge_t* edge1, Edge_t* edge2,
                                     const idVec2i& pos) {
  Edge_t* left = edge1;
  Edge_t* right = edge2;
  if (edge2->horizontal || edge1->deltaX > edge2->deltaX) {
    left = edge2;
    right = edge1;
  }
  AddPolygonPoint(left, right, pos);
  right->outIndex = left->outIndex;
  left->side = EDGESIDE_LEFT;
  right->side = EDGESIDE_RIGHT;
}

void idCutterClip::AddLocalMaxPolygon(Edge_t* edge1, Edge_t* edge2,
                                     const idVec2i& pos) {
  AddPolygonPoint(edge1, nullptr, pos);
  if (edge1->outIndex == edge2->outIndex) {
    edge1->outIndex = -1;
    edge2->outIndex = -1;
  } else {
    AppendPolygon(edge1, edge2);
  }
}

void idCutterClip::AddEdgePolygon(addEdge_t addFlags, Edge_t* edge1, Edge_t* edge2,
                                  const idVec2i& pos) {
  if ((addFlags & ADDEDGE_LEFT) != 0)
    AddPolygonPoint(edge1, edge2, pos);
  if ((addFlags & ADDEDGE_RIGHT) != 0)
    AddPolygonPoint(edge2, edge1, pos);
  const std::uint16_t side = edge1->side;
  edge1->side = edge2->side;
  edge2->side = side;
  const std::int16_t outIndex = edge1->outIndex;
  edge1->outIndex = edge2->outIndex;
  edge2->outIndex = outIndex;
}

void idCutterClip::IntersectEdges(Edge_t* edge1, Edge_t* edge2, const idVec2i& pos,
                                  intersectProtects_t protects) {
  const bool edge1Stops = (protects & INTERSECTPROTECTS_LEFT) == 0 &&
                          edge1->nextLML == nullptr && SamePoint(pos, edge1->top);
  const bool edge2Stops = (protects & INTERSECTPROTECTS_RIGHT) == 0 &&
                          edge2->nextLML == nullptr && SamePoint(pos, edge2->top);
  const bool edge1HasOutput = edge1->outIndex != -1;
  const bool edge2HasOutput = edge2->outIndex != -1;
  const bool edge1Subject = edge1->polyType == POLYTYPE_SUBJECT;
  const bool edge2Subject = edge2->polyType == POLYTYPE_SUBJECT;

  if (edge1Subject == edge2Subject) {
    const fillType_t fill = edge1Subject ? fillTypeSubject : fillTypeClip;
    if (fill == FILLTYPE_EVEN_ODD) {
      std::swap(edge1->windingCount, edge2->windingCount);
    } else if (fill == FILLTYPE_NON_ZERO) {
      const int count1 = edge1->windingCount;
      const int sum = count1 + edge2->windingDelta;
      edge1->windingCount = static_cast<std::int16_t>(sum != 0 ? sum : -count1);
      const int count2 = edge2->windingCount;
      edge2->windingCount = static_cast<std::int16_t>(
          count2 == edge1->windingDelta ? -count2 : count2 - edge1->windingDelta);
    }
  } else {
    const fillType_t fill1 = edge2Subject ? fillTypeSubject : fillTypeClip;
    if (fill1 == FILLTYPE_EVEN_ODD)
      edge1->windingCount2 = edge1->windingCount2 == 0;
    else if (fill1 == FILLTYPE_NON_ZERO)
      edge1->windingCount2 += edge2->windingDelta;

    const fillType_t fill2 = edge1Subject ? fillTypeSubject : fillTypeClip;
    if (fill2 == FILLTYPE_EVEN_ODD)
      edge2->windingCount2 = edge2->windingCount2 == 0;
    else if (fill2 == FILLTYPE_NON_ZERO)
      edge2->windingCount2 -= edge1->windingDelta;
  }

  const int winding1 = std::abs(static_cast<int>(edge1->windingCount));
  const int windingOther1 = std::abs(static_cast<int>(edge1->windingCount2));
  const int winding2 = std::abs(static_cast<int>(edge2->windingCount));
  const int windingOther2 = std::abs(static_cast<int>(edge2->windingCount2));

  if (edge1HasOutput) {
    if (edge2HasOutput) {
      if (!edge1Stops && !edge2Stops &&
          (edge1Subject == edge2Subject || clipMode == CLIPMODE_XOR) &&
          winding1 <= 1 && winding2 <= 1) {
        AddEdgePolygon(ADDEDGE_BOTH, edge1, edge2, pos);
        return;
      }
      AddLocalMaxPolygon(edge1, edge2, pos);
    } else if (winding2 < 2 &&
               (clipMode != CLIPMODE_INTERSECT || edge2Subject || windingOther2 != 0)) {
      AddPolygonPoint(edge1, edge2, pos);
      const std::uint16_t side = edge1->side;
      edge1->side = edge2->side;
      edge2->side = side;
      std::swap(edge1->outIndex, edge2->outIndex);
    }
  } else if (edge2HasOutput) {
    if (winding1 < 2 &&
        (clipMode != CLIPMODE_INTERSECT || edge1Subject || windingOther1 != 0)) {
      AddPolygonPoint(edge2, edge1, pos);
      const std::uint16_t side = edge1->side;
      edge1->side = edge2->side;
      edge2->side = side;
      std::swap(edge1->outIndex, edge2->outIndex);
    }
  } else if (winding1 < 2 && winding2 < 2 && !edge1Stops) {
    if (!edge2Stops) {
      bool addLocalMinimum = edge1Subject != edge2Subject;
      if (!addLocalMinimum) {
        if (winding1 != 1 || winding2 != 1) {
          const std::uint16_t side = edge1->side;
          edge1->side = edge2->side;
          edge2->side = side;
          return;
        }
        switch (clipMode) {
          case CLIPMODE_INTERSECT:
            addLocalMinimum = windingOther1 > 0 && windingOther2 > 0;
            break;
          case CLIPMODE_UNION:
            addLocalMinimum = windingOther1 == 0 && windingOther2 == 0;
            break;
          case CLIPMODE_DIFFERENCE:
            addLocalMinimum =
                (!edge1Subject && !edge2Subject && windingOther1 != 0 &&
                 windingOther2 != 0) ||
                (edge1Subject && edge2Subject && windingOther1 == 0 &&
                 windingOther2 == 0);
            break;
          case CLIPMODE_XOR:
            addLocalMinimum = true;
            break;
        }
      }
      if (addLocalMinimum) {
        AddLocalMinPolygon(edge1, edge2, pos);
        return;
      }
    }
  }

  if (edge1Stops != edge2Stops &&
      ((edge1Stops && edge1HasOutput) || (edge2Stops && edge2HasOutput))) {
    const std::uint16_t side = edge1->side;
    edge1->side = edge2->side;
    edge2->side = side;
    std::swap(edge1->outIndex, edge2->outIndex);
  }
  if (edge1Stops)
    RemoveEdgeFromAEL(edge1);
  if (edge2Stops)
    RemoveEdgeFromAEL(edge2);
}

idCutterClip::Edge_t* idCutterClip::UpdateEdgeInAEL(Edge_t* edge) {
  Edge_t* replacement = edge->nextLML;
  if (replacement == nullptr)
    return nullptr;
  replacement->outIndex = edge->outIndex;
  replacement->prevAEL = edge->prevAEL;
  replacement->nextAEL = edge->nextAEL;
  if (replacement->prevAEL != nullptr)
    replacement->prevAEL->nextAEL = replacement;
  else
    activeEdges = replacement;
  if (replacement->nextAEL != nullptr)
    replacement->nextAEL->prevAEL = replacement;
  replacement->side = edge->side;
  replacement->windingDelta = edge->windingDelta;
  replacement->windingCount = edge->windingCount;
  replacement->windingCount2 = edge->windingCount2;
  if (!replacement->horizontal)
    AddScanbeam(replacement->top.y);
  return replacement;
}

void idCutterClip::ProcessIntersectList() {
  for (IntersectNode_t* node = intersectNodes; node != nullptr; node = node->next) {
    IntersectEdges(node->edge1, node->edge2, node->pos, INTERSECTPROTECTS_BOTH);
    SwapEdgeInAEL(node->edge1, node->edge2);
  }
  while (intersectNodes != nullptr) {
    IntersectNode_t* node = intersectNodes;
    intersectNodes = node->next;
    node->next = intersectNodesPool;
    --numIntersectNode;
    intersectNodesPool = node;
  }
}

void idCutterClip::BuildIntersectList(int yBottom, int yTop) {
  intersectNodes = nullptr;
  if (activeEdges == nullptr)
    return;

  auto topX = [yTop](Edge_t* edge) {
    if (edge->top.y == yTop)
      return WrapMultiply(precisionModifier, edge->top.x);
    return WrapAdd(
        WrapMultiply(precisionModifier, edge->bottom.x),
        static_cast<int>(std::floor(
            static_cast<float>(WrapMultiply(
                precisionModifier, WrapSubtract(yTop, edge->bottom.y))) *
                edge->deltaX +
            0.5f)));
  };
  auto bottomX = [yBottom](Edge_t* edge) {
    if (edge->top.y == yBottom)
      return WrapMultiply(precisionModifier, edge->top.x);
    return WrapAdd(
        WrapMultiply(precisionModifier, edge->bottom.x),
        static_cast<int>(std::floor(
            static_cast<float>(WrapMultiply(
                precisionModifier, WrapSubtract(yBottom, edge->bottom.y))) *
                edge->deltaX +
            0.5f)));
  };

  activeEdges->tmpX = topX(activeEdges);
  sortedEdges = activeEdges;
  activeEdges->prevSEL = nullptr;
  for (Edge_t* edge = activeEdges->nextAEL; edge != nullptr; edge = edge->nextAEL) {
    edge->nextSEL = nullptr;
    edge->prevSEL = edge->prevAEL;
    edge->prevAEL->nextSEL = edge;
    edge->tmpX = topX(edge);
  }

  for (;;) {
    Edge_t* edge = sortedEdges;
    if (edge == nullptr)
      break;
    Edge_t* next = edge->nextSEL;
    bool swapped = false;
    while (next != nullptr) {
      idVec2i position;
      if (edge->tmpX > next->tmpX && EdgeEdgeIntersection(edge, next, &position)) {
        if (position.y > yBottom) {
          position.x = bottomX(edge);
          position.y = yBottom;
        }
        IntersectNode_t* node = AllocIntersectNode();
        node->edge1 = edge;
        node->edge2 = next;
        node->pos = position;
        node->next = nullptr;

        IntersectNode_t* current = intersectNodes;
        IntersectNode_t* previous = nullptr;
        while (current != nullptr) {
          bool insertBefore = node->pos.y > current->pos.y;
          if (node->pos.y == current->pos.y) {
            insertBefore = current->pos.x > node->pos.x;
            if (edge == current->edge1 || next == current->edge1 ||
                edge == current->edge2 || next == current->edge2) {
              Edge_t* shared = (edge == current->edge1 || next == current->edge1)
                                   ? current->edge1
                                   : current->edge2;
              if (shared->deltaX > 0.0f)
                insertBefore = !insertBefore;
            }
          }
          if (insertBefore)
            break;
          previous = current;
          current = current->next;
        }
        node->next = current;
        if (previous != nullptr)
          previous->next = node;
        else
          intersectNodes = node;
        SwapEdgeInSEL(edge, next);
        swapped = true;
      } else {
        edge = next;
      }
      next = edge->nextSEL;
    }
    if (edge->prevSEL == nullptr)
      break;
    edge->prevSEL->nextSEL = nullptr;
    if (!swapped)
      break;
  }
  sortedEdges = nullptr;
}

bool idCutterClip::DoMaxima(Edge_t* edge, Edge_t* edgeMaxPair, int y) {
  Edge_t* next = edge->nextAEL;
  idVec2i pos;
  pos.x = edge->top.x;
  pos.y = y;
  while (next != nullptr && next != edgeMaxPair) {
    IntersectEdges(edge, next, pos, INTERSECTPROTECTS_BOTH);
    next = next->nextAEL;
  }
  if (next != edgeMaxPair)
    return false;
  if (edge->outIndex == -1 && edgeMaxPair->outIndex == -1) {
    RemoveEdgeFromAEL(edge);
    RemoveEdgeFromAEL(edgeMaxPair);
    return true;
  }
  if (edge->outIndex != -1 && edgeMaxPair->outIndex != -1) {
    IntersectEdges(edge, edgeMaxPair, pos, INTERSECTPROTECTS_NONE);
    return true;
  }
  return false;
}

void idCutterClip::AddLocalMinimaToAEL(int yBottom) {
  while (currentLocalMinima != nullptr && currentLocalMinima->y == yBottom) {
    LocalMinima_t* minima = currentLocalMinima;
    Edge_t* left = minima->leftBound;
    Edge_t* right = minima->rightBound;
    AddEdgeToAEL(left);
    AddScanbeam(left->top.y);
    AddEdgeToAEL(right);

    const fillType_t fill = left->polyType == POLYTYPE_SUBJECT
                                ? fillTypeSubject
                                : fillTypeClip;
    if (fill == FILLTYPE_EVEN_ODD) {
      left->windingDelta = 1;
      right->windingDelta = 1;
    } else if (fill == FILLTYPE_NON_ZERO) {
      right->windingDelta = -left->windingDelta;
    }
    SetWindingCount(left);
    right->windingCount = left->windingCount;
    right->windingCount2 = left->windingCount2;

    if (!right->horizontal) {
      AddScanbeam(right->top.y);
    } else {
      right->prevSEL = nullptr;
      right->nextSEL = sortedEdges;
      if (sortedEdges != nullptr)
        sortedEdges->prevSEL = right;
      sortedEdges = right;
      AddScanbeam(right->nextLML->top.y);
    }

    if (IsContributing(clipMode, left)) {
      idVec2i pos;
      pos.x = left->current.x;
      pos.y = minima->y;
      AddLocalMinPolygon(left, right, pos);
    }

    if (left->outIndex != -1) {
      Edge_t* previous = left->prevAEL;
      if (previous != nullptr && previous->outIndex != -1 &&
          previous->current.x == left->bottom.x && SlopesEqual(left, previous)) {
        AddJoinRecord(left, previous, left->outIndex, previous->outIndex);
      }
    }

    if (right->outIndex != -1 && right->horizontal) {
      for (int index = 0; index < horizontalJoins.Num(); ++index) {
        HorizontalJoinRecord_t* horizontal = horizontalJoins[index];
        if (IsSegmentOverlapping(horizontal->edge->current, horizontal->edge->top,
                                 right->bottom, right->top, nullptr, nullptr)) {
          AddJoinRecord(horizontal->edge, right, horizontal->savedIndex,
                        right->outIndex);
        }
      }
    }

    if (left->nextAEL != right) {
      if (right->outIndex != -1) {
        Edge_t* previous = right->prevAEL;
        if (previous->outIndex != -1 && SlopesEqual(previous, right))
          AddJoinRecord(right, previous, right->outIndex, previous->outIndex);
      }
      const idVec2i pos = left->current;
      for (Edge_t* edge = left->nextAEL; edge != right; edge = edge->nextAEL)
        IntersectEdges(right, edge, pos, INTERSECTPROTECTS_NONE);
    }

    currentLocalMinima = minima->next;
  }
}

bool idCutterClip::ProcessHorizontal(Edge_t* edge) {
  const int left = std::min(edge->current.x, edge->top.x);
  const int right = std::max(edge->current.x, edge->top.x);
  const horizontalDirection_t direction =
      edge->current.x < edge->top.x ? HORIZONTALDIRECTION_LEFTTORIGHT
                                    : HORIZONTALDIRECTION_RIGHTTOLEFT;

  Edge_t* maximaPair = nullptr;
  if (edge->nextLML == nullptr) {
    maximaPair = edge->next;
    if (maximaPair == nullptr || maximaPair->top.y != edge->top.y ||
        maximaPair->nextLML != nullptr || maximaPair->top.x != edge->top.x) {
      maximaPair = edge->prev;
    }
  }

  Edge_t* crossing = direction == HORIZONTALDIRECTION_LEFTTORIGHT
                         ? edge->nextAEL
                         : edge->prevAEL;
  while (crossing != nullptr) {
    Edge_t* nextCrossing = direction == HORIZONTALDIRECTION_LEFTTORIGHT
                               ? crossing->nextAEL
                               : crossing->prevAEL;

    bool process = maximaPair != nullptr;
    if (!process) {
      if (direction == HORIZONTALDIRECTION_LEFTTORIGHT)
        process = crossing->current.x <= right;
      else
        process = crossing->current.x >= left;
      if (!process)
        break;
    }

    if (crossing->current.x == edge->top.x && maximaPair == nullptr) {
      Edge_t* nextLML = edge->nextLML;
      if (SlopesEqual(crossing, nextLML)) {
        if (edge->outIndex != -1 && crossing->outIndex != -1)
          AddJoinRecord(nextLML, crossing, edge->outIndex, -1);
        break;
      }
      if (nextLML->deltaX > crossing->deltaX)
        break;
    }

    idVec2i pos;
    pos.x = crossing->current.x;
    pos.y = edge->current.y;
    if (crossing == maximaPair) {
      if (direction == HORIZONTALDIRECTION_LEFTTORIGHT)
        IntersectEdges(edge, crossing, pos, INTERSECTPROTECTS_NONE);
      else
        IntersectEdges(crossing, edge, pos, INTERSECTPROTECTS_NONE);
      return maximaPair->outIndex == -1;
    }

    bool protectedByHorizontal = false;
    for (Edge_t* horizontal = sortedEdges; horizontal != nullptr;
         horizontal = horizontal->nextSEL) {
      const int horizontalLeft = std::min(horizontal->current.x, horizontal->top.x);
      const int horizontalRight = std::max(horizontal->current.x, horizontal->top.x);
      if (crossing->top.x >= horizontalLeft && crossing->top.x <= horizontalRight) {
        protectedByHorizontal = true;
        break;
      }
    }

    if (direction == HORIZONTALDIRECTION_LEFTTORIGHT) {
      IntersectEdges(edge, crossing, pos,
                     protectedByHorizontal ? INTERSECTPROTECTS_BOTH
                                           : INTERSECTPROTECTS_LEFT);
    } else {
      IntersectEdges(crossing, edge, pos,
                     protectedByHorizontal ? INTERSECTPROTECTS_BOTH
                                           : INTERSECTPROTECTS_RIGHT);
    }
    SwapEdgeInAEL(edge, crossing);
    crossing = nextCrossing;
  }

  if (edge->nextLML != nullptr) {
    if (edge->outIndex != -1)
      AddPolygonPoint(edge, nullptr, edge->top);
    UpdateEdgeInAEL(edge);
  } else {
    if (edge->outIndex != -1) {
      idVec2i pos;
      pos.x = edge->top.x;
      pos.y = edge->current.y;
      IntersectEdges(edge, maximaPair, pos, INTERSECTPROTECTS_BOTH);
    }
    RemoveEdgeFromAEL(maximaPair);
    RemoveEdgeFromAEL(edge);
  }
  return true;
}

void idCutterClip::JoinEdges() {
  auto signedArea = [](PolygonPoint_t* first) {
    std::int32_t area = 0;
    PolygonPoint_t* point = first;
    PolygonPoint_t* next = point->next;
    do {
      area = WrapAdd(area,
                     WrapSubtract(WrapMultiply(point->pos.x, next->pos.y),
                                  WrapMultiply(point->pos.y, next->pos.x)));
      point = next;
      next = next->next;
    } while (point != first);
    return area;
  };
  auto distanceSquared = [](const idVec2i& left, const idVec2i& right) {
    const int dx = WrapSubtract(left.x, right.x);
    const int dy = WrapSubtract(left.y, right.y);
    return WrapAdd(WrapMultiply(dx, dx), WrapMultiply(dy, dy));
  };
  auto containsPoint = [](PolygonPoint_t* ring, const idVec2i& pos) {
    PolygonPoint_t* point = ring;
    do {
      if (SamePoint(point->pos, pos))
        return true;
      point = point->next;
    } while (point != ring);
    return false;
  };
  auto findOverlappingEdge = [](PolygonPoint_t* first, const idVec2i& segmentA,
                                const idVec2i& segmentB, PolygonPoint_t** edgeEnd,
                                idVec2i* overlapA,
                                idVec2i* overlapB) {
    if (first == nullptr)
      return false;
    PolygonPoint_t* point = first;
    do {
      if (Contour::SlopesEqual(segmentA, segmentB, point->pos,
                               point->prev->pos) &&
          Contour::SlopesEqual(segmentA, segmentB, point->pos) &&
          IsSegmentOverlapping(segmentA, segmentB, point->pos, point->prev->pos,
                               overlapA, overlapB)) {
        *edgeEnd = point;
        return true;
      }
      point = point->next;
    } while (point != first);
    return false;
  };
  auto insertJoinPoints = [this, &distanceSquared](PolygonPoint_t* edgeEnd,
                                                   const idVec2i& joinStart,
                                                   const idVec2i& joinEnd,
                                                   PolygonPoint_t** startNode,
                                                   PolygonPoint_t** endNode) {
    PolygonPoint_t* previous = edgeEnd->prev;
    PolygonPoint_t* start;
    if (SamePoint(joinStart, edgeEnd->pos)) {
      start = edgeEnd;
    } else if (SamePoint(joinStart, previous->pos)) {
      start = previous;
    } else {
      start = AllocPolygonPoint();
      start->pos = joinStart;
      previous->next = start;
      edgeEnd->prev = start;
      start->prev = previous;
      start->next = edgeEnd;
    }

    PolygonPoint_t* end;
    if (SamePoint(joinEnd, edgeEnd->pos)) {
      end = edgeEnd;
    } else if (SamePoint(joinEnd, start->pos)) {
      end = start;
    } else if (SamePoint(joinEnd, previous->pos)) {
      end = previous;
    } else {
      PolygonPoint_t* first = edgeEnd;
      PolygonPoint_t* second = start;
      if (distanceSquared(joinEnd, edgeEnd->pos) >
          distanceSquared(start->pos, edgeEnd->pos)) {
        first = start;
        second = previous;
      }
      end = AllocPolygonPoint();
      end->pos = joinEnd;
      if (first->next != second)
        std::swap(first, second);
      first->next = end;
      second->prev = end;
      end->prev = first;
      end->next = second;
    }
    *startNode = start;
    *endNode = end;
  };

  for (int joinIndex = 0; joinIndex < joins.Num(); ++joinIndex) {
    JoinRecord_t* record = joins[joinIndex];
    Polygon_t* polygon1 = polygons[record->polyIndex1];
    Polygon_t* polygon2 = polygons[record->polyIndex2];
    if (polygon1->points == nullptr)
      continue;

    PolygonPoint_t* edge1 = nullptr;
    idVec2i overlap1A;
    idVec2i overlap1B;
    if (!findOverlappingEdge(polygon1->points, record->point2a, record->point2b,
                             &edge1, &overlap1A, &overlap1B)) {
      continue;
    }

    PolygonPoint_t* secondStart = polygon2->points;
    if (record->polyIndex1 == record->polyIndex2) {
      secondStart = edge1->next;
      if (secondStart == edge1)
        continue;
    }
    PolygonPoint_t* edge2 = nullptr;
    idVec2i overlap2A;
    idVec2i overlap2B;
    if (!findOverlappingEdge(secondStart, record->point1a, record->point1b,
                             &edge2, &overlap2A, &overlap2B)) {
      continue;
    }

    idVec2i joinStart;
    idVec2i joinEnd;
    if (!IsSegmentOverlapping(overlap1A, overlap1B, overlap2A, overlap2B,
                              &joinStart, &joinEnd)) {
      continue;
    }

    std::int32_t polygon1Area = 0;
    std::int32_t polygon2Area = 0;
    if (record->polyIndex1 != record->polyIndex2) {
      polygon1Area = signedArea(polygon1->points);
      polygon2Area = signedArea(polygon2->points);
    }

    PolygonPoint_t* firstStart;
    PolygonPoint_t* firstEnd;
    PolygonPoint_t* secondJoinStart;
    PolygonPoint_t* secondJoinEnd;
    insertJoinPoints(edge1, joinStart, joinEnd, &firstStart, &firstEnd);
    insertJoinPoints(edge2, joinStart, joinEnd, &secondJoinStart, &secondJoinEnd);

    if (firstStart->next == firstEnd && secondJoinStart->prev == secondJoinEnd) {
      firstStart->next = secondJoinStart;
      secondJoinStart->prev = firstStart;
      firstEnd->prev = secondJoinEnd;
      secondJoinEnd->next = firstEnd;
    } else if (firstStart->prev == firstEnd &&
               secondJoinStart->next == secondJoinEnd) {
      firstStart->prev = secondJoinStart;
      secondJoinStart->next = firstStart;
      firstEnd->next = secondJoinEnd;
      secondJoinEnd->prev = firstEnd;
    } else {
      continue;
    }

    if (record->polyIndex1 == record->polyIndex2) {
      const std::int32_t firstArea = signedArea(firstStart);
      const std::int32_t secondArea = signedArea(firstEnd);
      PolygonPoint_t* originalRing = firstStart;
      PolygonPoint_t* newRing = firstEnd;
      if (WrapAbs(firstArea) < WrapAbs(secondArea)) {
        originalRing = firstEnd;
        newRing = firstStart;
      }

      polygon1->points = FindBottom(originalRing);
      polygon1->bottomPoint = polygon1->points;
      polygon1->bottomPoint->index = polygon1->index;

      Polygon_t* newPolygon = AllocPolygon();
      newPolygon->index = polygons.Num();
      newPolygon->points = FindBottom(newRing);
      newPolygon->bottomPoint = newPolygon->points;
      newPolygon->bottomPoint->index = newPolygon->index;
      record->polyIndex2 = static_cast<std::int16_t>(newPolygon->index);
      polygons.Append(newPolygon);

      if (PointInPolygon(newRing->pos, originalRing)) {
        newPolygon->hole = !polygon1->hole;
        newPolygon->firstLeft = polygon1;
        if (newPolygon->hole == IsClockwise(newRing))
          ReverseLinks(newRing);
      } else {
        const bool separate = !PointInPolygon(originalRing->pos, newRing);
        newPolygon->hole = polygon1->hole;
        if (separate) {
          newPolygon->firstLeft = polygon1->firstLeft;
        } else {
          polygon1->hole = !polygon1->hole;
          newPolygon->firstLeft = polygon1->firstLeft;
          polygon1->firstLeft = newPolygon;
          if (polygon1->hole == IsClockwise(polygon1->points))
            ReverseLinks(polygon1->points);
        }
      }

      for (int future = joinIndex + 1; future < joins.Num(); ++future) {
        JoinRecord_t* later = joins[future];
        if (later->polyIndex1 == record->polyIndex1 &&
            containsPoint(firstEnd, later->point1a)) {
          later->polyIndex1 = record->polyIndex2;
        }
        if (later->polyIndex2 == record->polyIndex1 &&
            containsPoint(firstEnd, later->point2a)) {
          later->polyIndex2 = record->polyIndex2;
        }
      }
      FixupPolygon(polygon1);
      FixupPolygon(newPolygon);
    } else {
      Polygon_t* survivor = polygon1;
      Polygon_t* absorbed = polygon2;
      std::int16_t survivorIndex = static_cast<std::int16_t>(polygon1->index);
      std::int16_t absorbedIndex = static_cast<std::int16_t>(polygon2->index);
      if (polygon1Area < polygon2Area) {
        survivor = polygon2;
        absorbed = polygon1;
        polygon2->points = polygon1->points;
        survivorIndex = static_cast<std::int16_t>(polygon2->index);
        absorbedIndex = static_cast<std::int16_t>(polygon1->index);
      }
      absorbed->appendLink = survivor;
      absorbed->points = nullptr;
      absorbed->bottomPoint = nullptr;
      if (survivor->hole)
        survivor->hole = survivor->hole && absorbed->hole;

      for (int future = joinIndex + 1; future < joins.Num(); ++future) {
        if (joins[future]->polyIndex1 == absorbedIndex)
          joins[future]->polyIndex1 = survivorIndex;
        if (joins[future]->polyIndex2 == absorbedIndex)
          joins[future]->polyIndex2 = survivorIndex;
      }
      if (survivor->points != nullptr)
        FixupPolygon(survivor);
      else
        FixupPolygon(absorbed);
    }
  }
}

bool idCutterClip::ProcessEdgesAtTopOfScanbeam(int y) {
  Edge_t* edge = activeEdges;
  while (edge != nullptr) {
    Edge_t* maximaPair = nullptr;
    if (edge->top.y == y && edge->nextLML == nullptr) {
      maximaPair = edge->next;
      if (maximaPair == nullptr || maximaPair->top.y != edge->top.y ||
          maximaPair->nextLML != nullptr || maximaPair->top.x != edge->top.x) {
        maximaPair = edge->prev;
      }
    }
    if (maximaPair != nullptr && !maximaPair->horizontal) {
      Edge_t* previous = edge->prevAEL;
      if (!DoMaxima(edge, maximaPair, y))
        return false;
      edge = previous != nullptr ? previous->nextAEL : activeEdges;
      continue;
    }

    if (edge->top.y == y && edge->nextLML != nullptr &&
        edge->nextLML->horizontal) {
      if (edge->outIndex != -1) {
        AddPolygonPoint(edge, nullptr, edge->top);
        Edge_t* horizontalEdge = edge->nextLML;
        for (int index = 0; index < horizontalJoins.Num(); ++index) {
          HorizontalJoinRecord_t* horizontal = horizontalJoins[index];
          if (IsSegmentOverlapping(horizontal->edge->bottom, horizontal->edge->top,
                                   horizontalEdge->bottom, horizontalEdge->top,
                                   nullptr, nullptr)) {
            AddJoinRecord(horizontal->edge, horizontalEdge,
                          horizontal->savedIndex, edge->outIndex);
          }
        }
        HorizontalJoinRecord_t* horizontal = AllocHorizontalJoinRecord();
        horizontal->edge = horizontalEdge;
        horizontal->savedIndex = edge->outIndex;
        horizontalJoins.Append(horizontal);
      }
      edge = UpdateEdgeInAEL(edge);
      edge->prevSEL = nullptr;
      edge->nextSEL = sortedEdges;
      if (sortedEdges != nullptr)
        sortedEdges->prevSEL = edge;
      sortedEdges = edge;
    } else {
      int scaledX;
      if (edge->top.y == y) {
        scaledX = WrapMultiply(precisionModifier, edge->top.x);
      } else {
        scaledX = WrapAdd(
            WrapMultiply(precisionModifier, edge->bottom.x),
            static_cast<int>(std::floor(
                static_cast<float>(WrapMultiply(
                    precisionModifier, WrapSubtract(y, edge->bottom.y))) *
                    edge->deltaX +
                0.5f)));
      }
      edge->current.y = y;
      edge->current.x = scaledX / precisionModifier;
    }
    edge = edge->nextAEL;
  }

  while (sortedEdges != nullptr) {
    Edge_t* horizontal = sortedEdges;
    Edge_t* previous = horizontal->prevSEL;
    Edge_t* next = horizontal->nextSEL;
    horizontal->prevSEL = nullptr;
    horizontal->nextSEL = nullptr;
    if (previous != nullptr)
      previous->nextSEL = next;
    if (next != nullptr)
      next->prevSEL = previous;
    if (previous == nullptr)
      sortedEdges = next;
    if (!ProcessHorizontal(horizontal))
      return false;
  }

  edge = activeEdges;
  while (edge != nullptr) {
    if (edge->top.y == y && edge->nextLML != nullptr) {
      if (edge->outIndex != -1)
        AddPolygonPoint(edge, nullptr, edge->top);
      edge = UpdateEdgeInAEL(edge);
      if (edge != nullptr && edge->outIndex != -1) {
        Edge_t* previous = edge->prevAEL;
        Edge_t* next = edge->nextAEL;
        if (previous != nullptr && previous->outIndex != -1 &&
            SamePoint(previous->current, edge->bottom) &&
            Contour::SlopesEqual(edge->bottom, edge->top, previous->top)) {
          AddPolygonPoint(previous, nullptr, edge->bottom);
          AddJoinRecord(edge, previous, edge->outIndex, previous->outIndex);
        } else if (next != nullptr && next->outIndex != -1 &&
                   next->current.y > next->top.y &&
                   next->current.y < next->bottom.y &&
                   SamePoint(next->current, edge->bottom) &&
                   Contour::SlopesEqual(edge->bottom, edge->top, next->top)) {
          AddPolygonPoint(next, nullptr, edge->bottom);
          AddJoinRecord(edge, next, edge->outIndex, next->outIndex);
        }
      }
    }
    edge = edge->nextAEL;
  }
  return true;
}
