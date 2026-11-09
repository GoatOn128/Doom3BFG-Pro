#pragma once

#include "shared/idlib/color.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

class idMaterial;
class idRenderModelGui;

class idDebugGraph {
public:
  enum fillMode_t { GRAPH_LINE = 0, GRAPH_FILL = 1, GRAPH_FILL_REVERSE = 2 };
  enum captionPos_t {
    CAP_NONE = 0,
    CAP_LEFT = 1,
    CAP_RIGHT = 2,
    CAP_TOP = 3,
    CAP_BOTTOM = 4
  };
  struct graphPlot_t {
    float value;
    idColor color;
  };

  explicit idDebugGraph(int numItems = 0);
  ~idDebugGraph() = default;
  void Enable(bool value) { enable = value; }
  void Init(int numBars);
  void AddGridLine(float value, const idColor& color);
  void SetValue(int bar, float value, const idColor& color);
  float GetValue(int bar) { return bars[bar].value; }
  void SetLabel(int bar, const char* text);
  void SetFillMode(fillMode_t value) { mode = value; }
  void SetSideways(bool value) { sideways = value; }
  void SetBackgroundColor(const idColor& value) { bgColor = value; }
  void SetLabelColor(const idColor& value) { fontColor = value; }
  void SetBorder(float value) { border = value; }
  void SetPosition(float x, float y, float width, float height) {
    position = {x, y, width, height};
  }
  float GetWidth() { return position.z; }
  float GetHeight() { return position.w; }
  int GetNumBars() { return bars.Num(); }
  void SetClamp(bool value) { clamp = value; }
  void SetScaled(bool value) { scaled = value; }
  void SetCaption(const char* text, captionPos_t where) {
    caption = text;
    capPos = where;
  }
  void Render(idRenderModelGui* gui);

  const idMaterial* white;
  idColor bgColor;
  idColor fontColor;
  fillMode_t mode;
  bool sideways;
  float border;
  idVec4 position;
  bool enable;
  bool clamp;
  bool scaled;
  idStr caption;
  captionPos_t capPos;
  idList<graphPlot_t, 3> bars;
  idList<graphPlot_t, 3> grid;
  idList<idStr, 3> labels;
};

class idDebugHistoryGraph {
public:
  struct function_t {
    function_t()
        : name(), values(), peakValue(0.0f), showPeakValue(false),
          peakTime(0) {}
    idStr name;
    idList<float, 3> values;
    float peakValue;
    bool showPeakValue;
    int peakTime;
  };

  idDebugHistoryGraph(float minRange, float maxRange, float width,
                      float height);
  ~idDebugHistoryGraph() = default;
  void AddValue(int functionIndex, float value);
  void AddFunction(const idStr& name);
  void UpdatePeakValue(int functionIndex, float value, bool positiveIsPeak);
  void Render(idRenderModelGui* gui);
  void SetCaption(const idStr& value) { caption = value; }
  void SetPosition(int x, int y) { posX = x; posY = y; }
  void Enable(bool value) { enable = value; }
  bool IsEnabled() { return enable; }
  int GetWidth() { return width; }
  int GetHeight() { return height; }

  idList<function_t, 3> functions;
  idStr caption;
  float minRange;
  float maxRange;
  bool enable;
  int posX;
  int posY;
  int width;
  int height;
};

static_assert(sizeof(idDebugGraph::graphPlot_t) == 0x14,
              "debug graph plot ABI drift");
static_assert(sizeof(idDebugGraph) == 0xC0, "debug graph ABI drift");
static_assert(sizeof(idDebugHistoryGraph::function_t) == 0x50,
              "history graph function ABI drift");
static_assert(sizeof(idDebugHistoryGraph) == 0x60,
              "history graph ABI drift");
