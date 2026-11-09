#pragma once

#include "tungsten/game/game_runtime.h"

class idWaterEntity : public idEntity {
public:
  ~idWaterEntity() override = default;
};

class idDynamicWaterEntity : public idWaterEntity {
public:
  void AddRipple(const idVec3& position, float magnitude, float radius) {
    lastRipplePosition = position;
    lastRippleMagnitude = magnitude;
    lastRippleRadius = radius;
    ++rippleCount;
  }

  int GetRippleCountForTest() const { return rippleCount; }
  const idVec3& GetLastRipplePositionForTest() const {
    return lastRipplePosition;
  }
  float GetLastRippleMagnitudeForTest() const {
    return lastRippleMagnitude;
  }
  float GetLastRippleRadiusForTest() const { return lastRippleRadius; }

private:
  idVec3 lastRipplePosition{};
  float lastRippleMagnitude{0.0F};
  float lastRippleRadius{0.0F};
  int rippleCount{0};
};
