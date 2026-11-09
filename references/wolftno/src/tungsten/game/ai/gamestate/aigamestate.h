#pragma once

#include "tungsten/game/ai/gamestate/aientitystate.h"

#include <memory>
#include <vector>

template <typename T, int BlockSize, int Tag>
class idBlockAlloc {
public:
  idBlockAlloc() = default;
  ~idBlockAlloc();
  void AllocNewBlock();
  T* Alloc();
  void Shutdown();
  int GetActiveCount() const { return static_cast<int>(active.size()); }
  int GetCapacity() const { return capacity; }

private:
  std::vector<std::unique_ptr<T>> active;
  int capacity{0};
};

extern idCVar ai_debugGameState;
