#pragma once

#include "shared/idlib/math/vector.h"

class idSphere {
public:
  idVec3 origin;
  float radius;

  bool RayIntersection(const idVec3& rayStart, const idVec3& rayDirection, float& scale1,
                       float& scale2) const;
};

static_assert(sizeof(idSphere) == 16, "idSphere ABI drift");
