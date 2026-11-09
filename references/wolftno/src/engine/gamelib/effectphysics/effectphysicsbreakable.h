#pragma once

#include "engine/decls/declbreakable.h"
#include "engine/gamelib/effectphysics/effectphysicsbroadphase.h"
#include "engine/gamelib/effectphysics/effectphysicsproperties.h"
#include "engine/gamelib/effectphysics/effectphysicsrigidbody.h"
#include "engine/gamelib/effects/fxmanager_abi.h"
#include "engine/sound/soundchannel.h"
#include "shared/idlib/math/random.h"

#include <cstddef>
#include <cstdint>

class idClip;
class idCollisionModel;
class idDeclImpactSound;
class idDeclParticle;
class idGameLibEffects;
class idMaterial;
class idPhysics;
class idRenderModelDiscreteAnimation;
class idSoundEmitter;
class idSoundWorld;
class idTraceModel;
class idTraceModelCache;

class idEffectPhysicsBreakable {
public:
  struct breakablePieceSound_t {
    breakablePieceSound_t()
        : soundWorld(nullptr), emitter(nullptr), channel(SND_CHANNEL_ANY),
          impactTable(nullptr), nextSoundTime(0), minBounceVelSqr(0.0f) {}

    idSoundWorld* soundWorld;
    idSoundEmitter* emitter;
    soundChannel_t channel;
    std::uint32_t padding14;
    const idDeclImpactSound* impactTable;
    int nextSoundTime;
    float minBounceVelSqr;
  };

  struct idBreakableParticle {
    const idDeclParticle* particle;
    idVec3 pos;
    int startTime;
  };

  struct idSpark {
    const idMaterial* material;
    float velocityScale;
    float size;
  };

  struct idExplosion {
    idBreakableExplosion decl;
    int startTime;
    int fxIndex;
    fxCondition_t fxCondition;
    idVec3 worldPosition;
    bool effectsEnabled;
    bool forceApplied;
    bool localSpace;
    std::uint8_t paddingB3[5];
  };

  struct idTrail {
    struct trailEntry_t {
      int pieceStartTime;
      idVec3 pieceLastParticleDropPos;
    };

    idBreakableTrail decl;
    idList<trailEntry_t, 59> entries;
  };

  struct idDecalFX {
    struct decalEntry_t {
      int pieceStartTime;
      idVec3 lastCollisionPos;
      idVec3 lastCollisionNormal;
      idVec3 pieceLastDecalDropPos;
    };

    idBreakableDecal decl;
    idList<decalEntry_t, 59> entries;
  };

  idEffectPhysicsBreakable();
  ~idEffectPhysicsBreakable() noexcept(false);

  void Init(idClip* clip, idTraceModelCache* traceModelCache,
            idGameLibEffects* effects,
            idRenderModelDiscreteAnimation* renderModel, int currentTime,
            breakablePieceSound_t* pieceSound);
  void Shutdown() { Reset(true); }
  void Reset(bool fullReset);
  void UpdateProperties(idEffectPhysicsProperties* properties);
  void StopExplosionFX(int currentTime);
  void UpdateModel(const idVec3& origin, const idMat3& axis,
                   int gameMillisecondsPerFrame);
  void AddRadiusImpact(int currentTime, const idPhysics* inflictor,
                       const idVec3& position, float radius, float impulse);
  void ApplyImpulseToBody(int bodyIndex, const idPhysics* inflictor,
                          const idVec3& point, const idVec3& impulse);
  bool IsAtRest();
  bool ActivateNoisy(int pieceIndex, const idVec3& origin,
                     const idMat3& axis);
  void ApplyForce(const idVec3& origin, const idMat3& axis,
                  const idVec3& force);
  void DisablePiece(int index);
  void UpdateSimulation(const idVec3& origin, const idMat3& axis,
                        int currentTime, int gameMillisecondsPerFrame,
                        int serverCurrentTime, float attenuation);
  void DrawCollisionModels(int currentTime, const idVec3& origin,
                           const idMat3& axis);
  bool ActivateAllNow(const idVec3& origin, const idMat3& axis);
  bool ActivateAllNow() { return ActivateAllNow(vec3_origin, mat3_identity); }
  void CollideWith(const idVec3& velocity, const idPhysics* first,
                   const idPhysics* second, float collisionScale);
  idEffectPhysicsProperties*
  GetPropertiesForTraceModel(const idTraceModel& traceModel);
  void AddParticle(const idDeclParticle* particle, int startTime,
                   const idVec3& position);
  void EmitSmokeForEachPiece(const idDeclParticle* particle, int startTime);
  void StartExplosions(const idVec3& origin, const idMat3& axis,
                       int currentTime);
  bool ActivateFloatingPieces(const idVec3& origin, const idMat3& axis,
                              float activationChance);
  bool DamagePiece(int currentTime, int pieceIndex, float damage,
                   const idVec3& position);
  void RemoveExplosions();
  void AddRayImpact(int currentTime, const idVec3& origin,
                    const idMat3& axis, const idVec3& position,
                    const idVec3& direction, bool harmless);
  void AddExplosion(const idDeclParticle* particle, int startTime,
                    const idVec3& position, float radius, float impulse,
                    float falloff, float angular, int delay, int duration,
                    bool startNow, int activateAllDelay,
                    float distanceTimeScaleDelay, bool useLinear,
                    const idVec3& endLinear, bool localSpace);
  void InitExplosionsAndTrails(breakablePieceSound_t* pieceSound,
                               idGameLibEffects* effects, float diversity);

  const idVec3& GetExplosionWorldPosition(int index) const {
    return explosions[index].worldPosition;
  }
  const idDeclParticle* GetExplosionParticle(int index) const {
    return explosions[index].decl.particle;
  }
  int GetNumPieces() const { return rigidBodies.num; }
  bool PieceIsActive(int index) const { return rigidBodies[index]->active; }
  bool PieceHasSettled(int index) const { return rigidBodies[index]->settled; }
  idEffectPhysicsRigidBody* GetPieceBody(int index) const {
    return rigidBodies[index];
  }
  idEffectPhysicsBroadPhase* GetBroadPhase() { return &broadPhase; }
  void SubmitCollisionQueries(bool worldCollisionModelOnly) {
    broadPhase.SubmitQueries(worldCollisionModelOnly);
  }
  const idDeclBreakable* GetDeclBreakable() const { return decl; }
  void SetPieceActivationTimes(const idList<int, 5>& times) {
    pieceActivationTimes = times;
  }

  idList<idEffectPhysicsProperties*, 59> properties;
  idList<idEffectPhysicsRigidBody*, 59> rigidBodies;
  idList<idEffectPhysicsCollision, 59> collisions;
  idCollisionModel* basePiece;
  idList<idExplosion, 59> explosions;
  idList<idFXManager, 59> explosionFX;
  idList<idTrail, 59> trails;
  idList<float, 59> damage;
  idList<idBreakableParticle, 59> particles;
  idList<idDecalFX, 59> decals;
  idList<idSpark, 59> sparks;
  idList<int, 5> pieceActivationTimes;
  idEffectPhysicsBroadPhase broadPhase;
  idRandom random;
  std::uint32_t padding184;
  idGameLibEffects* gameLibEffects;
  idRenderModelDiscreteAnimation* model;
  const idDeclBreakable* decl;
  breakablePieceSound_t soundInfo;
};

static_assert(sizeof(idEffectPhysicsBreakable::breakablePieceSound_t) == 0x28,
              "breakable sound ABI drift");
static_assert(sizeof(idEffectPhysicsBreakable::idBreakableParticle) == 0x18,
              "breakable particle ABI drift");
static_assert(sizeof(idEffectPhysicsBreakable::idSpark) == 0x10,
              "breakable spark ABI drift");
static_assert(sizeof(idEffectPhysicsBreakable::idExplosion) == 0xB8,
              "breakable explosion ABI drift");
static_assert(sizeof(idEffectPhysicsBreakable::idTrail::trailEntry_t) == 0x10,
              "breakable trail entry ABI drift");
static_assert(sizeof(idEffectPhysicsBreakable::idTrail) == 0x78,
              "breakable trail ABI drift");
static_assert(sizeof(idEffectPhysicsBreakable::idDecalFX::decalEntry_t) ==
                  0x28,
              "breakable decal entry ABI drift");
static_assert(sizeof(idEffectPhysicsBreakable::idDecalFX) == 0x78,
              "breakable decal ABI drift");
static_assert(offsetof(idEffectPhysicsBreakable, broadPhase) == 0x110,
              "breakable broadphase offset drift");
static_assert(offsetof(idEffectPhysicsBreakable, random) == 0x180,
              "breakable random offset drift");
static_assert(offsetof(idEffectPhysicsBreakable, gameLibEffects) == 0x188,
              "breakable effects offset drift");
static_assert(offsetof(idEffectPhysicsBreakable, soundInfo) == 0x1A0,
              "breakable sound offset drift");
static_assert(sizeof(idEffectPhysicsBreakable) == 0x1C8,
              "breakable manager ABI drift");
