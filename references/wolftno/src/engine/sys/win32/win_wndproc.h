#pragma once

#include <Windows.h>

struct winWindowCallbacks_t {
  void (*onWindowCreated)(HWND window){};
  void (*onCloseRequested)(){};
  void (*onDisplayChanged)(){};
  void (*onWindowSizeChanged)(int width, int height, int x, int y){};
  void (*onFocusChanged)(bool focused){};
  void (*toggleFullscreen)(){};
  bool (*isFullscreen)(){};
  bool (*showMenuCursor)(){};
  bool (*isFullscreenWindowed)(){};
  bool (*onPauseRequested)(){};
};

void Win_SetWindowCallbacks(const winWindowCallbacks_t& callbacks);
void WIN_Sizing(UINT side, RECT* rectangle);
LRESULT CALLBACK MainWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);

extern UINT WM_SHOW_OS_CURSOR;
extern UINT WM_HIDE_OS_CURSOR;
