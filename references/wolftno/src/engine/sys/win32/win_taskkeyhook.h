#pragma once

#include <windows.h>

LRESULT CALLBACK MyTaskKeyHookLL(int code, WPARAM message, LPARAM data);
void DisableTaskKeys(int disable, int beep, int taskManager);
