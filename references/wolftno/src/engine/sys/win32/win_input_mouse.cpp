#include "engine/sys/win32/win_input.h"
#include "engine/sys/win32/win_wndproc.h"

#include "engine/sys/sys_local.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <cstdlib>

extern idCVar in_joystickButtons;
extern idCVar in_unlockMouseInMenus;

namespace {
bool hideWindowCursor = false;
bool hideWindowCursorChanged = false;
}

idMouse::~idMouse() = default;
idMouseWin32 mouse;

idMouse* idSysLocal::Mouse() { return &mouse; }

bool idMouseWin32::Init() {
  if (win32.directInput == nullptr) return false;
  if (mouseDevice != nullptr) Shutdown();
  if (FAILED(win32.directInput->CreateDevice(GUID_SysMouse, &mouseDevice, nullptr))) return false;
  if (FAILED(mouseDevice->SetDataFormat(&c_dfDIMouse2))) return false;
  if (FAILED(mouseDevice->SetCooperativeLevel(win32.hWnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND))) return false;
  DIPROPDWORD property{};
  property.diph.dwSize = sizeof(property);
  property.diph.dwHeaderSize = sizeof(property.diph);
  property.diph.dwHow = DIPH_DEVICE;
  property.dwData = 256;
  if (FAILED(mouseDevice->SetProperty(DIPROP_BUFFERSIZE, &property.diph))) return false;
  if (!in_unlockMouseInMenus.GetBool()) Activate();
  PollInputEvents();
  idLib::Printf("mouse: DirectInput initialized.\n");
  return true;
}

void idMouseWin32::Shutdown() {
  Deactivate();
  if (mouseDevice != nullptr) { mouseDevice->Release(); mouseDevice = nullptr; }
}

void idMouseWin32::Activate() {
  if (active || mouseDevice == nullptr) return;
  active = true;
  PostMessageA(win32.hWnd, WM_USER + 11, 0, 0);
  if (!hideWindowCursor) { hideWindowCursor = true; hideWindowCursorChanged = true; }
  if (SUCCEEDED(mouseDevice->Acquire())) {
    mouseDevice->SetCooperativeLevel(win32.hWnd, DISCL_EXCLUSIVE | DISCL_FOREGROUND);
    PollInputEvents();
  }
}

void idMouseWin32::Deactivate() {
  if (mouseDevice == nullptr || !active) return;
  mouseDevice->Unacquire();
  if (in_unlockMouseInMenus.GetBool()) {
    RECT rectangle{};
    if (GetClientRect(win32.hWnd, &rectangle)) {
      POINT point{rectangle.right / 2, rectangle.bottom / 2};
      const int clientX = point.x;
      const int clientY = point.y;
      if (ClientToScreen(win32.hWnd, &point)) {
        SetCursorPos(point.x, point.y);
        sys->QueueEvent(SE_MOUSE_ABSOLUTE, clientX, clientY, 0, nullptr, 0);
      }
    }
  }
  PostMessageA(win32.hWnd, WM_USER + 10, 0, 0);
  if (hideWindowCursor) { hideWindowCursor = false; hideWindowCursorChanged = true; }
  active = false;
}

void idMouseWin32::QueueEvents(int count, bool swapLeftRight) {
  constexpr int keyMouse1 = 286;
  constexpr int keyWheelDown = 294;
  constexpr int keyWheelUp = 295;
  for (int i = 0; i < count; ++i) {
    const DWORD offset = polled[i].dwOfs;
    if (offset >= DIMOFS_BUTTON0 && offset <= DIMOFS_BUTTON7) {
      int button = static_cast<int>(offset - DIMOFS_BUTTON0);
      if (swapLeftRight && button == 0) button = 1;
      else if (swapLeftRight && button == 1) button = 0;
      sys->QueueEvent(SE_KEY, keyMouse1 + button, (polled[i].dwData & 0x80U) != 0, 0, nullptr, 0);
    } else if (offset == DIMOFS_X) {
      sys->QueueEvent(SE_MOUSE, static_cast<int>(polled[i].dwData), 0, 0, nullptr, 0);
    } else if (offset == DIMOFS_Y) {
      sys->QueueEvent(SE_MOUSE, 0, static_cast<int>(polled[i].dwData), 0, nullptr, 0);
    } else if (offset == DIMOFS_Z) {
      const int steps = static_cast<int>(polled[i].dwData) / WHEEL_DELTA;
      const int key = steps < 0 ? keyWheelDown : keyWheelUp;
      for (int step = 0; step < std::abs(steps); ++step) {
        sys->QueueEvent(SE_KEY, key, 1, 0, nullptr, 0);
        sys->QueueEvent(SE_KEY, key, 0, 0, nullptr, 0);
      }
    }
  }
}

int idMouseWin32::PollInputEvents() {
  if (mouseDevice == nullptr || !active) return 0;
  DWORD count = 256;
  HRESULT result = mouseDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), polled, &count, 0);
  if (result != DI_OK) {
    if (FAILED(mouseDevice->Acquire())) return 0;
    mouseDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), polled, &count, 0);
  }
  if (count != 0) {
    QueueEvents(static_cast<int>(count), GetSystemMetrics(SM_SWAPBUTTON) == 1);
    in_joystickButtons.SetInteger(2);
  }
  return static_cast<int>(count);
}

bool idMouseWin32::ReturnInputEvent(unsigned n, int& action, int& value) {
  if (n > 255) return false;
  const DWORD offset = polled[n].dwOfs;
  if (offset >= DIMOFS_BUTTON0 && offset <= DIMOFS_BUTTON7) {
    action = static_cast<int>(offset - DIMOFS_BUTTON0);
    value = (polled[n].dwData & 0x80U) != 0;
    return true;
  }
  if (offset == DIMOFS_X) { action = 8; value = static_cast<int>(polled[n].dwData); return true; }
  if (offset == DIMOFS_Y) { action = 9; value = static_cast<int>(polled[n].dwData); return true; }
  if (offset == DIMOFS_Z) { action = 10; value = static_cast<int>(polled[n].dwData) / WHEEL_DELTA; return value != 0; }
  return false;
}

void idMouseWin32::GrabCursor(bool grab) {
  win32.mouseReleased = !grab;
  if (!grab) IN_Frame();
}

void idMouseWin32::ShowOSCursor() { SetCursor(LoadCursorA(nullptr, IDC_ARROW)); }

idVec2i idMouseWin32::GetCursorPosition() const {
  POINT point{};
  if (GetCursorPos(&point) && ScreenToClient(win32.hWnd, &point)) return {point.x, point.y};
  return {};
}

void Sys_ShowHideCursorRenderThread() {
  if (!hideWindowCursorChanged) return;
  PostMessageA(win32.hWnd, hideWindowCursor ? WM_HIDE_OS_CURSOR : WM_SHOW_OS_CURSOR, 0, 0);
  hideWindowCursorChanged = false;
}
