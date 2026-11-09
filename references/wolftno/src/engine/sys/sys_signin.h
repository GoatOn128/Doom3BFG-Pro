#pragma once

#include "engine/sys/sys_localuser.h"
#include "engine/sys/sys_public.h"

class idSignInManagerBase {
public:
  idSignInManagerBase() = default;
  virtual ~idSignInManagerBase();

  virtual void Pump() = 0;
  virtual int GetNumLocalUsers() const = 0;
  virtual idLocalUser* GetLocalUserByIndex(int index) = 0;
  virtual const idLocalUser* GetLocalUserByIndex(int index) const = 0;
  virtual void RemoveLocalUserByIndex(int index) = 0;
  virtual void RegisterLocalUser(int inputDevice) = 0;
  virtual idLocalUser* GetRegisteringUser() { return nullptr; }
  virtual idLocalUser* GetRegisteringUserByInputDevice(int) { return nullptr; }
  virtual void SignIn(int, bool) {}
  virtual void SignIn() {}
  virtual bool IsDeviceBeingRegistered(int) { return false; }
  virtual bool IsAnyDeviceBeingRegistered() { return false; }
  virtual void Shutdown() {}
  virtual void DebugOutputLocalUserInfo() {}
  virtual bool IsDialogActive() const { return false; }
  virtual bool IsCreditsInputLocked() const { return false; }
  virtual bool IsTitleStorageLoaded() const { return false; }
  virtual int GetTitleStorageDlcReleaseVersion() const { return 0; }
  virtual void SetMarketplaceHasNewContent(bool) {}
  virtual void ShowNewContentAvailable() {}

  void SetDesiredLocalUsers(int minimum, int maximum) {
    minDesiredLocalUsers = minimum;
    maxDesiredLocalUsers = maximum;
  }
  bool ProcessInputEvent(const sysEvent_t* event);
  idPlayerProfile* GetDefaultProfile();
  idLocalUser* GetMasterLocalUser();
  idLocalUser* GetLocalUserByInputDevice(int inputDevice);
  idPlayerProfile* GetPlayerProfileByInputDevice(int inputDevice);
  idLocalUser* GetLocalUserByHandle(localUserHandle_t handle);
  bool RemoveLocalUserByHandle(localUserHandle_t handle);
  void RemoveAllLocalUsers();
  void ValidateLocalUsers(bool requireOnline);
  localUserHandle_t GetUniqueLocalUserHandle(const char* name);
  void UpdateLocalDlcInformation();

protected:
  int minDesiredLocalUsers{};
  int maxDesiredLocalUsers{};
  idPlayerProfile* defaultProfile{};
  bool dlcVersionChecked{};
};

idSignInManagerBase* Sys_GetActiveSignInManager();
void Sys_SetActiveSignInManager(idSignInManagerBase* manager);
