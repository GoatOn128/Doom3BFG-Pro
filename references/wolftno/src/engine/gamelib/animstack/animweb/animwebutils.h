#pragma once

#include "shared/idlib/math/vector.h"

float LerpToWithScale(float cur, float dest, float scale);
float LerpToWithRate(float cur, float dest, float absRate);

bool UpdateWalkBlend(const idVec3& velocity, const idMat3& axis,
                     float lerpScale, float& blendBack, float& blendRight,
                     float& blendAngle, float& sync8index1,
                     float& sync8index2, float& sync8blendAngle,
                     float& fusionMoveAngle);

