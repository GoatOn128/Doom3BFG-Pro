#include "engine/cm/collisionquery.h"

queryParms_t& queryParms_t::operator=(const queryParms_t& other) {
  if (this != &other) {
    start = other.start;
    end = other.end;
    axis = other.axis;
    traceModel = other.traceModel;
    contentsMask = other.contentsMask;
  }
  return *this;
}

planeSide_t CM_BoundsPlaneSide(const idBounds& bounds, const idPlane& plane, float epsilon) {
  return bounds.PlaneSide(plane, epsilon);
}

bool CM_BoundsShortPlaneCull(const idBounds& bounds, const idPlane& plane, float epsilon) {
  return CM_BoundsPlaneSide(bounds, plane, epsilon) == PLANESIDE_FRONT;
}

queryParms_t idCollisionQueryExecute::SetupDependentParms(const queryParms_t& parent,
                                                          const idVec3& modelOrigin,
                                                          const idMat3& modelAxis) {
  queryParms_t result = parent;
  const auto toLocal = [&modelAxis, &modelOrigin](const idVec3& point) {
    const idVec3 translated = point - modelOrigin;
    return idVec3{modelAxis[0] * translated, modelAxis[1] * translated, modelAxis[2] * translated};
  };
  result.start = toLocal(parent.start);
  result.end = toLocal(parent.end);
  result.contentsMask = parent.contentsMask == CONTENTS_NONE ? CONTENTS_ALL : parent.contentsMask;
  return result;
}

trace_t idCollisionQueryExecute::ExecutePolygonModelQuery(const idCollisionModelLocal& model,
                                                          const queryParms_t& parms) {
  return idPolygonModelCollisionDetection::StartTranslation(model, parms);
}

trace_t idCollisionQueryExecute::ExecuteSphereModelQuery(const idCollisionModelLocal& model,
                                                         const queryParms_t& parms) {
  return idSphereModelCollisionDetection::TraceThroughModel(model, parms);
}

trace_t idCollisionQueryExecute::ExecuteSubModelQuery(const idCollisionModelLocal& model,
                                                      const queryParms_t& parms) {
  return model.GetType() == CM_TYPE_SPHERE ? ExecuteSphereModelQuery(model, parms)
                                           : ExecutePolygonModelQuery(model, parms);
}

bool idCollisionQueryExecute::TestPolygonModelQuery(const idCollisionModelLocal& model,
                                                    const queryParms_t& parms) {
  idBounds traceBounds;
  traceBounds.Clear();
  traceBounds.AddPoint(parms.start);
  traceBounds.AddPoint(parms.end);
  if (parms.traceModel) {
    traceBounds[0] += parms.traceModel->bounds[0];
    traceBounds[1] += parms.traceModel->bounds[1];
  }
  return (model.GetContents() & parms.contentsMask) != 0 &&
         CM_BoundsIntersect(traceBounds, model.GetBounds());
}

bool idCollisionQueryExecute::TestSubModelQuery(const idCollisionModelLocal& model,
                                                const queryParms_t& parms) {
  return TestPolygonModelQuery(model, parms);
}
