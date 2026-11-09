#include "engine/renderer/triangles.h"

#include "shared/idlib/geometry/geometry.h"

#include <immintrin.h>

namespace {

// These constants come from the SIMD helper header included by the retail
// translation unit.  They are intentionally initialized through intrinsics:
// the shipping object owns one dynamic initializer for each constant.
alignas(16) const __m128i vector_int_zero = _mm_setzero_si128();
alignas(16) const __m128i vector_int_one = _mm_set1_epi32(1);
alignas(16) const __m128 vector_float_mask_clear_last =
    _mm_castsi128_ps(_mm_setr_epi32(-1, -1, -1, 0));

} // namespace

float idTriangles::GetSurfaceArea() const {
  if (indexes == nullptr)
    return 0.0f;
  float area = 0.0f;
  for (int i = 0; i < numIndexes; i += 3) {
    area += idGeometry::TriangleArea(verts[indexes[i]].xyz,
                                     verts[indexes[i + 1]].xyz,
                                     verts[indexes[i + 2]].xyz);
  }
  return area;
}
