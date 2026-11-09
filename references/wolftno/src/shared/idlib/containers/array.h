#pragma once

// Retail idArray is a fixed-capacity aggregate with no bookkeeping fields.
template <typename T, int Count> class idArray {
public:
  T ptr[Count];

  T& operator[](int index) { return ptr[index]; }
  const T& operator[](int index) const { return ptr[index]; }
};
