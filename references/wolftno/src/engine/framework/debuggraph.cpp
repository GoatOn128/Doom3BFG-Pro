#include "engine/framework/debuggraph.h"

#include "engine/guis/devicecontext.h"
#include "engine/models/rendermodelgui.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cmath>

extern float SMALLCHAR_HEIGHT;
extern float SMALLCHAR_WIDTH;

idDebugGraph::idDebugGraph(int numItems)
    : white(nullptr), bgColor{0.0f, 0.0f, 0.0f, 0.5f},
      fontColor{1.0f, 1.0f, 1.0f, 1.0f}, mode(GRAPH_FILL),
      sideways(false), border(0.0f), position{100.0f, 100.0f, 100.0f, 100.0f},
      enable(true), clamp(false), scaled(false), caption(), capPos(CAP_NONE),
      bars(), grid(), labels() {
  Init(numItems);
}

void idDebugGraph::Init(int numBars) {
  bars.SetNum(numBars);
  labels.ClearFree();
  for (int index = 0; index < numBars; ++index)
    bars[index].value = 0.0f;
}

void idDebugGraph::SetLabel(int bar, const char* text) {
  labels.SetNum(bars.Num());
  labels[bar] = text;
}

void idDebugGraph::AddGridLine(float value, const idColor& color) {
  graphPlot_t* line = grid.Alloc();
  line->value = value;
  line->color = color;
}

void idDebugGraph::SetValue(int bar, float value, const idColor& color) {
  if (!enable)
    return;
  if (clamp)
    value = std::clamp(value, 0.0f, 1.0f);
  if (bar >= 0) {
    bars[bar].value = value;
    bars[bar].color = color;
    return;
  }
  if (bars.Num() > 0)
    bars.RemoveIndex(0);
  graphPlot_t* plot = bars.Alloc();
  plot->value = value;
  plot->color = color;
}

void idDebugGraph::Render(idRenderModelGui* gui) {
  if (!enable)
    return;
  if (white == nullptr)
    white = static_cast<const idMaterial*>(
        idMaterial::resourceList.FindWithInheritance("_white", true, true));
  gui->currentVertexColor = idColor::PackColor(bgColor);
  gui->DrawStretchPic(position.x, position.y, 0.0f, position.z, position.w,
                      0, 0, 0, 0, white);
  const int count = bars.Num();
  if (count == 0)
    return;
  float scale = 1.0f;
  if (scaled) {
    float maximum = 0.0f;
    for (int i = 0; i < count; ++i)
      maximum = std::fmax(maximum, bars[i].value);
    scale = 1.0f / maximum;
  }
  const float innerX = position.x + border;
  const float innerY = position.y + border;
  const float innerW = position.z - border * 2.0f;
  const float innerH = position.w - border * 2.0f;
  if (sideways) {
    const float barH = (position.w - border) / count;
    for (int i = 0; i < count; ++i) {
      float x = 0.0f;
      float y = 0.0f;
      float w = 0.0f;
      float h = 0.0f;
      if (mode == GRAPH_LINE) {
        x = innerX + innerW * bars[i].value * scale;
        y = innerY + i * barH;
        w = 1.0f;
        h = barH - border;
      } else if (mode == GRAPH_FILL) {
        x = innerX;
        y = innerY + i * barH;
        w = innerW * bars[i].value * scale;
        h = barH - border;
      } else if (mode == GRAPH_FILL_REVERSE) {
        x = innerX + innerW;
        y = innerY + i * barH;
        w = innerW - innerW * bars[i].value * scale;
        h = barH - border;
      }
      gui->currentVertexColor = idColor::PackColor(bars[i].color);
      gui->DrawStretchPic(x, y, 0, w, h, 0, 0, 0, 0, white);
    }
    if (labels.Num() > 0) {
      int maximumLength = 0;
      for (int i = 0; i < labels.Num(); ++i)
        maximumLength = (std::max)(maximumLength, labels[i].Length());
      const float labelWidth = maximumLength * SMALLCHAR_WIDTH;
      gui->currentVertexColor = idColor::PackColor(bgColor);
      gui->DrawStretchPic(position.x - labelWidth, position.y, 0, labelWidth,
                          position.w, 0, 0, 0, 0, white);
      for (int i = 0; i < labels.Num(); ++i) {
        gui->DrawString(
            static_cast<float>(static_cast<int>(innerX - labelWidth)),
            static_cast<float>(static_cast<int>(innerY + i * barH)),
            labels[i].data, fontColor, true, 1.0f);
      }
    }
  } else {
    const float barW = (position.z - border) / count;
    const float bottom = innerY + innerH;
    for (int i = 0; i < grid.Num(); ++i) {
      gui->currentVertexColor = idColor::PackColor(grid[i].color);
      gui->DrawStretchPic(position.x, bottom - innerH * grid[i].value, 0,
                          position.z, 1, 0, 0, 0, 0, white);
    }
    for (int i = 0; i < count; ++i) {
      float x = 0.0f;
      float y = 0.0f;
      float w = 0.0f;
      float h = 0.0f;
      const float valueH = innerH * bars[i].value * scale;
      if (mode == GRAPH_LINE) {
        x = innerX + i * barW;
        y = bottom - valueH;
        w = barW - border;
        h = 1.0f;
      } else if (mode == GRAPH_FILL) {
        x = innerX + i * barW;
        y = bottom - valueH;
        w = barW - border;
        h = valueH;
      } else if (mode == GRAPH_FILL_REVERSE) {
        x = innerX + i * barW;
        y = innerY;
        w = barW - border;
        h = valueH;
      }
      gui->currentVertexColor = idColor::PackColor(bars[i].color);
      gui->DrawStretchPic(x, y, 0, w, h, 0, 0, 0, 0, white);
    }
    if (labels.Num() > 0) {
      gui->currentVertexColor = idColor::PackColor(bgColor);
      gui->DrawStretchPic(position.x, position.y + innerH, 0, position.z,
                          SMALLCHAR_HEIGHT, 0, 0, 0, 0, white);
      for (int i = 0; i < labels.Num(); ++i) {
        gui->DrawString(
            static_cast<float>(static_cast<int>(innerX + i * barW)),
            static_cast<float>(static_cast<int>(bottom + border)),
            labels[i].data, fontColor, true, 1.0f);
      }
    }
  }
  if (capPos != CAP_NONE) {
    const float textWidth = idStr::LengthWithoutColors(caption.data) * SMALLCHAR_WIDTH;
    float x = 0.0f, y = 0.0f;
    if (capPos == CAP_LEFT) {
      x = position.x - textWidth - 5.0f;
      y = position.y + position.w * 0.5f - SMALLCHAR_HEIGHT * 0.5f;
    } else if (capPos == CAP_RIGHT) {
      x = position.x + position.z + 5.0f;
      y = position.y + position.w * 0.5f - SMALLCHAR_HEIGHT * 0.5f;
    } else if (capPos == CAP_TOP) {
      x = position.x + position.z * 0.5f - textWidth * 0.5f;
      y = position.y - 5.0f - SMALLCHAR_HEIGHT;
    } else {
      x = position.x + position.z * 0.5f - textWidth * 0.5f;
      y = position.y + position.w + 5.0f;
    }
    gui->DrawString(static_cast<float>(static_cast<int>(x + SMALLCHAR_WIDTH * 0.5f)),
                    static_cast<float>(static_cast<int>(y)), caption.data,
                    fontColor, true, 1.0f);
  }
}

idDebugHistoryGraph::idDebugHistoryGraph(float minimum, float maximum,
                                         float graphWidth, float graphHeight)
    : functions(), caption(), minRange(minimum), maxRange(maximum),
      enable(true), posX(0), posY(0), width(static_cast<int>(graphWidth)),
      height(static_cast<int>(graphHeight)) {}

void idDebugHistoryGraph::AddFunction(const idStr& functionName) {
  function_t function;
  function.name = functionName;
  function.values.SetNum(width);
  for (int index = 0; index < width; ++index)
    function.values[index] = minRange;
  functions.Append(function);
}

void idDebugHistoryGraph::AddValue(int functionIndex, float value) {
  function_t& function = functions[functionIndex];
  const float graphValue =
      (value - minRange) * static_cast<float>(height) / (maxRange - minRange);
  if (function.values.Num() > 0)
    function.values.RemoveIndex(0);
  function.values.Append(graphValue);
}

void idDebugHistoryGraph::UpdatePeakValue(int functionIndex, float value,
                                          bool positiveIsPeak) {
  function_t& function = functions[functionIndex];
  function.showPeakValue = true;
  if ((positiveIsPeak && value > function.peakValue) ||
      (!positiveIsPeak && function.peakValue > value)) {
    function.peakValue = value;
    function.peakTime = static_cast<int>(Sys_Milliseconds());
    return;
  }
  if (Sys_Milliseconds() > function.peakTime + 5000)
    function.peakValue = value;
}

void idDebugHistoryGraph::Render(idRenderModelGui* gui) {
  if (!enable)
    return;
  static const idColor colors[5] = {
      {0.95686281f, 0.94117653f, 0.7960785f, 1.0f},
      {0.71764708f, 0.90196085f, 0.54509807f, 1.0f},
      {0.8705883f, 0.82352948f, 0.91372555f, 1.0f},
      {0.7019608f, 0.64705884f, 0.50196081f, 1.0f},
      {0.40784317f, 0.33725491f, 0.25882354f, 1.0f}};
  idDeviceContext dc(gui);
  dc.DrawFilledRect(static_cast<float>(posX), static_cast<float>(posY),
                    static_cast<float>(width), static_cast<float>(height),
                    idVec4{0.0f, 0.0f, 0.0f, 0.5f});
  float textX = static_cast<float>(posX);
  for (int functionIndex = 0; functionIndex < functions.Num(); ++functionIndex) {
    function_t& function = functions[functionIndex];
    const idColor& color = colors[functionIndex % 5];
    for (int point = 0; point < width - 1; ++point) {
      dc.DrawLine({static_cast<float>(posX + point),
                   static_cast<float>(posY + height) - function.values[point]},
                  {static_cast<float>(posX + point + 1),
                   static_cast<float>(posY + height) - function.values[point + 1]},
                  0.5f, color);
    }
    idStr label;
    if (function.showPeakValue)
      label.Format("%s(%.01f)", function.name.data, function.peakValue);
    else
      label = function.name;
    dc.DrawShadowText(textX, static_cast<float>(posY + height + dc.TextHeight(0.25f)),
                      0.25f, color, label.data);
    textX += static_cast<float>(dc.TextWidth(0.25f, label.data, -1) + 8);
  }
  dc.DrawShadowText(static_cast<float>(posX), static_cast<float>(posY - 4),
                    0.25f, idColor::colorWhite, caption.data);
}
