#pragma once

struct idParametricDecay {
  float delta;
  float linear;
  float t0;
  float tdelta;
  float lambda;
  idParametricDecay();
  void Init(float delta, float linear, float t0, float tdelta, float lambda);
  float Evaluate(float t) const;
};

static_assert(sizeof(idParametricDecay) == 20, "idParametricDecay ABI drift");
