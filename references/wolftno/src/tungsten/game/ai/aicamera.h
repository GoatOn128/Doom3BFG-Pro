#pragma once

#include "tungsten/game/ai/ai2_actions.h"

struct usercmd_t {
  int angles[3]{0, 0, 0};
};

class idAICamera_Control {
public:
  idAICamera_Control();
  virtual ~idAICamera_Control();

  void Stop();
  void FlyTo(const idVec3* endPosition);
  void Draw(idPlayer* player);
  void Update(const usercmd_t* previousCommand, const usercmd_t* currentCommand,
              bool actionScriptRunning);
  void SetTarget(idEntity* value) { target = value; }
  idEntity* GetTarget() const { return target; }
  const idVec3& GetCameraPosition() const { return cameraPosition; }
  const idMat3& GetCameraAxis() const { return cameraAxis; }
  bool IsFlying() const { return flying; }
  bool WasDrawn() const { return drawn; }
  void SetCameraTransformForTest(const idVec3& position,
                                 const idMat3& axis) {
    cameraPosition = position;
    cameraAxis = axis;
  }

private:
  idEntity* target{nullptr};
  bool flying{false};
  bool drawn{false};
  idVec3 flyDestination{};
  idVec3 cameraPosition{};
  idMat3 cameraAxis{};
  idVec3 interactionNormal{1.0F, 0.0F, 0.0F};
  idAngles angles{};
  float distance{128.0F};
};
