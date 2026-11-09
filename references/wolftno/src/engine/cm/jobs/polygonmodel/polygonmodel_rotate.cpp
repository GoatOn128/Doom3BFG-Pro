#include "engine/cm/collisionquery.h"

#include <algorithm>
#include <cmath>
#include <limits>

float CM_TanZeroHalfPI(float angleRadians) {
  constexpr float halfPi = 1.5707963267948966f;
  return std::tan(std::clamp(angleRadians, 0.0f, halfPi - 1.0e-6f));
}

float CM_ArcTanPositive(float value) {
  return std::atan(std::max(value, 0.0f));
}

idBounds CM_PointRotationBounds(const idVec3& point, const idRotation& rotation) {
  idBounds result;
  result.FromPointRotation(point, rotation);
  return result;
}

trace_t idPolygonModelCollisionDetection::FinishRotation(const trace_t& trace) {
  return trace;
}

queryParms_t idPolygonModelCollisionDetection::RotationSetup(const queryParms_t& parms,
                                                             const idRotation& rotation) {
  queryParms_t result = parms;
  result.end = rotation * parms.start;
  return result;
}

idVec3 idPolygonModelCollisionDetection::TransformFromOriginAxisAngle(const idVec3& point,
                                                                      const idVec3& origin,
                                                                      const idVec3& axis,
                                                                      float angle) {
  return idRotation(origin, axis, angle) * point;
}

idMat3 idPolygonModelCollisionDetection::TransformAxisToZAxis(const idVec3& axis) {
  idVec3 normalized = axis;
  if (normalized.Normalize() == 0.0f) {
    return mat3_identity;
  }
  return normalized.ToZAxis();
}

std::vector<idVec3> idPolygonModelCollisionDetection::RotationVerts(const idTraceModel& traceModel,
                                                                    const idRotation& rotation) {
  std::vector<idVec3> result;
  result.reserve(traceModel.numVerts);
  for (std::uint32_t index = 0; index < traceModel.numVerts; ++index) {
    result.push_back(rotation * traceModel.GetVertex(index));
  }
  return result;
}

std::vector<cmEdge_t>
idPolygonModelCollisionDetection::RotationEdges(const idTraceModel& traceModel) {
  return TranslationEdges(traceModel);
}

std::vector<std::size_t>
idPolygonModelCollisionDetection::RotationPolys(const idTraceModel& traceModel) {
  return TranslationPolys(traceModel);
}

idBounds idPolygonModelCollisionDetection::RotationBounds(const queryParms_t& parms,
                                                          const idRotation& rotation) {
  idBounds result;
  if (parms.traceModel) {
    result.FromBoundsRotation(parms.traceModel->bounds, parms.start, parms.axis, rotation);
  } else {
    result = CM_PointRotationBounds(parms.start, rotation);
  }
  return result;
}

std::vector<float>
idPolygonModelCollisionDetection::RotationEdgePlueckerCache(const std::vector<cmEdge_t>& edges,
                                                            const std::vector<idVec3>& vertices) {
  return TranslationEdgePlueckerCache(edges, vertices);
}

std::vector<bool>
idPolygonModelCollisionDetection::RotationCullPolygonEdges(const cmPolygon_t& polygon,
                                                           const idVec3& axis) {
  const idVec3 normal{polygon.plane.a, polygon.plane.b, polygon.plane.c};
  return std::vector<bool>(polygon.vertices.size(), std::fabs(normal * axis) < 1.0e-6f);
}

bool idPolygonModelCollisionDetection::LineIntersectsBoundsShort(const idVec3& start,
                                                                 const idVec3& end,
                                                                 const idBounds& bounds) {
  return bounds.LineIntersection(start, end);
}

bool idPolygonModelCollisionDetection::CollisionBetweenEdgeBounds(const idVec3& firstStart,
                                                                  const idVec3& firstEnd,
                                                                  const idVec3& secondStart,
                                                                  const idVec3& secondEnd,
                                                                  float epsilon) {
  idBounds first;
  idBounds second;
  first.Clear();
  second.Clear();
  first.AddPoint(firstStart);
  first.AddPoint(firstEnd);
  second.AddPoint(secondStart);
  second.AddPoint(secondEnd);
  for (int axis = 0; axis < 3; ++axis) {
    first[0][axis] -= epsilon;
    first[1][axis] += epsilon;
    second[0][axis] -= epsilon;
    second[1][axis] += epsilon;
  }
  return CM_BoundsIntersect(first, second);
}

bool idPolygonModelCollisionDetection::RotateEdgeThroughEdge(
    const idVec3& movingStart, const idVec3& movingEnd, const idVec3& edgeStart,
    const idVec3& edgeEnd, const idRotation& rotation, float& fraction) {
  const idVec3 rotatedStart = rotation * movingStart;
  const idVec3 rotatedEnd = rotation * movingEnd;
  if (!CollisionBetweenEdgeBounds(movingStart, rotatedStart, edgeStart, edgeEnd) &&
      !CollisionBetweenEdgeBounds(movingEnd, rotatedEnd, edgeStart, edgeEnd)) {
    fraction = 1.0f;
    return false;
  }
  float firstFraction = 1.0f;
  float secondFraction = 1.0f;
  const bool firstHit =
      TranslateEdgeThroughEdge(movingStart, rotatedStart, edgeStart, edgeEnd, firstFraction);
  const bool secondHit =
      TranslateEdgeThroughEdge(movingEnd, rotatedEnd, edgeStart, edgeEnd, secondFraction);
  fraction = std::min(firstFraction, secondFraction);
  return firstHit || secondHit;
}

idVec3 idPolygonModelCollisionDetection::EdgeFurthestFromEdge(const idVec3& first,
                                                              const idVec3& second,
                                                              const idVec3& edgeStart,
                                                              const idVec3& edgeEnd) {
  const idVec3 midpoint = (edgeStart + edgeEnd) * 0.5f;
  return (first - midpoint).LengthSqr() >= (second - midpoint).LengthSqr() ? first : second;
}

bool idPolygonModelCollisionDetection::RotatePointThroughPlane(const idVec3& point,
                                                               const idRotation& rotation,
                                                               const idPlane& plane,
                                                               float& fraction) {
  return RotatePointThroughEpsilonPlane(point, rotation, plane, 0.0f, fraction);
}

idVec3 idPolygonModelCollisionDetection::PointFurthestFromPlane(const std::vector<idVec3>& points,
                                                                const idPlane& plane) {
  idVec3 result{};
  float largest = -std::numeric_limits<float>::infinity();
  for (const idVec3& point : points) {
    const float distance = plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d;
    if (distance > largest) {
      largest = distance;
      result = point;
    }
  }
  return result;
}

bool idPolygonModelCollisionDetection::RotatePointThroughEpsilonPlane(const idVec3& point,
                                                                      const idRotation& rotation,
                                                                      const idPlane& plane,
                                                                      float epsilon,
                                                                      float& fraction) {
  const auto distance = [&plane](const idVec3& value) {
    return plane.a * value.x + plane.b * value.y + plane.c * value.z + plane.d;
  };
  float previousDistance = distance(point) - epsilon;
  idVec3 previousPoint = point;
  constexpr int samples = 64;
  for (int sample = 1; sample <= samples; ++sample) {
    const float currentFraction = static_cast<float>(sample) / samples;
    const idVec3 currentPoint =
        idRotation(rotation.origin, rotation.vec, rotation.angle * currentFraction) * point;
    const float currentDistance = distance(currentPoint) - epsilon;
    if ((previousDistance > 0.0f) != (currentDistance > 0.0f)) {
      const float local = previousDistance / (previousDistance - currentDistance);
      fraction = (static_cast<float>(sample - 1) + local) / samples;
      return true;
    }
    previousDistance = currentDistance;
    previousPoint = currentPoint;
  }
  (void)previousPoint;
  fraction = 1.0f;
  return false;
}

bool idPolygonModelCollisionDetection::RotateTrmVertsThroughPolygon(const idTraceModel& traceModel,
                                                                    const idRotation& rotation,
                                                                    const cmPolygon_t& polygon,
                                                                    float& fraction) {
  bool hit = false;
  fraction = 1.0f;
  for (std::uint32_t index = 0; index < traceModel.numVerts; ++index) {
    float candidate{};
    if (RotatePointThroughEpsilonPlane(traceModel.GetVertex(index), rotation, polygon.plane, 0.001f,
                                       candidate)) {
      const idVec3 point = idRotation(rotation.origin, rotation.vec, rotation.angle * candidate) *
                           traceModel.GetVertex(index);
      if (CM_PointInsideBounds(point, polygon.bounds, 0.001f)) {
        fraction = std::min(fraction, candidate);
        hit = true;
      }
    }
  }
  return hit;
}

bool idPolygonModelCollisionDetection::RotateTrmEdgesThroughPolygon(const idTraceModel& traceModel,
                                                                    const idRotation& rotation,
                                                                    const cmPolygon_t& polygon,
                                                                    float& fraction) {
  return RotateTrmVertsThroughPolygon(traceModel, rotation, polygon, fraction);
}

bool idPolygonModelCollisionDetection::RotatePolygonVertsThroughTrm(
    const cmPolygon_t& polygon, const idCollisionModelLocal& model, const idTraceModel& traceModel,
    const idRotation& rotation, float& fraction) {
  for (std::uint32_t vertex : polygon.vertices) {
    if (vertex < model.vertices.size() &&
        traceModel.ContainsPoint(rotation * model.vertices[vertex])) {
      fraction = 0.0f;
      return true;
    }
  }
  fraction = 1.0f;
  return false;
}

bool idPolygonModelCollisionDetection::RotateTrmThroughPolygon(const idTraceModel& traceModel,
                                                               const idRotation& rotation,
                                                               const cmPolygon_t& polygon,
                                                               const idCollisionModelLocal& model,
                                                               float& fraction) {
  float traceFraction = 1.0f;
  float polygonFraction = 1.0f;
  const bool traceHit = RotateTrmVertsThroughPolygon(traceModel, rotation, polygon, traceFraction);
  const bool polygonHit =
      RotatePolygonVertsThroughTrm(polygon, model, traceModel, rotation, polygonFraction);
  fraction = std::min(traceFraction, polygonFraction);
  return traceHit || polygonHit;
}

trace_t idPolygonModelCollisionDetection::StartRotation(const idCollisionModelLocal& model,
                                                        const queryParms_t& parms,
                                                        const idRotation& rotation) {
  queryParms_t sweep = parms;
  sweep.end = rotation * parms.start;
  return StartTranslation(model, sweep);
}

trace_t idPolygonModelCollisionDetection::StartRotationPoint(const idCollisionModelLocal& model,
                                                             const queryParms_t& parms,
                                                             const idRotation& rotation) {
  queryParms_t pointParms = parms;
  pointParms.traceModel = nullptr;
  return StartRotation(model, pointParms, rotation);
}
