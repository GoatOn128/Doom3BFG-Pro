#include "shared/idlib/lib.h"

#include <Windows.h>
#include <mutex>
#include <set>
#include <utility>

bool AssertFailed(const char* file, int line, const char* expression, bool inlineBreak) {
  static std::mutex mutex;
  static std::set<std::pair<std::string, int>> reported;
  const std::pair<std::string, int> location(file ? file : "", line);
  {
    std::lock_guard<std::mutex> lock(mutex);
    if (!reported.insert(location).second)
      return false;
  }
  idLib::Warning("ASSERTION FAILED! %s(%d): '%s'", file ? file : "", line,
                 expression ? expression : "");
  const bool debugger = IsDebuggerPresent() != FALSE;
  if (debugger && inlineBreak)
    DebugBreak();
  return debugger;
}
