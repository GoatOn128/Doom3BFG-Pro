#pragma once

#include "shared/idlib/csystems/autocomplete.h"
#include "shared/idlib/text/str.h"

class idRenderModelGui;

class idEditField {
public:
  idEditField();
  ~idEditField();

  void Clear();
  void SetCursor(int value) { cursor = value; }
  int GetCursor() const { return cursor; }
  void ClearAutoComplete();
  bool AcceptAutoComplete();
  void AutoComplete(bool reverseOrder);
  void CharEvent(int ch);
  void KeyDownEvent(int key);
  void Paste();
  const char* GetBuffer() const { return buffer.data; }
  int GetLength() const { return buffer.Length(); }
  void SetBuffer(const char* text);
  void Draw(idRenderModelGui* gui, int x, int y, int width,
            bool showCursor) const;

  bool overstrikeMode;
  int cursor;
  mutable int scroll;
  mutable int widthInChars;
  idStr buffer;
  idAutoComplete autoComplete;
};

static_assert(offsetof(idEditField, buffer) == 0x10,
              "edit-field buffer offset drift");
static_assert(offsetof(idEditField, autoComplete) == 0x38,
              "edit-field autocomplete offset drift");
static_assert(sizeof(idEditField) == 0x1860, "edit-field ABI drift");
