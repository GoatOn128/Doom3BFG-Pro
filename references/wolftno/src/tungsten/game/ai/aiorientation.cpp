#include "tungsten/game/ai/aiorientation.h"

#include <algorithm>
#include <cmath>

namespace {

idVec3 Projected(idVec3 vector, const idVec3& normal) {
  vector -= normal * (vector * normal);
  vector.Normalize();
  return vector;
}

idVec3 RotateAroundAxis(const idVec3& vector, idVec3 normal,
                        float degrees) {
  normal.Normalize();
  const float radians = degrees * 0.0174532925F;
  const float cosine = std::cos(radians);
  const float sine = std::sin(radians);
  return vector * cosine + normal.Cross(vector) * sine +
         normal * ((normal * vector) * (1.0F - cosine));
}

}  // namespace

// DIA: idAIOrientation::UpdateStatic
void idAIOrientation::UpdateStatic() {
  ++updateStaticCount;
  previousTurnDir = currentTurnDir;
  currentTurnDir = TURN_NONE;
}

// DIA: idAIOrientation::idAIOrientation
idAIOrientation::idAIOrientation(float minimumRate, float maximumRate)
    : minRate(minimumRate), maxRate(maximumRate) {
  axis[0] = {1.0F, 0.0F, 0.0F};
  axis[1] = {0.0F, 1.0F, 0.0F};
  axis[2] = {0.0F, 0.0F, 1.0F};
}

// DIA: idAIOrientation::Init
void idAIOrientation::Init(const idMat3& value) {
  axis = value;
  idealDir = axis[0];
  lastTurnDelta = 0.0F;
  lastTurnRate = 0.0F;
  previousTurnDir = TURN_NONE;
  currentTurnDir = TURN_NONE;
  turnVelocity = 0.0F;
}

// DIA: idAIOrientation::SetIdealDir
void idAIOrientation::SetIdealDir(idAI2* ai, const idVec3& direction) {
  const idVec3 gravityNormal =
      ai != nullptr && ai->GetPhysics() != nullptr
          ? ai->GetPhysics()->GetGravityNormal()
          : idVec3{0.0F, 0.0F, -1.0F};
  const idVec3 projected = Projected(direction, gravityNormal);
  if (projected.LengthSqr() > 0.001F) idealDir = projected;
}

// DIA: idAIOrientation::SetIdealDirTowards
void idAIOrientation::SetIdealDirTowards(idAI2* ai, const idVec3& point) {
  const idVec3 origin = ai != nullptr && ai->GetPhysics() != nullptr
                            ? ai->GetPhysics()->GetOrigin()
                            : idVec3{};
  SetIdealDir(ai, point - origin);
}

// DIA: idAIOrientation::UpdateFromAnim
void idAIOrientation::UpdateFromAnim(const idMat3& value, float turnRate,
                                     turnDirection_t turnDirection,
                                     bool updateIdeal) {
  previousTurnDir = currentTurnDir;
  currentTurnDir = turnDirection;
  lastTurnRate = turnRate;
  lastTurnDelta = 0.0F;
  axis = value;
  if (updateIdeal) idealDir = axis[0];
}

// DIA: idAIOrientation::CalculateTurnDirection
turnDirection_t idAIOrientation::CalculateTurnDirection(
    const idVec3& normal) const {
  const idVec3 forward = Projected(axis[0], normal);
  const idVec3 left = normal.Cross(forward);
  return (left * idealDir) < 0.0F ? TURN_RIGHT : TURN_LEFT;
}

// DIA: idAIOrientation::CalcTurnRateForTime
float idAIOrientation::CalcTurnRateForTime(
    const idVec3& rotationAxis, const idVec3& destinationDirection,
    float timeSeconds) const {
  if (timeSeconds < 0.0001F) return 0.0F;
  const idVec3 destination = Projected(destinationDirection, rotationAxis);
  const idVec3 forward = Projected(axis[0], rotationAxis);
  const float dot = std::clamp(forward * destination, -1.0F, 1.0F);
  return std::acos(dot) * 57.29578F / timeSeconds;
}

// DIA: idAIOrientation::SetAxis
void idAIOrientation::SetAxis(const idVec3& direction,
                              const idVec3& gravityDirection) {
  idVec3 up = gravityDirection * -1.0F;
  up.Normalize();
  idVec3 forward = Projected(direction, up);
  if (forward.LengthSqr() < 0.001F) forward = axis[0];
  idVec3 left = up.Cross(forward);
  left.Normalize();
  up = forward.Cross(left);
  up.Normalize();
  axis[0] = forward;
  axis[1] = left;
  axis[2] = up;
}

// DIA: idAIOrientation::SetAxis
void idAIOrientation::SetAxis(const idMat3& value) { axis = value; }

// DIA: idAIOrientation::UpdateAxis
turnDirection_t idAIOrientation::UpdateAxis(const idVec3& normal,
                                             float turnRate,
                                             float timeSeconds) {
  ++updateAxisCount;
  lastUpdateNormal = normal;
  lastUpdateRate = turnRate;
  lastUpdateSeconds = timeSeconds;
  lastForcedDirection = TURN_NONE;
  const turnDirection_t result =
      RotateTowardIdeal(normal, turnRate, timeSeconds, TURN_NONE);
  if (result != TURN_NONE) turnVelocity = 0.0F;
  return result;
}

// DIA: idAIOrientation::UpdateBodyAxis
turnDirection_t idAIOrientation::UpdateBodyAxis(
    const idVec3& normal, turnDirection_t turnDirection, float turnRate,
    float timeSeconds) {
  ++updateBodyAxisCount;
  lastUpdateNormal = normal;
  lastUpdateRate = turnRate;
  lastUpdateSeconds = timeSeconds;
  lastForcedDirection = turnDirection;
  return RotateTowardIdeal(normal, turnRate, timeSeconds, turnDirection);
}

// DIA: idAIOrientation::Update
void idAIOrientation::Update(
    const idVec3& velocity, const idVec3& origin,
    const idVec3& destination, const idVec3& gravityDirection,
    float turnRate, float maximumTurnRate, float timeSeconds) {
  const idVec3 forward = Projected(axis[0], gravityDirection);
  const idVec3 ideal = Projected(idealDir, gravityDirection);
  const float dot = std::clamp(forward * ideal, -1.0F, 1.0F);
  const float angle = std::acos(dot) * 57.29578F;
  const float distance = std::sqrt((destination - origin).LengthSqr());
  const float speed = std::sqrt(velocity.LengthSqr());
  float availableTime = speed > 0.001F ? distance / speed : timeSeconds;
  if (availableTime < 0.0001F) availableTime = timeSeconds;
  float requiredRate = availableTime > 0.0001F ? angle / availableTime : angle;
  requiredRate = std::clamp(requiredRate, minRate,
                            std::min(maxRate, maximumTurnRate));
  RotateTowardIdeal(gravityDirection, std::max(turnRate, requiredRate),
                    timeSeconds, TURN_NONE);
}

// DIA: idAIOrientation::SetAxis
void idAIOrientation::SetAxis(idAI2* ai, const idVec3& direction) {
  (void)ai;
  SetAxis(direction, {0.0F, 0.0F, -1.0F});
}

turnDirection_t idAIOrientation::RotateTowardIdeal(
    const idVec3& normal, float turnRate, float timeSeconds,
    turnDirection_t forcedDirection) {
  previousTurnDir = currentTurnDir;
  currentTurnDir = TURN_NONE;
  const idVec3 forward = Projected(axis[0], normal);
  const idVec3 destination = Projected(idealDir, normal);
  const float dot = std::clamp(forward * destination, -1.0F, 1.0F);
  lastTurnDelta = dot;
  if (dot > tolerance) {
    lastTurnRate = 0.0F;
    if (tolerance >= 0.99984771F) SetAxis(idealDir, normal * -1.0F);
    return TURN_NONE;
  }
  currentTurnDir = forcedDirection != TURN_NONE
                       ? forcedDirection
                       : CalculateTurnDirection(normal);
  lastTurnRate = std::clamp(turnRate, minRate, maxRate);
  const float remaining = std::acos(dot) * 57.29578F;
  float step = std::min(remaining, lastTurnRate * timeSeconds);
  if (currentTurnDir == TURN_RIGHT) step = -step;
  const idVec3 rotated = RotateAroundAxis(forward, normal, step);
  SetAxis(rotated, normal * -1.0F);
  return currentTurnDir;
}
