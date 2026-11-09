#define WOLFTNO_RETAIL_CUTTABLE_COLLISION_CTOR_ABI
#define WOLFTNO_RETAIL_CUTTABLE_STATICLIST_CTOR_ABI
#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_EMPTY_CTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/effectphysics/effectphysicscuttable.h"

#include "engine/cm/collisionmodel.h"
#include "engine/decls/declsimple.h"
#include "engine/framework/common.h"
#include "engine/gamelib/effectphysics/effectphysicsproperties.h"
#include "engine/models/cuttable/rendermodelcuttabledynamic.h"
#include "engine/models/cuttable/rendermodelcuttableresource.h"
#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "engine/renderer/autorenderparms.h"
#include "engine/sound/soundsystem.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/sys/sys_alloc.h"

#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_EMPTY_CTOR_ABI
#undef WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_CUTTABLE_COLLISION_CTOR_ABI
#undef WOLFTNO_RETAIL_CUTTABLE_STATICLIST_CTOR_ABI

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>

#pragma warning(disable : 4458)

class idCollisionModelManager {
public:
  void DrawCollisionModel(idCollisionModel* model,
                          const idJointMat* modelJoints,
                          const unsigned char* sphereActives,
                          const idVec3& modelOrigin,
                          const idMat3& modelAxis,
                          const idVec3& viewOrigin,
                          const idMat3& viewAxis, float radius, float scale,
                          int lifeTime);
};

class idEntity;
class idScenePoint {
public:
  virtual void ActivateTargets(idEntity* target);
};

extern idCollisionModelManager* collisionModelManager;
extern idCVar cm_depthTest;
extern bool forceIdListsToReallocateEveryAppend;

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

void operator delete[](void* memory, memTag_t tag) noexcept;

__forceinline void* operator new(std::size_t size, memTag_t tag) noexcept {
  return idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size, tag,
      false, ALIGN_16, HEAP_DEFAULTHEAP);
}

__forceinline void operator delete(void* memory, memTag_t tag) noexcept {
  operator delete[](memory, tag);
}

namespace {

constexpr memTag_t TAG_FXPHYSICS_RETAIL = static_cast<memTag_t>(59);

template <typename Type, int Tag>
__forceinline void ClearListExact(idList<Type, Tag>& list) {
  if (list.listStatic == 0 || list.listStatic == 2) {
    if (list.list != nullptr) {
      idMem::Singleton().Free(list.list, ALIGN_16);
    }
    list.list = nullptr;
    list.size = 0;
  }
  list.num = 0;
}

template <typename Type>
__forceinline void AppendOpaqueEight(Type& list, const void* value) {
  reinterpret_cast<idList<void*, 107>&>(list).Append(
      const_cast<void*&>(value));
}

__forceinline bool UsesCommittedTransform(const idRenderModel* model) {
  return (model->useDeferredPosition &
          (model->deferredPositionInitialized >> 1u) & 1u) != 0u;
}

__forceinline bool ParmBlockIsLocked(const idRenderModel* model) {
  return (reinterpret_cast<const std::uint8_t*>(&model->g)[120] & 4u) != 0u;
}

__forceinline void SetFadeParmFlag(idRenderModel* model, bool enabled) {
  std::uint8_t& flags = reinterpret_cast<std::uint8_t*>(&model->g)[118];
  if (enabled) {
    flags = static_cast<std::uint8_t>(flags | 0x20u);
  } else {
    flags = static_cast<std::uint8_t>(flags & ~0x20u);
  }
}

__forceinline void CopyModelTransformOriginFirst(
    idRenderModel* model, const idEffectPhysicsRigidBody* body) {
  if (!UsesCommittedTransform(model)) {
    model->g.origin = body->currentState.positionFiltered;
  }
  model->deferredOrigin = body->currentState.positionFiltered;
  if (!UsesCommittedTransform(model)) {
    model->g.axis = body->currentState.orientationFiltered;
  }
  model->deferredAxis = body->currentState.orientationFiltered;
}

__forceinline void CopyModelTransformAxisFirst(
    idRenderModel* model, const idEffectPhysicsRigidBody* body) {
  if (!UsesCommittedTransform(model)) {
    model->g.axis = body->currentState.orientationFiltered;
  }
  model->deferredAxis = body->currentState.orientationFiltered;
  if (!UsesCommittedTransform(model)) {
    model->g.origin = body->currentState.positionFiltered;
  }
  model->deferredOrigin = body->currentState.positionFiltered;
}

__forceinline void AddRenderModel(idRenderWorld* world,
                                  idRenderModel* model) {
  world->AddRenderModel(model);
}

__forceinline idCollisionModel* GetCachedCollisionModel(
    idTraceModelCache* cache, int traceModelIndex) {
  void** const entries = *reinterpret_cast<void***>(
      reinterpret_cast<std::uint8_t*>(cache) + 0x48);
  return *reinterpret_cast<idCollisionModel**>(
      static_cast<std::uint8_t*>(entries[traceModelIndex]) + 0x48);
}

__forceinline int PlayPositionedShaderDirectly(
    idSoundWorld* world, const idSoundShader* shader,
    const idVec3& origin, const idMat3& axis, float volume) {
  using Function = int (*)(idSoundWorld*, const idSoundShader*,
                           const idVec3&, const idMat3&, float);
  void** const vtable = *reinterpret_cast<void***>(world);
  return reinterpret_cast<Function>(vtable[20])(
      world, shader, origin, axis, volume);
}

}  // namespace

template <>
__declspec(noinline) bool
idList<idEffectPhysicsCuttable::piece_t, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearListExact(*this);
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  if (size > 0 || newSize == granularity) {
    reinterpret_cast<idScenePoint*>(this)->idScenePoint::ActivateTargets(
        reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }
  idEffectPhysicsCuttable::piece_t* const replacement =
      static_cast<idEffectPhysicsCuttable::piece_t*>(
          idListArrayResize<idEffectPhysicsCuttable::piece_t>(
          list, size, newSize, static_cast<memTag_t>(memTag), false));
  list = replacement;
  if (replacement == nullptr) {
    return false;
  }
  size = newSize;
  if (num > newSize) {
    num = newSize;
  }
  return true;
}

template <>
__declspec(noinline) int
idList<idEffectPhysicsCuttable::piece_t, 5>::Append(
    const idEffectPhysicsCuttable::piece_t& value) {
  if (list == nullptr) {
    int newSize = granularity;
    if (newSize <= 0) {
      newSize = 1;
    }
    if (!Resize(newSize)) {
      return -1;
    }
  }
  if (num == size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = size + 1;
    } else if (granularity != 0) {
      const int rounded = size + granularity;
      newSize = rounded - rounded % granularity;
    } else {
      newSize = 2 * size;
      if (newSize < size) {
        newSize = 0x7fffffff;
      } else if (newSize == 0) {
        newSize = 1;
      }
    }
    if (!Resize(newSize)) {
      return -1;
    }
  }
  const int index = num;
  if (index >= size) {
    return index - 1;
  }
  list[index] = value;
  ++num;
  return index;
}

template <>
__declspec(noinline)
idStaticList<idEffectPhysicsCuttable::piece_t, 8>::idStaticList() noexcept {
  size = 8;
  num = 0;
  list = staticList;
  *reinterpret_cast<std::uint32_t*>(&granularity) = 0x01050001u;
}

idCVar g_cuttableUpdatePhysics(
    "g_cuttableUpdatePhysics", "1", CVAR_BOOL,
    "Updates cuttable physics", static_cast<argCompletionRef_t>(nullptr));

idEffectPhysicsCuttable::idEffectPhysicsCuttable()
    : freeModels(), models(), pieces(), broadPhase() {
  *reinterpret_cast<std::uint64_t*>(&pieceCollisionAgeDecay) = 0;
  pieceMaxLifeTime = 30000;
  pieceLifeTime = 5000;
  pieceFadeTime = 2000;
  pieceMass = 8;
}

idEffectPhysicsCuttable::~idEffectPhysicsCuttable() noexcept(false) {
  ClearListExact(freeModels);
  for (int index = 0; index < pieces.num; ++index) {
    piece_t& piece = pieces.list[index];
    if (piece.rigidBody != nullptr) {
      idEffectPhysicsRigidBody* const rigidBody = piece.rigidBody;
      rigidBody->~idEffectPhysicsRigidBody();
      idMem::Singleton().Free(rigidBody, ALIGN_16);
      piece.rigidBody = nullptr;
    }
    if (piece.properties != nullptr) {
      idEffectPhysicsProperties* const properties = piece.properties;
      properties->broadPhase = nullptr;
      idMem::Singleton().Free(properties, ALIGN_16);
      piece.properties = nullptr;
    }
    piece.soundTable = nullptr;
  }
  ClearListExact(pieces);
}

void idEffectPhysicsCuttable::SubmitCollisionQueries() {
  broadPhase.SubmitQueries(false);
}

void idEffectPhysicsCuttable::UpdateModel(const idVec3& scale,
                                          int currentTime) {
  (void)scale;
  const float inverseFadeTime = 1.0f / static_cast<float>(pieceFadeTime);
  for (int index = 0; index < pieces.num; ++index) {
    piece_t& piece = pieces.list[index];
    if (piece.renderModelIndex < 0) {
      continue;
    }
    idRenderModelCuttableDynamic* const model =
        models.list[piece.renderModelIndex];
    if (model == nullptr || !model->IsValid()) {
      continue;
    }
    idEffectPhysicsRigidBody* const body = piece.rigidBody;
    if (piece.timeStamp == 0 || !body->active) {
      continue;
    }
    CopyModelTransformAxisFirst(model, body);
    if (piece.firstCollisionTime != 0) {
      const int fadeAge =
          currentTime - pieceLifeTime - piece.firstCollisionTime;
      if (fadeAge > 0) {
        const float fade = std::max(
            0.0f, static_cast<float>(pieceFadeTime - fadeAge) *
                      inverseFadeTime);
        if (!ParmBlockIsLocked(model)) {
          model->g.modelFade = fade;
          const idDeclRenderParm* const fadeParm =
              static_cast<const idDeclRenderParm*>(rp_modelFade.r);
          if (fade == 1.0f) {
            SetFadeParmFlag(model, false);
            model->gameParmBlock.ClearParm(fadeParm);
          } else {
            SetFadeParmFlag(model, true);
            parmValue_t value;
            value.value[0] = fade;
            value.value[1] = 1.0f;
            value.value[2] = 0.0f;
            value.value[3] = 0.0f;
            model->gameParmBlock.SetParm(fadeParm, value);
          }
        }
      }
    }
    model->CommitThisFrame();
  }
}

void idEffectPhysicsCuttable::DrawCollisionModels(int single) {
  for (int index = 0; index < pieces.num; ++index) {
    if (single >= 0 && index != single) {
      continue;
    }
    piece_t& piece = pieces.list[index];
    idEffectPhysicsRigidBody* const body = piece.rigidBody;
    if (piece.timeStamp == 0 || !body->active) {
      continue;
    }
    const int traceModelIndex = body->properties->traceModelIndex;
    const idVec3 bodyOrigin = body->currentState.positionFiltered;
    const idMat3 bodyAxis = body->currentState.orientationFiltered;
    collisionModelManager->DrawCollisionModel(
        GetCachedCollisionModel(broadPhase.traceModelCache, traceModelIndex),
        nullptr, nullptr, bodyOrigin, bodyAxis, vec3_origin, mat3_identity,
        0.0f, 1.0f, 0);
    common->RW()->GetRenderDebug()->DebugAxis(
        bodyOrigin, bodyAxis, 0, cm_depthTest.valueInteger != 0);
  }
}

void idEffectPhysicsCuttable::Init(idRenderWorld* renderWorld,
                                   idSoundWorld* soundWorldValue,
                                   idClip* clip,
                                   idTraceModelCache* traceModelCache) {
  broadPhase.Init(clip, traceModelCache);
  pieceCollisionAgeDecay = 0.0f;
  pieceMaxLifeTime = 30000;
  pieceLifeTime = 5000;
  pieceFadeTime = 2000;
  pieceFriction = 0.0f;

  const volatile int recoveredPieceCount = 8;
  for (int index = 0; index < recoveredPieceCount; ++index) {
    piece_t piece;
    piece.collision.collisionPoint = vec3_origin;
    piece.collision.collisionNormalVelocity = vec3_origin;
    piece.collision.collisionSurfaceType = 0;
    piece.position = vec3_origin;
    piece.orientation = mat3_identity;
    *reinterpret_cast<std::uint64_t*>(&piece.timeStamp) = 0;
    std::memset(&piece, 0, 24);
    piece.renderModelIndex = -1;
    piece.nextSoundTime = 0;
    piece.minBounceVelocitySqr = 1.0f;
    pieces.Append(piece);
  }

  model_t freeModel;
  freeModel.timeStamp = 0;
  for (int index = 0; index < 10; ++index) {
    idRenderModelCuttableDynamic* renderModel =
        new (static_cast<memTag_t>(TAG_RENDERMODEL))
            idRenderModelCuttableDynamic;
    AddRenderModel(renderWorld, renderModel);
    AppendOpaqueEight(models, renderModel);
    freeModel.index = index;
    void* const packedModel =
        *reinterpret_cast<void* const*>(&freeModel);
    AppendOpaqueEight(freeModels, packedModel);
  }
  soundWorld = soundWorldValue;
}

int idEffectPhysicsCuttable::Emit(
    const cuttablePhysicsEmitInfo_t& emitInfo,
    const idCuttableGenGeometry* genGeometry) {
  static float gravity = -750.0f;
  static float bouncyness = 0.2f;
  static int pieceMass = 8;

  const idMaterial* material = nullptr;
  const idMaterial* renderMaterial = nullptr;
  const idMaterial* depthMaterial = nullptr;
  if (genGeometry->surfaces.num == 1) {
    material = genGeometry->surfaces.list[0].material;
    renderMaterial = material;
  } else if (genGeometry->surfaces.num == 2) {
    depthMaterial = genGeometry->surfaces.list[0].material;
    material = genGeometry->surfaces.list[1].material;
    renderMaterial = material;
  }

  int pieceIndex = 0;
  for (int index = 0; index < pieces.num; ++index) {
    if (pieces.list[index].timeStamp < pieces.list[pieceIndex].timeStamp) {
      pieceIndex = index;
    }
  }

  int freeModelIndex = 0;
  const int numFreeModels = freeModels.num;
  for (int index = 0; index < numFreeModels; ++index) {
    if (freeModels.list[index].timeStamp <
        freeModels.list[freeModelIndex].timeStamp) {
      freeModelIndex = index;
    }
  }
  const int renderModelIndex = freeModels.list[freeModelIndex].index;
  if (freeModelIndex >= 0 && freeModelIndex < numFreeModels) {
    --freeModels.num;
    if (freeModelIndex != freeModels.num) {
      freeModels.list[freeModelIndex] = freeModels.list[freeModels.num];
    }
  }

  piece_t& piece = pieces.list[pieceIndex];
  piece.position = emitInfo.position;
  piece.orientation = emitInfo.orientation;
  piece.timeStamp = emitInfo.emitTime;
  piece.firstCollisionTime = 0;
  piece.soundTable = emitInfo.soundTable;
  piece.minBounceVelocitySqr = emitInfo.minBounceVelocitySqr;
  piece.nextSoundTime = emitInfo.emitTime;
  if (piece.renderModelIndex >= 0) {
    const model_t freeModel{piece.renderModelIndex, emitInfo.emitTime};
    void* const packedModel =
        *reinterpret_cast<void* const*>(&freeModel);
    AppendOpaqueEight(freeModels, packedModel);
  }
  piece.renderModelIndex = renderModelIndex;
  piece.collision.collisionNormalVelocity = vec3_origin;
  piece.collision.collisionPoint = vec3_origin;
  piece.collision.collisionSurfaceType = 0;

  if (piece.rigidBody != nullptr) {
    idEffectPhysicsRigidBody* const oldBody = piece.rigidBody;
    oldBody->~idEffectPhysicsRigidBody();
    idMem::Singleton().Free(oldBody, ALIGN_16);
    piece.rigidBody = nullptr;
  }
  if (piece.properties != nullptr) {
    idEffectPhysicsProperties* const oldProperties = piece.properties;
    oldProperties->broadPhase = nullptr;
    idMem::Singleton().Free(oldProperties, ALIGN_16);
    piece.properties = nullptr;
  }

  const int traceModelIndex = broadPhase.traceModelCache->AllocTraceModel(
      *emitInfo.traceModel, material);
  idEffectPhysicsProperties* const properties =
      new (TAG_FXPHYSICS_RETAIL) idEffectPhysicsProperties;
  properties->Setup(&broadPhase, traceModelIndex,
                    static_cast<float>(pieceMass));
  properties->linearFriction = 0.60000002f;
  properties->angularFriction = 0.60000002f;
  properties->contactFriction = 0.5f;
  properties->linearFrictionWater = 1.0f;
  properties->angularFrictionWater = 1.0f;
  properties->bouncyness = bouncyness;
  properties->gravityVector.x = 0.0f;
  properties->gravityVector.y = 0.0f;
  properties->gravityVector.z = gravity;
  properties->worldCollisionOnly = true;
  properties->simplePointCollision = false;
  properties->crazyBounceChance = 0.0f;
  properties->stopSpeed = 30.0f;
  properties->maxLinearVelocity = 5000.0f;
  properties->maxAngularVelocity = idMath::PI * 4.0f;
  properties->clipMask = 385;

  idEffectPhysicsRigidBody* const body =
      new (TAG_FXPHYSICS_RETAIL) idEffectPhysicsRigidBody;
  body->Setup(properties);
  body->SPObject.entityNum = emitInfo.entityNum;

  const idVec3 physicsPosition{
      ((emitInfo.centerOfMass.y * emitInfo.orientation.mat[1].x) +
       (emitInfo.centerOfMass.x * emitInfo.orientation.mat[0].x)) +
          (emitInfo.centerOfMass.z * emitInfo.orientation.mat[2].x) +
          emitInfo.position.x,
      ((emitInfo.centerOfMass.y * emitInfo.orientation.mat[1].y) +
       (emitInfo.centerOfMass.x * emitInfo.orientation.mat[0].y)) +
          (emitInfo.centerOfMass.z * emitInfo.orientation.mat[2].y) +
          emitInfo.position.y,
      ((emitInfo.centerOfMass.y * emitInfo.orientation.mat[1].z) +
       (emitInfo.centerOfMass.x * emitInfo.orientation.mat[0].z)) +
          (emitInfo.centerOfMass.z * emitInfo.orientation.mat[2].z) +
          emitInfo.position.z};

  body->currentState.position = physicsPosition;
  body->currentState.positionFiltered = physicsPosition;
  body->properties->broadPhase->SetObjectPosition(
      &body->SPObject, body->currentState.position,
      body->currentState.orientation, nullptr);
  body->currentState.orientation = emitInfo.orientation;
  body->currentState.orientationFiltered = emitInfo.orientation;
  body->properties->broadPhase->SetObjectPosition(
      &body->SPObject, body->currentState.position,
      body->currentState.orientation, nullptr);

  body->currentState.linearMomentum.x = vec3_origin.x * properties->mass;
  body->currentState.linearMomentum.y = vec3_origin.y * properties->mass;
  body->currentState.linearMomentum.z = vec3_origin.z * properties->mass;
  const idVec3 angularVelocity = emitInfo.orientation.mat[0];
  body->currentState.angularMomentum.x =
      ((angularVelocity.y * properties->inertiaTensor.mat[1].x) +
       (angularVelocity.x * properties->inertiaTensor.mat[0].x)) +
      (angularVelocity.z * properties->inertiaTensor.mat[2].x);
  body->currentState.angularMomentum.y =
      ((angularVelocity.x * properties->inertiaTensor.mat[0].y) +
       (angularVelocity.y * properties->inertiaTensor.mat[1].y)) +
      (angularVelocity.z * properties->inertiaTensor.mat[2].y);
  body->currentState.angularMomentum.z =
      ((angularVelocity.x * properties->inertiaTensor.mat[0].z) +
       (angularVelocity.y * properties->inertiaTensor.mat[1].z)) +
      (angularVelocity.z * properties->inertiaTensor.mat[2].z);

  if (emitInfo.impulse.LengthSqr() > 0.25f) {
    body->ApplyImpulse(emitInfo.impulsePoint, emitInfo.impulse);
  }
  body->simulationTimeFirstContact = -1.0f;
  body->simulationTime = 0.0f;
  body->active = true;
  body->SPObject.motion.ignore = false;
  piece.properties = properties;
  piece.rigidBody = body;

  idRenderModelCuttableDynamic* const model = models.list[renderModelIndex];
  if (model != nullptr) {
    model->Upload(renderMaterial, depthMaterial, genGeometry->vertices,
                  genGeometry->st, genGeometry->indices);
    if (!ParmBlockIsLocked(model)) {
      model->g.modelFade = 1.0f;
      SetFadeParmFlag(model, false);
      model->gameParmBlock.ClearParm(
          static_cast<const idDeclRenderParm*>(rp_modelFade.r));
    }
    CopyModelTransformOriginFirst(model, body);
    model->CommitThisFrame();
  }
  return pieceIndex;
}

void idEffectPhysicsCuttable::UpdateSimulation(
    const idVec3& origin, const idMat3& axis, int currentTime,
    int gameMsPerFrame) {
  (void)origin;
  (void)axis;
  const float deltaTime = static_cast<float>(gameMsPerFrame) * 0.001f;
  for (int index = 0; index < pieces.num; ++index) {
    piece_t& piece = pieces.list[index];
    idEffectPhysicsRigidBody* const body = piece.rigidBody;
    if (piece.timeStamp == 0 || !body->active) {
      continue;
    }
    const int deathTime =
        piece.firstCollisionTime != 0
            ? piece.firstCollisionTime + pieceLifeTime + pieceFadeTime
            : piece.timeStamp + pieceMaxLifeTime;
    if (currentTime <= deathTime) {
      if (g_cuttableUpdatePhysics.valueInteger != 0) {
        body->Update(deltaTime, piece.collision);
      }
      if (body->settled) {
        if (piece.firstCollisionTime == 0) {
          piece.firstCollisionTime = currentTime;
        }
      } else if ((reinterpret_cast<const std::uint32_t*>(
                       &piece.collision.collisionNormalVelocity)[0] |
                  reinterpret_cast<const std::uint32_t*>(
                       &piece.collision.collisionNormalVelocity)[1] |
                  reinterpret_cast<const std::uint32_t*>(
                       &piece.collision.collisionNormalVelocity)[2]) &
                 0x7fffffffu) {
        if (piece.firstCollisionTime == 0) {
          piece.firstCollisionTime = currentTime;
        }
        const idEffectPhysicsProperties* const properties = body->properties;
        const idVec3 velocity{
            (properties->inverseMass * body->currentState.linearMomentum.x) -
                (deltaTime * properties->gravityVector.x),
            (properties->inverseMass * body->currentState.linearMomentum.y) -
                (deltaTime * properties->gravityVector.y),
            (properties->inverseMass * body->currentState.linearMomentum.z) -
                (deltaTime * properties->gravityVector.z)};
        if (soundWorld != nullptr && piece.soundTable != nullptr &&
            currentTime >= piece.nextSoundTime &&
            velocity.LengthSqr() > piece.minBounceVelocitySqr) {
          const idSoundShader* const impactSound =
              piece.soundTable->GetImpactSoundForMaterial(
                  piece.collision.collisionSurfaceType);
          if (impactSound != nullptr) {
            PlayPositionedShaderDirectly(
                soundWorld, impactSound,
                body->currentState.positionFiltered, mat3_identity, 0.0f);
            piece.nextSoundTime =
                currentTime + piece.soundTable->soundInterval;
          }
        }
      }
    } else {
      body->active = false;
      piece.timeStamp = 0;
      if (piece.renderModelIndex >= 0) {
        idRenderModelCuttableDynamic* const model =
            models.list[piece.renderModelIndex];
        if (model != nullptr && (model->unlinked & 8u) == 0u) {
          model->CommitThisFrame();
          model->UnlinkSubclass();
          model->unlinked =
              static_cast<std::uint8_t>(model->unlinked | 8u);
        }
        const model_t freeModel{piece.renderModelIndex, currentTime};
        void* const packedModel =
            *reinterpret_cast<void* const*>(&freeModel);
        AppendOpaqueEight(freeModels, packedModel);
        piece.renderModelIndex = -1;
      }
    }
  }
}
