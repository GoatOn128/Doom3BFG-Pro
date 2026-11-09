#include "decay.h"

#include <cmath>

idParametricDecay::idParametricDecay() : delta(0), linear(0), t0(0), tdelta(0), lambda(0) {}

void idParametricDecay::Init(float newDelta, float newLinear, float newT0, float newTdelta,
                             float newLambda) {
  delta = newDelta;
  lambda = newLambda;
  linear = newLinear;
  t0 = newT0;
  tdelta = newTdelta;
}

float idParametricDecay::Evaluate(float t) const {
  if (t0 > t)
    return delta;
  const float elapsed = t - t0;
  if (elapsed > tdelta)
    return 0.0f;
  return (std::pow(0.5f, elapsed / (tdelta * lambda)) * (1.0f - linear) +
          (1.0f - elapsed / tdelta) * linear) *
         delta;
}
