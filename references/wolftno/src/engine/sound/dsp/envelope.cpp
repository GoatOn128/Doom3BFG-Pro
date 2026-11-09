#include "engine/sound/dsp/envelope.h"

#include <algorithm>
#include <cmath>

void mgEnvelope::Initialize(float sampleRate, float time) {
  decay = std::pow(0.059999999F,
                   1.0F / (sampleRate * std::max(time, 0.0000099999997F)));
  decay_v128 = _mm_set1_ps(decay);
}
