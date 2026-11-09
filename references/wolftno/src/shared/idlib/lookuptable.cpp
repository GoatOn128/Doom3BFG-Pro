#include "lookuptable.h"

#include <algorithm>
#include <cmath>
#include <utility>

void idLookupTable::SetSnap(bool enabled) {
  snap = enabled;
  if (enabled) {
    spline = false;
  }
}

void idLookupTable::SetSpline(bool enabled) {
  spline = enabled;
  if (enabled) {
    snap = false;
  }
}

void idCatmullRomSpline<float>::Sort() {
  for (int end = numKnots - 1; end > 0; --end) {
    int maximum = 0;
    for (int index = 1; index <= end; ++index) {
      if (times[index] > times[maximum]) {
        maximum = index;
      }
    }
    std::swap(times[maximum], times[end]);
    std::swap(values[maximum], values[end]);
  }
  changed = true;
}

void idCatmullRomSpline<float>::Normalize(float totalTime) {
  if (numKnots < 1 || times[numKnots - 1] == 0.0f) {
    return;
  }
  const float scale = totalTime / times[numKnots - 1];
  for (int index = 0; index < numKnots; ++index) {
    times[index] *= scale;
  }
  changed = true;
}

float idCatmullRomSpline<float>::ClampedTime(float t) const {
  if (numKnots <= 0) {
    return t;
  }
  if (boundaryType == CLAMPED) {
    return std::clamp(t, times[0], times[numKnots - 1]);
  }
  if (boundaryType == CLOSED) {
    const float period = times[numKnots - 1] + closeTime;
    if (period > 0.0f) {
      t -= std::floor(t / period) * period;
    }
  }
  return t;
}

int idCatmullRomSpline<float>::IndexForTime(float time,
                                            bool fastSearch) const {
  if (numKnots <= 0) {
    currentIndex = 0;
    return 0;
  }

  if (fastSearch && currentIndex >= 0 && currentIndex <= numKnots) {
    if (currentIndex == 0 && time <= times[0]) {
      return currentIndex;
    }
    if (currentIndex == numKnots && time > times[numKnots - 1]) {
      return currentIndex;
    }
    if (currentIndex > 0 && currentIndex < numKnots && time > times[currentIndex - 1] &&
        time <= times[currentIndex]) {
      return currentIndex;
    }
    if (currentIndex < numKnots && time > times[currentIndex] &&
        (currentIndex + 1 == numKnots || time <= times[currentIndex + 1])) {
      return ++currentIndex;
    }
  }

  const auto* first = times;
  const auto* last = times + numKnots;
  const int result = static_cast<int>(std::lower_bound(first, last, time) - first);
  if (fastSearch) {
    currentIndex = result;
  }
  return result;
}

float idCatmullRomSpline<float>::TimeForIndex(int index) const {
  const int last = numKnots - 1;
  if (numKnots <= 0) {
    return 0.0f;
  }
  if (index >= 0 && index <= last) {
    return times[index];
  }
  if (numKnots == 1) {
    return times[0];
  }
  if (boundaryType == CLOSED) {
    const float period = times[last] + closeTime;
    if (index > last) {
      return period * static_cast<float>(index / numKnots) + times[index % numKnots];
    }
    const int wrapped = (numKnots + index % numKnots) % numKnots;
    const int cycles = index / numKnots;
    return static_cast<float>(cycles) * period - (period - times[wrapped]);
  }
  if (index < 0) {
    return times[0] + static_cast<float>(index) * (times[1] - times[0]);
  }
  return times[last] + static_cast<float>(index - last) * (times[last] - times[last - 1]);
}

float idCatmullRomSpline<float>::ValueForIndex(int index) const {
  const int last = numKnots - 1;
  if (numKnots <= 0) {
    return 0.0f;
  }
  if (index >= 0 && index <= last) {
    return values[index];
  }
  if (numKnots == 1) {
    return values[0];
  }
  if (boundaryType == CLOSED) {
    return values[(numKnots + index % numKnots) % numKnots];
  }
  if (boundaryType == CLAMPED) {
    return index < 0 ? values[0] : values[last];
  }
  if (index < 0) {
    return values[0] + static_cast<float>(index) * (values[1] - values[0]);
  }
  return values[last] + static_cast<float>(index - last) * (values[last] - values[last - 1]);
}

void idCatmullRomSpline<float>::Basis(int index, float t, float* basisValues) const {
  const float start = TimeForIndex(index);
  const float end = TimeForIndex(index + 1);
  const float fraction = end == start ? 0.0f : (t - start) / (end - start);
  const float fraction2 = fraction * fraction;
  const float fraction3 = fraction2 * fraction;
  basisValues[0] = 0.5f * (-fraction3 + 2.0f * fraction2 - fraction);
  basisValues[1] = 0.5f * (3.0f * fraction3 - 5.0f * fraction2 + 2.0f);
  basisValues[2] = 0.5f * (-3.0f * fraction3 + 4.0f * fraction2 + fraction);
  basisValues[3] = 0.5f * (fraction3 - fraction2);
}

void idLookupTable::Finalize() {
  values.Sort();
  if (values.numKnots > 1 && values.times[values.numKnots - 1] > 1.0f) {
    values.Normalize(clamp ? static_cast<float>(values.numKnots - 1) /
                                 static_cast<float>(values.numKnots)
                           : 1.0f);
  }
  values.closeTime = values.numKnots > 0 ? 1.0f - values.times[values.numKnots - 1] : 0.0f;
  values.changed = true;
}

void idLookupTable::SetClamp(bool enabled) {
  clamp = enabled;
  values.changed = true;
  values.boundaryType =
      enabled ? idCatmullRomSpline<float>::CLAMPED : idCatmullRomSpline<float>::CLOSED;
}

void idLookupTable::NormalizeValues(float minimum, float maximum) {
  const float range = maximum - minimum;
  if (range == 0.0f)
    return;
  for (int i = 0; i < values.numKnots; ++i)
    values.values[i] = (values.values[i] - minimum) / range;
  values.changed = true;
  _min = _min * range + minimum;
  _max = _max * range + minimum;
}

void idLookupTable::AddValue(float time, float value) {
  if (values.numKnots >= 64) {
    return;
  }
  values.times[values.numKnots] = time;
  values.values[values.numKnots] = value;
  ++values.numKnots;
  values.changed = true;
}

void idLookupTable::Clear() {
  clamp = false;
  snap = false;
  spline = false;
  _min = 0.0f;
  _max = 1.0f;
  values.numKnots = 0;
  values.currentIndex = -1;
  values.changed = true;
  values.boundaryType = idCatmullRomSpline<float>::FREE;
  values.closeTime = 0.0f;
}

float idCatmullRomSpline<float>::GetCurrentValue(float time,
                                                  bool fastSearch) const {
  if (numKnots <= 0) {
    return 0.0f;
  }
  if (numKnots == 1) {
    return values[0];
  }
  const float clamped = ClampedTime(time);
  const int index = IndexForTime(clamped, fastSearch);
  float basis[4];
  Basis(index - 1, clamped, basis);
  float result = 0.0f;
  for (int offset = 0; offset < 4; ++offset) {
    result += ValueForIndex(index - 2 + offset) * basis[offset];
  }
  return result;
}

float idLookupTable::TableLookupNormalized(float time,
                                           bool fastSearch) const {
  if (values.numKnots <= 0) {
    return 0.0f;
  }
  if (values.numKnots == 1) {
    return values.values[0];
  }
  if (spline) {
    return values.GetCurrentValue(time, fastSearch);
  }
  if (clamp) {
    if (time <= values.times[0]) {
      return values.values[0];
    }
    if (time >= values.times[values.numKnots - 1]) {
      return values.values[values.numKnots - 1];
    }
  }

  const float clamped = values.ClampedTime(time);
  const int index = values.IndexForTime(clamped, fastSearch);
  if (snap) {
    return values.ValueForIndex(index - 1);
  }
  const float end = values.TimeForIndex(index);
  const float start = values.TimeForIndex(index - 1);
  if (end == start) {
    return 0.0f;
  }
  const float startValue = values.ValueForIndex(index - 1);
  const float endValue = values.ValueForIndex(index);
  return startValue + (endValue - startValue) * ((clamped - start) / (end - start));
}

float idLookupTable::TableLookup(float time, bool fastSearch) const {
  return _min + (_max - _min) * TableLookupNormalized(time, fastSearch);
}
