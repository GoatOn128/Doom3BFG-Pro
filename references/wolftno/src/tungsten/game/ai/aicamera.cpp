#include "tungsten/game/ai/aicamera.h"

#include <algorithm>
#include <cmath>

idCVar aiCam_lerpScale("aiCam_lerpScale", "1", 4, "camera interpolation scale");
idCVar aiCam_scale("aiCam_scale", "0", 4, "camera forward offset");

namespace {

idMat3 AxisFromForward(idVec3 forward) {
  if (forward.Normalize() == 0.0F) forward = {1.0F, 0.0F, 0.0F};
  idVec3 up{0.0F, 0.0F, 1.0F};
  idVec3 left = up.Cross(forward);
  if (left.Normalize() == 0.0F) left = {0.0F, 1.0F, 0.0F};
  up = forward.Cross(left);
  up.Normalize();
  idMat3 result{};
  result[0] = forward;
  result[1] = left;
  result[2] = up;
  return result;
}

}  // namespace

// DIA: idAICamera_Control::~idAICamera_Control
idAICamera_Control::~idAICamera_Control() = default;

// DIA: idAICamera_Control::Stop
void idAICamera_Control::Stop() {
  target = nullptr;
  flying = false;
  drawn = false;
}

// DIA: idAICamera_Control::idAICamera_Control
idAICamera_Control::idAICamera_Control() {
  cameraAxis = AxisFromForward({1.0F, 0.0F, 0.0F});
}

// DIA: idAICamera_Control::`scalar deleting destructor'

// DIA: idAICamera_Control::FlyTo
void idAICamera_Control::FlyTo(const idVec3* endPosition) {
  if (endPosition == nullptr) return;
  flying = true;
  flyDestination = *endPosition;
}

// DIA: idAICamera_Control::Draw
void idAICamera_Control::Draw(idPlayer* player) {
  drawn = target != nullptr && player != nullptr;
}

// DIA: idAICamera_Control::Update
void idAICamera_Control::Update(const usercmd_t* previousCommand,
                                const usercmd_t* currentCommand,
                                bool actionScriptRunning) {
  if (target == nullptr) return;
  const idVec3 targetPosition =
      target->GetPhysics() != nullptr ? target->GetPhysics()->GetOrigin()
                                      : idVec3{};
  idVec3 desiredPosition;
  if (flying) {
    desiredPosition = flyDestination;
    if ((desiredPosition - cameraPosition).LengthSqr() < 0.01F) flying = false;
  } else if (actionScriptRunning) {
    desiredPosition = targetPosition - interactionNormal * distance;
  } else {
    if (previousCommand != nullptr && currentCommand != nullptr) {
      angles.pitch += static_cast<float>(currentCommand->angles[0] -
                                         previousCommand->angles[0]) *
                      0.0054931641F;
      angles.yaw += static_cast<float>(currentCommand->angles[1] -
                                       previousCommand->angles[1]) *
                    0.0054931641F;
      angles.pitch = std::clamp(angles.pitch, -35.0F, 35.0F);
    }
    const float pitch = angles.pitch * 0.0174532925F;
    const float yaw = angles.yaw * 0.0174532925F;
    const idVec3 forward{std::cos(pitch) * std::cos(yaw),
                         std::cos(pitch) * std::sin(yaw), -std::sin(pitch)};
    desiredPosition = targetPosition - forward * distance;
  }
  const float lerp = std::clamp(
      static_cast<float>(aiCam_lerpScale.GetInteger()), 0.0F, 1.0F);
  cameraPosition += (desiredPosition - cameraPosition) * lerp;
  cameraAxis = AxisFromForward(targetPosition - cameraPosition);
  cameraPosition -= cameraAxis[0] *
                    static_cast<float>(aiCam_scale.GetInteger());
}

// DIA: `dynamic initializer for 'aiCam_lerpScale''
// DIA: `dynamic initializer for 'aiCam_scale''
// DIA: `dynamic atexit destructor for 'aiCam_lerpScale''
// DIA: `dynamic atexit destructor for 'aiCam_scale''
