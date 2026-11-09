#include "engine/sys/win32/win_input.h"

#include "engine/sys/sys_local.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <chrono>
#include <cstring>

idCVar in_joystickButtons(
    "in_joystickButtons", "2", CVAR_INTEGER,
    "Which sets of icons to use, depends on which controller was used");
// Retail defines this in engine/framework/usercmdgen.cpp. Keep the fallback
// here until that owner is recovered so the Win32 library remains linkable.
idCVar in_joystick("in_joystick", "1", CVAR_BOOL | CVAR_ARCHIVE, "enables joystick input");

idJoystick::~idJoystick() = default;
idJoystickWin32 joystick;

idJoystick* idSysLocal::Joystick() { return &joystick; }

void idJoystickPS4Win32::Init() {
  Shutdown();
  library = LoadLibraryA("libscepad_x64.dll");
  if (library == nullptr) return;
  init = reinterpret_cast<InitFn>(GetProcAddress(library, "scePadInit"));
  open = reinterpret_cast<OpenFn>(GetProcAddress(library, "scePadOpen"));
  close = reinterpret_cast<CloseFn>(GetProcAddress(library, "scePadClose"));
  readState = reinterpret_cast<ReadStateFn>(GetProcAddress(library, "scePadReadState"));
  read = reinterpret_cast<ReadFn>(GetProcAddress(library, "scePadRead"));
  setMotionSensorState = reinterpret_cast<SetBoolFn>(GetProcAddress(library, "scePadSetMotionSensorState"));
  setTiltCorrectionState = reinterpret_cast<SetBoolFn>(GetProcAddress(library, "scePadSetTiltCorrectionState"));
  setAngularVelocityDeadbandState = reinterpret_cast<SetBoolFn>(
      GetProcAddress(library, "scePadSetAngularVelocityDeadbandState"));
  resetOrientation = reinterpret_cast<ResetOrientationFn>(GetProcAddress(library, "scePadResetOrientation"));
  setVibration = reinterpret_cast<SetVibrationFn>(GetProcAddress(library, "scePadSetVibration"));
  setLightBar = reinterpret_cast<SetLightBarFn>(GetProcAddress(library, "scePadSetLightBar"));
  if (init == nullptr || open == nullptr || close == nullptr || readState == nullptr || read == nullptr ||
      setMotionSensorState == nullptr || setTiltCorrectionState == nullptr ||
      setAngularVelocityDeadbandState == nullptr || resetOrientation == nullptr ||
      setVibration == nullptr || setLightBar == nullptr || init() != 0) {
    Shutdown();
  }
}

void idJoystickPS4Win32::Shutdown() {
  if (library != nullptr) FreeLibrary(library);
  library = nullptr;
  init = nullptr;
  open = nullptr;
  close = nullptr;
  readState = nullptr;
  read = nullptr;
  setMotionSensorState = nullptr;
  setTiltCorrectionState = nullptr;
  setAngularVelocityDeadbandState = nullptr;
  resetOrientation = nullptr;
  setVibration = nullptr;
  setLightBar = nullptr;
}

idJoystickWin32::idJoystickWin32() = default;

bool idJoystickWin32::Init() {
  Shutdown();
  ps4Api.Init();
  for (auto& controller : controllers) controller = {};
  for (auto& pad : pads) pad = {};
  for (auto& pad : pads) {
    pad.handle = -1;
    pad.lastTouchId = {0xFF, 0xFF};
  }

  timer = CreateWaitableTimerA(nullptr, FALSE, "JoypadTimer");
  if (timer == nullptr) idLib::FatalError("CreateWaitableTimer for joystick failed");
  LARGE_INTEGER dueTime{};
  dueTime.QuadPart = -1;
  if (!SetWaitableTimer(timer, &dueTime, 4, nullptr, nullptr, FALSE))
    idLib::FatalError("SetWaitableTimer for joystick failed");

  running.store(1, std::memory_order_release);
  samplingThread = CreateThread(nullptr, 0,
      [](LPVOID parameter) -> DWORD {
        JoystickSamplingThread(parameter);
        return 0;
      }, this, 0, nullptr);
  if (samplingThread == nullptr) {
    running.store(0, std::memory_order_release);
    CloseHandle(timer);
    timer = nullptr;
    idLib::FatalError("CreateThread for joystick failed");
  }
  return false;
}

void idJoystickWin32::Shutdown() {
  if (running.exchange(0, std::memory_order_acq_rel) != 0 && timer != nullptr) {
    LARGE_INTEGER dueTime{};
    dueTime.QuadPart = -1;
    SetWaitableTimer(timer, &dueTime, 0, nullptr, nullptr, FALSE);
  }
  if (samplingThread != nullptr) {
    WaitForSingleObject(samplingThread, INFINITE);
    CloseHandle(samplingThread);
    samplingThread = nullptr;
  }
  if (timer != nullptr) {
    CancelWaitableTimer(timer);
    CloseHandle(timer);
    timer = nullptr;
  }
  for (unsigned device = 0; device < controllers.size(); ++device)
    SetRumble(static_cast<int>(device), 0, 0, 0, 0);
  if (ps4Api.close != nullptr) {
    for (auto& pad : pads) {
      if (pad.handle >= 0) ps4Api.close(pad.handle);
      pad.handle = -1;
      pad.valid = false;
    }
  }
  ps4Api.Shutdown();
}

int SampleJoystick360(unsigned deviceNumber) {
  if (deviceNumber >= 4) return 0;
  auto& controller = joystick.controllers[deviceNumber];
  if (!controller.valid) return 0;
  XINPUT_STATE state{};
  if (XInputGetState(deviceNumber, &state) != ERROR_SUCCESS) {
    controller.current = {};
    controller.valid = false;
    return 0;
  }
  const int packetDelta = static_cast<int>(state.dwPacketNumber - controller.current.dwPacketNumber);
  if (packetDelta > 0) {
    controller.current = state;
    controller.buttonBits |= state.Gamepad.wButtons;
  }
  return packetDelta;
}

int SampleJoystickPS4(int deviceNumber) {
  if (deviceNumber < 0 || deviceNumber >= 4 || joystick.ps4Api.readState == nullptr) return 0;
  auto& pad = joystick.pads[deviceNumber];
  if (!pad.valid) return 0;
  idJoystickPS4Win32::Data state{};
  if (joystick.ps4Api.readState(pad.handle, &state) != 0) {
    pad.current = {};
    pad.valid = false;
    return 0;
  }
  const int timestampDelta = static_cast<int>(state.timestamp - pad.current.timestamp);
  if (timestampDelta != 0) {
    pad.current = state;
    pad.buttonBits |= state.buttons;
  }
  return timestampDelta;
}

void JoystickSamplingThread(void* data) {
  auto* instance = static_cast<idJoystickWin32*>(data);
  if (instance == nullptr) instance = &joystick;
  while (instance->running.load(std::memory_order_acquire) != 0) {
    const std::uint64_t now = GetTickCount64() * 1000ULL;
    for (unsigned device = 0; device < instance->controllers.size(); ++device) {
      auto& controller = instance->controllers[device];
      if (!controller.valid && now > controller.lastValidCheck + 5000000ULL) {
        controller.lastValidCheck = now;
        XINPUT_STATE state{};
        if (XInputGetState(device, &state) == ERROR_SUCCESS) {
          controller.valid = true;
          controller.wasValid = true;
        }
      }
    }
    if (instance->ps4Api.library != nullptr) {
      for (unsigned device = 0; device < instance->pads.size(); ++device) {
        auto& pad = instance->pads[device];
        if (!pad.valid) {
          if (pad.handle < 0) pad.handle = instance->ps4Api.open(static_cast<int>(device + 1), 0, 0, nullptr);
          if (now > pad.lastValidCheck + 5000000ULL) {
            pad.lastValidCheck = now;
            idJoystickPS4Win32::Data state{};
            if (pad.handle >= 0 && instance->ps4Api.readState(pad.handle, &state) == 0)
              pad.valid = state.connected;
          }
        }
      }
    }
    instance->samplingMutex.Lock();
    for (unsigned device = 0; device < 4; ++device) {
      SampleJoystick360(device);
      SampleJoystickPS4(static_cast<int>(device));
    }
    instance->samplingMutex.Unlock();
    if (instance->timer != nullptr) WaitForSingleObject(instance->timer, INFINITE);
  }
  instance->running.store(-1, std::memory_order_release);
}

void idJoystickWin32::SetRumble(int inputDevice, int low, int high, int, int) {
  if (!in_joystick.GetBool() || inputDevice < 0 || inputDevice >= static_cast<int>(controllers.size())) return;
  if (in_joystickButtons.GetInteger() == 2) low = high = 0;
  if (controllers[inputDevice].valid) {
    XINPUT_VIBRATION vibration{};
    vibration.wLeftMotorSpeed = static_cast<WORD>(std::clamp(low, 0, 65535));
    vibration.wRightMotorSpeed = static_cast<WORD>(std::clamp(high, 0, 65535));
    const DWORD result = XInputSetState(static_cast<DWORD>(inputDevice), &vibration);
    if (result != ERROR_SUCCESS) idLib::Warning("XInputSetState error: 0x%x", result);
  }
  if (pads[inputDevice].valid && ps4Api.setVibration != nullptr) {
    idJoystickPS4Win32::VibrationParam vibration{};
    vibration.largeMotor = static_cast<std::uint8_t>(std::clamp(low, 0, 65535) >> 8);
    vibration.smallMotor = static_cast<std::uint8_t>(std::clamp(high, 0, 65535) >> 8);
    ps4Api.setVibration(pads[inputDevice].handle, &vibration);
  }
}

void idJoystickWin32::PostInputEvent(int inputDevice, int event, int value, int range) {
  if (inputDevice < 0 || inputDevice >= static_cast<int>(controllers.size()) || numEvents >= static_cast<int>(events.size()))
    return;
  controllerState_t& controller = controllers[inputDevice];
  auto pushButton = [&](int key, bool state) {
    if (key < 0 || key >= static_cast<int>(controller.buttonStates.size()) ||
        controller.buttonStates[key] == state) return;
    controller.buttonStates[key] = state;
    sys->QueueEvent(SE_KEY, key, state, 0, nullptr, inputDevice);
  };
  if (event >= 0 && event <= 31) pushButton(256 + event, value != 0);
  else if (event == 32) { pushButton(274, value < -range); pushButton(275, value > range); }
  else if (event == 33) { pushButton(272, value < -range); pushButton(273, value > range); }
  else if (event == 34) { pushButton(278, value < -range); pushButton(279, value > range); }
  else if (event == 35) { pushButton(276, value < -range); pushButton(277, value > range); }
  else if (event == 36) pushButton(280, value > range);
  else if (event == 37) pushButton(281, value > range);
  else if (event >= 44 && event <= 47) pushButton(282 + event - 44, value != 0);
  if (event >= 32 && event <= 37) {
    const int axis = event - 32;
    const int percentage = range != 0 ? 16 * value / range : 0;
    if (controller.joyAxis[axis] != percentage) {
      controller.joyAxis[axis] = percentage;
      sys->QueueEvent(SE_JOYSTICK, axis, percentage, 0, nullptr, inputDevice);
    }
  }
  events[numEvents++] = {event, value};
  win32.monitorPoweredOff = false;
}

int idJoystickWin32::ReturnInputEvent(int n, int& action, int& value) {
  if (n < 0 || n > 48) return 0;
  action = events[static_cast<std::size_t>(n)].event;
  value = events[static_cast<std::size_t>(n)].value;
  return 1;
}

int idJoystickWin32::PollInputEvents(int inputDevice) {
  numEvents = 0;
  if (!in_joystick.GetBool() || inputDevice < 0 || inputDevice >= static_cast<int>(controllers.size())) return 0;
  controllerState_t& controller = controllers[inputDevice];

  XINPUT_STATE state{};
  XINPUT_STATE old{};
  bool xinputValid = false;
  unsigned sampledButtons = 0;
  samplingMutex.Lock();
  SampleJoystick360(static_cast<unsigned>(inputDevice));
  xinputValid = controller.valid;
  if (xinputValid) {
    state = controller.current;
    old = controller.previous;
    controller.previous = controller.current;
    sampledButtons = controller.buttonBits;
    controller.buttonBits = 0;
  }
  samplingMutex.Unlock();

  if (xinputValid) {
    controller.wasValid = true;
    if (sampledButtons != 0 || std::abs(static_cast<int>(state.Gamepad.sThumbLY)) > 10000 ||
        std::abs(static_cast<int>(state.Gamepad.sThumbRY)) > 10000) in_joystickButtons.SetInteger(0);
    static constexpr int remap[16] = {44, 45, 46, 47, 8, 9, 6, 7, 4, 5, 0, 0, 0, 1, 2, 3};
    for (int bit = 0; bit < 16; ++bit) {
      const unsigned mask = 1U << bit;
      if ((state.Gamepad.wButtons & mask) != (old.Gamepad.wButtons & mask))
        PostInputEvent(inputDevice, remap[bit], (state.Gamepad.wButtons & mask) != 0);
    }
    if (state.Gamepad.bLeftTrigger != old.Gamepad.bLeftTrigger)
      PostInputEvent(inputDevice, 36, state.Gamepad.bLeftTrigger << 7);
    if (state.Gamepad.bRightTrigger != old.Gamepad.bRightTrigger)
      PostInputEvent(inputDevice, 37, state.Gamepad.bRightTrigger << 7);
    if (state.Gamepad.sThumbLX != old.Gamepad.sThumbLX)
      PostInputEvent(inputDevice, 32, state.Gamepad.sThumbLX);
    if (state.Gamepad.sThumbLY != old.Gamepad.sThumbLY)
      PostInputEvent(inputDevice, 33, -state.Gamepad.sThumbLY);
    if (state.Gamepad.sThumbRX != old.Gamepad.sThumbRX)
      PostInputEvent(inputDevice, 34, state.Gamepad.sThumbRX);
    if (state.Gamepad.sThumbRY != old.Gamepad.sThumbRY)
      PostInputEvent(inputDevice, 35, -state.Gamepad.sThumbRY);
  } else if (controller.wasValid) {
    controller.wasValid = false;
    PostInputEvent(inputDevice, 48, 0);
  }

  ps4State_t& pad = pads[inputDevice];
  idJoystickPS4Win32::Data padState{};
  idJoystickPS4Win32::Data oldPadState{};
  bool ps4Valid = false;
  unsigned ps4Buttons = 0;
  samplingMutex.Lock();
  SampleJoystickPS4(inputDevice);
  ps4Valid = pad.valid;
  if (ps4Valid) {
    padState = pad.current;
    oldPadState = pad.previous;
    pad.previous = pad.current;
    ps4Buttons = pad.buttonBits;
    pad.buttonBits = 0;
  }
  samplingMutex.Unlock();

  if (ps4Valid) {
    if (ps4Buttons != 0) in_joystickButtons.SetInteger(4);
    struct buttonRemap_t { unsigned mask; int event; };
    static constexpr buttonRemap_t remap[] = {
        {0x4000, 0}, {0x2000, 1}, {0x8000, 2}, {0x1000, 3},
        {0x0400, 4}, {0x0800, 5}, {0x0002, 6}, {0x0004, 7},
        {0x0008, 8}, {0x100000, 9}, {0x0010, 44}, {0x0040, 45},
        {0x0080, 46}, {0x0020, 47}};
    const unsigned changed = padState.buttons ^ oldPadState.buttons;
    for (const auto& mapping : remap) {
      if ((changed & mapping.mask) != 0)
        PostInputEvent(inputDevice, mapping.event, (padState.buttons & mapping.mask) != 0);
    }
    const auto stickValue = [](std::uint8_t value) { return (value | (value << 8)) - 0x7FFF; };
    const auto triggerValue = [](std::uint8_t value) { return (value | (value << 8)) >> 1; };
    if (padState.leftStick.x != oldPadState.leftStick.x)
      PostInputEvent(inputDevice, 32, stickValue(padState.leftStick.x));
    if (padState.leftStick.y != oldPadState.leftStick.y)
      PostInputEvent(inputDevice, 33, stickValue(padState.leftStick.y));
    if (padState.rightStick.x != oldPadState.rightStick.x)
      PostInputEvent(inputDevice, 34, stickValue(padState.rightStick.x));
    if (padState.rightStick.y != oldPadState.rightStick.y)
      PostInputEvent(inputDevice, 35, stickValue(padState.rightStick.y));
    if (padState.analogButtons.l2 != oldPadState.analogButtons.l2)
      PostInputEvent(inputDevice, 36, triggerValue(padState.analogButtons.l2));
    if (padState.analogButtons.r2 != oldPadState.analogButtons.r2)
      PostInputEvent(inputDevice, 37, triggerValue(padState.analogButtons.r2));

    const auto touchPresent = [&](std::uint8_t id) {
      for (unsigned i = 0; i < padState.touchData.touchNum && i < 2; ++i)
        if (padState.touchData.touch[i].id == id) return true;
      return false;
    };
    if (pad.lastTouchId[0] != 0xFF && !touchPresent(pad.lastTouchId[0])) {
      PostInputEvent(inputDevice, 40, 0);
      pad.lastTouchId[0] = 0xFF;
    }
    if (pad.lastTouchId[1] != 0xFF && !touchPresent(pad.lastTouchId[1])) {
      PostInputEvent(inputDevice, 43, 0);
      pad.lastTouchId[1] = 0xFF;
    }
    const auto touchAxis = [](std::uint16_t value, int maximum) {
      return 65534 * static_cast<int>(value) / maximum - 0x7FFF;
    };
    for (unsigned slot = 0; slot < padState.touchData.touchNum && slot < 2; ++slot) {
      const auto& touch = padState.touchData.touch[slot];
      unsigned logicalSlot = slot;
      if (pad.lastTouchId[1] == touch.id) logicalSlot = 1;
      else if (pad.lastTouchId[0] == touch.id) logicalSlot = 0;
      else if (pad.lastTouchId[0] != 0xFF && pad.lastTouchId[1] == 0xFF) logicalSlot = 1;
      PostInputEvent(inputDevice, logicalSlot == 0 ? 38 : 41, touchAxis(touch.x, 1919));
      PostInputEvent(inputDevice, logicalSlot == 0 ? 39 : 42, touchAxis(touch.y, 941));
      pad.lastTouchId[logicalSlot] = touch.id;
    }
  }
  return numEvents;
}
