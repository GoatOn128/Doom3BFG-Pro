#include "engine/cm/collisionquery.h"

namespace {
float PlaneDistance(const idPlane& plane, const idVec3& point) {
  return plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d;
}
} // namespace

bool idPolygonModelCollisionDetection::ClipInPlace(std::vector<idVec3>& vertices,
                                                   const idPlane& plane, float epsilon) {
  if (vertices.empty()) {
    return false;
  }
  std::vector<idVec3> clipped;
  clipped.reserve(vertices.size() + 2);
  idVec3 previous = vertices.back();
  float previousDistance = PlaneDistance(plane, previous);
  for (const idVec3& current : vertices) {
    const float currentDistance = PlaneDistance(plane, current);
    const bool previousInside = previousDistance <= epsilon;
    const bool currentInside = currentDistance <= epsilon;
    if (previousInside != currentInside) {
      const float fraction = previousDistance / (previousDistance - currentDistance);
      clipped.push_back(previous + (current - previous) * fraction);
    }
    if (currentInside) {
      clipped.push_back(current);
    }
    previous = current;
    previousDistance = currentDistance;
  }
  vertices = std::move(clipped);
  return !vertices.empty();
}

clipResult_t idPolygonModelCollisionDetection::ClipPolygonWithTrm(
    const cmPolygon_t& polygon, const idCollisionModelLocal& model, const idTraceModel& traceModel,
    const idVec3& origin, const idMat3& axis) {
  clipResult_t result;
  for (std::uint32_t index : polygon.vertices) {
    if (index < model.vertices.size()) {
      result.vertices.push_back(model.vertices[index]);
    }
  }
  // Clip against the six planes of the transformed trace-model bounds.  This
  // is exact for boxes and conservative for other trace models.
  idBounds bounds;
  bounds.FromTransformedBounds(traceModel.bounds, origin, axis);
  const idPlane planes[6] = {{-1, 0, 0, bounds[0].x}, {1, 0, 0, -bounds[1].x},
                             {0, -1, 0, bounds[0].y}, {0, 1, 0, -bounds[1].y},
                             {0, 0, -1, bounds[0].z}, {0, 0, 1, -bounds[1].z}};
  for (const idPlane& plane : planes) {
    if (!ClipInPlace(result.vertices, plane)) {
      return {};
    }
  }
  for (std::uint32_t index = 2; index < result.vertices.size(); ++index) {
    result.indices.insert(result.indices.end(), {0, index - 1, index});
  }
  return result;
}

clipResult_t idPolygonModelCollisionDetection::FinishClip(const clipResult_t& result) {
  return result;
}

clipResult_t idPolygonModelCollisionDetection::StartClip(const idCollisionModelLocal& model,
                                                         const queryParms_t& parms) {
  clipResult_t result;
  if (!parms.traceModel) {
    return result;
  }
  for (std::uint32_t i = 0; i < parms.traceModel->numVerts; ++i) {
    const idVec3 vertex = parms.axis * parms.traceModel->GetVertex(i) + parms.start;
    if (CM_PointInsideBounds(vertex, model.GetBounds(), 1.0e-4f)) {
      result.vertices.push_back(vertex);
    }
  }
  for (std::uint32_t i = 2; i < result.vertices.size(); ++i) {
    result.indices.push_back(0);
    result.indices.push_back(i - 1);
    result.indices.push_back(i);
  }
  return result;
}
