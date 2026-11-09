#pragma once

#include "shared/idlib/math/vector.h"

class idPresentable;

class idEntityInfluenceTrail {
public:
  idVec3 offset{};
  float innerRadius{32.0F};
  float outerRadius{64.0F};
  float angle{0.0F};
  float trailDist{32.0F};
  float trailFadeInTime{0.25F};
  float trailFadeOutTime{2.0F};
};

class idInfluenceTrailManager {
public:
  void FreeInfluenceTrail(idPresentable* presentable) {
    lastFreedPresentable = presentable;
    ++freeCount;
  }
  int GetFreeCountForTest() const { return freeCount; }
  idPresentable* GetLastFreedPresentableForTest() const {
    return lastFreedPresentable;
  }
  void ResetForTest() {
    freeCount = 0;
    lastFreedPresentable = nullptr;
  }

private:
  int freeCount{0};
  idPresentable* lastFreedPresentable{nullptr};
};

extern idInfluenceTrailManager* influenceTrailManager;
