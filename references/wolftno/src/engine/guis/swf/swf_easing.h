#pragma once

namespace idSWFEasing {
extern float PI;

struct Linear { static float ease(float t, float b, float c, float d); };
#define WOLF_EASING_FAMILY(name) \
  struct name { \
    static float easeIn(float t, float b, float c, float d); \
    static float easeOut(float t, float b, float c, float d); \
    static float easeInOut(float t, float b, float c, float d); \
  }
WOLF_EASING_FAMILY(Back);
WOLF_EASING_FAMILY(Bounce);
WOLF_EASING_FAMILY(Circ);
WOLF_EASING_FAMILY(Cubic);
WOLF_EASING_FAMILY(Elastic);
WOLF_EASING_FAMILY(Expo);
WOLF_EASING_FAMILY(Quad);
WOLF_EASING_FAMILY(Quart);
WOLF_EASING_FAMILY(Quint);
WOLF_EASING_FAMILY(Sine);
#undef WOLF_EASING_FAMILY
} // namespace idSWFEasing
