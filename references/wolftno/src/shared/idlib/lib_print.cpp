#include "lib.h"

#include "shared/idlib/text/str.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <mutex>

namespace {
struct WarningInfo {
  const char* type;
  const char* name;
};
std::array<WarningInfo, 32> warningInfo{};
int numWarningInfo = 0;
std::mutex printMutex;

const char* SeverityPrefix(int severity) {
  if (severity == PRINT_NOTE)
    return "NOTE: ";
  if (severity == PRINT_WARNING)
    return "WARNING: ";
  if (severity == PRINT_ERROR)
    return "ERROR: ";
  if (severity == PRINT_FATAL)
    return "FATAL: ";
  return "";
}
} // namespace

bool idLib::PushWarningInfo(const char* type, const char* name) {
  if (!IsMainThread() || numWarningInfo >= static_cast<int>(warningInfo.size()))
    return false;
  warningInfo[numWarningInfo++] = {type, name};
  return true;
}
void idLib::PopWarningInfo() {
  if (IsMainThread() && numWarningInfo > 0)
    --numWarningInfo;
}

void idPrintListener::RegisterPrintListener() {
  std::lock_guard<std::mutex> lock(printMutex);
  for (idPrintListener* listener = idLib::printListener; listener; listener = listener->next)
    if (listener == this)
      return;
  next = idLib::printListener;
  idLib::printListener = this;
}
void idPrintListener::UnRegisterPrintListener() {
  std::lock_guard<std::mutex> lock(printMutex);
  idPrintListener** link = &idLib::printListener;
  while (*link && *link != this)
    link = &(*link)->next;
  if (*link == this)
    *link = next;
  next = nullptr;
}

void idLib::VPrintf(int severity, const char* format, std::va_list arguments) {
  char message[16384];
  if (idStr::vsnPrintf(message, sizeof(message), format, arguments) < 0)
    return;
  idStr output(SeverityPrefix(severity));
  if (severity >= PRINT_WARNING && numWarningInfo > 0) {
    const WarningInfo& context = warningInfo[numWarningInfo - 1];
    output.Append(context.type ? context.type : "");
    output.Append(" '");
    output.Append(context.name ? context.name : "");
    output.Append("': ");
  }
  output.Append(message);
  std::lock_guard<std::mutex> lock(printMutex);
  std::FILE* stream = severity >= PRINT_WARNING ? stderr : stdout;
  std::fputs(output.data, stream);
  if (output.IsEmpty() || output[output.Length() - 1] != '\n')
    std::fputc('\n', stream);
  std::fflush(stream);
  for (idPrintListener* listener = printListener; listener; listener = listener->next)
    if (severity >= listener->minSeverity)
      listener->Print(output.data);
}
void idLib::VPrintf(const char* format, std::va_list arguments) {
  VPrintf(PRINT_INFO, format, arguments);
}

#define IDLIB_PRINT_WRAPPER(name, severity)                                                        \
  void idLib::name(const char* format, ...) {                                                      \
    std::va_list args;                                                                             \
    va_start(args, format);                                                                        \
    VPrintf(severity, format, args);                                                               \
    va_end(args);                                                                                  \
  }
IDLIB_PRINT_WRAPPER(Debugf, PRINT_DEBUG)
IDLIB_PRINT_WRAPPER(Printf, PRINT_INFO)
IDLIB_PRINT_WRAPPER(Note, PRINT_NOTE)
IDLIB_PRINT_WRAPPER(Warning, PRINT_WARNING)
#undef IDLIB_PRINT_WRAPPER

void idLib::PrintfIf(bool condition, const char* format, ...) {
  if (!condition)
    return;
  std::va_list args;
  va_start(args, format);
  VPrintf(PRINT_INFO, format, args);
  va_end(args);
}
void idLib::WarningIf(bool condition, const char* format, ...) {
  if (!condition)
    return;
  std::va_list args;
  va_start(args, format);
  VPrintf(PRINT_WARNING, format, args);
  va_end(args);
}
void idLib::ErrorOrWarning(bool error, const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  VPrintf(error ? PRINT_ERROR : PRINT_WARNING, format, args);
  va_end(args);
}
void idLib::PopupWarning(const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  VPrintf(PRINT_WARNING, format, args);
  va_end(args);
}
void idLib::PrintStackToConsole() {}

[[noreturn]] void idLib::Error(const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  VPrintf(PRINT_ERROR, format, args);
  va_end(args);
  std::abort();
}
[[noreturn]] void idLib::FatalError(const char* format, ...) {
  std::va_list args;
  va_start(args, format);
  VPrintf(PRINT_FATAL, format, args);
  va_end(args);
  std::abort();
}
