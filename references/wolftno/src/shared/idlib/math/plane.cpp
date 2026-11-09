#include "plane.h"

#include "mathlib.h"

#include <cmath>

bool idPlane::PlaneIntersection(const idPlane& plane, idVec3& start, idVec3& direction) const {
  const float length1 = a * a + b * b + c * c;
  const float dot = plane.a * a + plane.b * b + plane.c * c;
  const float length2 = plane.a * plane.a + plane.b * plane.b + plane.c * plane.c;
  const float determinant = length1 * length2 - dot * dot;
  if (std::fabs(determinant) < idMath::FLT_SMALLEST_NON_DENORMAL) {
    return false;
  }

  const float inverseDeterminant = 1.0f / determinant;
  const float scale1 = (plane.d * dot - d * length2) * inverseDeterminant;
  const float scale2 = (d * dot - plane.d * length1) * inverseDeterminant;
  direction.x = b * plane.c - c * plane.b;
  direction.y = c * plane.a - plane.c * a;
  direction.z = plane.b * a - b * plane.a;
  start.x = scale1 * a + scale2 * plane.a;
  start.y = scale1 * b + scale2 * plane.b;
  start.z = scale1 * c + scale2 * plane.c;
  return true;
}
