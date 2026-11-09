#pragma once

#include <xmmintrin.h>

struct alignas(16) mgEnvelope {
  void Initialize(float sampleRate, float time);

  __m128 decay_v128{};
  float decay{0.0F};
};

static_assert(alignof(mgEnvelope) == 16);
static_assert(sizeof(mgEnvelope) == 32);
