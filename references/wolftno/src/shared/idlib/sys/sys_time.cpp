#include "sys_time.h"

#include <Windows.h>
#include <algorithm>
#include <ctime>

namespace {
const char* sysLanguageNames[] = {"english", "french", "italian", "german", "spanish",
                                  "russian", "polish", "czech",   nullptr};
thread_local char timeString[128];
} // namespace

std::uint64_t Sys_CurrentSystemTime() {
  FILETIME time;
  GetSystemTimeAsFileTime(&time);
  return (static_cast<std::uint64_t>(time.dwHighDateTime) << 32) | time.dwLowDateTime;
}

std::uint64_t Sys_TimeSinceUnixEpoch() {
  constexpr std::uint64_t epochDifferenceMilliseconds = 11644473600000ull;
  return Sys_CurrentSystemTime() / 10000ull - epochDifferenceMilliseconds;
}

int Sys_NumLanguageIds() {
  return 8;
}
const char* Sys_GetLanguageName(unsigned int id) {
  return id < static_cast<unsigned int>(Sys_NumLanguageIds()) ? sysLanguageNames[id] : "english";
}
int Sys_GetLanguageId(const char* language) {
  for (int index = 0; index < Sys_NumLanguageIds(); ++index)
    if (idStr::Icmp(sysLanguageNames[index], language) == 0)
      return index;
  return 0;
}

char* Sys_TimeStampToLogFormat(int timeStamp) {
  if (timeStamp == -1) {
    timeString[0] = '\0';
    return timeString;
  }
  const std::time_t value = timeStamp;
  std::tm local{};
  localtime_s(&local, &value);
  idStr::snPrintf(timeString, sizeof(timeString), "%04d-%02d-%02d %02d:%02d:%02d",
                  local.tm_year + 1900, local.tm_mon + 1, local.tm_mday, local.tm_hour,
                  local.tm_min, local.tm_sec);
  return timeString;
}

char* Sys_TimeStampToStr(int timeStamp, bool padded) {
  if (timeStamp == -1) {
    timeString[0] = '\0';
    return timeString;
  }
  const std::time_t value = timeStamp;
  std::tm local{};
  localtime_s(&local, &value);
  const char* format = padded ? "%02d/%02d/%04d %02d:%02d:%02d" : "%d/%d/%d %d:%02d:%02d";
  idStr::snPrintf(timeString, sizeof(timeString), format, local.tm_mon + 1, local.tm_mday,
                  local.tm_year + 1900, local.tm_hour, local.tm_min, local.tm_sec);
  return timeString;
}

char* Sys_DateStr(bool padded) {
  return Sys_TimeStampToStr(static_cast<int>(std::time(nullptr)), padded);
}

idStr Sys_SecToStr(int seconds) {
  idStr result;
  const int weeks = seconds / 604800;
  if (weeks > 0) {
    result += weeks;
    result += "w, ";
    seconds %= 604800;
  }
  const int days = seconds / 86400;
  if (weeks > 0 || days > 0) {
    result += days;
    result += "d, ";
    seconds %= 86400;
  }
  result.Append(idStr(seconds / 3600));
  idStr tail;
  tail.Format(":%02d:%02d", (seconds % 3600) / 60, seconds % 60);
  result.Append(tail);
  return result;
}
