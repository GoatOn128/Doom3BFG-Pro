#include "engine/sys/win32/win_taskkeyhook.h"

#include "engine/sys/win32/win_local.h"

namespace {
HHOOK keyboardHook = nullptr;
int beepOnBlockedKey = 0;
constexpr const char* taskManagerPolicyKey =
    "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System";
constexpr const char* taskManagerPolicyValue = "DisableTaskMgr";
}

LRESULT CALLBACK MyTaskKeyHookLL(int code, WPARAM message, LPARAM data) {
  if (code != HC_ACTION) return CallNextHookEx(keyboardHook, code, message, data);
  const auto* key = reinterpret_cast<const KBDLLHOOKSTRUCT*>(data);
  const bool controlEscape = key->vkCode == VK_ESCAPE && (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
  const bool altTab = key->vkCode == VK_TAB && (key->flags & LLKHF_ALTDOWN) != 0;
  const bool altEscape = key->vkCode == VK_ESCAPE && (key->flags & LLKHF_ALTDOWN) != 0;
  const bool windowsKey = key->vkCode == VK_LWIN || key->vkCode == VK_RWIN;
  if (!controlEscape && !altTab && !altEscape && !windowsKey)
    return CallNextHookEx(keyboardHook, code, message, data);
  if (beepOnBlockedKey != 0 && (message == WM_KEYDOWN || message == WM_SYSKEYDOWN)) MessageBeep(0);
  return 1;
}

void DisableTaskKeys(int disable, int beep, int taskManager) {
  if (disable != 0 && keyboardHook == nullptr)
    keyboardHook = SetWindowsHookExA(WH_KEYBOARD_LL, MyTaskKeyHookLL, win32.hInstance, 0);
  else if (disable == 0 && keyboardHook != nullptr) {
    UnhookWindowsHookEx(keyboardHook);
    keyboardHook = nullptr;
  }
  beepOnBlockedKey = beep;
  if (taskManager == 0) return;
  HKEY key = nullptr;
  if (RegCreateKeyExA(HKEY_CURRENT_USER, taskManagerPolicyKey, 0, nullptr, 0, KEY_SET_VALUE,
                      nullptr, &key, nullptr) != ERROR_SUCCESS) return;
  if (disable != 0) {
    const DWORD value = 1;
    RegSetValueExA(key, taskManagerPolicyValue, 0, REG_DWORD,
                   reinterpret_cast<const BYTE*>(&value), sizeof(value));
  } else {
    RegDeleteValueA(key, taskManagerPolicyValue);
  }
  RegCloseKey(key);
}
