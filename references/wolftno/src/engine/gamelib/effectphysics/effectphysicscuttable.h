#pragma once

#include "engine/gamelib/effectphysics/effectphysicsbroadphase.h"
#include "engine/gamelib/effectphysics/effectphysicsrigidbody.h"
#include "shared/idlib/containers/staticlist.h"

#include <cstddef>
#include <cstdint>

class idClip;
class idCuttableGenGeometry;
class idDeclImpactSound;
class idRenderModelCuttableDynamic;
class idRenderWorld;
class idSoundWorld;
class idTraceModel;
class idTraceModelCache;

struct cuttablePhysicsEmitInfo_t {
  cuttablePhysicsEmitInfo_t() = default;

  const idDeclImpactSound* soundTable;
  const idTraceModel* traceModel;
  idMat3 orientation;
  idVec3 position;
  idVec3 impulsePoint;
  idVec3 impulse;
  idVec3 centerOfMass;
  int emitTime;
  int entityNum;
  int infoHandle;
  float minBounceVelocitySqr;
  std::uint32_t recoveredTail;
};

class idEffectPhysicsCuttable {
public:
  struct model_t {
    int index;
    int timeStamp;
  };

  struct piece_t {
    piece_t() noexcept = default;

    idEffectPhysicsProperties* properties;
    idEffectPhysicsRigidBody* rigidBody;
    const idDeclImpactSound* soundTable;
    idEffectPhysicsCollision collision;
    idMat3 orientation;
    idVec3 position;
    int timeStamp;
    int firstCollisionTime;
    int renderModelIndex;
    int nextSoundTime;
    float minBounceVelocitySqr;
  };

  idEffectPhysicsCuttable();
  ~idEffectPhysicsCuttable() noexcept(false);

  void Init(idRenderWorld* renderWorld, idSoundWorld* soundWorld,
            idClip* clip, idTraceModelCache* traceModelCache);
  int Emit(const cuttablePhysicsEmitInfo_t& emitInfo,
           const idCuttableGenGeometry* genGeometry);
  void UpdateSimulation(const idVec3& origin, const idMat3& axis,
                        int currentTime, int gameMsPerFrame);
  void UpdateModel(const idVec3& scale, int currentTime);
  void SubmitCollisionQueries();
  void DrawCollisionModels(int single);

  idStaticList<model_t, 10> freeModels;
  idStaticList<idRenderModelCuttableDynamic*, 10> models;
  idStaticList<piece_t, 8> pieces;
  idEffectPhysicsBroadPhase broadPhase;
  float pieceCollisionAgeDecay;
  float pieceFriction;
  int pieceMaxLifeTime;
  int pieceLifeTime;
  int pieceFadeTime;
  int pieceMass;
  idSoundWorld* soundWorld;
};

static_assert(sizeof(cuttablePhysicsEmitInfo_t) == 0x78,
              "cuttable emit-info ABI drift");
static_assert(offsetof(cuttablePhysicsEmitInfo_t, orientation) == 0x10,
              "cuttable emit orientation offset drift");
static_assert(offsetof(cuttablePhysicsEmitInfo_t, position) == 0x34,
              "cuttable emit position offset drift");
static_assert(offsetof(cuttablePhysicsEmitInfo_t, emitTime) == 0x64,
              "cuttable emit time offset drift");
static_assert(offsetof(cuttablePhysicsEmitInfo_t, minBounceVelocitySqr) ==
                  0x70,
              "cuttable emit bounce offset drift");

static_assert(sizeof(idEffectPhysicsCuttable::model_t) == 0x08,
              "cuttable model slot ABI drift");
static_assert(sizeof(idEffectPhysicsCuttable::piece_t) == 0x78,
              "cuttable piece ABI drift");
static_assert(offsetof(idEffectPhysicsCuttable::piece_t, collision) == 0x18,
              "cuttable collision offset drift");
static_assert(offsetof(idEffectPhysicsCuttable::piece_t, orientation) == 0x34,
              "cuttable orientation offset drift");
static_assert(offsetof(idEffectPhysicsCuttable::piece_t, timeStamp) == 0x64,
              "cuttable timestamp offset drift");
static_assert(offsetof(idEffectPhysicsCuttable::piece_t, renderModelIndex) ==
                  0x6C,
              "cuttable render-model offset drift");

static_assert(sizeof(idEffectPhysicsCuttable) == 0x538,
              "effect cuttable ABI drift");
static_assert(offsetof(idEffectPhysicsCuttable, models) == 0x68,
              "effect cuttable model-list offset drift");
static_assert(offsetof(idEffectPhysicsCuttable, pieces) == 0xD0,
              "effect cuttable piece-list offset drift");
static_assert(offsetof(idEffectPhysicsCuttable, broadPhase) == 0x4A8,
              "effect cuttable broad-phase offset drift");
static_assert(offsetof(idEffectPhysicsCuttable, pieceCollisionAgeDecay) ==
                  0x518,
              "effect cuttable tuning offset drift");
static_assert(offsetof(idEffectPhysicsCuttable, soundWorld) == 0x530,
              "effect cuttable sound-world offset drift");

extern class idCVar g_cuttableUpdatePhysics;
