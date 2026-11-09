#pragma once

#include "vector.h"

class idPlane {
public:
  float a;
  float b;
  float c;
  float d;

  bool PlaneIntersection(const idPlane& plane, idVec3& start, idVec3& direction) const;
  bool FromPoints(const idVec3& p1, const idVec3& p2, const idVec3& p3,
                  bool fixDegenerate = true);
};

static_assert(sizeof(idPlane) == 16, "idPlane ABI drift");
