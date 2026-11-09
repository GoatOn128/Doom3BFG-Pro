#pragma once

#include <emmintrin.h>

struct idBoundsShort {
  short b[2][3];
  static const __m128i vector_int_max_ushort;
  static const __m128i vector_int_zero;
};

static_assert(sizeof(idBoundsShort) == 12, "idBoundsShort ABI drift");
