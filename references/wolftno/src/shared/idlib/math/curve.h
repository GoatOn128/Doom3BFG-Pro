#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>

// The template layout and spline equations mirror the idTech curve hierarchy.
// The idVec3 constructor/destructor and factory specializations emitted by the
// cable compiland are defined in rendermodelcable.cpp.
template <typename T> class idCurve {
public:
  idCurve();
  virtual ~idCurve();

  virtual int AddValue(float time, const T& value) {
    const int index = IndexForTime(time);
    times.Insert(time, index);
    values.Insert(value, index);
    changed = true;
    return index;
  }
  virtual void RemoveIndex(int index) {
    values.RemoveIndex(index);
    times.RemoveIndex(index);
    changed = true;
  }
  virtual void Clear() {
    values.Clear();
    times.Clear();
    currentIndex = -1;
    changed = true;
  }
  virtual T GetCurrentValue(float time) const {
    const int index = IndexForTime(time);
    return index >= values.Num() ? values[values.Num() - 1] : values[index];
  }
  virtual T GetCurrentFirstDerivative(float) const {
    return values[0] - values[0];
  }
  virtual T GetCurrentSecondDerivative(float) const {
    return values[0] - values[0];
  }
  virtual bool IsDone(float time) const {
    return time >= times[times.Num() - 1];
  }
  virtual idCurve<T>* CreateNewCurve() = 0;

  int GetNumValues() const { return values.Num(); }
  float GetTime(int index) const { return times[index]; }
  T GetValue(int index) const { return values[index]; }
  void SetValue(int index, const T& value) {
    values[index] = value;
    changed = true;
  }

  float GetLengthForTime(float time) const {
    if (times.Num() < 2)
      return 0.0f;
    const int index = std::min(IndexForTime(time), times.Num() - 1);
    float length = 0.0f;
    for (int knot = 0; knot < index; ++knot)
      length += GetLengthBetweenKnots(knot, knot + 1);
    length += IntegrateSpeed(times[index], time);
    return length;
  }

  float GetTimeForLength(float length, float epsilon = 0.1f) const {
    if (times.Num() < 2 || length <= 0.0f)
      return times.Num() != 0 ? times[0] : 0.0f;

    float accumulated = 0.0f;
    int knot = 0;
    float segmentLength = 0.0f;
    for (; knot < times.Num() - 1; ++knot) {
      segmentLength = GetLengthBetweenKnots(knot, knot + 1);
      if (accumulated + segmentLength >= length)
        break;
      accumulated += segmentLength;
    }
    if (knot >= times.Num() - 1)
      return times[times.Num() - 1];

    const float begin = times[knot];
    const float end = times[knot + 1];
    float time = segmentLength > 0.0f
                     ? begin + (end - begin) *
                                   ((length - accumulated) / segmentLength)
                     : begin;
    for (int iteration = 0; iteration < 32; ++iteration) {
      const float difference = GetLengthForTime(time) - length;
      if (std::fabs(difference) <= epsilon)
        break;
      const float speed = GetCurrentFirstDerivative(time).Length();
      if (speed <= std::numeric_limits<float>::min())
        break;
      time = std::max(begin, std::min(end, time - difference / speed));
    }
    return time;
  }

  static void operator delete(void* memory) noexcept {
    idMem::Singleton()->Free(memory, ALIGN_16);
  }

protected:
  float GetLengthBetweenKnots(int first, int second) const {
    return IntegrateSpeed(times[first], times[second]);
  }

  float IntegrateSpeed(float begin, float end) const {
    constexpr int order = 5;
    float previous[order]{};
    float current[order]{};
    float delta = end - begin;
    previous[0] = 0.5f * delta *
                  (GetCurrentFirstDerivative(begin).Length() +
                   GetCurrentFirstDerivative(end).Length());
    for (int level = 1; level < order; ++level) {
      delta *= 0.5f;
      float midpointSum = 0.0f;
      const int samples = 1 << (level - 1);
      for (int sample = 0; sample < samples; ++sample) {
        midpointSum += GetCurrentFirstDerivative(
                           begin + (2 * sample + 1) * delta)
                           .Length();
      }
      current[0] = 0.5f * previous[0] + delta * midpointSum;
      float factor = 4.0f;
      for (int column = 1; column <= level; ++column) {
        current[column] =
            (factor * current[column - 1] - previous[column - 1]) /
            (factor - 1.0f);
        factor *= 4.0f;
      }
      for (int column = 0; column <= level; ++column)
        previous[column] = current[column];
    }
    return previous[order - 1];
  }

  int IndexForTime(float time) const {
    if (currentIndex >= 0 && currentIndex <= times.Num()) {
      if (currentIndex == 0) {
        if (time <= times[currentIndex])
          return currentIndex;
      } else if (currentIndex == times.Num()) {
        if (time > times[currentIndex - 1])
          return currentIndex;
      } else if (time > times[currentIndex - 1] &&
                 time <= times[currentIndex]) {
        return currentIndex;
      } else if (time > times[currentIndex] &&
                 (currentIndex + 1 == times.Num() ||
                  time <= times[currentIndex + 1])) {
        return ++currentIndex;
      }
    }

    int length = times.Num();
    int middle = length;
    int offset = 0;
    int result = 0;
    while (middle > 0) {
      middle = length >> 1;
      if (time == times[offset + middle])
        return offset + middle;
      if (time > times[offset + middle]) {
        offset += middle;
        length -= middle;
        result = 1;
      } else {
        length -= middle;
        result = 0;
      }
    }
    currentIndex = offset + result;
    return currentIndex;
  }

  float TimeForIndex(int index) const {
    const int last = times.Num() - 1;
    if (index < 0)
      return times[0] + index * (times[1] - times[0]);
    if (index > last)
      return times[last] + (index - last) * (times[last] - times[last - 1]);
    return times[index];
  }

  T ValueForIndex(int index) const {
    const int last = values.Num() - 1;
    if (index < 0)
      return values[0] + (values[1] - values[0]) * static_cast<float>(index);
    if (index > last)
      return values[last] +
             (values[last] - values[last - 1]) * static_cast<float>(index - last);
    return values[index];
  }

  idList<float, 116> times;
  idList<T, 116> values;
  mutable int currentIndex;
  mutable bool changed;
  unsigned char recoveredCurvePadding[3]{};
};

template <typename T> class idCurve_Spline : public idCurve<T> {
public:
  enum boundary_t { FREE = 0, CLAMPED = 1, CLOSED = 2 };

  idCurve_Spline() : boundaryType(FREE), closeTime(0.0f) {}

  bool IsDone(float time) const override {
    return boundaryType != CLOSED &&
           time >= this->times[this->times.Num() - 1];
  }
  void SetBoundaryType(boundary_t type) {
    boundaryType = type;
    this->changed = true;
  }
  boundary_t GetBoundaryType() const { return boundaryType; }
  void SetCloseTime(float time) {
    closeTime = time;
    this->changed = true;
  }
  float GetCloseTime() const { return boundaryType == CLOSED ? closeTime : 0.0f; }

protected:
  T ValueForIndex(int index) const {
    const int last = this->values.Num() - 1;
    if (index < 0) {
      if (boundaryType == CLOSED)
        return this->values[this->values.Num() + index % this->values.Num()];
      return this->values[0] +
             (this->values[1] - this->values[0]) * static_cast<float>(index);
    }
    if (index > last) {
      if (boundaryType == CLOSED)
        return this->values[index % this->values.Num()];
      return this->values[last] +
             (this->values[last] - this->values[last - 1]) *
                 static_cast<float>(index - last);
    }
    return this->values[index];
  }

  float TimeForIndex(int index) const {
    const int count = this->times.Num();
    const int last = count - 1;
    if (index < 0) {
      if (boundaryType == CLOSED) {
        return (index / count) * (this->times[last] + closeTime) -
               (this->times[last] + closeTime -
                this->times[count + index % count]);
      }
      return this->times[0] + index * (this->times[1] - this->times[0]);
    }
    if (index > last) {
      if (boundaryType == CLOSED)
        return (index / count) * (this->times[last] + closeTime) +
               this->times[index % count];
      return this->times[last] +
             (index - last) * (this->times[last] - this->times[last - 1]);
    }
    return this->times[index];
  }

  float ClampedTime(float time) const {
    if (boundaryType == CLAMPED) {
      if (time < this->times[0])
        return this->times[0];
      if (time >= this->times[this->times.Num() - 1])
        return this->times[this->times.Num() - 1];
    }
    return time;
  }

  boundary_t boundaryType;
  float closeTime;
};

template <typename T> class idCurve_BSpline : public idCurve_Spline<T> {
public:
  idCurve_BSpline() : order(4) {}
  int GetOrder() const { return order; }
  void SetOrder(int value) { order = value; }

protected:
  int order;
};

template <typename T>
class idCurve_NonUniformBSpline : public idCurve_BSpline<T> {
public:
  idCurve_NonUniformBSpline() = default;
  ~idCurve_NonUniformBSpline() override;

  T GetCurrentValue(float time) const override {
    if (this->times.Num() == 1)
      return this->values[0];
    std::array<float, 10> basis{};
    const float clamped = this->ClampedTime(time);
    const int index = this->IndexForTime(clamped);
    Basis(index - 1, this->order, clamped, basis.data());
    T value = this->values[0] - this->values[0];
    for (int j = 0; j < this->order; ++j) {
      const int knot = index + j - (this->order >> 1);
      value += this->ValueForIndex(knot) * basis[j];
    }
    return value;
  }

  T GetCurrentFirstDerivative(float time) const override {
    if (this->times.Num() == 1)
      return this->values[0] - this->values[0];
    std::array<float, 10> basis{};
    const float clamped = this->ClampedTime(time);
    const int index = this->IndexForTime(clamped);
    BasisFirstDerivative(index - 1, this->order, clamped, basis.data());
    T value = this->values[0] - this->values[0];
    for (int j = 0; j < this->order; ++j) {
      const int knot = index + j - (this->order >> 1);
      value += this->ValueForIndex(knot) * basis[j];
    }
    return value;
  }

  T GetCurrentSecondDerivative(float time) const override {
    if (this->times.Num() == 1)
      return this->values[0] - this->values[0];
    std::array<float, 10> basis{};
    const float clamped = this->ClampedTime(time);
    const int index = this->IndexForTime(clamped);
    BasisSecondDerivative(index - 1, this->order, clamped, basis.data());
    T value = this->values[0] - this->values[0];
    for (int j = 0; j < this->order; ++j) {
      const int knot = index + j - (this->order >> 1);
      value += this->ValueForIndex(knot) * basis[j];
    }
    return value;
  }

  idCurve<T>* CreateNewCurve() override;

protected:
  void Basis(int index, int curveOrder, float time, float* valuesOut) const {
    valuesOut[curveOrder - 1] = 1.0f;
    for (int recursion = 2; recursion <= curveOrder; ++recursion) {
      int knot = index - recursion + 1;
      valuesOut[curveOrder - recursion] = 0.0f;
      for (int sample = curveOrder - recursion + 1; sample < curveOrder;
           ++sample) {
        ++knot;
        const float omega =
            (time - this->TimeForIndex(knot)) /
            (this->TimeForIndex(knot + recursion - 1) -
             this->TimeForIndex(knot));
        valuesOut[sample - 1] += (1.0f - omega) * valuesOut[sample];
        valuesOut[sample] *= omega;
      }
    }
  }

  void BasisFirstDerivative(int index, int curveOrder, float time,
                            float* valuesOut) const {
    Basis(index, curveOrder - 1, time, valuesOut + 1);
    valuesOut[0] = 0.0f;
    int sample = 0;
    for (; sample < curveOrder - 1; ++sample) {
      valuesOut[sample] -= valuesOut[sample + 1];
      valuesOut[sample] *=
          static_cast<float>(curveOrder - 1) /
          (this->TimeForIndex(index + sample + (curveOrder - 1) - 2) -
           this->TimeForIndex(index + sample - 2));
    }
    valuesOut[sample] *=
        static_cast<float>(curveOrder - 1) /
        (this->TimeForIndex(index + sample + (curveOrder - 1) - 2) -
         this->TimeForIndex(index + sample - 2));
  }

  void BasisSecondDerivative(int index, int curveOrder, float time,
                             float* valuesOut) const {
    BasisFirstDerivative(index, curveOrder - 1, time, valuesOut + 1);
    valuesOut[0] = 0.0f;
    int sample = 0;
    for (; sample < curveOrder - 1; ++sample) {
      valuesOut[sample] -= valuesOut[sample + 1];
      valuesOut[sample] *=
          static_cast<float>(curveOrder - 1) /
          (this->TimeForIndex(index + sample + (curveOrder - 1) - 2) -
           this->TimeForIndex(index + sample - 2));
    }
    valuesOut[sample] *=
        static_cast<float>(curveOrder - 1) /
        (this->TimeForIndex(index + sample + (curveOrder - 1) - 2) -
         this->TimeForIndex(index + sample - 2));
  }
};

template <typename T>
class idCurve_NURBS : public idCurve_NonUniformBSpline<T> {
public:
  idCurve_NURBS();
  ~idCurve_NURBS() override;

  int AddValue(float time, const T& value) override {
    return AddValue(time, value, 1.0f);
  }
  int AddValue(float time, const T& value, float weight) {
    const int index = this->IndexForTime(time);
    this->times.Insert(time, index);
    this->values.Insert(value, index);
    weights.Insert(weight, index);
    this->changed = true;
    return index;
  }
  void RemoveIndex(int index) override {
    this->values.RemoveIndex(index);
    this->times.RemoveIndex(index);
    weights.RemoveIndex(index);
    this->changed = true;
  }
  void Clear() override {
    this->values.Clear();
    this->times.Clear();
    weights.Clear();
    this->currentIndex = -1;
    this->changed = true;
  }

  T GetCurrentValue(float time) const override {
    if (this->times.Num() == 1)
      return this->values[0];
    std::array<float, 10> basis{};
    const float clamped = this->ClampedTime(time);
    const int index = this->IndexForTime(clamped);
    this->Basis(index - 1, this->order, clamped, basis.data());
    T value = this->values[0] - this->values[0];
    float weightSum = 0.0f;
    for (int j = 0; j < this->order; ++j) {
      const int knot = index + j - (this->order >> 1);
      const float weightedBasis = basis[j] * WeightForIndex(knot);
      weightSum += weightedBasis;
      value += this->ValueForIndex(knot) * weightedBasis;
    }
    return value / weightSum;
  }

  T GetCurrentFirstDerivative(float time) const override {
    if (this->times.Num() == 1)
      return this->values[0];
    std::array<float, 10> basis{};
    std::array<float, 10> derivative{};
    const float clamped = this->ClampedTime(time);
    const int index = this->IndexForTime(clamped);
    this->Basis(index - 1, this->order, clamped, basis.data());
    this->BasisFirstDerivative(index - 1, this->order, clamped,
                               derivative.data());
    T weightedValue = this->values[0] - this->values[0];
    T weightedDerivative = weightedValue;
    float basisWeight = 0.0f;
    float derivativeWeight = 0.0f;
    for (int j = 0; j < this->order; ++j) {
      const int knot = index + j - (this->order >> 1);
      const float weight = WeightForIndex(knot);
      const float weightedBasis = basis[j] * weight;
      const float weightedBasisDerivative = derivative[j] * weight;
      basisWeight += weightedBasis;
      derivativeWeight += weightedBasisDerivative;
      const T knotValue = this->ValueForIndex(knot);
      weightedValue += knotValue * weightedBasis;
      weightedDerivative += knotValue * weightedBasisDerivative;
    }
    return (weightedDerivative * basisWeight -
            weightedValue * derivativeWeight) /
           (basisWeight * basisWeight);
  }

  T GetCurrentSecondDerivative(float time) const override {
    if (this->times.Num() == 1)
      return this->values[0];
    std::array<float, 10> basis{};
    std::array<float, 10> firstDerivative{};
    std::array<float, 10> secondDerivative{};
    const float clamped = this->ClampedTime(time);
    const int index = this->IndexForTime(clamped);
    this->Basis(index - 1, this->order, clamped, basis.data());
    this->BasisFirstDerivative(index - 1, this->order, clamped,
                               firstDerivative.data());
    this->BasisSecondDerivative(index - 1, this->order, clamped,
                                secondDerivative.data());
    T weightedValue = this->values[0] - this->values[0];
    T weightedFirstDerivative = weightedValue;
    T weightedSecondDerivative = weightedValue;
    float basisWeight = 0.0f;
    float firstDerivativeWeight = 0.0f;
    float secondDerivativeWeight = 0.0f;
    for (int j = 0; j < this->order; ++j) {
      const int knot = index + j - (this->order >> 1);
      const float weight = WeightForIndex(knot);
      const float weightedBasis = basis[j] * weight;
      const float weightedFirst = firstDerivative[j] * weight;
      const float weightedSecond = secondDerivative[j] * weight;
      basisWeight += weightedBasis;
      firstDerivativeWeight += weightedFirst;
      secondDerivativeWeight += weightedSecond;
      const T knotValue = this->ValueForIndex(knot);
      weightedValue += knotValue * weightedBasis;
      weightedFirstDerivative += knotValue * weightedFirst;
      weightedSecondDerivative += knotValue * weightedSecond;
    }
    const float basisWeightSquared = basisWeight * basisWeight;
    return (basisWeightSquared *
                (weightedSecondDerivative * basisWeight -
                 weightedValue * secondDerivativeWeight) -
            (weightedFirstDerivative * basisWeight -
             weightedValue * firstDerivativeWeight) *
                (2.0f * basisWeight * firstDerivativeWeight)) /
           (basisWeightSquared * basisWeightSquared);
  }

  idCurve_NURBS<T>* CreateNewCurve() override;

  int GetNumWeights() const { return weights.Num(); }
  float GetWeight(int index) const { return weights[index]; }

protected:
  float WeightForIndex(int index) const {
    const int last = weights.Num() - 1;
    if (index < 0) {
      if (this->boundaryType == idCurve_Spline<T>::CLOSED)
        return weights[weights.Num() + index % weights.Num()];
      return weights[0] + index * (weights[1] - weights[0]);
    }
    if (index > last) {
      if (this->boundaryType == idCurve_Spline<T>::CLOSED)
        return weights[index % weights.Num()];
      return weights[last] +
             (index - last) * (weights[last] - weights[last - 1]);
    }
    return weights[index];
  }

  idList<float, 116> weights;
};

template <> idCurve<idVec3>::idCurve();
template <> idCurve<idVec3>::~idCurve();
template <>
idCurve_NonUniformBSpline<idVec3>::~idCurve_NonUniformBSpline();
template <>
idCurve<idVec3>*
idCurve_NonUniformBSpline<idVec3>::CreateNewCurve();
template <> idCurve_NURBS<idVec3>::idCurve_NURBS();
template <> idCurve_NURBS<idVec3>::~idCurve_NURBS();
template <> idCurve_NURBS<idVec3>* idCurve_NURBS<idVec3>::CreateNewCurve();

static_assert(sizeof(idCurve<idVec3>) == 64, "idCurve<idVec3> ABI drift");
static_assert(sizeof(idCurve_Spline<idVec3>) == 72,
              "idCurve_Spline<idVec3> ABI drift");
static_assert(sizeof(idCurve_NonUniformBSpline<idVec3>) == 80,
              "idCurve_NonUniformBSpline<idVec3> ABI drift");
static_assert(sizeof(idCurve_NURBS<idVec3>) == 104,
              "idCurve_NURBS<idVec3> ABI drift");
