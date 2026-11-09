#include "engine/framework/debughud.h"

#include "engine/decls/decl.h"
#include "engine/guis/devicecontext.h"
#include "engine/models/rendermodelgui.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <algorithm>
#include <cstdarg>
#include <cstring>

extern float SMALLCHAR_HEIGHT;
extern float SMALLCHAR_WIDTH;
extern idCVar g_stopTime;

idCVar com_debugHUD("com_debugHUD", "0", CVAR_INTEGER, "1 = show debug HUD");
idDebugHUDLocal debugHUDLocal;
idDebugHUD* debugHUD = &debugHUDLocal;
char idDebugHUDLocal::buff[idDebugHUDLocal::MAX_DEST];

namespace {

void FormatLine(idGUILine& line, const char* format, std::va_list args) {
  line.text.VFormat(format, args);
}

} // namespace

idGUILine::idGUILine()
    : flags(0), x(0), y(0), headerColor{}, textColor{}, heading(), text() {}

idGUIRect::idGUIRect(const char* titleValue, int borderWidthValue,
                     int borderHeightValue, const idColor& bgColorValue,
                     const idColor& textColorValue,
                     const idColor& titleBarColorValue,
                     const idColor& titleTextColorValue, float alphaValue,
                     float textAlphaValue)
    : title(titleValue), borderWidth(borderWidthValue),
      borderHeight(borderHeightValue), bgColor(bgColorValue),
      textColor(textColorValue), titleBarColor(titleBarColorValue),
      titleTextColor(titleTextColorValue), alpha(alphaValue),
      textAlpha(textAlphaValue), lines(), left(0), top(0), width(0), height(0) {
  bgColor.a = alpha;
  textColor.a = alpha;
  titleBarColor.a = alpha;
  titleTextColor.a = alpha;
}

void idGUIRect::Printf(const char* headingValue, const char* format, ...) {
  idGUILine* line = lines.Alloc();
  line->flags = 0;
  line->heading = headingValue;
  std::va_list args;
  va_start(args, format);
  FormatLine(*line, format, args);
  va_end(args);
}

void idGUIRect::Printf(const idColor& header, const idColor& textValue,
                       const char* headingValue, const char* format, ...) {
  idGUILine* line = lines.Alloc();
  line->flags = 2;
  line->headerColor = header;
  line->textColor = textValue;
  line->heading = headingValue;
  std::va_list args;
  va_start(args, format);
  FormatLine(*line, format, args);
  va_end(args);
}

void idGUIRect::Printf(int xValue, int yValue, const idColor& header,
                       const idColor& textValue, const char* headingValue,
                       const char* format, ...) {
  idGUILine* line = lines.Alloc();
  line->flags = 3;
  line->x = xValue;
  line->y = yValue;
  line->headerColor = header;
  line->textColor = textValue;
  line->heading = headingValue;
  std::va_list args;
  va_start(args, format);
  FormatLine(*line, format, args);
  va_end(args);
}

void idGUIRect::Render(idDeviceContext& context, int xValue, int yValue,
                       float scale) {
  left = xValue;
  top = yValue;
  context.uiOffset = {static_cast<float>(xValue), static_cast<float>(yValue)};
  const int textHeight = context.TextHeight(scale);
  const bool hasTitle = !title.IsEmpty();
  int lineY = borderHeight;
  if (!hasTitle)
    lineY += textHeight;
  height = lineY + borderHeight + textHeight * lines.Num();
  int maximumHeadingWidth = 0;
  int maximumTextWidth = 0;
  for (int index = 0; index < lines.Num(); ++index) {
    maximumHeadingWidth = (std::max)(
        maximumHeadingWidth, context.TextWidth(scale, lines[index].heading.data, 0));
    maximumTextWidth = (std::max)(
        maximumTextWidth, context.TextWidth(scale, lines[index].text.data, 0));
    if ((lines[index].flags & 1) != 0)
      lineY = lines[index].y;
    else
      lineY += textHeight;
    height = (std::max)(height, borderHeight + lineY + textHeight);
  }
  width = maximumHeadingWidth + maximumTextWidth + borderWidth * 2 + 8;
  idColor scaledColor = bgColor;
  scaledColor.a *= alpha;
  context.DrawFilledRect(0.0f, 0.0f, static_cast<float>(width),
                         static_cast<float>(height), scaledColor);

  int textY = textHeight + (borderHeight >> 1);
  if (hasTitle) {
    scaledColor = titleBarColor;
    scaledColor.a *= alpha;
    context.DrawFilledRect(0.0f, 0.0f, static_cast<float>(width),
                           static_cast<float>(textHeight + borderHeight * 2),
                           scaledColor);
    scaledColor = titleTextColor;
    scaledColor.a = textAlpha * alpha;
    context.DrawTextA(static_cast<float>(borderWidth), static_cast<float>(textY),
                      scale, scaledColor, title.data);
    textY += textHeight + borderHeight - (borderHeight >> 1);
  }
  const float textX = static_cast<float>(borderWidth + maximumHeadingWidth + 8);
  for (int index = 0; index < lines.Num(); ++index) {
    idGUILine& line = lines[index];
    if ((line.flags & 1) != 0)
      textY = line.y;
    idColor heading = (line.flags & 2) != 0 ? line.headerColor : titleTextColor;
    idColor text = (line.flags & 2) != 0 ? line.textColor : textColor;
    heading.a = ((line.flags & 2) != 0 ? heading.a : textAlpha) * alpha;
    text.a = ((line.flags & 2) != 0 ? text.a : textAlpha) * alpha;
    context.DrawTextA(static_cast<float>(borderWidth + line.x),
                      static_cast<float>(textY), scale, heading,
                      line.heading.data);
    context.DrawTextA(textX, static_cast<float>(textY), scale, text,
                      line.text.data);
    textY += textHeight;
  }
  lines.ClearFree();
}

idDebugHUDLocal::idDebugHUDLocal()
    : debugText(), persistentText(), curColor{1.0f, 1.0f, 1.0f, 1.0f},
      curScale(0.75f), curX(0), curY(0), startX(0), alignRight(false),
      white(nullptr) {}

idDebugHUDLocal::~idDebugHUDLocal() { Clear(true); }

idDebugHUDLocal::debugText_t* idDebugHUDLocal::Alloc() {
  return debugText.Alloc();
}

idDebugHUDLocal::debugText_t* idDebugHUDLocal::AllocPersistent(int x, int y) {
  RemovePersistentText(x, y);
  return persistentText.Alloc();
}

void idDebugHUDLocal::SetLeftAlign() { alignRight = false; }
void idDebugHUDLocal::SetRightAlign() { alignRight = true; }
void idDebugHUDLocal::SetTextPosition(int x, int y) {
  curX = x;
  curY = y;
  startX = x;
}
void idDebugHUDLocal::SetTextScale(float scale) { curScale = scale; }
void idDebugHUDLocal::SetTextColor(const idColor& color) { curColor = color; }

void idDebugHUDLocal::Printf(const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  Printf(curX, curY, curColor, 1.0f, 0, format, args);
  va_end(args);
}

void idDebugHUDLocal::Printf(int x, int y, const char* format,
                             std::va_list args) {
  Printf(x, y, curColor, 1.0f, 4, format, args);
}

void idDebugHUDLocal::Printf(int x, int y, const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  Printf(x, y, curColor, 1.0f, 4, format, args);
  va_end(args);
}

void idDebugHUDLocal::Printf(int x, int y, const idColor& color,
                             const idColor& background, const char* format,
                             std::va_list args) {
  Printf(x, y, color, background, 1.0f, 37, format, args);
}

void idDebugHUDLocal::Printf(int x, int y, const idColor& color,
                             const idColor& background, const char* format,
                             ...) {
  std::va_list args;
  va_start(args, format);
  Printf(x, y, color, background, 1.0f, 37, format, args);
  va_end(args);
}

void idDebugHUDLocal::Printf(int x, int y, const idColor& color,
                             const char* format, std::va_list args) {
  Printf(x, y, color, idColor::colorBlack, 1.0f, 5, format, args);
}

void idDebugHUDLocal::Printf(int x, int y, const idColor& color,
                             const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  Printf(x, y, color, idColor::colorBlack, 1.0f, 5, format, args);
  va_end(args);
}

void idDebugHUDLocal::Printf(int x, int y, const idColor& color, float scale,
                             const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  Printf(x, y, color, idColor::colorBlack, scale, 7, format, args);
  va_end(args);
}

void idDebugHUDLocal::Printf(int x, int y, const idColor& color, float scale,
                             int flags, const char* format,
                             std::va_list args) {
  Printf(x, y, color, idColor::colorBlack, scale, flags, format, args);
}

void idDebugHUDLocal::Printf(int x, int y, const idColor& color,
                             const idColor& background, float scale,
                             const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  Printf(x, y, color, background, scale, 39, format, args);
  va_end(args);
}

void idDebugHUDLocal::Printf(int x, int y, const idColor& color,
                             const idColor& background, float scale, int flags,
                             const char* format, std::va_list args) {
  if (com_debugHUD.valueInteger == 0 || g_stopTime.valueInteger != 0)
    return;
  idStr::vsnPrintf(buff, MAX_DEST, format, args);
  char linesText[MAX_LINES][MAX_DEST]{};
  int lineLengths[MAX_LINES]{};
  bool newLines[MAX_LINES]{};
  int lineIndex = 0;
  for (const char* source = buff; *source != '\0'; ++source) {
    if (*source == '\n') {
      newLines[lineIndex] = true;
      if (++lineIndex >= MAX_LINES || source[1] == '\0')
        break;
    } else if (lineLengths[lineIndex] < MAX_DEST - 1) {
      linesText[lineIndex][lineLengths[lineIndex]++] = *source;
    }
  }
  const int lineHeight = static_cast<int>(SMALLCHAR_HEIGHT * scale * curScale);
  const int characterWidth = static_cast<int>(SMALLCHAR_WIDTH * scale * curScale);
  if ((flags & 4) != 0)
    SetTextPosition(x, y);
  for (int line = 0; line <= lineIndex && line < MAX_LINES; ++line) {
    if (linesText[line][0] != '\0') {
      debugText_t* output = Alloc();
      output->text = linesText[line];
      output->scale = scale;
      output->flags = flags | (alignRight ? 8 : 0);
      output->x = curX;
      output->y = curY;
      output->color = color;
      output->len = lineLengths[line];
      output->backgroundColor = background;
    }
    if (newLines[line]) {
      curY += lineHeight;
      curX = startX;
    } else {
      curX += characterWidth * lineLengths[line];
    }
  }
}

void idDebugHUDLocal::AddPersistentText(int x, int y, const idColor& color,
                                        float scale, const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  idStr::vsnPrintf(buff, MAX_DEST, format, args);
  va_end(args);
  debugText_t* output = AllocPersistent(x, y);
  output->text = buff;
  output->x = x;
  output->y = y;
  output->scale = scale;
  output->flags = (alignRight ? 8 : 0) | 0x17;
  output->color = color;
  output->len = output->text.Length();
}

void idDebugHUDLocal::RemovePersistentText(int x, int y) {
  for (int index = persistentText.Num() - 1; index >= 0; --index) {
    if (persistentText[index].x == x && persistentText[index].y == y)
      persistentText.RemoveIndexFast(index);
  }
}

void idDebugHUDLocal::Clear(bool clearPersistent) {
  if (g_stopTime.valueInteger != 0)
    return;
  startX = 0;
  curX = 0;
  curY = 0;
  alignRight = false;
  debugText.ClearFree();
  if (clearPersistent)
    persistentText.ClearFree();
}

void idDebugHUDLocal::Frame() {
  if (g_stopTime.valueInteger == 0)
    Clear(false);
}

void idDebugHUDLocal::Render(idRenderModelGui* gui) {
  if (com_debugHUD.valueInteger == 0) {
    Clear(false);
    return;
  }
  if (white == nullptr)
    white = static_cast<const idMaterial*>(
        idMaterial::resourceList.FindWithInheritance("_white", true, true));
  auto drawText = [&](debugText_t& entry) {
    const float renderScale = entry.scale * curScale;
    float drawX = static_cast<float>(entry.x);
    const float drawWidth = entry.len * SMALLCHAR_WIDTH * renderScale;
    if ((entry.flags & 8) != 0)
      drawX -= drawWidth;
    if (white != nullptr && (entry.flags & 0x20) != 0) {
      gui->currentVertexColor = idColor::PackColor(entry.backgroundColor);
      gui->DrawStretchPic(drawX, static_cast<float>(entry.y), 0.0f, drawWidth,
                          SMALLCHAR_HEIGHT * renderScale, 0.0f, 0.0f, 1.0f,
                          1.0f, white);
    }
    gui->DrawString(drawX, static_cast<float>(entry.y), entry.text.data,
                    entry.color, false, renderScale);
  };
  const float virtualHeight = gui->GetVirtualHeight();
  for (int index = 0; index < debugText.Num(); ++index) {
    debugText_t& entry = debugText[index];
    gui->currentVertexColor = idColor::PackColor(idColor::colorWhite);
    const float textHeight = SMALLCHAR_HEIGHT * entry.scale * curScale;
    if (entry.y <= virtualHeight - textHeight && entry.y + textHeight >= 0.0f)
      drawText(entry);
  }
  for (int index = 0; index < persistentText.Num(); ++index) {
    debugText_t& entry = persistentText[index];
    const float textHeight = SMALLCHAR_HEIGHT * entry.scale * curScale;
    if (entry.y <= 480 && entry.y + textHeight >= 0.0f) {
      gui->currentVertexColor = idColor::PackColor(idColor::colorWhite);
      drawText(entry);
    }
  }
}
