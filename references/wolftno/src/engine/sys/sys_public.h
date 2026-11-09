#pragma once

#include <cstdint>
#include "shared/idlib/math/vector.h"

class idCVar;

enum sysEventType_t : std::int32_t {
  SE_NONE = 0,
  SE_KEY = 1,
  SE_CHAR = 2,
  SE_MOUSE = 3,
  SE_MOUSE_ABSOLUTE = 4,
  SE_MOUSE_LEAVE = 5,
  SE_JOYSTICK = 6,
  SE_CONSOLE = 7,
  SE_SPEECH = 8
};

struct sysEvent_t {
  sysEventType_t evType{SE_NONE};
  int evValue{};
  int evValue2{};
  int evPtrLength{};
  void* evPtr{};
  int inputDevice{};
};

enum cpuid_t : std::int32_t {
  CPUID_NONE = 0,
  CPUID_UNSUPPORTED = 1 << 0,
  CPUID_GENERIC = 1 << 1,
  CPUID_INTEL = 1 << 2,
  CPUID_AMD = 1 << 3,
  CPUID_XENON = 1 << 4,
  CPUID_CELL = 1 << 5,
  CPUID_CMOV = 1 << 6,
  CPUID_MMX = 1 << 7,
  CPUID_SSE = 1 << 8,
  CPUID_SSE2 = 1 << 9,
  CPUID_SSE3 = 1 << 10,
  CPUID_SSSE3 = 1 << 11,
  CPUID_SSE41 = 1 << 12,
  CPUID_SSE42 = 1 << 13,
  CPUID_AVX = 1 << 14,
  CPUID_ALTIVEC = 1 << 15,
  CPUID_VMX128 = 1 << 16,
  CPUID_HTT = 1 << 17,
  CPUID_EM64T = 1 << 18
};

class idJoystick {
public:
  virtual ~idJoystick();
  virtual bool Init() { return false; }
  virtual void Shutdown() {}
  virtual void Deactivate() {}
  virtual void SetRumble(int, int, int, int, int) {}
  virtual void SetColor(int, int, int, int) {}
  virtual void ResetColor(int) {}
  virtual int PollInputEvents(int) { return 0; }
  virtual int ReturnInputEvent(int, int&, int&) { return 0; }
  virtual void EndInputEvents() {}
};
class idKeyboard {
public:
  virtual ~idKeyboard();
  virtual bool Init() { return false; }
  virtual void Shutdown() {}
  virtual void Deactivate() {}
  virtual int PollInputEvents() { return 0; }
  virtual int ReturnInputEvent(unsigned, unsigned&, bool&) { return 0; }
  virtual void EndInputEvents() {}
};
class idMouse {
public:
  virtual ~idMouse();
  virtual bool Init() { return false; }
  virtual void Shutdown() {}
  virtual void Activate() {}
  virtual void Deactivate() {}
  virtual bool IsActive() const { return false; }
  virtual int PollInputEvents() { return 0; }
  virtual bool ReturnInputEvent(unsigned, int&, int&) { return false; }
  virtual void EndInputEvents() {}
  virtual void GrabCursor(bool) {}
  virtual void ShowOSCursor() {}
  virtual idVec2i GetCursorPosition() const { return {}; }
};

class idSys {
public:
  virtual ~idSys() = default;
  virtual std::int64_t GetClockTicks() = 0;
  virtual std::int64_t ClockTicksPerSecond() = 0;
  virtual cpuid_t GetProcessorId() = 0;
  virtual const char* GetProcessorString() = 0;
  virtual const char* FPU_GetState() = 0;
  virtual bool FPU_StackIsEmpty() = 0;
  virtual void FPU_SetFTZ(bool enable) = 0;
  virtual void FPU_SetDAZ(bool enable) = 0;
  virtual void FPU_EnableExceptions(int exceptions) = 0;
  virtual std::uintptr_t DLL_Load(const char* name) = 0;
  virtual void* DLL_GetProcAddress(std::uintptr_t handle, const char* name) = 0;
  virtual void DLL_Unload(std::uintptr_t handle) = 0;
  virtual void DLL_GetFileName(const char* baseName, char* output, int outputSize) = 0;
  virtual void OpenURL(const char*, bool) {}
  virtual void StartProcess(const char*, bool) {}
  virtual char* GetClipboardData() = 0;
  virtual void SetClipboardData(const char* data) = 0;
  virtual int Milliseconds() = 0;
  virtual unsigned VblCount() { return 0; }
  virtual void PrintStackTrace() {}
  virtual idJoystick* Joystick() { return nullptr; }
  virtual idKeyboard* Keyboard() { return nullptr; }
  virtual idMouse* Mouse() { return nullptr; }
  virtual const char* GetOSUserName() = 0;
  virtual const char* GetMachineName() = 0;
  virtual void QueueEvent(sysEventType_t type, int value, int value2, int pointerLength,
                          void* pointer, int inputDevice) = 0;
  virtual void GenerateEvents() {}
  virtual sysEvent_t GetEvent() = 0;
  virtual void ClearEvents() = 0;
};

extern idSys* sys;

// NEEDS_WORK(engine/sys): the definition is owned by the system-language
// translation unit, not renderer/rendersystem_bink.cpp.
extern idCVar sys_lang;
