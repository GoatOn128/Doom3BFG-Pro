#pragma once

#include "shared/idlib/math/vector.h"

// Retail ABI recovered from WolfNewOrder_x64_rtm.pdb.  The geometric member
// functions belong to the idlib/bv recovery pass; renderer/debug only needs
// the exact data contract.
struct idCylinder {
  idVec3 origin{};
  float halfHeight{};
  float radius{};

  idCylinder() = default;
  idCylinder(const idVec3& cylinderOrigin, float cylinderHalfHeight,
             float cylinderRadius)
      : origin(cylinderOrigin), halfHeight(cylinderHalfHeight),
        radius(cylinderRadius) {}

  const idVec3& GetOrigin() const { return origin; }
  float GetRadius() const { return radius; }
  float GetHalfHeight() const { return halfHeight; }
  float GetHeight() const { return halfHeight * 2.0f; }
};

static_assert(sizeof(idCylinder) == 20, "idCylinder ABI drift");
