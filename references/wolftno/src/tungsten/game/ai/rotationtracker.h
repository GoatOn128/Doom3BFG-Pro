#pragma once

#include "shared/idlib/math/vector.h"

#include <algorithm>
#include <cmath>

// Shared rotation-tracker contract.  Its recovered translation unit is later
// in the alphabetical port; these small inline operations let earlier users
// share the real state model without claiming that file complete.
class idRotationTracker {
public:
  struct range_t {
    float yawLeft{90.0F};
    float yawRight{90.0F};
    float pitchUp{60.0F};
    float pitchDown{60.0F};
  };

  idRotationTracker();
  virtual ~idRotationTracker();

  void Update(const idVec3& referenceDirection, const idVec3& idealDirection,
              const idAngles& rate, float elapsedSeconds,
              bool projectWhenBehind, bool useAcceleration);

  const idVec3& GetTrackedDirection() const { return worldDirection; }
  void GetWorldSpaceDir(const idMat3& referenceAxis, idVec3* output) const;
  void GetWorldSpaceAxis(const idMat3& referenceAxis, idMat3* output) const;
  void GetConstraints(range_t* output) const;
  void SetConstraints(float maxPitchRadians, float maxYawRadians);
  void SetConstraints(const range_t& value);
  const range_t& GetConstraints() const { return constraints; }
  void SetYawOnlyThreshold(float value);
  float GetYawOnlyThreshold() const { return yawOnlyThreshold; }
  float GetVelocity() const { return velocity; }
  bool WasYawClamped() const { return clampedYaw; }
  bool WasPitchClamped() const { return clampedPitch; }

protected:
  idVec3 worldDirection{1.0F, 0.0F, 0.0F};
  range_t constraints;
  float yawOnlyThreshold{0.0F};
  float velocity{0.0F};
  float maxVelocity{1440.0F};
  float acceleration{1440.0F};
  float deceleration{1440.0F};
  bool projectWhenBehind{true};
  bool clampedYaw{false};
  bool clampedPitch{false};
};

void ClampOnAxis(const idVec3& rotationAxis, const idVec3& referenceAxis,
                 const idVec3& idealDirection, float maxDot,
                 float halfMaxDot, idQuat* rotation, float* dot,
                 bool* clamped);
float ScaleForDot(float currentDot, float maxDot, float minimumClamp);
