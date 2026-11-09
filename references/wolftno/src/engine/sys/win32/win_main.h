#pragma once

#include <Windows.h>

#include <cstdint>

using winMainCallback_t = int (*)(const char* commandLine);
using winShutdownCallback_t = void (*)(int exitCode);

void Win_SetMainCallbacks(winMainCallback_t mainCallback, winShutdownCallback_t shutdownCallback);
bool Sys_IsGameWindowActive();
void Sys_PumpRenderThreadEvents();
bool Sys_PumpRenderThreadEventsIfRequested();
void Sys_RequestRenderThreadEventPump();
void Sys_PumpEvents();
bool Win_KillOtherInstances();
BOOL CALLBACK HideThreadWindows(HWND window, LPARAM parameter);
void invalidParameterHandler(const wchar_t* expression, const wchar_t* function,
                             const wchar_t* file, unsigned line, std::uintptr_t reserved = 0);
void Sys_ReLaunch(const char* data, unsigned dataSize);
[[noreturn]] void Win_Error(const char* error);
[[noreturn]] void Sys_Quit(UINT exitCode);

