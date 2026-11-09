#pragma once

#include <cstdint>

class idRandom {
public:
  constexpr explicit idRandom(int initialSeed = 0) : seed(initialSeed) {}

  void SetSeed(int value) { seed = value; }
  int GetSeed() const { return seed; }
  int RandomInt() {
    seed = 1103515245 * seed + 12345;
    return (seed >> 16) & 0x7FFF;
  }
  int RandomInt(int maximum) {
    return maximum != 0 ? RandomInt() % maximum : 0;
  }
  float RandomFloat() {
    return static_cast<float>(RandomInt()) * 0.000030518509f;
  }
  float CRandomFloat() { return RandomFloat() * 2.0f - 1.0f; }

private:
  int seed;
};

class idRandom2 {
public:
  int RandomInt(int maximum);
  std::uint32_t seed;
};

static_assert(sizeof(idRandom2) == 4, "idRandom2 ABI drift");
static_assert(sizeof(idRandom) == 4, "idRandom ABI drift");
