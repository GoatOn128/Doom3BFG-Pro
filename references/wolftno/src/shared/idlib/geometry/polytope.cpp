#include "polytope.h"

#include <cmath>

idBounds idPolytope::ConvexPolytopeBounds(const idPlane* planes, int numPlanes) {
  idBounds result;
  result.Clear();
  if (planes == nullptr || numPlanes < 3)
    return result;
  for (int first = 0; first < numPlanes - 2; ++first) {
    const idVec3 normal1{planes[first].a, planes[first].b, planes[first].c};
    for (int second = first + 1; second < numPlanes - 1; ++second) {
      const idVec3 normal2{planes[second].a, planes[second].b, planes[second].c};
      for (int third = second + 1; third < numPlanes; ++third) {
        const idVec3 normal3{planes[third].a, planes[third].b, planes[third].c};
        const float determinant = normal1 * normal2.Cross(normal3);
        if (std::fabs(determinant) < 1.0e-8f)
          continue;
        const idVec3 point = (normal2.Cross(normal3) * -planes[first].d +
                              normal3.Cross(normal1) * -planes[second].d +
                              normal1.Cross(normal2) * -planes[third].d) /
                             determinant;
        if (ConvexPolytopeContainsPoint(planes, numPlanes, point, 1.0e-4f)) {
          result.AddPoint(point);
        }
      }
    }
  }
  return result;
}

bool idPolytope::ConvexPolytopeContainsPoint(const idPlane* planes, int numPlanes,
                                             const idVec3& point, float epsilon) {
  for (int index = 0; index < numPlanes; ++index) {
    if (planes[index].a * point.x + planes[index].b * point.y + planes[index].c * point.z +
            planes[index].d >
        epsilon)
      return false;
  }
  return true;
}

void idPolytope::ConvexPolytopeFromBounds(const idBounds& bounds, idPlane planes[6]) {
  planes[0] = {1.0f, 0.0f, 0.0f, -bounds.b[1].x};
  planes[1] = {-1.0f, 0.0f, 0.0f, bounds.b[0].x};
  planes[2] = {0.0f, 1.0f, 0.0f, -bounds.b[1].y};
  planes[3] = {0.0f, -1.0f, 0.0f, bounds.b[0].y};
  planes[4] = {0.0f, 0.0f, 1.0f, -bounds.b[1].z};
  planes[5] = {0.0f, 0.0f, -1.0f, bounds.b[0].z};
}
