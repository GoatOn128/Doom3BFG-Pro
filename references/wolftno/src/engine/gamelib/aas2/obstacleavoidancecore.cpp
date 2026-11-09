#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#include "engine/gamelib/aas2/obstacleavoidance_abi.h"

#include "shared/idlib/math/mathlib.h"

#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <xmmintrin.h>

namespace {

constexpr int kBoxPlaneBitsSilVerts[64][7] = {
    {0}, {4, 7, 4, 0, 3}, {4, 5, 6, 2, 1}, {0},
    {4, 4, 5, 1, 0}, {6, 3, 7, 4, 5, 1, 0},
    {6, 4, 5, 6, 2, 1, 0}, {0},
    {4, 6, 7, 3, 2}, {6, 6, 7, 4, 0, 3, 2},
    {6, 5, 6, 7, 3, 2, 1}, {0}, {0}, {0}, {0}, {0},
    {4, 0, 1, 2, 3}, {6, 0, 1, 2, 3, 7, 4},
    {6, 3, 2, 6, 5, 1, 0}, {0},
    {6, 1, 2, 3, 0, 4, 5}, {6, 1, 2, 3, 7, 4, 5},
    {6, 2, 3, 0, 4, 5, 6}, {0},
    {6, 0, 1, 2, 6, 7, 3}, {6, 0, 1, 2, 6, 7, 4},
    {6, 0, 1, 5, 6, 7, 3}, {0}, {0}, {0}, {0}, {0},
    {4, 7, 6, 5, 4}, {6, 7, 6, 5, 4, 0, 3},
    {6, 5, 4, 7, 6, 2, 1}, {0},
    {6, 4, 7, 6, 5, 1, 0}, {6, 3, 7, 6, 5, 1, 0},
    {6, 4, 7, 6, 2, 1, 0}, {0},
    {6, 6, 5, 4, 7, 3, 2}, {6, 6, 5, 4, 0, 3, 2},
    {6, 5, 4, 7, 3, 2, 1}, {0}, {0}, {0}, {0}, {0},
    {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
    {0}, {0}, {0}, {0}, {0}, {0}, {0}, {0},
};

__forceinline float InverseLength(float x, float y) {
  const float lengthSquared = x * x + y * y;
  return 1.0f /
         _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
             _mm_set_ss(lengthSquared),
             idMath::vector_float_smallestNonDenorm)));
}

__forceinline float InverseLength(float x, float y, float z) {
  const float lengthSquared = x * x + y * y + z * z;
  return 1.0f /
         _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
             _mm_set_ss(lengthSquared),
             idMath::vector_float_smallestNonDenorm)));
}

__forceinline float Length(float x, float y) {
  return _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
      _mm_set_ss(x * x + y * y), idMath::vector_float_smallestNonDenorm)));
}

__forceinline float LineSide(const idVec2& point, const idVec2& start,
                             const idVec2& end) {
  const float dx = end.x - start.x;
  const float dy = start.y - end.y;
  return (dx * point.y + dy * point.x) -
         (start.x * dy + start.y * dx);
}

__forceinline bool OppositeSign(float first, float second) {
  return (std::bit_cast<std::uint32_t>(first) ^
          std::bit_cast<std::uint32_t>(second)) >= 0x80000000u;
}

__forceinline idVec2 EdgeNormal(const obstacleEdge_t& edge,
                                const obstacleVertex_t* vertices) {
  const idVec3& first = vertices[edge.verts[0]].position;
  const idVec3& second = vertices[edge.verts[1]].position;
  const float inverseLength =
      InverseLength(first.x - second.x, second.y - first.y);
  return {(first.x - second.x) * inverseLength,
          (second.y - first.y) * inverseLength};
}

__forceinline idVec2 JointBevel(const idVec2& first,
                                const idVec2& second) {
  const float dot = first.x * second.x + first.y * second.y;
  if (dot <= -0.70710677f) {
    return {first.x + second.x, first.y + second.y};
  }
  const float scale = 1.0f / (dot + 1.0f);
  return {(first.x + second.x) * scale,
          (first.y + second.y) * scale};
}

void FixDegenerateNormalExact(idVec3& normal) {
  const float absX = std::fabs(normal.x);
  const float absY = std::fabs(normal.y);
  if (absX >= idMath::FLT_SMALLEST_NON_DENORMAL) {
    if (absY < idMath::FLT_SMALLEST_NON_DENORMAL &&
        std::fabs(normal.z) < idMath::FLT_SMALLEST_NON_DENORMAL) {
      normal = {normal.x <= 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f};
      return;
    }
  } else {
    if (absY < idMath::FLT_SMALLEST_NON_DENORMAL) {
      normal = {0.0f, 0.0f, normal.z <= 0.0f ? -1.0f : 1.0f};
      return;
    }
    if (std::fabs(normal.z) < idMath::FLT_SMALLEST_NON_DENORMAL) {
      normal = {0.0f, normal.y <= 0.0f ? -1.0f : 1.0f, 0.0f};
      return;
    }
  }

  constexpr float almostOne = 1.0f - 1.192092896e-7f;
  if (absX > almostOne) {
    normal = {normal.x <= 0.0f ? -1.0f : 1.0f, 0.0f, 0.0f};
  } else if (absY > almostOne) {
    normal = {0.0f, normal.y <= 0.0f ? -1.0f : 1.0f, 0.0f};
  } else if (std::fabs(normal.z) > almostOne) {
    normal = {0.0f, 0.0f, normal.z <= 0.0f ? -1.0f : 1.0f};
  }
}

} // namespace

idObstacleAvoidanceCore::idObstacleAvoidanceCore()
    : bspNodes(nullptr),
      numBspNodes(0),
      obstacleIndices(nullptr),
      expandedObstacles(nullptr),
      maxExpandedObstacles(0),
      numExpandedObstacles(0),
      windingPoints(nullptr),
      maxWindingPoints(0),
      numWindingPoints(0),
      tempMemory(nullptr),
      tempMemorySize(0),
      tempMemoryStart(nullptr),
      tempMemoryEnd(nullptr),
      pathNodes(nullptr),
      maxPathNodes(0),
      numPathNodes(0),
      debugLines(nullptr),
      maxDebugLines(0),
      numDebugLines(0),
      debugText(nullptr),
      maxDebugText(0),
      numDebugText(0),
      inputTransformValid(false) {
  inputTransform.Identity();
}

idObstacleAvoidanceCore::~idObstacleAvoidanceCore() {
  bspNodes = nullptr;
  debugLines = nullptr;
  debugText = nullptr;
  expandedObstacles = nullptr;
  obstacleIndices = nullptr;
  pathNodes = nullptr;
  tempMemory = nullptr;
  tempMemoryEnd = nullptr;
  tempMemoryStart = nullptr;
  windingPoints = nullptr;
}

int idObstacleAvoidanceCore::BoxParallelProjectionSilhouetteVerts(
    const idMat3& axis, const idVec3& center, const idVec3& extents,
    const idVec3& projectionDir, idVec3* silVerts) {
  const idVec3 x = axis.mat[0] * extents.x;
  const idVec3 y = axis.mat[1] * extents.y;
  const idVec3 z = axis.mat[2] * extents.z;
  const float centerMinusXx = center.x - x.x;
  const float centerPlusXx = center.x + x.x;
  const float centerMinusXy = center.y - x.y;
  const float centerPlusXy = center.y + x.y;
  const float centerMinusXz = center.z - x.z;
  const float centerPlusXz = center.z + x.z;
  const float yzDifferenceX = y.x - z.x;
  const float yzDifferenceY = y.y - z.y;
  const float yzDifferenceZ = y.z - z.z;
  const float yzSumX = z.x + y.x;
  const float yzSumY = z.y + y.y;
  const float yzSumZ = z.z + y.z;
  const idVec3 points[8] = {
      {centerMinusXx - yzSumX, centerMinusXy - yzSumY,
       centerMinusXz - yzSumZ},
      {centerPlusXx - yzSumX, centerPlusXy - yzSumY,
       centerPlusXz - yzSumZ},
      {yzDifferenceX + centerPlusXx,
       yzDifferenceY + centerPlusXy,
       yzDifferenceZ + centerPlusXz},
      {yzDifferenceX + centerMinusXx,
       yzDifferenceY + centerMinusXy,
       yzDifferenceZ + centerMinusXz},
      {centerMinusXx - yzDifferenceX,
       centerMinusXy - yzDifferenceY,
       centerMinusXz - yzDifferenceZ},
      {centerPlusXx - yzDifferenceX,
       centerPlusXy - yzDifferenceY,
       centerPlusXz - yzDifferenceZ},
      {yzSumX + centerPlusXx, yzSumY + centerPlusXy,
       yzSumZ + centerPlusXz},
      {yzSumX + centerMinusXx, yzSumY + centerMinusXy,
       yzSumZ + centerMinusXz},
  };
  int planeBits = 0;
  const float axisDots[3] = {projectionDir * axis.mat[0],
                             projectionDir * axis.mat[1],
                             projectionDir * axis.mat[2]};
  for (int index = 0; index < 3; ++index) {
    if (std::fabs(axisDots[index]) != 0.0f) {
      planeBits |= (1 << (index * 2))
                   << static_cast<int>(axisDots[index] < 0.0f);
    }
  }
  const int* const indices = kBoxPlaneBitsSilVerts[planeBits];
  for (int index = 0; index < indices[0]; ++index) {
    silVerts[index] = points[indices[index + 1]];
  }
  return indices[0];
}

void idObstacleAvoidanceCore::GetCirclePoints(const idVec2& center,
                                               float radius,
                                               idVec2* circlePoints) {
  constexpr idVec2 directions[16] = {
      {0.0f, 1.0f}, {0.38268343f, 0.9238795f},
      {0.70710677f, 0.70710677f}, {0.9238795f, 0.38268343f},
      {1.0f, 0.0f}, {0.9238795f, -0.38268343f},
      {0.70710677f, -0.70710677f}, {0.38268343f, -0.9238795f},
      {0.0f, -1.0f}, {-0.38268343f, -0.9238795f},
      {-0.70710677f, -0.70710677f}, {-0.9238795f, -0.38268343f},
      {-1.0f, 0.0f}, {-0.9238795f, 0.38268343f},
      {-0.70710677f, 0.70710677f}, {-0.38268343f, 0.9238795f},
  };
  for (int index = 0; index < 16; ++index) {
    circlePoints[index] = {directions[index].x * radius + center.x,
                           directions[index].y * radius + center.y};
  }
}

bool idObstacleAvoidanceCore::Plane2DIntersection(const idVec3& plane1,
                                                   const idVec3& plane2,
                                                   idVec2& point) {
  const float plane1Length =
      plane1.x * plane1.x + plane1.y * plane1.y;
  const float plane2Length =
      plane2.x * plane2.x + plane2.y * plane2.y;
  const float dot = plane2.x * plane1.x + plane2.y * plane1.y;
  const float determinant = plane2Length * plane1Length - dot * dot;
  if (std::fabs(determinant) < 0.000001f) {
    return false;
  }
  const float inverseDeterminant = 1.0f / determinant;
  const float scale2 =
      (plane1.z * dot - plane2.z * plane1Length) * inverseDeterminant;
  const float scale1 =
      (plane2.z * dot - plane1.z * plane2Length) * inverseDeterminant;
  point = {plane2.x * scale2 + plane1.x * scale1,
           scale2 * plane2.y + scale1 * plane1.y};
  return true;
}

bool idObstacleAvoidanceCore::PointInsideWinding(const idVec2* winding,
                                                  int numPoints,
                                                  const idVec2& point,
                                                  float epsilon) {
  for (int index = 0; index < numPoints; ++index) {
    const idVec2& current = winding[index];
    const idVec2& next = winding[(index + 1) % numPoints];
    const float normalX = current.y - next.y;
    const float normalY = next.x - current.x;
    const float inverseLength = InverseLength(normalX, normalY);
    const float x = normalX * inverseLength;
    const float y = normalY * inverseLength;
    if ((point.x * x + point.y * y) -
            (current.y * y + current.x * x) >
        epsilon) {
      return false;
    }
  }
  return true;
}

void idObstacleAvoidanceCore::ExpandWinding(const idVec2* winding,
                                             int numPoints, float d,
                                             idVec2* expanded) {
  idVec2 edgeNormals[16];
  for (int index = 0; index < numPoints; ++index) {
    const idVec2& current = winding[index];
    const idVec2& next = winding[(index + 1) % numPoints];
    const float normalX = current.y - next.y;
    const float normalY = next.x - current.x;
    const float inverseLength = InverseLength(normalX, normalY);
    edgeNormals[index] = {d * (normalX * inverseLength),
                          d * (normalY * inverseLength)};
  }
  for (int index = 0; index < numPoints; ++index) {
    const idVec2& previousNormal =
        edgeNormals[(numPoints + index - 1) % numPoints];
    expanded[index] = {winding[index].x + edgeNormals[index].x +
                           previousNormal.x,
                       winding[index].y + edgeNormals[index].y +
                           previousNormal.y};
  }
}

int idObstacleAvoidanceCore::ExpandWindingForAxialBox(
    idVec2* winding, int numPoints, const idVec2* bounds) {
  idVec3 planes[16];
  int numPlanes = 0;
  for (int index = 0; index < numPoints; ++index) {
    const idVec2 point = winding[index];
    const idVec2& next = winding[(index + 1) % numPoints];
    const float dx = next.x - point.x;
    const float dy = next.y - point.y;
    if (dx * dx + dy * dy < 0.01f) {
      continue;
    }
    const float inverseLength = InverseLength(dx, point.y - next.y);
    const idVec3 plane{(point.y - next.y) * inverseLength,
                       dx * inverseLength, 0.0f};
    if (numPlanes > 0) {
      const idVec3& previous = planes[numPlanes - 1];
      idVec3 bevel{};
      bool addBevel = false;
      if (OppositeSign(previous.x, plane.x) &&
          std::fabs(previous.x) > 0.1f && std::fabs(plane.x) > 0.1f) {
        bevel.y = previous.y >= 0.0f ? 1.0f : -1.0f;
        addBevel = true;
      } else if (OppositeSign(previous.y, plane.y) &&
                 std::fabs(previous.y) > 0.1f &&
                 std::fabs(plane.y) > 0.1f) {
        bevel.x = previous.x >= 0.0f ? 1.0f : -1.0f;
        addBevel = true;
      }
      if (addBevel) {
        bevel.z = -(point.x * bevel.x + point.y * bevel.y);
        planes[numPlanes++] = bevel;
      }
    }
    planes[numPlanes] = plane;
    planes[numPlanes].z = -(point.x * plane.x + point.y * plane.y);
    ++numPlanes;
  }
  if (numPlanes <= 0) {
    return 0;
  }

  const idVec2 point = winding[0];
  const idVec3& previous = planes[numPlanes - 1];
  const idVec3& first = planes[0];
  idVec3 bevel{};
  bool addBevel = false;
  if (OppositeSign(previous.x, first.x) &&
      std::fabs(previous.x) > 0.1f && std::fabs(first.x) > 0.1f) {
    bevel.y = previous.y >= 0.0f ? 1.0f : -1.0f;
    addBevel = true;
  } else if (OppositeSign(previous.y, first.y) &&
             std::fabs(previous.y) > 0.1f && std::fabs(first.y) > 0.1f) {
    bevel.x = previous.x >= 0.0f ? 1.0f : -1.0f;
    addBevel = true;
  }
  if (addBevel) {
    bevel.z = -(point.x * bevel.x + point.y * bevel.y);
    planes[numPlanes++] = bevel;
  }

  for (int index = 0; index < numPlanes; ++index) {
    const int xIndex = std::signbit(planes[index].x) ? 1 : 0;
    const int yIndex = std::signbit(planes[index].y) ? 1 : 0;
    planes[index].z += bounds[xIndex].x * planes[index].x +
                       bounds[yIndex].y * planes[index].y;
  }
  int expandedPoints = 0;
  for (int index = 0; index < numPlanes; ++index) {
    if (Plane2DIntersection(
            planes[(numPlanes + index - 1) % numPlanes], planes[index],
            winding[expandedPoints])) {
      ++expandedPoints;
    }
  }
  return expandedPoints;
}

void idObstacleAvoidanceCore::GetWindingBounds(const idVec2* winding,
                                                int numPoints,
                                                idVec2* bounds) {
  if (numPoints == 0) {
    bounds[0] = {1.0e30f, 1.0e30f};
    bounds[1] = {-1.0e30f, -1.0e30f};
    return;
  }
  bounds[0] = winding[0];
  bounds[1] = winding[0];
  for (int index = 1; index < numPoints; ++index) {
    bounds[0].x = std::min(bounds[0].x, winding[index].x);
    bounds[0].y = std::min(bounds[0].y, winding[index].y);
    bounds[1].x = std::max(bounds[1].x, winding[index].x);
    bounds[1].y = std::max(bounds[1].y, winding[index].y);
  }
}

bool idObstacleAvoidanceCore::LineIntersectsPath(
    const idVec2& start, const idVec2& end, const pathNode_t* node) const {
  const pathNode_t* parent = node->parent;
  std::uint32_t nodeSide =
      std::bit_cast<std::uint32_t>(LineSide(node->pos, start, end)) >> 31;
  while (parent != nullptr) {
    const std::uint32_t parentSide =
        std::bit_cast<std::uint32_t>(LineSide(parent->pos, start, end)) >> 31;
    if (nodeSide != parentSide &&
        OppositeSign(LineSide(start, node->pos, parent->pos),
                     LineSide(end, node->pos, parent->pos))) {
      return true;
    }
    node = parent;
    parent = parent->parent;
    nodeSide = parentSide;
  }
  return false;
}

int idObstacleAvoidanceCore::PointInsideObstacle(const idVec2& point) const {
  if (expandedObstacles == nullptr) {
    return -1;
  }
  for (int index = 0; index < numExpandedObstacles; ++index) {
    const expandedObstacle_t& obstacle = expandedObstacles[index];
    if (obstacle.bounds[0].x <= point.x &&
        obstacle.bounds[0].y <= point.y && point.x <= obstacle.bounds[1].x &&
        point.y <= obstacle.bounds[1].y &&
        PointInsideWinding(obstacle.winding, obstacle.numPoints, point,
                           0.1f)) {
      return index;
    }
  }
  return -1;
}

int idObstacleAvoidanceCore::PointInsideObstacleList(
    const int* obstacleList, int numObstacles, const idVec2& point) const {
  if (expandedObstacles == nullptr) {
    return -1;
  }
  for (int index = 0; index < numObstacles; ++index) {
    const int obstacleIndex = obstacleList[index];
    const expandedObstacle_t& obstacle = expandedObstacles[obstacleIndex];
    if (obstacle.bounds[0].x <= point.x &&
        obstacle.bounds[0].y <= point.y && point.x <= obstacle.bounds[1].x &&
        point.y <= obstacle.bounds[1].y &&
        PointInsideWinding(obstacle.winding, obstacle.numPoints, point,
                           0.1f)) {
      return obstacleIndex;
    }
  }
  return -1;
}

int idObstacleAvoidanceCore::GetBspNodesForLine_r(
    const idVec2& start, const idVec2& end, int nodeNum, int currentNumNodes,
    int* nodes) const {
  int count = currentNumNodes;
  while (count < 31) {
    nodes[count++] = nodeNum;
    const bspNode_t& node = bspNodes[nodeNum];
    if (node.axis < 0) {
      break;
    }
    const float startDist = start[node.axis] - node.dist;
    const float endDist = end[node.axis] - node.dist;
    if (startDist >= 0.1f && endDist >= 0.1f) {
      nodeNum = node.children[0];
      continue;
    }
    if (startDist < -0.1f && endDist < -0.1f) {
      nodeNum = node.children[1];
      continue;
    }
    const float difference = std::fabs(startDist - endDist);
    if (idMath::FLT_SMALLEST_NON_DENORMAL < difference) {
      const bool endGreater = endDist > startDist;
      const float fraction = startDist / (startDist - endDist);
      const idVec2 middle{start.x + (end.x - start.x) * fraction,
                          start.y + (end.y - start.y) * fraction};
      count = GetBspNodesForLine_r(start, middle,
                                   node.children[endGreater], count, nodes);
      return GetBspNodesForLine_r(middle, end,
                                  node.children[!endGreater], count, nodes);
    }
    count = GetBspNodesForLine_r(start, end, node.children[0], count, nodes);
    return GetBspNodesForLine_r(start, end, node.children[1], count, nodes);
  }
  return count;
}

std::int16_t idObstacleAvoidanceCore::SetupBspTree_r(
    const idVec2& center, const idVec2& span, int axis, int depth) {
  if (bspNodes == nullptr) {
    return 0;
  }
  const std::int16_t nodeNum = static_cast<std::int16_t>(numBspNodes);
  bspNode_t& node = bspNodes[numBspNodes++];
  node.indexOffset = 0;
  node.numIndices = 0;
  if (depth < 4) {
    node.axis = axis;
    node.dist = center[axis];
    const int nextAxis = axis ^ 1;
    idVec2 newSpan{span.x * 0.5f, span.y * 0.5f};
    newSpan[nextAxis] = span[nextAxis];
    idVec2 center0{center.x + span.x * 0.5f,
                   center.y + span.y * 0.5f};
    idVec2 center1{center.x - span.x * 0.5f,
                   center.y - span.y * 0.5f};
    center0[nextAxis] = center[nextAxis];
    center1[nextAxis] = center[nextAxis];
    node.children[0] =
        SetupBspTree_r(center0, newSpan, nextAxis, depth + 1);
    node.children[1] =
        SetupBspTree_r(center1, newSpan, nextAxis, depth + 1);
  } else {
    node.axis = -1;
    node.dist = 0.0f;
    node.children[0] = -1;
    node.children[1] = -1;
  }
  return nodeNum;
}

void idObstacleAvoidanceCore::FilterObstacleIntoBspTree(
    std::int16_t nodeNum, int obstacleNum,
    std::int16_t* obstacleNodes) {
  if (bspNodes == nullptr || expandedObstacles == nullptr) {
    return;
  }
  while (true) {
    bspNode_t& node = bspNodes[nodeNum];
    if (node.axis < 0) {
      obstacleNodes[obstacleNum] = nodeNum;
      ++node.numIndices;
      return;
    }
    const expandedObstacle_t& obstacle = expandedObstacles[obstacleNum];
    if (obstacle.bounds[0][node.axis] <= node.dist) {
      if (node.dist <= obstacle.bounds[1][node.axis]) {
        obstacleNodes[obstacleNum] = nodeNum;
        ++node.numIndices;
        return;
      }
      nodeNum = node.children[1];
    } else {
      nodeNum = node.children[0];
    }
  }
}

void idObstacleAvoidanceCore::SetupWallOrLedgeWinding(
    expandedObstacle_t& obstacle, int lastEdgeIndex, int currentEdgeIndex,
    int nextEdgeIndex, const obstacleEdge_t* edges,
    const obstacleVertex_t* vertices) {
  const obstacleEdge_t& currentEdge = edges[currentEdgeIndex];
  const int currentType = (currentEdge.flags & 2) != 0 ? -3 : -2;
  const idVec2 currentNormal = EdgeNormal(currentEdge, vertices);
  const idVec2& first =
      *reinterpret_cast<const idVec2*>(&vertices[currentEdge.verts[0]].position);
  const idVec2& second =
      *reinterpret_cast<const idVec2*>(&vertices[currentEdge.verts[1]].position);
  obstacle.id = currentType;
  obstacle.numPoints = 4;
  obstacle.winding = &windingPoints[numWindingPoints];
  numWindingPoints += 4;
  idVec2* const winding = obstacle.winding;

  if (lastEdgeIndex == -1) {
    winding[1] = {first.x - currentNormal.x,
                  first.y + currentNormal.y};
    winding[2] = {first.x + currentNormal.x - currentNormal.y * 8.0f,
                  first.y - currentNormal.y - currentNormal.x * 8.0f};
  } else {
    const obstacleEdge_t& previousEdge = edges[lastEdgeIndex];
    const int previousType =
        (previousEdge.flags & 2) != 0 ? -3 : -2;
    const idVec2 previousNormal = EdgeNormal(previousEdge, vertices);
    const idVec2 bevel = JointBevel(previousNormal, currentNormal);
    const idVec2& point = *reinterpret_cast<const idVec2*>(
        &vertices[previousEdge.verts[1]].position);
    if (currentType == -2) {
      winding[1] = point;
      if (previousType == -2) {
        winding[2] = {point.x + currentNormal.x - bevel.y * 8.0f,
                      point.y - currentNormal.y - bevel.x * 8.0f};
      } else {
        winding[2] = {
            point.x - currentNormal.x - currentNormal.y * 8.0f,
            point.y - currentNormal.y - currentNormal.x * 8.0f};
      }
    } else if (previousType == -3) {
      winding[1] = {point.x + bevel.x * 12.0f,
                    point.y + bevel.y * 12.0f};
      winding[2] = {point.x + currentNormal.x + bevel.y,
                    point.y - currentNormal.y + bevel.x};
    } else {
      winding[1] = {point.x + currentNormal.y * 12.0f,
                    point.y + currentNormal.x * 12.0f};
      winding[2] = {point.x + previousNormal.x - previousNormal.y,
                    point.y - previousNormal.y - previousNormal.x};
    }
  }

  if (nextEdgeIndex == -1) {
    winding[0] = {second.x - currentNormal.x,
                  second.y + currentNormal.y};
    winding[3] = {
        second.x - currentNormal.x - currentNormal.y * 8.0f,
        second.y + currentNormal.y - currentNormal.x * 8.0f};
  } else {
    const obstacleEdge_t& nextEdge = edges[nextEdgeIndex];
    const int nextType = (nextEdge.flags & 2) != 0 ? -3 : -2;
    const idVec2 nextNormal = EdgeNormal(nextEdge, vertices);
    const idVec2 bevel = JointBevel(nextNormal, currentNormal);
    const idVec2& point = *reinterpret_cast<const idVec2*>(
        &vertices[currentEdge.verts[1]].position);
    if (currentType != -2) {
      if (nextType == -2) {
        winding[0] = {point.x + currentNormal.y * 12.0f,
                      point.y + currentNormal.x * 12.0f};
        winding[3] = {point.x - nextNormal.x - nextNormal.y,
                      point.y + nextNormal.y - nextNormal.x};
      } else {
        winding[0] = {point.x + bevel.y * 12.0f,
                      point.y + bevel.x * 12.0f};
        winding[3] = {point.x - currentNormal.x + bevel.y,
                      point.y + currentNormal.y + bevel.x};
      }
    } else {
      winding[0] = point;
      if (nextType == -2) {
        winding[3] = {point.x - currentNormal.x - bevel.y * 8.0f,
                      point.y + currentNormal.y - bevel.x * 8.0f};
      } else {
        winding[3] = {
            point.x + currentNormal.x - currentNormal.y * 8.0f,
            point.y - currentNormal.y - currentNormal.x * 8.0f};
      }
    }
  }
  GetWindingBounds(winding, 4, obstacle.bounds);
}

void idObstacleAvoidanceCore::PrunePathTree(pathNode_t* root,
                                             const idVec2& seekPos) {
  if (root == nullptr) {
    return;
  }
  for (;;) {
    while (root != nullptr) {
      const float dx = seekPos.x - root->pos.x;
      const float dy = seekPos.y - root->pos.y;
      root->targetDistSqr = dx * dx + dy * dy;
      if (root->children[0] == nullptr) {
        break;
      }
      root = root->children[0];
    }
    if (root == nullptr) {
      return;
    }
    if (root->children[1] != nullptr) {
      root = root->children[1];
      continue;
    }
    float closestDistance = 1.0e30f;
    for (pathNode_t* node = root; node != nullptr; node = node->parent) {
      if (node->children[0] != nullptr && node->children[1] != nullptr) {
        break;
      }
      if (closestDistance > node->targetDistSqr) {
        closestDistance = node->targetDistSqr;
        root = node;
      }
    }
    root->children[0] = nullptr;
    root->children[1] = nullptr;
    pathNode_t* parent = root->parent;
    for (; parent != nullptr; parent = parent->parent) {
      if (parent->children[1] != nullptr && parent->children[1] != root) {
        root = parent->children[1];
        break;
      }
      root = parent;
    }
    if (parent == nullptr) {
      return;
    }
  }
}

void idObstacleAvoidanceCore::GetSeekPointsOnPath(
    const idVec2* path, int numPathPoints, const idVec2& corner,
    float maxSeekDistance, float nextSeekDistance, idVec3* seekPos,
    int& numSeekPos, bool skipFirstPoint) {
  int generated = 0;
  int outputIndex = 0;
  float remaining = nextSeekDistance;
  for (int segment = 0; segment < numPathPoints - 1; ++segment) {
    if (outputIndex >= 2) {
      if (outputIndex < 30) {
        seekPos[outputIndex].x = path[segment].x;
        seekPos[outputIndex].y = path[segment].y;
        ++generated;
        ++outputIndex;
      }
      continue;
    }
    const float dx = path[segment + 1].x - path[segment].x;
    const float dy = path[segment + 1].y - path[segment].y;
    const float segmentLength = Length(dx, dy);
    remaining -= segmentLength;
    if (remaining < 0.0f) {
      do {
        const float fraction = (segmentLength + remaining) / segmentLength;
        idVec2 point{path[segment].x + fraction * dx,
                     path[segment].y + fraction * dy};
        if (segment > 0 && segment < numPathPoints - 2 &&
            maxSeekDistance != 0.0f) {
          const float fromCornerX = point.x - corner.x;
          const float fromCornerY = point.y - corner.y;
          const float inverseLength =
              InverseLength(fromCornerX, fromCornerY);
          const float radius = std::fabs(maxSeekDistance);
          point = {corner.x + fromCornerX * inverseLength * radius,
                   corner.y + fromCornerY * inverseLength * radius};
        }
        seekPos[outputIndex].x = point.x;
        seekPos[outputIndex].y = point.y;
        ++generated;
        ++outputIndex;
        if (outputIndex >= 2) {
          break;
        }
        remaining += nextSeekDistance;
      } while (remaining < 0.0f);
    }
  }
  if (skipFirstPoint) {
    numSeekPos = generated;
  } else {
    numSeekPos = std::max(numSeekPos, generated);
  }
}

void idObstacleAvoidanceCore::TransformInput(
    const idVec3& gravity, obstacleRoute_t* route, idVec3& lastDir,
    idVec4& lastCorner, obstacleBox_t* obstacles, int numObstacles,
    obstacleVertex_t* vertices, int numVertices, obstacleCorner_t* corners,
    int numCorners, obstacleTrace_t* traces, int numTraces) {
  const idVec3 origin = route->areas[0].start;
  idVec3 up{-gravity.x, -gravity.y, -gravity.z};
  const float upInvLength = InverseLength(up.x, up.y, up.z);
  up.x *= upInvLength;
  up.y *= upInvLength;
  up.z *= upInvLength;

  if (up.x * up.x + up.y * up.y == 0.0f) {
    inputTransformValid = false;
    return;
  }

  const float horizontalInvLength = InverseLength(up.x, up.y);
  const idVec3 right{-horizontalInvLength * up.y,
                     horizontalInvLength * up.x, 0.0f};
  const idVec3 forward{-(up.z * (horizontalInvLength * up.x)),
                       up.z * right.x,
                       horizontalInvLength * up.x * up.x - right.x * up.y};
  inputTransform.mat[0] = right.x;
  inputTransform.mat[1] = right.y;
  inputTransform.mat[2] = right.z;
  inputTransform.mat[3] = -(origin.x * right.x + origin.y * right.y +
                            origin.z * right.z);
  inputTransform.mat[4] = forward.x;
  inputTransform.mat[5] = forward.y;
  inputTransform.mat[6] = forward.z;
  inputTransform.mat[7] = -(origin.x * forward.x + origin.y * forward.y +
                            origin.z * forward.z);
  inputTransform.mat[8] = up.x;
  inputTransform.mat[9] = up.y;
  inputTransform.mat[10] = up.z;
  inputTransform.mat[11] =
      -(origin.x * up.x + origin.y * up.y + origin.z * up.z);
  inputTransformValid = true;

  const auto transformPoint = [this](idVec3& point) {
    const idVec3 input = point;
    inputTransform.Transform(point, input);
  };
  const auto transformVector = [this](idVec3& vector) {
    const idVec3 input = vector;
    vector = {
        inputTransform.mat[0] * input.x +
            inputTransform.mat[1] * input.y +
            inputTransform.mat[2] * input.z,
        inputTransform.mat[4] * input.x +
            inputTransform.mat[5] * input.y +
            inputTransform.mat[6] * input.z,
        inputTransform.mat[8] * input.x +
            inputTransform.mat[9] * input.y +
            inputTransform.mat[10] * input.z,
    };
  };

  transformVector(lastDir);
  idVec3 cornerPoint{lastCorner.x, lastCorner.y, lastCorner.z};
  transformPoint(cornerPoint);
  lastCorner.x = cornerPoint.x;
  lastCorner.y = cornerPoint.y;
  lastCorner.z = cornerPoint.z;

  for (int index = 0; index < route->numAreas; ++index) {
    transformPoint(route->areas[index].start);
    transformPoint(route->areas[index].end);
  }
  transformVector(route->endAlignDir);

  for (int index = 0; index < numObstacles; ++index) {
    transformPoint(obstacles[index].center);
    for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
      transformVector(obstacles[index].axis.mat[axisIndex]);
    }
  }
  for (int index = 0; index < numVertices; ++index) {
    transformPoint(vertices[index].position);
  }
  for (int index = 0; index < numCorners; ++index) {
    transformPoint(corners[index].corner);
  }
  for (int index = 0; index < numTraces; ++index) {
    idVec3 point{traces[index].start.x, traces[index].start.y, 0.0f};
    transformPoint(point);
    traces[index].start = {point.x, point.y};
    point = {traces[index].end.x, traces[index].end.y, 0.0f};
    transformPoint(point);
    traces[index].end = {point.x, point.y};
  }
}

void idObstacleAvoidanceCore::UnTransformOutput(obstaclePath_t* path) {
  if (!inputTransformValid) {
    const float height = path->startPosOutsideObstacles.z;
    for (int index = 0; index < maxDebugLines; ++index) {
      if (debugLines[index].color == COLOR_INVALID) {
        break;
      }
      debugLines[index].start.z = height;
      debugLines[index].end.z = height;
    }
    return;
  }

  for (int index = 0; index < path->numSeekPos; ++index) {
    const idVec3 input = path->seekPos[index];
    inputTransform.UnTransform(path->seekPos[index], input);
  }

  const idPlane plane = path->seekPosPlane;
  path->seekPosPlane.a = plane.a * inputTransform.mat[0] +
                         plane.b * inputTransform.mat[4] +
                         plane.c * inputTransform.mat[8];
  path->seekPosPlane.b = plane.a * inputTransform.mat[1] +
                         plane.b * inputTransform.mat[5] +
                         plane.c * inputTransform.mat[9];
  path->seekPosPlane.c = plane.a * inputTransform.mat[2] +
                         plane.b * inputTransform.mat[6] +
                         plane.c * inputTransform.mat[10];
  path->seekPosPlane.d = plane.d + plane.a * inputTransform.mat[3] +
                         plane.b * inputTransform.mat[7] +
                         plane.c * inputTransform.mat[11];

  const auto untransform = [this](idVec3& point) {
    const idVec3 input = point;
    inputTransform.UnTransform(point, input);
  };
  untransform(path->firstObstaclePosition);
  untransform(path->startPosOutsideObstacles);
  untransform(path->seekPosOutsideObstacles);
  for (obstaclePath_t::radar_t& radar : path->radar) {
    untransform(radar.direction);
  }
  for (int index = 0; index < maxDebugLines; ++index) {
    if (debugLines[index].color == COLOR_INVALID) {
      break;
    }
    untransform(debugLines[index].start);
    untransform(debugLines[index].end);
  }
}

bool idObstacleAvoidanceCore::WindingRayIntersection(
    const idVec2* winding, int numPoints, const idVec2& start,
    const idVec2& dir, float& scale1, float& scale2, int* edgeNums) {
  scale1 = 0.0f;
  scale2 = 0.0f;
  int sides[20]{};
  int counts[3]{};
  const idVec3 rayPlane{-dir.y, dir.x,
                        -(start.x * -dir.y + start.y * dir.x)};
  for (int index = 0; index < numPoints; ++index) {
    const float distance = rayPlane.x * winding[index].x +
                           rayPlane.y * winding[index].y + rayPlane.z;
    int side;
    if (distance > 0.1f) {
      side = 0;
    } else if (distance < -0.1f) {
      side = 1;
    } else {
      side = 2;
    }
    sides[index] = side;
    ++counts[side];
  }
  sides[numPoints] = sides[0];
  if (counts[0] == 0 || counts[1] == 0) {
    return false;
  }

  idVec3 edgePlanes[2]{};
  int intersectedEdges[2]{};
  int numIntersections = 0;
  for (int index = 0; index < numPoints && numIntersections < 2; ++index) {
    if (sides[index] == sides[index + 1] || sides[index + 1] == 2) {
      continue;
    }
    const int next = (index + 1) % numPoints;
    const float a = winding[index].y - winding[next].y;
    const float b = winding[next].x - winding[index].x;
    edgePlanes[numIntersections] = {
        a, b, -(winding[index].x * a + winding[index].y * b)};
    intersectedEdges[numIntersections] = index;
    ++numIntersections;
  }
  if (numIntersections < 2) {
    return false;
  }

  const auto intersectionScale = [&](const idVec3& plane, float& scale) {
    const float denominator = -(dir.x * plane.x + dir.y * plane.y);
    if (std::fabs(denominator) <= idMath::FLT_SMALLEST_NON_DENORMAL) {
      return false;
    }
    scale = (start.x * plane.x + start.y * plane.y + plane.z) /
            denominator;
    return true;
  };
  if (!intersectionScale(edgePlanes[0], scale1) ||
      !intersectionScale(edgePlanes[1], scale2)) {
    return false;
  }
  if (std::fabs(scale1) > std::fabs(scale2)) {
    std::swap(scale1, scale2);
    std::swap(intersectedEdges[0], intersectedEdges[1]);
  }
  if (edgeNums != nullptr) {
    edgeNums[0] = intersectedEdges[0];
    edgeNums[1] = intersectedEdges[1];
  }
  return true;
}

bool idObstacleAvoidanceCore::LineIntersectsEdgeSequence(
    const idVec2& start, const idVec2& end,
    const obstacleVertex_t* vertices, const obstacleEdge_t* edges,
    const std::int16_t* firstEdge, const std::int16_t* nextEdge,
    int numSequences) const {
  if (std::fabs(start.x - end.x) <= 0.1f &&
      std::fabs(start.y - end.y) <= 0.1f) {
    return false;
  }

  const bool padded = (flags & 0x100) != 0;
  const float lineDx = end.x - start.x;
  const float lineDy = start.y - end.y;
  const float lineScale = padded ? InverseLength(lineDx, lineDy) : 1.0f;
  const idVec3 linePlane{lineDy * lineScale, lineDx * lineScale,
                         -(start.x * lineDy + start.y * lineDx) *
                             lineScale};
  const auto distance = [](const idVec3& plane, const idVec2& point) {
    return plane.x * point.x + plane.y * point.y + plane.z;
  };

  for (int sequence = 0; sequence < numSequences; ++sequence) {
    int edgeNum = firstEdge[sequence];
    if (edgeNum == -1) {
      continue;
    }
    const obstacleEdge_t& first = edges[edgeNum];
    float firstSide = distance(
        linePlane, *reinterpret_cast<const idVec2*>(
                       &vertices[first.verts[0]].position));
    bool firstSign = std::signbit(firstSide);
    while (edgeNum != -1) {
      const obstacleEdge_t& edge = edges[edgeNum];
      if ((edge.flags & 0x200) == 0) {
        const idVec2& p0 = *reinterpret_cast<const idVec2*>(
            &vertices[edge.verts[0]].position);
        const idVec2& p1 = *reinterpret_cast<const idVec2*>(
            &vertices[edge.verts[1]].position);
        const float secondSide = distance(linePlane, p1);
        const bool crossesLine =
            padded ? ((firstSide < 1.0f && secondSide > -1.0f) ||
                      (firstSide > -1.0f && secondSide < 1.0f))
                   : (firstSign != std::signbit(secondSide));
        if (crossesLine) {
          const float edgeDx = p1.x - p0.x;
          const float edgeDy = p0.y - p1.y;
          const float edgeScale =
              padded ? InverseLength(edgeDx, edgeDy) : 1.0f;
          const idVec3 edgePlane{
              edgeDy * edgeScale, edgeDx * edgeScale,
              -(p0.x * edgeDy + p0.y * edgeDx) * edgeScale};
          const float startSide = distance(edgePlane, start);
          const float endSide = distance(edgePlane, end);
          const bool crossesEdge =
              padded ? ((startSide < 1.0f && endSide > -1.0f) ||
                        (startSide > -1.0f && endSide < 1.0f))
                     : (std::signbit(startSide) != std::signbit(endSide));
          if (crossesEdge) {
            const idVec2& edgePoint =
                std::fabs(secondSide) <= std::fabs(firstSide) ? p1 : p0;
            const idVec2& linePoint =
                std::fabs(endSide) <= std::fabs(startSide) ? end : start;
            const float dx = edgePoint.x - linePoint.x;
            const float dy = edgePoint.y - linePoint.y;
            if (dx * dx + dy * dy > 0.010000001f) {
              return true;
            }
          }
        }
        firstSide = secondSide;
        firstSign = std::signbit(secondSide);
      }
      edgeNum = nextEdge[edgeNum];
    }
  }
  return false;
}

bool idObstacleAvoidanceCore::LineIntersectsWall(const idVec2& start,
                                                  const idVec2& end) const {
  if (bspNodes == nullptr || obstacleIndices == nullptr ||
      expandedObstacles == nullptr) {
    return false;
  }
  const idVec2 delta{end.x - start.x, end.y - start.y};
  const float length = Length(delta.x, delta.y);
  const idVec2 bounds[2] = {
      {std::min(start.x, end.x) - 1.0f,
       std::min(start.y, end.y) - 1.0f},
      {std::max(start.x, end.x) + 1.0f,
       std::max(start.y, end.y) + 1.0f},
  };
  int nodes[32];
  const int numNodes = GetBspNodesForLine_r(start, end, 0, 0, nodes);
  for (int nodeIndex = 0; nodeIndex < numNodes; ++nodeIndex) {
    const bspNode_t& node = bspNodes[nodes[nodeIndex]];
    for (int index = 0; index < node.numIndices; ++index) {
      const expandedObstacle_t& obstacle =
          expandedObstacles[obstacleIndices[node.indexOffset + index]];
      if (bounds[0].x > obstacle.bounds[1].x ||
          bounds[0].y > obstacle.bounds[1].y ||
          obstacle.bounds[0].x > bounds[1].x ||
          obstacle.bounds[0].y > bounds[1].y ||
          (obstacle.id != -2 && obstacle.id != -3 && obstacle.id != -4) ||
          (obstacle.id == -4 &&
           !PointInsideWinding(obstacle.winding, obstacle.numPoints, start,
                               0.0001f))) {
        continue;
      }
      float scale1;
      float scale2;
      int edgeNums[2];
      if (WindingRayIntersection(obstacle.winding, obstacle.numPoints,
                                 start, delta, scale1, scale2, edgeNums) &&
          length * scale1 > -0.01f && scale1 < 1.0f &&
          (obstacle.id != -3 || edgeNums[0] == 2 || edgeNums[1] == 2)) {
        return true;
      }
    }
  }
  return false;
}

bool idObstacleAvoidanceCore::LineIntersectsObstacle(
    const idVec2& start, const idVec2& end, int endObstacle,
    int ignoreObstacle) const {
  if (bspNodes == nullptr || expandedObstacles == nullptr ||
      obstacleIndices == nullptr) {
    return false;
  }
  const idVec2 delta{end.x - start.x, end.y - start.y};
  const float length = Length(delta.x, delta.y);
  const idVec2 bounds[2] = {
      {std::min(start.x, end.x) - 1.0f,
       std::min(start.y, end.y) - 1.0f},
      {std::max(start.x, end.x) + 1.0f,
       std::max(start.y, end.y) + 1.0f},
  };
  int nodes[32];
  const int numNodes = GetBspNodesForLine_r(start, end, 0, 0, nodes);
  for (int nodeIndex = 0; nodeIndex < numNodes; ++nodeIndex) {
    const bspNode_t& node = bspNodes[nodes[nodeIndex]];
    for (int index = 0; index < node.numIndices; ++index) {
      const int obstacleNum = obstacleIndices[node.indexOffset + index];
      if (obstacleNum == ignoreObstacle) {
        continue;
      }
      const expandedObstacle_t& obstacle = expandedObstacles[obstacleNum];
      if ((obstacle.id == -4 &&
           !PointInsideWinding(obstacle.winding, obstacle.numPoints, start,
                               0.0001f)) ||
          bounds[0].x > obstacle.bounds[1].x ||
          bounds[0].y > obstacle.bounds[1].y ||
          obstacle.bounds[0].x > bounds[1].x ||
          obstacle.bounds[0].y > bounds[1].y) {
        continue;
      }
      float scale1;
      float scale2;
      int edgeNums[2];
      if (!WindingRayIntersection(obstacle.winding, obstacle.numPoints,
                                  start, delta, scale1, scale2, edgeNums) ||
          scale1 < 0.0f || scale1 > 1.0f ||
          (obstacleNum == endObstacle &&
           length - 0.5f <= length * scale1)) {
        continue;
      }
      const int edgeEnd = (edgeNums[0] + 1) % obstacle.numPoints;
      const idVec2& edgeStartPoint = obstacle.winding[edgeNums[0]];
      const idVec2& edgeEndPoint = obstacle.winding[edgeEnd];
      const auto equal = [](const idVec2& lhs, const idVec2& rhs) {
        return lhs.x == rhs.x && lhs.y == rhs.y;
      };
      if (!equal(edgeStartPoint, start) && !equal(edgeEndPoint, start) &&
          !equal(edgeStartPoint, end) && !equal(edgeEndPoint, end)) {
        return true;
      }
    }
  }
  return false;
}

bool idObstacleAvoidanceCore::GetFirstBlockingObstacle(
    const idVec2& start, const idVec2& delta, int skipObstacle,
    float& blockingScale, int& blockingObstacle, int& blockingEdgeNum) {
  if (bspNodes == nullptr || expandedObstacles == nullptr ||
      obstacleIndices == nullptr) {
    return false;
  }
  const idVec2 end{start.x + delta.x, start.y + delta.y};
  const idVec2 bounds[2] = {
      {std::min(start.x, end.x) - 1.0f,
       std::min(start.y, end.y) - 1.0f},
      {std::max(start.x, end.x) + 1.0f,
       std::max(start.y, end.y) + 1.0f},
  };
  blockingScale = 1.0e30f;
  const float length = Length(delta.x, delta.y);
  int nodes[32];
  const int numNodes = GetBspNodesForLine_r(start, end, 0, 0, nodes);
  for (int nodeIndex = 0; nodeIndex < numNodes; ++nodeIndex) {
    const bspNode_t& node = bspNodes[nodes[nodeIndex]];
    for (int index = 0; index < node.numIndices; ++index) {
      const int obstacleNum = obstacleIndices[node.indexOffset + index];
      if (obstacleNum == skipObstacle) {
        continue;
      }
      const expandedObstacle_t& obstacle = expandedObstacles[obstacleNum];
      if (bounds[0].x > obstacle.bounds[1].x ||
          bounds[0].y > obstacle.bounds[1].y ||
          obstacle.bounds[0].x > bounds[1].x ||
          obstacle.bounds[0].y > bounds[1].y ||
          (obstacle.id == -4 &&
           !PointInsideWinding(obstacle.winding, obstacle.numPoints, start,
                               0.0001f))) {
        continue;
      }
      float scale1;
      float scale2;
      int edgeNums[2];
      if (WindingRayIntersection(obstacle.winding, obstacle.numPoints,
                                 start, delta, scale1, scale2, edgeNums) &&
          blockingScale > scale1 && length * scale2 > 0.01f) {
        blockingScale = scale1;
        blockingObstacle = obstacleNum;
        blockingEdgeNum = edgeNums[0];
      }
    }
  }
  return blockingScale < 1.0f;
}

bool idObstacleAvoidanceCore::GetPointOutsideObstacles(
    idVec2& point, int* obstacle, int* edgeNum, const idVec2& startPos,
    bool moveAroundSeekPosObstacles) {
  if (obstacle != nullptr) {
    *obstacle = -1;
  }
  if (edgeNum != nullptr) {
    *edgeNum = -1;
  }
  if (expandedObstacles == nullptr) {
    return true;
  }

  idVec2 startPoint = point;
  idVec2 bestPoint = point;
  idVec2 outputPoint = point;
  int outputObstacle = -1;
  int outputEdge = -1;
  int numVisited = 0;
  tempMemoryEnd -= (sizeof(int) * numExpandedObstacles + 15) & ~15;
  int* const visited = reinterpret_cast<int*>(tempMemoryEnd);

  for (;;) {
    const int currentObstacle = PointInsideObstacle(bestPoint);
    if (currentObstacle == -1) {
      point = outputPoint;
      if (obstacle != nullptr) {
        *obstacle = outputObstacle;
      }
      if (edgeNum != nullptr) {
        *edgeNum = outputEdge;
      }
      if (tempMemory != nullptr) {
        tempMemoryEnd = tempMemory + tempMemorySize;
      }
      return true;
    }

    expandedObstacle_t& current = expandedObstacles[currentObstacle];
    const bool wallOrLedge = current.id == -2 || current.id == -3;
    float bestDistance = 1.0e30f;
    bool found = false;
    for (int index = 0; index < current.numPoints; ++index) {
      const int next = (index + 1) % current.numPoints;
      const float normalX = current.winding[next].y -
                            current.winding[index].y;
      const float normalY = current.winding[index].x -
                            current.winding[next].x;
      const float inverseLength = InverseLength(normalX, normalY);
      const idVec3 plane{
          normalX * inverseLength, normalY * inverseLength,
          -(current.winding[next].x * normalX * inverseLength +
            current.winding[next].y * normalY * inverseLength)};
      if (!wallOrLedge && moveAroundSeekPosObstacles &&
          plane.x * startPos.x + plane.y * startPos.y + plane.z < 0.0f) {
        continue;
      }
      const float distance = plane.x * startPoint.x +
                             plane.y * startPoint.y + plane.z;
      if (bestDistance <= distance) {
        if (wallOrLedge) {
          break;
        }
        continue;
      }
      const idVec2 candidate{
          startPoint.x - (distance + 0.5f) * plane.x,
          startPoint.y - (distance + 0.5f) * plane.y};
      const float dx = candidate.x - startPoint.x;
      const float dy = candidate.y - startPoint.y;
      const float distanceSqr = dx * dx + dy * dy;
      if (distanceSqr < bestDistance &&
          PointInsideObstacleList(visited, numVisited, candidate) == -1 &&
          (wallOrLedge || !LineIntersectsWall(startPoint, candidate))) {
        outputPoint = candidate;
        bestPoint = candidate;
        bestDistance = distanceSqr;
        outputObstacle = currentObstacle;
        outputEdge = index;
        found = true;
        if (wallOrLedge) {
          startPoint = candidate;
          break;
        }
      }
    }

    idVec2 expandedCurrent[16];
    ExpandWinding(current.winding, current.numPoints, 0.5f,
                  expandedCurrent);
    const int currentEdgeCount = wallOrLedge ? 1 : current.numPoints;
    for (int visitedIndex = 0; visitedIndex < numVisited; ++visitedIndex) {
      const expandedObstacle_t& other = expandedObstacles[visited[visitedIndex]];
      if (other.bounds[0].x > current.bounds[1].x + 0.5f ||
          other.bounds[0].y > current.bounds[1].y + 0.5f ||
          current.bounds[0].x - 0.5f > other.bounds[1].x ||
          current.bounds[0].y - 0.5f > other.bounds[1].y) {
        continue;
      }
      const bool otherWallOrLedge = other.id == -2 || other.id == -3;
      idVec2 expandedOther[16];
      ExpandWinding(other.winding, other.numPoints, 0.5f, expandedOther);
      for (int edge = 0; edge < currentEdgeCount; ++edge) {
        const int next = (edge + 1) % currentEdgeCount;
        const idVec2 direction{expandedCurrent[next].x -
                                   expandedCurrent[edge].x,
                               expandedCurrent[next].y -
                                   expandedCurrent[edge].y};
        float scales[2];
        int edgeNums[2];
        if (!WindingRayIntersection(expandedOther, other.numPoints,
                                    expandedCurrent[edge], direction,
                                    scales[0], scales[1], edgeNums)) {
          continue;
        }
        for (int hit = 0; hit < 2; ++hit) {
          if ((otherWallOrLedge && edgeNums[hit] != 0) ||
              scales[hit] < -0.01f || scales[hit] > 1.01f) {
            continue;
          }
          const idVec2 candidate{
              expandedCurrent[edge].x + scales[hit] * direction.x,
              expandedCurrent[edge].y + scales[hit] * direction.y};
          const float dx = candidate.x - startPoint.x;
          const float dy = candidate.y - startPoint.y;
          const float distanceSqr = dx * dx + dy * dy;
          if (distanceSqr < bestDistance &&
              PointInsideObstacleList(visited, numVisited, candidate) == -1 &&
              (wallOrLedge || !LineIntersectsWall(startPoint, candidate))) {
            outputPoint = candidate;
            bestPoint = candidate;
            bestDistance = distanceSqr;
            outputObstacle = currentObstacle;
            outputEdge = edge;
            found = true;
            if (wallOrLedge) {
              startPoint = candidate;
            }
          }
        }
      }
    }

    if (found && numVisited + 1 < 16) {
      visited[numVisited++] = currentObstacle;
      continue;
    }
    if (obstacle != nullptr) {
      *obstacle = outputObstacle;
    }
    if (edgeNum != nullptr) {
      *edgeNum = outputEdge;
    }
    if (tempMemory != nullptr) {
      tempMemoryEnd = tempMemory + tempMemorySize;
    }
    return false;
  }
}

bool idObstacleAvoidanceCore::SetupExpandedObstacles(
    const idBounds& bbox, const idVec3& gravity, const idVec3& startPos,
    const idVec3& seekPos, float obstacleRadius, bool assumeValidAASPath,
    const obstacleBox_t* obstacles, int numObstacles,
    const obstacleVertex_t* vertices, int numVertices,
    const obstacleEdge_t* edges, int numEdges,
    const obstacleCorner_t* corners, int numCorners, bool hasTraces) {
  (void)numVertices;
  const idVec2 axialBounds[2] = {
      {bbox.b[0].x - 1.0f, bbox.b[0].y - 1.0f},
      {bbox.b[1].x + 1.0f, bbox.b[1].y + 1.0f},
  };
  const float terminatorWidth =
      (axialBounds[1].x - axialBounds[0].x) * 0.5f + 1.0f;
  const auto addDebugLine = [&](avoidColor_t color, const idVec2& start,
                                const idVec2& end) {
    if (debugLines == nullptr || numDebugLines >= maxDebugLines - 1) {
      return;
    }
    debugLines[numDebugLines++] =
        {color, {start.x, start.y, 0.0f}, {end.x, end.y, 0.0f}};
    debugLines[numDebugLines].color = COLOR_INVALID;
  };
  const auto addDebugIndex = [&](const expandedObstacle_t& obstacle,
                                 int index) {
    if (debugText == nullptr || numDebugText >= maxDebugText - 1) {
      return;
    }
    avoidDebugText_t& text = debugText[numDebugText++];
    text.color = COLOR_WHITE;
    text.pos = {(obstacle.bounds[0].x + obstacle.bounds[1].x) * 0.5f,
                (obstacle.bounds[0].y + obstacle.bounds[1].y) * 0.5f,
                0.0f};
    std::snprintf(text.text, sizeof(text.text), "%d", index);
    text.text[sizeof(text.text) - 1] = '\0';
    debugText[numDebugText].color = COLOR_INVALID;
  };

  for (int obstacleIndex = 0; obstacleIndex < numObstacles;
       ++obstacleIndex) {
    expandedObstacle_t& expanded =
        expandedObstacles[numExpandedObstacles];
    expanded.id = obstacles[obstacleIndex].id.value;
    idVec3 silhouette[16];
    const int numSilhouette = BoxParallelProjectionSilhouetteVerts(
        obstacles[obstacleIndex].axis, obstacles[obstacleIndex].center,
        obstacles[obstacleIndex].extents, gravity, silhouette);
    idVec2 winding[16];
    for (int index = 0; index < numSilhouette; ++index) {
      winding[index] = {silhouette[index].x, silhouette[index].y};
    }
    expanded.numPoints = ExpandWindingForAxialBox(
        winding, numSilhouette, axialBounds);
    if (expanded.numPoints <= 0) {
      continue;
    }
    GetWindingBounds(winding, expanded.numPoints, expanded.bounds);
    const idVec2 seek2{seekPos.x, seekPos.y};
    if (obstacles[obstacleIndex].soft) {
      const bool outsideBounds =
          seek2.x < expanded.bounds[0].x ||
          seek2.y < expanded.bounds[0].y ||
          expanded.bounds[1].x < seek2.x ||
          expanded.bounds[1].y < seek2.y;
      if (!outsideBounds ||
          PointInsideWinding(winding, expanded.numPoints, seek2, 0.1f)) {
        continue;
      }
    }
    expanded.winding = windingPoints + numWindingPoints;
    numWindingPoints += expanded.numPoints;
    std::memcpy(expanded.winding, winding,
                sizeof(idVec2) * expanded.numPoints);
    const int addedIndex = numExpandedObstacles++;
    if (debugLines != nullptr) {
      for (int index = 0; index < numSilhouette; ++index) {
        const int next = (index + 1) % numSilhouette;
        addDebugLine(COLOR_ORANGE,
                     {silhouette[index].x, silhouette[index].y},
                     {silhouette[next].x, silhouette[next].y});
      }
    }
    addDebugIndex(expanded, addedIndex);
  }

  bool result = true;
  const idVec2 start2{startPos.x, startPos.y};
  if (assumeValidAASPath && PointInsideObstacle(start2) == -1) {
    const idVec2 delta{seekPos.x - startPos.x, seekPos.y - startPos.y};
    float blockingScale;
    int blockingObstacle;
    int blockingEdge;
    if (!GetFirstBlockingObstacle(start2, delta, -1, blockingScale,
                                  blockingObstacle, blockingEdge)) {
      if (!hasTraces) {
        return false;
      }
      result = false;
    }
  }

  const std::size_t edgeArraySize =
      (sizeof(std::int16_t) * static_cast<std::size_t>(numEdges) + 15) &
      ~std::size_t(15);
  tempMemoryEnd -= edgeArraySize;
  std::int16_t* const nextEdge =
      reinterpret_cast<std::int16_t*>(tempMemoryEnd);
  tempMemoryEnd -= edgeArraySize;
  std::int16_t* const sequenceFirst =
      reinterpret_cast<std::int16_t*>(tempMemoryEnd);
  tempMemoryEnd -= edgeArraySize;
  std::int16_t* const sequenceLast =
      reinterpret_cast<std::int16_t*>(tempMemoryEnd);
  int numSequences = numEdges;
  for (int index = 0; index < numEdges; ++index) {
    nextEdge[index] = -1;
    sequenceFirst[index] = static_cast<std::int16_t>(index);
    sequenceLast[index] = static_cast<std::int16_t>(index);
  }

  for (int pass = 0; pass <= 4; ++pass) {
    bool merged = false;
    for (int first = 0; first < numSequences; ++first) {
      for (int second = first + 1; second < numSequences; ++second) {
        const int firstHead = sequenceFirst[first];
        const int firstTail = sequenceLast[first];
        const int secondHead = sequenceFirst[second];
        const int secondTail = sequenceLast[second];
        if (edges[firstHead].vertNums[0] ==
            edges[secondTail].vertNums[1]) {
          nextEdge[secondTail] = static_cast<std::int16_t>(firstHead);
          sequenceFirst[first] = static_cast<std::int16_t>(secondHead);
        } else if (edges[firstTail].vertNums[1] ==
                   edges[secondHead].vertNums[0]) {
          nextEdge[firstTail] = static_cast<std::int16_t>(secondHead);
          sequenceLast[first] = static_cast<std::int16_t>(secondTail);
        } else {
          continue;
        }
        --numSequences;
        sequenceFirst[second] = sequenceFirst[numSequences];
        sequenceLast[second] = sequenceLast[numSequences];
        merged = true;
        --second;
      }
    }
    if (!merged) {
      break;
    }
  }

  for (int sequence = 0; sequence < numSequences; ++sequence) {
    int first = sequenceFirst[sequence];
    while (first != -1 && (edges[first].flags & 0x200) != 0) {
      first = nextEdge[first];
    }
    if (first == -1) {
      continue;
    }
    int second = nextEdge[first];
    while (second != -1 && (edges[second].flags & 0x200) != 0) {
      second = nextEdge[second];
    }
    if (second == -1) {
      expandedObstacle_t& expanded =
          expandedObstacles[numExpandedObstacles++];
      SetupWallOrLedgeWinding(expanded, -1, first, -1, edges, vertices);
      addDebugIndex(expanded, numExpandedObstacles - 1);
      continue;
    }
    int previous = first;
    int current = second;
    int next = nextEdge[current];
    while (next != -1) {
      while (next != -1 && (edges[next].flags & 0x200) != 0) {
        next = nextEdge[next];
      }
      if (next == -1) {
        break;
      }
      expandedObstacle_t& expanded =
          expandedObstacles[numExpandedObstacles++];
      SetupWallOrLedgeWinding(expanded, previous, current, next, edges,
                              vertices);
      addDebugIndex(expanded, numExpandedObstacles - 1);
      previous = current;
      current = next;
      next = nextEdge[next];
    }
    const bool closed = edges[sequenceLast[sequence]].vertNums[1] ==
                        edges[sequenceFirst[sequence]].vertNums[0];
    expandedObstacle_t& endExpanded =
        expandedObstacles[numExpandedObstacles++];
    SetupWallOrLedgeWinding(endExpanded, previous, current,
                            closed ? first : -1, edges, vertices);
    addDebugIndex(endExpanded, numExpandedObstacles - 1);
    expandedObstacle_t& firstExpanded =
        expandedObstacles[numExpandedObstacles++];
    SetupWallOrLedgeWinding(firstExpanded, closed ? current : -1, first,
                            second, edges, vertices);
    addDebugIndex(firstExpanded, numExpandedObstacles - 1);
  }

  struct terminator_t {
    float distSqr;
    int lastEdgeIndex;
    int firstEdgeIndex;
    int padding;
  };
  const idVec2 terminatorBounds[2] = {
      {startPos.x - obstacleRadius, startPos.y - obstacleRadius},
      {startPos.x + obstacleRadius, startPos.y + obstacleRadius},
  };
  const std::size_t terminatorMemorySize =
      (sizeof(terminator_t) * static_cast<std::size_t>(numSequences) + 15) &
      ~std::size_t(15);
  tempMemoryEnd -= terminatorMemorySize;
  terminator_t* const addedTerminators =
      reinterpret_cast<terminator_t*>(tempMemoryEnd);
  int numTerminators = 0;
  for (int sequence = 0; sequence < numSequences; ++sequence) {
    const int sequenceTail = sequenceLast[sequence];
    const int sequenceHead = sequenceFirst[sequence];
    if (edges[sequenceTail].vertNums[1] ==
        edges[sequenceHead].vertNums[0]) {
      continue;
    }
    terminator_t best[2] = {{1.0e30f, -1, -1, 0},
                            {1.0e30f, -1, -1, 0}};
    for (int other = 0; other < numSequences; ++other) {
      const int otherTail = sequenceLast[other];
      const int otherHead = sequenceFirst[other];
      if (edges[otherTail].vertNums[1] ==
          edges[otherHead].vertNums[0]) {
        continue;
      }
      const idVec2& endA = *reinterpret_cast<const idVec2*>(
          &vertices[edges[sequenceTail].verts[1]].position);
      const idVec2& startB = *reinterpret_cast<const idVec2*>(
          &vertices[edges[otherHead].verts[0]].position);
      if (endA.x >= terminatorBounds[0].x &&
          endA.x <= terminatorBounds[1].x &&
          endA.y >= terminatorBounds[0].y &&
          endA.y <= terminatorBounds[1].y &&
          startB.x >= terminatorBounds[0].x &&
          startB.x <= terminatorBounds[1].x &&
          startB.y >= terminatorBounds[0].y &&
          startB.y <= terminatorBounds[1].y) {
        const float dx = startB.x - endA.x;
        const float dy = startB.y - endA.y;
        const float distance = dx * dx + dy * dy;
        if (distance < best[0].distSqr) {
          best[0] = {distance, sequenceTail, otherHead, 0};
        }
      }
      const idVec2& endB = *reinterpret_cast<const idVec2*>(
          &vertices[edges[otherTail].verts[1]].position);
      const idVec2& startA = *reinterpret_cast<const idVec2*>(
          &vertices[edges[sequenceHead].verts[0]].position);
      if (endB.x >= terminatorBounds[0].x &&
          endB.x <= terminatorBounds[1].x &&
          endB.y >= terminatorBounds[0].y &&
          endB.y <= terminatorBounds[1].y &&
          startA.x >= terminatorBounds[0].x &&
          startA.x <= terminatorBounds[1].x &&
          startA.y >= terminatorBounds[0].y &&
          startA.y <= terminatorBounds[1].y) {
        const float dx = startA.x - endB.x;
        const float dy = startA.y - endB.y;
        const float distance = dx * dx + dy * dy;
        if (distance < best[1].distSqr) {
          best[1] = {distance, otherTail, sequenceHead, 0};
        }
      }
    }
    for (const terminator_t& candidate : best) {
      if (candidate.lastEdgeIndex == -1 ||
          candidate.firstEdgeIndex == -1) {
        continue;
      }
      bool duplicate = false;
      for (int index = 0; index < numTerminators; ++index) {
        if (addedTerminators[index].lastEdgeIndex ==
                candidate.lastEdgeIndex &&
            addedTerminators[index].firstEdgeIndex ==
                candidate.firstEdgeIndex) {
          duplicate = true;
          break;
        }
      }
      if (duplicate) {
        continue;
      }
      addedTerminators[numTerminators++] = candidate;
      const idVec2& firstPoint = *reinterpret_cast<const idVec2*>(
          &vertices[edges[candidate.lastEdgeIndex].verts[1]].position);
      const idVec2& secondPoint = *reinterpret_cast<const idVec2*>(
          &vertices[edges[candidate.firstEdgeIndex].verts[0]].position);
      if (LineIntersectsEdgeSequence(firstPoint, secondPoint, vertices,
                                     edges, sequenceFirst, nextEdge,
                                     numSequences)) {
        continue;
      }
      if (numExpandedObstacles >= maxExpandedObstacles) {
        break;
      }
      const obstacleEdge_t& last = edges[candidate.lastEdgeIndex];
      const obstacleEdge_t& firstEdgeData = edges[candidate.firstEdgeIndex];
      const idVec2 lastNormal = EdgeNormal(last, vertices);
      const idVec2 firstNormal = EdgeNormal(firstEdgeData, vertices);
      const float connectorInv =
          InverseLength(firstPoint.x - secondPoint.x,
                        secondPoint.y - firstPoint.y);
      const idVec2 connector{(firstPoint.x - secondPoint.x) * connectorInv,
                             (secondPoint.y - firstPoint.y) * connectorInv};
      expandedObstacle_t& expanded =
          expandedObstacles[numExpandedObstacles++];
      expanded.id = -4;
      expanded.numPoints = 4;
      expanded.winding = windingPoints + numWindingPoints;
      numWindingPoints += 4;
      expanded.winding[0] = secondPoint;
      expanded.winding[1] = firstPoint;
      expanded.winding[2] = {
          (firstPoint.x - -connector.x) -
              connector.y * terminatorWidth -
              lastNormal.y * terminatorWidth,
          (firstPoint.y - connector.y) -
              connector.x * terminatorWidth -
              lastNormal.x * terminatorWidth};
      expanded.winding[3] = {
          (-connector.x + secondPoint.x) -
              connector.y * terminatorWidth -
              firstNormal.y * terminatorWidth,
          (connector.y + secondPoint.y) -
              connector.x * terminatorWidth -
              firstNormal.x * terminatorWidth};
      GetWindingBounds(expanded.winding, 4, expanded.bounds);
    }
  }

  for (int cornerIndex = 0;
       cornerIndex < numCorners &&
       numExpandedObstacles < maxExpandedObstacles;
       ++cornerIndex) {
    expandedObstacle_t& expanded =
        expandedObstacles[numExpandedObstacles++];
    expanded.numPoints = 16;
    expanded.winding = windingPoints + numWindingPoints;
    numWindingPoints += 16;
    GetCirclePoints({corners[cornerIndex].corner.x,
                     corners[cornerIndex].corner.y},
                    corners[cornerIndex].radius, expanded.winding);
    const float radius = corners[cornerIndex].radius;
    expanded.bounds[0] = {corners[cornerIndex].corner.x - radius,
                          corners[cornerIndex].corner.y - radius};
    expanded.bounds[1] = {corners[cornerIndex].corner.x + radius,
                          corners[cornerIndex].corner.y + radius};
    expanded.id = -5;
  }

  if (bspNodes != nullptr) {
    bspNode_t& root = bspNodes[numBspNodes++];
    root.axis = 0;
    root.dist = startPos.x;
    root.indexOffset = 0;
    root.numIndices = 0;
    const idVec2 span{obstacleRadius * 0.5f, obstacleRadius};
    root.children[0] = SetupBspTree_r(
        {startPos.x + obstacleRadius * 0.5f, startPos.y}, span, 1, 1);
    root.children[1] = SetupBspTree_r(
        {startPos.x - obstacleRadius * 0.5f, startPos.y}, span, 1, 1);
  }
  if (tempMemory != nullptr) {
    tempMemoryEnd = tempMemory + tempMemorySize;
  }
  const std::size_t obstacleNodeMemory =
      (sizeof(std::int16_t) *
           static_cast<std::size_t>(numExpandedObstacles) +
       15) &
      ~std::size_t(15);
  tempMemoryEnd -= obstacleNodeMemory;
  std::int16_t* const obstacleNodes =
      reinterpret_cast<std::int16_t*>(tempMemoryEnd);
  for (int index = 0; index < numExpandedObstacles; ++index) {
    FilterObstacleIntoBspTree(0, index, obstacleNodes);
  }
  int indexOffset = 0;
  for (int index = 0; index < numBspNodes; ++index) {
    bspNodes[index].indexOffset = static_cast<std::int16_t>(indexOffset);
    indexOffset += bspNodes[index].numIndices;
    bspNodes[index].numIndices = 0;
  }
  for (int index = 0; index < numExpandedObstacles; ++index) {
    bspNode_t& node = bspNodes[obstacleNodes[index]];
    obstacleIndices[node.indexOffset + node.numIndices++] =
        static_cast<std::int16_t>(index);
  }

  if (debugLines != nullptr) {
    for (int obstacleIndex = 0; obstacleIndex < numExpandedObstacles;
         ++obstacleIndex) {
      const expandedObstacle_t& obstacle = expandedObstacles[obstacleIndex];
      if (obstacle.id == -4) {
        addDebugLine(COLOR_RED, obstacle.winding[0], obstacle.winding[1]);
      } else if (obstacle.id == -2 || obstacle.id == -3) {
        addDebugLine(obstacle.id == -2 ? COLOR_WHITE : COLOR_ORANGE,
                     obstacle.winding[0], obstacle.winding[1]);
        for (int index = 1; index < obstacle.numPoints; ++index) {
          addDebugLine(obstacle.id == -2 ? COLOR_LIGHT_GREY : COLOR_ORANGE,
                       obstacle.winding[index],
                       obstacle.winding[(index + 1) % obstacle.numPoints]);
        }
      } else {
        for (int index = 0; index < obstacle.numPoints; ++index) {
          addDebugLine(COLOR_GREEN, obstacle.winding[index],
                       obstacle.winding[(index + 1) % obstacle.numPoints]);
        }
      }
    }
  }
  if (tempMemory != nullptr) {
    tempMemoryEnd = tempMemory + tempMemorySize;
  }
  return result;
}

bool idObstacleAvoidanceCore::GetPathNodeDelta(pathNode_t* node,
                                                const idVec2& seekPos,
                                                const idBounds& bounds,
                                                bool passedCorner) {
  (void)bounds;
  if (expandedObstacles == nullptr) {
    return false;
  }
  const expandedObstacle_t& obstacle = expandedObstacles[node->obstacle];
  const int numPoints = obstacle.numPoints;
  int pointIndex = (node->edgeNum + node->dir) % numPoints;
  node->delta = {obstacle.winding[pointIndex].x - node->pos.x,
                 obstacle.winding[pointIndex].y - node->pos.y};
  if (node->delta.x * node->delta.x + node->delta.y * node->delta.y <=
      0.0001f) {
    do {
      node->edgeNum =
          (node->edgeNum + 2 * node->dir + numPoints - 1) % numPoints;
      pointIndex = (node->edgeNum + node->dir) % numPoints;
      node->delta = {obstacle.winding[pointIndex].x - node->pos.x,
                     obstacle.winding[pointIndex].y - node->pos.y};
    } while (node->delta.x * node->delta.x +
                 node->delta.y * node->delta.y <=
             0.0001f);
  } else if (!passedCorner) {
    goto check_repeated_edge;
  }

  {
    const idVec2 toSeek{seekPos.x - node->pos.x,
                        seekPos.y - node->pos.y};
    const bool passed =
        ((toSeek.y * node->delta.x - toSeek.x * node->delta.y) *
         static_cast<float>(2 * node->dir - 1)) >= 0.0f;
    idVec2 testDelta = node->delta;
    const float deltaLengthSqr =
        testDelta.x * testDelta.x + testDelta.y * testDelta.y;
    if (deltaLengthSqr > 1.0f) {
      const float inverseLength = InverseLength(testDelta.x, testDelta.y);
      testDelta.x *= inverseLength;
      testDelta.y *= inverseLength;
    }
    if (passed) {
      const idVec2 testStart{node->pos.x + testDelta.x,
                             node->pos.y + testDelta.y};
      if (!LineIntersectsPath(testStart, seekPos, node->parent)) {
        node->delta = toSeek;
        node->edgeNum = -1;
      }
    }
  }

check_repeated_edge:
  if (node->edgeNum == -1 || node->parent == nullptr) {
    return true;
  }
  for (const pathNode_t* parent = node->parent; parent != nullptr;
       parent = parent->parent) {
    if (node->obstacle != parent->obstacle ||
        node->edgeNum != parent->edgeNum) {
      continue;
    }
    const float currentProjection =
        node->pos.x * node->delta.x + node->pos.y * node->delta.y;
    const float currentEndProjection =
        (node->pos.x + node->delta.x) * node->delta.x +
        (node->pos.y + node->delta.y) * node->delta.y;
    const float parentProjection =
        parent->pos.x * node->delta.x + parent->pos.y * node->delta.y;
    const float parentEndProjection =
        (parent->pos.x + parent->delta.x) * node->delta.x +
        (parent->pos.y + parent->delta.y) * node->delta.y;
    if (parentProjection <= currentEndProjection &&
        currentProjection <= parentEndProjection) {
      return false;
    }
  }
  return true;
}

idObstacleAvoidanceCore::pathNode_t* idObstacleAvoidanceCore::BuildPathTree(
    const idVec3& startPos, const idVec3& seekPos, float obstacleRadius,
    obstaclePath_t* path) {
  const idBounds clipBounds{{
      {startPos.x - obstacleRadius, startPos.y - obstacleRadius, 0.0f},
      {startPos.x + obstacleRadius, startPos.y + obstacleRadius, 0.0f}}};
  const auto initNode = [](pathNode_t* node) {
    node->dir = 0;
    node->pos = {0.0f, 0.0f};
    node->delta = {0.0f, 0.0f};
    node->obstacle = -1;
    node->edgeNum = -1;
    node->numNodes = 0;
    node->parent = nullptr;
    node->children[0] = nullptr;
    node->children[1] = nullptr;
    node->queueNext = -1;
  };
  pathNode_t* const root = &pathNodes[numPathNodes++];
  initNode(root);
  root->pos = {startPos.x, startPos.y};
  root->delta = {seekPos.x - startPos.x, seekPos.y - startPos.y};
  root->numNodes = 1;

  int pending[128];
  int treeOrder[128];
  int enqueueCount = 1;
  int dequeueCount = 0;
  int treeCount = 0;
  int bestNumNodes = 64;
  pending[0] = static_cast<int>(root - pathNodes);
  while (dequeueCount < enqueueCount &&
         numPathNodes < maxPathNodes - 1) {
    pathNode_t* node = &pathNodes[pending[dequeueCount % 128]];
    ++dequeueCount;
    treeOrder[treeCount++ % 128] = static_cast<int>(node - pathNodes);
    if (node->numNodes >= 4 * bestNumNodes) {
      continue;
    }
    const idVec2 end{node->pos.x + node->delta.x,
                     node->pos.y + node->delta.y};
    if (clipBounds.b[0].x > end.x - 10.0f ||
        end.x + 10.0f > clipBounds.b[1].x ||
        clipBounds.b[0].y > end.y - 10.0f ||
        end.y + 10.0f > clipBounds.b[1].y) {
      continue;
    }
    float blockingScale;
    int blockingObstacle;
    int blockingEdge;
    if (!GetFirstBlockingObstacle(node->pos, node->delta, node->obstacle,
                                  blockingScale, blockingObstacle,
                                  blockingEdge)) {
      pathNode_t* child = &pathNodes[numPathNodes++];
      initNode(child);
      node->children[node->dir] = child;
      child->parent = node;
      child->dir = node->dir;
      child->pos = end;
      child->numNodes = node->numNodes + 1;
      if (node->edgeNum == -1) {
        bestNumNodes = std::min(bestNumNodes, node->numNodes);
        continue;
      }
      child->obstacle = node->obstacle;
      const int numPoints = expandedObstacles[node->obstacle].numPoints;
      child->edgeNum =
          (node->edgeNum + 2 * node->dir + numPoints - 1) % numPoints;
      if (GetPathNodeDelta(child, {seekPos.x, seekPos.y}, clipBounds,
                           true)) {
        pending[enqueueCount++ % 128] =
            static_cast<int>(child - pathNodes);
      }
      continue;
    }

    const int obstacleId = expandedObstacles[blockingObstacle].id;
    if (obstacleId == -4) {
      continue;
    }
    if (path->firstObstacle == -1 && obstacleId != -2 &&
        obstacleId != -3 && obstacleId != -5) {
      path->firstObstacle = obstacleId;
    }
    node->delta.x *= blockingScale;
    node->delta.y *= blockingScale;
    const idVec2 blockedPos{node->pos.x + node->delta.x,
                            node->pos.y + node->delta.y};
    if (node->edgeNum != -1) {
      pathNode_t* child = &pathNodes[numPathNodes++];
      initNode(child);
      node->children[node->dir] = child;
      child->parent = node;
      child->dir = node->dir;
      child->pos = blockedPos;
      child->obstacle = blockingObstacle;
      child->edgeNum = blockingEdge;
      child->numNodes = node->numNodes + 1;
      if (GetPathNodeDelta(child, {seekPos.x, seekPos.y}, clipBounds,
                           false)) {
        pending[enqueueCount++ % 128] =
            static_cast<int>(child - pathNodes);
      }
    } else {
      for (int direction = 0; direction < 2; ++direction) {
        pathNode_t* child = &pathNodes[numPathNodes++];
        initNode(child);
        node->children[direction] = child;
        child->parent = node;
        child->dir = direction;
        child->pos = blockedPos;
        child->obstacle = blockingObstacle;
        child->edgeNum = blockingEdge;
        child->numNodes = node->numNodes + 1;
        if (GetPathNodeDelta(child, {seekPos.x, seekPos.y}, clipBounds,
                             false)) {
          pending[enqueueCount++ % 128] =
              static_cast<int>(child - pathNodes);
        }
      }
    }
  }
  for (int index = 0; index < treeCount - 1; ++index) {
    pathNodes[treeOrder[index % 128]].queueNext =
        treeOrder[(index + 1) % 128];
  }
  return root;
}

float idObstacleAvoidanceCore::OptimizePath(
    const pathNode_t* root, const pathNode_t* leaf,
    const idVec2& originalSeekPos, const idVec2& lastDir,
    idVec3* optimizedPath, int& numPathPoints) const {
  if (leaf == root) {
    optimizedPath[0].x = root->pos.x;
    optimizedPath[0].y = root->pos.y;
    optimizedPath[1].x = root->pos.x + root->delta.x;
    optimizedPath[1].y = root->pos.y + root->delta.y;
    return Length(root->delta.x, root->delta.y);
  }
  optimizedPath[0].x = originalSeekPos.x;
  optimizedPath[0].y = originalSeekPos.y;
  optimizedPath[1].x = originalSeekPos.x;
  optimizedPath[1].y = originalSeekPos.y;
  int count = 0;
  float length = 0.0f;
  const pathNode_t* current = root;
  do {
    const pathNode_t* next = leaf;
    while (next->parent != current) {
      if (next->obstacle != current->obstacle &&
          !LineIntersectsObstacle(current->pos, next->pos,
                                  next->obstacle, -1)) {
        break;
      }
      next = next->parent;
    }
    length += Length(next->pos.x - current->pos.x,
                     next->pos.y - current->pos.y);
    if (count < 31) {
      optimizedPath[count].x = next->pos.x;
      optimizedPath[count].y = next->pos.y;
      ++count;
    }
    current = next;
  } while (current != leaf);
  numPathPoints = count;
  const idVec2 initialDelta{optimizedPath[0].x - root->pos.x,
                            optimizedPath[0].y - root->pos.y};
  const bool forward =
      ((std::bit_cast<std::uint32_t>(lastDir.x) |
        std::bit_cast<std::uint32_t>(lastDir.y)) &
       0x7FFFFFFFu) != 0
          ? initialDelta.x * lastDir.x + initialDelta.y * lastDir.y >=
                0.0f
          : initialDelta.x * (originalSeekPos.x - root->pos.x) +
                    initialDelta.y * (originalSeekPos.y - root->pos.y) >=
                0.0f;
  return forward ? length : length + 100.0f;
}

void idObstacleAvoidanceCore::FindOptimalPath(
    const pathNode_t* root, const idVec2& originalSeekPos,
    const idVec2& lastDir, idVec3* optimizedPath, int& numPathPoints,
    float& targetDist, float& pathLength) {
  const pathNode_t* closest = root;
  for (const pathNode_t* node = root; node != nullptr;) {
    if (closest == nullptr || closest->targetDistSqr > node->targetDistSqr) {
      closest = node;
    }
    if (node->children[0] != nullptr) {
      node = node->children[0];
    } else if (node->children[1] != nullptr) {
      node = node->children[1];
    } else {
      while (node->parent != nullptr &&
             (node->parent->children[1] == nullptr ||
              node->parent->children[1] == node)) {
        node = node->parent;
      }
      node = node->parent != nullptr ? node->parent->children[1] : nullptr;
    }
  }
  const float closestDistanceSqr =
      closest == root ? 0.0f : closest->targetDistSqr;
  numPathPoints = 2;
  float bestLength = OptimizePath(root, closest, originalSeekPos, lastDir,
                                  optimizedPath, numPathPoints);
  for (const pathNode_t* node = root; node != nullptr;) {
    if (node != closest && node->targetDistSqr < closestDistanceSqr + 1.0f) {
      idVec3 candidate[31];
      std::memmove(candidate, optimizedPath, sizeof(candidate));
      int candidateCount = 0;
      const float candidateLength =
          OptimizePath(root, node, originalSeekPos, lastDir, candidate,
                       candidateCount);
      if (candidateLength < bestLength) {
        closest = node;
        bestLength = candidateLength;
        std::memmove(optimizedPath, candidate, sizeof(candidate));
        numPathPoints = candidateCount;
      }
    }
    if (node->children[0] != nullptr) {
      node = node->children[0];
    } else if (node->children[1] != nullptr) {
      node = node->children[1];
    } else {
      while (node->parent != nullptr &&
             (node->parent->children[1] == nullptr ||
              node->parent->children[1] == node)) {
        node = node->parent;
      }
      node = node->parent != nullptr ? node->parent->children[1] : nullptr;
    }
  }
  targetDist = std::sqrt(std::max(
      closestDistanceSqr, idMath::FLT_SMALLEST_NON_DENORMAL));
  pathLength = bestLength;

  const auto addLine = [&](avoidColor_t color, const idVec2& start,
                           const idVec2& end) {
    if (debugLines == nullptr || numDebugLines >= maxDebugLines - 1) {
      return;
    }
    debugLines[numDebugLines++] =
        {color, {start.x, start.y, 0.0f}, {end.x, end.y, 0.0f}};
    debugLines[numDebugLines].color = COLOR_INVALID;
  };
  if (debugLines != nullptr) {
    if (root != nullptr) {
      addLine(COLOR_GREEN, root->pos,
              {optimizedPath[0].x, optimizedPath[0].y});
    }
    addLine(COLOR_YELLOW,
            {optimizedPath[0].x, optimizedPath[0].y},
            {optimizedPath[1].x, optimizedPath[1].y});
    addLine(COLOR_GREEN,
            {optimizedPath[0].x - 16.0f, optimizedPath[0].y},
            {optimizedPath[0].x + 16.0f, optimizedPath[0].y});
    addLine(COLOR_GREEN,
            {optimizedPath[0].x, optimizedPath[0].y - 16.0f},
            {optimizedPath[0].x, optimizedPath[0].y + 16.0f});
    addLine(COLOR_YELLOW,
            {optimizedPath[1].x - 16.0f, optimizedPath[1].y},
            {optimizedPath[1].x + 16.0f, optimizedPath[1].y});
    addLine(COLOR_YELLOW,
            {optimizedPath[1].x, optimizedPath[1].y - 16.0f},
            {optimizedPath[1].x, optimizedPath[1].y + 16.0f});
  }
}

void idObstacleAvoidanceCore::SubSamplePath(const idVec3& startPos,
                                             const idVec3& pathStart,
                                             const idVec3& pathEnd,
                                             idVec3& result) {
  const idVec3 delta{pathEnd.x - pathStart.x, pathEnd.y - pathStart.y,
                     pathEnd.z - pathStart.z};
  const float lengthSqr = delta.x * delta.x + delta.y * delta.y +
                          delta.z * delta.z;
  float step = 0.25f;
  float fraction = 0.5f;
  result = pathStart;
  for (int iteration = 0; iteration < 8; ++iteration) {
    if (step * lengthSqr <= 16.0f) {
      break;
    }
    const idVec3 point{pathStart.x + fraction * delta.x,
                       pathStart.y + fraction * delta.y,
                       pathStart.z + fraction * delta.z};
    if (LineIntersectsWall({startPos.x, startPos.y},
                           {point.x, point.y})) {
      fraction -= step;
    } else {
      result = point;
      fraction += step;
    }
    step *= 0.5f;
  }
}

bool idObstacleAvoidanceCore::FindSeekPosForRoute(
    const obstacleRoute_t* route, float obstacleRadius,
    const idVec3& startPos, idVec3& seekPos, idVec3* wallCorners,
    int& outNextRouteIndex) {
  if (expandedObstacles == nullptr) {
    return false;
  }
  const float originalSeekHeight = seekPos.z;
  const idVec3 routeEnd =
      route->areas[std::max(route->numAreas - 1, 0)].end;
  const idVec2 routeEnd2{routeEnd.x, routeEnd.y};
  const int ignoreObstacle = PointInsideObstacle(routeEnd2);
  if (LineIntersectsObstacle({startPos.x, startPos.y}, routeEnd2,
                             ignoreObstacle, ignoreObstacle)) {
    idVec3 positions[2] = {route->areas[0].end, route->areas[0].end};
    int firstRouteIndex = 1;
    for (int index = route->numAreas - 1; index >= 1; --index) {
      const idVec3& areaStart = route->areas[index].start;
      if (!LineIntersectsWall({startPos.x, startPos.y},
                              {areaStart.x, areaStart.y})) {
        SubSamplePath(startPos, areaStart, route->areas[index].end,
                      positions[0]);
        firstRouteIndex = index;
        break;
      }
    }
    positions[1] = positions[0];
    for (int index = route->numAreas - 1; index >= firstRouteIndex;
         --index) {
      const idVec3& areaStart = route->areas[index].start;
      if (LineIntersectsWall({positions[0].x, positions[0].y},
                             {areaStart.x, areaStart.y})) {
        continue;
      }
      SubSamplePath(positions[0], areaStart, route->areas[index].end,
                    positions[1]);
      const idVec3 delta{positions[1].x - positions[0].x,
                         positions[1].y - positions[0].y,
                         positions[1].z - positions[0].z};
      const float inverseLength =
          InverseLength(delta.x, delta.y, delta.z);
      if ((delta.x * delta.x + delta.y * delta.y + delta.z * delta.z) *
              inverseLength >
          0.2f) {
        positions[1].x -= delta.x * inverseLength * 0.2f;
        positions[1].y -= delta.y * inverseLength * 0.2f;
        positions[1].z -= delta.z * inverseLength * 0.2f;
      }
      outNextRouteIndex = index;
      break;
    }

    const float radius = obstacleRadius - 20.0f;
    const auto distanceSqr = [&](const idVec3& point) {
      const float dx = point.x - startPos.x;
      const float dy = point.y - startPos.y;
      const float dz = point.z - startPos.z;
      return dx * dx + dy * dy + dz * dz;
    };
    const float firstDistanceSqr = distanceSqr(positions[0]);
    const float secondDistanceSqr = distanceSqr(positions[1]);
    if (firstDistanceSqr <= radius * radius) {
      if (secondDistanceSqr <= radius * radius) {
        seekPos = positions[1];
      } else {
        const float fraction =
            (radius - std::sqrt(std::max(
                          firstDistanceSqr,
                          idMath::FLT_SMALLEST_NON_DENORMAL))) /
            std::sqrt(std::max(secondDistanceSqr,
                               idMath::FLT_SMALLEST_NON_DENORMAL));
        seekPos = {positions[0].x +
                       (positions[1].x - positions[0].x) * fraction,
                   positions[0].y +
                       (positions[1].y - positions[0].y) * fraction,
                   positions[0].z +
                       (positions[1].z - positions[0].z) * fraction};
      }
    } else {
      const float fraction =
          radius /
          std::sqrt(std::max(firstDistanceSqr,
                             idMath::FLT_SMALLEST_NON_DENORMAL));
      seekPos = {startPos.x + (positions[0].x - startPos.x) * fraction,
                 startPos.y + (positions[0].y - startPos.y) * fraction,
                 startPos.z + (positions[0].z - startPos.z) * fraction};
    }
    seekPos.z = originalSeekHeight;
    wallCorners[0] = positions[0];
    wallCorners[1] = positions[1];
    return false;
  }

  seekPos = {routeEnd.x, routeEnd.y, originalSeekHeight};
  wallCorners[0] = routeEnd;
  wallCorners[1] = routeEnd;
  if (ignoreObstacle == -1) {
    return true;
  }
  const int id = expandedObstacles[ignoreObstacle].id;
  return id == -1 || id == -3;
}

bool idObstacleAvoidanceCore::PathAroundCircle(
    const idVec2& startPos, const idVec2& corner, const idVec2& endPos,
    const idVec2* circlePoints, bool clockwise, idVec2* path,
    int& numPathPoints) {
  bool visibleFromStart[16];
  bool visibleFromEnd[16];
  for (int index = 0; index < 16; ++index) {
    const int next = (index + 1) & 15;
    const float normalX = circlePoints[next].y - circlePoints[index].y;
    const float normalY = circlePoints[index].x - circlePoints[next].x;
    visibleFromStart[index] =
        (circlePoints[index].y - startPos.y) * normalY +
            (circlePoints[index].x - startPos.x) * normalX >
        0.0f;
    visibleFromEnd[index] =
        (circlePoints[index].y - endPos.y) * normalY +
            (circlePoints[index].x - endPos.x) * normalX >
        0.0f;
  }
  int count = 1;
  path[0] = startPos;
  if (clockwise) {
    int tangent = 0;
    while (tangent < 16 &&
           !(visibleFromStart[tangent] &&
             !visibleFromStart[(tangent + 1) & 15])) {
      ++tangent;
    }
    if (tangent < 16) {
      for (int index = tangent + 1; index - tangent - 1 < 16; ++index) {
        path[count++] = circlePoints[index & 15];
        if (visibleFromEnd[index & 15]) {
          break;
        }
      }
    }
  } else {
    int tangent = 15;
    while (tangent >= 0 &&
           !(visibleFromStart[tangent] &&
             !visibleFromStart[(tangent - 1) & 15])) {
      --tangent;
    }
    if (tangent >= 0) {
      for (int offset = 0; offset < 16; ++offset) {
        const int index = tangent - offset;
        path[count++] = circlePoints[index & 15];
        if (visibleFromEnd[(index - 1) & 15]) {
          break;
        }
      }
    }
  }
  path[count++] = endPos;
  numPathPoints = count;
  for (int index = 0; index < count - 1; ++index) {
    idVec2 delta{path[index + 1].x - path[index].x,
                 path[index + 1].y - path[index].y};
    float blockingScale;
    int blockingObstacle;
    int blockingEdge;
    if (!GetFirstBlockingObstacle(path[index], delta, -1, blockingScale,
                                  blockingObstacle, blockingEdge)) {
      continue;
    }
    if (index < count - 2) {
      return false;
    }
    const float remainingX = delta.x - delta.x * blockingScale;
    const float remainingY = delta.y - delta.y * blockingScale;
    if (remainingX * remainingX + remainingY * remainingY < 1.0f) {
      return true;
    }
    path[index + 1] = {corner.x + (endPos.x - corner.x) * 0.5f,
                       corner.y + (endPos.y - corner.y) * 0.5f};
    delta = {path[index + 1].x - path[index].x,
             path[index + 1].y - path[index].y};
    if (GetFirstBlockingObstacle(path[index], delta, -1, blockingScale,
                                 blockingObstacle, blockingEdge)) {
      return false;
    }
  }
  return true;
}

void idObstacleAvoidanceCore::CircleCorners(obstaclePath_t* path,
                                             const idVec4& lastCorner,
                                             const idBounds& bbox,
                                             float frameMoveDist,
                                             float cornerCircleRadius) {
  (void)bbox;
  path->nextCorner = {0.0f, 0.0f, 0.0f, 0.0f};
  path->nextCornerAngle = 0.0f;
  if (path->numSeekPos < 2) {
    return;
  }
  const idVec2 pathStart{path->startPosOutsideObstacles.x,
                         path->startPosOutsideObstacles.y};
  const idVec2 corner{path->seekPos[0].x, path->seekPos[0].y};
  const idVec2 nextPoint{path->seekPos[1].x, path->seekPos[1].y};
  const idVec2 incoming{corner.x - pathStart.x,
                        corner.y - pathStart.y};
  const idVec2 outgoing{corner.x - nextPoint.x,
                        corner.y - nextPoint.y};
  const float incomingInv = InverseLength(incoming.x, incoming.y);
  const float outgoingInv = InverseLength(outgoing.x, outgoing.y);
  const float incomingLength =
      (incoming.x * incoming.x + incoming.y * incoming.y) * incomingInv;
  const float outgoingLength =
      (outgoing.x * outgoing.x + outgoing.y * outgoing.y) * outgoingInv;
  const idVec2 incomingDirection{incoming.x * incomingInv,
                                 incoming.y * incomingInv};
  const idVec2 outgoingDirection{outgoing.x * outgoingInv,
                                 outgoing.y * outgoingInv};

  idVec2 currentCurve[48];
  currentCurve[0] = pathStart;
  currentCurve[1] = corner;
  int currentCount = 2;
  idVec2 currentEnd = nextPoint;
  idVec2 continued = nextPoint;
  if (std::min(incomingLength, outgoingLength) * 0.9f >
      cornerCircleRadius * 0.25f) {
    const bool clockwise =
        outgoingDirection.x * incomingDirection.y -
            outgoingDirection.y * incomingDirection.x <
        0.0f;
    float radius = cornerCircleRadius;
    bool found = false;
    idVec2 circlePoints[16];
    for (int attempt = 0; attempt < 3 && !found; ++attempt) {
      GetCirclePoints(corner, radius, circlePoints);
      currentEnd = {nextPoint.x + outgoingDirection.x,
                    nextPoint.y + outgoingDirection.y};
      found = PathAroundCircle(pathStart, corner, currentEnd, circlePoints,
                               clockwise, currentCurve, currentCount);
      radius *= 0.4f;
    }
    if (found) {
      const float signedRadius = clockwise ? radius : -radius;
      path->nextCorner = {corner.x, corner.y, path->seekPos[0].z,
                          signedRadius};
      path->nextCornerAngle =
          std::max((static_cast<float>(currentCount) - 3.0f) * 22.5f,
                   0.0f);
      GetSeekPointsOnPath(currentCurve, currentCount, corner, signedRadius,
                          frameMoveDist, path->seekPos, path->numSeekPos,
                          false);
      continued = currentCurve[1];
    } else {
      currentCount = 0;
      continued = corner;
    }
  }

  int lastCount = 0;
  idVec2 lastCurve[19];
  if (lastCorner.w != 0.0f &&
      (std::fabs(lastCorner.x - corner.x) > 0.1f ||
       std::fabs(lastCorner.y - corner.y) > 0.1f) &&
      (lastCorner.y - corner.y) * (corner.y - pathStart.y) +
              (lastCorner.x - corner.x) * (corner.x - pathStart.x) <
          0.0f) {
    float radius = std::fabs(lastCorner.w);
    const float firstDistanceSqr =
        (pathStart.x - lastCorner.x) * (pathStart.x - lastCorner.x) +
        (pathStart.y - lastCorner.y) * (pathStart.y - lastCorner.y);
    const float secondDistanceSqr =
        (continued.x - lastCorner.x) * (continued.x - lastCorner.x) +
        (continued.y - lastCorner.y) * (continued.y - lastCorner.y);
    radius = std::min(
        radius,
        std::sqrt(std::max(std::min(firstDistanceSqr, secondDistanceSqr),
                           idMath::FLT_SMALLEST_NON_DENORMAL)));
    idVec2 circlePoints[16];
    GetCirclePoints({lastCorner.x, lastCorner.y}, radius, circlePoints);
    const idVec2 ray{continued.x - pathStart.x,
                     continued.y - pathStart.y};
    float scale1;
    float scale2;
    if (WindingRayIntersection(circlePoints, 16, pathStart, ray, scale1,
                               scale2, nullptr) &&
        (scale1 > 0.0f || scale2 > 0.0f) &&
        PathAroundCircle(pathStart, {lastCorner.x, lastCorner.y}, continued,
                         circlePoints, lastCorner.w > 0.0f, lastCurve,
                         lastCount)) {
      path->nextCorner = {lastCorner.x, lastCorner.y, lastCorner.z,
                          lastCorner.w};
      path->nextCornerAngle =
          std::max((static_cast<float>(lastCount) - 3.0f) * 22.5f,
                   0.0f);
      GetSeekPointsOnPath(lastCurve, lastCount,
                          {lastCorner.x, lastCorner.y}, lastCorner.w,
                          frameMoveDist, path->seekPos, path->numSeekPos,
                          false);
    } else {
      lastCount = 0;
    }
  }

  if (debugLines != nullptr) {
    const auto drawCurve = [&](const idVec2* curve, int count,
                               avoidColor_t color) {
      for (int index = 0; index + 1 < count; ++index) {
        if (numDebugLines >= maxDebugLines - 1) {
          return;
        }
        debugLines[numDebugLines++] =
            {color, {curve[index].x, curve[index].y, 0.0f},
             {curve[index + 1].x, curve[index + 1].y, 0.0f}};
        debugLines[numDebugLines].color = COLOR_INVALID;
      }
    };
    if (lastCount != 0 && currentCount != 0) {
      idVec2 combined[48];
      int count = 0;
      for (int index = 0; index < lastCount; ++index) {
        combined[count++] = lastCurve[index];
      }
      for (int index = 2; index < currentCount; ++index) {
        combined[count++] = currentCurve[index];
      }
      drawCurve(combined, count, COLOR_CYAN);
    } else if (lastCount != 0) {
      drawCurve(lastCurve, lastCount, COLOR_CYAN);
    } else if (currentCount != 0) {
      drawCurve(currentCurve, currentCount, COLOR_CYAN);
    }
  }
}

void idObstacleAvoidanceCore::CircleApproachDestination(
    obstaclePath_t* path, const obstacleRoute_t* route,
    float frameMoveDist) {
  const idVec2 align{route->endAlignDir.x, route->endAlignDir.y};
  const idVec2 destination{
      route->areas[std::max(route->numAreas - 1, 0)].end.x,
      route->areas[std::max(route->numAreas - 1, 0)].end.y};
  idVec2 start{path->startPosOutsideObstacles.x,
               path->startPosOutsideObstacles.y};
  const float signedRadiusMagnitude = route->endTurnRadius + 1.0f;
  float signedRadius = signedRadiusMagnitude;
  if ((destination.y - start.y) * align.x +
          (destination.x - start.x) * -align.y >
      0.0f) {
    signedRadius = -signedRadiusMagnitude;
  }
  const idVec2 center{destination.x - align.y * signedRadius,
                      destination.y + align.x * signedRadius};
  const float dx = start.x - center.x;
  const float dy = start.y - center.y;
  const float inverseDistance = InverseLength(dx, dy);
  const float distance = (dx * dx + dy * dy) * inverseDistance;
  if (distance > signedRadiusMagnitude * 0.75f &&
      signedRadiusMagnitude > distance) {
    start = {center.x + dx * inverseDistance * signedRadiusMagnitude,
             center.y + dy * inverseDistance * signedRadiusMagnitude};
  }
  idVec2 circlePoints[16];
  GetCirclePoints(center, route->endTurnRadius, circlePoints);
  idVec2 curvePoints[48];
  int numPoints = 2;
  const bool clockwise = signedRadius > 0.0f;
  if (PathAroundCircle(start, center, destination, circlePoints, clockwise,
                       curvePoints, numPoints)) {
    GetSeekPointsOnPath(curvePoints, numPoints, center,
                        route->endTurnRadius, frameMoveDist, path->seekPos,
                        path->numSeekPos, false);
    if (debugLines != nullptr) {
      for (int index = 0; index + 1 < numPoints; ++index) {
        if (numDebugLines >= maxDebugLines - 1) {
          break;
        }
        debugLines[numDebugLines++] =
            {COLOR_CYAN,
             {curvePoints[index].x, curvePoints[index].y, 0.0f},
             {curvePoints[index + 1].x, curvePoints[index + 1].y,
              0.0f}};
        debugLines[numDebugLines].color = COLOR_INVALID;
      }
    }
  }
}

void idObstacleAvoidanceCore::UpdateRadar(obstaclePath_t* path,
                                           float frameMoveDist) {
  float forwardX;
  float forwardY;
  float forwardDistance;
  if (path->useRadarForward) {
    forwardX = path->radarForward.x;
    forwardY = path->radarForward.y;
    forwardDistance = frameMoveDist;
  } else {
    const float dx = path->seekPos[0].x -
                     path->startPosOutsideObstacles.x;
    const float dy = path->seekPos[0].y -
                     path->startPosOutsideObstacles.y;
    const float inverseLength = InverseLength(dx, dy);
    forwardX = dx * inverseLength;
    forwardY = dy * inverseLength;
    forwardDistance = (dx * dx + dy * dy) * inverseLength;
  }
  if (forwardDistance <= 0.0f) {
    forwardX = 0.0f;
    forwardY = 1.0f;
  }
  const float x = forwardX * 256.0f;
  const float y = forwardY * 256.0f;
  path->radar[0].direction = {-x, -y, -0.0f};
  path->radar[1].direction = {-y, x, 0.0f};
  path->radar[2].direction = {y, -x, -0.0f};
  path->radar[3].direction = {x - y, y + x, 0.0f};
  path->radar[4].direction = {x + y, y - x, 0.0f};
  path->radar[5].direction = {-y - x, x - y, 0.0f};
  path->radar[6].direction = {-x + y, -y - x, -0.0f};
  idVec2 start{path->startPosOutsideObstacles.x,
               path->startPosOutsideObstacles.y};
  const auto traceRadar = [&](int index, const idVec2& rayStart) {
    idVec2 delta{path->radar[index].direction.x,
                 path->radar[index].direction.y};
    float scale;
    int obstacle;
    int edge;
    if (GetFirstBlockingObstacle(rayStart, delta, -1, scale, obstacle,
                                 edge)) {
      path->radar[index].direction.x *= scale;
      path->radar[index].direction.y *= scale;
      path->radar[index].obstacle = expandedObstacles[obstacle].id;
    } else {
      path->radar[index].obstacle = -1;
    }
  };
  for (int index = 0; index < 7; ++index) {
    traceRadar(index, start);
  }
  const float advance = std::min(forwardDistance, frameMoveDist);
  start.x += advance * forwardX;
  start.y += advance * forwardY;
  traceRadar(1, start);
  traceRadar(2, start);
  if (debugLines != nullptr) {
    const idVec2 origin{path->startPosOutsideObstacles.x,
                        path->startPosOutsideObstacles.y};
    for (int index = 0; index < 7; ++index) {
      if (numDebugLines >= maxDebugLines - 1) {
        break;
      }
      debugLines[numDebugLines++] =
          {COLOR_ORANGE,
           {origin.x, origin.y, 0.0f},
           {origin.x + path->radar[index].direction.x,
            origin.y + path->radar[index].direction.y, 0.0f}};
      debugLines[numDebugLines].color = COLOR_INVALID;
    }
  }
}

void idObstacleAvoidanceCore::FindPathAroundObstacles(
    obstaclePath_t* path, avoidDebugLine_t* outputDebugLines,
    int outputMaxDebugLines, avoidDebugText_t* outputDebugText,
    int outputMaxDebugText, void* tempBuffer, int tempBufferSize,
    const idBounds& bbox, const idVec3& gravity, const idVec3& lastDir,
    const idVec4& lastCorner, float obstacleRadius, float frameMoveDist,
    float cornerCircleRadius, int inputFlags,
    const obstacleRoute_t* inputRoute, const obstacleBox_t* inputObstacles,
    int numObstacles, const obstacleVertex_t* inputVertices,
    int numVertices, const obstacleEdge_t* edges, int numEdges,
    const obstacleCorner_t* inputCorners, int numCorners,
    const obstacleTrace_t* inputTraces, int numTraces,
    float* traceFractions) {
  flags = inputFlags;
  auto* const bytes = static_cast<std::uint8_t*>(tempBuffer);
  tempMemorySize = tempBufferSize - 32;
  tempMemory = bytes + 16;
  tempMemoryStart = bytes + 16;
  tempMemoryEnd = bytes + tempBufferSize - 16;
  for (int index = 0; index < 16; ++index) {
    bytes[index] = static_cast<std::uint8_t>(index);
    bytes[tempBufferSize - 16 + index] = static_cast<std::uint8_t>(index);
  }

  int numUsableEdges = 0;
  for (int index = 0; index < numEdges; ++index) {
    numUsableEdges += (edges[index].flags & 0x200) == 0;
  }
  debugLines = outputDebugLines;
  maxDebugLines = outputMaxDebugLines;
  maxExpandedObstacles =
      numUsableEdges + numObstacles + numCorners + 16;
  maxWindingPoints =
      10 * numObstacles +
      4 * (numUsableEdges + 4 * numCorners + 16);
  bspNodes = reinterpret_cast<bspNode_t*>(tempMemoryStart);
  numBspNodes = 0;
  obstacleIndices = reinterpret_cast<std::int16_t*>(tempMemoryStart + 496);
  expandedObstacles = reinterpret_cast<expandedObstacle_t*>(
      tempMemoryStart + 496 +
      ((sizeof(std::int16_t) *
            static_cast<std::size_t>(maxExpandedObstacles) +
        15) &
       ~std::size_t(15)));
  windingPoints = reinterpret_cast<idVec2*>(
      reinterpret_cast<std::uint8_t*>(expandedObstacles) +
      ((sizeof(expandedObstacle_t) *
            static_cast<std::size_t>(maxExpandedObstacles) +
        15) &
       ~std::size_t(15)));
  tempMemoryStart = reinterpret_cast<std::uint8_t*>(windingPoints) +
                    ((sizeof(idVec2) *
                          static_cast<std::size_t>(maxWindingPoints) +
                      15) &
                     ~std::size_t(15));
  numExpandedObstacles = 0;
  numWindingPoints = 0;
  pathNodes = nullptr;
  maxPathNodes = 0;
  numPathNodes = 0;
  debugText = outputDebugText;
  numDebugLines = 0;
  maxDebugText = outputMaxDebugText;
  numDebugText = 0;
  if (debugLines != nullptr) {
    debugLines[0].color = COLOR_INVALID;
  }
  if (debugText != nullptr) {
    debugText[0].color = COLOR_INVALID;
  }

  obstacleRoute_t* const route = const_cast<obstacleRoute_t*>(inputRoute);
  obstacleBox_t* const obstacles =
      const_cast<obstacleBox_t*>(inputObstacles);
  obstacleVertex_t* const vertices =
      const_cast<obstacleVertex_t*>(inputVertices);
  obstacleCorner_t* const corners =
      const_cast<obstacleCorner_t*>(inputCorners);
  obstacleTrace_t* const traces =
      const_cast<obstacleTrace_t*>(inputTraces);
  idVec3 localLastDir = lastDir;
  idVec4 localLastCorner = lastCorner;
  TransformInput(gravity, route, localLastDir, localLastCorner, obstacles,
                 numObstacles, vertices, numVertices, corners, numCorners,
                 traces, numTraces);

  const idVec3 originalStart = route->areas[0].start;
  const idVec3 originalSeek = route->areas[0].end;
  idVec3 cappedSeek = originalSeek;
  const idVec3 originalDelta{originalSeek.x - originalStart.x,
                             originalSeek.y - originalStart.y,
                             originalSeek.z - originalStart.z};
  const float deltaLengthSqr =
      originalDelta.y * originalDelta.y + originalDelta.x * originalDelta.x +
      originalDelta.z * originalDelta.z;
  const float deltaInvLength =
      InverseLength(originalDelta.x, originalDelta.y, originalDelta.z);
  if (deltaInvLength * deltaLengthSqr > obstacleRadius) {
    cappedSeek = {
        ((originalDelta.x * deltaInvLength) * obstacleRadius) * 0.9f +
            originalStart.x,
        ((originalDelta.y * deltaInvLength) * obstacleRadius) * 0.9f +
            originalStart.y,
        ((originalDelta.z * deltaInvLength) * obstacleRadius) * 0.9f +
            originalStart.z};
  }

  path->seekPos[0] = cappedSeek;
  path->seekPos[1] = cappedSeek;
  path->numSeekPos = 1;
  path->seekPosPlane = {0.0f, 0.0f, 0.0f, 0.0f};
  path->firstObstaclePosition = {0.0f, 0.0f, 0.0f};
  path->firstObstacle = -1;
  path->startPosOutsideObstacles = originalStart;
  path->startPosObstacle = -1;
  path->seekPosOutsideObstacles = cappedSeek;
  path->seekPosObstacle = -1;
  path->targetDist = 1.0e30f;
  path->pathLength = 1.0e30f;
  path->nextCorner = {0.0f, 0.0f, 0.0f, 0.0f};
  path->nextCornerAngle = 0.0f;
  path->wallCorners[0] = cappedSeek;
  path->wallCorners[1] = cappedSeek;
  path->hasValidPath = true;
  path->startPosValid = true;
  path->seekPosValid = true;
  for (obstaclePath_t::radar_t& radar : path->radar) {
    radar.direction = {0.0f, 0.0f, 0.0f};
    radar.obstacle = -1;
  }

  if ((inputFlags & 0x40) != 0) {
    std::printf(
        "(PPU):idObstacleAvoidanceCore::FindPathAroundObstacles: startPos = ( %.2f %.2f %.2f ) seekPos = ( %.2f %.2f %.2f )\n",
        originalStart.x, originalStart.y, originalStart.z, originalSeek.x,
        originalSeek.y, originalSeek.z);
  }
  const bool needsAvoidance = SetupExpandedObstacles(
      bbox, gravity, path->startPosOutsideObstacles,
      path->seekPosOutsideObstacles, obstacleRadius, (inputFlags & 1) != 0,
      obstacles, numObstacles, vertices, numVertices, edges, numEdges,
      corners, numCorners, numTraces > 0);

  for (int index = 0; index < numTraces; ++index) {
    traceFractions[index] =
        LineIntersectsObstacle(traces[index].start, traces[index].end, 0, 0)
            ? 0.0f
            : 1.0f;
  }
  if (!needsAvoidance) {
    bspNodes = nullptr;
    path->hasValidPath = true;
    if ((inputFlags & 8) != 0) {
      UpdateRadar(path, frameMoveDist);
    }
    UnTransformOutput(path);
    return;
  }

  int startObstacle = -1;
  path->startPosValid = GetPointOutsideObstacles(
      *reinterpret_cast<idVec2*>(&path->startPosOutsideObstacles),
      &startObstacle, nullptr,
      *reinterpret_cast<const idVec2*>(&path->startPosOutsideObstacles),
      false);
  if (startObstacle != -1) {
    path->startPosObstacle = expandedObstacles[startObstacle].id;
  }

  int nextRouteIndex = route->numAreas - 1;
  const bool directRoute = FindSeekPosForRoute(
      route, obstacleRadius, path->startPosOutsideObstacles, cappedSeek,
      path->wallCorners, nextRouteIndex);
  path->seekPos[0] = cappedSeek;
  path->seekPos[1] = cappedSeek;
  path->numSeekPos = 1;
  path->seekPosOutsideObstacles = cappedSeek;
  const float cappedDx = cappedSeek.x - originalStart.x;
  const float cappedDy = cappedSeek.y - originalStart.y;
  const float cappedDz = cappedSeek.z - originalStart.z;
  if (cappedDx * cappedDx + cappedDy * cappedDy + cappedDz * cappedDz <
          0.010000001f ||
      directRoute) {
    bspNodes = nullptr;
    path->hasValidPath = true;
    if ((inputFlags & 8) != 0) {
      UpdateRadar(path, frameMoveDist);
    }
    UnTransformOutput(path);
    return;
  }

  int seekObstacle = -1;
  path->seekPosValid = GetPointOutsideObstacles(
      *reinterpret_cast<idVec2*>(&path->seekPosOutsideObstacles),
      &seekObstacle, nullptr,
      *reinterpret_cast<const idVec2*>(&path->startPosOutsideObstacles),
      (inputFlags & 2) != 0);
  if (seekObstacle != -1) {
    path->seekPosObstacle = expandedObstacles[seekObstacle].id;
  }
  path->seekPos[1] = path->seekPosOutsideObstacles;
  path->numSeekPos = 2;

  const float movedDx = path->seekPosOutsideObstacles.x -
                        path->startPosOutsideObstacles.x;
  const float movedDy = path->seekPosOutsideObstacles.y -
                        path->startPosOutsideObstacles.y;
  if ((startObstacle == -1 && seekObstacle == -1) ||
      movedDx * movedDx + movedDy * movedDy >= 1.0f) {
    if (tempMemory != nullptr) {
      tempMemoryEnd = tempMemory + tempMemorySize;
    }
    pathNodes = reinterpret_cast<pathNode_t*>(
        windingPoints + numWindingPoints);
    maxPathNodes = static_cast<int>(
        (tempMemoryEnd - reinterpret_cast<std::uint8_t*>(pathNodes)) /
        sizeof(pathNode_t));
    pathNode_t* root = nullptr;
    for (int attempt = 0; attempt < 2; ++attempt) {
      numPathNodes = 0;
      root = BuildPathTree(path->startPosOutsideObstacles,
                           path->seekPosOutsideObstacles, obstacleRadius,
                           path);
      PrunePathTree(root, *reinterpret_cast<const idVec2*>(
                              &path->seekPosOutsideObstacles));
      FindOptimalPath(root, {cappedSeek.x, cappedSeek.y},
                      {localLastDir.x, localLastDir.y}, path->seekPos,
                      path->numSeekPos, path->targetDist,
                      path->pathLength);
      path->hasValidPath = path->targetDist < 1.0f;
      if (path->hasValidPath || numCorners <= 0) {
        break;
      }
      numExpandedObstacles -= numCorners;
    }
    path->wallCorners[0] = path->seekPos[0];
    path->wallCorners[1] = path->seekPos[1];

    if ((inputFlags & 0x20) != 0 && debugLines != nullptr) {
      for (pathNode_t* node = root; node != nullptr;) {
        pathNode_t* child = node->children[0] != nullptr
                                ? node->children[0]
                                : node->children[1];
        if (child != nullptr && numDebugLines < maxDebugLines - 1) {
          const avoidColor_t color =
              node->edgeNum == -1
                  ? COLOR_GREEN
                  : (node->children[0] != nullptr ? COLOR_ORANGE
                                                   : COLOR_BLUE);
          debugLines[numDebugLines++] =
              {color, {node->pos.x, node->pos.y, 0.0f},
               {child->pos.x, child->pos.y, 0.0f}};
          debugLines[numDebugLines].color = COLOR_INVALID;
        }
        node = node->queueNext >= 0 ? &pathNodes[node->queueNext] : nullptr;
      }
    }
    if (root != nullptr) {
      const pathNode_t* first = root->children[0] != nullptr
                                    ? root->children[0]
                                    : root->children[1];
      if (first == nullptr) {
        first = root;
      }
      path->firstObstaclePosition = {first->pos.x, first->pos.y,
                                     originalStart.z};
    }
    if ((inputFlags & 4) != 0) {
      CircleCorners(path, localLastCorner, bbox, frameMoveDist,
                    cornerCircleRadius);
    }
    if (route->endTurnRadius > 0.0f) {
      CircleApproachDestination(path, route, frameMoveDist);
    }
    for (int index = nextRouteIndex;
         index < route->numAreas && path->numSeekPos < 30; ++index) {
      path->seekPos[path->numSeekPos++] = route->areas[index].end;
    }
    if ((inputFlags & 8) != 0) {
      UpdateRadar(path, frameMoveDist);
    }

    const idVec3 firstDelta{path->seekPos[0].x - originalStart.x,
                            path->seekPos[0].y - originalStart.y,
                            path->seekPos[0].z - originalStart.z};
    const idVec3 secondDelta{
        path->seekPos[1].x - path->seekPos[0].x,
        path->seekPos[1].y - path->seekPos[0].y,
        path->seekPos[1].z - path->seekPos[0].z};
    const float firstInv =
        InverseLength(firstDelta.x, firstDelta.y, firstDelta.z);
    const float secondInv =
        InverseLength(secondDelta.x, secondDelta.y, secondDelta.z);
    idVec3 planeNormal{firstDelta.x * firstInv + secondDelta.x * secondInv,
                       firstDelta.y * firstInv + secondDelta.y * secondInv,
                       0.0f};
    const float planeInv =
        InverseLength(planeNormal.x, planeNormal.y, planeNormal.z);
    planeNormal.x *= planeInv;
    planeNormal.y *= planeInv;
    planeNormal.z *= planeInv;
    FixDegenerateNormalExact(planeNormal);
    path->seekPosPlane = {
        planeNormal.x, planeNormal.y, planeNormal.z,
        -(path->seekPos[0].x * planeNormal.x +
          path->seekPos[0].y * planeNormal.y +
          path->seekPos[0].z * planeNormal.z)};
    UnTransformOutput(path);
    return;
  }

  if (startObstacle != -1) {
    path->startPosObstacle = expandedObstacles[startObstacle].id;
  }
  if (seekObstacle != -1) {
    path->seekPosObstacle = expandedObstacles[seekObstacle].id;
  }
  path->firstObstacle = path->startPosObstacle != -1
                            ? path->startPosObstacle
                            : path->seekPosObstacle;
  if (movedDx * movedDx + movedDy * movedDy <= 4.0f) {
    path->seekPos[0] = path->seekPosOutsideObstacles;
    path->seekPos[1] = path->seekPosOutsideObstacles;
    path->numSeekPos = 1;
    path->hasValidPath = true;
    if ((inputFlags & 8) != 0) {
      UpdateRadar(path, frameMoveDist);
    }
    UnTransformOutput(path);
    return;
  }
  path->seekPos[0] = path->startPosOutsideObstacles;
  path->seekPos[1] = path->startPosOutsideObstacles;
  path->hasValidPath = false;
}
