#pragma once

#include "shared/idlib/color.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

#include <cstdarg>

class idDeviceContext;
class idMaterial;
class idRenderModelGui;

class idDebugHUD {
public:
  virtual ~idDebugHUD() = default;
  virtual void Render(idRenderModelGui* gui) = 0;
  virtual void Frame() = 0;
  virtual void Clear(bool clearPersistent) = 0;
  virtual void Printf(int x, int y, const idColor& color,
                      const idColor& backgroundColor, float scale, int flags,
                      const char* format, std::va_list args) = 0;
  virtual void Printf(int x, int y, const idColor& color,
                      const idColor& backgroundColor, float scale,
                      const char* format, ...) = 0;
  virtual void Printf(int x, int y, const idColor& color, float scale,
                      int flags, const char* format, std::va_list args) = 0;
  virtual void Printf(int x, int y, const idColor& color, float scale,
                      const char* format, ...) = 0;
  virtual void Printf(int x, int y, const idColor& color, const char* format,
                      ...) = 0;
  virtual void Printf(int x, int y, const idColor& color, const char* format,
                      std::va_list args) = 0;
  virtual void Printf(int x, int y, const idColor& color,
                      const idColor& backgroundColor, const char* format,
                      ...) = 0;
  virtual void Printf(int x, int y, const idColor& color,
                      const idColor& backgroundColor, const char* format,
                      std::va_list args) = 0;
  virtual void Printf(int x, int y, const char* format, ...) = 0;
  virtual void Printf(int x, int y, const char* format, std::va_list args) = 0;
  virtual void Printf(const char* format, ...) = 0;
  virtual void AddPersistentText(int x, int y, const idColor& color,
                                 float scale, const char* format, ...) = 0;
  virtual void RemovePersistentText(int x, int y) = 0;
  virtual void SetLeftAlign() = 0;
  virtual void SetRightAlign() = 0;
  virtual void SetTextPosition(int x, int y) = 0;
  virtual void SetTextScale(float scale) = 0;
  virtual void SetTextColor(const idColor& color) = 0;
};

struct idGUILine {
  idGUILine();
  int flags;
  int x;
  int y;
  idColor headerColor;
  idColor textColor;
  idStr heading;
  idStr text;
};

class idGUIRect {
public:
  idGUIRect(const char* title, int borderWidth, int borderHeight,
            const idColor& bgColor, const idColor& textColor,
            const idColor& titleBarColor, const idColor& titleTextColor,
            float alpha, float textAlpha);
  ~idGUIRect() = default;
  void Printf(const char* heading, const char* format, ...);
  void Printf(const idColor& headerColor, const idColor& textColor,
              const char* heading, const char* format, ...);
  void Printf(int x, int y, const idColor& headerColor,
              const idColor& textColor, const char* heading,
              const char* format, ...);
  void Render(idDeviceContext& context, int x, int y, float scale);
  int GetLeft() { return left; }
  int GetRight() { return left + width; }
  int GetTop() { return top; }
  int GetBottom() { return top + height; }
  void SetTextAlpha(float value) { textAlpha = value; }

  idStr title;
  int borderWidth;
  int borderHeight;
  idColor bgColor;
  idColor textColor;
  idColor titleBarColor;
  idColor titleTextColor;
  float alpha;
  float textAlpha;
  idList<idGUILine, 5> lines;
  int left;
  int top;
  int width;
  int height;
};

class idDebugHUDLocal final : public idDebugHUD {
public:
  static constexpr int MAX_LINES = 4;
  static constexpr int MAX_DEST = 1024;
  struct debugText_t {
    idStr text;
    int len = 0;
    int x = 0;
    int y = 0;
    idColor color{};
    int flags = 0;
    float scale = 0.0f;
    idColor backgroundColor{};
  };

  idDebugHUDLocal();
  ~idDebugHUDLocal() override;
  void Render(idRenderModelGui* gui) override;
  void Frame() override;
  void Clear(bool clearPersistent) override;
  void Printf(int x, int y, const idColor& color,
              const idColor& backgroundColor, float scale, int flags,
              const char* format, std::va_list args) override;
  void Printf(int x, int y, const idColor& color,
              const idColor& backgroundColor, float scale,
              const char* format, ...) override;
  void Printf(int x, int y, const idColor& color, float scale, int flags,
              const char* format, std::va_list args) override;
  void Printf(int x, int y, const idColor& color, float scale,
              const char* format, ...) override;
  void Printf(int x, int y, const idColor& color, const char* format,
              ...) override;
  void Printf(int x, int y, const idColor& color, const char* format,
              std::va_list args) override;
  void Printf(int x, int y, const idColor& color,
              const idColor& backgroundColor, const char* format, ...) override;
  void Printf(int x, int y, const idColor& color,
              const idColor& backgroundColor, const char* format,
              std::va_list args) override;
  void Printf(int x, int y, const char* format, ...) override;
  void Printf(int x, int y, const char* format, std::va_list args) override;
  void Printf(const char* format, ...) override;
  void AddPersistentText(int x, int y, const idColor& color, float scale,
                         const char* format, ...) override;
  void RemovePersistentText(int x, int y) override;
  void SetLeftAlign() override;
  void SetRightAlign() override;
  void SetTextPosition(int x, int y) override;
  void SetTextScale(float scale) override;
  void SetTextColor(const idColor& color) override;

  debugText_t* Alloc();
  debugText_t* AllocPersistent(int x, int y);

  idList<debugText_t, 5> debugText;
  idList<debugText_t, 5> persistentText;
  idColor curColor;
  float curScale;
  int curX;
  int curY;
  int startX;
  bool alignRight;
  const idMaterial* white;
  static char buff[MAX_DEST];
};

extern idDebugHUDLocal debugHUDLocal;
extern idDebugHUD* debugHUD;

static_assert(sizeof(idDebugHUD) == 8, "debug HUD interface ABI drift");
static_assert(sizeof(idGUILine) == 0x80, "GUI line ABI drift");
static_assert(sizeof(idGUIRect) == 0xA0, "GUI rectangle ABI drift");
static_assert(sizeof(idDebugHUDLocal::debugText_t) == 0x60,
              "debug HUD text ABI drift");
static_assert(sizeof(idDebugHUDLocal) == 0x68, "debug HUD local ABI drift");
