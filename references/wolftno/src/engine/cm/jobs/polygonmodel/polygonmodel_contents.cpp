#include "engine/cm/collisionquery.h"

namespace {
float PlaneDistance(const idPlane& plane, const idVec3& point) {
  return plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d;
}
} // namespace

int idPolygonModelCollisionDetection::FinishContents(int contents) {
  return contents;
}

bool idPolygonModelCollisionDetection::TestTrmVertsInPolytope(const idTraceModel& traceModel,
                                                              const idVec3& origin,
                                                              const cmPolytope_t& polytope) {
  for (std::uint32_t vertex = 0; vertex < traceModel.numVerts; ++vertex) {
    const idVec3 point = traceModel.GetVertex(vertex) + origin;
    bool inside = true;
    for (const idPlane& plane : polytope.planes) {
      if (PlaneDistance(plane, point) > 0.001f) {
        inside = false;
        break;
      }
    }
    if (inside) {
      return true;
    }
  }
  return false;
}

bool idPolygonModelCollisionDetection::TestTrmInPolygon(const idTraceModel& traceModel,
                                                        const idVec3& origin,
                                                        const cmPolygon_t& polygon) {
  const idBounds translated = CM_TranslatedBounds(traceModel.bounds, origin);
  return CM_BoundsIntersect(translated, polygon.bounds);
}

int idPolygonModelCollisionDetection::StartContentsPoint(const idCollisionModelLocal& model,
                                                         const idVec3& point, std::uint32_t mask) {
  if ((model.GetContents() & mask) == 0 || !CM_PointInsideBounds(point, model.GetBounds())) {
    return CONTENTS_NONE;
  }
  if (model.GetType() == CM_TYPE_SPHERE &&
      point.LengthSqr() > model.sphereRadius * model.sphereRadius) {
    return CONTENTS_NONE;
  }
  return static_cast<int>(model.GetContents() & mask);
}

int idPolygonModelCollisionDetection::StartContents(const idCollisionModelLocal& model,
                                                    const queryParms_t& parms) {
  if (!parms.traceModel) {
    return StartContentsPoint(model, parms.start, parms.contentsMask);
  }
  const idBounds translated = CM_TranslatedBounds(parms.traceModel->bounds, parms.start);
  return CM_BoundsIntersect(translated, model.GetBounds())
             ? static_cast<int>(model.GetContents() & parms.contentsMask)
             : CONTENTS_NONE;
}
