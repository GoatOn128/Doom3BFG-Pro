#pragma once

#include "shared/idlib/bv/bounds.h"

struct idPolytope {
  static idBounds ConvexPolytopeBounds(const idPlane* planes, int numPlanes);
  static bool ConvexPolytopeContainsPoint(const idPlane* planes, int numPlanes, const idVec3& point,
                                          float epsilon);
  static void ConvexPolytopeFromBounds(const idBounds& bounds, idPlane planes[6]);
};
