#pragma once

#include "engine/sys/sys_public.h"
#include "engine/sys/win32/win_local.h"

#include <array>
#include <atomic>
#include <cstdint>
#include <Xinput.h>
#include "shared/idlib/parallelism/thread.h"

class idKeyboardWin32 final : public idKeyboard {
public:
  ~idKeyboardWin32() override { Shutdown(); }
  bool Init() override;
  void Shutdown() override;
  void Deactivate() override;
  int PollInputEvents() override;
  int ReturnInputEvent(unsigned n, unsigned& key, bool& state) override;

private:
  IDirectInputDevice8A* keyboardDevice{};
  DIDEVICEOBJECTDATA polled[256]{};
  std::array<unsigned char, 256> previousKeyState{};
};

class idMouseWin32 final : public idMouse {
public:
  ~idMouseWin32() override { Shutdown(); }
  bool Init() override;
  void Shutdown() override;
  void Activate() override;
  void Deactivate() override;
  bool IsActive() const override { return active; }
  int PollInputEvents() override;
  bool ReturnInputEvent(unsigned n, int& action, int& value) override;
  void GrabCursor(bool grab) override;
  void ShowOSCursor() override;
  idVec2i GetCursorPosition() const override;
  void QueueEvents(int count, bool swapLeftRight);

private:
  IDirectInputDevice8A* mouseDevice{};
  bool active{};
  DIDEVICEOBJECTDATA polled[256]{};
};

extern idKeyboardWin32 keyboard;
extern idMouseWin32 mouse;

class idJoystickPS4Win32 {
public:
  struct OpenParam { std::uint8_t reserved[8]{}; };
  struct AnalogStick { std::uint8_t x{}, y{}; };
  struct AnalogButtons { std::uint8_t l2{}, r2{}, padding[2]{}; };
  struct Quaternion { float x{}, y{}, z{}, w{}; };
  struct Vector3 { float x{}, y{}, z{}; };
  struct Touch { std::uint8_t id{}; std::uint8_t reserved{}; std::uint16_t x{}, y{}; };
  struct TouchData { std::uint8_t touchNum{}; std::uint8_t reserved[7]{}; Touch touch[2]{}; };
  struct Data {
    std::uint32_t buttons{};
    AnalogStick leftStick{};
    AnalogStick rightStick{};
    AnalogButtons analogButtons{};
    Quaternion orientation{};
    Vector3 acceleration{};
    Vector3 angularVelocity{};
    TouchData touchData{};
    bool connected{};
    std::uint8_t alignment[7]{};
    std::uint64_t timestamp{};
  };
  struct VibrationParam { std::uint8_t largeMotor{}, smallMotor{}; };
  struct LightBarParam { std::uint8_t red{}, green{}, blue{}, reserved{}; };

  using InitFn = int (*)();
  using OpenFn = int (*)(int, int, int, const OpenParam*);
  using CloseFn = int (*)(int);
  using ReadStateFn = int (*)(int, Data*);
  using ReadFn = int (*)(int, Data*, int);
  using SetBoolFn = int (*)(int, bool);
  using ResetOrientationFn = int (*)(int);
  using SetVibrationFn = int (*)(int, const VibrationParam*);
  using SetLightBarFn = int (*)(int, const LightBarParam*);

  void Init();
  void Shutdown();
  HMODULE library{};
  InitFn init{};
  OpenFn open{};
  CloseFn close{};
  ReadStateFn readState{};
  ReadFn read{};
  SetBoolFn setMotionSensorState{};
  SetBoolFn setTiltCorrectionState{};
  SetBoolFn setAngularVelocityDeadbandState{};
  ResetOrientationFn resetOrientation{};
  SetVibrationFn setVibration{};
  SetLightBarFn setLightBar{};
};

static_assert(sizeof(idJoystickPS4Win32::Data) == 0x58, "ScePadData ABI drift");

class idJoystickWin32 final : public idJoystick {
public:
  idJoystickWin32();
  ~idJoystickWin32() override { Shutdown(); }
  bool Init() override;
  void Shutdown() override;
  void SetRumble(int inputDevice, int low, int high, int duration = 0,
                 int fadeDuration = 0) override;
  int PollInputEvents(int inputDevice) override;
  int ReturnInputEvent(int n, int& action, int& value) override;
  void PostInputEvent(int inputDevice, int event, int value, int range = 0x4000);

private:
  friend int SampleJoystick360(unsigned deviceNumber);
  friend int SampleJoystickPS4(int deviceNumber);
  friend void JoystickSamplingThread(void* data);
  struct controllerState_t {
    XINPUT_STATE current{};
    XINPUT_STATE previous{};
    unsigned buttonBits{};
    bool valid{};
    bool wasValid{};
    std::uint64_t lastValidCheck{};
    std::array<bool, 320> buttonStates{};
    std::array<int, 6> joyAxis{};
  };
  struct ps4State_t {
    bool valid{};
    int handle{-1};
    std::uint64_t lastValidCheck{};
    unsigned buttonBits{};
    idJoystickPS4Win32::Data current{};
    idJoystickPS4Win32::Data previous{};
    std::array<std::uint8_t, 2> lastTouchId{0xFF, 0xFF};
  };
  struct event_t { int event{}; int value{}; };
  idSysMutex samplingMutex;
  HANDLE timer{};
  HANDLE samplingThread{};
  std::array<controllerState_t, 4> controllers{};
  std::array<ps4State_t, 4> pads{};
  std::array<event_t, 49> events{};
  int numEvents{};
  idJoystickPS4Win32 ps4Api;
  std::atomic<int> running{0};
};

extern idJoystickWin32 joystick;

void IN_InitDirectInput();
void Sys_ShutdownInput();
void IN_Frame();
void Sys_InitInput();
void Sys_ShowHideCursorRenderThread();
int SampleJoystick360(unsigned deviceNumber);
int SampleJoystickPS4(int deviceNumber);
void JoystickSamplingThread(void* data);
