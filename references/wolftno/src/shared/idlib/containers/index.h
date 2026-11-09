#pragma once

template <typename T, typename InvalidTag> class idIndex {
public:
#if defined(WOLFTNO_RETAIL_TRIVIAL_IDINDEX_ABI)
  T value;
#else
  T value = static_cast<T>(-1);
#endif

  constexpr idIndex() = default;
  constexpr explicit idIndex(T initial) : value(initial) {}
};
