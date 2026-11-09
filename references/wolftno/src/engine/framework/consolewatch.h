#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

struct idCommandLink;

class idConsoleWatch {
public:
  idConsoleWatch() = default;
  idConsoleWatch(const char* watchString, int drawX, int drawY)
      : _watchString(watchString), _drawX(drawX), _drawY(drawY) {}

  idStr _watchString;
  int _drawX = -1;
  int _drawY = -1;
};

extern idList<idConsoleWatch, 5> cwatchList;
extern idList<idStr, 5> cwatchResultTextList;
extern idCommandLink cwatch;
extern idCommandLink cunwatch;

static_assert(sizeof(idConsoleWatch) == 0x30, "console-watch ABI drift");
static_assert(offsetof(idConsoleWatch, _drawX) == 0x28,
              "console-watch draw-X offset drift");

