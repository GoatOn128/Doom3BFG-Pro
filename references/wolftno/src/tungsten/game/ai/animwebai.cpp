#include "tungsten/game/ai/animwebai.h"

#include <algorithm>
#include <cmath>
#include <cstring>

idCVar ai_walkLerpScale("ai_walkLerpScale", "0.1", 4, "walk blend rate");
idCVar ai_gangstaLerpScale("ai_gangstaLerpScale", "0.1", 4,
                           "gangsta pose blend rate");
idCVar ai_aimLerpScale("ai_aimLerpScale", "0.1", 4, "aim blend rate");
idCVar ai_animWebDebug("ai_animWebDebug", "0", 1, "");
idCVar ai_debugWeb("ai_debugWeb", "-1", 1, "AI anim web to debug");
idCVar ai_deathAnimScale("ai_deathAnimScale", "1.0", 4,
                         "death animation scale");
idCVar ai_debugtransitiondirs("ai_debugtransitiondirs", "0", 1,
                              "debug transition directions");
idCVar ai_stairWalkRate("ai_stairWalkRate", "0.5", 4, "stair walk rate");
idCVar ai_stairRunRate("ai_stairRunRate", "0.5", 4, "stair run rate");
idCVar ai_debugForwardRunIndex("ai_debugForwardRunIndex", "0", 1,
                               "debug forward run index");
idCVar ai_runCycleBlendMode("ai_runCycleBlendMode", "2", 1,
                            "run cycle blend mode");
idCVar ai_turnLeanLerpScaleUp("ai_turnLeanLerpScaleUp", "0.075", 4,
                              "turn lean blend-up rate");
idCVar ai_turnLeanLerpScaleDown("ai_turnLeanLerpScaleDown", "0.04", 4,
                                "turn lean blend-down rate");
idCVar ai_runSpeedScaleUp("ai_runSpeedScaleUp", "0.1", 4, "");
idCVar ai_runSpeedScaleDown("ai_runSpeedScaleDown", "0.2", 4, "");

namespace {

float Approach(float current, float target, float scale) {
  const float amount = std::clamp(scale, 0.0F, 1.0F);
  return current + (target - current) * amount;
}

std::uint32_t FloatBits(float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

float BitsFloat(std::uint32_t bits) {
  float value = 0.0F;
  std::memcpy(&value, &bits, sizeof(value));
  return value;
}

}  // namespace

void idAnimWebAI::InternalTriggerEvent(const idAnimWebAINode* node,
                                       int eventType, int webHandle,
                                       int eventNumber, int eventParameter) {
  (void)node;
  (void)eventType;
  lastEventWebHandle = webHandle;
  lastEventNumber = eventNumber;
  lastEventParameter = eventParameter;
}

// DIA: idAnimWeb_TurretGunner::InternalTriggerEvent
void idAnimWeb_TurretGunner::InternalTriggerEvent(
    const idAnimWebAINode* node, int eventType, int webHandle,
    int eventNumber, int eventParameter) {
  idAnimWebAI::InternalTriggerEvent(node, eventType, webHandle, eventNumber,
                                    eventParameter);
}

// DIA: idAnimWebAI::~idAnimWebAI
idAnimWebAI::~idAnimWebAI() = default;

// DIA: idAnimWebAI::InternalInitWeb
void idAnimWebAI::InternalInitWeb() {
  static const char* names[] = {
      "blendBack",          "blendRight",       "blendAngle",
      "fusionMoveAngle",    "forwardRunIndex",  "leftRunIndex",
      "rightRunIndex",      "backRunIndex",     "idleIndex",
      "previousIdleIndex",  "injuredRunOffset", "deathAnimIndex",
      "previousRunIndex",   "forwardRunBlend",  "forwardRunBlendLinear",
      "startTransitionIndex", "stopTransitionIndex", "aimAlpha",
      "gangstaAlpha",       "walkRate",         "runRate",
      "desiredTurn",        "sync8index1",      "sync8index2",
      "sync8blendAngle",    "select01",         "traversalAttack",
      "gestureIndex",       "transitionSelect", "toCombatTurn",
      "toSearchTurn",       "meleeAttack",      "turnLeanDirection",
      "turnLeanAmount",     "aimBlendBack",     "aimBlendRight",
      "runSpeedScale",      "aimBlendAngle",    "scriptBlend"};
  blendVariableNames.assign(std::begin(names), std::end(names));
}

// DIA: idAnimWebAI::InternalFinishBlend
void idAnimWebAI::InternalFinishBlend(const idAnimWebAINode* previousNode,
                                      const idAnimWebAINode* currentNode,
                                      int currentTime) {
  (void)previousNode;
  (void)currentNode;
  (void)currentTime;
  lockdownBlendAngle = false;
}

// DIA: idAnimWebAI::idAnimWebAI
idAnimWebAI::idAnimWebAI() {
  runIndices.fill(-1.0F);
  idleIndices.fill(-1.0F);
  runIndices[ANIMWEBAI_RUNINDEXTYPE_NORMAL] = 0.0F;
  idleIndices[ANIMWEBAI_RUNINDEXTYPE_NORMAL] = 0.0F;
}

// DIA: idAnimWebAI::`scalar deleting destructor'

void idAnimWebAI::SetOrientation(const idVec3& newBodyForward,
                                 const idVec3& bodyIdealDirection,
                                 const idVec3& newMoveForward,
                                 const idVec3& moveIdealDirection) {
  bodyForward = newBodyForward;
  bodyIdeal = bodyIdealDirection;
  moveForward = newMoveForward;
  moveIdeal = moveIdealDirection;
}

// DIA: idAnimWebAI::InternalStartPath
void idAnimWebAI::InternalStartPath() {
  idVec3 normalizedBody = bodyForward;
  idVec3 normalizedBodyIdeal = bodyIdeal;
  idVec3 normalizedMove = moveForward;
  idVec3 normalizedMoveIdeal = moveIdeal;
  normalizedBody.Normalize();
  normalizedBodyIdeal.Normalize();
  normalizedMove.Normalize();
  normalizedMoveIdeal.Normalize();
  bodyTurnDot = normalizedBody * normalizedBodyIdeal;
  movementTurnDot = normalizedMove * normalizedMoveIdeal;
}

// DIA: EaseInEaseOut
float EaseInEaseOut(float linearInput) {
  if (linearInput >= 0.5F) {
    const float inverse = 1.0F - (linearInput - 0.5F) * 2.0F;
    return (1.0F - inverse * inverse) * 0.5F + 0.5F;
  }
  const float doubled = linearInput * 2.0F;
  return doubled * doubled * 0.5F;
}

// DIA: idAnimWebAI::SetForwardRunIndex
void idAnimWebAI::SetForwardRunIndex(float newRunIndex, float newIdleIndex,
                                     runIndexType_t type, bool blend,
                                     bool forced) {
  if (!forced && forwardRunBlendLinear < 1.0F) {
    nextRunIndex = newRunIndex;
    nextIdleIndex = newIdleIndex;
    nextRunIndexType = type;
    return;
  }
  previousRunIndex = forwardRunIndex;
  previousIdleIndex = idleIndex;
  forwardRunIndex = newRunIndex;
  idleIndex = newIdleIndex;
  nextRunIndex = newRunIndex;
  nextIdleIndex = newIdleIndex;
  runIndexType = type;
  nextRunIndexType = type;
  forwardRunBlendLinear = blend && previousRunIndex != newRunIndex ? 0.0F : 1.0F;
  forwardRunBlend = EaseInEaseOut(forwardRunBlendLinear);
}

// DIA: idAnimWebAI::InternalGetEdgeCost
int idAnimWebAI::InternalGetEdgeCost(int nodeIndex, int edgeIndex,
                                     bool randomEdgeCost) {
  (void)nodeIndex;
  (void)edgeIndex;
  if (!randomEdgeCost) return 100;
  randomSeed = 1664525u * randomSeed + 1013904223u;
  return 100 - static_cast<int>(((randomSeed >> 10) & 0x7fffu) % 10u);
}

float idAnimWebAI::ClassifyTurn(const idVec3& desired,
                               const idVec3& forward) {
  idVec3 normalizedDesired = desired;
  idVec3 normalizedForward = forward;
  normalizedDesired.Normalize();
  normalizedForward.Normalize();
  const float dot = normalizedDesired * normalizedForward;
  if (dot >= 0.70710671F) return 0.0F;
  if (dot < -0.70710671F) return 3.0F;
  const idVec3 left{-normalizedForward.y, normalizedForward.x, 0.0F};
  return (left * normalizedDesired) > 0.0F ? 1.0F : 2.0F;
}

// DIA: idAnimWebAI::UpdateDesiredTurnIndex
void idAnimWebAI::UpdateDesiredTurnIndex() {
  desiredTurn = ClassifyTurn(desiredDirection, bodyForward);
  transitionSelect = desiredTurn;
}

// DIA: idAnimWebAI::CalcToCombatTurn
void idAnimWebAI::CalcToCombatTurn(const idVec3& direction) {
  const idEntity* entity = ai;
  const idVec3 forward = entity != nullptr && entity->GetPresentableAxis() != nullptr
                             ? (*entity->GetPresentableAxis())[0]
                             : bodyForward;
  toCombatTurn = ClassifyTurn(direction, forward);
}

// DIA: idAnimWebAI::CalcToSearchTurn
void idAnimWebAI::CalcToSearchTurn(const idVec3& direction) {
  const idEntity* entity = ai;
  const idVec3 forward = entity != nullptr && entity->GetPresentableAxis() != nullptr
                             ? (*entity->GetPresentableAxis())[0]
                             : bodyForward;
  toSearchTurn = ClassifyTurn(direction, forward);
}

void idAnimWebAI::SetRunIndices(runIndexType_t type, float runIndex,
                                float idleValue) {
  if (type < 0 || type >= ANIMWEBAI_RUNINDEXTYPE_MAX) return;
  runIndices[type] = runIndex;
  idleIndices[type] = idleValue;
}

// DIA: idAnimWebAI::SetAnimIndicesForRunType
void idAnimWebAI::SetAnimIndicesForRunType(int type, bool blend) {
  if (type < 0 || type >= ANIMWEBAI_RUNINDEXTYPE_MAX ||
      runIndices[type] < 0.0F) {
    return;
  }
  SetForwardRunIndex(runIndices[type], idleIndices[type],
                     static_cast<runIndexType_t>(type), blend, false);
}

// DIA: idAnimWebAI::SerializeSnapshot
void idAnimWebAI::SerializeSnapshot(idSerializer* serializer) {
  if (serializer == nullptr) return;
  float* values[] = {&blendBack,       &blendRight,      &blendAngle,
                     &fusionMoveAngle, &forwardRunIndex, &leftRunIndex,
                     &rightRunIndex,   &backRunIndex,    &idleIndex,
                     &previousIdleIndex, &previousRunIndex, &forwardRunBlend,
                     &forwardRunBlendLinear, &aimAlpha, &gangstaAlpha,
                     &walkRate, &runRate, &desiredTurn, &transitionSelect,
                     &toCombatTurn, &toSearchTurn, &turnLeanDirection,
                     &turnLeanAmount, &runSpeedScale, &scriptBlend};
  for (float* value : values) {
    if (serializer->writing) {
      serializer->words.push_back(FloatBits(*value));
    } else if (serializer->cursor < serializer->words.size()) {
      *value = BitsFloat(serializer->words[serializer->cursor++]);
    }
  }
}

// DIA: idAnimWebAI::GetDestinationIndices
bool idAnimWebAI::GetDestinationIndices(int* subWebIndex,
                                        int* stateIndex) const {
  if (subWebIndex == nullptr || stateIndex == nullptr) return false;
  int subWeb = destinationSubWebIndex;
  int state = destinationStateIndex;
  if (subWeb < 0 || state < 0) {
    subWeb = pathDestinationSubWebIndex;
    state = pathDestinationStateIndex;
  }
  if (subWeb < 0 || state < 0) return false;
  *subWebIndex = subWeb;
  *stateIndex = state;
  return true;
}

// DIA: idAnimWebAI::DebugDrawNode
int idAnimWebAI::DebugDrawNode(idRenderModelGui* gui, idAnimWebAINode* node,
                               float nodeAlpha, float offset) {
  if (node == nullptr) return 0;
  int rows = 0;
  if (!node->animationName.empty()) {
    if (gui != nullptr) {
      gui->DrawAnimation(node->animationName.c_str(), nodeAlpha, offset);
    }
    ++rows;
  }
  for (idAnimWebAINode* child : node->children) {
    rows += DebugDrawNode(gui, child, nodeAlpha,
                          offset + static_cast<float>(rows) * 50.0F);
  }
  return rows;
}

// DIA: idAnimWebAI::InternalUpdateScalars
void idAnimWebAI::InternalUpdateScalars(int currentTime) {
  const int elapsed = lastUpdateTime == 0 ? 16 : std::max(currentTime - lastUpdateTime, 0);
  lastUpdateTime = currentTime;
  const float frameScale = static_cast<float>(elapsed) / 16.0F;
  if (forwardRunBlendLinear < 1.0F) {
    forwardRunBlendLinear =
        std::min(1.0F, forwardRunBlendLinear +
                          ai_walkLerpScale.GetFloat() * frameScale);
    forwardRunBlend = EaseInEaseOut(forwardRunBlendLinear);
    if (forwardRunBlendLinear >= 1.0F && nextRunIndex != forwardRunIndex) {
      SetForwardRunIndex(nextRunIndex, nextIdleIndex, nextRunIndexType, true,
                         true);
    }
  }
  aimAlpha = Approach(aimAlpha, targetAimAlpha,
                      ai_aimLerpScale.GetFloat() * frameScale);
  gangstaAlpha = Approach(gangstaAlpha, targetGangstaAlpha,
                          ai_gangstaLerpScale.GetFloat() * frameScale);
  const float runScale = targetRunSpeedScale > runSpeedScale
                             ? ai_runSpeedScaleUp.GetFloat()
                             : ai_runSpeedScaleDown.GetFloat();
  runSpeedScale = Approach(runSpeedScale, targetRunSpeedScale,
                           runScale * frameScale);
}

// DIA: idAnimWebAI::InternalStartBlend
void idAnimWebAI::InternalStartBlend(const idAnimWebAINode* currentNode,
                                     const idAnimWebAINode* nextNode,
                                     int currentTime) {
  if (nextNode != nullptr) InternalSetAnimDelta(0, nextNode);
  UpdateDesiredTurnIndex();
  if (currentNode != nullptr && (currentNode->flags & 1) != 0) {
    lockdownBlendAngle = true;
    lockdownBlendAngleStartTime = currentTime;
    blendBack = 0.0F;
    blendRight = 0.0F;
    blendAngle = 0.0F;
  }
}

// DIA: idAnimWebAI::DebugDrawNodes
void idAnimWebAI::DebugDrawNodes(idRenderModelGui* gui) {
  DebugDrawNode(gui, debugRoot, 1.0F, 0.0F);
}

// DIA: idAnimWebAI::InternalSetAnimDelta
void idAnimWebAI::InternalSetAnimDelta(int modelIndex,
                                       const idAnimWebAINode* node) {
  if (modelIndex != 0 || node == nullptr) return;
  switch (node->animDelta) {
    case 0:
      movementMode = 0;
      break;
    case 1:
    case 2:
      movementMode = 2;
      break;
    case 3:
      movementMode = 3;
      break;
    case 4:
      movementMode = 4;
      break;
    case 6:
      movementMode = 6;
      break;
    default:
      break;
  }
  if (node->animDelta != 7) worldCollision = node->animDelta != 2;
}

// DIA: `dynamic initializer for 'ai_walkLerpScale''
// DIA: `dynamic initializer for 'ai_gangstaLerpScale''
// DIA: `dynamic initializer for 'ai_aimLerpScale''
// DIA: `dynamic initializer for 'ai_animWebDebug''
// DIA: `dynamic initializer for 'ai_debugWeb''
// DIA: `dynamic initializer for 'ai_deathAnimScale''
// DIA: `dynamic initializer for 'ai_debugtransitiondirs''
// DIA: `dynamic initializer for 'ai_stairWalkRate''
// DIA: `dynamic initializer for 'ai_stairRunRate''
// DIA: `dynamic initializer for 'ai_debugForwardRunIndex''
// DIA: `dynamic initializer for 'ai_runCycleBlendMode''
// DIA: `dynamic initializer for 'ai_turnLeanLerpScaleUp''
// DIA: `dynamic initializer for 'ai_turnLeanLerpScaleDown''
// DIA: `dynamic initializer for 'ai_runSpeedScaleUp''
// DIA: `dynamic initializer for 'ai_runSpeedScaleDown''

// DIA: `dynamic atexit destructor for 'ai_walkLerpScale''
// DIA: `dynamic atexit destructor for 'ai_gangstaLerpScale''
// DIA: `dynamic atexit destructor for 'ai_aimLerpScale''
// DIA: `dynamic atexit destructor for 'ai_animWebDebug''
// DIA: `dynamic atexit destructor for 'ai_debugWeb''
// DIA: `dynamic atexit destructor for 'ai_deathAnimScale''
// DIA: `dynamic atexit destructor for 'ai_debugtransitiondirs''
// DIA: `dynamic atexit destructor for 'ai_stairWalkRate''
// DIA: `dynamic atexit destructor for 'ai_stairRunRate''
// DIA: `dynamic atexit destructor for 'ai_debugForwardRunIndex''
// DIA: `dynamic atexit destructor for 'ai_runCycleBlendMode''
// DIA: `dynamic atexit destructor for 'ai_turnLeanLerpScaleUp''
// DIA: `dynamic atexit destructor for 'ai_turnLeanLerpScaleDown''
// DIA: `dynamic atexit destructor for 'ai_runSpeedScaleUp''
// DIA: `dynamic atexit destructor for 'ai_runSpeedScaleDown''
