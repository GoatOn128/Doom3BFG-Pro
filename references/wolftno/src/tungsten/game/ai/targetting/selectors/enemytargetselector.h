#pragma once

#include "tungsten/game/ai/targetting/targetselector.h"

class idEnemyTargetSelector final : public idTargetSelector {
public:
  struct weights_t {
    float range{1.0F};
    float angle{1.0F};
    float relation{1.0F};
    float group{1.0F};
    float current{1.0F};
    float damager{1.0F};
    float spotted{1.0F};
    float player{1.0F};
    float ai{1.0F};
    float fake{1.0F};
    float vehicle{1.0F};
    float aggro{1.0F};
  };

  float GetWeight(const idEntity* owner,
                  const idTargetInfo* targetInfo) const override;
  void SetFacing(const idVec3& value) { facing = value; facing.Normalize(); }
  void SetRange(float cap, float maximum) { rangeCap = cap; rangeMax = maximum; }
  void SetWeights(const weights_t& value) { weights = value; }
  void SetCurrentTarget(const idEntity* value) { currentTarget = value; }
  void SetFakeEnemy(const idEntity* value, float weight) {
    fakeEnemy = value;
    fakeEnemyWeight = weight;
  }
  void SetGroupTarget(const idEntity* value) { groupTarget = value; }
  void SetRecentDamager(const idEntity* value) { recentDamager = value; }
  void SetAggroTarget(const idEntity* value, float valueWeight) {
    aggroTarget = value;
    aggro = valueWeight;
  }

private:
  idVec3 facing{1.0F, 0.0F, 0.0F};
  float rangeCap{1024.0F};
  float rangeMax{2048.0F};
  float spottedMaxSeconds{5.0F};
  weights_t weights{};
  const idEntity* currentTarget{nullptr};
  const idEntity* fakeEnemy{nullptr};
  const idEntity* groupTarget{nullptr};
  const idEntity* recentDamager{nullptr};
  const idEntity* aggroTarget{nullptr};
  float fakeEnemyWeight{1.0F};
  float aggro{0.0F};
};
