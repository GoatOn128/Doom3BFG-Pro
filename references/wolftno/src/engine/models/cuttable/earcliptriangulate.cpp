#include "engine/models/cuttable/earcliptriangulate.h"

#include "shared/idlib/lib.h"
#include "shared/idlib/containers/staticlist.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>

namespace {

constexpr float kPointEpsilon = 1.192092896e-7f;

float Orientation(const idVec2& previous, const idVec2& current,
                  const idVec2& next) {
  return (next.y - previous.y) * (current.x - previous.x) -
         (current.y - previous.y) * (next.x - previous.x);
}

bool SamePoint(const idVec2& lhs, const idVec2& rhs) {
  return !(std::fabs(lhs.x - rhs.x) > kPointEpsilon ||
           std::fabs(lhs.y - rhs.y) > kPointEpsilon);
}

bool IsConvexCorner(const idVec2& previous, const idVec2& current,
                    const idVec2& next) {
  return !(Orientation(previous, current, next) < 0.0f);
}

void NormalVectors(const idVec3& normal, idVec3& left, idVec3& down) {
  const float horizontalLengthSquared =
      normal.x * normal.x + normal.y * normal.y;
  if (horizontalLengthSquared == 0.0f) {
    left = {1.0f, 0.0f, 0.0f};
  } else {
    const float inverseLength =
        1.0f / std::sqrt(std::max(horizontalLengthSquared,
                                  std::numeric_limits<float>::min()));
    left = {-inverseLength * normal.y, inverseLength * normal.x, 0.0f};
  }
  down = left.Cross(normal);
}

} // namespace

bool idEarClipTriangulate::IsClockwise(const idList<idVec2>& points) {
  float area = 0.0f;
  int previous = points.Num() - 1;
  for (int current = 0; current < points.Num(); ++current) {
    area += points[current].y * points[previous].x -
            points[previous].y * points[current].x;
    previous = current;
  }
  return area < 0.0f;
}

void idEarClipTriangulate::RemapIndices() {
  maxIndex = -1;
  for (int i = 0; i < triangles.Num(); ++i) {
    const int mapped = indexMap.hashIndex.First(triangles[i]);
    if (mapped != -1)
      triangles[i] = indexMap.indices[mapped];
    maxIndex = std::max(maxIndex, static_cast<int>(triangles[i]));
  }
}

bool idEarClipTriangulate::RemoveReflex(int index) {
  if (headReflex == -1 || tailReflex == -1) {
    idLib::Warning("Reflex vertices must exists!");
    return false;
  }

  vertex_t& vertex = vertices[index];
  if (index == headReflex) {
    headReflex = vertex.nextShared.reflex;
    if (headReflex != -1)
      vertices[headReflex].prevShared.reflex = -1;
    vertex.nextShared.reflex = -1;
    return true;
  }

  if (index == tailReflex) {
    tailReflex = vertex.prevShared.reflex;
    if (tailReflex != -1) {
      vertices[tailReflex].nextShared.reflex = -1;
      vertex.prevShared.reflex = -1;
      return true;
    }
  } else {
    const int next = vertex.nextShared.reflex;
    const int previous = vertex.prevShared.reflex;
    if (next == -1 || previous == -1) {
      idLib::Warning("Very strange reflex vertex.");
      return false;
    }
    vertices[previous].nextShared.reflex = static_cast<std::int16_t>(next);
    vertices[next].prevShared.reflex = static_cast<std::int16_t>(previous);
    vertex.nextShared.reflex = -1;
  }

  vertex.prevShared.reflex = -1;
  return true;
}

void idEarClipTriangulate::Reset() {
  indices.ClearFree();
  vertices.ClearFree();
  triangles.ClearFree();
  headConvex = -1;
  tailConvex = -1;
  headReflex = -1;
  tailReflex = -1;
  headEar = -1;
  tailEar = -1;
  indexMap.hashIndex.InternalFree();
  indexMap.indices.ClearFree();
}

int idEarClipTriangulate::TriangleQuery(const idVec2& point, const idVec2& v0,
                                        const idVec2& v1,
                                        const idVec2& v2) const {
  const float edge0 = (v2.y - v1.y) * (point.x - v1.x) -
                      (point.y - v1.y) * (v2.x - v1.x);
  if (edge0 > 0.0f)
    return 1;
  const int sign0 = edge0 < 0.0f ? -1 : 0;

  const float edge1 = (v2.y - v0.y) * (point.x - v0.x) -
                      (point.y - v0.y) * (v2.x - v0.x);
  int sign1;
  if (edge1 <= 0.0f) {
    if (edge1 < 0.0f)
      return 1;
    sign1 = 0;
  } else {
    sign1 = 1;
  }

  const float edge2 = (v1.y - v0.y) * (point.x - v0.x) -
                      (point.y - v0.y) * (v1.x - v0.x);
  if (edge2 > 0.0f)
    return 1;
  const int sign2 = edge2 < 0.0f ? -1 : 0;

  return sign0 != 0 && sign1 != 0 && sign2 != 0 ? -1 : 0;
}

idEarClipTriangulate::IndexMap::IndexMap() : hashIndex(), indices() {
  hashIndex.memTag = 0x4E;
}

idEarClipTriangulate::IndexMap::~IndexMap() = default;

bool idEarClipTriangulate::UpdateEar(int vertexIndex) {
  vertex_t& vertex = vertices[vertexIndex];
  vertex.isEar = -1;
  if (headReflex == -1)
    return vertex.isEar != 0;

  const int previousVertex = vertex.prevVertex;
  const int nextVertex = vertex.nextVertex;
  const idVec2& previous = positions[vertices[previousVertex].index];
  const idVec2& current = positions[vertex.index];
  const idVec2& next = positions[vertices[nextVertex].index];

  for (int candidate = headReflex; candidate != -1;
       candidate = vertices[candidate].nextShared.reflex) {
    if (candidate == previousVertex || candidate == vertexIndex ||
        candidate == nextVertex)
      continue;

    const idVec2& point = positions[vertices[candidate].index];
    if (SamePoint(point, previous) || SamePoint(point, current) ||
        SamePoint(point, next))
      continue;

    if (TriangleQuery(point, previous, current, next) <= 0) {
      vertex.isEar = 0;
      return false;
    }
  }

  return vertex.isEar != 0;
}

idEarClipTriangulate::idEarClipTriangulate()
    : positions(), vertices(), contours(), indices(), triangles(), indexMap() {}

void idEarClipTriangulate::BuildGeometry(
    const idPlane& plane, idList<idVec2>& outputPositions,
    idList<std::uint16_t>& outputIndices) {
  idVec3 left;
  idVec3 down;
  NormalVectors({plane.a, plane.b, plane.c}, left, down);

  const int numPositions = maxIndex + 1;
  if (numPositions > outputPositions.size)
    outputPositions.ResizeForPort(numPositions);
  for (int i = 0; i <= maxIndex; ++i)
    outputPositions.Append(positions[i]);

  if (triangles.Num() > outputIndices.size)
    outputIndices.ResizeForPort(triangles.Num());
  for (int i = 0; i < triangles.Num(); ++i)
    outputIndices.Append(static_cast<std::uint16_t>(triangles[i]));
}

idEarClipTriangulate::~idEarClipTriangulate() {
  for (int i = 0; i < contours.Num(); ++i)
    delete contours[i];
}

void idEarClipTriangulate::SetOuterFromPoints(
    const idList<idVec2>& points) {
  if (contours.Num() <= 0)
    contours.Append(nullptr);
  if (contours[0] == nullptr)
    contours[0] = new contour_t;

  contour_t& contour = *contours[0];
  contour.indices.ClearFree();
  if (points.Num() > contour.indices.size)
    contour.indices.ResizeForPort(points.Num());
  float valueMax = -1.0e30f;
  std::int16_t indexMax = -1;

  if (IsClockwise(points)) {
    for (int i = points.Num() - 1; i >= 0; --i) {
      const std::int16_t positionIndex =
          static_cast<std::int16_t>(positions.Append(points[i]));
      const int contourIndex = contour.indices.Append(positionIndex);
      if (points[i].x > valueMax) {
        valueMax = points[i].x;
        indexMax = static_cast<std::int16_t>(contourIndex);
      }
    }
  } else {
    for (int i = 0; i < points.Num(); ++i) {
      const std::int16_t positionIndex =
          static_cast<std::int16_t>(positions.Append(points[i]));
      const int contourIndex = contour.indices.Append(positionIndex);
      if (points[i].x > valueMax) {
        valueMax = points[i].x;
        indexMax = static_cast<std::int16_t>(contourIndex);
      }
    }
  }

  contour.indexMax = indexMax;
  contour.valueMax = valueMax;
}

void idEarClipTriangulate::AddInnerFromPoints(
    const idList<idVec2>& points) {
  if (contours.Num() <= 0)
    contours.Append(nullptr);

  auto* contour = new contour_t;
  contour->indices.ClearFree();
  if (points.Num() > contour->indices.size)
    contour->indices.ResizeForPort(points.Num());
  float valueMax = -1.0e30f;
  std::int16_t indexMax = -1;

  if (IsClockwise(points)) {
    for (int i = 0; i < points.Num(); ++i) {
      const std::int16_t positionIndex =
          static_cast<std::int16_t>(positions.Append(points[i]));
      const int contourIndex = contour->indices.Append(positionIndex);
      if (points[i].x > valueMax) {
        valueMax = points[i].x;
        indexMax = static_cast<std::int16_t>(contourIndex);
      }
    }
  } else {
    for (int i = points.Num() - 1; i >= 0; --i) {
      const std::int16_t positionIndex =
          static_cast<std::int16_t>(positions.Append(points[i]));
      const int contourIndex = contour->indices.Append(positionIndex);
      if (points[i].x > valueMax) {
        valueMax = points[i].x;
        indexMax = static_cast<std::int16_t>(contourIndex);
      }
    }
  }

  contour->valueMax = valueMax;
  contour->indexMax = indexMax;
  contours.Append(contour);
}

void idEarClipTriangulate::CreateVertices() {
  if (indices.Num() > vertices.size)
    vertices.ResizeForPort(indices.Num());
  int previous = indices.Num() - 1;

  for (int current = 0; current < indices.Num(); ++current) {
    vertex_t vertex;
    vertex.index = indices[current];
    vertex.prevVertex = static_cast<std::int16_t>(previous);
    vertex.nextVertex =
        static_cast<std::int16_t>((current + 1) % indices.Num());
    vertex.prevEar = -1;
    vertex.nextEar = -1;
    vertex.prevShared.value = -1;
    vertex.nextShared.value = -1;
    vertex.isConvex = IsConvexCorner(
        positions[indices[previous]], positions[indices[current]],
        positions[indices[vertex.nextVertex]]);
    vertex.isEar = 0;
    vertices.Append(vertex);

    if (vertices[current].isConvex != 0) {
      if (headConvex == -1) {
        headConvex = current;
      } else {
        vertices[tailConvex].nextShared.convex =
            static_cast<std::int16_t>(current);
        vertices[current].prevShared.convex =
            static_cast<std::int16_t>(tailConvex);
      }
      tailConvex = current;
    } else {
      if (headReflex == -1) {
        headReflex = current;
      } else {
        vertices[tailReflex].nextShared.reflex =
            static_cast<std::int16_t>(current);
        vertices[current].prevShared.reflex =
            static_cast<std::int16_t>(tailReflex);
      }
      tailReflex = current;
    }
    previous = current;
  }
}

bool idEarClipTriangulate::Process() {
  int remaining = vertices.Num();
  if (headReflex == -1) {
    const std::int16_t first = indices[0];
    const int numTriangleIndices = 3 * (remaining - 2);
    if (numTriangleIndices > triangles.size)
      triangles.ResizeForPort(numTriangleIndices);
    for (int i = 1; i < remaining - 1; ++i) {
      triangles.Append(first);
      triangles.Append(indices[i]);
      triangles.Append(indices[i + 1]);
    }
    return true;
  }

  for (int vertexIndex = headConvex; vertexIndex != -1;
       vertexIndex = vertices[vertexIndex].nextShared.convex) {
    if (!UpdateEar(vertexIndex))
      continue;
    if (headEar == -1) {
      headEar = vertexIndex;
      tailEar = vertexIndex;
    }
    vertices[tailEar].nextEar = static_cast<std::int16_t>(vertexIndex);
    vertices[vertexIndex].prevEar = static_cast<std::int16_t>(tailEar);
    tailEar = vertexIndex;
  }

  vertices[headEar].prevEar = static_cast<std::int16_t>(tailEar);
  vertices[tailEar].nextEar = static_cast<std::int16_t>(headEar);

  auto appendEarTriangle = [this](int ear) {
    const vertex_t& vertex = vertices[ear];
    triangles.Append(vertices[vertex.prevVertex].index);
    triangles.Append(vertex.index);
    triangles.Append(vertices[vertex.nextVertex].index);
  };
  auto unlinkPolygonVertex = [this](int vertexIndex) {
    vertex_t& vertex = vertices[vertexIndex];
    vertices[vertex.prevVertex].nextVertex = vertex.nextVertex;
    vertices[vertex.nextVertex].prevVertex = vertex.prevVertex;
  };
  auto removeEar = [this](int vertexIndex) {
    const int previous = vertices[vertexIndex].prevEar;
    const int next = vertices[vertexIndex].nextEar;
    vertices[previous].nextEar = static_cast<std::int16_t>(next);
    vertices[next].prevEar = static_cast<std::int16_t>(previous);
  };
  auto recomputeConvex = [this](int vertexIndex) {
    vertex_t& vertex = vertices[vertexIndex];
    const bool wasReflex = vertex.isConvex == 0;
    vertex.isConvex = IsConvexCorner(
        positions[vertices[vertex.prevVertex].index], positions[vertex.index],
        positions[vertices[vertex.nextVertex].index]);
    return wasReflex;
  };

  appendEarTriangle(headEar);
  unlinkPolygonVertex(headEar);
  --remaining;
  if (remaining < 3) {
    idLib::Warning(
        "Triangulation removed up until less than three vertices left");
    return false;
  }

  while (remaining != 3) {
    const int currentEar = headEar;
    const int previousVertex = vertices[currentEar].prevVertex;
    const int nextVertex = vertices[currentEar].nextVertex;

    if (vertices[previousVertex].isEar != 0) {
      if (!UpdateEar(previousVertex))
        removeEar(previousVertex);
    } else {
      const bool wasReflex = recomputeConvex(previousVertex);
      if (vertices[previousVertex].isConvex != 0) {
        if (wasReflex && !RemoveReflex(previousVertex))
          return false;
        if (UpdateEar(previousVertex)) {
          vertex_t& previousVertexData = vertices[previousVertex];
          previousVertexData.prevEar = vertices[currentEar].prevEar;
          previousVertexData.nextEar = static_cast<std::int16_t>(currentEar);
          vertices[previousVertexData.prevEar].nextEar =
              static_cast<std::int16_t>(previousVertex);
          vertices[currentEar].prevEar =
              static_cast<std::int16_t>(previousVertex);
        }
      }
    }

    if (vertices[nextVertex].isEar != 0) {
      if (!UpdateEar(nextVertex))
        removeEar(nextVertex);
    } else {
      const bool wasReflex = recomputeConvex(nextVertex);
      if (vertices[nextVertex].isConvex != 0) {
        if (wasReflex && !RemoveReflex(nextVertex))
          return false;
        if (UpdateEar(nextVertex)) {
          vertex_t& nextVertexData = vertices[nextVertex];
          nextVertexData.prevEar = static_cast<std::int16_t>(currentEar);
          nextVertexData.nextEar = vertices[currentEar].nextEar;
          vertices[nextVertexData.nextEar].prevEar =
              static_cast<std::int16_t>(nextVertex);
          vertices[currentEar].nextEar =
              static_cast<std::int16_t>(nextVertex);
        }
      }
    }

    const int nextEar = vertices[currentEar].nextEar;
    removeEar(currentEar);
    headEar = nextEar;
    if (currentEar == nextEar) {
      const idVec2& previousPosition = positions[vertices[previousVertex].index];
      const idVec2& currentPosition = positions[vertices[nextEar].index];
      const idVec2& nextPosition = positions[vertices[nextVertex].index];
      const char* nextText = nextPosition.ToString(2);
      const char* currentText = currentPosition.ToString(2);
      const char* previousText = previousPosition.ToString(2);
      idLib::Warning(
          "idEarClipTriangulate::Process Circularity error same ear being "
          "processed.! prev %s index %s next %s",
          previousText, currentText, nextText);
      return false;
    }

    appendEarTriangle(headEar);
    unlinkPolygonVertex(headEar);
    --remaining;
    if (remaining < 3) {
      idLib::Warning(
          "Triangulation removed up until less than three vertices left");
      return false;
    }
  }

  const int finalEar = vertices[headEar].nextEar;
  removeEar(headEar);
  headEar = finalEar;
  appendEarTriangle(headEar);
  return true;
}

bool idEarClipTriangulate::CombineContours(
    const contour_t* contour, const idList<std::int16_t>& outerIndices,
    idList<std::int16_t>& combined) {
  if (contour->indexMax == -1) {
    idLib::Printf(
        "contourIndex max == -1 cannot be combined most likely the "
        "innercontour is broken");
    return false;
  }

  const int innerContourIndex = contour->indexMax;
  const std::int16_t innerPositionIndex =
      contour->indices[innerContourIndex];
  const idVec2 innerPosition = positions[innerPositionIndex];
  float closestDistance = 1.0e30f;
  int bridgeIndex = -1;
  int edgePrevious = -1;
  int edgeCurrent = -1;

  int previous = outerIndices.Num() - 1;
  for (int current = 0; current < outerIndices.Num(); ++current) {
    const idVec2 previousDelta = positions[outerIndices[previous]] - innerPosition;
    const idVec2 currentDelta = positions[outerIndices[current]] - innerPosition;
    if (!(previousDelta.y > 0.0f || currentDelta.y < 0.0f)) {
      float intersection;
      int endpoint = -1;
      if (previousDelta.y >= 0.0f) {
        if (currentDelta.y <= 0.0f) {
          if (currentDelta.x <= previousDelta.x) {
            intersection = currentDelta.x;
            endpoint = current;
          } else {
            intersection = previousDelta.x;
            endpoint = previous;
          }
        } else {
          intersection = previousDelta.x;
          endpoint = previous;
        }
      } else if (currentDelta.y <= 0.0f) {
        intersection = currentDelta.x;
        endpoint = current;
      } else {
        intersection =
            previousDelta.x +
            previousDelta.y / (previousDelta.y - currentDelta.y) *
                (currentDelta.x - previousDelta.x);
      }

      if (intersection >= 0.0f && intersection < closestDistance) {
        closestDistance = intersection;
        edgePrevious = previous;
        edgeCurrent = current;
        bridgeIndex = endpoint;
      }
    }
    previous = current;
  }

  const idVec2 intersection{innerPosition.x + closestDistance,
                            innerPosition.y};
  if (bridgeIndex == -1) {
    if (edgePrevious < 0 || edgeCurrent < 0) {
      idLib::Warning("Inner polygons share vertex/edge?");
      return false;
    }

    idVec2 triangle[3];
    if (positions[outerIndices[edgePrevious]].x <=
        positions[outerIndices[edgeCurrent]].x) {
      triangle[0] = positions[outerIndices[edgeCurrent]];
      triangle[1] = innerPosition;
      triangle[2] = intersection;
      bridgeIndex = edgeCurrent;
    } else {
      triangle[0] = positions[outerIndices[edgePrevious]];
      triangle[1] = intersection;
      triangle[2] = innerPosition;
      bridgeIndex = edgePrevious;
    }

    const idVec2 initial = triangle[0] - innerPosition;
    float bestDistance = initial.LengthSqr();
    float bestCosineSquared = initial.x * initial.x / bestDistance;
    for (int candidate = 0; candidate < outerIndices.Num(); ++candidate) {
      if (candidate == bridgeIndex)
        continue;

      const int candidatePrevious =
          (candidate + outerIndices.Num() - 1) % outerIndices.Num();
      const int candidateNext = (candidate + 1) % outerIndices.Num();
      const idVec2& candidatePosition = positions[outerIndices[candidate]];
      if (!(Orientation(positions[outerIndices[candidatePrevious]],
                        candidatePosition,
                        positions[outerIndices[candidateNext]]) <= 0.0f &&
            TriangleQuery(candidatePosition, triangle[0], triangle[1],
                          triangle[2]) <= 0))
        continue;

      const idVec2 difference = candidatePosition - innerPosition;
      const float distance = difference.LengthSqr();
      const float cosineSquared = difference.x * difference.x / distance;
      if (cosineSquared <= bestCosineSquared) {
        if (std::fabs(cosineSquared - bestCosineSquared) > kPointEpsilon ||
            bestDistance <= distance)
          continue;
      } else {
        bestCosineSquared = cosineSquared;
      }
      bridgeIndex = candidate;
      bestDistance = distance;
    }
  }

  combined.Clear();
  for (int i = 0; i <= bridgeIndex; ++i)
    combined.Append(outerIndices[i]);
  for (int i = 0; i < contour->indices.Num(); ++i) {
    combined.Append(
        contour->indices[(i + innerContourIndex) % contour->indices.Num()]);
  }

  auto remappedIndex = [this](std::int16_t positionIndex) {
    const int mapped = indexMap.hashIndex.First(positionIndex);
    return mapped == -1 ? positionIndex : indexMap.indices[mapped];
  };
  auto setRemap = [this](std::int16_t positionIndex,
                         std::int16_t originalIndex) {
    const int mapped = indexMap.hashIndex.First(positionIndex);
    if (mapped == -1) {
      const int mapIndex = indexMap.indices.Num();
      indexMap.hashIndex.Add(positionIndex, mapIndex);
      indexMap.indices.Append(originalIndex);
    } else {
      indexMap.indices[mapped] = originalIndex;
    }
  };

  const std::int16_t innerOriginal = remappedIndex(innerPositionIndex);
  const std::int16_t outerPositionIndex = outerIndices[bridgeIndex];
  const std::int16_t outerOriginal = remappedIndex(outerPositionIndex);

  const std::int16_t duplicateInner =
      static_cast<std::int16_t>(positions.Num());
  combined.Append(duplicateInner);
  setRemap(duplicateInner, innerOriginal);
  positions.Append(innerPosition);

  const std::int16_t duplicateOuter =
      static_cast<std::int16_t>(positions.Num());
  combined.Append(duplicateOuter);
  setRemap(duplicateOuter, outerOriginal);
  positions.Append(positions[outerPositionIndex]);

  for (int i = bridgeIndex + 1; i < outerIndices.Num(); ++i)
    combined.Append(outerIndices[i]);
  return true;
}

void idEarClipTriangulate::SetOuterFromContour(const Contour* outer) {
  if (outer->next != nullptr) {
    idLib::Printf(
        "Multiple outer contours! Need tree view of inner contours belonging "
        "to outer.\n");
  }

  idStaticList<idVec2, 1024> points;
  for (const ContourEdge_t* edge = outer->edges; edge != nullptr;
       edge = edge->next) {
    points.Append({static_cast<float>(edge->node->pos.x),
                   static_cast<float>(edge->node->pos.y)});
  }
  SetOuterFromPoints(points);
}

void idEarClipTriangulate::SetOuterFromPoints(
    const idList<idVec2i>& points) {
  idStaticList<idVec2, 1024> converted;
  converted.EnsureCapacity(points.Num());
  for (int i = 0; i < points.Num(); ++i) {
    converted.Append(
        {static_cast<float>(points[i].x), static_cast<float>(points[i].y)});
  }
  SetOuterFromPoints(converted);
}

void idEarClipTriangulate::AddInnerFromContour(const Contour* inner) {
  idStaticList<idVec2, 1024> points;
  for (const Contour* contour = inner; contour != nullptr;
       contour = contour->next) {
    points.Clear();
    for (const ContourEdge_t* edge = contour->edges; edge != nullptr;
         edge = edge->next) {
      points.Append({static_cast<float>(edge->node->pos.x),
                     static_cast<float>(edge->node->pos.y)});
    }
    AddInnerFromPoints(points);
  }
}

void idEarClipTriangulate::AddInnerFromPoints(
    const idList<idVec2i>& points) {
  idStaticList<idVec2, 1024> converted;
  converted.EnsureCapacity(points.Num());
  for (int i = 0; i < points.Num(); ++i) {
    converted.Append(
        {static_cast<float>(points[i].x), static_cast<float>(points[i].y)});
  }
  AddInnerFromPoints(converted);
}

bool idEarClipTriangulate::CreateSimplePolygon() {
  const int numContours = contours.Num();
  if (numContours == 1)
    indices.Append(contours[0]->indices);

  for (int sorted = 1; sorted < numContours; ++sorted) {
    for (int index = numContours - 1; index > sorted; --index) {
      if (contours[index]->valueMax > contours[index - 1]->valueMax)
        std::swap(contours[index], contours[index - 1]);
    }
  }

  int requiredCapacity = 2 * numContours;
  for (int contourIndex = 0; contourIndex < numContours; ++contourIndex)
    requiredCapacity += contours[contourIndex]->indices.Num();

  idList<std::int16_t> workIndices;
  if (requiredCapacity > indices.size)
    indices.ResizeForPort(requiredCapacity);
  if (requiredCapacity > 0)
    workIndices.ResizeForPort(requiredCapacity);

  idList<std::int16_t>* source = &workIndices;
  idList<std::int16_t>* destination = &indices;
  if ((numContours & 1) != 0)
    std::swap(source, destination);

  source->Clear();
  source->Append(contours[0]->indices);
  for (int contourIndex = 1; contourIndex < numContours; ++contourIndex) {
    if (!CombineContours(contours[contourIndex], *source, *destination))
      return false;
    std::swap(source, destination);
  }
  return true;
}

bool idEarClipTriangulate::Triangulate() {
  Reset();
  if (CreateSimplePolygon()) {
    CreateVertices();
    if (Process()) {
      RemapIndices();
      for (int i = 0; i < triangles.Num(); ++i) {
        if (triangles[i] >= vertices.Num()) {
          idLib::Warning("Triangulation index is out of range!");
          return false;
        }
      }
      return true;
    }
  }
  return false;
}
