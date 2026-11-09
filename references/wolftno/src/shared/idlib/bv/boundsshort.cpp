#include "boundsshort.h"

// Constants referenced by the generated initializers at RVAs 0x00BC4B30 and
// 0x00BC4B50 are four 0xFFFFFFFF words and four zero words in retail .rdata.
const __m128i idBoundsShort::vector_int_max_ushort = [] { return _mm_set1_epi32(-1); }();
const __m128i idBoundsShort::vector_int_zero = [] { return _mm_setzero_si128(); }();
