#include "sphere.h"

#include "shared/idlib/math/mathlib.h"

#include <algorithm>
#include <cmath>

bool idSphere::RayIntersection(const idVec3& rayStart, const idVec3& rayDirection, float& scale1,
                               float& scale2) const {
  const idVec3 offset{rayStart.x - origin.x, rayStart.y - origin.y, rayStart.z - origin.z};
  const float directionLength = rayDirection.x * rayDirection.x + rayDirection.y * rayDirection.y +
                                rayDirection.z * rayDirection.z;
  const float projected =
      rayDirection.x * offset.x + rayDirection.y * offset.y + rayDirection.z * offset.z;
  const float discriminant =
      projected * projected -
      ((offset.x * offset.x + offset.y * offset.y + offset.z * offset.z) - radius * radius) *
          directionLength;
  if (discriminant < 0.0f)
    return false;
  const float root = std::sqrt(std::max(discriminant, idMath::FLT_SMALLEST_NON_DENORMAL));
  const float inverseLength = 1.0f / directionLength;
  scale1 = (root - projected) * inverseLength;
  scale2 = (-projected - root) * inverseLength;
  return true;
}
