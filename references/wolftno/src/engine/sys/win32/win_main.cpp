#include "engine/sys/win32/win_main.h"

#include "engine/sys/sys_local.h"
#include "engine/sys/win32/win_crashhandler.h"
#include "engine/sys/win32/win_cpu.h"
#include "engine/sys/win32/win_input.h"
#include "engine/sys/win32/win_local.h"
#include "engine/sys/win32/win_syscon.h"
#include "engine/sys/win32/win_taskkeyhook.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/win32_pc/stacktracer.h"
#include "shared/idlib/sys/win32_pc/win_files.h"
#include "shared/idlib/parallelism/thread.h"

#include <Psapi.h>
#include <mmsystem.h>

#include <algorithm>
#include <atomic>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cwchar>
#include <string>

idCVar win_notaskkeys("win_notaskkeys", "0", CVAR_INTEGER,
                      "disable Windows task keys: 0=never, 1=fullscreen, 2=always");
idCVar win_viewlog("win_viewlog", "0", CVAR_INTEGER,
                   "show the system console window");
idCVar win_viewLogInHeadLess("win_viewLogInHeadLess", "1", CVAR_BOOL,
                            "show the system console in headless mode");
idCVar win_consoleVisibility("win_consoleVisibility", "1", CVAR_INTEGER,
                            "console visibility override");
idCVar win_allowMultipleInstances("win_allowMultipleInstances", "0", CVAR_ALL,
                                 "allow more than one game process");
idCVar win_floatExceptions("win_floatExceptions", "0", CVAR_BOOL,
                          "enable floating-point exceptions");
idCVar com_pid("com_pid", "0", CVAR_INTEGER | CVAR_INIT,
               "operating-system process identifier");

namespace {

std::atomic_bool requestingPumpRenderThreadEvents{false};
std::atomic_bool renderThreadGotQuitMessage{false};
std::atomic_bool pumpingEvents{false};
std::atomic_bool enteredEventLoop{false};
winMainCallback_t engineMainCallback = nullptr;
winShutdownCallback_t engineShutdownCallback = nullptr;
HANDLE gameMutex = nullptr;

void PumpMessageQueue(bool renderThread) {
  MSG message{};
  while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE)) {
    if (message.message == WM_QUIT) {
      if (renderThread) {
        renderThreadGotQuitMessage = true;
      }
      break;
    }
    TranslateMessage(&message);
    DispatchMessageA(&message);
  }
}

class idMainThread final : public idSysThread {
public:
  HINSTANCE instance{};
  const char* commandLine{};

  int Run() override {
    win32.hInstance = instance;
    idLib::mainThreadID = GetCurrentThreadId();
    Sys_SetCurrentThreadName("Wolfenstein The New Order");
    ULONG heapCompatibility = 2;
    if (!HeapSetInformation(GetProcessHeap(), HeapCompatibilityInformation,
                            &heapCompatibility, sizeof(heapCompatibility)))
      OutputDebugStringA("Set low fragmentation heap failed");
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    _set_invalid_parameter_handler(invalidParameterHandler);

    std::string startupCommandLine = commandLine != nullptr ? commandLine : "";
    std::transform(startupCommandLine.begin(), startupCommandLine.end(), startupCommandLine.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    const bool bypassInstanceCheck =
        startupCommandLine.find("win_allowmultipleinstances") != std::string::npos ||
        startupCommandLine.find("renderbump") != std::string::npos ||
        win_allowMultipleInstances.GetBool();
    if (!bypassInstanceCheck) {
      gameMutex = CreateMutexA(nullptr, FALSE, "Wolfenstein The New Order");
      const DWORD mutexError = GetLastError();
      if (gameMutex == nullptr || mutexError == ERROR_ALREADY_EXISTS || mutexError == ERROR_ACCESS_DENIED) {
        const int answer = MessageBoxA(nullptr,
            "There is another instance of Wolfenstein The New Order running.\n\n"
            "Would you like to terminate the other instance?",
            "Startup Error", MB_ICONERROR | MB_YESNO);
        if (answer != IDYES || !Win_KillOtherInstances()) return 0;
      }
    }

    SetCursor(LoadCursorA(nullptr, IDC_WAIT));
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    timeBeginPeriod(1);
    idStackTracer::Init();
    if (win32.crashHandler == nullptr) {
      win32.crashHandler = new idCrashHandler();
    }
    Sys_CreateConsole();
    Sys_DetectCPU();
    if (win_floatExceptions.GetBool()) Sys_FPU_EnableExceptions(0x3F);
    com_pid.SetInteger(static_cast<int>(GetCurrentProcessId()));
    if (win_notaskkeys.GetInteger() != 0) DisableTaskKeys(1, 0, 0);
    const int result = engineMainCallback != nullptr
                           ? engineMainCallback(commandLine != nullptr ? commandLine : "")
                           : 0;
    DisableTaskKeys(0, 0, 0);
    Sys_ShutdownInput();
    Sys_DestroyConsole();
    delete win32.crashHandler;
    win32.crashHandler = nullptr;
    idStackTracer::Shutdown();
    timeEndPeriod(1);
    CoUninitialize();
    if (gameMutex != nullptr) {
      CloseHandle(gameMutex);
      gameMutex = nullptr;
    }
    return result;
  }
};

}  // namespace

void Win_SetMainCallbacks(winMainCallback_t mainCallback, winShutdownCallback_t shutdownCallback) {
  engineMainCallback = mainCallback;
  engineShutdownCallback = shutdownCallback;
}

bool Sys_IsGameWindowActive() {
  return win32.activeApp;
}

void idSysLocal::PrintStackTrace() {
  idStackTrace trace;
  trace.Trace(GetCurrentThread(), 1, false, true);
  idLib::Printf("---------------- Stack Trace ----------------\n");
  for (int index = 0; index < trace.stack.depth; ++index) {
    char* line = nullptr;
    if (trace.stack.Item(index, &line)) {
      idLib::Printf("%s\n", line);
    }
  }
  idLib::Printf("---------------- End stack trace ----------------\n");
}

void Sys_PumpRenderThreadEvents() {
  if (!idLib::IsMainThread()) {
    PumpMessageQueue(true);
    Sys_ShowHideCursorRenderThread();
  }
}

bool Sys_PumpRenderThreadEventsIfRequested() {
  if (!requestingPumpRenderThreadEvents.exchange(false)) {
    return false;
  }
  Sys_PumpRenderThreadEvents();
  return true;
}

void Sys_RequestRenderThreadEventPump() {
  requestingPumpRenderThreadEvents = true;
}

void Sys_PumpEvents() {
  if (pumpingEvents.exchange(true)) {
    return;
  }
  PumpMessageQueue(false);
  pumpingEvents = false;
}

void idSysLocal::GenerateEvents() {
  if (enteredEventLoop.exchange(true)) {
    return;
  }
  try {
    Sys_PumpEvents();
    IN_Frame();
    const char* consoleInput = Sys_ConsoleInput();
    if (consoleInput != nullptr) {
      const std::size_t length = std::strlen(consoleInput) + 1;
      void* copy = std::malloc(length);
      if (copy != nullptr) {
        std::memcpy(copy, consoleInput, length);
        QueueEvent(SE_CONSOLE, 0, 0, static_cast<int>(length), copy, 0);
      }
    }
  } catch (...) {
    enteredEventLoop = false;
    throw;
  }
  enteredEventLoop = false;
}

bool Win_KillOtherInstances() {
  DWORD processIds[1024]{};
  DWORD bytesNeeded = 0;
  if (!EnumProcesses(processIds, sizeof(processIds), &bytesNeeded)) {
    return false;
  }

  char currentName[MAX_PATH]{};
  GetModuleBaseNameA(GetCurrentProcess(), nullptr, currentName, MAX_PATH);
  bool killedAny = false;
  const DWORD currentId = GetCurrentProcessId();
  for (DWORD index = 0; index < bytesNeeded / sizeof(DWORD); ++index) {
    if (processIds[index] == 0 || processIds[index] == currentId) {
      continue;
    }
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ | PROCESS_TERMINATE,
                                 FALSE, processIds[index]);
    if (process == nullptr) {
      continue;
    }
    char processName[MAX_PATH]{};
    if (GetModuleBaseNameA(process, nullptr, processName, MAX_PATH) != 0 &&
        _stricmp(currentName, processName) == 0 && TerminateProcess(process, 0)) {
      killedAny = true;
    }
    CloseHandle(process);
  }
  return killedAny;
}

BOOL CALLBACK HideThreadWindows(HWND window, LPARAM parameter) {
  (void)parameter;
  if (window == GetAncestor(window, GA_ROOT)) {
    ShowWindow(window, SW_HIDE);
  }
  return TRUE;
}

void invalidParameterHandler(const wchar_t* expression, const wchar_t* function,
                             const wchar_t* file, unsigned line, std::uintptr_t reserved) {
  (void)reserved;
  char expressionText[1024]{}, functionText[1024]{}, fileText[1024]{};
  std::wcstombs(expressionText, expression != nullptr ? expression : L"", sizeof(expressionText) - 1);
  std::wcstombs(functionText, function != nullptr ? function : L"", sizeof(functionText) - 1);
  std::wcstombs(fileText, file != nullptr ? file : L"", sizeof(fileText) - 1);
  idLib::Warning("Invalid parameter in %s. File: %s Line: %u Expression: %s",
                 functionText, fileText, line, expressionText);
}

void Sys_ReLaunch(const char* data, unsigned dataSize) {
  (void)dataSize;
  char commandLine[32768]{};
  std::snprintf(commandLine, sizeof(commandLine), "\"%s\" %s", Sys_EXEPath(), data != nullptr ? data : "");
  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  if (gameMutex != nullptr) {
    CloseHandle(gameMutex);
    gameMutex = nullptr;
  }
  if (!CreateProcessA(nullptr, commandLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr,
                      &startup, &process)) {
    idLib::Error("Could not start process: '%s'", commandLine);
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  if (cmdSystem != nullptr) cmdSystem->AppendCommandText("quit\n");
}

[[noreturn]] void Win_Error(const char* error) {
  if (idLib::headless && engineShutdownCallback != nullptr) {
    engineShutdownCallback(1);
  }
  EnumThreadWindows(GetCurrentThreadId(), HideThreadWindows, 0);
  Win_SetErrorText(error != nullptr ? error : "Unknown error");
  Sys_ShowConsole(1, true);
  timeEndPeriod(1);
  Sys_ShutdownInput();
  Conbuf_AppendText("\n***************************\n");
  Conbuf_AppendText(error != nullptr ? error : "Unknown error");
  Conbuf_AppendText("\n***************************\n");
  MSG message{};
  for (;;) {
    const BOOL result = GetMessageA(&message, nullptr, 0, 0);
    if (result <= 0) {
      if (engineShutdownCallback != nullptr) engineShutdownCallback(1);
      ExitProcess(1);
    }
    TranslateMessage(&message);
    DispatchMessageA(&message);
  }
}

[[noreturn]] void Sys_Quit(UINT exitCode) {
  idStackTracer::Shutdown();
  delete win32.crashHandler;
  win32.crashHandler = nullptr;
  if (engineShutdownCallback != nullptr) {
    engineShutdownCallback(static_cast<int>(exitCode));
  }
  CoUninitialize();
  timeEndPeriod(1);
  Sys_ShutdownInput();
  Sys_DestroyConsole();
  ExitProcess(exitCode);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE, LPSTR commandLine, int) {
  idMainThread mainThread;
  mainThread.instance = instance;
  mainThread.commandLine = commandLine;
  if (!mainThread.StartThread("main thread", CORE_ANY, THREAD_NORMAL, 0x800000)) {
    return 1;
  }
  mainThread.WaitForThread();
  return 0;
}
