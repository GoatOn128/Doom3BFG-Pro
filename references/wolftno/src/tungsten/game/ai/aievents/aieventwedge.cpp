#include "tungsten/game/ai/aievents/aievents.h"

#include <algorithm>
#include <cmath>

namespace {

float PlaneDistance(const idPlane& plane, const idVec3& point) {
  return plane.a * point.x + plane.b * point.y + plane.c * point.z +
         plane.d;
}

idPlane PlaneThroughPoint(const idVec3& normal, const idVec3& point) {
  return {normal.x, normal.y, normal.z, -(normal * point)};
}

bool BoundsEntirelyInFront(const idPlane& plane, const idBounds& bounds,
                           float epsilon) {
  const idVec3 closest = {
      plane.a >= 0.0F ? bounds[0].x : bounds[1].x,
      plane.b >= 0.0F ? bounds[0].y : bounds[1].y,
      plane.c >= 0.0F ? bounds[0].z : bounds[1].z,
  };
  return PlaneDistance(plane, closest) > epsilon;
}

float DistanceToBounds(const idVec3& point, const idBounds& bounds) {
  const float dx = std::max({bounds[0].x - point.x, 0.0F,
                             point.x - bounds[1].x});
  const float dy = std::max({bounds[0].y - point.y, 0.0F,
                             point.y - bounds[1].y});
  const float dz = std::max({bounds[0].z - point.z, 0.0F,
                             point.z - bounds[1].z});
  return std::sqrt(dx * dx + dy * dy + dz * dz);
}

}  // namespace

// DIA: idAIEventWedge::InternalUpdateAttached
idAIEvent::aiEventUpdateResult_t idAIEventWedge::InternalUpdateAttached(
    int currentTime) {
  lastRefresh = -1;
  return idAIEvent::InternalUpdateAttached(currentTime);
}

// DIA: idAIEventWedge::idAIEventWedge
idAIEventWedge::idAIEventWedge() = default;

idAIEventWedge::~idAIEventWedge() = default;

// DIA: idAIEventWedge::`scalar deleting destructor'

// DIA: idAIEventWedge::Cache
void idAIEventWedge::Cache(int currentTime) {
  if (lastRefresh >= currentTime || direction.LengthSqr() < 1.0F) return;

  idVec3 forward = direction;
  forward.Normalize();
  idVec3 up{0.0F, 0.0F, 1.0F};
  if (std::fabs(forward * up) > 0.9999F) up = {0.0F, 1.0F, 0.0F};
  idVec3 left = up.Cross(forward);
  left.Normalize();
  up = forward.Cross(left);
  up.Normalize();
  axis[0] = forward;
  axis[1] = left;
  axis[2] = up;

  const idDeclAiEvent* declaration = GetDeclaration();
  const float halfArc =
      (declaration != nullptr ? declaration->arcWidthDegrees : 0.0F) *
      0.5F * 0.01745329251994329577F;
  const float sine = std::sin(halfArc);
  const float cosine = std::cos(halfArc);
  const idVec3 leftNormal = left * cosine - forward * sine;
  const idVec3 rightNormal = -left * cosine - forward * sine;
  const float heightMax = declaration != nullptr ? declaration->heightMax : 0.0F;
  const float heightMin = declaration != nullptr ? declaration->heightMin : 0.0F;
  planes[0] = PlaneThroughPoint(up, GetOrigin() + up * heightMax);
  planes[1] = PlaneThroughPoint(-up, GetOrigin() - up * heightMin);
  planes[2] = PlaneThroughPoint(leftNormal, GetOrigin());
  planes[3] = PlaneThroughPoint(rightNormal, GetOrigin());
  lastRefresh = currentTime;
}

// DIA: idAIEventWedge::InternalIsTouching
bool idAIEventWedge::InternalIsTouching(idEntity* entity, int currentTime) {
  if (entity == nullptr || entity->GetPhysics() == nullptr || radius == 0.0F)
    return false;
  const idBounds& bounds = entity->GetPhysics()->GetAbsBounds();
  const float distance = DistanceToBounds(GetOrigin(), bounds);
  if (distance < 1.0F || distance > radius) return false;
  Cache(currentTime);
  if (lastRefresh < 0) return false;
  for (const idPlane& plane : planes)
    if (BoundsEntirelyInFront(plane, bounds, 0.1F)) return false;
  return true;
}

// DIA: idAIEventWedge::InternalDrawDebug
void idAIEventWedge::InternalDrawDebug(int level, int currentTime,
                                       int duration) {
  Cache(currentTime);
  debugDuration =
      duration > 0
          ? duration
          : (gameLocal != nullptr ? gameLocal->GetGameMsPerFrame() : 16);
  debugPrimitiveCount =
      level > 0 && lastRefresh >= 0 && direction.LengthSqr() >= 1.0F ? 10 : 0;
}
