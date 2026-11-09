#pragma once

#include "shared/idlib/containers/list.h"

#include <array>
#include <string>

class idHudInfo {
public:
  struct weaponSelectionElement_t {
    void Clear();
    int slotRef{-1};
    int slotIndex{-1};
    std::string icon;
    std::string weaponName;
    std::string weaponNameMulti;
    bool hidden{false};
    bool dualWield{false};
  };
  struct weaponSelection_t {
    void ClearElements();
    bool SetCurrentWeapon(const void* player, int weapon, int angleIndex);
    std::array<weaponSelectionElement_t, 13> weaponElements{};
    int numWeapons{0};
    int currentSelection{-1};
    int currentSelectArray{-1};
    int selectionAngleIndex{-1};
    int prevSelectionAngle{-1};
    int prevWeaponSelection{-1};
  };
};

class idHudDangerInfo {
public:
  enum commanderText_t { none, signal, alarm, reinforce, signalLost, terminated, engaging };
  struct commanderInfo_t {
    int commanderFlags{0};
    int arrayIndex{-1};
    int distanceToCommander{-1};
    int entityIndex{-1};
    int dangerIndex{-1};
    int resetTime{0};
    int timer{0};
    commanderText_t currentText{none};
    bool operator==(const commanderInfo_t& other) const { return arrayIndex == other.arrayIndex; }
  };
  struct dangerEntry_t { int spawnIndex{-1}; int flags{0}; float playerRange{0}; int terminatedTimer{0}; };

  idHudDangerInfo();
  bool CommanderExist(int spawnIndex) const;
  void Clear();
  void UpdateCommanderSlots();
  void UpdateCommander(int index, int currentTime);
  bool AddCommander(int arrayIndex);
  void SetDangerEntry(int index, const dangerEntry_t& entry) { if(index>=0&&index<3) dangerEntries[static_cast<std::size_t>(index)]=entry; }
  int NumCommanders() const { return commanders.Num(); }
  const commanderInfo_t& GetCommander(int index) const { return commanders[index]; }

private:
  idList<commanderInfo_t, 5> commanders;
  std::array<dangerEntry_t, 3> dangerEntries{};
  std::array<int, 8> slotsTaken{};
};
