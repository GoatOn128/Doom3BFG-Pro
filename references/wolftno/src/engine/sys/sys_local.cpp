#include "engine/sys/sys_local.h"

#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/sys/win32_pc/win_utils.h"
#include "shared/idlib/text/str.h"
#include "engine/sys/win32/win_cpu.h"
#include "engine/sys/win32/win_misc.h"

#include <array>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <windows.h>
#include <xmmintrin.h>

namespace {
constexpr unsigned kEventQueueSize = 256;
std::array<sysEvent_t, kEventQueueSize> eventQueue{};
unsigned eventHead = 0;
unsigned eventTail = 0;
std::mutex eventQueueMutex;
}

idSysLocal sysLocal;
idSys* sys = &sysLocal;

std::int64_t idSysLocal::GetClockTicks() { return Sys_GetClockTicks(); }
std::int64_t idSysLocal::ClockTicksPerSecond() { return Sys_ClockTicksPerSecond(); }

cpuid_t idSysLocal::GetProcessorId() {
  return Sys_GetProcessorId();
}

const char* idSysLocal::GetProcessorString() { return Sys_GetProcessorString(); }

const char* idSysLocal::FPU_GetState() { return Sys_FPU_GetState(); }

bool idSysLocal::FPU_StackIsEmpty() { return true; }

void idSysLocal::FPU_SetFTZ(bool enable) {
  Sys_FPU_SetFTZ(enable);
}

void idSysLocal::FPU_SetDAZ(bool enable) {
  Sys_FPU_SetDAZ(enable);
}

void idSysLocal::FPU_EnableExceptions(int exceptions) {
  Sys_FPU_EnableExceptions(exceptions);
}

std::uintptr_t idSysLocal::DLL_Load(const char* name) {
  return reinterpret_cast<std::uintptr_t>(Sys_DLL_Load(name));
}

void* idSysLocal::DLL_GetProcAddress(std::uintptr_t handle, const char* name) {
  return reinterpret_cast<void*>(Sys_DLL_GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
}

void idSysLocal::DLL_Unload(std::uintptr_t handle) {
  Sys_DLL_Unload(reinterpret_cast<HMODULE>(handle));
}

void idSysLocal::DLL_GetFileName(const char* baseName, char* output, int outputSize) {
  idStr::snPrintf(output, outputSize, "%sx64.dll", baseName != nullptr ? baseName : "");
}

char* idSysLocal::GetClipboardData() { return Sys_GetClipboardData(); }

void idSysLocal::SetClipboardData(const char* data) { Sys_SetClipboardData(data); }

int idSysLocal::Milliseconds() { return static_cast<int>(Sys_Milliseconds()); }
const char* idSysLocal::GetOSUserName() { return Sys_GetOSUserName(); }
const char* idSysLocal::GetMachineName() { return Sys_GetMachineName(); }

void idSysLocal::QueueEvent(sysEventType_t type, int value, int value2, int pointerLength,
                            void* pointer, int inputDevice) {
  std::lock_guard<std::mutex> lock(eventQueueMutex);
  if (eventHead - eventTail >= kEventQueueSize) {
    sysEvent_t& discarded = eventQueue[eventTail & (kEventQueueSize - 1)];
    std::free(discarded.evPtr);
    ++eventTail;
  }
  sysEvent_t& event = eventQueue[eventHead & (kEventQueueSize - 1)];
  ++eventHead;
  event.evType = type;
  event.evValue = value;
  event.evValue2 = value2;
  event.evPtrLength = pointerLength;
  event.evPtr = pointer;
  event.inputDevice = inputDevice;
}

sysEvent_t idSysLocal::GetEvent() {
  std::lock_guard<std::mutex> lock(eventQueueMutex);
  if (eventTail == eventHead) {
    return {};
  }
  return eventQueue[eventTail++ & (kEventQueueSize - 1)];
}

void idSysLocal::ClearEvents() {
  std::lock_guard<std::mutex> lock(eventQueueMutex);
  eventHead = 0;
  eventTail = 0;
}
