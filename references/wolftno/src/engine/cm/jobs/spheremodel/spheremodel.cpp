#include "engine/cm/collisionquery.h"

#include <algorithm>
#include <cmath>

bool idSphereModelCollisionDetection::SetupCollisionSpherePtrs(const idCollisionModelLocal& model,
                                                               idVec3& center, float& radius) {
  if (model.GetType() != CM_TYPE_SPHERE || model.sphereRadius < 0.0f) {
    center = {};
    radius = 0.0f;
    return false;
  }
  center = (model.GetBounds()[0] + model.GetBounds()[1]) * 0.5f;
  radius = model.sphereRadius;
  return true;
}

trace_t idSphereModelCollisionDetection::TraceThroughModel(const idCollisionModelLocal& model,
                                                           const queryParms_t& parms) {
  trace_t result;
  result.endpos = parms.end;
  result.endAxis = parms.axis;
  if ((model.GetContents() & parms.contentsMask) == 0) {
    return result;
  }
  float radius = model.sphereRadius;
  idVec3 center{};
  if (!SetupCollisionSpherePtrs(model, center, radius)) {
    return result;
  }
  if (parms.traceModel) {
    radius += parms.traceModel->bounds.GetRadius();
  }
  const idVec3 start = parms.start - center;
  const idVec3 direction = parms.end - parms.start;
  const float c = start.LengthSqr() - radius * radius;
  if (c <= 0.0f) {
    result.fraction = 0.0f;
    result.endpos = start;
    result.startSolid = true;
    result.allSolid = (parms.end - center).LengthSqr() <= radius * radius;
    result.contact.normal = start;
    result.contact.normal.Normalize();
    result.contact.point = parms.start;
    result.contact.contents = model.GetContents();
    return result;
  }
  const float a = direction.LengthSqr();
  if (a <= 1.0e-12f) {
    return result;
  }
  const float b = start * direction;
  const float discriminant = b * b - a * c;
  if (discriminant < 0.0f) {
    return result;
  }
  const float fraction = (-b - std::sqrt(discriminant)) / a;
  if (fraction < 0.0f || fraction > 1.0f) {
    return result;
  }
  result.fraction = std::max(0.0f, fraction - 1.0e-5f);
  result.endpos = start + direction * result.fraction;
  result.contact.point = result.endpos;
  result.contact.normal = result.endpos - center;
  result.contact.normal.Normalize();
  result.contact.distance = radius;
  result.contact.contents = model.GetContents();
  return result;
}
