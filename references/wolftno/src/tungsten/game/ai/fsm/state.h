#pragma once

#include "tungsten/game/ai/fsm/aistate.h"

#include <cstring>

template <typename T, int MaxClasses, int HashSize>
class idClassFactory {
public:
  const char* Find(const char* name) const {
    return name != nullptr && *name != '\0' ? name : nullptr;
  }
  T* Create(const char* name) const {
    return Find(name) != nullptr ? new T(name) : nullptr;
  }
};
