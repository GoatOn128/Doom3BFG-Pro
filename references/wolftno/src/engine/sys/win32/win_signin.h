#pragma once

#include "engine/sys/sys_signin.h"
#include "engine/sys/win32/win_localuser.h"
#include "shared/idlib/text/str.h"

#include <memory>
#include <vector>

struct DlcInstalled_t {
  unsigned appId{};
};

struct winSignInCallbacks_t {
  void (*onLocalUserSignin)(idLocalUserWin* user){};
  void (*onLocalUserSignout)(idLocalUserWin* user){};
  void (*onDlcInstalled)(const DlcInstalled_t& event){};
  const char* (*platformPersonaName)(){};
};

void Win_SetSignInCallbacks(const winSignInCallbacks_t& callbacks);

class idSignInManagerWin final : public idSignInManagerBase {
public:
  idSignInManagerWin();
  ~idSignInManagerWin() override;

  void OnDLCInstalled(DlcInstalled_t* event);
  void Pump() override;
  int GetNumLocalUsers() const override;
  idLocalUser* GetLocalUserByIndex(int index) override;
  const idLocalUser* GetLocalUserByIndex(int index) const override;
  void RemoveLocalUserByIndex(int index) override;
  void RegisterLocalUser(int inputDevice) override;
  void Shutdown() override;

  void EncodeUser(idLocalUser* localUser, idStr& outputBuffer) const;
  bool CreateNewUserFromEncodedSwapData(const idStr& encodedSwapData);

private:
  std::vector<std::unique_ptr<idLocalUserWin>> localUsers;
};

