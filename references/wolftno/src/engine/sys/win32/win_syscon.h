#pragma once

#include <windows.h>

LRESULT CALLBACK InputLineWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
void Sys_DestroyConsole();
void Sys_RedirectIO();
void Sys_ShowConsole(int visibilityLevel, bool quitOnClose);
char* Sys_ConsoleInput();
void Conbuf_AppendText(const char* message);
LRESULT CALLBACK ConWndProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
void Sys_CreateConsole();
void Win_SetErrorText(const char* text);
