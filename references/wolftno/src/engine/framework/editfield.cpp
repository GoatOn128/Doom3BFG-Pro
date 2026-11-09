#include "engine/framework/editfield.h"

#include "engine/framework/keyinput.h"
#include "engine/models/rendermodelgui.h"
#include "engine/sys/win32/win_misc.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>

extern float SMALLCHAR_HEIGHT;
extern float SMALLCHAR_WIDTH;

void idAutoComplete::Clear() {
  currentIndex = -1;
  matchLength = 0;
  args.argc = 0;
  suggestions.ClearFree();
}

idEditField::idEditField()
    : overstrikeMode(false), cursor(0), scroll(0), widthInChars(0) {
  Clear();
}

idEditField::~idEditField() = default;

void idEditField::Clear() {
  buffer.Clear();
  cursor = 0;
  scroll = 0;
  autoComplete.Clear();
}

void idEditField::ClearAutoComplete() {
  if (autoComplete.matchLength > 0 &&
      autoComplete.matchLength <= buffer.Length()) {
    buffer.CapLength(autoComplete.matchLength);
    if (cursor > autoComplete.matchLength)
      cursor = autoComplete.matchLength;
  }
  autoComplete.Clear();
}

bool idEditField::AcceptAutoComplete() {
  if (autoComplete.currentIndex < 0 || autoComplete.suggestions.Num() <= 1 ||
      autoComplete.matchLength == 0)
    return false;
  autoComplete.Clear();
  cursor = buffer.Length();
  return true;
}

void idEditField::AutoComplete(bool reverseOrder) {
  const int suggestionCount = autoComplete.suggestions.Num();
  if (suggestionCount != 0) {
    if (suggestionCount <= 1)
      return;
    if (reverseOrder) {
      --autoComplete.currentIndex;
      if (autoComplete.currentIndex < 0)
        autoComplete.currentIndex = suggestionCount - 1;
    } else {
      ++autoComplete.currentIndex;
      if (autoComplete.currentIndex == suggestionCount)
        autoComplete.currentIndex = 0;
    }
    buffer = autoComplete.suggestions[autoComplete.currentIndex];
    if (buffer.Length() - scroll > widthInChars)
      scroll = buffer.Length() - widthInChars;
    return;
  }

  idStr completionCommand(buffer);
  completionCommand.CapLength(cursor);
  autoComplete.Clear();
  autoComplete.args.TokenizeString(completionCommand.data, false);
  if (autoComplete.args.argc == 0)
    return;
  if (completionCommand[cursor - 1] == ' ')
    autoComplete.args.AppendArg("");

  cmdSystem->CommandCompletion(&autoComplete);
  cvarSystem->CommandCompletion(&autoComplete);

  while (autoComplete.suggestions.Num() == 1) {
    if (buffer == autoComplete.suggestions[0].data) {
      buffer += ' ';
      cursor = buffer.Length();
      autoComplete.Clear();
      return;
    }
    buffer = autoComplete.suggestions[0];
    cursor = buffer.Length();
    autoComplete.Clear();
    autoComplete.args.TokenizeString(buffer.data, false);
    cmdSystem->CommandCompletion(&autoComplete);
    cvarSystem->CommandCompletion(&autoComplete);
  }

  if (autoComplete.suggestions.Num() == 0)
    return;

  buffer = autoComplete.suggestions[0];
  autoComplete.matchLength = buffer.Length();
  for (int suggestion = 1; suggestion < autoComplete.suggestions.Num();
       ++suggestion) {
    autoComplete.matchLength = (std::min)(
        autoComplete.matchLength,
        autoComplete.suggestions[suggestion].Length());
    int matching = 0;
    while (matching < autoComplete.matchLength &&
           idStr::ToUpper(buffer[matching]) ==
               idStr::ToUpper(autoComplete.suggestions[suggestion][matching]))
      ++matching;
    autoComplete.matchLength = matching;
  }
  buffer.CapLength(autoComplete.matchLength);
  cursor = autoComplete.matchLength;
  autoComplete.currentIndex = -1;

  if (autoComplete.suggestions.Num() <= 1)
    return;
  idLib::Printf("]%s\n", completionCommand.data);
  for (int index = 0; index < autoComplete.suggestions.Num(); ++index) {
    if (autoComplete.args.argc == 1) {
      idCVar* cvar = cvarSystem->Find(autoComplete.suggestions[index].data);
      if (cvar != nullptr) {
        idLib::Printf(" %s^7 = \"%s\"\n",
                      autoComplete.suggestions[index].data,
                      cvar->GetString());
        continue;
      }
    }
    idLib::Printf(" %s\n", autoComplete.suggestions[index].data);
  }
}

void idEditField::CharEvent(int ch) {
  const char character = static_cast<char>(ch);
  switch (ch) {
  case 22:
    Paste();
    return;
  case 3:
  case 21:
    Clear();
    return;
  case 11:
    buffer.CapLength(cursor);
    return;
  case 8:
    if (cursor > 0) {
      for (int index = cursor; index < buffer.Length(); ++index)
        buffer[index - 1] = buffer[index];
      buffer.CapLength(buffer.Length() - 1);
      --cursor;
    }
    return;
  case 1:
    cursor = 0;
    return;
  case 5:
    cursor = buffer.Length();
    return;
  default:
    break;
  }
  if (ch < 32)
    return;
  if (overstrikeMode) {
    if (cursor >= buffer.Length())
      buffer += character;
    else
      buffer[cursor] = character;
  } else {
    buffer.Insert(character, cursor);
  }
  ++cursor;
}

void idEditField::Paste() {
  char* clipboard = Sys_GetClipboardData();
  if (clipboard == nullptr)
    return;
  for (const char* source = clipboard; *source != '\0'; ++source) {
    if (*source == '\n' || *source == '\r' || *source == '\b')
      break;
    CharEvent(static_cast<unsigned char>(*source));
  }
  idMem::Singleton()->Free(clipboard, ALIGN_16);
}

void idEditField::SetBuffer(const char* text) {
  Clear();
  buffer = text;
  cursor = buffer.Length();
}

void idEditField::KeyDownEvent(int key) {
  if ((key == 210 || key == 82) && idKeyInput::IsShiftDown(0)) {
    ClearAutoComplete();
    Paste();
    return;
  }

  switch (key) {
  case 211:
    if (autoComplete.matchLength > 0) {
      ClearAutoComplete();
      return;
    }
    if (cursor < buffer.Length()) {
      for (int index = cursor; index < buffer.Length() - 1; ++index)
        buffer[index] = buffer[index + 1];
      buffer.CapLength(buffer.Length() - 1);
    }
    return;
  case 32:
    if (!idKeyInput::IsCtrlDown(0)) {
      ClearAutoComplete();
      return;
    }
    if (autoComplete.matchLength > 0) {
      ClearAutoComplete();
      return;
    }
    if (cursor < buffer.Length()) {
      for (int index = cursor; index < buffer.Length() - 1; ++index)
        buffer[index] = buffer[index + 1];
      buffer.CapLength(buffer.Length() - 1);
    }
    return;
  case 205:
  case 33:
    if (idKeyInput::IsCtrlDown(0) && key != 33) {
      while (cursor < buffer.Length() && buffer[cursor] != ' ')
        ++cursor;
      while (cursor < buffer.Length() && buffer[cursor] == ' ')
        ++cursor;
    } else {
      ++cursor;
    }
    if (cursor > buffer.Length())
      cursor = buffer.Length();
    if (autoComplete.matchLength > 0)
      autoComplete.matchLength = cursor;
    autoComplete.suggestions.ClearFree();
    return;
  case 203:
  case 48:
    if (idKeyInput::IsCtrlDown(0) && key != 48) {
      while (cursor > 0 && buffer[cursor - 1] == ' ')
        --cursor;
      while (cursor > 0 && buffer[cursor - 1] != ' ')
        --cursor;
    } else {
      --cursor;
    }
    if (cursor < 0)
      cursor = 0;
    if (autoComplete.matchLength > 0)
      autoComplete.matchLength = cursor;
    autoComplete.suggestions.ClearFree();
    return;
  case 199:
  case 30:
    if (key == 30 && !idKeyInput::IsCtrlDown(0))
      break;
    cursor = 0;
    autoComplete.Clear();
    return;
  case 207:
  case 18:
    if (key == 18 && !idKeyInput::IsCtrlDown(0))
      break;
    cursor = buffer.Length();
    autoComplete.Clear();
    return;
  case 210:
    overstrikeMode = !overstrikeMode;
    return;
  default:
    break;
  }
  if (key != 58 && key != 56 && key != 184 && key != 29 && key != 157 &&
      key != 42 && key != 54)
    ClearAutoComplete();
}

void idEditField::Draw(idRenderModelGui* gui, int x, int y, int width,
                       bool showCursor) const {
  widthInChars = static_cast<int>(static_cast<float>(width) / SMALLCHAR_WIDTH);
  if (widthInChars > 25)
    widthInChars -= 13;
  if (cursor < scroll)
    scroll = (std::max)(cursor - 5, 0);
  if (cursor > widthInChars + scroll)
    scroll = cursor - widthInChars + 5;

  if (autoComplete.matchLength > 0) {
    const int matchStart = autoComplete.matchLength - scroll;
    int matchWidth = buffer.Length() - autoComplete.matchLength;
    if (matchStart < widthInChars) {
      matchWidth = (std::min)(matchWidth, widthInChars - matchStart);
      const idVec4 color{0.80000001f, 0.2f, 0.2f, 0.40000001f};
      gui->DrawFilled(color, static_cast<float>(matchStart) * SMALLCHAR_WIDTH + x,
                      static_cast<float>(y + 2),
                      static_cast<float>(matchWidth) * SMALLCHAR_WIDTH,
                      SMALLCHAR_HEIGHT - 2.0f);
    }
  }

  idStr visible;
  buffer.Mid(scroll, widthInChars, visible);
  gui->DrawString(static_cast<float>(x), static_cast<float>(y), visible.data,
                  idColor::colorWhite, false, 1.0f);
  if (!showCursor || (Sys_Milliseconds() & 0x100) != 0)
    return;

  int cursorColumn = cursor - scroll;
  for (int index = 0; index < cursor; ++index)
    if (idStr::IsColor(&buffer.data[index]))
      cursorColumn -= 2;
  gui->DrawChar(static_cast<float>(cursorColumn) * SMALLCHAR_WIDTH + x,
                static_cast<float>(y), overstrikeMode ? 11u : 95u, 1.0f);
}
