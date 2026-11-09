#pragma once

#include "shared/idlib/math/vector.h"

struct idGeometry {
  static float ClosestPositionOnRay(const idVec3& point, const idVec3& rayStart,
                                    const idVec3& rayDirection);
  static float ClosestPositionOnLineSegment(const idVec3& point, const idVec3& start,
                                            const idVec3& end);
  static bool RayIntersectsLineSegment2D(const idVec2& rayStart, const idVec2& rayDirection,
                                         const idVec2& segmentStart, const idVec2& segmentEnd,
                                         float& fraction);
  static void RayToRayClosestPoints(const idVec3& start1, const idVec3& direction1,
                                    const idVec3& start2, const idVec3& direction2, idVec3& output1,
                                    idVec3& output2, float& distance1, float& distance2);
  static idVec3 FindNearestPerpendicular(const idVec3& direction1, const idVec3& direction2,
                                         const idVec3& hint);
  static idVec3 TriangleNormal(const idVec3& a, const idVec3& b, const idVec3& c);
  static float TriangleArea(const idVec3& a, const idVec3& b, const idVec3& c);
};
