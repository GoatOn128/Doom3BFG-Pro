#include "runningaverage.h"

#include <algorithm>

float idRunningAverage::GetAverage() const {
  if (vals.num == 0)
    return 0.0f;
  float total = 0.0f;
  for (int i = 0; i < vals.num; ++i)
    total += vals.list[i];
  return total / static_cast<float>(vals.num);
}

float idRunningAverage::GetMin() const {
  if (vals.num == 0)
    return 0.0f;
  float result = vals.list[0];
  for (int i = 1; i < vals.num; ++i)
    result = std::min(result, vals.list[i]);
  return result;
}

float idRunningAverage::GetMax() const {
  if (vals.num == 0)
    return 0.0f;
  float result = vals.list[0];
  for (int i = 1; i < vals.num; ++i)
    result = std::max(result, vals.list[i]);
  return result;
}

void idRunningAverage::Init(int count) {
  current = 0;
  if (maxNum != count && count > vals.size)
    vals.EnsureCapacity(count);
  vals.num = 0;
  maxNum = std::max(0, count);
}

void idRunningAverage::Add(float value) {
  if (maxNum <= 0)
    return;
  if (vals.num == maxNum) {
    if (++current >= maxNum)
      current = 0;
  } else {
    vals.Alloc();
    current = vals.num - 1;
  }
  vals.list[current] = value;
}
