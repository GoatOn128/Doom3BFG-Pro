#pragma once

#include "tungsten/game/ai/ai2_actions.h"

#include <array>
#include <cstdint>
#include <string>
#include <vector>

struct idAnimWebAINode {
  int subWebIndex{-1};
  int stateIndex{-1};
  int animDelta{0};
  int flags{0};
  bool randomEdgeCost{false};
  std::string animationName;
  std::vector<idAnimWebAINode*> children;
};

struct idSerializer {
  bool writing{true};
  std::vector<std::uint32_t> words;
  std::size_t cursor{0};
};

class idRenderModelGui {
public:
  virtual ~idRenderModelGui() = default;
  virtual void DrawAnimation(const char* name, float alpha, float offset) = 0;
};

class idAnimWebAI {
public:
  idAnimWebAI();
  virtual ~idAnimWebAI();

  virtual void InternalTriggerEvent(const idAnimWebAINode* node,
                                    int eventType, int webHandle,
                                    int eventNumber, int eventParameter);
  void InternalInitWeb();
  void InternalFinishBlend(const idAnimWebAINode* previousNode,
                           const idAnimWebAINode* currentNode,
                           int currentTime);
  void InternalStartPath();
  void SetForwardRunIndex(float newRunIndex, float newIdleIndex,
                          runIndexType_t type, bool blend, bool forced);
  int InternalGetEdgeCost(int nodeIndex, int edgeIndex,
                          bool randomEdgeCost);
  void UpdateDesiredTurnIndex();
  void CalcToCombatTurn(const idVec3& desiredDirection);
  void CalcToSearchTurn(const idVec3& desiredDirection);
  void SetAnimIndicesForRunType(int type, bool blend);
  void SerializeSnapshot(idSerializer* serializer);
  bool GetDestinationIndices(int* destinationSubWebIndex,
                             int* destinationStateIndex) const;
  int DebugDrawNode(idRenderModelGui* gui, idAnimWebAINode* node,
                    float nodeAlpha, float offset);
  void InternalUpdateScalars(int currentTime);
  void InternalStartBlend(const idAnimWebAINode* currentNode,
                          const idAnimWebAINode* nextNode, int currentTime);
  void DebugDrawNodes(idRenderModelGui* gui);
  void InternalSetAnimDelta(int modelIndex, const idAnimWebAINode* node);

  void SetAI(idAI2* value) { ai = value; }
  void SetOrientation(const idVec3& bodyForward,
                      const idVec3& bodyIdealDirection,
                      const idVec3& moveForward,
                      const idVec3& moveIdealDirection);
  void SetDesiredDirection(const idVec3& value) { desiredDirection = value; }
  void SetRunIndices(runIndexType_t type, float runIndex, float idleValue);
  void SetDestination(int subWeb, int state) {
    destinationSubWebIndex = subWeb;
    destinationStateIndex = state;
  }
  void SetPathDestination(int subWeb, int state) {
    pathDestinationSubWebIndex = subWeb;
    pathDestinationStateIndex = state;
  }
  void SetDebugRoot(idAnimWebAINode* value) { debugRoot = value; }
  void SetBlendTargets(float aim, float gangsta, float speed) {
    targetAimAlpha = aim;
    targetGangstaAlpha = gangsta;
    targetRunSpeedScale = speed;
  }
  const std::vector<std::string>& GetBlendVariableNames() const {
    return blendVariableNames;
  }
  float GetForwardRunIndex() const { return forwardRunIndex; }
  float GetIdleIndex() const { return idleIndex; }
  runIndexType_t GetRunIndexType() const { return runIndexType; }
  float GetForwardRunBlendLinear() const { return forwardRunBlendLinear; }
  float GetForwardRunBlend() const { return forwardRunBlend; }
  float GetDesiredTurn() const { return desiredTurn; }
  float GetToCombatTurn() const { return toCombatTurn; }
  float GetToSearchTurn() const { return toSearchTurn; }
  float GetAimAlpha() const { return aimAlpha; }
  float GetGangstaAlpha() const { return gangstaAlpha; }
  float GetRunSpeedScale() const { return runSpeedScale; }
  int GetMovementMode() const { return movementMode; }
  bool GetWorldCollision() const { return worldCollision; }
  bool IsBlendAngleLocked() const { return lockdownBlendAngle; }
  int GetLastEventNumber() const { return lastEventNumber; }
  int GetLastEventParameter() const { return lastEventParameter; }
  void SetRandomSeed(std::uint32_t value) { randomSeed = value; }

protected:
  static float ClassifyTurn(const idVec3& desiredDirection,
                            const idVec3& forwardDirection);

  idAI2* ai{nullptr};
  std::vector<std::string> blendVariableNames;
  idVec3 bodyForward{1.0F, 0.0F, 0.0F};
  idVec3 bodyIdeal{1.0F, 0.0F, 0.0F};
  idVec3 moveForward{1.0F, 0.0F, 0.0F};
  idVec3 moveIdeal{1.0F, 0.0F, 0.0F};
  idVec3 desiredDirection{1.0F, 0.0F, 0.0F};
  float bodyTurnDot{0.0F};
  float movementTurnDot{0.0F};
  float forwardRunIndex{0.0F};
  float leftRunIndex{1.0F};
  float rightRunIndex{2.0F};
  float backRunIndex{3.0F};
  float previousRunIndex{0.0F};
  float nextRunIndex{0.0F};
  float idleIndex{0.0F};
  float previousIdleIndex{0.0F};
  float nextIdleIndex{0.0F};
  float forwardRunBlend{1.0F};
  float forwardRunBlendLinear{1.0F};
  float blendBack{0.0F};
  float blendRight{0.0F};
  float blendAngle{0.0F};
  float fusionMoveAngle{0.0F};
  float desiredTurn{0.0F};
  float transitionSelect{0.0F};
  float toCombatTurn{0.0F};
  float toSearchTurn{0.0F};
  float aimAlpha{0.0F};
  float gangstaAlpha{0.0F};
  float runSpeedScale{0.0F};
  float targetAimAlpha{0.0F};
  float targetGangstaAlpha{0.0F};
  float targetRunSpeedScale{0.0F};
  float turnLeanAmount{0.0F};
  float turnLeanDirection{0.0F};
  float scriptBlend{0.0F};
  float walkRate{1.0F};
  float runRate{1.0F};
  runIndexType_t runIndexType{ANIMWEBAI_RUNINDEXTYPE_NORMAL};
  runIndexType_t nextRunIndexType{ANIMWEBAI_RUNINDEXTYPE_NORMAL};
  std::array<float, ANIMWEBAI_RUNINDEXTYPE_MAX> runIndices{};
  std::array<float, ANIMWEBAI_RUNINDEXTYPE_MAX> idleIndices{};
  int destinationSubWebIndex{-1};
  int destinationStateIndex{-1};
  int pathDestinationSubWebIndex{-1};
  int pathDestinationStateIndex{-1};
  int movementMode{0};
  int lastUpdateTime{0};
  int lockdownBlendAngleStartTime{0};
  int lastEventNumber{-1};
  int lastEventParameter{0};
  int lastEventWebHandle{-1};
  bool lockdownBlendAngle{false};
  bool worldCollision{true};
  bool debug{false};
  std::uint32_t randomSeed{0x12345678u};
  idAnimWebAINode* debugRoot{nullptr};
};

class idAnimWeb_TurretGunner : public idAnimWebAI {
public:
  void InternalTriggerEvent(const idAnimWebAINode* node, int eventType,
                            int webHandle, int eventNumber,
                            int eventParameter) override;
};

float EaseInEaseOut(float linearInput);

extern idCVar ai_walkLerpScale;
extern idCVar ai_gangstaLerpScale;
extern idCVar ai_aimLerpScale;
extern idCVar ai_animWebDebug;
extern idCVar ai_debugWeb;
extern idCVar ai_deathAnimScale;
extern idCVar ai_debugtransitiondirs;
extern idCVar ai_stairWalkRate;
extern idCVar ai_stairRunRate;
extern idCVar ai_debugForwardRunIndex;
extern idCVar ai_runCycleBlendMode;
extern idCVar ai_turnLeanLerpScaleUp;
extern idCVar ai_turnLeanLerpScaleDown;
extern idCVar ai_runSpeedScaleUp;
extern idCVar ai_runSpeedScaleDown;
