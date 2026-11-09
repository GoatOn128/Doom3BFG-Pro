#include "bounds.h"

#include "sphere.h"
#include "shared/idlib/math/mathlib.h"

#include <algorithm>
#include <cmath>

const idBounds bounds_zero{{{0.0f, 0.0f, 0.0f},
                            {0.0f, 0.0f, 0.0f}}};
const idBounds bounds_zeroOneCube{{{0.0f, 0.0f, 0.0f},
                                   {1.0f, 1.0f, 1.0f}}};
const idBounds bounds_unitCube{{{-1.0f, -1.0f, -1.0f},
                                {1.0f, 1.0f, 1.0f}}};

namespace {
idVec3 TransformPoint(const idVec3& point, const idVec3& origin, const idMat3& axis) {
  return origin + axis * point;
}

void AddRotatedPointExtrema(idBounds& output, const idVec3& point, const idRotation& rotation) {
  output.AddPoint(point);
  output.AddPoint(rotation * point);
  if (rotation.angle == 0.0f)
    return;

  idVec3 rotationAxis = rotation.vec;
  if (rotationAxis.Normalize() == 0.0f)
    return;
  const idVec3 relative = point - rotation.origin;
  const idVec3 constant = rotation.origin + rotationAxis * (rotationAxis * relative);
  const idVec3 cosine = point - constant;
  const idVec3 quarterTurn = idRotation(rotation.origin, rotationAxis, 90.0f) * point;
  const idVec3 sine = quarterTurn - constant;
  const float start = std::min(0.0f, rotation.angle) * idMath::M_DEG2RAD;
  const float end = std::max(0.0f, rotation.angle) * idMath::M_DEG2RAD;
  for (int component = 0; component < 3; ++component) {
    const float base = std::atan2(sine[component], cosine[component]);
    const int firstCycle = static_cast<int>(std::floor((start - base) / idMath::PI)) - 1;
    const int lastCycle = static_cast<int>(std::ceil((end - base) / idMath::PI)) + 1;
    for (int cycle = firstCycle; cycle <= lastCycle; ++cycle) {
      const float radians = base + static_cast<float>(cycle) * idMath::PI;
      if (radians < start || radians > end)
        continue;
      output.AddPoint(idRotation(rotation.origin, rotationAxis, radians * idMath::M_RAD2DEG) *
                      point);
    }
  }
}
} // namespace

void idBounds::Clear() {
  b[0] = {1.0e30f, 1.0e30f, 1.0e30f};
  b[1] = {-1.0e30f, -1.0e30f, -1.0e30f};
}

bool idBounds::AddPoint(const idVec3& point) {
  bool expanded = false;
  for (int component = 0; component < 3; ++component) {
    if (point[component] < b[0][component]) {
      b[0][component] = point[component];
      expanded = true;
    }
    if (point[component] > b[1][component]) {
      b[1][component] = point[component];
      expanded = true;
    }
  }
  return expanded;
}

float idBounds::GetRadius() const {
  return GetRadius(vec3_origin);
}

float idBounds::GetInternalRadiusXY() const {
  return std::max((b[1].x - b[0].x) * 0.5f, (b[1].y - b[0].y) * 0.5f);
}

float idBounds::GetRadiusXY() const {
  const float xRadius = std::max(std::fabs(b[0].x), std::fabs(b[1].x));
  const float yRadius = std::max(std::fabs(b[0].y), std::fabs(b[1].y));
  return std::sqrt(xRadius * xRadius + yRadius * yRadius);
}

float idBounds::GetRadius(const idVec3& center) const {
  float radiusSqr = 0.0f;
  for (int component = 0; component < 3; ++component) {
    const float distance = std::max(std::fabs(center[component] - b[0][component]),
                                    std::fabs(b[1][component] - center[component]));
    radiusSqr += distance * distance;
  }
  return std::sqrt(radiusSqr);
}

planeSide_t idBounds::PlaneSide(const idPlane& plane, float epsilon) const {
  const idVec3 center = (b[0] + b[1]) * 0.5f;
  const idVec3 extents = b[1] - center;
  const idVec3 normal{plane.a, plane.b, plane.c};
  const float distance = normal * center + plane.d;
  const float radius = std::fabs(extents.x * normal.x) + std::fabs(extents.y * normal.y) +
                       std::fabs(extents.z * normal.z);
  if (distance - radius > epsilon)
    return PLANESIDE_FRONT;
  if (distance + radius < -epsilon)
    return PLANESIDE_BACK;
  return PLANESIDE_CROSS;
}

bool idBounds::LineIntersection(const idVec3& start, const idVec3& end) const {
  const idVec3 direction = end - start;
  float nearScale = 0.0f;
  float farScale = 1.0f;
  for (int component = 0; component < 3; ++component) {
    if (std::fabs(direction[component]) <= idMath::FLT_SMALLEST_NON_DENORMAL) {
      if (start[component] < b[0][component] || start[component] > b[1][component])
        return false;
      continue;
    }
    float first = (b[0][component] - start[component]) / direction[component];
    float second = (b[1][component] - start[component]) / direction[component];
    if (first > second)
      std::swap(first, second);
    nearScale = std::max(nearScale, first);
    farScale = std::min(farScale, second);
    if (nearScale > farScale)
      return false;
  }
  return true;
}

bool idBounds::RayIntersection(const idVec3& rayStart, const idVec3& rayDirection, float& scale1,
                               float& scale2) const {
  scale1 = -1.0e30f;
  scale2 = 1.0e30f;
  for (int component = 0; component < 3; ++component) {
    if (std::fabs(rayDirection[component]) <= idMath::FLT_SMALLEST_NON_DENORMAL) {
      if (rayStart[component] < b[0][component] || rayStart[component] > b[1][component])
        return false;
      continue;
    }
    float first = (b[0][component] - rayStart[component]) / rayDirection[component];
    float second = (b[1][component] - rayStart[component]) / rayDirection[component];
    if (first > second)
      std::swap(first, second);
    scale1 = std::max(scale1, first);
    scale2 = std::min(scale2, second);
    if (scale1 > scale2)
      return false;
  }
  return true;
}

void idBounds::FromTransformedBounds(const idBounds& bounds, const idVec3& origin,
                                     const idMat3& axis) {
  const idVec3 sourceCenter = (bounds.b[0] + bounds.b[1]) * 0.5f;
  const idVec3 sourceExtents = bounds.b[1] - sourceCenter;
  const idVec3 transformedCenter = TransformPoint(sourceCenter, origin, axis);
  idVec3 transformedExtents{};
  for (int component = 0; component < 3; ++component) {
    transformedExtents[component] = std::fabs(axis.mat[0][component] * sourceExtents.x) +
                                    std::fabs(axis.mat[1][component] * sourceExtents.y) +
                                    std::fabs(axis.mat[2][component] * sourceExtents.z);
  }
  b[0] = transformedCenter - transformedExtents;
  b[1] = transformedCenter + transformedExtents;
}

void idBounds::FromTransformedBounds(const idBounds& bounds, const idVec3& origin,
                                     const idMat3& axis, const idVec3& scale) {
  Clear();
  for (int corner = 0; corner < 8; ++corner) {
    idVec3 point{bounds.b[(corner >> 0) & 1].x * scale.x, bounds.b[(corner >> 1) & 1].y * scale.y,
                 bounds.b[(corner >> 2) & 1].z * scale.z};
    AddPoint(TransformPoint(point, origin, axis));
  }
}

void idBounds::FromModelMatrixBounds(const idBounds& bounds, const idRenderMatrix& matrix) {
  const idVec3 center = (bounds.b[0] + bounds.b[1]) * 0.5f;
  const idVec3 extents = bounds.b[1] - center;
  const idVec3 transformedCenter{
      center.x * matrix.m[0] + center.y * matrix.m[1] + center.z * matrix.m[2] + matrix.m[3],
      center.x * matrix.m[4] + center.y * matrix.m[5] + center.z * matrix.m[6] + matrix.m[7],
      center.x * matrix.m[8] + center.y * matrix.m[9] + center.z * matrix.m[10] + matrix.m[11]};
  const idVec3 transformedExtents{
      std::fabs(extents.x * matrix.m[0]) + std::fabs(extents.y * matrix.m[1]) +
          std::fabs(extents.z * matrix.m[2]),
      std::fabs(extents.x * matrix.m[4]) + std::fabs(extents.y * matrix.m[5]) +
          std::fabs(extents.z * matrix.m[6]),
      std::fabs(extents.x * matrix.m[8]) + std::fabs(extents.y * matrix.m[9]) +
          std::fabs(extents.z * matrix.m[10])};
  b[0] = transformedCenter - transformedExtents;
  b[1] = transformedCenter + transformedExtents;
}

void idBounds::FromBoundsTranslation(const idBounds& bounds, const idVec3& boundsOrigin,
                                     const idMat3& boundsAxis, const idVec3& translation) {
  FromTransformedBounds(bounds, boundsOrigin, boundsAxis);
  for (int component = 0; component < 3; ++component) {
    if (translation[component] < 0.0f)
      b[0][component] += translation[component];
    else
      b[1][component] += translation[component];
  }
}

void idBounds::FromPointRotation180(const idVec3& point, const idRotation& rotation) {
  Clear();
  AddRotatedPointExtrema(*this, point, rotation);
}

void idBounds::FromPointRotation(const idVec3& point, const idRotation& rotation) {
  Clear();
  AddRotatedPointExtrema(*this, point, rotation);
}

void idBounds::FromBoundsRotation180(const idBounds& bounds, const idVec3& boundsOrigin,
                                     const idMat3& boundsAxis, const idRotation& rotation) {
  Clear();
  for (int corner = 0; corner < 8; ++corner) {
    const idVec3 local{bounds.b[(corner >> 0) & 1].x, bounds.b[(corner >> 1) & 1].y,
                       bounds.b[(corner >> 2) & 1].z};
    AddRotatedPointExtrema(*this, TransformPoint(local, boundsOrigin, boundsAxis), rotation);
  }
}

void idBounds::FromBoundsRotation(const idBounds& bounds, const idVec3& boundsOrigin,
                                  const idMat3& boundsAxis, const idRotation& rotation) {
  FromBoundsRotation180(bounds, boundsOrigin, boundsAxis, rotation);
}

bool idBounds::SphereIntersection(const idSphere& sphere) const {
  float distanceSqr = 0.0f;
  for (int component = 0; component < 3; ++component) {
    const float closest = std::clamp(sphere.origin[component], b[0][component], b[1][component]);
    const float distance = sphere.origin[component] - closest;
    distanceSqr += distance * distance;
  }
  return distanceSqr <= sphere.radius * sphere.radius;
}
