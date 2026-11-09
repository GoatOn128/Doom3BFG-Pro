#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/aas2/aas2callback_avoidlocation.h"
#include "shared/idlib/math/mathlib.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI

#include <cstdint>
#include <xmmintrin.h>

namespace {
float RetailSquareRoot(float value) {
  const __m128 input = _mm_set_ss(value);
  return _mm_cvtss_f32(
      _mm_sqrt_ps(_mm_max_ps(input, idMath::vector_float_smallestNonDenorm)));
}
} // namespace

idAAS2Callback_AvoidLocation::idAAS2Callback_AvoidLocation() {
  *reinterpret_cast<std::uint64_t*>(&avoidLocation.y) = 0;
  avoidLocation.x = 0.0f;
  avoidDist = 0.0f;
  obstacles = nullptr;
  numObstacles = 0;
}

idAAS2Callback_AvoidLocation::~idAAS2Callback_AvoidLocation() {
  obstacles = nullptr;
}

bool idAAS2Callback_AvoidLocation::PathValid(const idAAS2*,
                                             const idVec3& start,
                                             const idVec3& end) {
  if (obstacles == nullptr) {
    return true;
  }

  for (int obstacleIndex = 0; obstacleIndex < numObstacles; ++obstacleIndex) {
    if (obstacles[obstacleIndex].expAbsBounds.LineIntersection(start, end)) {
      return false;
    }
  }
  return true;
}

int idAAS2Callback_AvoidLocation::AdditionalTravelTimeForPath(
    const idAAS2*, const idVec3& start, const idVec3& end) {
  if (avoidDist <= 0.0f) {
    return 0;
  }

  const float deltaX = end.x - start.x;
  const float deltaY = end.y - start.y;
  const float deltaZ = end.z - start.z;
  const float lengthSquaredXY = deltaY * deltaY + deltaX * deltaX;
  const float inverseLength =
      1.0f / RetailSquareRoot(lengthSquaredXY + deltaZ * deltaZ);
  const float directionX = deltaX * inverseLength;
  const float directionY = deltaY * inverseLength;
  const float directionZ = deltaZ * inverseLength;

  const float dotXY = (avoidLocation.y - start.y) * directionY +
                      (avoidLocation.x - start.x) * directionX;
  const float projectionDistance =
      dotXY + (avoidLocation.z - start.z) * directionZ;
  const float projectedY = start.y + directionY * projectionDistance;
  const float projectedZ = start.z + directionZ * projectionDistance;
  const float projectedX = start.x + projectionDistance * directionX;
  const idVec3 projected{projectedX, projectedY, projectedZ};

  bool projectionOnSegment = true;
  for (int component = 0; component < 3; ++component) {
    if ((projected[component] > start[component] + 0.1f &&
         projected[component] > end[component] + 0.1f) ||
        (start[component] - 0.1f > projected[component] &&
         end[component] - 0.1f > projected[component])) {
      projectionOnSegment = false;
      break;
    }
  }

  const idVec3& closestPoint = projectionOnSegment ? projected : end;
  const float distanceX = avoidLocation.x - closestPoint.x;
  const float distanceY = avoidLocation.y - closestPoint.y;
  const float distanceZ = avoidLocation.z - closestPoint.z;
  const float distanceSquaredXY =
      distanceY * distanceY + distanceX * distanceX;
  const float distance =
      RetailSquareRoot(distanceSquaredXY + distanceZ * distanceZ);

  if (!(avoidDist > distance)) {
    return 0;
  }

  const __m128 travelTime = _mm_set_ss((avoidDist - distance) * 10.0f);
  return _mm_cvtt_ss2si(travelTime);
}
