#pragma once

#include "tungsten/game/ai/ai2_actions.h"

enum turnDirection_t { TURN_NONE, TURN_LEFT, TURN_RIGHT };

class idAIOrientation {
public:
  idAIOrientation* operator->() { return this; }
  const idAIOrientation* operator->() const { return this; }
  bool operator==(const idAIOrientation* other) const {
    return this == other;
  }
  idAIOrientation(float minimumRate = 0.0F, float maximumRate = 360.0F);
  void UpdateStatic();
  void Init(const idMat3& value);
  void SetIdealDir(idAI2* ai, const idVec3& direction);
  void SetIdealDirTowards(idAI2* ai, const idVec3& point);
  void UpdateFromAnim(const idMat3& value, float turnRate,
                      turnDirection_t turnDirection, bool updateIdeal);
  turnDirection_t CalculateTurnDirection(const idVec3& normal) const;
  float CalcTurnRateForTime(const idVec3& rotationAxis,
                            const idVec3& destinationDirection,
                            float timeSeconds) const;
  void SetAxis(const idVec3& direction, const idVec3& gravityDirection);
  void SetAxis(const idMat3& value);
  turnDirection_t UpdateAxis(const idVec3& normal, float turnRate,
                             float timeSeconds);
  turnDirection_t UpdateBodyAxis(const idVec3& normal,
                                 turnDirection_t turnDirection,
                                 float turnRate, float timeSeconds);
  void Update(const idVec3& velocity, const idVec3& origin,
              const idVec3& destination, const idVec3& gravityDirection,
              float turnRate, float maximumTurnRate, float timeSeconds);
  void SetAxis(idAI2* ai, const idVec3& direction);

  const idMat3& GetAxis() const { return axis; }
  const idVec3& GetIdealDir() const { return idealDir; }
  turnDirection_t GetCurrentTurnDirection() const { return currentTurnDir; }
  turnDirection_t GetPreviousTurnDirection() const { return previousTurnDir; }
  float GetLastTurnRate() const { return lastTurnRate; }
  void SetTolerance(float dotTolerance) { tolerance = dotTolerance; }
  float GetTurnVelocity() const { return turnVelocity; }
  float GetMinimumRateForTest() const { return minRate; }
  float GetMaximumRateForTest() const { return maxRate; }
  void SetTurnVelocity(float value) { turnVelocity = value; }
  void SetCurrentTurnDirection(turnDirection_t value) {
    currentTurnDir = value;
  }
  int GetUpdateStaticCountForTest() const { return updateStaticCount; }
  int GetUpdateAxisCountForTest() const { return updateAxisCount; }
  int GetUpdateBodyAxisCountForTest() const { return updateBodyAxisCount; }
  const idVec3& GetLastUpdateNormalForTest() const {
    return lastUpdateNormal;
  }
  float GetLastUpdateRateForTest() const { return lastUpdateRate; }
  float GetLastUpdateSecondsForTest() const { return lastUpdateSeconds; }
  turnDirection_t GetLastForcedDirectionForTest() const {
    return lastForcedDirection;
  }
  void ResetUpdateObservationsForTest() {
    updateStaticCount = 0;
    updateAxisCount = 0;
    updateBodyAxisCount = 0;
    lastUpdateNormal = {};
    lastUpdateRate = 0.0F;
    lastUpdateSeconds = 0.0F;
    lastForcedDirection = TURN_NONE;
  }

private:
  turnDirection_t RotateTowardIdeal(const idVec3& normal, float turnRate,
                                    float timeSeconds,
                                    turnDirection_t forcedDirection);
  idVec3 idealDir{1.0F, 0.0F, 0.0F};
  idMat3 axis{};
  float lastTurnDelta{0.0F};
  float lastTurnRate{0.0F};
  float tolerance{0.99984771F};
  turnDirection_t currentTurnDir{TURN_NONE};
  turnDirection_t previousTurnDir{TURN_NONE};
  float turnVelocity{0.0F};
  float minRate{0.0F};
  float maxRate{360.0F};
  int updateStaticCount{0};
  int updateAxisCount{0};
  int updateBodyAxisCount{0};
  idVec3 lastUpdateNormal{};
  float lastUpdateRate{0.0F};
  float lastUpdateSeconds{0.0F};
  turnDirection_t lastForcedDirection{TURN_NONE};
};
