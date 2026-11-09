#pragma once

#include "shared/idlib/metrics/gaugemetric.h"

#include <cstddef>

class idStr;

class idResolutionScale {
public:
  idResolutionScale();
  void InitForMap(const char* mapName);
  void GetCurrentResolutionScale(float& x, float& y) const;
  void SetCurrentGPUFrameTime(int microseconds);
  void GetConsoleText(idStr& text);
  void LogMetrics();

  float dropMilliseconds;
  float raiseMilliseconds;
  int framesAboveRaise;
  float currentResolution;
  idGaugeMetric resolutionScaleMetricX;
  idGaugeMetric resolutionScaleMetricY;
};

#if defined(WOLF_RENDERER_RENDERTHREAD_TEST_ACCESS)
alignas(idResolutionScale) extern unsigned char
    renderThreadTestResolutionScaleStorage[sizeof(idResolutionScale)];
#define resolutionScale (*reinterpret_cast<idResolutionScale*>(             \
    renderThreadTestResolutionScaleStorage))
#else
extern idResolutionScale resolutionScale;
#endif

static_assert(offsetof(idResolutionScale, dropMilliseconds) == 0,
              "resolution scale drop threshold offset drift");
static_assert(offsetof(idResolutionScale, framesAboveRaise) == 8,
              "resolution scale frame counter offset drift");
static_assert(offsetof(idResolutionScale, currentResolution) == 12,
              "resolution scale current value offset drift");
static_assert(offsetof(idResolutionScale, resolutionScaleMetricX) == 16,
              "resolution scale first metric offset drift");
static_assert(offsetof(idResolutionScale, resolutionScaleMetricY) == 144,
              "resolution scale second metric offset drift");
static_assert(sizeof(idResolutionScale) == 272,
              "resolution scale ABI drift");
