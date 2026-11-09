#pragma once

#include "shared/idlib/text/str.h"

#include <Windows.h>

class idCrashHandler {
public:
  idCrashHandler();
  ~idCrashHandler();

  static bool DumpMiniDump(HANDLE dumpFile, HANDLE process, EXCEPTION_POINTERS* exceptionInfo);
  static const char* GetExceptionDescription(unsigned exceptionCode);
  static idStr GetExceptionInfo(EXCEPTION_POINTERS* exceptionInfo);
  static idStr GetRegisterInfo(CONTEXT* context);
  static idStr GetCallstackInfo(EXCEPTION_POINTERS* exceptionInfo);
  static idStr GetNonVolatileCallstackTop(EXCEPTION_POINTERS* exceptionInfo);
  static INT_PTR CALLBACK crashDialogCallback(HWND dialog, UINT message,
                                               WPARAM wParam, LPARAM lParam);
  static void StoreCrashData(EXCEPTION_POINTERS* exceptionInfo);
  static LONG WINAPI idUnhandledExceptionFilterProc(EXCEPTION_POINTERS* exceptionInfo);

  LPTOP_LEVEL_EXCEPTION_FILTER originalHandler{};
  bool defaultMessage{true};
};

void WinFileWriteLine(HANDLE file, const char* text);
void DoMinidump(const char* prefix, EXCEPTION_POINTERS* exceptionInfo);
void SampleTrace(int count, float* values, idStr* text);
