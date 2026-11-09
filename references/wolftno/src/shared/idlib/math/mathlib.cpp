#include "mathlib.h"

// Clean definitions replacing compiler-generated initializers at
// RVAs 0x00BC3AB0 and 0x00BC3AD0.
float idMath::ONE_OVER_TWO_PI = 1.0f / idMath::TWO_PI;
__m128 idMath::vector_float_smallestNonDenorm = {
    idMath::FLT_SMALLEST_NON_DENORMAL, idMath::FLT_SMALLEST_NON_DENORMAL,
    idMath::FLT_SMALLEST_NON_DENORMAL, idMath::FLT_SMALLEST_NON_DENORMAL};
