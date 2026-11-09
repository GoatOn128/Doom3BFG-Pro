#pragma once

#include "shared/idlib/math/vector.h"

struct idBounds2D {
  idVec2 bounds[2];

  bool AddPoint(const idVec2& point);
  bool ContainsPoint(const idVec2& point) const;
  bool IntersectBounds(const idBounds2D& other) const;
};

extern idBounds2D bounds2d_zero;
static_assert(sizeof(idBounds2D) == 16, "idBounds2D ABI drift");
