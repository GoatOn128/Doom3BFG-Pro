#pragma once

#include <cstddef>

template <typename T> class idCatmullRomSpline;

template <> class idCatmullRomSpline<float> {
public:
  enum boundary_t {
    FREE = 0,
    CLAMPED = 1,
    CLOSED = 2,
  };

  float times[64]{};
  float values[64]{};
  int numKnots = 0;
  mutable int currentIndex = -1;
  bool changed = true;
  boundary_t boundaryType = FREE;
  float closeTime = 0.0f;

  void Sort();
  void Normalize(float totalTime);
  float ClampedTime(float t) const;
  int IndexForTime(float time, bool fastSearch) const;
  float TimeForIndex(int index) const;
  float ValueForIndex(int index) const;
  void Basis(int index, float t, float* basisValues) const;
  float GetCurrentValue(float time, bool fastSearch = false) const;
};

class idLookupTable {
public:
  idLookupTable() { Clear(); }

  void SetSnap(bool enabled);
  void SetSpline(bool enabled);
  void Finalize();
  void SetClamp(bool enabled);
  void AddValue(float time, float value);
  void Clear();
  float TableLookupNormalized(float time, bool fastSearch = false) const;
  float TableLookup(float time, bool fastSearch = false) const;

  void SetRange(float minimum, float maximum) {
    _min = minimum;
    _max = maximum;
  }

  int GetNumValues() const { return values.numKnots; }
  bool IsSnap() const { return snap; }
  bool IsSpline() const { return spline; }
  float GetMinimum() const { return _min; }
  float GetMaximum() const { return _max; }
  void NormalizeValues(float minimum, float maximum);

private:
  bool clamp = false;
  bool snap = false;
  bool spline = false;
  float _min = 0.0f;
  float _max = 1.0f;
  idCatmullRomSpline<float> values;
};

static_assert(sizeof(idCatmullRomSpline<float>) == 532, "idCatmullRomSpline<float> ABI drift");
static_assert(sizeof(idLookupTable) == 544, "idLookupTable ABI drift");
