#pragma once

#include "engine/common/surfacetypes.h"
#include "engine/gamelib/physics/physics.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/math/plane.h"

#include <cstddef>
#include <cstdint>

class idCVar;
class idDeclDamage;

enum waterSplash_t : int {
  WATER_ENTERED = 0,
  WATER_EXITED = 1,
};

struct buoyantClipModel_t {
  int physicsId = -1;
  int bodyId = -1;
};

class idBuoyancy {
public:
  void ApplyBuoyancy(idClip* clip, idPhysics* physics, float timeStep,
                     const idDeclDamage* waterDamage,
                     const idVec3& waterCurrent, float waterDensity,
                     float waterViscosity, const idVec3& gravityVector,
                     int clipMask, surfTypes_t surfOverride,
                     bool ignoreSurfaceForMovement);

  idStaticList<buoyantClipModel_t, 128> buoyantClipModels;

private:
  void CalculateBuoyancyWeights(const int* polytopeNumPlanes,
                                const idPlane* polytopePlanes,
                                int numPolytopes,
                                const idClipModel* clipModel,
                                float* const weights);
};

extern idCVar buoy_damping;

template <>
idList<buoyantClipModel_t, 5>&
idList<buoyantClipModel_t, 5>::operator=(
    const idList<buoyantClipModel_t, 5>& other);

static_assert(sizeof(buoyantClipModel_t) == 0x08,
              "buoyant clip-model ABI drift");
static_assert(sizeof(idStaticList<buoyantClipModel_t, 128>) == 0x418,
              "buoyant static-list ABI drift");
static_assert(sizeof(idBuoyancy) == 0x418, "idBuoyancy ABI drift");
