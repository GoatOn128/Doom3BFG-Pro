#include "engine/cm/collisionquery.h"
#include "engine/cm/collisionmodelbuilder.h"

const idCollisionModelLocal*
idPolygonModelCollisionDetection::SetupSubModelPtrsFromData(const idCollisionModelLocal& model) {
  return &model;
}

idCollisionModelLocal
idPolygonModelCollisionDetection::SetupSubModelForBounds(const idBounds& bounds,
                                                         std::uint32_t contents) {
  idCollisionModelBuilder builder;
  return *builder.BuildForBounds(bounds, "queryBounds", contents);
}

idBounds
idPolygonModelCollisionDetection::LocalExtentsFromUnTransformedBounds(const idBounds& bounds,
                                                                      const idVec3& origin) {
  idBounds result = bounds;
  result[0] -= origin;
  result[1] -= origin;
  return result;
}

bool idPolygonModelCollisionDetection::TestStuckInSubModelBounds(const idCollisionModelLocal& model,
                                                                 const queryParms_t& parms) {
  if (!parms.traceModel) {
    return CM_PointInsideBounds(parms.start, model.GetBounds());
  }
  return CM_BoundsIntersect(CM_TranslatedBounds(parms.traceModel->bounds, parms.start),
                            model.GetBounds());
}

bool idPolygonModelCollisionDetection::TraceThroughSubModel(const idCollisionModelLocal& model,
                                                            const queryParms_t& parms,
                                                            trace_t& result) {
  result = StartTranslation(model, parms);
  return result.startSolid || result.fraction < 1.0f;
}

bool idPolygonModelCollisionDetection::TraceThroughSubModelTree(const idCollisionModelLocal& model,
                                                                const queryParms_t& parms,
                                                                trace_t& result) {
  if (!idCollisionQueryExecute::TestSubModelQuery(model, parms)) {
    result = {};
    result.endpos = parms.end;
    return false;
  }
  return TraceThroughSubModel(model, parms, result);
}

std::vector<const idCollisionModelLocal*>
idPolygonModelCollisionDetection::GetSubModelsForTrace(const idCollisionModelLocal& model,
                                                       const queryParms_t& parms) {
  if (!idCollisionQueryExecute::TestSubModelQuery(model, parms)) {
    return {};
  }
  return {SetupSubModelPtrsFromData(model)};
}

bool idPolygonModelCollisionDetection::TraceThroughModel(const idCollisionModelLocal& model,
                                                         const queryParms_t& parms,
                                                         trace_t& result) {
  result = model.GetType() == CM_TYPE_SPHERE
               ? idSphereModelCollisionDetection::TraceThroughModel(model, parms)
               : StartTranslation(model, parms);
  return result.fraction < 1.0f || result.startSolid;
}
