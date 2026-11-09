#pragma once

#include "shared/idlib/text/str.h"

struct UserStatsStored_t {
  int result{};
};

struct UserStatsReceived_t {
  int result{};
};

class idSteamStats {
public:
  enum state_t {
    STATE_INIT = 1,
    STATE_BUSY_FIRST_UPDATE,
    STATE_NORMAL,
    STATE_BUSY_STORING,
    STATE_BUSY_STORING_RETRY,
    STATE_FATAL_ERROR
  };
  enum error_t { ERROR_NONE = 0, ERROR_STORE_FAIL_UNRECOVERABLE = 1 };

  idSteamStats();
  ~idSteamStats();
  void Pump();
  bool Store(bool fromPump = false);
  void OnStatsStored(UserStatsStored_t* result);
  void OnStatsReceived(UserStatsReceived_t* result);
  template <typename T> void Set(int stat, T value);
  void SetInt(int stat, unsigned value);

  state_t GetState() const { return state; }
  error_t GetError() const { return error; }

private:
  state_t state{STATE_INIT};
  error_t error{ERROR_NONE};
  idStr steamStatStringBuffer;
};
