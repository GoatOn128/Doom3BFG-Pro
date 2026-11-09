#pragma once

#include <array>
#include <cstdint>

// wingdi.h defines ERROR as 0, while the retail profile state enum also uses
// ERROR.  The engine header must remain includable after Windows.h.
#ifdef ERROR
#undef ERROR
#endif

struct localUserHandle_t {
  std::uint32_t handle{};
  friend bool operator==(localUserHandle_t lhs, localUserHandle_t rhs) { return lhs.handle == rhs.handle; }
};

#if !defined(WOLFTNO_PLAYERPROFILE_DEFINED)
class idPlayerProfile {
public:
  enum state_t { IDLE = 0, LOADING, SAVING, ERROR };

  void SetAchievement(int index);
  void ClearAchievement(int index);
  bool HasAchievement(int index) const;
  void StatSetInt(int index, int value);
  void StatSetFloat(int index, float value);
  int StatGetInt(int index) const;
  float StatGetFloat(int index) const;

  state_t state{IDLE};
  std::array<std::uint64_t, 2> achievementBits{};
  std::array<int, 500> integerStats{};
  std::array<float, 500> floatStats{};
  int dlcReleaseVersion{};
};
#endif

class idProfileMgr {
public:
  idProfileMgr();
  ~idProfileMgr();
  void Init(class idLocalUser* owner);
  void Pump();
  idPlayerProfile* GetProfile();
  const idPlayerProfile* GetProfile() const;
  void SetProfile(idPlayerProfile* value);
  bool LoadSettingsAsync();
  bool SaveSettingsAsync();

private:
  class idLocalUser* owner{};
  idPlayerProfile* profile{};
  bool ownsProfile{};
};

class idLocalUser {
public:
  idLocalUser();
  virtual ~idLocalUser();
  virtual void PumpPlatform() {}
  virtual bool IsPersistent();
  virtual bool IsProfileReady() { return GetProfile() != nullptr; }
  virtual bool IsOnline() { return true; }
  virtual bool IsAgeRestricted() { return false; }
  virtual unsigned GetOnlineCaps() { return ~0U; }
  virtual bool HasOwnerChanged() { return false; }
  virtual int GetInputDevice() { return -1; }
  virtual const char* GetGamerTag() { return "Player"; }
  virtual bool IsInParty() { return false; }
  virtual int GetPartyCount() { return 1; }
  virtual bool IsStorageDeviceAvailable();
  virtual void ResetStorageDevice() {}
  virtual bool StorageSizeAvailable(std::uint64_t minimumBytes, std::int64_t* neededBytes);
  virtual void SetStatInt(int stat, int value);
  virtual void SetStatFloat(int stat, float value);
  virtual int GetStatInt(int stat);
  virtual float GetStatFloat(int stat);
  virtual idPlayerProfile* GetProfile();
  localUserHandle_t GetLocalUserHandle() const { return localUserHandle; }

  void Pump();
  void LoadProfileSettings();
  void SaveProfileSettings();

  bool joiningLobby[2]{};
  localUserHandle_t localUserHandle{};
  idProfileMgr profileMgr;
  bool syncAchievementsRequested{};
  bool onlineWarningDisplayed{};
};
