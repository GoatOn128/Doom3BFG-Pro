#include "timer.h"

#include "shared/idlib/sys/sys_time.h"

#include <algorithm>

std::int64_t idTimer::base = 0;
void idTimer::Start() {
  if (state != TS_STARTED) {
    state = TS_STARTED;
    start = Sys_GetClockTicks();
  }
}
void idTimer::Stop() {
  if (state == TS_STARTED) {
    clockTicks += std::max<std::int64_t>(0, Sys_GetClockTicks() - start - base);
    state = TS_STOPPED;
  }
}
void idTimer::Clear() {
  state = TS_STOPPED;
  start = 0;
  clockTicks = 0;
}
double idTimer::Milliseconds() const {
  return static_cast<double>(clockTicks) * 1000.0 / static_cast<double>(Sys_ClockTicksPerSecond());
}
void idTimer::InitBaseClockTicks() {
  base = 0;
  std::int64_t minimum = -1;
  for (int index = 0; index < 1000; ++index) {
    idTimer timer;
    timer.Start();
    timer.Stop();
    if (minimum < 0 || timer.clockTicks < minimum)
      minimum = timer.clockTicks;
  }
  base = std::max<std::int64_t>(0, minimum);
}
