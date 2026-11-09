#pragma once

#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/math/vector.h"

#include <cstdint>

class idConvexHull {
public:
  void Create(const idList<idVec2i>& points, float shrink = 0.0f);

  idStaticList<idVec2, 32> vertices;

private:
#if defined(WOLF_CONVEXHULL_TEST_ACCESS)
  friend struct idConvexHullTestAccess;
#endif
  static void QuickSortWeights(const float* weights, std::int16_t* indices,
                               int num);
  static void QuickSortPoints(const idVec2i* points, std::int16_t* indices,
                              int num);
};

static_assert(sizeof(idConvexHull) == 280, "idConvexHull ABI drift");
