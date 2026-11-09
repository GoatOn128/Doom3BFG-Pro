#include "engine/sys/win32/win_misc.h"
#include "engine/sys/win32/win_local.h"

#include "engine/sys/sys_local.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/text/str.h"

#include <ShlObj.h>
#include <shellapi.h>

#include <cstdlib>
#include <cstring>

namespace {
char savePath[MAX_PATH]{};
char cachePath[MAX_PATH]{};
bool doExitSpamGuard = false;
soundCaptureStart_t captureStart = nullptr;
soundCaptureStop_t captureStop = nullptr;
}

win32State_t win32{};

unsigned idSysLocal::VblCount() { return static_cast<unsigned>(Sys_Milliseconds() / 16); }

HMODULE Sys_DLL_Load(const char* dllName) {
  if (dllName == nullptr) return nullptr;
  HMODULE module = LoadLibraryA(dllName);
  if (module != nullptr) {
    char loadedPath[MAX_PATH]{};
    GetModuleFileNameA(module, loadedPath, MAX_PATH - 1);
    char requestedPath[MAX_PATH]{};
    if (GetFullPathNameA(dllName, MAX_PATH, requestedPath, nullptr) != 0 &&
        idStr::IcmpPath(requestedPath, loadedPath) != 0) {
      FreeLibrary(module);
      idLib::Error("LoadLibrary '%s' wants to load '%s'", dllName, loadedPath);
    }
  }
  return module;
}

FARPROC Sys_DLL_GetProcAddress(HMODULE module, const char* procedureName) {
  return GetProcAddress(module, procedureName);
}

void Sys_DLL_Unload(HMODULE module) {
  if (module == nullptr || FreeLibrary(module)) return;
  const DWORD code = GetLastError();
  char* message = nullptr;
  FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
                     FORMAT_MESSAGE_IGNORE_INSERTS,
                 nullptr, code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                 reinterpret_cast<char*>(&message), 0, nullptr);
  idStr errorText(message != nullptr ? message : "");
  if (message != nullptr) LocalFree(message);
  idLib::Error("Sys_DLL_Unload: FreeLibrary failed - %s (%lu)", errorText.data, code);
}

const char* Sys_DefaultDevStoragePath() {
  savePath[0] = '\0';
  PWSTR knownPath = nullptr;
  if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_SavedGames,
                                     static_cast<KNOWN_FOLDER_FLAG>(0x8800),
                                     nullptr, &knownPath))) {
    if (WideCharToMultiByte(CP_ACP, 0, knownPath, -1, savePath,
                            static_cast<int>(sizeof(savePath)), nullptr, nullptr) == 0) {
      savePath[0] = '\0';
    }
    CoTaskMemFree(knownPath);
  }
  if (savePath[0] == '\0') {
    SHGetFolderPathA(nullptr, CSIDL_MYDOCUMENTS | CSIDL_FLAG_CREATE, nullptr,
                     SHGFP_TYPE_CURRENT, savePath);
    idStr::Append(savePath, sizeof(savePath), "\\My Games");
  }
  idStr::Append(savePath, sizeof(savePath), "\\MachineGames\\Wolfenstein The New Order");
  return savePath;
}

const char* Sys_DefaultCachePath() {
  cachePath[0] = '\0';
  if (FAILED(SHGetFolderPathA(nullptr, CSIDL_LOCAL_APPDATA | CSIDL_FLAG_CREATE, nullptr,
                              SHGFP_TYPE_CURRENT, cachePath))) {
    cachePath[0] = '\0';
  }
  idStr::Append(cachePath, sizeof(cachePath), "\\MachineGames\\Wolfenstein The New Order");
  return cachePath;
}

char* Sys_GetClipboardData() {
  if (!OpenClipboard(nullptr)) return nullptr;
  HANDLE object = GetClipboardData(CF_TEXT);
  char* result = nullptr;
  if (object != nullptr) {
    const char* source = static_cast<const char*>(GlobalLock(object));
    if (source != nullptr) {
      const std::size_t length = std::strlen(source) + 1;
      result = static_cast<char*>(std::malloc(length));
      if (result != nullptr) std::memcpy(result, source, length);
      GlobalUnlock(object);
    }
  }
  CloseClipboard();
  return result;
}

void Sys_SetClipboardData(const char* text) {
  if (text == nullptr) return;
  const std::size_t length = std::strlen(text) + 1;
  HGLOBAL object = GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, length);
  if (object == nullptr) return;
  void* destination = GlobalLock(object);
  if (destination == nullptr) { GlobalFree(object); return; }
  std::memcpy(destination, text, length);
  GlobalUnlock(object);
  if (!OpenClipboard(nullptr)) { GlobalFree(object); return; }
  EmptyClipboard();
  if (SetClipboardData(CF_TEXT, object) == nullptr) GlobalFree(object);
  CloseClipboard();
}

void idSysLocal::OpenURL(const char* url, bool doExit) {
  if (doExitSpamGuard) {
    idLib::Printf("OpenURL: already in an exit sequence, ignoring %s\n", url);
    return;
  }
  idLib::Printf("Open URL: %s\n", url);
  const HINSTANCE result = ShellExecuteA(nullptr, "open", url, nullptr, nullptr, SW_SHOWDEFAULT);
  if (reinterpret_cast<std::intptr_t>(result) <= 32) {
    idLib::Error("Could not open url: '%s' ", url);
    return;
  }
  if (HWND foreground = GetForegroundWindow()) ShowWindow(foreground, SW_MAXIMIZE);
  if (doExit) { doExitSpamGuard = true; cmdSystem->AppendCommandText("quit\n"); }
}

void idSysLocal::StartProcess(const char* executablePath, bool doExit) {
  STARTUPINFOA startup{};
  startup.cb = sizeof(startup);
  PROCESS_INFORMATION process{};
  char commandLine[MAX_PATH + 12]{};
  idStr::Copynz(commandLine, executablePath != nullptr ? executablePath : "", sizeof(commandLine));
  if (!CreateProcessA(nullptr, commandLine, nullptr, nullptr, FALSE, 0, nullptr, nullptr, &startup,
                      &process)) {
    idLib::Error("Could not start process: '%s' ", commandLine);
    return;
  }
  CloseHandle(process.hThread);
  CloseHandle(process.hProcess);
  if (doExit) cmdSystem->AppendCommandText("quit\n");
}

int Sys_GetRamMegabytes() {
  MEMORYSTATUSEX status{};
  status.dwLength = sizeof(status);
  GlobalMemoryStatusEx(&status);
  return (static_cast<int>(status.ullTotalPhys >> 20U) + 8) & ~15;
}

std::uint64_t Sys_GetDriveFreeSpaceInBytes(const char* path) {
  ULARGE_INTEGER available{};
  ULARGE_INTEGER total{};
  ULARGE_INTEGER free{};
  return GetDiskFreeSpaceExA(path, &available, &total, &free) ? available.QuadPart : 1;
}

void Sys_SetSoundCaptureBackend(soundCaptureStart_t start, soundCaptureStop_t stop) {
  captureStart = start;
  captureStop = stop;
}

void Sys_SoundCaptureStop() { if (captureStop != nullptr) captureStop(); }
void Sys_SoundCaptureStart(const char* outputName) { if (captureStart != nullptr) captureStart(outputName); }

void Sys_SetLanguageFromSystem() {
  idCVar* sysLang = cvarSystem->Find("sys_lang");
  if (sysLang == nullptr || idStr::Icmp(sysLang->GetString(), "english") != 0) return;
  wchar_t localeName[LOCALE_NAME_MAX_LENGTH]{};
  const char* language = "english";
  if (GetUserDefaultLocaleName(localeName, LOCALE_NAME_MAX_LENGTH)) {
    if (wcsncmp(localeName, L"fr", 2) == 0) language = "french";
    else if (wcsncmp(localeName, L"de", 2) == 0) language = "german";
    else if (wcsncmp(localeName, L"it", 2) == 0) language = "italian";
    else if (wcsncmp(localeName, L"ja", 2) == 0) language = "japanese";
    else if (wcsncmp(localeName, L"pl", 2) == 0) language = "polish";
    else if (wcsncmp(localeName, L"ru", 2) == 0) language = "russian";
    else if (wcsncmp(localeName, L"es", 2) == 0) language = "spanish";
  }
  sysLang->SetString(language);
}
