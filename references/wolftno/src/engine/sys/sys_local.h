#pragma once

#include "engine/sys/sys_public.h"

class idSysLocal final : public idSys {
public:
  std::int64_t GetClockTicks() override;
  std::int64_t ClockTicksPerSecond() override;
  cpuid_t GetProcessorId() override;
  const char* GetProcessorString() override;
  const char* FPU_GetState() override;
  bool FPU_StackIsEmpty() override;
  void FPU_SetFTZ(bool enable) override;
  void FPU_SetDAZ(bool enable) override;
  void FPU_EnableExceptions(int exceptions) override;
  std::uintptr_t DLL_Load(const char* name) override;
  void* DLL_GetProcAddress(std::uintptr_t handle, const char* name) override;
  void DLL_Unload(std::uintptr_t handle) override;
  void DLL_GetFileName(const char* baseName, char* output, int outputSize) override;
  void OpenURL(const char* url, bool doExit) override;
  void StartProcess(const char* executablePath, bool doExit) override;
  char* GetClipboardData() override;
  void SetClipboardData(const char* data) override;
  int Milliseconds() override;
  unsigned VblCount() override;
  void PrintStackTrace() override;
  void GenerateEvents() override;
  idKeyboard* Keyboard() override;
  idMouse* Mouse() override;
  idJoystick* Joystick() override;
  const char* GetOSUserName() override;
  const char* GetMachineName() override;
  void QueueEvent(sysEventType_t type, int value, int value2, int pointerLength,
                  void* pointer, int inputDevice) override;
  sysEvent_t GetEvent() override;
  void ClearEvents() override;
};

extern idSysLocal sysLocal;
