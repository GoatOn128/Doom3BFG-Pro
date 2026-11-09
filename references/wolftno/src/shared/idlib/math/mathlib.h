#pragma once

#include <xmmintrin.h>

struct idMath {
  static constexpr float PI = 3.14159265358979323846f;
  static constexpr float TWO_PI = 6.28318530717958647692f;
  static constexpr float M_DEG2RAD = PI / 180.0f;
  static constexpr float M_RAD2DEG = 180.0f / PI;
#if defined(WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI)
#ifdef INFINITY
#undef INFINITY
#endif
  static const float INFINITY;
  static const float FLOAT_EPSILON;
#else
  static constexpr float FLOAT_EPSILON = 1.192092896e-07f;
#endif
  static constexpr float FLT_SMALLEST_NON_DENORMAL = 1.175494351e-38f;
  static float ATan16(float y, float x);
  static float Exp16(float f);
  static float Sin16(float a);
  static float Cos16(float a);
  static float Cos(float a);
  static bool IsValid(float f);
  static float ASin(float value);
  static float ACos16(float value);
  static float AngleNormalize360(float angle);
  static float AngleNormalize180(float angle);
  static float AngleDelta(float angle1, float angle2);
  static void SinCos16(float a, float& s, float& c);
  static float ONE_OVER_TWO_PI;
#if defined(WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI)
  static const __m128 vector_float_smallestNonDenorm;
#else
  static __m128 vector_float_smallestNonDenorm;
#endif
};
