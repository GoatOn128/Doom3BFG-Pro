#pragma once

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>
#include <windows.h>

#include <cstdint>

class idCrashHandler;

struct win32State_t {
  HWND hWnd{};
  HINSTANCE hInstance{};
  IDirectInput8A* directInput{};
  OSVERSIONINFOA osVersion{};
  std::uint32_t cpuid{};
  bool mouseReleased{true};
  bool movingWindow{};
  bool activeApp{true};
  bool cdsFullscreen{};
  bool monitorPoweredOff{};
  bool visible{};
  idCrashHandler* crashHandler{};
};

extern win32State_t win32;
