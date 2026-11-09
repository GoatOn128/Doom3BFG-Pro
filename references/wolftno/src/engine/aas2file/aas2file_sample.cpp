#include "engine/aas2file/aas2file.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>

namespace {
bool PointInside(const idVec3& point, const idBounds& bounds, float epsilon = 0.0f) {
  return point.x >= bounds[0].x - epsilon && point.x <= bounds[1].x + epsilon &&
         point.y >= bounds[0].y - epsilon && point.y <= bounds[1].y + epsilon &&
         point.z >= bounds[0].z - epsilon && point.z <= bounds[1].z + epsilon;
}
bool BoundsIntersect(const idBounds& a, const idBounds& b) {
  for (int axis = 0; axis < 3; ++axis)
    if (a[1][axis] < b[0][axis] || a[0][axis] > b[1][axis]) return false;
  return true;
}
bool SegmentBounds(const idVec3& start, const idVec3& end, const idBounds& bounds,
                   float& entry, float& exit) {
  entry = 0.0f; exit = 1.0f;
  const idVec3 delta = end - start;
  for (int axis = 0; axis < 3; ++axis) {
    if (std::fabs(delta[axis]) < 1.0e-8f) {
      if (start[axis] < bounds[0][axis] || start[axis] > bounds[1][axis]) return false;
      continue;
    }
    float first = (bounds[0][axis] - start[axis]) / delta[axis];
    float second = (bounds[1][axis] - start[axis]) / delta[axis];
    if (first > second) std::swap(first, second);
    entry = std::max(entry, first); exit = std::min(exit, second);
    if (entry > exit) return false;
  }
  return exit >= 0.0f && entry <= 1.0f;
}
float PlaneDistance(const idPlane& plane, const idVec3& point) {
  return plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d;
}
} // namespace

idVec3 idAAS2File::AreaCenter(int areaNum) const {
  return areaNum >= 0 && static_cast<std::size_t>(areaNum) < areas.size() ? areas[areaNum].center
                                                                         : idVec3{};
}
idBounds idAAS2File::EdgeBounds(int edgeNum) const {
  idBounds result; result.Clear();
  edgeNum = std::abs(edgeNum);
  if (static_cast<std::size_t>(edgeNum) >= edges.size()) return result;
  for (int vertex : edges[edgeNum].vertex)
    if (vertex >= 0 && static_cast<std::size_t>(vertex) < vertices.size()) result.AddPoint(vertices[vertex]);
  return result;
}
idBounds idAAS2File::AreaBounds(int areaNum) const {
  if (areaNum >= 0 && static_cast<std::size_t>(areaNum) < areas.size()) return areas[areaNum].bounds;
  idBounds empty; empty.Clear(); return empty;
}
int idAAS2File::PointAreaNum(int tree, const idVec3* origin) const {
  if (!origin) return -1;
  for (std::size_t i = 0; i < areas.size(); ++i)
    if ((tree < 0 || areas[i].treeNum == tree) && PointInside(*origin, areas[i].bounds, 0.001f)) return static_cast<int>(i);
  return -1;
}
int idAAS2File::PointAreaNumMaxheight(int tree, const idVec3* origin, float maxHeight) const {
  if (!origin) return -1;
  int best = -1; float bestHeight = maxHeight;
  for (std::size_t i = 0; i < areas.size(); ++i) {
    if (tree >= 0 && areas[i].treeNum != tree) continue;
    if (origin->x < areas[i].bounds[0].x || origin->x > areas[i].bounds[1].x ||
        origin->y < areas[i].bounds[0].y || origin->y > areas[i].bounds[1].y) continue;
    const float height = origin->z - areas[i].bounds[1].z;
    if (height >= -0.001f && height <= bestHeight) { bestHeight = height; best = static_cast<int>(i); }
  }
  return best;
}
int idAAS2File::BoundsAreaNums(int tree, const idBounds* bounds, int* output, int maxAreas) const {
  if (!bounds || maxAreas < 0) return 0;
  int count = 0;
  for (std::size_t i = 0; i < areas.size(); ++i) {
    if ((tree < 0 || areas[i].treeNum == tree) && BoundsIntersect(*bounds, areas[i].bounds)) {
      if (output && count < maxAreas) output[count] = static_cast<int>(i);
      ++count;
    }
  }
  return count;
}
void idAAS2File::FlagBoundsAreas(int tree, const idBounds* bounds, bool* areasTouched) const {
  if (!areasTouched) return;
  for (std::size_t i = 0; i < areas.size(); ++i)
    areasTouched[i] = bounds && (tree < 0 || areas[i].treeNum == tree) && BoundsIntersect(*bounds, areas[i].bounds);
}
int idAAS2File::BoundsReachableAreaNum(int tree, const idBounds* bounds,
                                       std::uint16_t areaFlags, int excludeTravelFlags) const {
  if (!bounds) return -1;
  const idVec3 center = ((*bounds)[0] + (*bounds)[1]) * 0.5f;
  int best = -1; float bestDistance = std::numeric_limits<float>::max();
  for (std::size_t i = 0; i < areas.size(); ++i) {
    const auto& area = areas[i];
    if ((tree >= 0 && area.treeNum != tree) || !BoundsIntersect(*bounds, area.bounds) ||
        (area.flags & areaFlags) != areaFlags || (area.travelFlags & excludeTravelFlags)) continue;
    const float distance = (area.center - center).LengthSqr();
    if (distance < bestDistance) { bestDistance = distance; best = static_cast<int>(i); }
  }
  return best;
}
bool idAAS2File::TraceHeight(int tree, aas2TraceHeight_t* trace,
                             const idVec3* start, const idVec3* end) const {
  if (!trace || !start || !end) return false;
  aas2Trace_t base;
  const bool result = Trace(tree, &base, start, end);
  static_cast<aas2Trace_t&>(*trace) = base;
  trace->height = base.endPos.z;
  return result;
}
bool idAAS2File::GetFloorEdgeSplitPoints(floorEdgeSplitPoint_t* nearest,
                                         floorEdgeSplitPoint_t* furthest, int areaNum,
                                         const idPlane* pathPlane, const idPlane* nearPlane) const {
  if (!nearest || !furthest || !pathPlane || !nearPlane || areaNum < 0 ||
      static_cast<std::size_t>(areaNum) >= areas.size()) return false;
  nearest->distance = std::numeric_limits<float>::max(); furthest->distance = -std::numeric_limits<float>::max();
  bool found = false;
  for (int encoded : areas[areaNum].edges) {
    const int edgeNum = std::abs(encoded); if (static_cast<std::size_t>(edgeNum) >= edges.size()) continue;
    for (int vertex : edges[edgeNum].vertex) {
      if (vertex < 0 || static_cast<std::size_t>(vertex) >= vertices.size()) continue;
      const idVec3& point = vertices[vertex];
      if (std::fabs(PlaneDistance(*pathPlane, point)) > 0.25f) continue;
      const float distance = PlaneDistance(*nearPlane, point);
      if (distance < nearest->distance) *nearest = {point, distance, edgeNum};
      if (distance > furthest->distance) *furthest = {point, distance, edgeNum};
      found = true;
    }
  }
  return found;
}
void idAAS2File::MaxTreeDepth_r(int nodeNum, int* depth, int* maxDepth) const {
  if (!depth || !maxDepth || nodeNum <= 0 || static_cast<std::size_t>(nodeNum) >= nodes.size()) return;
  ++*depth; *maxDepth = std::max(*maxDepth, *depth);
  for (int child : nodes[nodeNum].children) if (child > 0) MaxTreeDepth_r(child, depth, maxDepth);
  --*depth;
}
int idAAS2File::MaxTreeDepth(int tree) const {
  if (tree < 0 || static_cast<std::size_t>(tree) >= trees.size()) return 0;
  int depth = 0, maximum = 0; MaxTreeDepth_r(trees[tree].rootNode, &depth, &maximum); return maximum;
}
bool idAAS2File::Trace(int tree, aas2Trace_t* trace, const idVec3* start, const idVec3* end) const {
  if (!trace || !start || !end) return false;
  *trace = {}; trace->endPos = *end; trace->areaNum = PointAreaNum(tree, end);
  trace->lastAreaNum = PointAreaNum(tree, start); trace->startSolid = trace->lastAreaNum < 0;
  float best = 1.0f; int bestArea = trace->areaNum;
  for (std::size_t i = 0; i < areas.size(); ++i) {
    if (tree >= 0 && areas[i].treeNum != tree) continue;
    float entry{}, exit{};
    if (SegmentBounds(*start, *end, areas[i].bounds, entry, exit) && entry < best) {
      best = std::max(0.0f, entry); bestArea = static_cast<int>(i);
    }
  }
  if (bestArea >= 0) {
    trace->fraction = best; trace->endPos = *start + (*end - *start) * best; trace->areaNum = bestArea; return true;
  }
  return false;
}
int idAAS2File::GetObstaclePVSWallEdges(int areaNum, int edgeFlags, int* output, int maxEdges) const {
  if (areaNum < 0 || static_cast<std::size_t>(areaNum) >= areas.size()) return 0;
  int count = 0;
  for (int encoded : areas[areaNum].edges) {
    const int edgeNum = std::abs(encoded);
    if (static_cast<std::size_t>(edgeNum) < edges.size() && (edges[edgeNum].flags & edgeFlags) == edgeFlags) {
      if (output && count < maxEdges) output[count] = encoded;
      ++count;
    }
  }
  return count;
}
void idAAS2File::ClipGridToAreas(const idVec3* startOrigin, int, int travelFlags,
                                 int cellSize, unsigned dimension, std::uint8_t* reachable) const {
  if (!startOrigin || !reachable || cellSize <= 0) return;
  for (unsigned y = 0; y < dimension; ++y) for (unsigned x = 0; x < dimension; ++x) {
    idVec3 point = *startOrigin + idVec3{static_cast<float>(x * cellSize), static_cast<float>(y * cellSize), 0.0f};
    const int area = PointAreaNum(-1, &point);
    reachable[y * dimension + x] = area >= 0 && (areas[area].travelFlags & travelFlags) == travelFlags ? 1 : 0;
  }
}
float idAAS2File::GetFloorDistance(int areaNum, const idPlane* floorPlane, const idVec3* point,
                                   float bboxHeight, float maxEdgeDist) const {
  if (!floorPlane || !point || areaNum < 0 || static_cast<std::size_t>(areaNum) >= areas.size()) return maxEdgeDist;
  const float normalLength = std::sqrt(floorPlane->a * floorPlane->a + floorPlane->b * floorPlane->b + floorPlane->c * floorPlane->c);
  if (normalLength <= 1.0e-8f) return maxEdgeDist;
  return std::clamp(std::fabs(PlaneDistance(*floorPlane, *point)) / normalLength - bboxHeight, 0.0f, maxEdgeDist);
}
void idAAS2File::PointBestReachableAreaNum(int tree, const idVec3* origin,
                                           bestReachableArea_t* bestArea) const {
  if (!origin || !bestArea) return;
  *bestArea = {};
  for (std::size_t i = 0; i < areas.size(); ++i) {
    if ((tree >= 0 && areas[i].treeNum != tree) || !(areas[i].flags & AAS2_AREA_REACHABLE)) continue;
    const float distance = (areas[i].center - *origin).LengthSqr();
    if (distance < bestArea->distance) { bestArea->distance = distance; bestArea->areaNum = static_cast<int>(i); bestArea->point = areas[i].center; }
  }
}
void idAAS2File::BoundsBestReachableAreaNum(int tree, const idBounds* bounds,
                                            const idVec3* origin, bestReachableArea_t* bestArea) const {
  if (!bounds || !origin || !bestArea) return;
  *bestArea = {};
  for (std::size_t i = 0; i < areas.size(); ++i) {
    if ((tree >= 0 && areas[i].treeNum != tree) || !BoundsIntersect(*bounds, areas[i].bounds) || !(areas[i].flags & AAS2_AREA_REACHABLE)) continue;
    const float distance = (areas[i].center - *origin).LengthSqr();
    if (distance < bestArea->distance) { bestArea->distance = distance; bestArea->areaNum = static_cast<int>(i); bestArea->point = areas[i].center; }
  }
}
int idAAS2File::PointReachableAreaNum(int tree, const idVec3* origin, int areaFlags,
                                      float excludeTravelFlags) const {
  const int pointArea = PointAreaNum(tree, origin);
  if (pointArea >= 0 && (areas[pointArea].flags & areaFlags) == areaFlags &&
      !(areas[pointArea].travelFlags & static_cast<int>(excludeTravelFlags))) return pointArea;
  bestReachableArea_t best; PointBestReachableAreaNum(tree, origin, &best); return best.areaNum;
}
int idAAS2File::PushPointIntoAreaNum(int areaNum, idVec3* point) const {
  if (!point || areaNum < 0 || static_cast<std::size_t>(areaNum) >= areas.size()) return -1;
  for (int axis = 0; axis < 3; ++axis) (*point)[axis] = std::clamp((*point)[axis], areas[areaNum].bounds[0][axis], areas[areaNum].bounds[1][axis]);
  return areaNum;
}
bool idAAS2File::PushPointIntoNearestAreaNum(int* areaNum, idVec3* point) const {
  if (!areaNum || !point) return false;
  if (*areaNum >= 0 && static_cast<std::size_t>(*areaNum) < areas.size()) return PushPointIntoAreaNum(*areaNum, point) >= 0;
  bestReachableArea_t best; PointBestReachableAreaNum(-1, point, &best);
  if (best.areaNum < 0) return false; *areaNum = best.areaNum; return PushPointIntoAreaNum(*areaNum, point) >= 0;
}
void idAAS2File::FloodClearVisitedAreas(int) { for (auto& area : areas) area.flags &= ~AAS2_AREA_VISITED; }
bool idAAS2File::TraceFloor(aas2TraceFloor_t* trace, const idVec3* start, int startAreaNum,
                            const idVec3* end, unsigned endAreaNum, int travelFlags,
                            bool allowFloorNormalChange, bool, bool ignoreSameArea) const {
  if (!trace || !start || !end) return false;
  aas2Trace_t base; const int tree = GetTreeForArea(startAreaNum);
  const bool hit = Trace(tree, &base, start, end); static_cast<aas2Trace_t&>(*trace) = base;
  if (base.areaNum >= 0 && static_cast<std::size_t>(base.areaNum) < areas.size() &&
      (areas[base.areaNum].travelFlags & travelFlags) != travelFlags) return false;
  if (base.areaNum >= 0) {
    const int areaTree = GetTreeForArea(base.areaNum);
    if (areaTree >= 0 && static_cast<std::size_t>(areaTree) < trees.size()) trace->floorNormal = trees[areaTree].floorNormal;
  }
  if (!allowFloorNormalChange && tree >= 0 && static_cast<std::size_t>(tree) < trees.size()) trace->floorNormal = trees[tree].floorNormal;
  return hit && (!ignoreSameArea || base.areaNum != startAreaNum) && (endAreaNum == static_cast<unsigned>(-1) || base.areaNum == static_cast<int>(endAreaNum));
}

