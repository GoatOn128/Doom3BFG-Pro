#include "engine/sys/win32/win_localuser.h"

bool idLocalUserWin::VerifyUserState(winUserState_t* state) {
  return state != nullptr && state->inputDevice == inputDevice;
}

void idLocalUserWin::Init(int inputDeviceValue, const char* gamertagValue, int numLocalUsers) {
  if (numLocalUsers == 1) gamertag = gamertagValue != nullptr ? gamertagValue : "";
  else gamertag.Format("%s (%i)", gamertagValue != nullptr ? gamertagValue : "", numLocalUsers);
  inputDevice = inputDeviceValue;
}
