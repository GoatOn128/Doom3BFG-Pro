#include "engine/cm/collisionquery.h"

#include <algorithm>
#include <cmath>
#include <limits>

bool CM_PointInsideBounds(const idVec3& point, const idBounds& bounds, float epsilon) {
  for (int axis = 0; axis < 3; ++axis) {
    if (point[axis] < bounds[0][axis] - epsilon || point[axis] > bounds[1][axis] + epsilon) {
      return false;
    }
  }
  return true;
}

trace_t CM_TraceBounds(const idBounds& target, std::uint32_t contents, const queryParms_t& parms) {
  trace_t result;
  result.endpos = parms.end;
  result.endAxis = parms.axis;
  if ((contents & parms.contentsMask) == 0) {
    return result;
  }

  idBounds expanded = target;
  if (parms.traceModel) {
    for (int axis = 0; axis < 3; ++axis) {
      expanded[0][axis] -= parms.traceModel->bounds[1][axis];
      expanded[1][axis] -= parms.traceModel->bounds[0][axis];
    }
  }

  result.startSolid = CM_PointInsideBounds(parms.start, expanded);
  if (result.startSolid) {
    result.fraction = 0.0f;
    result.endpos = parms.start;
    result.allSolid = CM_PointInsideBounds(parms.end, expanded);
    result.contact.point = parms.start;
    result.contact.contents = contents;
    return result;
  }

  const idVec3 direction = parms.end - parms.start;
  float entering = 0.0f;
  float leaving = 1.0f;
  idVec3 normal{};
  for (int axis = 0; axis < 3; ++axis) {
    if (std::fabs(direction[axis]) < 1.0e-8f) {
      if (parms.start[axis] < expanded[0][axis] || parms.start[axis] > expanded[1][axis]) {
        return result;
      }
      continue;
    }
    float first = (expanded[0][axis] - parms.start[axis]) / direction[axis];
    float second = (expanded[1][axis] - parms.start[axis]) / direction[axis];
    float sign = -1.0f;
    if (first > second) {
      std::swap(first, second);
      sign = 1.0f;
    }
    if (first > entering) {
      entering = first;
      normal = {};
      normal[axis] = sign;
    }
    leaving = std::min(leaving, second);
    if (entering > leaving) {
      return result;
    }
  }
  if (entering < 0.0f || entering > 1.0f) {
    return result;
  }
  result.fraction = std::max(0.0f, entering - 1.0e-5f);
  result.endpos = parms.start + direction * result.fraction;
  result.contact.point = result.endpos;
  result.contact.normal = normal;
  result.contact.distance = normal * result.contact.point;
  result.contact.contents = contents;
  return result;
}

trace_t idPolygonModelCollisionDetection::StartTranslation(const idCollisionModelLocal& model,
                                                           const queryParms_t& parms) {
  return CM_TraceBounds(model.GetBounds(), model.GetContents(), parms);
}

trace_t idPolygonModelCollisionDetection::StartTranslationPoint(const idCollisionModelLocal& model,
                                                                const queryParms_t& parms) {
  queryParms_t pointParms = parms;
  pointParms.traceModel = nullptr;
  return StartTranslation(model, pointParms);
}

trace_t idPolygonModelCollisionDetection::FinishTranslation(const trace_t& trace) {
  return trace;
}

idTraceWork idPolygonModelCollisionDetection::TranslationSetup(const queryParms_t& parms) {
  idTraceWork work;
  work.Init(parms);
  return work;
}

std::vector<std::size_t>
idPolygonModelCollisionDetection::TranslationUsedPrimitives(const idCollisionModelLocal& model,
                                                            const idBounds& sweptBounds) {
  std::vector<std::size_t> result;
  for (std::size_t index = 0; index < model.polygons.size(); ++index) {
    if (CM_BoundsIntersect(sweptBounds, model.polygons[index].bounds)) {
      result.push_back(index);
    }
  }
  return result;
}

std::vector<idPlane>
idPolygonModelCollisionDetection::TranslationHeartPlanes(const idCollisionModelLocal& model) {
  std::vector<idPlane> result;
  result.reserve(model.polygons.size());
  for (const cmPolygon_t& polygon : model.polygons) {
    result.push_back(polygon.plane);
  }
  return result;
}

std::vector<idVec3>
idPolygonModelCollisionDetection::TranslationVerts(const idTraceModel& traceModel,
                                                   const idVec3& origin) {
  std::vector<idVec3> result;
  result.reserve(traceModel.numVerts);
  for (std::uint32_t index = 0; index < traceModel.numVerts; ++index) {
    result.push_back(traceModel.GetVertex(index) + origin);
  }
  return result;
}

std::vector<cmEdge_t>
idPolygonModelCollisionDetection::TranslationEdges(const idTraceModel& traceModel) {
  std::vector<cmEdge_t> result(traceModel.numEdges);
  for (std::uint32_t index = 0; index < traceModel.numEdges; ++index) {
    result[index].vertex[0] = traceModel.edges[index].v[0];
    result[index].vertex[1] = traceModel.edges[index].v[1];
    result[index].normal = {traceModel.edgeNormalX[index], traceModel.edgeNormalY[index],
                            traceModel.edgeNormalZ[index]};
  }
  return result;
}

std::vector<std::size_t>
idPolygonModelCollisionDetection::TranslationPolys(const idTraceModel& traceModel) {
  std::vector<std::size_t> result(traceModel.numPolys);
  for (std::size_t index = 0; index < result.size(); ++index) {
    result[index] = index;
  }
  return result;
}

idBounds idPolygonModelCollisionDetection::TranslationBounds(const queryParms_t& parms) {
  idBounds result;
  result.Clear();
  result.AddPoint(parms.start);
  result.AddPoint(parms.end);
  if (parms.traceModel) {
    result[0] += parms.traceModel->bounds[0];
    result[1] += parms.traceModel->bounds[1];
  }
  return result;
}

void idPolygonModelCollisionDetection::TranslationUpdateBounds(idBounds& bounds,
                                                               const idVec3& point) {
  bounds.AddPoint(point);
}

std::vector<float>
idPolygonModelCollisionDetection::TranslationPlueckerCache(const std::vector<idVec3>& vertices,
                                                           const idVec3& direction) {
  std::vector<float> result;
  result.reserve(vertices.size());
  for (const idVec3& vertex : vertices) {
    result.push_back(vertex.Cross(direction).LengthSqr());
  }
  return result;
}

std::vector<float> idPolygonModelCollisionDetection::TranslationEdgePlueckerCache(
    const std::vector<cmEdge_t>& edges, const std::vector<idVec3>& vertices) {
  std::vector<float> result;
  result.reserve(edges.size());
  for (const cmEdge_t& edge : edges) {
    result.push_back(edge.vertex[0] < vertices.size() && edge.vertex[1] < vertices.size()
                         ? (vertices[edge.vertex[1]] - vertices[edge.vertex[0]]).LengthSqr()
                         : 0.0f);
  }
  return result;
}

std::vector<int>
idPolygonModelCollisionDetection::TranslationSideCache(const std::vector<float>& values) {
  std::vector<int> result;
  result.reserve(values.size());
  for (float value : values) {
    result.push_back(value > 0.0f ? 1 : value < 0.0f ? -1 : 0);
  }
  return result;
}

std::vector<int>
idPolygonModelCollisionDetection::TranslationPolygonSideCache(const idCollisionModelLocal& model,
                                                              const idVec3& point) {
  std::vector<int> result;
  result.reserve(model.polygons.size());
  for (const cmPolygon_t& polygon : model.polygons) {
    const float distance = polygon.plane.a * point.x + polygon.plane.b * point.y +
                           polygon.plane.c * point.z + polygon.plane.d;
    result.push_back(distance > 0.0f ? 1 : distance < 0.0f ? -1 : 0);
  }
  return result;
}

void idPolygonModelCollisionDetection::AddContact(contactsResult_t& contacts,
                                                  const contactInfo_t& contact,
                                                  std::size_t maximum) {
  if (contacts.contacts.size() < maximum) {
    contacts.contacts.push_back(contact);
  }
}

bool idPolygonModelCollisionDetection::TranslatePointThroughPlane(const idVec3& start,
                                                                  const idVec3& end,
                                                                  const idPlane& plane,
                                                                  float& fraction) {
  const auto distance = [&plane](const idVec3& point) {
    return plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d;
  };
  const float first = distance(start);
  const float second = distance(end);
  if ((first > 0.0f && second > 0.0f) || (first <= 0.0f && second <= 0.0f)) {
    return false;
  }
  fraction = std::clamp(first / (first - second), 0.0f, 1.0f);
  return true;
}

bool idPolygonModelCollisionDetection::TranslatePointThroughPolygon(const idVec3& start,
                                                                    const idVec3& end,
                                                                    const cmPolygon_t& polygon,
                                                                    float& fraction) {
  if (!TranslatePointThroughPlane(start, end, polygon.plane, fraction)) {
    return false;
  }
  return CM_PointInsideBounds(start + (end - start) * fraction, polygon.bounds, 0.001f);
}

bool idPolygonModelCollisionDetection::TranslateEdgeThroughEdge(const idVec3& movingStart,
                                                                const idVec3& movingEnd,
                                                                const idVec3& edgeStart,
                                                                const idVec3& edgeEnd,
                                                                float& fraction) {
  idBounds edgeBounds;
  edgeBounds.Clear();
  edgeBounds.AddPoint(edgeStart);
  edgeBounds.AddPoint(edgeEnd);
  for (int axis = 0; axis < 3; ++axis) {
    edgeBounds[0][axis] -= 0.001f;
    edgeBounds[1][axis] += 0.001f;
  }
  queryParms_t parms;
  parms.start = movingStart;
  parms.end = movingEnd;
  const trace_t trace = CM_TraceBounds(edgeBounds, CONTENTS_SOLID, parms);
  fraction = trace.fraction;
  return trace.fraction < 1.0f || trace.startSolid;
}

bool idPolygonModelCollisionDetection::TranslateTrmVertsThroughPolygon(
    const idTraceModel& traceModel, const idVec3& start, const idVec3& end,
    const cmPolygon_t& polygon, float& fraction) {
  bool hit = false;
  fraction = 1.0f;
  for (std::uint32_t index = 0; index < traceModel.numVerts; ++index) {
    float candidate{};
    if (TranslatePointThroughPolygon(traceModel.GetVertex(index) + start,
                                     traceModel.GetVertex(index) + end, polygon, candidate)) {
      fraction = std::min(fraction, candidate);
      hit = true;
    }
  }
  return hit;
}

bool idPolygonModelCollisionDetection::TranslateTrmEdgesThroughPolygon(
    const idTraceModel& traceModel, const idVec3& start, const idVec3& end,
    const cmPolygon_t& polygon, float& fraction) {
  return TranslateTrmVertsThroughPolygon(traceModel, start, end, polygon, fraction);
}

bool idPolygonModelCollisionDetection::TranslatePolygonVertsThroughTrm(
    const cmPolygon_t& polygon, const idCollisionModelLocal& model, const idTraceModel& traceModel,
    const idVec3& start, const idVec3& end, float& fraction) {
  bool hit = false;
  fraction = 1.0f;
  const idVec3 reverseEnd = start - (end - start);
  for (std::uint32_t vertex : polygon.vertices) {
    if (vertex >= model.vertices.size()) {
      continue;
    }
    queryParms_t parms;
    parms.start = model.vertices[vertex];
    parms.end = model.vertices[vertex] + (reverseEnd - start);
    const trace_t trace =
        CM_TraceBounds(CM_TranslatedBounds(traceModel.bounds, start), polygon.contents, parms);
    if (trace.fraction < fraction) {
      fraction = trace.fraction;
      hit = true;
    }
  }
  return hit;
}

bool idPolygonModelCollisionDetection::TranslateTrmThroughPolygon(
    const idTraceModel& traceModel, const idVec3& start, const idVec3& end,
    const cmPolygon_t& polygon, const idCollisionModelLocal& model, float& fraction) {
  float vertexFraction = 1.0f;
  float polygonFraction = 1.0f;
  const bool vertexHit =
      TranslateTrmVertsThroughPolygon(traceModel, start, end, polygon, vertexFraction);
  const bool polygonHit =
      TranslatePolygonVertsThroughTrm(polygon, model, traceModel, start, end, polygonFraction);
  fraction = std::min(vertexFraction, polygonFraction);
  return vertexHit || polygonHit;
}
