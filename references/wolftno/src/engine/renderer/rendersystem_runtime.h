#pragma once

#include "engine/framework/usercmd.h"
#include "engine/renderer/renderview.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/geometry/screenrect.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <type_traits>

class idRenderModel;
struct idRenderModelCommitted;
class idRenderWorld;
class idRenderWorldCommitted;

struct alignas(16) idScreenView {
  idScreenView()
      : viewIndex(-1), usePreviousRendering(false), world(nullptr), g(),
        bypassUserCmd(), viewGuis(), guiOriginOffset(0.0f) {
    screenRect.Clear();
  }

  idScreenRect screenRect;
  int viewIndex;
  bool usePreviousRendering;
  std::uint8_t alignmentBeforeWorld[3];
  idRenderWorld* world;
  renderView_t g;
  usercmd_t bypassUserCmd;
  std::uint8_t alignmentBeforeGuis[4];
  idStaticList<idRenderModel*, 16> viewGuis;
  float guiOriginOffset;
  std::uint8_t alignmentTail[4];
};

struct idScreenViewCommitted {
  idScreenViewCommitted();
  ~idScreenViewCommitted();
  idScreenViewCommitted(const idScreenViewCommitted&) = default;
  idScreenViewCommitted& operator=(const idScreenViewCommitted& other);

  idScreenRect screenRect;
  bool usePreviousRendering{};
  std::uint8_t alignmentBeforeWorld[7]{};
  idRenderWorldCommitted* world{};
  idRenderView* renderView{};
  float guiOriginOffset{};
  std::uint8_t alignmentBeforeGuis[4]{};
  idStaticList<idRenderModelCommitted*, 16> committedGuis;
};

struct idRenderFrameInfoCommitted {
  idStaticList<idScreenViewCommitted, 3> committedViews;
  int renderTime;
  bool captureFrame;
  std::uint8_t alignmentTail[3];

  ~idRenderFrameInfoCommitted();
};

struct alignas(16) idRenderFrameInfo {
  idRenderFrameInfo();
  ~idRenderFrameInfo();

  idStaticList<idScreenView, 3> screenViews;
  idStaticList<idScreenView, 2> worldViews;
  bool captureFrame;
  std::uint8_t alignmentTail[15];

  void Commit(idRenderFrameInfoCommitted& committed) const;
  void Clear();
};

template <typename T> struct idRenderStat {
  explicit idRenderStat(T maximum = T{}) : maxValue(maximum) {}

  T GetAverage() const {
    T sum{};
    for (const T value : values) {
      sum += value;
    }
    return sum / static_cast<T>(16);
  }

  void Update(T value, bool freeze, bool setDelta) {
    if (!freeze) {
      values[valueIndex] = value;
      valueIndex = (valueIndex + 1) & 15;
    }
    if (setDelta) {
      deltaFromValue = value;
    }
  }

  const char* GetValueString(bool showDelta, bool showThousands);

  T maxValue{};
  T values[16]{};
  T deltaFromValue{};
  int valueIndex{};
  char valueString[32]{};
};

template <typename T>
T* idListArrayNew(int num, unsigned tag, bool zeroBuffer);
template <typename T> void idListArrayDelete(T* pointer, int num);
template <typename T>
T* idListArrayResize(T* oldPointer, int oldNum, int newNum, unsigned tag,
                     bool zeroBuffer);

template <>
bool idList<idScreenViewCommitted, 5>::Resize(int newSize);

static_assert(sizeof(usercmd_t) == 76, "user command ABI drift");
static_assert(offsetof(idScreenView, g) == 32,
              "screen-view renderer parms offset drift");
static_assert(offsetof(idScreenView, viewGuis) == 6640,
              "screen-view GUI list offset drift");
static_assert(sizeof(idScreenView) == 6800, "screen-view ABI drift");
static_assert(offsetof(idScreenViewCommitted, world) == 24,
              "committed screen-view world offset drift");
static_assert(offsetof(idScreenViewCommitted, committedGuis) == 48,
              "committed screen-view GUI offset drift");
static_assert(sizeof(idScreenViewCommitted) == 200,
              "committed screen-view ABI drift");
static_assert(sizeof(idRenderFrameInfoCommitted) == 632,
              "committed frame-info ABI drift");
static_assert(offsetof(idRenderFrameInfo, worldViews) == 20432,
              "frame-info world-view offset drift");
static_assert(offsetof(idRenderFrameInfo, captureFrame) == 34064,
              "frame-info capture flag offset drift");
static_assert(sizeof(idRenderFrameInfo) == 34080,
              "frame-info ABI drift");
static_assert(sizeof(idRenderStat<int>) == 108,
              "integer render-stat ABI drift");
static_assert(sizeof(idRenderStat<float>) == 108,
              "float render-stat ABI drift");
