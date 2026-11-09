#include "geometry.h"

#include "shared/idlib/math/mathlib.h"

#include <algorithm>
#include <cmath>

float idGeometry::ClosestPositionOnRay(const idVec3& point, const idVec3& rayStart,
                                       const idVec3& rayDirection) {
  return (point - rayStart) * rayDirection;
}

float idGeometry::ClosestPositionOnLineSegment(const idVec3& point, const idVec3& start,
                                               const idVec3& end) {
  const idVec3 direction = end - start;
  const float lengthSqr = direction.LengthSqr();
  if (lengthSqr < idMath::FLT_SMALLEST_NON_DENORMAL)
    return 0.0f;
  return std::clamp(((point - start) * direction) / lengthSqr, 0.0f, 1.0f);
}

bool idGeometry::RayIntersectsLineSegment2D(const idVec2& rayStart, const idVec2& rayDirection,
                                            const idVec2& segmentStart, const idVec2& segmentEnd,
                                            float& fraction) {
  const idVec2 segment = segmentEnd - segmentStart;
  const idVec2 delta = segmentStart - rayStart;
  const float denominator = rayDirection.x * segment.y - rayDirection.y * segment.x;
  if (denominator * denominator > 1.0e-4f) {
    const float segmentFraction =
        (rayDirection.y * delta.x - rayDirection.x * delta.y) / denominator;
    if (segmentFraction < 0.0f || segmentFraction > 1.0f)
      return false;
    fraction = (delta.x * segment.y - delta.y * segment.x) / denominator;
    return fraction >= 0.0f;
  }
  const float cross = rayDirection.y * delta.x - rayDirection.x * delta.y;
  if (cross * cross > 1.0e-4f)
    return false;
  const float rayLengthSqr = rayDirection.LengthSqr();
  if (rayLengthSqr <= idMath::FLT_SMALLEST_NON_DENORMAL)
    return false;
  const float first = (segmentStart - rayStart) * rayDirection / rayLengthSqr;
  const float second = (segmentEnd - rayStart) * rayDirection / rayLengthSqr;
  const float nearest = std::min(first, second);
  const float farthest = std::max(first, second);
  if (farthest < 0.0f)
    return false;
  fraction = std::max(0.0f, nearest);
  return true;
}

void idGeometry::RayToRayClosestPoints(const idVec3& start1, const idVec3& direction1,
                                       const idVec3& start2, const idVec3& direction2,
                                       idVec3& output1, idVec3& output2, float& distance1,
                                       float& distance2) {
  const idVec3 separation = start1 - start2;
  const float a = direction1 * direction1;
  const float b = direction1 * direction2;
  const float c = direction2 * direction2;
  const float d = direction1 * separation;
  const float e = direction2 * separation;
  const float denominator = a * c - b * b;
  if (denominator > idMath::FLOAT_EPSILON) {
    distance1 = std::max(0.0f, (b * e - c * d) / denominator);
    distance2 = std::max(0.0f, (a * e - b * d) / denominator);
    if (distance1 == 0.0f && c > 0.0f)
      distance2 = std::max(0.0f, e / c);
    if (distance2 == 0.0f && a > 0.0f)
      distance1 = std::max(0.0f, -d / a);
  } else {
    distance1 = a > 0.0f ? std::max(0.0f, -d / a) : 0.0f;
    distance2 = 0.0f;
  }
  output1 = start1 + direction1 * distance1;
  output2 = start2 + direction2 * distance2;
}

idVec3 idGeometry::FindNearestPerpendicular(const idVec3& direction1, const idVec3& direction2,
                                            const idVec3& hint) {
  idVec3 result = direction2.Cross(direction1);
  if (result * hint < 0.0f)
    result = -result;
  result.Normalize();
  return result;
}

idVec3 idGeometry::TriangleNormal(const idVec3& a, const idVec3& b, const idVec3& c) {
  idVec3 result = (c - a).Cross(b - a);
  result.Normalize();
  return result;
}

float idGeometry::TriangleArea(const idVec3& a, const idVec3& b, const idVec3& c) {
  return (c - a).Cross(b - a).Length() * 0.5f;
}
