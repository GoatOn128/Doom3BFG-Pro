#include "engine/sys/win32/win_input.h"

#include "engine/sys/sys_local.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <cstring>

extern idCVar in_joystickButtons;

idKeyboard::~idKeyboard() = default;
idKeyboardWin32 keyboard;

idKeyboard* idSysLocal::Keyboard() { return &keyboard; }

bool idKeyboardWin32::Init() {
  if (win32.directInput == nullptr) {
    idLib::Printf("keyboard: DirectInput has not been started\n");
    return false;
  }
  if (keyboardDevice != nullptr) Shutdown();
  if (FAILED(win32.directInput->CreateDevice(GUID_SysKeyboard, &keyboardDevice, nullptr))) return false;
  if (FAILED(keyboardDevice->SetDataFormat(&c_dfDIKeyboard))) return false;
  if (FAILED(keyboardDevice->SetCooperativeLevel(win32.hWnd,
                                                 DISCL_NONEXCLUSIVE | DISCL_FOREGROUND | DISCL_NOWINKEY)))
    return false;
  DIPROPDWORD property{};
  property.diph.dwSize = sizeof(property);
  property.diph.dwHeaderSize = sizeof(property.diph);
  property.diph.dwHow = DIPH_DEVICE;
  property.dwData = 256;
  if (FAILED(keyboardDevice->SetProperty(DIPROP_BUFFERSIZE, &property.diph))) return false;
  keyboardDevice->Acquire();
  idLib::Printf("keyboard: DirectInput initialized.\n");
  return true;
}

void idKeyboardWin32::Shutdown() {
  Deactivate();
  if (keyboardDevice != nullptr) { keyboardDevice->Release(); keyboardDevice = nullptr; }
}

void idKeyboardWin32::Deactivate() { if (keyboardDevice != nullptr) keyboardDevice->Unacquire(); }

int idKeyboardWin32::PollInputEvents() {
  if (keyboardDevice == nullptr) return 0;
  DWORD count = 256;
  HRESULT result = keyboardDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), polled, &count, 0);
  if (result == DI_OK) {
    keyboardDevice->GetDeviceState(static_cast<DWORD>(previousKeyState.size()), previousKeyState.data());
  } else {
    if (FAILED(keyboardDevice->Acquire())) return 0;
    keyboardDevice->GetDeviceData(sizeof(DIDEVICEOBJECTDATA), polled, &count, 0);
    std::array<unsigned char, 256> current{};
    keyboardDevice->GetDeviceState(static_cast<DWORD>(current.size()), current.data());
    for (unsigned key = 0; key < current.size() && count < 256; ++key) {
      if (current[key] == previousKeyState[key]) continue;
      polled[count].dwOfs = key;
      polled[count].dwData = current[key];
      ++count;
    }
    previousKeyState = current;
  }
  if (count != 0) in_joystickButtons.SetInteger(2);
  return static_cast<int>(count);
}

int idKeyboardWin32::ReturnInputEvent(unsigned n, unsigned& key, bool& state) {
  if (n > 255) return 0;
  key = polled[n].dwOfs;
  state = (polled[n].dwData & 0x80U) != 0;
  if (key == 183 || key == 29 || key == 157 || key == 56 || key == 184)
    sys->QueueEvent(SE_KEY, static_cast<int>(key), state, 0, nullptr, 0);
  return static_cast<int>(key);
}
