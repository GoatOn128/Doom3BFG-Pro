#include "engine/sys/win32/win_wndproc.h"

#include "engine/sys/sys_local.h"
#include "engine/sys/win32/win_input.h"
#include "engine/sys/win32/win_local.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <Windowsx.h>

#include <algorithm>
#include <cmath>

idCVar win_allowAltTab("win_allowAltTab", "0", CVAR_BOOL,
                      "allow Alt-Tab when fullscreen");
idCVar win_xpos("win_xpos", "3", CVAR_INTEGER | CVAR_ARCHIVE,
                "horizontal position of window");
idCVar win_ypos("win_ypos", "22", CVAR_INTEGER | CVAR_ARCHIVE,
                "vertical position of window");
idCVar win_pauseOnAltTab("win_pauseOnAltTab", "1", CVAR_BOOL,
                        "pause the game when focus is lost");
extern idCVar in_unlockMouseInMenus;

UINT WM_SHOW_OS_CURSOR = WM_USER + 10;
UINT WM_HIDE_OS_CURSOR = WM_USER + 11;

namespace {

constexpr int K_MOUSE1 = 286;
constexpr int K_MWHEELDOWN = 294;
constexpr int K_MWHEELUP = 295;
winWindowCallbacks_t windowCallbacks{};
int oldMouseX = 0;
int oldMouseY = 0;
bool haveOldMousePosition = false;

bool IsFullscreen() {
  return windowCallbacks.isFullscreen != nullptr && windowCallbacks.isFullscreen();
}

unsigned ModifierMask() {
  unsigned modifiers = 0;
  if (GetAsyncKeyState(VK_LSHIFT) & 0x8000) modifiers |= 1;
  if (GetAsyncKeyState(VK_RSHIFT) & 0x8000) modifiers |= 2;
  if (GetAsyncKeyState(VK_LCONTROL) & 0x8000) modifiers |= 4;
  if (GetAsyncKeyState(VK_RCONTROL) & 0x8000) modifiers |= 8;
  if (GetAsyncKeyState(VK_LMENU) & 0x8000) modifiers |= 16;
  if (GetAsyncKeyState(VK_RMENU) & 0x8000) modifiers |= 32;
  return modifiers;
}

unsigned ScanCodeFromMessage(LPARAM parameter) {
  const unsigned scan = (static_cast<unsigned>(parameter) >> 16U) & 0xFFU;
  const unsigned extended = (static_cast<unsigned>(parameter) >> 24U) & 1U;
  return scan | (extended << 7U);
}

void QueueKey(LPARAM parameter, bool down) {
  const int key = static_cast<int>(ScanCodeFromMessage(parameter));
  switch (key) {
    case 0x1D:
    case 0x9D:
    case 0x38:
    case 0xB8:
      return;
    case 0xB7:
      if (!down) return;
      break;
    default:
      break;
  }
  sys->QueueEvent(SE_KEY, key, down ? 1 : 0, 0, nullptr, 0);
  const unsigned modifiers = ModifierMask();
  if (modifiers != 0) {
    sys->QueueEvent(SE_KEY, key, static_cast<int>((modifiers << 1U) | (down ? 1U : 0U)),
                    0, nullptr, 0);
  }
}

void SetCursorVisible(bool visible) {
  for (int attempt = 0; attempt < 10; ++attempt) {
    const int count = ShowCursor(visible ? TRUE : FALSE);
    if ((visible && count >= 0) || (!visible && count < 0)) break;
  }
}

}  // namespace

void Win_SetWindowCallbacks(const winWindowCallbacks_t& callbacks) {
  windowCallbacks = callbacks;
}

void WIN_Sizing(UINT side, RECT* rectangle) {
  if (rectangle == nullptr || (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0) return;

  RECT decoration{};
  DWORD style = win32.hWnd != nullptr
                    ? static_cast<DWORD>(GetWindowLongPtrA(win32.hWnd, GWL_STYLE))
                    : WS_OVERLAPPEDWINDOW;
  AdjustWindowRect(&decoration, style, FALSE);
  const int decorationWidth = decoration.right - decoration.left;
  const int decorationHeight = decoration.bottom - decoration.top;
  int clientWidth = (std::max)(160, static_cast<int>(rectangle->right - rectangle->left) - decorationWidth);
  int clientHeight = (std::max)(120, static_cast<int>(rectangle->bottom - rectangle->top) - decorationHeight);

  HMONITOR monitor = MonitorFromRect(rectangle, MONITOR_DEFAULTTONEAREST);
  MONITORINFO monitorInfo{};
  monitorInfo.cbSize = sizeof(monitorInfo);
  GetMonitorInfoA(monitor, &monitorInfo);
  const int modeWidth = monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
  const int modeHeight = monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
  if (modeWidth <= 0 || modeHeight <= 0) return;
  const float aspect = static_cast<float>(modeWidth) / static_cast<float>(modeHeight);

  switch (side) {
    case WMSZ_LEFT:
      rectangle->left = rectangle->right - decorationWidth - clientWidth;
      rectangle->bottom = rectangle->top + decorationHeight +
                          static_cast<int>(clientWidth / aspect);
      break;
    case WMSZ_RIGHT:
      rectangle->right = rectangle->left + decorationWidth + clientWidth;
      rectangle->bottom = rectangle->top + decorationHeight +
                          static_cast<int>(clientWidth / aspect);
      break;
    case WMSZ_TOP:
    case WMSZ_TOPRIGHT:
      rectangle->top = rectangle->bottom - decorationHeight - clientHeight;
      rectangle->right = rectangle->left + decorationWidth +
                         static_cast<int>(clientHeight * aspect);
      break;
    case WMSZ_BOTTOM:
    case WMSZ_BOTTOMRIGHT:
      rectangle->bottom = rectangle->top + decorationHeight + clientHeight;
      rectangle->right = rectangle->left + decorationWidth +
                         static_cast<int>(clientHeight * aspect);
      break;
    case WMSZ_TOPLEFT:
      rectangle->top = rectangle->bottom - decorationHeight - clientHeight;
      rectangle->left = rectangle->right - decorationWidth -
                        static_cast<int>(clientHeight * aspect);
      break;
    case WMSZ_BOTTOMLEFT:
      rectangle->bottom = rectangle->top + decorationHeight + clientHeight;
      rectangle->left = rectangle->right - decorationWidth -
                        static_cast<int>(clientHeight * aspect);
      break;
    default:
      break;
  }
}

LRESULT CALLBACK MainWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam) {
  switch (message) {
    case WM_CREATE:
      win32.hWnd = window;
      if (windowCallbacks.onWindowCreated != nullptr) windowCallbacks.onWindowCreated(window);
      break;
    case WM_DESTROY:
      win32.hWnd = nullptr;
      break;
    case WM_MOVE:
      if (!IsFullscreen()) {
        RECT decoration{0, 0, 1, 1};
        AdjustWindowRect(&decoration,
                         static_cast<DWORD>(GetWindowLongPtrA(window, GWL_STYLE)), FALSE);
        win_xpos.SetInteger(GET_X_LPARAM(lParam) + decoration.left);
        win_ypos.SetInteger(GET_Y_LPARAM(lParam) + decoration.top);
        win_xpos.flags &= ~CVAR_MODIFIED;
        win_ypos.flags &= ~CVAR_MODIFIED;
      }
      break;
    case WM_ACTIVATE:
      win32.activeApp = LOWORD(wParam) != WA_INACTIVE;
      if (windowCallbacks.onFocusChanged != nullptr) {
        windowCallbacks.onFocusChanged(win32.activeApp);
      }
      break;
    case WM_SETFOCUS:
      win32.activeApp = true;
      SetPriorityClass(GetCurrentProcess(), NORMAL_PRIORITY_CLASS);
      if (sys->Keyboard() != nullptr) sys->Keyboard()->PollInputEvents();
      if (sys->Mouse() != nullptr) sys->Mouse()->GrabCursor(true);
      if (windowCallbacks.onFocusChanged != nullptr) windowCallbacks.onFocusChanged(true);
      break;
    case WM_KILLFOCUS:
      win32.activeApp = false;
      win32.movingWindow = false;
      if (win_pauseOnAltTab.GetBool() && windowCallbacks.onPauseRequested != nullptr &&
          windowCallbacks.onPauseRequested()) {
        SetPriorityClass(GetCurrentProcess(), IDLE_PRIORITY_CLASS);
      }
      if (IsFullscreen() &&
          (windowCallbacks.isFullscreenWindowed == nullptr ||
           !windowCallbacks.isFullscreenWindowed())) {
        ShowWindow(window, SW_MINIMIZE);
      }
      if (sys->Keyboard() != nullptr) sys->Keyboard()->Deactivate();
      if (windowCallbacks.onFocusChanged != nullptr) windowCallbacks.onFocusChanged(false);
      break;
    case WM_CLOSE:
      if (windowCallbacks.onCloseRequested != nullptr) windowCallbacks.onCloseRequested();
      else if (cmdSystem != nullptr) cmdSystem->AppendCommandText("quit");
      return 0;
    case WM_SETCURSOR:
      if (LOWORD(lParam) == HTCLIENT) {
        const bool menuCursor = windowCallbacks.showMenuCursor != nullptr &&
                                windowCallbacks.showMenuCursor();
        SetCursor(menuCursor ? LoadCursorA(nullptr, IDC_ARROW) : nullptr);
        return TRUE;
      }
      break;
    case WM_WINDOWPOSCHANGED: {
      RECT client{};
      POINT origin{};
      if (GetClientRect(window, &client) && ClientToScreen(window, &origin) &&
          windowCallbacks.onWindowSizeChanged != nullptr) {
        windowCallbacks.onWindowSizeChanged(client.right - client.left, client.bottom - client.top,
                                            origin.x, origin.y);
      }
      break;
    }
    case WM_DISPLAYCHANGE:
      if (windowCallbacks.onDisplayChanged != nullptr) windowCallbacks.onDisplayChanged();
      break;
    case WM_KEYDOWN:
      win32.monitorPoweredOff = false;
      QueueKey(lParam, true);
      break;
    case WM_KEYUP:
      win32.monitorPoweredOff = false;
      QueueKey(lParam, false);
      break;
    case WM_CHAR:
      sys->QueueEvent(SE_CHAR, static_cast<int>(wParam), 0, 0, nullptr, 0);
      break;
    case WM_SYSKEYDOWN:
      win32.monitorPoweredOff = false;
      if (wParam == VK_RETURN) {
        if (windowCallbacks.toggleFullscreen != nullptr) windowCallbacks.toggleFullscreen();
        return 0;
      }
      if (wParam == VK_TAB && IsFullscreen() && !win_allowAltTab.GetBool()) return 0;
      QueueKey(lParam, true);
      break;
    case WM_SYSKEYUP:
      QueueKey(lParam, false);
      break;
    case WM_SYSCOMMAND:
      if (wParam == SC_SCREENSAVE || wParam == SC_KEYMENU) {
        return 0;
      }
      if (wParam == SC_MONITORPOWER && lParam == 2) win32.monitorPoweredOff = true;
      break;
    case WM_MOUSEMOVE: {
      const int x = GET_X_LPARAM(lParam);
      const int y = GET_Y_LPARAM(lParam);
      if (!haveOldMousePosition || x != oldMouseX || y != oldMouseY) {
        win32.monitorPoweredOff = false;
        oldMouseX = x;
        oldMouseY = y;
        haveOldMousePosition = true;
      }
      if (in_unlockMouseInMenus.GetBool()) {
        sys->QueueEvent(SE_MOUSE_ABSOLUTE, x, y, 0, nullptr, 0);
        TRACKMOUSEEVENT track{sizeof(track), TME_LEAVE, window, 0};
        TrackMouseEvent(&track);
      }
      return 0;
    }
    case WM_MOUSELEAVE:
      sys->QueueEvent(SE_MOUSE_LEAVE, 0, 0, 0, nullptr, 0);
      return 0;
    case WM_LBUTTONDOWN:
      win32.monitorPoweredOff = false;
      if (in_unlockMouseInMenus.GetBool())
        sys->QueueEvent(SE_KEY, K_MOUSE1, 1, 0, nullptr, 0);
      return 0;
    case WM_LBUTTONUP:
      win32.monitorPoweredOff = false;
      if (in_unlockMouseInMenus.GetBool())
        sys->QueueEvent(SE_KEY, K_MOUSE1, 0, 0, nullptr, 0);
      return 0;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
      win32.monitorPoweredOff = false;
      break;
    case WM_MOUSEWHEEL: {
      win32.monitorPoweredOff = false;
      if (!in_unlockMouseInMenus.GetBool()) break;
      const int steps = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
      const int key = steps >= 0 ? K_MWHEELUP : K_MWHEELDOWN;
      for (int index = 0; index < std::abs(steps); ++index) {
        sys->QueueEvent(SE_KEY, key, 1, 0, nullptr, 0);
        sys->QueueEvent(SE_KEY, key, 0, 0, nullptr, 0);
      }
      break;
    }
    case WM_ENTERSIZEMOVE:
      win32.movingWindow = true;
      break;
    case WM_EXITSIZEMOVE:
      win32.movingWindow = false;
      break;
    case WM_SIZING:
      WIN_Sizing(static_cast<UINT>(wParam), reinterpret_cast<RECT*>(lParam));
      break;
    default:
      if (message == WM_SHOW_OS_CURSOR) SetCursorVisible(true);
      else if (message == WM_HIDE_OS_CURSOR) SetCursorVisible(false);
      break;
  }
  return DefWindowProcA(window, message, wParam, lParam);
}
