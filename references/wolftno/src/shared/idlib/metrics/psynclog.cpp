#include "shared/idlib/metrics/recoverymetrics.h"
#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/sys/sys_time.h"
idPSyncLog::idPSyncLog() = default;
idPSyncLog::~idPSyncLog() = default;
void idPSyncLog::Add(const char* l) {
  std::lock_guard<std::mutex> lock(mutex);
  events.push_back({static_cast<std::uint64_t>(Sys_Microseconds()),
                    static_cast<std::uint32_t>(Sys_GetCurrentThreadID()), idStr(l ? l : "")});
}
void idPSyncLog::Clear() {
  std::lock_guard<std::mutex> lock(mutex);
  events.clear();
}
std::vector<idPSyncLog::sync_t> idPSyncLog::Snapshot() const {
  std::lock_guard<std::mutex> lock(mutex);
  return events;
}
