#pragma once

#include <cstdint>
#include <cstdarg>

enum printSeverity_t {
  PRINT_DEBUG,
  PRINT_INFO,
  PRINT_NOTE,
  PRINT_WARNING,
  PRINT_ERROR,
  PRINT_FATAL
};

class idPrintListener {
public:
  idPrintListener(bool color = false, bool safe = false, printSeverity_t minimum = PRINT_DEBUG)
      : next(nullptr), wantColor(color), threadSafe(safe), minSeverity(minimum) {}
  virtual ~idPrintListener() { UnRegisterPrintListener(); }
  virtual void Print(const char* text) = 0;
  void RegisterPrintListener();
  void UnRegisterPrintListener();
  idPrintListener* next;
  bool wantColor;
  bool threadSafe;
  printSeverity_t minSeverity;
};

class idFileSystem;
class idSourceControl;

extern idFileSystem* fileSystem;

struct idLib {
  static std::uint32_t mainThreadID;
  static int production;
  static int frameNumber;
  static bool headless;
  static bool dontTrack;
  static idPrintListener* printListener;
  static idSourceControl* sourceControl;

  static bool IsMainThread();
  static int AvoidLinkErrorGetProduction();
  static void Init();
  static void ShutDown();
  static bool PushWarningInfo(const char* type, const char* name);
  static void PopWarningInfo();
  static void VPrintf(int severity, const char* format, std::va_list arguments);
  static void VPrintf(const char* format, std::va_list arguments);
  static void Debugf(const char* format, ...);
  static void Printf(const char* format, ...);
  static void PrintfIf(bool condition, const char* format, ...);
  static void Note(const char* format, ...);
  static void Warning(const char* format, ...);
  static void WarningIf(bool condition, const char* format, ...);
  static void ErrorOrWarning(bool error, const char* format, ...);
  static void PopupWarning(const char* format, ...);
  static void PrintStackToConsole();
#if !defined(WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI)
  [[noreturn]]
#endif
  static void Error(const char* format, ...);
  [[noreturn]] static void FatalError(const char* format, ...);
};

class idScopedDontTrack {
public:
  idScopedDontTrack();
  ~idScopedDontTrack();

  bool oldDontTrack;
};
