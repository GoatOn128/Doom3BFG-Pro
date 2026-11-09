#pragma once

#include <cstdint>
#include <windows.h>

HMODULE Sys_DLL_Load(const char* dllName);
FARPROC Sys_DLL_GetProcAddress(HMODULE module, const char* procedureName);
void Sys_DLL_Unload(HMODULE module);
const char* Sys_DefaultDevStoragePath();
const char* Sys_DefaultCachePath();
char* Sys_GetClipboardData();
void Sys_SetClipboardData(const char* text);
int Sys_GetRamMegabytes();
std::uint64_t Sys_GetDriveFreeSpaceInBytes(const char* path);
void Sys_SoundCaptureStop();
void Sys_SoundCaptureStart(const char* outputName);
void Sys_SetLanguageFromSystem();

using soundCaptureStart_t = void (*)(const char* outputName);
using soundCaptureStop_t = void (*)();
void Sys_SetSoundCaptureBackend(soundCaptureStart_t start, soundCaptureStop_t stop);
