#pragma once

#include "tungsten/game/ai/aimemory.h"
#include "tungsten/game/ai/aimoveparms.h"
#include "tungsten/game/ai/aiorientation.h"

#include <cstddef>

enum aiMoveStatus_t {
  MOVE_STATUS_DONE,
  MOVE_STATUS_MOVING,
  MOVE_STATUS_BLOCKED,
  MOVE_STATUS_UNREACHABLE,
  MOVE_STATUS_ABORTED,
};

enum aiMovementMode_t {
  AIMOVEMODE_STATIC = 0,
  AIMOVEMODE_GROUND = 1,
  AIMOVEMODE_AIR = 2,
  AIMOVEMODE_DROP = 3,
  AIMOVEMODE_DEAD = 4,
  AIMOVEMODE_ANIM = 5,
  AIMOVEMODE_ANIM_TURN = 6,
  AIMOVEMODE_ANIM_GRAVITY = 7,
  AIMOVEMODE_ANIM_DRIVEN = 8,
  AIMOVEMODE_PLAYER = 9,
  AIMOVEMODE_BOUND = 10,
  AIMOVEMODE_PERFECTORIGIN = 11,
  AIMOVEMODE_MAX = 12,

  // Compatibility names used by the already-ported movement helpers.
  MOVEMENT_STATIC = AIMOVEMODE_STATIC,
  MOVEMENT_WALKING = AIMOVEMODE_GROUND,
  MOVEMENT_FLYING = AIMOVEMODE_AIR,
  MOVEMENT_DEAD = AIMOVEMODE_DEAD,
  MOVEMENT_ANIM = AIMOVEMODE_ANIM,
  MOVEMENT_PLAYER = AIMOVEMODE_PLAYER,
  MOVEMENT_PERFECT_ORIGIN = AIMOVEMODE_PERFECTORIGIN,
};

enum smoothAnimType_t {
  SMOOTH_ANIM_INVALID = 0,
  SMOOTH_ANIM_START = 1,
  SMOOTH_ANIM_TURN = 2,
  SMOOTH_ANIM_STOP = 3,
};

enum aiVelocityType_t {
  AIVELOCITY_SPEED = 0,
  AIVELOCITY_ANIMSPEED = 1,
  AIVELOCITY_ANIMDELTA = 2,
};

struct moveFailureInfo_t {
  idVec3 origin{};
  idVec3 destination{};
  int time{0};
  aiMoveStatus_t status{MOVE_STATUS_UNREACHABLE};
};

struct idObstacleInfo {
  void Reset();
  idVec3 wallCorners[2]{};
  idEntity* obstacle{nullptr};
  idVec3 avoidPoint{};
  bool valid{false};
};

class idAIMoveState {
public:
  idAIMoveState();
  virtual ~idAIMoveState();

  const idVec3& GetFinalPoint() const;
  aiMoveStatus_t GetMoveStatus() const;
  aiMoveStatus_t GetMoveStatus(bool /*checkAlignment*/) const {
    return moveStatus;
  }
  idRecoveredValue GetNextPoint(...);
  bool IsPlayingTraversalAnim() const;
  void SetTranslationDeltaScale(const idVec3& scale);

  bool ReachedPoint(const idVec3& point, float tolerance) const;
  void EndTurn();
  void SetOverrideAnimOrientationWithFocus(bool value);
  bool IsOverridingAnimOrientationWithFocus() const;
  idScenePoint* GetDestinationTraversal() const;
  void SetMoveStatus(aiMoveStatus_t value);
  void Init(idAI2* value);
  aiMoveStatus_t Stop(aiMoveStatus_t reason = MOVE_STATUS_ABORTED);
  int PointReachableAreaNum(const idVec3& point) const;
  bool PointReachable(const idVec3& point) const;
  bool PointInMovementRange(const idVec3& point) const;
  bool WillReachPoint(const idVec3& point) const;
  float GetReachedEntityDist(const idEntity* entity) const;
  bool ReachedEntity(const idEntity* entity) const;
  bool WillReachEntity(const idEntity* entity) const;
  bool PathToPoint(const idVec3& point);
  void SetMovementModeForReal(aiMovementMode_t value);
  void SetVelocityType(aiVelocityType_t value) { velocityType = value; }
  void SetAccelRate(float value) { accelRate = value; }
  void SetDecelRate(float value) { decelRate = value; }
  void SetMaxMoveSpeed(float value) { maxMoveSpeed = value; }
  void BeginTurn(float yaw);
  void BeginTurn(const idVec3& direction);
  void UpdatePushVelocity(const idVec3& velocity);
  void StartSmoothBlendLerp_Traversal(const idVec3& destination,
                                      int duration);
  void AbortSmoothBlendLerp();
  idVec3 GetMovementDelta() const;
  void UpdateMovement_Dead(int time);
  void ResetStuck();
  void UpdateStuck(int time);
  void StartLerp(const idVec3& destination, int startTime, int duration);
  void StartLerpToPoint(const idVec3& destination, int startTime,
                        int duration);
  void StartPush(const idVec3& velocity, int duration);
  bool AlignToNextPoint();
  bool IsLineInNavGraph(const idVec3& start, const idVec3& end) const;
  bool InternalTestMoveToPoint(const idVec3& point) const;
  void StartPerfectOrigin(const idVec3& destination);
  void StartPerfectDestination(const idVec3& destination);
  void UpdateDeathMovement(int time);
  void StartSmoothBlendLerp(const idVec3& destination, int duration);
  void StartSmoothBlendLerp_SmoothAnim(const idVec3& destination,
                                       int duration);
  const char* DetermineSmoothStartAnimation() const;
  const char* DetermineSmoothStopAnimation() const;
  void UpdateMovement_PerfectOrigin(int time);
  void UpdateMovement_Player(int time);
  void UpdateDirection();
  bool CheckStuck(int time);
  bool PushEntityIfClose(idEntity* entity, float distance);
  bool RecentlyFailedMove(const idVec3& destination, int time) const;
  bool TestMove(const idVec3& destination) const;
  void UpdateMovement_Static(int time);
  void UpdateMovement_Walking(int time);
  void UpdateMovement_Anim(int time);
  void UpdateMovement_Flying(int time);
  bool PathAroundObstacles();
  idEntity* GetGoalEntity() const;
  bool Verify_SmoothAnims() const;
  bool ContinueMove(int time);
  void UpdateMovement(int time);
  void FinishPlayingTraversalAnim();
  bool Verify() const;
  bool StartMove(const idAIMoveParms& parameters, aiMovementMode_t mode,
                 int time);
  void MoveFailed(const idVec3& destination, aiMoveStatus_t reason,
                  int time);

  const idVec3& GetCurrentOrigin() const { return currentOrigin; }
  const idVec3& GetCurrentVelocity() const { return currentVelocity; }
  const idVec3& GetFinalPointValue() const { return finalPoint; }
  aiMoveStatus_t GetMoveStatusValue() const { return moveStatus; }
  aiMovementMode_t GetMovementMode() const { return movementMode; }
  aiVelocityType_t GetVelocityType() const { return velocityType; }
  float GetAccelRate() const { return accelRate; }
  float GetDecelRate() const { return decelRate; }
  float GetMaxMoveSpeed() const { return maxMoveSpeed; }
  const idAI2* GetOwnerForTest() const { return owner; }
  bool IsTurning() const { return turning; }
  bool IsLerping() const { return lerping; }
  bool IsTraversalPlaying() const { return traversalPlaying; }
  void SetCurrentOrigin(const idVec3& value) { currentOrigin = value; }
  void SetCurrentVelocity(const idVec3& value) { currentVelocity = value; }
  void SetDestinationTraversal(idScenePoint* value) {
    destinationTraversal = value;
  }
  void SetMovementRange(float value) { movementRange = value; }
  idAIOrientation* GetMoveOrientation() { return &moveOrientation; }
  const idAIOrientation* GetMoveOrientation() const {
    return &moveOrientation;
  }
  const idVec3& GetTranslationDeltaScale() const { return translationScale; }
  int GetStartMoveFrame() const { return startMoveFrame; }
  int GetRemainingTransitionFrames() const {
    return remainingTransitionFrames;
  }
  const idVec3& GetPushVelocityForTest() const { return pushVelocity; }
  int GetPushEndTimeForTest() const { return pushEndTime; }
  void SetLastUpdateTimeForTest(int value) { lastUpdateTime = value; }
  smoothAnimType_t GetCurrentSmoothAnimType() const {
    return currentSmoothAnimType;
  }
  void SetStartMoveFrameForTest(int value) { startMoveFrame = value; }
  void SetRemainingTransitionFramesForTest(int value) {
    remainingTransitionFrames = value;
  }
  void SetCurrentSmoothAnimTypeForTest(smoothAnimType_t value) {
    currentSmoothAnimType = value;
  }
  void SetTraversalPlayingForTest(bool value) { traversalPlaying = value; }
  void ConfigureAlignToNextPointResultForTest(bool enabled, bool result) {
    alignToNextPointOverrideEnabled = enabled;
    alignToNextPointOverrideResult = result;
  }
  int GetAlignToNextPointCallCountForTest() const {
    return alignToNextPointCallCount;
  }
  void ResetAlignToNextPointCallCountForTest() {
    alignToNextPointCallCount = 0;
  }
  const idObstacleInfo* GetObstacleInfo() const { return &obstacleInfo; }
  idObstacleInfo* GetObstacleInfo() { return &obstacleInfo; }
private:
  void AdvanceLinearMovement(int time, float speedScale);

  idAI2* owner{nullptr};
  idAIMoveParms moveParms;
  aiMoveStatus_t moveStatus{MOVE_STATUS_DONE};
  aiMovementMode_t movementMode{MOVEMENT_STATIC};
  aiVelocityType_t velocityType{AIVELOCITY_SPEED};
  idVec3 currentOrigin{};
  idVec3 previousOrigin{};
  idVec3 nextPoint{};
  idVec3 finalPoint{};
  idVec3 currentVelocity{};
  idVec3 pushVelocity{};
  idVec3 movementDirection{1.0F, 0.0F, 0.0F};
  idVec3 lerpStart{};
  idVec3 lerpDestination{};
  int lerpStartTime{0};
  int lerpDuration{0};
  int lastUpdateTime{0};
  int pushEndTime{0};
  int stuckStartTime{-1};
  int zeroVelocityFrames{0};
  int startMoveFrame{0};
  int remainingTransitionFrames{0};
  float movementRange{100000.0F};
  float accelRate{0.0F};
  float decelRate{0.0F};
  float maxMoveSpeed{0.0F};
  float turnYaw{0.0F};
  bool turning{false};
  bool overrideAnimOrientationWithFocus{false};
  bool lerping{false};
  bool smoothBlend{false};
  bool traversalPlaying{false};
  smoothAnimType_t currentSmoothAnimType{SMOOTH_ANIM_INVALID};
  bool alignToNextPointOverrideEnabled{false};
  bool alignToNextPointOverrideResult{false};
  int alignToNextPointCallCount{0};
  idScenePoint* destinationTraversal{nullptr};
  idObstacleInfo obstacleInfo;
  idList<moveFailureInfo_t, 5> moveFailures;
  idAIOrientation moveOrientation;
  idVec3 translationScale{1.0F, 1.0F, 1.0F};
};

class idAIMoveInterface {
public:
  void Bind(idAIMoveState* value) { state = value; }
  idAIMoveState* operator->() { return state; }
  const idAIMoveState* operator->() const { return state; }
  operator idAIMoveState*() { return state; }
  operator const idAIMoveState*() const { return state; }
  bool operator!=(std::nullptr_t) const { return state != nullptr; }

private:
  idAIMoveState* state{nullptr};
};

extern idCVar ai_debugMove;
extern idCVar ai_testPaths;
extern idCVar ai_avoidObstacles;
extern idCVar ai_useObstacleRoute;
extern idCVar ai_forceMoveSpeed;
extern idCVar ai_turndecay;
extern idCVar ai_turndecayAng;
extern idCVar ai_pointTolerance;
extern idCVar ai_useTurningCircles;
extern idCVar ai_showTurningCircles;
extern idCVar ai_turningCircleDebugTime;
extern idCVar ai_turningCircleAlignmentTolerance;
extern idCVar ai_debugLerp;
extern idCVar ai_debugDeltas;
extern idCVar ai_debugReactionBasedMovement;
extern idCVar ai_checkStuck;
extern idCVar ai_debugStuck;
extern idCVar ai_noSmoothAfterStuckTime;
extern idCVar ai_errorCorrectTurns;
extern idCVar ai_showMovementTrails;
extern idCVar ai_cornerCircleRadius;
extern idCVar ai_minCornerCircleRadius;
extern idCVar ai_zeroVelocityFramesBeforeStuck;
extern idCVar ai_useStartTransitions;
extern idCVar ai_useStopTransitions;
extern idCVar ai_allowTranstionOvershoot;
extern idCVar ai_alignToPointDist;
extern idCVar ai_debugPushPlayer;
extern idCVar ai_forceMicrotransitions;
extern idCVar ai_pathResolver;
extern idCVar ai_aaspush;
