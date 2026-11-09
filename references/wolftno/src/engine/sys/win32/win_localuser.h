#pragma once

#include "engine/sys/sys_localuser.h"
#include "shared/idlib/text/str.h"

struct winUserState_t {
  int inputDevice{-1};
};

class idLocalUserWin : public idLocalUser {
public:
  idLocalUserWin();
  ~idLocalUserWin() override;
  idLocalUserWin& operator=(const idLocalUserWin& other);
  bool VerifyUserState(winUserState_t* state);
  void Init(int inputDeviceValue, const char* gamertagValue, int numLocalUsers);
  unsigned GetOnlineCaps() override;
  int GetInputDevice() override;
  const char* GetGamerTag() override { return gamertag.data; }
  winUserState_t GetUserState() const { return {inputDevice}; }

private:
  idStrStatic<64> gamertag;
  int inputDevice{-1};
};
