#include "engine/cm/collisionquery.h"

#include <cmath>

namespace {
float PlaneDistance(const idPlane& plane, const idVec3& point) {
  return plane.a * point.x + plane.b * point.y + plane.c * point.z + plane.d;
}
} // namespace

contactsResult_t
idPolygonModelCollisionDetection::FinishContacts(const contactsResult_t& contacts) {
  return contacts;
}

bool idPolygonModelCollisionDetection::TestTrmVertexInContactWithPolygon(const idVec3& vertex,
                                                                         const cmPolygon_t& polygon,
                                                                         float epsilon,
                                                                         contactInfo_t* contact) {
  const float distance = PlaneDistance(polygon.plane, vertex);
  if (std::fabs(distance) > epsilon || !CM_PointInsideBounds(vertex, polygon.bounds, epsilon)) {
    return false;
  }
  if (contact) {
    contact->point = vertex;
    contact->normal = {polygon.plane.a, polygon.plane.b, polygon.plane.c};
    contact->distance = distance;
    contact->contents = polygon.contents;
    contact->material = polygon.material;
  }
  return true;
}

bool idPolygonModelCollisionDetection::TestTrmEdgeInContactWithPolygon(const idVec3& start,
                                                                       const idVec3& end,
                                                                       const cmPolygon_t& polygon,
                                                                       float epsilon,
                                                                       contactInfo_t* contact) {
  const float startDistance = PlaneDistance(polygon.plane, start);
  const float endDistance = PlaneDistance(polygon.plane, end);
  if ((startDistance > epsilon && endDistance > epsilon) ||
      (startDistance < -epsilon && endDistance < -epsilon)) {
    return false;
  }
  const float denominator = startDistance - endDistance;
  const float fraction = std::fabs(denominator) > 1.0e-8f ? startDistance / denominator : 0.0f;
  const idVec3 point = start + (end - start) * fraction;
  return TestTrmVertexInContactWithPolygon(point, polygon, epsilon, contact);
}

bool idPolygonModelCollisionDetection::TestVertexInContactWithTrmPolygon(
    const idVec3& vertex, const idTraceModel& traceModel, const idVec3& origin, float epsilon) {
  const idVec3 local = vertex - origin;
  if (traceModel.ContainsPoint(local)) {
    return true;
  }
  for (std::uint32_t polygon = 0; polygon < traceModel.numPolys; ++polygon) {
    const idPlane plane{traceModel.polyPlaneX[polygon], traceModel.polyPlaneY[polygon],
                        traceModel.polyPlaneZ[polygon], traceModel.polyPlaneW[polygon]};
    if (std::fabs(PlaneDistance(plane, local)) <= epsilon) {
      return true;
    }
  }
  return false;
}

bool idPolygonModelCollisionDetection::TestTrmInContactWithPolygon(const idTraceModel& traceModel,
                                                                   const idVec3& origin,
                                                                   const cmPolygon_t& polygon,
                                                                   float epsilon,
                                                                   contactInfo_t* contact) {
  for (std::uint32_t vertex = 0; vertex < traceModel.numVerts; ++vertex) {
    if (TestTrmVertexInContactWithPolygon(traceModel.GetVertex(vertex) + origin, polygon, epsilon,
                                          contact)) {
      return true;
    }
  }
  for (std::uint32_t edge = 0; edge < traceModel.numEdges; ++edge) {
    const idVec3 start = traceModel.GetVertex(traceModel.edges[edge].v[0]) + origin;
    const idVec3 end = traceModel.GetVertex(traceModel.edges[edge].v[1]) + origin;
    if (TestTrmEdgeInContactWithPolygon(start, end, polygon, epsilon, contact)) {
      return true;
    }
  }
  return false;
}

contactsResult_t idPolygonModelCollisionDetection::StartContacts(const idCollisionModelLocal& model,
                                                                 const queryParms_t& parms,
                                                                 float distance) {
  contactsResult_t result;
  idBounds expanded = model.GetBounds();
  for (int axis = 0; axis < 3; ++axis) {
    expanded[0][axis] -= distance;
    expanded[1][axis] += distance;
  }
  queryParms_t contactParms = parms;
  contactParms.end = parms.start;
  const trace_t trace = CM_TraceBounds(expanded, model.GetContents(), contactParms);
  if (trace.startSolid || trace.fraction < 1.0f) {
    contactInfo_t contact = trace.contact;
    contact.contents = model.GetContents() & parms.contentsMask;
    if (contact.normal.LengthSqr() == 0.0f) {
      const idVec3 center = (model.GetBounds()[0] + model.GetBounds()[1]) * 0.5f;
      contact.normal = parms.start - center;
      if (contact.normal.Normalize() == 0.0f) {
        contact.normal = {0.0f, 0.0f, 1.0f};
      }
    }
    result.contacts.push_back(contact);
  }
  return result;
}
