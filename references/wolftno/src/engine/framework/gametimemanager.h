#pragma once

#include "engine/framework/timemanager.h"

#include <cstddef>

class idCVar;

enum gameTimeType_t { GAMETIME_NORMAL = 0, GAMETIME_SCALED = 1 };

#ifndef SLOWMO_PARMS_T_DEFINED
#define SLOWMO_PARMS_T_DEFINED
struct slowmoParms_t {
  float easeInDuration{};
  float slowmoDuration{};
  float easeOutDuration{};
  float timeScale{};
};
#endif

template <typename T> class idInterpolate {
public:
  idInterpolate()
      : startTime(0.0f), duration(0.0f), startValue(), endValue(),
        currentTime(0.0f), currentValue() {}

  void Init(float start, float length, const T& initial, const T& final) {
    startTime = start;
    duration = length;
    startValue = initial;
    endValue = final;
    currentTime = start - 1.0f;
    currentValue = initial;
  }
  void SetStartTime(float value) { startTime = value; }
  void SetDuration(float value) { duration = value; }
  void SetStartValue(const T& value) { startValue = value; }
  void SetEndValue(const T& value) { endValue = value; }
  T GetCurrentValue(float time);
  T GetCurrentValueEaseOut(float time) {
    float blend = GetCurrentBlend(time);
    blend = 1.0f - (1.0f - blend) * (1.0f - blend);
    return startValue + (endValue - startValue) * blend;
  }
  bool IsDone(float time) { return time >= startTime + duration; }
  float GetCurrentBlend(float time) {
    if (duration == 0.0f)
      return time < startTime ? 0.0f : 1.0f;
    const float blend = (time - startTime) / duration;
    return blend < 0.0f ? 0.0f : (blend > 1.0f ? 1.0f : blend);
  }
  float GetStartTime() { return startTime; }
  float GetEndTime() { return startTime + duration; }
  float GetDuration() { return duration; }
  const T& GetStartValue() { return startValue; }
  const T& GetEndValue() { return endValue; }

  float startTime;
  float duration;
  T startValue;
  T endValue;
  float currentTime;
  T currentValue;
};

class idGameTimeManager {
public:
  idGameTimeManager();
  ~idGameTimeManager() = default;

  void Clear();
  void UpdateGameFrame();
  void UpdateGameMs();
  void UpdateTimeScale();
  void SetGameFrame(const int frame);
  void SetGameMs(const int newTime);
  void SetTimeScale(const float newScale, const gameTimeType_t type);
  static void SetGameHz(const int hertz);
  bool IsInSlowMotion() { return inSlowMotion; }
  int GetGameHz(gameTimeType_t type) const;
  int GetGameFrame() { return framenum; }
  int GetGameMs(gameTimeType_t type) const;
  float GetGameSec(gameTimeType_t type) { return GetGameMs(type) * 0.001f; }
  int GetPreviousGameMs(gameTimeType_t type) const;
  float GetPreviousGameSec(gameTimeType_t type) {
    return GetPreviousGameMs(type) * 0.001f;
  }
  int GetGameMsPerFrame(gameTimeType_t type) const;
  float GetGameSecPerFrame(gameTimeType_t type) {
    return GetGameMsPerFrame(type) * 0.001f;
  }
  int GetRealMs() { return time[GAMETIME_NORMAL]; }
  float GetRealSec() { return GetRealMs() * 0.001f; }
  float GetRealMsPerFrame(gameTimeType_t type) const;
  float GetRealSecPerFrame(gameTimeType_t type) {
    return GetRealMsPerFrame(type) * 0.001f;
  }
  float GetTimeScale(gameTimeType_t type) const;
  int GetGameMsPerRealSec() {
    return timeManager.gameTimePerSecond.value;
  }
  const idTypesafeNumber<int, millisecondUnique_t>
  ConvertTimeGameToReal(idTypesafeNumber<int, gameTimeUnique_t> ticks) const;
  const idTypesafeNumber<int, gameTimeUnique_t>
  ConvertTimeRealToGame(idTypesafeNumber<int, millisecondUnique_t> ms) const;
  void StartSlowmotion(const slowmoParms_t& parms);
  void StopSlowmotion();
  void SetCurrentAndPreviousGameMs(const int current, const int previous) {
    time[0] = time[1] = current;
    previousTime[0] = previousTime[1] = previous;
  }

  int framenum;
  int hz[2];
  int ticks[2];
  float timeDelta[2];
  int time[2];
  int previousTime[2];
  float scale[2];
  idInterpolate<float> timeScaleExtrapolation[2];
  slowmoParms_t slowmoParms;
  bool inSlowMotion;
  bool useInterpolator;
  float slowmoDuration;
};

extern idCVar g_enableGameTimeScaling;

static_assert(sizeof(idInterpolate<float>) == 0x18,
              "idInterpolate<float> ABI drift");
static_assert(sizeof(slowmoParms_t) == 0x10, "slowmoParms_t ABI drift");
static_assert(sizeof(idGameTimeManager) == 0x7C,
              "idGameTimeManager ABI drift");
static_assert(offsetof(idGameTimeManager, timeScaleExtrapolation) == 0x34,
              "idGameTimeManager interpolation offset drift");
static_assert(offsetof(idGameTimeManager, slowmoParms) == 0x64,
              "idGameTimeManager slowmo offset drift");
