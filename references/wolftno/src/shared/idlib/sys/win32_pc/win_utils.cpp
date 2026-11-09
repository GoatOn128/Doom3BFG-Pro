#include "win_utils.h"

#include <Windows.h>

namespace {
char userName[128]{};
char machineName[128]{};
char emptyName[1]{};
} // namespace

char* Sys_GetOSUserName() {
  DWORD length = static_cast<DWORD>(sizeof(userName));
  return ::GetUserNameA(userName, &length) ? userName : emptyName;
}

char* Sys_GetMachineName() {
  DWORD length = static_cast<DWORD>(sizeof(machineName));
  return ::GetComputerNameA(machineName, &length) ? machineName : emptyName;
}
