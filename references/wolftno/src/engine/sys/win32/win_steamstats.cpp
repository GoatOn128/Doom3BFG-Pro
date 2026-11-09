#include "engine/sys/win32/win_steamstats.h"

#include "engine/sys/win32/win_achievements.h"
#include "shared/idlib/lib.h"

namespace {
constexpr int kResultOK = 1;
constexpr int kResultInvalidParam = 8;
}

idSteamStats::idSteamStats() = default;
idSteamStats::~idSteamStats() = default;

void idSteamStats::Pump() {
  idSteamUserStats* stats = Sys_GetSteamUserStats();
  if (state == STATE_INIT && stats != nullptr && stats->RequestCurrentStats()) {
    state = STATE_BUSY_FIRST_UPDATE;
  }
}

bool idSteamStats::Store(bool fromPump) {
  idSteamUserStats* stats = Sys_GetSteamUserStats();
  if (state != STATE_NORMAL || stats == nullptr) {
    if (!fromPump) idLib::Warning("[idSteamStats::Store] Can't store stats: bad state: %d", state);
    return false;
  }
  if (!stats->StoreStats()) {
    return false;
  }
  state = STATE_BUSY_STORING;
  return true;
}

void idSteamStats::OnStatsStored(UserStatsStored_t* result) {
  if (result != nullptr && result->result == kResultOK) {
    state = STATE_NORMAL;
    return;
  }
  if (result != nullptr && state == STATE_BUSY_STORING &&
      result->result == kResultInvalidParam) {
    idLib::Warning("[idSteamStats::OnStatsStored] failed (%d), retrying", result->result);
    state = STATE_BUSY_STORING_RETRY;
    return;
  }
  idLib::Warning("[idSteamStats::OnStatsStored] unrecoverable failure (%d)",
                 result != nullptr ? result->result : -1);
  state = STATE_FATAL_ERROR;
  error = ERROR_STORE_FAIL_UNRECOVERABLE;
}

void idSteamStats::OnStatsReceived(UserStatsReceived_t* result) {
  if (result != nullptr && result->result == kResultOK) {
    if (state == STATE_BUSY_FIRST_UPDATE) {
      state = STATE_NORMAL;
      error = ERROR_NONE;
    }
  } else {
    idLib::Warning("[idSteamStats::OnStatsReceived] failed (%d)",
                   result != nullptr ? result->result : -1);
  }
}

template <typename T>
void idSteamStats::Set(int stat, T value) {
  if (state != STATE_NORMAL && state != STATE_BUSY_STORING) {
    idLib::Warning("[idSteamStats::Set] Can't set stats: bad state: %d", state);
    return;
  }
  idSteamUserStats* stats = Sys_GetSteamUserStats();
  if (stats == nullptr) return;
  steamStatStringBuffer.Format("%i", stat + 1);
  stats->SetStat(steamStatStringBuffer.data, static_cast<int>(value));
}

template void idSteamStats::Set<int>(int stat, int value);
template void idSteamStats::Set<unsigned>(int stat, unsigned value);

void idSteamStats::SetInt(int stat, unsigned value) {
  Set<unsigned>(stat, value);
}
