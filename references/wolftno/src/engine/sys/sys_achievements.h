#pragma once

#include <array>
#include <cstdint>
#include "shared/idlib/containers/list.h"

class idLocalUser;

struct achievementDescription_t {
  char name[500]{};
  char description[1000]{};
  bool hidden{};
};

class idAchievementSystem {
public:
  virtual ~idAchievementSystem();
  virtual void Init() {}
  virtual void Shutdown() {}
  virtual bool IsInitialized() const { return true; }
  virtual void RegisterLocalUser(idLocalUser* user);
  virtual void RemoveLocalUser(idLocalUser* user);
  virtual void AchievementUnlock(idLocalUser*, int) {}
  virtual void AchievementLock(idLocalUser*, int) {}
  virtual void AchievementLockAll(idLocalUser*, int) {}
  virtual void Pump() {}
  virtual void Reset(idLocalUser*) {}
  virtual void Cancel(idLocalUser*) {}
  virtual bool GetAchievementDescription(idLocalUser*, int, achievementDescription_t*) { return false; }
  virtual bool GetRequiredStorage(std::uint64_t* requiredSizeTrophiesBytes);
  virtual bool GetAchievementState(idLocalUser*, std::array<bool, 128>&) = 0;
  virtual bool SetAchievementState(idLocalUser*, const std::array<bool, 128>&) { return false; }

  void SyncAchievementBits(idLocalUser* user);

protected:
  idList<idLocalUser*> users;
};
