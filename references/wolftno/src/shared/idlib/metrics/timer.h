#pragma once

#include <cstdint>

class idTimer {
public:
  enum timerState_t { TS_STARTED, TS_STOPPED };
  idTimer() : state(TS_STOPPED), start(0), clockTicks(0) {}
  virtual ~idTimer() = default;
  void Start();
  void Stop();
  void Clear();
  std::int64_t ClockTicks() const { return clockTicks; }
  double Milliseconds() const;
  void InitBaseClockTicks();
  static std::int64_t base;
  timerState_t state;
  std::int64_t start;
  std::int64_t clockTicks;
};
static_assert(sizeof(idTimer) == 32, "idTimer ABI drift");
