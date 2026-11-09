#include "shared/idlib/sys/sys_time.h"

#include <Windows.h>
#include <atomic>

namespace {
std::atomic<std::int64_t> clockTicksAtStartup{0};
std::atomic<std::int64_t> clockTicksPerSecond{0};
std::atomic<std::int64_t> cpuCyclesPerSecond{0};
void EnsureInitialized() {
  if (clockTicksPerSecond.load() == 0)
    Sys_InitClockTicks();
}
} // namespace

void Sys_Sleep(unsigned long milliseconds) {
  ::Sleep(milliseconds);
}

void Sys_InitClockTicks() {
  LARGE_INTEGER frequency{}, counter{};
  QueryPerformanceFrequency(&frequency);
  QueryPerformanceCounter(&counter);
  clockTicksPerSecond = frequency.QuadPart;
  cpuCyclesPerSecond = frequency.QuadPart;
  clockTicksAtStartup = counter.QuadPart;
}

std::int64_t Sys_Milliseconds() {
  static const ULONGLONG start = GetTickCount64();
  return static_cast<std::int64_t>(GetTickCount64() - start);
}
std::int64_t Sys_GetClockTicks() {
  EnsureInitialized();
  LARGE_INTEGER counter{};
  QueryPerformanceCounter(&counter);
  return counter.QuadPart - clockTicksAtStartup.load();
}
std::int64_t Sys_ClockTicksPerSecond() {
  EnsureInitialized();
  return clockTicksPerSecond.load();
}
std::int64_t Sys_ClockCyclesPerSecond() {
  EnsureInitialized();
  return cpuCyclesPerSecond.load();
}
std::uint64_t Sys_Microseconds() {
  EnsureInitialized();
  return static_cast<std::uint64_t>(Sys_GetClockTicks()) * 1000000ull /
         static_cast<std::uint64_t>(clockTicksPerSecond.load());
}
