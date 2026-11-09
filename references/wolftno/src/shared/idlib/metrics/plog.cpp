#include "shared/idlib/metrics/recoverymetrics.h"
#include "shared/idlib/sys/sys_time.h"
#include <algorithm>
#include <map>
idPLog::idPLog() = default;
idPLog::~idPLog() {
  Free();
}
void idPLog::Free() {
  entries.clear();
  entries.shrink_to_fit();
}
void idPLog::Clear() {
  entries.clear();
}
void idPLog::EnsureEntries(std::size_t n) {
  entries.reserve(n);
}
std::size_t idPLog::Begin(const char* n, int d) {
  if (!enabled)
    return static_cast<std::size_t>(-1);
  entries.push_back({idStr(n ? n : ""), static_cast<std::uint64_t>(Sys_Microseconds()), 0, d});
  return entries.size() - 1;
}
void idPLog::End(std::size_t i) {
  if (i < entries.size())
    entries[i].ticks = static_cast<std::uint64_t>(Sys_Microseconds()) - entries[i].start;
}
std::uint64_t idPLog::GetOverHeadTicks() const {
  const auto a = Sys_Microseconds(), b = Sys_Microseconds();
  return static_cast<std::uint64_t>(b - a);
}
idStr idPLog::ShowUniqueEntries() const {
  std::map<std::string, std::pair<unsigned, std::uint64_t>> totals;
  for (const auto& e : entries) {
    auto& t = totals[e.name.data];
    ++t.first;
    t.second += e.ticks;
  }
  idStr out;
  char line[512];
  for (const auto& t : totals) {
    idStr::snPrintf(line, sizeof(line), "%s: %u calls, %llu us\n", t.first.c_str(), t.second.first,
                    static_cast<unsigned long long>(t.second.second));
    out.Append(line);
  }
  return out;
}
idStr idPLog::BuildLogData() const {
  idStr out;
  char line[512];
  for (const auto& e : entries) {
    idStr::snPrintf(line, sizeof(line), "%d,%s,%llu\n", e.depth, e.name.data,
                    static_cast<unsigned long long>(e.ticks));
    out.Append(line);
  }
  return out;
}
idStr idPLog::ShowCallGraph() const {
  idStr out;
  char line[128];
  for (const auto& e : entries) {
    for (int i = 0; i < e.depth; ++i)
      out.Append("  ");
    out.Append(e.name);
    idStr::snPrintf(line, sizeof(line), " (%llu us)\n", static_cast<unsigned long long>(e.ticks));
    out.Append(line);
  }
  return out;
}
