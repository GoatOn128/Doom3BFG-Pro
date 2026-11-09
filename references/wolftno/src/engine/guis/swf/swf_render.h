#pragma once

#include "engine/guis/swf/swf_types.h"
#include "shared/idlib/color.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

class idMaterial;

struct tooltipIcon_t {
  int startIndex;
  int endIndex;
  const idMaterial* material;
  std::int16_t imageWidth;
  std::int16_t imageHeight;
  int baseline;

  float GetRenderWidth(float maxGlyphHeight, float toolTipScale) const;
};

struct swfGlyphDrawInfo_t {
  idVec4 glyphTop;
  idVec4 glyphBottom;
  idVec2 s;
  idVec2 t;
  const idMaterial* glyphMat;
  idColor fontColor;
  float zDepth;
};

struct swfGlyphIconInfo_t {
  idVec4 iconTop;
  idVec4 iconBottom;
  idColor iconColor;
  float zDepth;
  const idMaterial* iconMat;
};

struct swfGlyphSelectionInfo_t {
  idVec4 selTop;
  idVec4 selBottom;
  idColor selColor;
  const idMaterial* selMat;
  float zDepth;
};

class TextIterator {
public:
  enum stepResult_t {
    STEP_GLYPH = 0,
    STEP_NEWLINE = 1,
    STEP_TAB = 2,
    STEP_COLOR = 3,
    STEP_END = 4,
    STEP_INSIDE_ICON = 5,
    STEP_ICON_START = 6,
  };

  stepResult_t StepForward();

  const idStr* m_text;
  std::uint32_t m_currTC;
  std::uint32_t m_prevTC;
  int m_charIndex;
  int m_iconIndex;
  int m_endIndex;
  int m_numLines;
  int m_lastBreakableCharIndex;
  bool m_insertingImage;
  const idList<tooltipIcon_t, 72>* m_tooltipIconList;
};

static_assert(sizeof(tooltipIcon_t) == 24, "tooltipIcon_t ABI drift");
static_assert(sizeof(swfGlyphDrawInfo_t) == 80, "swfGlyphDrawInfo_t ABI drift");
static_assert(sizeof(swfGlyphIconInfo_t) == 64, "swfGlyphIconInfo_t ABI drift");
static_assert(sizeof(swfGlyphSelectionInfo_t) == 64, "swfGlyphSelectionInfo_t ABI drift");
static_assert(sizeof(TextIterator) == 48, "TextIterator ABI drift");
