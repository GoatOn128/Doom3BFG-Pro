#include "engine/sys/win32/win_syscon.h"

#include "engine/sys/sys_public.h"
#include "engine/sys/win32/win_local.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/text/str.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>

namespace {
struct consoleWindowData_t {
  HWND window{};
  HWND input{};
  HWND buffer{};
  HWND errorBox{};
  HWND copyButton{};
  HWND clearButton{};
  HWND quitButton{};
  HFONT bufferFont{};
  HFONT buttonFont{};
  HBRUSH editBackground{};
  HBRUSH errorBackground{};
  WNDPROC originalInputProcedure{};
  bool quitOnClose{};
  char consoleText[512]{};
  char returnedText[512]{};
  char errorString[128]{};
  std::string completeText;
} consoleWindow;
bool redirected = false;
unsigned totalChars = 0;

bool ViewLogEnabled() {
  const idCVar* value = cvarSystem != nullptr ? cvarSystem->Find("win_viewlog") : nullptr;
  return value != nullptr && value->GetBool();
}
}

LRESULT CALLBACK InputLineWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  if (message == WM_KILLFOCUS &&
      (reinterpret_cast<HWND>(wParam) == consoleWindow.window ||
       reinterpret_cast<HWND>(wParam) == consoleWindow.errorBox)) {
    SetFocus(window);
    return 0;
  }
  const unsigned scanCode = static_cast<unsigned>((lParam >> 16) & 0xff);
  if (message != WM_CHAR || (scanCode != 28 && scanCode != 156))
    return CallWindowProcA(consoleWindow.originalInputProcedure, window, message, wParam, lParam);
  char line[256]{};
  GetWindowTextA(consoleWindow.input, line, sizeof(line));
  idLib::Printf("]%s\n", line);
  const std::size_t used = std::strlen(consoleWindow.consoleText);
  if (used < sizeof(consoleWindow.consoleText) - 2) {
    std::strncat(consoleWindow.consoleText, line, sizeof(consoleWindow.consoleText) - used - 2);
    std::strcat(consoleWindow.consoleText, "\n");
  }
  SetWindowTextA(consoleWindow.input, "");
  return 0;
}

void Sys_DestroyConsole() {
  if (consoleWindow.window == nullptr) return;
  ShowWindow(consoleWindow.window, SW_HIDE);
  DestroyWindow(consoleWindow.window);
  consoleWindow.window = nullptr;
}

void Sys_RedirectIO() {
  if (redirected) return;
  redirected = true;
  AllocConsole();
  HANDLE output = GetStdHandle(STD_OUTPUT_HANDLE);
  CONSOLE_SCREEN_BUFFER_INFO info{};
  if (GetConsoleScreenBufferInfo(output, &info)) { info.dwSize.Y = 500; SetConsoleScreenBufferSize(output, info.dwSize); }
  FILE* stream = nullptr;
  freopen_s(&stream, "CONOUT$", "w", stdout);
  freopen_s(&stream, "CONIN$", "r", stdin);
  freopen_s(&stream, "CONOUT$", "w", stderr);
  setvbuf(stdout, nullptr, _IONBF, 0);
  setvbuf(stdin, nullptr, _IONBF, 0);
  setvbuf(stderr, nullptr, _IONBF, 0);
}

void Sys_ShowConsole(int visibilityLevel, bool quitOnClose) {
  consoleWindow.quitOnClose = quitOnClose;
  if (consoleWindow.window == nullptr) return;
  const bool wasVisible = win32.visible;
  switch (visibilityLevel) {
    case 0: ShowWindow(consoleWindow.window, SW_HIDE); win32.visible = false; break;
    case 1:
      ShowWindow(consoleWindow.window, SW_SHOWNORMAL);
      SendMessageA(consoleWindow.buffer, EM_LINESCROLL, 0, 0xffff);
      win32.visible = true;
      break;
    case 2: ShowWindow(consoleWindow.window, SW_MINIMIZE); win32.visible = true; break;
    case 3: Sys_RedirectIO(); ShowWindow(consoleWindow.window, SW_HIDE); win32.visible = false; break;
    default: idLib::Error("Invalid visLevel %d sent to Sys_ShowConsole\n", visibilityLevel);
  }
  if (!wasVisible && win32.visible) {
    SetWindowTextA(consoleWindow.buffer, consoleWindow.completeText.c_str());
    SendMessageA(consoleWindow.buffer, EM_SETSEL, 0, -1);
  }
}

char* Sys_ConsoleInput() {
  if (consoleWindow.consoleText[0] == '\0') return nullptr;
  idStr::Copynz(consoleWindow.returnedText, consoleWindow.consoleText,
                sizeof(consoleWindow.returnedText));
  consoleWindow.consoleText[0] = '\0';
  return consoleWindow.returnedText;
}

void Conbuf_AppendText(const char* message) {
  if (message == nullptr) return;
  const std::size_t inputLength = std::strlen(message);
  const char* source = inputLength > 0x3fff ? message + inputLength - 0x3fff : message;
  std::string converted;
  converted.reserve(std::min<std::size_t>(inputLength * 2, 0x7fff));
  for (std::size_t i = 0; source[i] != '\0' && converted.size() < 0x7fff; ++i) {
    if (idStr::IsColor(source + i)) { ++i; continue; }
    if (source[i] == '\r' || source[i] == '\n') {
      if (source[i] == '\n' && source[i + 1] == '\r') ++i;
      converted += "\r\n";
    } else converted += source[i];
  }
  consoleWindow.completeText += converted;
  totalChars += static_cast<unsigned>(converted.size());
  if (totalChars > 0x7000) {
    consoleWindow.completeText = converted;
    totalChars = static_cast<unsigned>(converted.size());
    SendMessageA(consoleWindow.buffer, EM_SETSEL, 0, -1);
  }
  if (consoleWindow.buffer != nullptr) {
    SendMessageA(consoleWindow.buffer, EM_LINESCROLL, 0, 0xffff);
    SendMessageA(consoleWindow.buffer, EM_SCROLLCARET, 0, 0);
    SendMessageA(consoleWindow.buffer, EM_REPLACESEL, 0, reinterpret_cast<LPARAM>(converted.c_str()));
  }
}

LRESULT CALLBACK ConWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE:
      consoleWindow.editBackground = CreateSolidBrush(RGB(0, 0, 128));
      consoleWindow.errorBackground = CreateSolidBrush(RGB(255, 255, 255));
      break;
    case WM_ACTIVATE: if (LOWORD(wParam) != WA_INACTIVE) SetFocus(consoleWindow.input); break;
    case WM_COMMAND:
      if (LOWORD(wParam) == 1) {
        SendMessageA(consoleWindow.buffer, EM_SETSEL, 0, -1);
        SendMessageA(consoleWindow.buffer, WM_COPY, 0, 0);
      } else if (LOWORD(wParam) == 2) {
        if (consoleWindow.quitOnClose) PostQuitMessage(0);
        else {
          char* command = static_cast<char*>(std::malloc(5));
          if (command != nullptr) { std::memcpy(command, "quit", 5); sys->QueueEvent(SE_CONSOLE, 0, 0, 4, command, 0); }
        }
      } else if (LOWORD(wParam) == 3) {
        SetWindowTextA(consoleWindow.buffer, "");
        consoleWindow.completeText.clear();
        totalChars = 0;
      }
      break;
    case WM_CLOSE:
      if (consoleWindow.quitOnClose) PostQuitMessage(0);
      else Sys_ShowConsole(0, false);
      return 0;
    case WM_SYSCOMMAND: if ((wParam & 0xfff0) == SC_CLOSE) PostQuitMessage(0); break;
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLOREDIT:
      if (reinterpret_cast<HWND>(lParam) == consoleWindow.buffer) {
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(0, 0, 128));
        SetTextColor(reinterpret_cast<HDC>(wParam), RGB(255, 255, 0));
        return reinterpret_cast<LRESULT>(consoleWindow.editBackground);
      }
      if (reinterpret_cast<HWND>(lParam) == consoleWindow.errorBox) {
        SetBkColor(reinterpret_cast<HDC>(wParam), RGB(255, 255, 255));
        SetTextColor(reinterpret_cast<HDC>(wParam), RGB(0, 0, 0));
        return reinterpret_cast<LRESULT>(consoleWindow.errorBackground);
      }
      break;
  }
  return DefWindowProcA(window, message, wParam, lParam);
}

void Sys_CreateConsole() {
  WNDCLASSA windowClass{};
  windowClass.style = CS_OWNDC;
  windowClass.lpfnWndProc = ConWndProc;
  windowClass.hInstance = win32.hInstance;
  windowClass.hIcon = LoadIconA(win32.hInstance, MAKEINTRESOURCEA(101));
  windowClass.hCursor = LoadCursorA(nullptr, IDC_ARROW);
  windowClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
  windowClass.lpszClassName = "Wolfenstein The New Order WinConsole";
  if (!RegisterClassA(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) return;
  consoleWindow.window = CreateWindowExA(0, windowClass.lpszClassName,
      "Wolfenstein The New Order Console", WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU |
      WS_MINIMIZEBOX | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT, 540, 475,
      nullptr, nullptr, win32.hInstance, nullptr);
  if (consoleWindow.window == nullptr) return;
  consoleWindow.bufferFont = CreateFontA(-11, 0, 0, 0, FW_LIGHT, FALSE, FALSE, FALSE,
                                         ANSI_CHARSET, 0, 0, 0, FIXED_PITCH, "Courier New");
  consoleWindow.buttonFont = static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
  consoleWindow.input = CreateWindowExA(0, "edit", "", WS_CHILD | WS_VISIBLE | WS_BORDER,
                                        6, 400, 528, 20, consoleWindow.window,
                                        reinterpret_cast<HMENU>(101), win32.hInstance, nullptr);
  consoleWindow.copyButton = CreateWindowExA(0, "button", "Copy", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                             5, 425, 72, 24, consoleWindow.window,
                                             reinterpret_cast<HMENU>(1), win32.hInstance, nullptr);
  consoleWindow.clearButton = CreateWindowExA(0, "button", "Clear", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                              82, 425, 72, 24, consoleWindow.window,
                                              reinterpret_cast<HMENU>(3), win32.hInstance, nullptr);
  consoleWindow.quitButton = CreateWindowExA(0, "button", "Quit", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
                                             462, 425, 72, 24, consoleWindow.window,
                                             reinterpret_cast<HMENU>(2), win32.hInstance, nullptr);
  consoleWindow.buffer = CreateWindowExA(0, "edit", "", WS_CHILD | WS_VISIBLE | WS_VSCROLL |
      ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY, 6, 40, 526, 354,
      consoleWindow.window, reinterpret_cast<HMENU>(100), win32.hInstance, nullptr);
  for (HWND control : {consoleWindow.input, consoleWindow.copyButton, consoleWindow.clearButton,
                       consoleWindow.quitButton, consoleWindow.buffer})
    SendMessageA(control, WM_SETFONT, reinterpret_cast<WPARAM>(control == consoleWindow.buffer ||
                 control == consoleWindow.input ? consoleWindow.bufferFont : consoleWindow.buttonFont), FALSE);
  consoleWindow.originalInputProcedure = reinterpret_cast<WNDPROC>(SetWindowLongPtrA(
      consoleWindow.input, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(InputLineWndProc)));
  if (ViewLogEnabled()) {
    ShowWindow(consoleWindow.window, SW_SHOWDEFAULT);
    UpdateWindow(consoleWindow.window);
    SetForegroundWindow(consoleWindow.window);
    SetFocus(consoleWindow.input);
    win32.visible = true;
  }
}

void Win_SetErrorText(const char* text) {
  idStr::Copynz(consoleWindow.errorString, text != nullptr ? text : "", sizeof(consoleWindow.errorString));
  if (consoleWindow.errorBox == nullptr && consoleWindow.window != nullptr) {
    consoleWindow.errorBox = CreateWindowExA(0, "static", consoleWindow.errorString,
        WS_CHILD | WS_VISIBLE | SS_SUNKEN, 6, 5, 526, 32, consoleWindow.window,
        reinterpret_cast<HMENU>(10), win32.hInstance, nullptr);
    SendMessageA(consoleWindow.errorBox, WM_SETFONT,
                 reinterpret_cast<WPARAM>(consoleWindow.bufferFont), FALSE);
    DestroyWindow(consoleWindow.input);
    consoleWindow.input = nullptr;
  }
  idStr fileName;
  fileName.Format("ErrorLog_%s.txt", Sys_DateStr(true));
  fileName.Replace(" ", "_");
  fileName.Replace("/", "-");
  fileName.Replace(":", "-");
  std::ofstream output(fileName.data, std::ios::binary | std::ios::trunc);
  output << consoleWindow.completeText;
}
