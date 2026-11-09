#pragma once

#include "containers/list.h"

struct idRunningAverage {
  int maxNum;
  int current;
  idList<float> vals;

  idRunningAverage() : maxNum(0), current(0), vals() {}
  float GetAverage() const;
  float GetMin() const;
  float GetMax() const;
  void Init(int count);
  void Add(float value);
};

static_assert(sizeof(idRunningAverage) == 32, "idRunningAverage ABI drift");
