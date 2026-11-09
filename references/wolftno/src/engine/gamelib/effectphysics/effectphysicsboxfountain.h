#pragma once

#include "engine/gamelib/effectphysics/effectphysicsproperties.h"
#include "engine/gamelib/effectphysics/effectphysicsrigidbody.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/random.h"

#include <cstddef>

class idClip;
class idCVar;
class idRenderModelManyBoxes;
class idTraceModelCache;

class idEffectPhysicsBoxFountain {
public:
  idEffectPhysicsBoxFountain();
  ~idEffectPhysicsBoxFountain() noexcept(false);

  void Init(idClip* clip, idTraceModelCache* traceModelCache,
            int entityNumber);
  void Shutdown();
  void Update(int currentTime);
  void SubmitCollisionQueries();
  void ChangeNumRigidBodies(int count);

  idEffectPhysicsBroadPhase broadPhase;
  idEffectPhysicsProperties properties;
  idList<idEffectPhysicsRigidBody*, 59> rigidBodies;
  idRenderModelManyBoxes* manyBoxes;
  int lastFireTime;
  int currentFireRigidBody;
  idVec3 startOrigin;
  idVec3 spawnOrigin;
  idSPObject spawnObject;
  idRandom random;
};

static_assert(sizeof(idEffectPhysicsBoxFountain) == 0x208,
              "effect physics box-fountain ABI drift");
static_assert(offsetof(idEffectPhysicsBoxFountain, properties) == 0x70,
              "box-fountain properties offset drift");
static_assert(offsetof(idEffectPhysicsBoxFountain, rigidBodies) == 0x118,
              "box-fountain rigid-body list offset drift");
static_assert(offsetof(idEffectPhysicsBoxFountain, manyBoxes) == 0x130,
              "box-fountain render model offset drift");
static_assert(offsetof(idEffectPhysicsBoxFountain, startOrigin) == 0x140,
              "box-fountain start-origin offset drift");
static_assert(offsetof(idEffectPhysicsBoxFountain, spawnOrigin) == 0x14C,
              "box-fountain spawn-origin offset drift");
static_assert(offsetof(idEffectPhysicsBoxFountain, spawnObject) == 0x158,
              "box-fountain spawn-object offset drift");
static_assert(offsetof(idEffectPhysicsBoxFountain, random) == 0x200,
              "box-fountain random offset drift");

extern idCVar g_effectPhysicsBoxFountain;
extern idCVar g_effectPhysicsRespawnTime;
