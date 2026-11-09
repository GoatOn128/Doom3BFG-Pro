#include "shared/idlib/parallelism/thread.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <process.h>

#include <algorithm>
#include <utility>

namespace {
int WinPriority(xthreadPriority priority) {
  static constexpr int values[] = {THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_BELOW_NORMAL,
                                   THREAD_PRIORITY_NORMAL, THREAD_PRIORITY_ABOVE_NORMAL,
                                   THREAD_PRIORITY_HIGHEST};
  return values[std::clamp(static_cast<int>(priority), 0, 4)];
}

unsigned __stdcall ThreadEntry(void* context) {
  auto* pair = static_cast<std::pair<unsigned (*)(void*), void*>*>(context);
  const auto function = pair->first;
  void* parameters = pair->second;
  delete pair;
  return function(parameters);
}
} // namespace

std::uint64_t Sys_GetCurrentThreadPlaformPriority() {
  return static_cast<std::uint64_t>(GetThreadPriority(GetCurrentThread()));
}

void Sys_SetCurrentThreadPlaformPriority(std::uint64_t priority) {
  SetThreadPriority(GetCurrentThread(), static_cast<int>(priority));
}

std::uint64_t Sys_CalculatePlatformPriority(xthreadPriority priority) {
  return static_cast<std::uint64_t>(WinPriority(priority));
}

std::uint64_t Sys_GetCurrentThreadPlaformAffinity() {
  GROUP_AFFINITY affinity{};
  if (GetThreadGroupAffinity(GetCurrentThread(), &affinity))
    return affinity.Mask;
  return 0;
}

void Sys_SetCurrentThreadPlaformAffinity(std::uint64_t mask) {
  if (mask != 0)
    SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(mask));
}

std::int64_t Sys_CalculatePlatformAffinity(core_t affinity) {
  if (affinity == CORE_ANY)
    return -1;
  const int bit = std::clamp(static_cast<int>(affinity), 0, 62);
  return std::int64_t{1} << bit;
}

void Sys_SetThreadName(std::uint32_t threadId, const char* name) {
  HANDLE thread = threadId == GetCurrentThreadId()
                      ? GetCurrentThread()
                      : OpenThread(THREAD_SET_LIMITED_INFORMATION, FALSE, threadId);
  if (!thread)
    return;
  wchar_t wide[128]{};
  MultiByteToWideChar(CP_UTF8, 0, name ? name : "", -1, wide, static_cast<int>(std::size(wide)));
  SetThreadDescription(thread, wide);
  if (thread != GetCurrentThread())
    CloseHandle(thread);
}

void Sys_SetCurrentThreadName(const char* name) {
  Sys_SetThreadName(GetCurrentThreadId(), name);
}

std::uint64_t Sys_GetCurrentThreadID() {
  return GetCurrentThreadId();
}

void Sys_JoinThread(void* threadHandle) {
  if (!threadHandle)
    return;
  WaitForSingleObject(static_cast<HANDLE>(threadHandle), INFINITE);
  CloseHandle(static_cast<HANDLE>(threadHandle));
}

bool Sys_Yield() {
  return SwitchToThread() != FALSE;
}

void* Sys_CreateThread(unsigned (*function)(void*), void* parameters, xthreadPriority priority,
                       const char* name, core_t core, int stackSize, unsigned suspended) {
  auto* context = new std::pair<unsigned (*)(void*), void*>(function, parameters);
  unsigned id = 0;
  const unsigned flags = suspended ? CREATE_SUSPENDED : 0;
  const uintptr_t handle =
      _beginthreadex(nullptr, stackSize > 0 ? stackSize : 0, ThreadEntry, context, flags, &id);
  if (!handle) {
    delete context;
    return nullptr;
  }
  SetThreadPriority(reinterpret_cast<HANDLE>(handle), WinPriority(priority));
  if (core != CORE_ANY) {
    SetThreadAffinityMask(reinterpret_cast<HANDLE>(handle),
                          static_cast<DWORD_PTR>(Sys_CalculatePlatformAffinity(core)));
  }
  Sys_SetThreadName(id, name);
  return reinterpret_cast<void*>(handle);
}

void Sys_SignalCreate(void** handle, bool manualReset) {
  *handle = CreateEventW(nullptr, manualReset ? TRUE : FALSE, FALSE, nullptr);
}
void Sys_SignalDestroy(void** handle) {
  if (handle && *handle) {
    CloseHandle(*handle);
    *handle = nullptr;
  }
}
void Sys_SignalRaise(void** handle) {
  if (handle && *handle)
    SetEvent(*handle);
}
void Sys_SignalClear(void** handle) {
  if (handle && *handle)
    ResetEvent(*handle);
}
bool Sys_SignalWait(void** handle, std::uint32_t timeout) {
  return handle && *handle && WaitForSingleObject(*handle, timeout) == WAIT_OBJECT_0;
}
void Sys_MutexCreate(void** handle) {
  auto* section = new CRITICAL_SECTION;
  InitializeCriticalSection(section);
  *handle = section;
}
void Sys_MutexDestroy(void** handle) {
  if (handle && *handle) {
    auto* section = static_cast<CRITICAL_SECTION*>(*handle);
    DeleteCriticalSection(section);
    delete section;
    *handle = nullptr;
  }
}
bool Sys_MutexLock(void** handle, bool blocking) {
  if (!handle || !*handle)
    return false;
  auto* section = static_cast<CRITICAL_SECTION*>(*handle);
  if (blocking) {
    EnterCriticalSection(section);
    return true;
  }
  return TryEnterCriticalSection(section) != FALSE;
}
void Sys_MutexUnlock(void** handle) {
  if (handle && *handle)
    LeaveCriticalSection(static_cast<CRITICAL_SECTION*>(*handle));
}
std::int64_t Sys_InterlockedIncrement(int* v) {
  return InterlockedIncrement(reinterpret_cast<volatile LONG*>(v));
}
std::int64_t Sys_InterlockedDecrement(int* v) {
  return InterlockedDecrement(reinterpret_cast<volatile LONG*>(v));
}
std::int64_t Sys_InterlockedAdd(int* v, unsigned i) {
  return InterlockedExchangeAdd(reinterpret_cast<volatile LONG*>(v), static_cast<LONG>(i));
}
std::int64_t Sys_InterlockedExchange(int* v, int e) {
  return InterlockedExchange(reinterpret_cast<volatile LONG*>(v), e);
}
std::int64_t Sys_InterlockedCompareExchange(int* v, int c, int e) {
  return InterlockedCompareExchange(reinterpret_cast<volatile LONG*>(v), e, c);
}
std::int64_t Sys_InterlockedExchangePointer(void** p, std::int64_t e) {
  return reinterpret_cast<std::int64_t>(InterlockedExchangePointer(p, reinterpret_cast<void*>(e)));
}
std::int64_t Sys_InterlockedCompareExchangePointer(void** p, std::int64_t c, std::int64_t e) {
  return reinterpret_cast<std::int64_t>(
      InterlockedCompareExchangePointer(p, reinterpret_cast<void*>(e), reinterpret_cast<void*>(c)));
}

idSysMutex::idSysMutex() {
  Sys_MutexCreate(&handle);
}
idSysMutex::~idSysMutex() {
  Sys_MutexDestroy(&handle);
}
bool idSysMutex::Lock(bool b) {
  return Sys_MutexLock(&handle, b);
}
void idSysMutex::Unlock() {
  Sys_MutexUnlock(&handle);
}
idSysSignal::idSysSignal(bool m) {
  Sys_SignalCreate(&handle, m);
}
idSysSignal::~idSysSignal() {
  Sys_SignalDestroy(&handle);
}
void idSysSignal::Raise() {
  Sys_SignalRaise(&handle);
}
void idSysSignal::Clear() {
  Sys_SignalClear(&handle);
}
bool idSysSignal::Wait(std::uint32_t t) {
  return Sys_SignalWait(&handle, t);
}
