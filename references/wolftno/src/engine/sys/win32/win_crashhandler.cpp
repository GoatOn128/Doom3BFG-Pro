#include "engine/sys/win32/win_crashhandler.h"

#include "engine/sys/win32/win_local.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/win32_pc/stacktracer.h"

#include <DbgHelp.h>

#include <array>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <fstream>
#include <string>

idCVar com_enableCrashHandler("com_enableCrashHandler", "0", CVAR_BOOL | CVAR_ARCHIVE,
                             "install the unhandled exception filter");
idCVar win_crashDmp_enable("win_crashDmp_enable", "0", CVAR_BOOL | CVAR_ARCHIVE,
                          "write a minidump after an unhandled exception");
idCVar win_crashDmp_path("win_crashDmp_path", "", CVAR_ARCHIVE,
                        "directory in which crash reports are written");
idCVar win_silentCrash("win_silentCrash", "0", CVAR_BOOL | CVAR_ARCHIVE,
                      "suppress the operating-system crash dialog");
idCVar win_terminateOnCrash("win_terminateOnCrash", "0", CVAR_BOOL,
                           "terminate after storing crash data");
idCVar sys_minidumpEnable("sys_minidumpEnable", "1", CVAR_BOOL | CVAR_INIT,
                         "enable manual minidump generation");
idCVar sys_minidumpBigDump("sys_minidumpBigDump", "0", CVAR_BOOL,
                          "include full process memory in minidumps");
idCVar sys_minidumpLocation("sys_minidumpLocation", "\\\\mgfiles01\\builds\\crashdumps\\tungsten", CVAR_ARCHIVE,
                           "manual minidump output directory");

namespace {

idStr callstackList;
idStr nonVolatileCallStack;
idStr localCrashReportPath;
idStr crashReportSteps;
idStr crashReportDetails;
idStr crashReportTitle;

constexpr int IDC_CRASH_STEPS = 501;
constexpr int IDC_CRASH_DETAILS = 502;
constexpr int IDC_CRASH_MESSAGE = 503;
constexpr int IDC_CRASH_OPEN_REPORT = 504;
constexpr int IDC_CRASH_CLOSE = 505;
constexpr int IDC_CRASH_SUBMIT = 506;
constexpr int IDC_CRASH_HIDDEN_A = 508;
constexpr int IDC_CRASH_HIDDEN_B = 509;
constexpr const char* kCrashButtonInitialText = "Enter reproduction steps";
constexpr const char* kCrashButtonText = "Submit Crash Report";

bool ReadLengthPrefixedString(std::ifstream& input, idStr& output) {
  std::int32_t length = 0;
  input.read(reinterpret_cast<char*>(&length), sizeof(length));
  if (!input || length <= 0 || length > 1024 * 1024) {
    output.Empty();
    return input.good();
  }
  std::string bytes(static_cast<std::size_t>(length), '\0');
  input.read(bytes.data(), length);
  if (!input) return false;
  output = bytes.c_str();
  return true;
}

void WriteLengthPrefixedString(std::ofstream& output, const idStr& value) {
  const std::int32_t length = value.Length();
  output.write(reinterpret_cast<const char*>(&length), sizeof(length));
  if (length > 0) output.write(value.data, length);
}

idStr CrashDirectory(bool manualDump) {
  const char* configured = manualDump ? sys_minidumpLocation.GetString()
                                      : win_crashDmp_path.GetString();
  if (configured != nullptr && configured[0] != '\0') {
    CreateDirectoryA(configured, nullptr);
    return idStr(configured);
  }
  char directory[MAX_PATH]{};
  GetCurrentDirectoryA(MAX_PATH, directory);
  return idStr(directory);
}

idStr CrashBaseName(const char* prefix, bool manualDump = false) {
  std::time_t now = std::time(nullptr);
  std::tm local{};
  localtime_s(&local, &now);
  char stamp[64]{};
  std::strftime(stamp, sizeof(stamp), "%Y%m%d-%H%M%S", &local);
  idStr result = CrashDirectory(manualDump);
  if (result.Length() > 0 && result.data[result.Length() - 1] != '\\' &&
      result.data[result.Length() - 1] != '/') {
    result.Append("\\");
  }
  result.Append(prefix != nullptr && prefix[0] != '\0' ? prefix : "WolfNewOrder");
  result.Append("-");
  result.Append(stamp);
  return result;
}

void WriteCrashDump(EXCEPTION_POINTERS* info) {
  idStr path = CrashBaseName("WolfNewOrder-crash", false);
  path.Append(".dmp");
  HANDLE file = CreateFileA(path.data, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    idLib::Warning("Unable to create crash dump '%s' (error %lu)", path.data, GetLastError());
    return;
  }
  if (!idCrashHandler::DumpMiniDump(file, GetCurrentProcess(), info)) {
    idLib::Warning("MiniDumpWriteDump failed for '%s' (error %lu)", path.data, GetLastError());
  }
  CloseHandle(file);
}

}  // namespace

bool idCrashHandler::DumpMiniDump(HANDLE dumpFile, HANDLE process,
                                  EXCEPTION_POINTERS* exceptionInfo) {
  if (dumpFile == nullptr || dumpFile == INVALID_HANDLE_VALUE || process == nullptr) {
    return false;
  }
  MINIDUMP_EXCEPTION_INFORMATION dumpException{};
  dumpException.ThreadId = GetCurrentThreadId();
  dumpException.ExceptionPointers = exceptionInfo;
  dumpException.ClientPointers = FALSE;
  return MiniDumpWriteDump(process, GetProcessId(process), dumpFile, MiniDumpNormal,
                           exceptionInfo != nullptr ? &dumpException : nullptr,
                           nullptr, nullptr) != FALSE;
}

const char* idCrashHandler::GetExceptionDescription(unsigned code) {
  switch (code) {
    case EXCEPTION_ACCESS_VIOLATION: return "Access Violation";
    case EXCEPTION_DATATYPE_MISALIGNMENT: return "Address Misaligned";
    case EXCEPTION_BREAKPOINT: return "Breakpoint";
    case EXCEPTION_SINGLE_STEP: return "Single step";
    case 0xC0000008U: return "Invalid Handle";
    case 0xC0000017U: return "Out of Memory";
    case EXCEPTION_ILLEGAL_INSTRUCTION: return "Illegal Instruction";
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED: return "Array Out of Bounds";
    case EXCEPTION_FLT_DENORMAL_OPERAND: return "Floating Point De-normal Operand";
    case EXCEPTION_FLT_DIVIDE_BY_ZERO: return "Floating Point Divide by Zero";
    case EXCEPTION_FLT_INVALID_OPERATION: return "Invalid Float Operation";
    case EXCEPTION_FLT_OVERFLOW: return "Floating Point Overflow";
    case EXCEPTION_FLT_UNDERFLOW: return "Floating Point Underflow";
    case EXCEPTION_INT_DIVIDE_BY_ZERO: return "Integer Divide by Zero";
    case EXCEPTION_INT_OVERFLOW: return "Integer Overflow";
    case EXCEPTION_PRIV_INSTRUCTION: return "Privileged Instruction";
    case EXCEPTION_STACK_OVERFLOW: return "Stack Overflow";
    default: return "Undefined Exception";
  }
}

idStr idCrashHandler::GetExceptionInfo(EXCEPTION_POINTERS* info) {
  idStr result;
  if (info == nullptr || info->ExceptionRecord == nullptr) {
    result = "No exception record available";
    return result;
  }
  const EXCEPTION_RECORD& record = *info->ExceptionRecord;
  result.Format("Exception 0x%08X (%s) at 0x%p\n",
                record.ExceptionCode, GetExceptionDescription(record.ExceptionCode),
                record.ExceptionAddress);
  if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2) {
    const char* operation = record.ExceptionInformation[0] == 0
                                ? "read"
                                : (record.ExceptionInformation[0] == 1 ? "write" : "execute");
    idStr detail;
    detail.Format("Attempted to %s address 0x%llX\n", operation,
                  static_cast<unsigned long long>(record.ExceptionInformation[1]));
    result.Append(detail.data);
  }
  return result;
}

idStr idCrashHandler::GetRegisterInfo(CONTEXT* context) {
  idStr result;
  if (context == nullptr) {
    result = "No register context available";
    return result;
  }
#if defined(_M_X64)
  result.Format(
      "RAX=%016llX RBX=%016llX RCX=%016llX RDX=%016llX\n"
      "RSI=%016llX RDI=%016llX RBP=%016llX RSP=%016llX\n"
      "R8 =%016llX R9 =%016llX R10=%016llX R11=%016llX\n"
      "R12=%016llX R13=%016llX R14=%016llX R15=%016llX\n"
      "RIP=%016llX EFlags=%08lX\n",
      context->Rax, context->Rbx, context->Rcx, context->Rdx,
      context->Rsi, context->Rdi, context->Rbp, context->Rsp,
      context->R8, context->R9, context->R10, context->R11,
      context->R12, context->R13, context->R14, context->R15,
      context->Rip, context->EFlags);
#else
  result.Format("EAX=%08lX EBX=%08lX ECX=%08lX EDX=%08lX\nEIP=%08lX ESP=%08lX EBP=%08lX\n",
                context->Eax, context->Ebx, context->Ecx, context->Edx,
                context->Eip, context->Esp, context->Ebp);
#endif
  return result;
}

idStr idCrashHandler::GetCallstackInfo(EXCEPTION_POINTERS* info) {
  idStackTrace trace;
  trace.Trace(GetCurrentThread(), info, 0, false, true);
  callstackList = trace.GetFullFunctionDetails();
  return callstackList;
}

idStr idCrashHandler::GetNonVolatileCallstackTop(EXCEPTION_POINTERS* info) {
  idStackTrace trace;
  trace.Trace(GetCurrentThread(), info, 0, false, true);
  nonVolatileCallStack.Clear();
  const int count = trace.stack.depth < 4 ? trace.stack.depth : 4;
  for (int index = 0; index < count; ++index) {
    if (index != 0) nonVolatileCallStack.Append(" <- ");
    nonVolatileCallStack.Append(trace.stack.stack[index].Data());
  }
  return nonVolatileCallStack;
}

INT_PTR CALLBACK idCrashHandler::crashDialogCallback(HWND dialog, UINT message,
                                                     WPARAM wParam, LPARAM lParam) {
  if (message == WM_INITDIALOG) {
    SetWindowLongPtrA(dialog, GWLP_USERDATA, lParam);
    idStr text;
    text.Format("The engine has crashed! Detailed crash information is located in:\n\n\t%s\n\n"
                "Please provide details about what happened. Submission of a bug report is required!",
                localCrashReportPath.data);
    SetDlgItemTextA(dialog, IDC_CRASH_MESSAGE, text.data);
    ShowWindow(GetDlgItem(dialog, IDC_CRASH_CLOSE), SW_HIDE);
    ShowWindow(GetDlgItem(dialog, IDC_CRASH_HIDDEN_A), SW_HIDE);
    ShowWindow(GetDlgItem(dialog, IDC_CRASH_HIDDEN_B), SW_HIDE);
    SetDlgItemTextA(dialog, IDC_CRASH_SUBMIT, kCrashButtonInitialText);

    idStr previousStack;
    idStr previousSteps;
    idStr previousDetails;
    idStr previousTitle;
    std::ifstream previous("LastCrashInfo.txt", std::ios::binary);
    if (previous) {
      ReadLengthPrefixedString(previous, previousStack);
      ReadLengthPrefixedString(previous, previousSteps);
      ReadLengthPrefixedString(previous, previousDetails);
      ReadLengthPrefixedString(previous, previousTitle);
    }
    const bool sameCrash = previousStack.Icmp(nonVolatileCallStack.data) == 0;
    if (sameCrash) {
      crashReportSteps = previousSteps;
      crashReportDetails = previousDetails;
      crashReportTitle = previousTitle;
      SetDlgItemTextA(dialog, IDC_CRASH_STEPS, crashReportSteps.data);
      SetDlgItemTextA(dialog, IDC_CRASH_DETAILS, crashReportDetails.data);
      ShowWindow(GetDlgItem(dialog, IDC_CRASH_CLOSE), SW_SHOW);
    }
    const bool canSubmit = sameCrash && crashReportSteps.Length() >= 20;
    SetDlgItemTextA(dialog, IDC_CRASH_SUBMIT,
                    canSubmit ? kCrashButtonText : kCrashButtonInitialText);
    EnableWindow(GetDlgItem(dialog, IDC_CRASH_SUBMIT), canSubmit);
    return TRUE;
  }
  if (message == WM_COMMAND) {
    const int control = LOWORD(wParam);
    if (control == IDC_CRASH_STEPS && HIWORD(wParam) == EN_CHANGE) {
      char steps[1024]{};
      GetDlgItemTextA(dialog, IDC_CRASH_STEPS, steps, static_cast<int>(sizeof(steps)) - 1);
      const bool canSubmit = std::strlen(steps) >= 20;
      SetDlgItemTextA(dialog, IDC_CRASH_SUBMIT,
                      canSubmit ? kCrashButtonText : kCrashButtonInitialText);
      EnableWindow(GetDlgItem(dialog, IDC_CRASH_SUBMIT), canSubmit);
      return TRUE;
    }
    if (control == IDC_CRASH_OPEN_REPORT) {
      ShellExecuteA(nullptr, nullptr, localCrashReportPath.data, nullptr, nullptr, SW_SHOWNORMAL);
      return TRUE;
    }
    if (control == IDC_CRASH_CLOSE || control == IDCANCEL) {
      EndDialog(dialog, 0);
      return TRUE;
    }
    if (control == IDC_CRASH_SUBMIT || control == IDOK) {
      char buffer[1024]{};
      GetDlgItemTextA(dialog, IDC_CRASH_STEPS, buffer, static_cast<int>(sizeof(buffer)) - 1);
      crashReportSteps = buffer;
      GetDlgItemTextA(dialog, IDC_CRASH_DETAILS, buffer, static_cast<int>(sizeof(buffer)) - 1);
      crashReportDetails = buffer;
      if (crashReportTitle.IsEmpty()) crashReportTitle = "WolfNewOrder crash report";
      std::ofstream previous("LastCrashInfo.txt", std::ios::binary | std::ios::trunc);
      if (previous) {
        WriteLengthPrefixedString(previous, nonVolatileCallStack);
        WriteLengthPrefixedString(previous, crashReportSteps);
        WriteLengthPrefixedString(previous, crashReportDetails);
        WriteLengthPrefixedString(previous, crashReportTitle);
      }
      EndDialog(dialog, 1);
      return TRUE;
    }
  }
  return FALSE;
}

void WinFileWriteLine(HANDLE file, const char* text) {
  if (file == nullptr || file == INVALID_HANDLE_VALUE || text == nullptr) return;
  DWORD written = 0;
  WriteFile(file, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
  WriteFile(file, "\r\n", 2, &written, nullptr);
}

void DoMinidump(const char* prefix, EXCEPTION_POINTERS* info) {
  if (!sys_minidumpEnable.GetBool()) return;
  idStr path = CrashBaseName(prefix, true);
  path.Append(".dmp");
  HANDLE file = CreateFileA(path.data, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS,
                            FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    idLib::Warning("Unable to create minidump '%s' (error %lu)", path.data, GetLastError());
    return;
  }
  MINIDUMP_EXCEPTION_INFORMATION exception{};
  exception.ThreadId = GetCurrentThreadId();
  exception.ExceptionPointers = info;
  exception.ClientPointers = FALSE;
  MINIDUMP_TYPE type = static_cast<MINIDUMP_TYPE>(
      MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory |
      MiniDumpIgnoreInaccessibleMemory);
  if (sys_minidumpBigDump.GetBool()) {
    type = static_cast<MINIDUMP_TYPE>(type | MiniDumpWithPrivateReadWriteMemory);
  }
  if (!MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type,
                         info != nullptr ? &exception : nullptr, nullptr, nullptr)) {
    idLib::Warning("MiniDumpWriteDump failed for '%s' (error %lu)", path.data, GetLastError());
  }
  CloseHandle(file);
}

void idCrashHandler::StoreCrashData(EXCEPTION_POINTERS* info) {
  const idStr base = CrashBaseName("WolfNewOrder-crash");
  idStr reportPath = base;
  reportPath.Append(".txt");
  localCrashReportPath = reportPath;
  GetNonVolatileCallstackTop(info);
  HANDLE report = CreateFileA(reportPath.data, GENERIC_WRITE, FILE_SHARE_READ, nullptr,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (report != INVALID_HANDLE_VALUE) {
    const idStr exception = GetExceptionInfo(info);
    const idStr registers = GetRegisterInfo(info != nullptr ? info->ContextRecord : nullptr);
    const idStr callstack = GetCallstackInfo(info);
    WinFileWriteLine(report, "Wolfenstein: The New Order crash report");
    WinFileWriteLine(report, exception.data);
    WinFileWriteLine(report, registers.data);
    WinFileWriteLine(report, "Call stack:");
    WinFileWriteLine(report, callstack.data);
    CloseHandle(report);
  }
}

LONG WINAPI idCrashHandler::idUnhandledExceptionFilterProc(EXCEPTION_POINTERS* info) {
  if (win_terminateOnCrash.GetBool()) ExitProcess(0);
  if (com_enableCrashHandler.GetBool()) {
    DoMinidump("game", info);
    idLib::Printf("CrashHandler: Storing data and writing local report.\n");
    StoreCrashData(info);
    if (win_crashDmp_enable.GetBool()) WriteCrashDump(info);
    ShowCursor(TRUE);
    const bool silent = win_silentCrash.GetBool();
    if (!silent) {
      DialogBoxParamA(win32.hInstance, MAKEINTRESOURCEA(500), win32.hWnd,
                      crashDialogCallback, 0);
    }
    idLib::Printf("CrashHandler: Complete.\n");
    if (win32.crashHandler != nullptr && win32.crashHandler->originalHandler != nullptr) {
      SetUnhandledExceptionFilter(win32.crashHandler->originalHandler);
      win32.crashHandler->originalHandler = nullptr;
    }
    if (silent) {
      SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX |
                   SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOOPENFILEERRORBOX);
      return EXCEPTION_EXECUTE_HANDLER;
    }
  } else if (win32.crashHandler != nullptr && win32.crashHandler->originalHandler != nullptr) {
    SetUnhandledExceptionFilter(win32.crashHandler->originalHandler);
    win32.crashHandler->originalHandler = nullptr;
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

idCrashHandler::idCrashHandler() {
  originalHandler = SetUnhandledExceptionFilter(idUnhandledExceptionFilterProc);
}

idCrashHandler::~idCrashHandler() {
  if (originalHandler != nullptr) {
    SetUnhandledExceptionFilter(originalHandler);
    originalHandler = nullptr;
  }
}

void SampleTrace(int count, float* values, idStr* text) {
  volatile float accumulator = 0.0F;
  for (int index = 0; index < count; ++index) {
    accumulator += values != nullptr ? values[index] : 0.0F;
  }
  if (text != nullptr && accumulator == -1.0F) {
    text->Append("unreachable");
  }
  idStackTrace trace;
  trace.Trace(GetCurrentThread(), 1, false, true);
}
