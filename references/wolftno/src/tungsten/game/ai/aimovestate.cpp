#include "tungsten/game/ai/aimovestate.h"

#include <algorithm>
#include <cmath>
#include <limits>

#define MOVE_CVAR(name, value) \
  idCVar name(#name, value, 1, "AI movement reconstruction control")

MOVE_CVAR(ai_debugMove, "0");
MOVE_CVAR(ai_testPaths, "0");
MOVE_CVAR(ai_avoidObstacles, "1");
MOVE_CVAR(ai_useObstacleRoute, "1");
MOVE_CVAR(ai_forceMoveSpeed, "0");
MOVE_CVAR(ai_turndecay, "5");
MOVE_CVAR(ai_turndecayAng, "45");
MOVE_CVAR(ai_pointTolerance, "8");
MOVE_CVAR(ai_useTurningCircles, "1");
MOVE_CVAR(ai_showTurningCircles, "0");
MOVE_CVAR(ai_turningCircleDebugTime, "1000");
MOVE_CVAR(ai_turningCircleAlignmentTolerance, "10");
MOVE_CVAR(ai_debugLerp, "0");
MOVE_CVAR(ai_debugDeltas, "0");
MOVE_CVAR(ai_debugReactionBasedMovement, "0");
MOVE_CVAR(ai_checkStuck, "1");
MOVE_CVAR(ai_debugStuck, "0");
MOVE_CVAR(ai_noSmoothAfterStuckTime, "1000");
MOVE_CVAR(ai_errorCorrectTurns, "1");
MOVE_CVAR(ai_showMovementTrails, "0");
MOVE_CVAR(ai_cornerCircleRadius, "48");
MOVE_CVAR(ai_minCornerCircleRadius, "8");
MOVE_CVAR(ai_zeroVelocityFramesBeforeStuck, "15");
MOVE_CVAR(ai_useStartTransitions, "1");
MOVE_CVAR(ai_useStopTransitions, "1");
MOVE_CVAR(ai_allowTranstionOvershoot, "0");
MOVE_CVAR(ai_alignToPointDist, "64");
MOVE_CVAR(ai_debugPushPlayer, "0");
MOVE_CVAR(ai_forceMicrotransitions, "0");
MOVE_CVAR(ai_pathResolver, "1");
MOVE_CVAR(ai_aaspush, "1");

#undef MOVE_CVAR

namespace {

idVec3 EntityOrigin(const idEntity* entity) {
  return entity != nullptr && entity->GetPhysics() != nullptr
             ? entity->GetPhysics()->GetOrigin()
             : idVec3{};
}

}  // namespace

// DIA: idAIMoveState::ReachedPoint
bool idAIMoveState::ReachedPoint(const idVec3& point, float tolerance) const {
  return (point - currentOrigin).LengthSqr() <= tolerance * tolerance;
}

// DIA: idAIMoveState::EndTurn
void idAIMoveState::EndTurn() { turning = false; }

// DIA: idAIMoveState::SetOverrideAnimOrientationWithFocus
void idAIMoveState::SetOverrideAnimOrientationWithFocus(bool value) {
  overrideAnimOrientationWithFocus = value;
}

// DIA: idAIMoveState::IsOverridingAnimOrientationWithFocus
bool idAIMoveState::IsOverridingAnimOrientationWithFocus() const {
  return overrideAnimOrientationWithFocus;
}

// DIA: idAIMoveState::GetDestinationTraversal
idScenePoint* idAIMoveState::GetDestinationTraversal() const {
  return destinationTraversal;
}

// DIA: idAIMoveState::SetMoveStatus
void idAIMoveState::SetMoveStatus(aiMoveStatus_t value) { moveStatus = value; }

// DIA: idAIMoveState::Init
void idAIMoveState::Init(idAI2* value) {
  owner = value;
  currentOrigin = EntityOrigin(value);
  previousOrigin = currentOrigin;
  ResetStuck();
  Stop(MOVE_STATUS_DONE);
}

// DIA: idAIMoveState::Stop
aiMoveStatus_t idAIMoveState::Stop(aiMoveStatus_t reason) {
  moveStatus = reason;
  currentVelocity = {};
  pushVelocity = {};
  lerping = false;
  smoothBlend = false;
  traversalPlaying = false;
  turning = false;
  return reason;
}

// DIA: idAIMoveState::PointReachableAreaNum
int idAIMoveState::PointReachableAreaNum(const idVec3& point) const {
  return PointReachable(point) ? 1 : 0;
}

// DIA: idAIMoveState::PointReachable
bool idAIMoveState::PointReachable(const idVec3& point) const {
  return std::isfinite(point.x) && std::isfinite(point.y) &&
         std::isfinite(point.z) && PointInMovementRange(point);
}

// DIA: idAIMoveState::PointInMovementRange
bool idAIMoveState::PointInMovementRange(const idVec3& point) const {
  return (point - currentOrigin).LengthSqr() <= movementRange * movementRange;
}

// DIA: idAIMoveState::WillReachPoint
bool idAIMoveState::WillReachPoint(const idVec3& point) const {
  return moveStatus == MOVE_STATUS_MOVING && PointReachable(point);
}

// DIA: idAIMoveState::GetReachedEntityDist
float idAIMoveState::GetReachedEntityDist(const idEntity* entity) const {
  return entity != nullptr
             ? std::sqrt((EntityOrigin(entity) - currentOrigin).LengthSqr())
             : std::numeric_limits<float>::max();
}

// DIA: idAIMoveState::ReachedEntity
bool idAIMoveState::ReachedEntity(const idEntity* entity) const {
  return GetReachedEntityDist(entity) <= moveParms.GetGoalTolerance();
}

// DIA: idAIMoveState::WillReachEntity
bool idAIMoveState::WillReachEntity(const idEntity* entity) const {
  return entity != nullptr && WillReachPoint(EntityOrigin(entity));
}

// DIA: idAIMoveState::PathToPoint
bool idAIMoveState::PathToPoint(const idVec3& point) {
  if (!PointReachable(point)) return false;
  nextPoint = point;
  finalPoint = point;
  return true;
}

// DIA: idAIMoveState::SetMovementModeForReal
void idAIMoveState::SetMovementModeForReal(aiMovementMode_t value) {
  movementMode = value;
}

// DIA: idAIMoveState::BeginTurn
void idAIMoveState::BeginTurn(float yaw) {
  turnYaw = yaw;
  turning = true;
}

// DIA: idAIMoveState::BeginTurn
void idAIMoveState::BeginTurn(const idVec3& direction) {
  movementDirection = direction;
  movementDirection.Normalize();
  turnYaw = std::atan2(movementDirection.y, movementDirection.x) * 57.29578F;
  turning = true;
}

// DIA: idAIMoveState::UpdatePushVelocity
void idAIMoveState::UpdatePushVelocity(const idVec3& velocity) {
  pushVelocity = velocity;
}

// DIA: idAIMoveState::StartSmoothBlendLerp_Traversal
void idAIMoveState::StartSmoothBlendLerp_Traversal(
    const idVec3& destination, int duration) {
  traversalPlaying = true;
  StartSmoothBlendLerp(destination, duration);
}

// DIA: idAIMoveState::AbortSmoothBlendLerp
void idAIMoveState::AbortSmoothBlendLerp() {
  lerping = false;
  smoothBlend = false;
}

// DIA: idAIMoveState::GetMovementDelta
idVec3 idAIMoveState::GetMovementDelta() const {
  return currentOrigin - previousOrigin;
}

// DIA: idAIMoveState::UpdateMovement_Dead
void idAIMoveState::UpdateMovement_Dead(int time) { UpdateDeathMovement(time); }

// DIA: idAIMoveState::ResetStuck
void idAIMoveState::ResetStuck() {
  stuckStartTime = -1;
  zeroVelocityFrames = 0;
}

// DIA: idAIMoveState::UpdateStuck
void idAIMoveState::UpdateStuck(int time) {
  if (currentVelocity.LengthSqr() < 0.0001F &&
      moveStatus == MOVE_STATUS_MOVING) {
    if (stuckStartTime < 0) stuckStartTime = time;
    ++zeroVelocityFrames;
  } else {
    ResetStuck();
  }
}

// DIA: idAIMoveState::StartLerp
void idAIMoveState::StartLerp(const idVec3& destination, int startTime,
                              int duration) {
  lerpStart = currentOrigin;
  lerpDestination = destination;
  lerpStartTime = startTime;
  lerpDuration = std::max(1, duration);
  lerping = true;
}

// DIA: idAIMoveState::StartLerpToPoint
void idAIMoveState::StartLerpToPoint(const idVec3& destination,
                                     int startTime, int duration) {
  nextPoint = destination;
  finalPoint = destination;
  StartLerp(destination, startTime, duration);
}

// DIA: idAIMoveState::StartPush
void idAIMoveState::StartPush(const idVec3& velocity, int duration) {
  pushVelocity = velocity;
  pushEndTime = lastUpdateTime + std::max(0, duration);
}

// DIA: idAIMoveState::AlignToNextPoint
bool idAIMoveState::AlignToNextPoint() {
  ++alignToNextPointCallCount;
  if (alignToNextPointOverrideEnabled) {
    return alignToNextPointOverrideResult;
  }

  // The retail routine only takes ownership of body alignment near a move
  // goal with an explicit arrival orientation.  The remaining movement-state
  // port supplies that richer state; these existing fields cover its normal
  // false path and prevent this query from consuming ordinary body turns.
  if (moveStatus == MOVE_STATUS_BLOCKED || (moveParms.GetFlags() & 1) == 0) {
    return false;
  }
  if (owner == nullptr || owner->GetPhysics() == nullptr) return false;
  const idVec3 velocity = owner->GetPhysics()->GetLinearVelocity(0);
  if (velocity.LengthSqr() < 0.00000001F) return false;
  return false;
}

// DIA: idAIMoveState::IsLineInNavGraph
bool idAIMoveState::IsLineInNavGraph(const idVec3& start,
                                     const idVec3& end) const {
  return (end - start).LengthSqr() <= movementRange * movementRange;
}

// DIA: idAIMoveState::InternalTestMoveToPoint
bool idAIMoveState::InternalTestMoveToPoint(const idVec3& point) const {
  return PointReachable(point) && IsLineInNavGraph(currentOrigin, point);
}

// DIA: idAIMoveState::StartPerfectOrigin
void idAIMoveState::StartPerfectOrigin(const idVec3& destination) {
  movementMode = MOVEMENT_PERFECT_ORIGIN;
  finalPoint = destination;
}

// DIA: idAIMoveState::StartPerfectDestination
void idAIMoveState::StartPerfectDestination(const idVec3& destination) {
  StartPerfectOrigin(destination);
}

// DIA: idAIMoveState::UpdateDeathMovement
void idAIMoveState::UpdateDeathMovement(int time) {
  (void)time;
  previousOrigin = currentOrigin;
  currentVelocity *= 0.8F;
  currentOrigin += currentVelocity;
}

// DIA: idAIMoveState::StartSmoothBlendLerp
void idAIMoveState::StartSmoothBlendLerp(const idVec3& destination,
                                         int duration) {
  smoothBlend = true;
  StartLerp(destination, lastUpdateTime, duration);
}

// DIA: idAIMoveState::StartSmoothBlendLerp_SmoothAnim
void idAIMoveState::StartSmoothBlendLerp_SmoothAnim(
    const idVec3& destination, int duration) {
  StartSmoothBlendLerp(destination, duration);
}

// DIA: idAIMoveState::DetermineSmoothStartAnimation
const char* idAIMoveState::DetermineSmoothStartAnimation() const {
  return movementMode == MOVEMENT_FLYING ? "fly_start" : "walk_start";
}

// DIA: idAIMoveState::DetermineSmoothStopAnimation
const char* idAIMoveState::DetermineSmoothStopAnimation() const {
  return movementMode == MOVEMENT_FLYING ? "fly_stop" : "walk_stop";
}

// DIA: idAIMoveState::UpdateMovement_PerfectOrigin
void idAIMoveState::UpdateMovement_PerfectOrigin(int time) {
  (void)time;
  previousOrigin = currentOrigin;
  currentOrigin = finalPoint;
  Stop(MOVE_STATUS_DONE);
}

// DIA: idAIMoveState::UpdateMovement_Player
void idAIMoveState::UpdateMovement_Player(int time) {
  AdvanceLinearMovement(time, 1.0F);
}

// DIA: idAIMoveState::UpdateDirection
void idAIMoveState::UpdateDirection() {
  movementDirection = nextPoint - currentOrigin;
  if (movementDirection.Normalize() == 0.0F) movementDirection = {1, 0, 0};
}

// DIA: idAIMoveState::CheckStuck
bool idAIMoveState::CheckStuck(int time) {
  UpdateStuck(time);
  if (ai_checkStuck.GetInteger() == 0) return false;
  if (zeroVelocityFrames >= ai_zeroVelocityFramesBeforeStuck.GetInteger()) {
    moveStatus = MOVE_STATUS_BLOCKED;
    return true;
  }
  return false;
}

// DIA: idAIMoveState::PushEntityIfClose
bool idAIMoveState::PushEntityIfClose(idEntity* entity, float distance) {
  if (entity == nullptr || GetReachedEntityDist(entity) > distance) return false;
  StartPush(movementDirection * distance, 100);
  return true;
}

// DIA: idAIMoveState::RecentlyFailedMove
bool idAIMoveState::RecentlyFailedMove(const idVec3& destination,
                                       int time) const {
  for (int index = 0; index < moveFailures.Num(); ++index) {
    if ((moveFailures[index].destination - destination).LengthSqr() < 1.0F &&
        time - moveFailures[index].time < 1000) return true;
  }
  return false;
}

// DIA: idAIMoveParms::GetGoalEntity
const idEntity* idAIMoveParms::GetGoalEntity() const { return goalEntity; }

// DIA: idAIMoveParms::MovingToEntity
bool idAIMoveParms::MovingToEntity() const { return goalEntity != nullptr; }

// DIA: idAIMemory::GetActionEntity
idEntity* idAIMemory::GetActionEntity() const { return actionEntity; }

// DIA: idObstacleInfo::Reset
void idObstacleInfo::Reset() {
  wallCorners[0] = {};
  wallCorners[1] = {};
  obstacle = nullptr;
  avoidPoint = {};
  valid = false;
}

// DIA: idAIMoveState::TestMove
bool idAIMoveState::TestMove(const idVec3& destination) const {
  return InternalTestMoveToPoint(destination) &&
         !RecentlyFailedMove(destination, lastUpdateTime);
}

// DIA: idAIMoveState::UpdateMovement_Static
void idAIMoveState::UpdateMovement_Static(int time) {
  (void)time;
  previousOrigin = currentOrigin;
  currentVelocity = {};
}

// DIA: idAIMoveState::UpdateMovement_Walking
void idAIMoveState::UpdateMovement_Walking(int time) {
  AdvanceLinearMovement(time, 1.0F);
}

// DIA: `idAIMoveState::UpdateMovement_Walking'::`1'::dtor$0

// DIA: idAIMoveState::UpdateMovement_Anim
void idAIMoveState::UpdateMovement_Anim(int time) {
  AdvanceLinearMovement(time, 0.75F);
}

// DIA: idAIMoveState::UpdateMovement_Flying
void idAIMoveState::UpdateMovement_Flying(int time) {
  AdvanceLinearMovement(time, 1.25F);
}

// DIA: idAIMoveState::PathAroundObstacles
bool idAIMoveState::PathAroundObstacles() {
  if (!obstacleInfo.valid) return true;
  nextPoint = obstacleInfo.avoidPoint;
  return PointReachable(nextPoint);
}

// DIA: idAIMoveState::GetGoalEntity
idEntity* idAIMoveState::GetGoalEntity() const {
  return const_cast<idEntity*>(moveParms.GetGoalEntity());
}

// DIA: idAI2::GetDefaultMovementMode
int idAI2::GetDefaultMovementMode() const { return MOVEMENT_WALKING; }

// DIA: idAIMoveState::idAIMoveState
idAIMoveState::idAIMoveState() = default;

// DIA: `idAIMoveState::idAIMoveState'::`1'::dtor$0

// DIA: idAIMoveState::Verify_SmoothAnims
bool idAIMoveState::Verify_SmoothAnims() const {
  return !smoothBlend || lerpDuration > 0;
}

// DIA: `idAIMoveState::Verify_SmoothAnims'::`1'::dtor$0
// DIA: `idAIMoveState::Verify_SmoothAnims'::`1'::dtor$1

// DIA: idAIMoveState::ContinueMove
bool idAIMoveState::ContinueMove(int time) {
  if (!Verify() || moveStatus != MOVE_STATUS_MOVING) return false;
  UpdateMovement(time);
  return moveStatus == MOVE_STATUS_MOVING;
}

// DIA: `idAIMoveState::ContinueMove'::`1'::dtor$0
// DIA: `idAIMoveState::ContinueMove'::`1'::dtor$1
// DIA: `idAIMoveState::ContinueMove'::`1'::dtor$2

// DIA: idAIMoveState::UpdateMovement
void idAIMoveState::UpdateMovement(int time) {
  if (lerping) {
    previousOrigin = currentOrigin;
    const float fraction = std::clamp(
        static_cast<float>(time - lerpStartTime) /
            static_cast<float>(std::max(1, lerpDuration)),
        0.0F, 1.0F);
    currentOrigin = lerpStart + (lerpDestination - lerpStart) * fraction;
    if (fraction >= 1.0F) {
      lerping = false;
      smoothBlend = false;
      if (ReachedPoint(finalPoint, moveParms.GetGoalTolerance()))
        moveStatus = MOVE_STATUS_DONE;
    }
    lastUpdateTime = time;
    return;
  }
  switch (movementMode) {
    case MOVEMENT_STATIC: UpdateMovement_Static(time); break;
    case MOVEMENT_WALKING: UpdateMovement_Walking(time); break;
    case MOVEMENT_ANIM: UpdateMovement_Anim(time); break;
    case MOVEMENT_FLYING: UpdateMovement_Flying(time); break;
    case MOVEMENT_DEAD: UpdateMovement_Dead(time); break;
    case MOVEMENT_PLAYER: UpdateMovement_Player(time); break;
    case MOVEMENT_PERFECT_ORIGIN: UpdateMovement_PerfectOrigin(time); break;
  }
  lastUpdateTime = time;
}

// DIA: idAIMoveState::FinishPlayingTraversalAnim
void idAIMoveState::FinishPlayingTraversalAnim() {
  traversalPlaying = false;
  destinationTraversal = nullptr;
}

// DIA: idList<moveFailureInfo_t,5>::Resize
template bool idList<moveFailureInfo_t, 5>::Resize(int);

// DIA: idAIMoveState::Verify
bool idAIMoveState::Verify() const {
  return owner != nullptr && PointReachable(finalPoint) && Verify_SmoothAnims();
}

// DIA: idAIMoveState::StartMove
bool idAIMoveState::StartMove(const idAIMoveParms& parameters,
                              aiMovementMode_t mode, int time) {
  moveParms = parameters;
  finalPoint = parameters.GetRelevantGoalOrigin(owner);
  if (!TestMove(finalPoint)) {
    MoveFailed(finalPoint, MOVE_STATUS_UNREACHABLE, time);
    return false;
  }
  nextPoint = finalPoint;
  movementMode = mode;
  moveStatus = MOVE_STATUS_MOVING;
  lastUpdateTime = time;
  startMoveFrame = gameLocal != nullptr ? gameLocal->GetFrameNum() : 0;
  UpdateDirection();
  ResetStuck();
  return true;
}

// DIA: idList<moveFailureInfo_t,5>::Alloc
template moveFailureInfo_t* idList<moveFailureInfo_t, 5>::Alloc();

// DIA: idAIMoveState::MoveFailed
void idAIMoveState::MoveFailed(const idVec3& destination,
                               aiMoveStatus_t reason, int time) {
  moveStatus = reason;
  moveFailureInfo_t* failure = moveFailures.Alloc();
  *failure = {currentOrigin, destination, time, reason};
  if (moveFailures.Num() > 8) moveFailures.RemoveIndex(0);
}

void idAIMoveState::AdvanceLinearMovement(int time, float speedScale) {
  previousOrigin = currentOrigin;
  const float seconds = std::max(0, time - lastUpdateTime) * 0.001F;
  idVec3 desired = finalPoint - currentOrigin;
  const float distance = std::sqrt(desired.LengthSqr());
  if (distance <= moveParms.GetGoalTolerance()) {
    currentOrigin = finalPoint;
    moveStatus = MOVE_STATUS_DONE;
    currentVelocity = {};
    return;
  }
  desired.Normalize();
  const float forcedSpeed = static_cast<float>(ai_forceMoveSpeed.GetInteger());
  const float speed = forcedSpeed > 0.0F ? forcedSpeed : 100.0F;
  const float step = std::min(distance, speed * speedScale * seconds);
  currentVelocity = desired * (speed * speedScale) + pushVelocity;
  currentOrigin += desired * step + pushVelocity * seconds;
  if (step >= distance && pushVelocity.LengthSqr() < 0.0001F) {
    currentOrigin = finalPoint;
    currentVelocity = {};
    moveStatus = MOVE_STATUS_DONE;
  }
  if (time >= pushEndTime) pushVelocity = {};
  CheckStuck(time);
}

// DIA: `dynamic initializer for 'ai_debugMove''
// DIA: `dynamic initializer for 'ai_testPaths''
// DIA: `dynamic initializer for 'ai_avoidObstacles''
// DIA: `dynamic initializer for 'ai_useObstacleRoute''
// DIA: `dynamic initializer for 'ai_forceMoveSpeed''
// DIA: `dynamic initializer for 'ai_turndecay''
// DIA: `dynamic initializer for 'ai_turndecayAng''
// DIA: `dynamic initializer for 'ai_pointTolerance''
// DIA: `dynamic initializer for 'ai_useTurningCircles''
// DIA: `dynamic initializer for 'ai_showTurningCircles''
// DIA: `dynamic initializer for 'ai_turningCircleDebugTime''
// DIA: `dynamic initializer for 'ai_turningCircleAlignmentTolerance''
// DIA: `dynamic initializer for 'ai_debugLerp''
// DIA: `dynamic initializer for 'ai_debugDeltas''
// DIA: `dynamic initializer for 'ai_debugReactionBasedMovement''
// DIA: `dynamic initializer for 'ai_checkStuck''
// DIA: `dynamic initializer for 'ai_debugStuck''
// DIA: `dynamic initializer for 'ai_noSmoothAfterStuckTime''
// DIA: `dynamic initializer for 'ai_errorCorrectTurns''
// DIA: `dynamic initializer for 'ai_showMovementTrails''
// DIA: `dynamic initializer for 'ai_cornerCircleRadius''
// DIA: `dynamic initializer for 'ai_minCornerCircleRadius''
// DIA: `dynamic initializer for 'ai_zeroVelocityFramesBeforeStuck''
// DIA: `dynamic initializer for 'ai_useStartTransitions''
// DIA: `dynamic initializer for 'ai_useStopTransitions''
// DIA: `dynamic initializer for 'ai_allowTranstionOvershoot''
// DIA: `dynamic initializer for 'ai_alignToPointDist''
// DIA: `dynamic initializer for 'ai_debugPushPlayer''
// DIA: `dynamic initializer for 'ai_forceMicrotransitions''
// DIA: `dynamic initializer for 'ai_pathResolver''
// DIA: `dynamic initializer for 'ai_aaspush''
// DIA: `dynamic atexit destructor for 'ai_debugMove''
// DIA: `dynamic atexit destructor for 'ai_testPaths''
// DIA: `dynamic atexit destructor for 'ai_avoidObstacles''
// DIA: `dynamic atexit destructor for 'ai_useObstacleRoute''
// DIA: `dynamic atexit destructor for 'ai_forceMoveSpeed''
// DIA: `dynamic atexit destructor for 'ai_turndecay''
// DIA: `dynamic atexit destructor for 'ai_turndecayAng''
// DIA: `dynamic atexit destructor for 'ai_pointTolerance''
// DIA: `dynamic atexit destructor for 'ai_useTurningCircles''
// DIA: `dynamic atexit destructor for 'ai_showTurningCircles''
// DIA: `dynamic atexit destructor for 'ai_turningCircleDebugTime''
// DIA: `dynamic atexit destructor for 'ai_turningCircleAlignmentTolerance''
// DIA: `dynamic atexit destructor for 'ai_debugLerp''
// DIA: `dynamic atexit destructor for 'ai_debugDeltas''
// DIA: `dynamic atexit destructor for 'ai_debugReactionBasedMovement''
// DIA: `dynamic atexit destructor for 'ai_checkStuck''
// DIA: `dynamic atexit destructor for 'ai_debugStuck''
// DIA: `dynamic atexit destructor for 'ai_noSmoothAfterStuckTime''
// DIA: `dynamic atexit destructor for 'ai_errorCorrectTurns''
// DIA: `dynamic atexit destructor for 'ai_showMovementTrails''
// DIA: `dynamic atexit destructor for 'ai_cornerCircleRadius''
// DIA: `dynamic atexit destructor for 'ai_minCornerCircleRadius''
// DIA: `dynamic atexit destructor for 'ai_zeroVelocityFramesBeforeStuck''
// DIA: `dynamic atexit destructor for 'ai_useStartTransitions''
// DIA: `dynamic atexit destructor for 'ai_useStopTransitions''
// DIA: `dynamic atexit destructor for 'ai_allowTranstionOvershoot''
// DIA: `dynamic atexit destructor for 'ai_alignToPointDist''
// DIA: `dynamic atexit destructor for 'ai_debugPushPlayer''
// DIA: `dynamic atexit destructor for 'ai_forceMicrotransitions''
// DIA: `dynamic atexit destructor for 'ai_pathResolver''
// DIA: `dynamic atexit destructor for 'ai_aaspush''

// Header-emitted move-state accessors used by AI2.
aiMoveStatus_t idAIMoveState::GetMoveStatus() const { return moveStatus; }

const idVec3& idAIMoveState::GetFinalPoint() const { return finalPoint; }

bool idAIMoveState::IsPlayingTraversalAnim() const {
  return traversalPlaying;
}

void idAIMoveState::SetTranslationDeltaScale(const idVec3& scale) {
  translationScale = scale;
}
