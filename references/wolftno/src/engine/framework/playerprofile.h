#pragma once

#define WOLFTNO_PLAYERPROFILE_DEFINED 1

#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

class idSerializer;

union profileStatValue_t {
  int i;
  float f;
};

class idPlayerProfile {
public:
  static constexpr int MAX_PLAYER_PROFILE_STATS = 500;
  static constexpr int MAX_DLC_ITEMS = 15;

  enum state_t {
    IDLE = 0,
    SAVING = 1,
    LOADING = 2,
    SAVE_REQUESTED = 3,
    LOAD_REQUESTED = 4,
    ERR = 5,
  };

  virtual ~idPlayerProfile();
  virtual void PrepareForNewUser() = 0;
  virtual bool Serialize(idSerializer& serializer) = 0;
  virtual bool UpdateDisplayModeFromCvars() { return false; }
  virtual bool CommitDisplayChanges() { return false; }
  virtual float GetMouseSensitivity() { return 1.0f; }
  virtual void SetDefaultDevelopmentParameters() {}
  virtual int GetLevel() = 0;
  virtual void SetInvertLook(bool invert) = 0;
  virtual unsigned GetSubtitleLanguageMask() { return ~0u; }
  virtual void SetSubtitleLanguageMask(unsigned mask) = 0;
  virtual void GetLoadingScreenDevMessage(idStr& message) = 0;
  virtual bool HasPlayerProgress() = 0;
  virtual unsigned GetCompletionValue(bool includeSecrets) = 0;

  int GetDeviceNumForProfile() { return deviceNum; }
  void SetDeviceNumForProfile(int value) { deviceNum = value; }
  void SaveSettings(bool withIndicatorMessage);
  void LoadSettings();
  state_t GetState() { return state; }
  state_t GetRequestedState() { return requestedState; }
  bool GetAchievement(int id) const;
  void SetAchievement(int id);
  void ClearAchievement(int id);
  int GetDlcReleaseVersion() { return dlcReleaseVersion; }
  void SetDlcReleaseVersion(int version) { dlcReleaseVersion = version; }
  void SetGameModeType(int type) { gameModeType = type; }
  int GetGameModeType() { return gameModeType; }
  void SetNoUser(bool noUser) { hasUser = !noUser; }
  void SetClearExistingOnWrite(bool clear) { clearExistingOnWrite = clear; }
  bool GetClearExistingOnWrite() { return clearExistingOnWrite; }
  void SetResetFileOwner(bool reset) { resetFileOwner = reset; }
  bool GetResetFileOwner() { return resetFileOwner; }
  float GetMouseXFrac() { return mouseXFrac; }
  float GetMouseYFrac() { return mouseYFrac; }
  void SetMouseFrac(float x, float y) { mouseXFrac = x; mouseYFrac = y; }
  bool AddSeenDLCItem(std::uint64_t item) {
    for (int index = 0; index < seenDLCItems.Num(); ++index)
      if (seenDLCItems[index] == item)
        return false;
    return seenDLCItems.Append(item) >= 0;
  }
  void SetState(state_t value) { state = value; }
  void SetRequestedState(state_t value) { requestedState = value; }

protected:
  idPlayerProfile();
  void SetDefaultsImpl();

private:
  void StatSetInt(int stat, int value);
  void StatSetFloat(int stat, float value);
  int StatGetInt(int stat) const;
  float StatGetFloat(int stat) const;

public:

  state_t state;
  state_t requestedState;
  int deviceNum;
  bool hasUser;
  bool clearExistingOnWrite;
  bool resetFileOwner;
  float mouseXFrac;
  float mouseYFrac;
  std::uint64_t achievementBits;
  std::uint64_t achievementBits2;
  int dlcReleaseVersion;
  int gameModeType;
  idStaticList<profileStatValue_t, MAX_PLAYER_PROFILE_STATS> stats;
  idStaticList<std::uint64_t, MAX_DLC_ITEMS> seenDLCItems;
};

static_assert(sizeof(profileStatValue_t) == 4, "profile-stat ABI drift");
static_assert(offsetof(idPlayerProfile, state) == 0x8,
              "player-profile state offset drift");
static_assert(offsetof(idPlayerProfile, stats) == 0x38,
              "player-profile stats offset drift");
static_assert(offsetof(idPlayerProfile, seenDLCItems) == 0x820,
              "player-profile DLC offset drift");
static_assert(sizeof(idPlayerProfile) == 0x8B0,
              "player-profile ABI drift");
