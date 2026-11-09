#pragma once

#include "shared/idlib/text/str.h"

#include <cstdint>

enum core_t : int { CORE_ANY = -1, CORE_0A = 0, CORE_0B, CORE_1A, CORE_1B, CORE_2A, CORE_2B };

enum xthreadPriority : int {
  THREAD_LOWEST = 0,
  THREAD_BELOW_NORMAL,
  THREAD_NORMAL,
  THREAD_ABOVE_NORMAL,
  THREAD_HIGHEST
};

class idSysMutex {
public:
  void* handle{};
  idSysMutex();
  ~idSysMutex();
  bool Lock(bool blocking = true);
  void Unlock();
};

class idSysSignal {
public:
  void* handle{};
  explicit idSysSignal(bool manualReset = false);
  ~idSysSignal();
  void Raise();
  void Clear();
  bool Wait(std::uint32_t timeout = 0xFFFFFFFFu);
};

class idScopedCriticalSection {
public:
  explicit idScopedCriticalSection(idSysMutex& criticalMutex)
      : mutex(&criticalMutex) {
    mutex->Lock(true);
  }
  ~idScopedCriticalSection() { mutex->Unlock(); }

  idScopedCriticalSection(const idScopedCriticalSection&) = delete;
  idScopedCriticalSection& operator=(const idScopedCriticalSection&) = delete;

  idSysMutex* mutex;
};

std::uint64_t Sys_GetCurrentThreadPlaformPriority();
void Sys_SetCurrentThreadPlaformPriority(std::uint64_t priority);
std::uint64_t Sys_CalculatePlatformPriority(xthreadPriority priority);
std::uint64_t Sys_GetCurrentThreadPlaformAffinity();
void Sys_SetCurrentThreadPlaformAffinity(std::uint64_t mask);
std::int64_t Sys_CalculatePlatformAffinity(core_t affinity);
void Sys_SetThreadName(std::uint32_t threadId, const char* name);
void Sys_SetCurrentThreadName(const char* name);
std::uint64_t Sys_GetCurrentThreadID();
void Sys_JoinThread(void* threadHandle);
bool Sys_Yield();
void* Sys_CreateThread(unsigned (*function)(void*), void* parameters, xthreadPriority priority,
                       const char* name, core_t core, int stackSize, unsigned suspended = 0);

void Sys_SignalCreate(void** handle, bool manualReset);
void Sys_SignalDestroy(void** handle);
void Sys_SignalRaise(void** handle);
void Sys_SignalClear(void** handle);
bool Sys_SignalWait(void** handle, std::uint32_t timeout);
void Sys_MutexCreate(void** handle);
void Sys_MutexDestroy(void** handle);
bool Sys_MutexLock(void** handle, bool blocking);
void Sys_MutexUnlock(void** handle);
std::int64_t Sys_InterlockedIncrement(int* value);
std::int64_t Sys_InterlockedDecrement(int* value);
std::int64_t Sys_InterlockedAdd(int* value, unsigned amount);
std::int64_t Sys_InterlockedExchange(int* value, int exchange);
std::int64_t Sys_InterlockedCompareExchange(int* value, int comparand, int exchange);
std::int64_t Sys_InterlockedExchangePointer(void** pointer, std::int64_t exchange);
std::int64_t Sys_InterlockedCompareExchangePointer(void** pointer, std::int64_t comparand,
                                                   std::int64_t exchange);

class idSysThread {
public:
  idSysThread();
  virtual ~idSysThread();
  virtual int Run() = 0;

  bool StartThread(const char* threadName, core_t core = CORE_ANY,
                   xthreadPriority priority = THREAD_NORMAL, int stackSize = 0);
  bool StartWorkerThread(const char* threadName, core_t core = CORE_ANY,
                         xthreadPriority priority = THREAD_NORMAL, int stackSize = 0);
  void StopThread(bool wait = true);
  void WaitForThread();
  void SignalWork();
  static unsigned ThreadProc(void* thread);

  idStr name;
  std::uint64_t threadHandle{};
  bool isWorker{};
  bool isRunning{};
  volatile bool isTerminating{};
  volatile bool moreWorkToDo{};
  idSysSignal signalWorkerDone;
  idSysSignal signalMoreWorkToDo;
  idSysMutex signalMutex;
};

class idScopeThreadPriority {
public:
  explicit idScopeThreadPriority(xthreadPriority priority)
      : oldPrio(Sys_GetCurrentThreadPlaformPriority()) {
    Sys_SetCurrentThreadPlaformPriority(
        Sys_CalculatePlatformPriority(priority));
  }
  ~idScopeThreadPriority() noexcept(false) {
    Sys_SetCurrentThreadPlaformPriority(oldPrio);
  }

  std::uint64_t oldPrio;
};

class idScopeThreadAffinity {
public:
  explicit idScopeThreadAffinity(core_t affinity)
      : oldAffinity(Sys_GetCurrentThreadPlaformAffinity()) {
    Sys_SetCurrentThreadPlaformAffinity(
        static_cast<std::uint64_t>(Sys_CalculatePlatformAffinity(affinity)));
  }
  ~idScopeThreadAffinity() noexcept(false);

  std::uint64_t oldAffinity;
};

class idScopeThreadPriorityAndAffinity {
public:
  idScopeThreadPriorityAndAffinity(core_t affinity, xthreadPriority priority)
      : affinty(affinity), prio(priority) {}
  ~idScopeThreadPriorityAndAffinity() noexcept(false);

  idScopeThreadAffinity affinty;
  idScopeThreadPriority prio;
};

static_assert(sizeof(idSysMutex) == 8, "mutex ABI drift");
static_assert(sizeof(idSysSignal) == 8, "signal ABI drift");
static_assert(sizeof(idSysThread) == 0x58, "thread ABI drift");
static_assert(sizeof(idScopedCriticalSection) == 8,
              "critical-section scope ABI drift");
static_assert(sizeof(idScopeThreadPriority) == 8,
              "thread-priority scope ABI drift");
static_assert(sizeof(idScopeThreadAffinity) == 8,
              "thread-affinity scope ABI drift");
static_assert(sizeof(idScopeThreadPriorityAndAffinity) == 16,
              "combined thread scope ABI drift");
