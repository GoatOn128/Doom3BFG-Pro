#pragma once

#include "bounds.h"

#if defined(WOLFTNO_RETAIL_IDBOX_CLASS_ABI)
class idBox {
public:
#else
struct idBox {
#endif
  idVec3 center;
  idVec3 extents;
  idMat3 axis;

  void AxisProjection(const idMat3& projectionAxis, idBounds& bounds) const;
  bool AddPoint(const idVec3& point);
  bool IntersectsBox(const idBox& other) const;
  bool LineIntersection(const idVec3& start, const idVec3& end) const;
  void ToPoints(idVec3 points[8]) const;
  void FromPoints(const idVec3* points, int numPoints);
};

static_assert(sizeof(idBox) == 60, "idBox ABI drift");
