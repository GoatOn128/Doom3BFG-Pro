#include "engine/sys/win32/win_input.h"

#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

idCVar in_mouse("in_mouse", "1", CVAR_BOOL | CVAR_ARCHIVE, "enable mouse input");
idCVar in_lockMouse("in_lockMouse", "1", CVAR_BOOL | CVAR_ARCHIVE, "Enable mouse locking");

void IN_InitDirectInput() {
  idLib::Printf("Initializing DirectInput...\n");
  if (win32.directInput != nullptr) { win32.directInput->Release(); win32.directInput = nullptr; }
  if (FAILED(DirectInput8Create(GetModuleHandleA(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A,
                                reinterpret_cast<void**>(&win32.directInput), nullptr)))
    idLib::Printf("DirectInput8Create failed\n");
}

void Sys_ShutdownInput() {
  if (sys->Joystick() != nullptr) sys->Joystick()->Shutdown();
  if (sys->Mouse() != nullptr) sys->Mouse()->Shutdown();
  if (sys->Keyboard() != nullptr) sys->Keyboard()->Shutdown();
  if (win32.directInput != nullptr) { win32.directInput->Release(); win32.directInput = nullptr; }
}

void IN_Frame() {
  bool shouldBeActive = in_mouse.GetBool();
  if (win32.mouseReleased || win32.movingWindow || !win32.activeApp) shouldBeActive = false;
  idMouse* inputMouse = sys->Mouse();
  if (inputMouse == nullptr) return;
  if (shouldBeActive != inputMouse->IsActive()) {
    if (inputMouse->IsActive()) inputMouse->Deactivate();
    else if (in_mouse.GetBool()) inputMouse->Activate();
  }
  if (inputMouse->IsActive() && !in_lockMouse.GetBool()) {
    RECT rectangle{};
    GetWindowRect(win32.hWnd, &rectangle);
    SetCursorPos((rectangle.left + rectangle.right) / 2, (rectangle.top + rectangle.bottom) / 2);
  }
}

void Sys_InitInput() {
  if (idLib::headless) return;
  idLib::Printf("\n------- Input Initialization -------\n");
  IN_InitDirectInput();
  if (in_mouse.GetBool()) {
    if (sys->Mouse() != nullptr) { sys->Mouse()->Init(); sys->Mouse()->GrabCursor(false); }
  } else idLib::Printf("Mouse control not active.\n");
  if (sys->Joystick() != nullptr) sys->Joystick()->Init();
  if (sys->Keyboard() != nullptr) sys->Keyboard()->Init();
  idLib::Printf("------------------------------------\n");
  in_mouse.flags &= ~CVAR_MODIFIED;
}
