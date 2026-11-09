#pragma once

#include "engine/gamelib/effectphysics/effectphysicsbroadphase.h"
#include "engine/gamelib/effectphysics/effectphysicsrigidbody.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/random.h"

#include <cstddef>
#include <cstdint>

class idClip;
class idDeclBreakable;
class idEffectPhysicsProperties;
class idRenderModelDiscreteAnimation;
class idRenderModelEffects;
class idSoundWorld;
class idTraceModel;
class idTraceModelCache;
class idDeclImpactSound;

class idEffectPhysicsPieceEmitter {
public:
  class idBreakablePiece {
  public:
    __forceinline idBreakablePiece()
        : rigidBody(nullptr),
          collision{vec3_origin, vec3_origin, 0},
          emitTime(0), firstCollisionTime(0) {}
    __forceinline ~idBreakablePiece() noexcept(false) {
      if (rigidBody != nullptr) {
        idEffectPhysicsRigidBody* const body = rigidBody;
        body->~idEffectPhysicsRigidBody();
        idMem::Singleton()->Free(body, ALIGN_16);
      }
    }

    idEffectPhysicsRigidBody* rigidBody;
    idEffectPhysicsCollision collision;
    int emitTime;
    int firstCollisionTime;
  };

  struct pieceEmitterSound_t {
    __forceinline pieceEmitterSound_t()
        : soundWorld(nullptr), impactTable(nullptr), nextSoundTime(0),
          minBounceVelSqr(0.0f) {}

    idSoundWorld* soundWorld;
    const idDeclImpactSound* impactTable;
    int nextSoundTime;
    float minBounceVelSqr;
  };

  idEffectPhysicsPieceEmitter();
  ~idEffectPhysicsPieceEmitter() noexcept(false);

  void Init(idClip* clip, idTraceModelCache* traceModelCache,
            idRenderModelDiscreteAnimation* model,
            idRenderModelEffects* modelEffects,
            pieceEmitterSound_t* soundInfo, int entityNumber);
  void Shutdown() {}
  void Reset();
  void EmitPiece(int emitTime, const idVec3& position,
                 const idMat3& axes, const idVec3& impulsePoint,
                 const idVec3& impulse);
  void UpdateSimulation(const idVec3& origin, const idMat3& axis,
                        int currentTime, int gameMsPerFrame);
  void UpdateModel(const idVec3& activeDrawScale3D);
  void SubmitCollisionQueries(bool worldCollisionModelOnly);
  void Hide();
  void Show();
  void DrawCollisionModels(int single, const idVec3& offset,
                           const idMat3& axis);

  void SetPieceLifeTime(int lifeTime) { pieceLifeTime = lifeTime; }
  void SetPieceFriction(float friction) { pieceFriction = friction; }
  float PieceMass() const { return pieceMass; }
  void SetPieceCollisionVolumeDecay(float decay) {
    pieceCollisionVolumeDecay = decay;
  }
  int GetNumPiecesInUse() const { return numPiecesInUse; }
  int GetNumPieces() const { return pieces.num; }
  bool PieceHasSettled(int index) const {
    return pieces.list[index].rigidBody->settled;
  }
  bool PieceIsActive(int index) const {
    return pieces.list[index].rigidBody->active;
  }
  idEffectPhysicsBroadPhase* GetBroadPhase() { return &broadPhase; }
  void HideOverride() { hiddenOverride = true; }
  void ShowOverride() { hiddenOverride = false; }

private:
  __declspec(noinline) idEffectPhysicsProperties* GetPropertiesForTraceModel(
      const idTraceModel& traceModel);
  __declspec(noinline) void UpdateProperties(
      idEffectPhysicsProperties* properties);

public:
  idList<idEffectPhysicsProperties*, 59> properties;
  idList<idBreakablePiece, 59> pieces;
  idEffectPhysicsBroadPhase broadPhase;
  idRandom random;
  float pieceMass;
  float pieceCollisionVolumeDecay;
  idRenderModelDiscreteAnimation* model;
  const idDeclBreakable* decl;
  idRenderModelEffects* modelEffects;
  pieceEmitterSound_t soundInfo;
  int pieceLifeTime;
  float pieceFriction;
  int pieceIndex;
  int numPiecesInUse;
  bool hidden;
  bool hiddenOverride;

  std::uint8_t tailPadding[6];
};

static_assert(sizeof(idEffectPhysicsPieceEmitter::idBreakablePiece) == 0x30,
              "breakable physics piece ABI drift");
static_assert(offsetof(idEffectPhysicsPieceEmitter::idBreakablePiece,
                       collision) == 0x08,
              "breakable collision offset drift");
static_assert(offsetof(idEffectPhysicsPieceEmitter::idBreakablePiece,
                       emitTime) == 0x24,
              "breakable emission-time offset drift");
static_assert(sizeof(idEffectPhysicsPieceEmitter::pieceEmitterSound_t) ==
                  0x18,
              "pieceEmitterSound_t ABI drift");
static_assert(offsetof(idEffectPhysicsPieceEmitter, properties) == 0x00,
              "piece emitter properties ABI drift");
static_assert(offsetof(idEffectPhysicsPieceEmitter, pieces) == 0x18,
              "piece emitter pieces ABI drift");
static_assert(offsetof(idEffectPhysicsPieceEmitter, broadPhase) == 0x30,
              "piece emitter broadphase ABI drift");
static_assert(offsetof(idEffectPhysicsPieceEmitter, random) == 0xA0,
              "piece emitter random ABI drift");
static_assert(offsetof(idEffectPhysicsPieceEmitter, model) == 0xB0,
              "piece emitter model ABI drift");
static_assert(offsetof(idEffectPhysicsPieceEmitter, soundInfo) == 0xC8,
              "piece emitter sound ABI drift");
static_assert(offsetof(idEffectPhysicsPieceEmitter, pieceLifeTime) == 0xE0,
              "piece emitter lifetime ABI drift");
static_assert(offsetof(idEffectPhysicsPieceEmitter, numPiecesInUse) == 0xEC,
              "piece emitter in-use count ABI drift");
static_assert(sizeof(idEffectPhysicsPieceEmitter) == 0xF8,
              "idEffectPhysicsPieceEmitter ABI drift");
