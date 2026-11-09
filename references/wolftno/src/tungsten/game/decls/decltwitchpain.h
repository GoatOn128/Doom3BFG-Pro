#pragma once

#include <string>
#include <vector>

class idTreeAnimator;

class idDeclTwitchPain {
public:
  void AddAliasForTest(int damageDirection, short jointIndex,
                       unsigned short aliasHandle) {
    aliases.push_back({damageDirection, jointIndex, aliasHandle});
  }
  unsigned short GetTwitchPainAliasHandle(const idTreeAnimator* model,
                                          int damageDirection,
                                          short jointIndex) const {
    ++aliasQueryCount;
    lastModel = model;
    lastDamageDirection = damageDirection;
    lastJointIndex = jointIndex;
    for (const alias_t& alias : aliases) {
      if (alias.damageDirection == damageDirection &&
          alias.jointIndex == jointIndex) {
        return alias.aliasHandle;
      }
    }
    for (const alias_t& alias : aliases) {
      if (alias.damageDirection == damageDirection &&
          alias.jointIndex == -1) {
        return alias.aliasHandle;
      }
    }
    return 0xFFFFU;
  }
  void AddTwitchGroupForTest(short jointIndex, const char* groupName) {
    twitchGroups.push_back(
        {jointIndex, groupName != nullptr ? groupName : ""});
  }
  const char* GetTwitchGroupName(short jointIndex) const {
    ++groupQueryCount;
    lastGroupJointIndex = jointIndex;
    for (const twitchGroup_t& group : twitchGroups) {
      if (group.jointIndex == jointIndex) return group.groupName.c_str();
    }
    return nullptr;
  }
  int GetAliasQueryCountForTest() const { return aliasQueryCount; }
  const idTreeAnimator* GetLastModelForTest() const { return lastModel; }
  int GetLastDamageDirectionForTest() const { return lastDamageDirection; }
  short GetLastJointIndexForTest() const { return lastJointIndex; }
  int GetGroupQueryCountForTest() const { return groupQueryCount; }
  short GetLastGroupJointIndexForTest() const {
    return lastGroupJointIndex;
  }

private:
  struct alias_t {
    int damageDirection;
    short jointIndex;
    unsigned short aliasHandle;
  };
  struct twitchGroup_t {
    short jointIndex;
    std::string groupName;
  };

  std::vector<alias_t> aliases;
  std::vector<twitchGroup_t> twitchGroups;
  mutable int aliasQueryCount{0};
  mutable const idTreeAnimator* lastModel{nullptr};
  mutable int lastDamageDirection{0};
  mutable short lastJointIndex{-1};
  mutable int groupQueryCount{0};
  mutable short lastGroupJointIndex{-1};
};
