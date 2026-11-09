#pragma once

#include <cstddef>

#ifndef ID_GAME_TIME_UNIQUE_T_DEFINED
#define ID_GAME_TIME_UNIQUE_T_DEFINED
enum gameTimeUnique_t { GAME_TIME_UNIQUE };
#endif

#ifndef ID_MILLISECOND_UNIQUE_T_DEFINED
#define ID_MILLISECOND_UNIQUE_T_DEFINED
enum millisecondUnique_t { MILLISECOND_UNIQUE };
#endif

#ifndef ID_TYPESAFE_NUMBER_DEFINED
#define ID_TYPESAFE_NUMBER_DEFINED
template <typename T, typename UniqueType>
class idTypesafeNumber {
public:
  constexpr idTypesafeNumber() : value{} {}
  constexpr idTypesafeNumber(T input) : value(input) {}
  constexpr idTypesafeNumber(const idTypesafeNumber& other)
      : value(other.value) {}
  constexpr operator T() const { return value; }
  constexpr T GetValue() const { return value; }

  T value;
};
#endif

template <typename T, typename UniqueType>
using idTimeValue = idTypesafeNumber<T, UniqueType>;

using idGameTime = idTypesafeNumber<int, gameTimeUnique_t>;

class idTimeManager {
public:
  constexpr explicit idTimeManager(int hertz)
      : gameHz(hertz), inverseGameHz(1.0f / static_cast<float>(hertz)),
        gameTimePerFrame(
            static_cast<int>(1000.0f / static_cast<float>(hertz))),
        gameTimePerSecond(
            hertz * static_cast<int>(1000.0f / static_cast<float>(hertz))),
        inverseGameTimePerSecond(
            1.0f /
            static_cast<float>(hertz * static_cast<int>(
                                             1000.0f /
                                             static_cast<float>(hertz)))) {}

  void SetGameHz(int hertz);

  int GetGameHz() const { return gameHz; }
  float GetInvGameHz() const { return inverseGameHz; }
  idGameTime GetGameTimePerFrame() const { return gameTimePerFrame; }
  idGameTime GetGameTimePerSec() const { return gameTimePerSecond; }
  float GetInverseGameTimePerSecond() const {
    return inverseGameTimePerSecond;
  }

  int gameHz;
  float inverseGameHz;
  idGameTime gameTimePerFrame;
  idGameTime gameTimePerSecond;
  float inverseGameTimePerSecond;
};

extern idTimeManager timeManager;

static_assert(sizeof(idGameTime) == 0x04, "idGameTime ABI drift");
static_assert(sizeof(idTimeManager) == 0x14, "idTimeManager ABI drift");
static_assert(offsetof(idTimeManager, gameTimePerFrame) == 0x08,
              "idTimeManager::gameTimePerFrame ABI drift");
static_assert(offsetof(idTimeManager, inverseGameTimePerSecond) == 0x10,
              "idTimeManager::inverseGameTimePerSecond ABI drift");
