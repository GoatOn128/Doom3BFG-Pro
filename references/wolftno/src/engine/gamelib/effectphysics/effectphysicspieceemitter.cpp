#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_DELETECONTENTS_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_MUTABLE_IDCOLOR_ABI
#define WOLFTNO_RETAIL_PIECEEMITTER_IDJOINTMAT_CLASS_ABI
#include "engine/gamelib/effectphysics/effectphysicspieceemitter.h"

#pragma warning(push)
#pragma warning(disable : 4099)
#include "engine/decls/declbreakable.h"
#include "engine/decls/declsimple.h"
#include "engine/gamelib/effectphysics/effectphysicsproperties.h"
#include "engine/models/rendermodel.h"
#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "engine/sound/soundsystem.h"
#include "shared/idlib/geometry/tracemodel.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/sys/sys_alloc.h"
#pragma warning(pop)

#undef WOLFTNO_RETAIL_PIECEEMITTER_IDJOINTMAT_CLASS_ABI
#undef WOLFTNO_RETAIL_MUTABLE_IDCOLOR_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_DELETECONTENTS_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI

#include <cstdint>
#include <cstring>
#include <new>
#include <xmmintrin.h>

#pragma warning(disable : 4458)

class idRenderModelDiscreteAnimation {
public:
  void Update();
  void GetPieceTransform(int index, idVec3& position,
                         idMat3& orientation) const;
  void SetPieceTransform(int index, const idVec3& position,
                         const idMat3& orientation);
  void CollapsePiece(int index);
  void GetPieceTraceModel(int index, idTraceModel& traceModel);
  void Reset();
};

class idCollisionModel;
class idEntity;
class idJointMat;

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

class idScenePoint {
public:
  virtual void ActivateTargets(idEntity* target);
};

extern idCollisionModelManager* collisionModelManager;
idRenderDebug* GetGameLibRenderDebug();

template <typename Type>
void idListArrayDelete(void* pointer, int count);

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

using BreakablePiece = idEffectPhysicsPieceEmitter::idBreakablePiece;
using PieceList = idList<BreakablePiece, 59>;

void ClearPieceList(PieceList& list) {
  if (list.listStatic == 0 || list.listStatic == 2) {
    if (list.list != nullptr) {
      idListArrayDelete<BreakablePiece>(list.list, list.size);
    }
    list.list = nullptr;
    list.size = 0;
  }
  list.num = 0;
}

void AppendOpaquePointer(idList<idEffectPhysicsProperties*, 59>& list,
                         idEffectPhysicsProperties* value) {
  reinterpret_cast<idList<void*, 107>&>(list).Append(
      reinterpret_cast<void* const&>(value));
}

idRenderModel* AsRenderModel(idRenderModelDiscreteAnimation* model) {
  return reinterpret_cast<idRenderModel*>(model);
}

const idRenderModel* AsRenderModel(
    const idRenderModelDiscreteAnimation* model) {
  return reinterpret_cast<const idRenderModel*>(model);
}

const idDeclBreakable* GetBreakableDecl(
    const idRenderModelDiscreteAnimation* model) {
  return *reinterpret_cast<const idDeclBreakable* const*>(
      reinterpret_cast<const std::uint8_t*>(model) + 0x888);
}

int GetTransformCount(const idRenderModelDiscreteAnimation* model) {
  return *reinterpret_cast<const int*>(
      reinterpret_cast<const std::uint8_t*>(model) + 0x8D8);
}

const idMaterial* GetFirstSurfaceMaterial(
    const idRenderModelDiscreteAnimation* model) {
  return AsRenderModel(model)->surfaces.list[0].material;
}

idCollisionModel* GetCachedCollisionModel(idTraceModelCache* cache,
                                          int traceModelIndex) {
  void** const entries = *reinterpret_cast<void***>(
      reinterpret_cast<std::uint8_t*>(cache) + 0x48);
  return *reinterpret_cast<idCollisionModel**>(
      static_cast<std::uint8_t*>(entries[traceModelIndex]) + 0x48);
}

void UnlinkRenderModel(idRenderModelDiscreteAnimation* model) {
  if (model == nullptr) {
    return;
  }
  idRenderModel* const renderModel = AsRenderModel(model);
  if ((renderModel->unlinked & 8u) != 0u) {
    return;
  }
  renderModel->CommitThisFrame();
  using RenderModelFunction = void (*)(idRenderModel*);
  RenderModelFunction* const vtable =
      *reinterpret_cast<RenderModelFunction**>(renderModel);
  vtable[7](renderModel);
  renderModel->unlinked =
      static_cast<std::uint8_t>(renderModel->unlinked | 8u);
}

void DeleteRenderModel(idRenderModelDiscreteAnimation* model) {
  if (model == nullptr) {
    return;
  }
  idRenderModel* const renderModel = AsRenderModel(model);
  renderModel->CommitThisFrame();
  UnlinkRenderModel(model);
  using RenderModelFunction = void (*)(idRenderModel*);
  RenderModelFunction* const vtable =
      *reinterpret_cast<RenderModelFunction**>(renderModel);
  vtable[8](renderModel);
  renderModel->deleteOnSync =
      static_cast<std::uint8_t>(renderModel->deleteOnSync | 4u);
}

int PlayPositionedShaderDirectly(idSoundWorld* world,
                                 const idSoundShader* shader,
                                 const idVec3& origin,
                                 const idMat3& axis, float volume) {
  using Function = int (*)(idSoundWorld*, const idSoundShader*,
                           const idVec3&, const idMat3&, float);
  void** const vtable = *reinterpret_cast<void***>(world);
  return reinterpret_cast<Function>(vtable[20])(
      world, shader, origin, axis, volume);
}

void SetBodyPosition(idEffectPhysicsRigidBody* body,
                     const idVec3& position) {
  body->currentState.position = position;
  body->currentState.positionFiltered = position;
  body->properties->broadPhase->SetObjectPosition(
      &body->SPObject, body->currentState.position,
      body->currentState.orientation, nullptr);
}

void SetBodyOrientation(idEffectPhysicsRigidBody* body,
                        const idMat3& orientation) {
  body->currentState.orientation = orientation;
  body->currentState.orientationFiltered = orientation;
  body->properties->broadPhase->SetObjectPosition(
      &body->SPObject, body->currentState.position,
      body->currentState.orientation, nullptr);
}

void ZeroBodyMomentum(idEffectPhysicsRigidBody* body) {
  const idEffectPhysicsProperties* const properties = body->properties;
  const float mass = properties->mass;
  body->currentState.linearMomentum.x = vec3_origin.x * mass;
  body->currentState.linearMomentum.y = vec3_origin.y * mass;
  body->currentState.linearMomentum.z = vec3_origin.z * mass;

  body->currentState.angularMomentum.x =
      ((vec3_origin.y * properties->inertiaTensor.mat[1].x) +
       (vec3_origin.x * properties->inertiaTensor.mat[0].x)) +
      (vec3_origin.z * properties->inertiaTensor.mat[2].x);
  body->currentState.angularMomentum.y =
      ((vec3_origin.y * properties->inertiaTensor.mat[1].y) +
       (vec3_origin.x * properties->inertiaTensor.mat[0].y)) +
      (vec3_origin.z * properties->inertiaTensor.mat[2].y);
  body->currentState.angularMomentum.z =
      ((vec3_origin.y * properties->inertiaTensor.mat[1].z) +
       (vec3_origin.x * properties->inertiaTensor.mat[0].z)) +
      (vec3_origin.z * properties->inertiaTensor.mat[2].z);
}

bool HasNonZeroBits(const idVec3& value) {
  const auto* const words =
      reinterpret_cast<const std::uint32_t*>(&value);
  return ((words[0] | words[1] | words[2]) & 0x7FFFFFFFu) != 0u;
}

void ApplyAngularFriction(idEffectPhysicsRigidBody* body,
                          float deltaTime, float friction) {
  const idEffectPhysicsProperties* const properties = body->properties;
  const idVec3& momentum = body->currentState.angularMomentum;
  const idMat3& inverseInertia = properties->inverseInertiaTensor;

  idVec3 angularVelocity;
  angularVelocity.x =
      ((momentum.y * inverseInertia.mat[1].x) +
       (momentum.x * inverseInertia.mat[0].x)) +
      (momentum.z * inverseInertia.mat[2].x);
  angularVelocity.y =
      ((momentum.x * inverseInertia.mat[0].y) +
       (momentum.y * inverseInertia.mat[1].y)) +
      (momentum.z * inverseInertia.mat[2].y);
  angularVelocity.z =
      ((momentum.x * inverseInertia.mat[0].z) +
       (momentum.y * inverseInertia.mat[1].z)) +
      (momentum.z * inverseInertia.mat[2].z);

  if (!HasNonZeroBits(angularVelocity)) {
    return;
  }

  const float lengthSquared =
      ((angularVelocity.y * angularVelocity.y) +
       (angularVelocity.x * angularVelocity.x)) +
      (angularVelocity.z * angularVelocity.z);
  const __m128 clampedLengthSquared = _mm_max_ps(
      _mm_set_ss(lengthSquared), idMath::vector_float_smallestNonDenorm);
  const float inverseLength =
      _mm_cvtss_f32(_mm_div_ps(_mm_set1_ps(1.0f),
                               _mm_sqrt_ps(clampedLengthSquared)));
  const float reducedLengthCandidate =
      (inverseLength * lengthSquared) - (deltaTime * friction);
  const float reducedLength = _mm_cvtss_f32(
      _mm_max_ss(_mm_set_ss(reducedLengthCandidate), _mm_setzero_ps()));

  const float velocityX = (angularVelocity.x * inverseLength) * reducedLength;
  const float velocityY = (angularVelocity.y * inverseLength) * reducedLength;
  const float velocityZ = (angularVelocity.z * inverseLength) * reducedLength;
  const idMat3& inertia = properties->inertiaTensor;

  body->currentState.angularMomentum.x =
      ((velocityY * inertia.mat[1].x) +
       (velocityX * inertia.mat[0].x)) +
      (velocityZ * inertia.mat[2].x);
  body->currentState.angularMomentum.y =
      ((velocityY * inertia.mat[1].y) +
       (velocityX * inertia.mat[0].y)) +
      (velocityZ * inertia.mat[2].y);
  body->currentState.angularMomentum.z =
      ((velocityY * inertia.mat[1].z) +
       (velocityX * inertia.mat[0].z)) +
      (velocityZ * inertia.mat[2].z);
}

}  // namespace

template <>
__declspec(noinline) idList<BreakablePiece, 59>::~idList() {
  ClearPieceList(*this);
}

template <>
__declspec(noinline) bool idList<BreakablePiece, 59>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearPieceList(*this);
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  if (size > 0 || newSize == granularity) {
    reinterpret_cast<idScenePoint*>(this)->idScenePoint::ActivateTargets(
        reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }
  list = static_cast<BreakablePiece*>(idListArrayResize<BreakablePiece>(
      list, size, newSize, static_cast<memTag_t>(memTag), false));
  if (list == nullptr) {
    return false;
  }
  size = newSize;
  if (num > newSize) {
    num = newSize;
  }
  return true;
}

idEffectPhysicsPieceEmitter::idEffectPhysicsPieceEmitter()
    : properties(), pieces(), broadPhase(), random(0), pieceMass(10.0f),
      pieceCollisionVolumeDecay(0.0f), model(nullptr), decl(nullptr),
      modelEffects(nullptr), soundInfo(), pieceLifeTime(0),
      pieceFriction(0.0f), pieceIndex(0), numPiecesInUse(0), hidden(false),
      hiddenOverride(false) {}

idEffectPhysicsPieceEmitter::~idEffectPhysicsPieceEmitter() noexcept(false) {
  if (model != nullptr) {
    for (int index = 0; index < properties.num; ++index) {
      broadPhase.traceModelCache->FreeTraceModel(
          properties.list[index]->traceModelIndex);
    }
    DeleteRenderModel(model);
  }
  ClearPieceList(pieces);
  properties.DeleteContents();
}

void idEffectPhysicsPieceEmitter::SubmitCollisionQueries(
    bool worldCollisionModelOnly) {
  broadPhase.SubmitQueries(worldCollisionModelOnly);
}

void idEffectPhysicsPieceEmitter::Show() { hidden = false; }

void idEffectPhysicsPieceEmitter::UpdateProperties(
    idEffectPhysicsProperties* props) {
  props->linearFriction = decl->settings.linearFriction;
  props->angularFriction = decl->settings.angularFriction;
  props->contactFriction = decl->settings.contactFriction;
  props->linearFrictionWater = decl->settings.linearFrictionWater;
  props->angularFrictionWater = decl->settings.angularFrictionWater;
  props->bouncyness = decl->settings.bouncyness;
  props->gravityVector = decl->settings.gravityVector;
  props->worldCollisionOnly = decl->settings.worldCollisionOnly;
  props->simplePointCollision = decl->settings.simplePointCollision;
  props->crazyBounceChance = decl->settings.crazyBounceChance;
  props->stopSpeed = decl->settings.stopSpeed;
  props->maxLinearVelocity = decl->settings.maxLinearVelocity;
  props->maxAngularVelocity = decl->settings.maxAngularVelocity;
  props->clipMask = decl->settings.clipMask;
  props->maxSimulationTime = static_cast<int>(decl->settings.maxSimulationTime);
  props->maxLinearVelocity = decl->settings.maxLinearVelocity;
  props->maxAngularVelocity = decl->settings.maxAngularVelocity;
}

void idEffectPhysicsPieceEmitter::EmitPiece(
    int emitTime, const idVec3& position, const idMat3& axes,
    const idVec3& impulsePoint, const idVec3& impulse) {
  idBreakablePiece& piece = pieces.list[pieceIndex];
  idEffectPhysicsRigidBody* const body = piece.rigidBody;
  if (!body->active) {
    ++numPiecesInUse;
  }
  hiddenOverride = false;
  piece.emitTime = emitTime;
  piece.firstCollisionTime = 0;
  SetBodyPosition(body, position);
  body->SetOrientation(axes);
  ZeroBodyMomentum(body);

  const idVec3 point{
      position.x + impulsePoint.x,
      impulsePoint.y + position.y,
      impulsePoint.z + position.z,
  };
  body->ApplyImpulse(point, impulse);
  body->simulationTimeFirstContact = -1.0f;
  body->simulationTime = 0.0f;
  body->active = true;
  body->SPObject.motion.ignore = false;

  if (pieceIndex >= pieces.num - 1) {
    pieceIndex = 0;
  } else {
    ++pieceIndex;
  }
}

void idEffectPhysicsPieceEmitter::UpdateModel(
    const idVec3& activeDrawScale3D) {
  idMat3 scaleMatrix;
  scaleMatrix.mat[0] = {activeDrawScale3D.x, 0.0f, 0.0f};
  scaleMatrix.mat[1] = {0.0f, activeDrawScale3D.y, 0.0f};
  scaleMatrix.mat[2] = {0.0f, 0.0f, activeDrawScale3D.z};

  for (int index = 0; index < pieces.num; ++index) {
    idEffectPhysicsRigidBody* const body = pieces.list[index].rigidBody;
    if (body->active) {
      const idMat3 orientation =
          body->currentState.orientationFiltered * scaleMatrix;
      model->SetPieceTransform(index, body->currentState.positionFiltered,
                               orientation);
    } else {
      model->CollapsePiece(index);
    }
  }
  if (model != nullptr && !hidden && !hiddenOverride) {
    model->Update();
  }
}

void idEffectPhysicsPieceEmitter::Hide() {
  hidden = true;
  UnlinkRenderModel(model);
}

void idEffectPhysicsPieceEmitter::Reset() {
  if (model == nullptr) {
    return;
  }

  model->Reset();
  random.SetSeed(0);
  pieceIndex = 0;
  numPiecesInUse = 0;

  for (int index = 0; index < pieces.num; ++index) {
    idBreakablePiece& piece = pieces.list[index];
    piece.emitTime = 0;
    piece.firstCollisionTime = 0;

    idVec3 position;
    idMat3 orientation;
    model->GetPieceTransform(index, position, orientation);

    idEffectPhysicsRigidBody* const body = piece.rigidBody;
    body->active = false;
    SetBodyPosition(body, position);
    SetBodyOrientation(body, orientation);
    ZeroBodyMomentum(body);
  }

  for (int index = 0; index < properties.num; ++index) {
    UpdateProperties(properties.list[index]);
  }
  hiddenOverride = true;
  UnlinkRenderModel(model);
}

void idEffectPhysicsPieceEmitter::UpdateSimulation(
    const idVec3& origin, const idMat3& axis, int currentTime,
    int gameMsPerFrame) {
  (void)origin;
  (void)axis;
  const float deltaTime = static_cast<float>(gameMsPerFrame) * 0.001f;

  for (int index = 0; index < pieces.num; ++index) {
    idBreakablePiece& piece = pieces.list[index];
    idEffectPhysicsRigidBody* const body = piece.rigidBody;
    if (!body->active) {
      continue;
    }

    if (currentTime >= piece.emitTime + pieceLifeTime) {
      piece.emitTime = 0;
      piece.firstCollisionTime = 0;
      body->active = false;
      --numPiecesInUse;
      if (numPiecesInUse == 0) {
        hiddenOverride = true;
        UnlinkRenderModel(model);
      }
      continue;
    }

    body->Update(deltaTime, piece.collision);
    if (body->settled ||
        !HasNonZeroBits(piece.collision.collisionNormalVelocity)) {
      continue;
    }

    if (piece.firstCollisionTime == 0) {
      piece.firstCollisionTime = currentTime;
    }

    const idEffectPhysicsProperties* const props = body->properties;
    const float velocityX =
        (props->inverseMass * body->currentState.linearMomentum.x) -
        (deltaTime * props->gravityVector.x);
    const float velocityZ =
        (props->inverseMass * body->currentState.linearMomentum.z) -
        (deltaTime * props->gravityVector.z);

    if (soundInfo.soundWorld != nullptr && soundInfo.impactTable != nullptr &&
        currentTime >= soundInfo.nextSoundTime) {
      const float velocityY =
          (props->inverseMass * body->currentState.linearMomentum.y) -
          (deltaTime * props->gravityVector.y);
      const float velocityLengthSquared =
          ((velocityY * velocityY) + (velocityX * velocityX)) +
          (velocityZ * velocityZ);
      if (velocityLengthSquared > soundInfo.minBounceVelSqr) {
        const idSoundShader* const impactSound =
            soundInfo.impactTable->GetImpactSoundForMaterial(
                piece.collision.collisionSurfaceType);
        if (impactSound != nullptr) {
          float volume = 0.0f;
          if (pieceCollisionVolumeDecay > 0.0f) {
            volume =
                (static_cast<float>(currentTime - piece.firstCollisionTime) *
                 -0.001f) *
                pieceCollisionVolumeDecay;
          }
          PlayPositionedShaderDirectly(
              soundInfo.soundWorld, impactSound,
              body->currentState.positionFiltered,
              body->currentState.orientationFiltered, volume);
          soundInfo.nextSoundTime =
              currentTime + soundInfo.impactTable->soundInterval;
        }
      }
    }

    ApplyAngularFriction(body, deltaTime, pieceFriction);
  }
}

void idEffectPhysicsPieceEmitter::DrawCollisionModels(
    int single, const idVec3& offset, const idMat3& axis) {
  for (int index = 0; index < pieces.num; ++index) {
    if (single >= 0 && index != single) {
      continue;
    }

    idEffectPhysicsRigidBody* const body = pieces.list[index].rigidBody;
    const int traceModelIndex = body->properties->traceModelIndex;
    idVec3 rigidBodyOrigin = body->currentState.positionFiltered;
    idMat3 rigidBodyAxis = body->currentState.orientationFiltered;
    if (!body->active) {
      const float transformedX =
          ((rigidBodyOrigin.y * axis.mat[1].x) +
           (rigidBodyOrigin.x * axis.mat[0].x)) +
          (rigidBodyOrigin.z * axis.mat[2].x) + offset.x;
      const float transformedY =
          ((rigidBodyOrigin.y * axis.mat[1].y) +
           (rigidBodyOrigin.x * axis.mat[0].y)) +
          (rigidBodyOrigin.z * axis.mat[2].y) + offset.y;
      const float transformedZ =
          ((rigidBodyOrigin.y * axis.mat[1].z) +
           (rigidBodyOrigin.x * axis.mat[0].z)) +
          (rigidBodyOrigin.z * axis.mat[2].z) + offset.z;
      rigidBodyOrigin = {transformedX, transformedY, transformedZ};
      rigidBodyAxis *= axis;
    }

    collisionModelManager->DrawCollisionModel(
        GetCachedCollisionModel(broadPhase.traceModelCache, traceModelIndex),
        nullptr, nullptr, rigidBodyOrigin, rigidBodyAxis, vec3_origin,
        mat3_identity, 0.0f, 1.0f, 0);

    if (GetGameLibRenderDebug() != nullptr) {
      idRenderDebug* const renderDebug = GetGameLibRenderDebug();
      va label("%d", index + 1);
      renderDebug->DebugText(label, rigidBodyOrigin, 0.1f,
                             idColor::colorWhite, 1, 0, false, false);
    }
  }
}

__declspec(noinline) idEffectPhysicsProperties*
idEffectPhysicsPieceEmitter::GetPropertiesForTraceModel(
    const idTraceModel& traceModel) {
  const int traceModelIndex = broadPhase.traceModelCache->AllocTraceModel(
      traceModel, GetFirstSurfaceMaterial(model));
  for (int index = 0; index < properties.num; ++index) {
    if (properties.list[index]->traceModelIndex == traceModelIndex) {
      return properties.list[index];
    }
  }

  idEffectPhysicsProperties* const props =
      new (TAG_FXPHYSICS_RETAIL) idEffectPhysicsProperties;
  props->Setup(&broadPhase, traceModelIndex, pieceMass);
  UpdateProperties(props);
  AppendOpaquePointer(properties, props);
  return props;
}

void idEffectPhysicsPieceEmitter::Init(
    idClip* clip, idTraceModelCache* traceModelCache,
    idRenderModelDiscreteAnimation* modelValue,
    idRenderModelEffects* modelEffectsValue,
    pieceEmitterSound_t* soundInfoValue, int entityNumber) {
  broadPhase.Init(clip, traceModelCache);
  model = modelValue;
  decl = GetBreakableDecl(modelValue);
  modelEffects = modelEffectsValue;
  if (soundInfoValue != nullptr) {
    soundInfo = *soundInfoValue;
  }
  pieceIndex = 0;
  numPiecesInUse = 0;
  properties.DeleteContents();

  const int pieceCount = GetTransformCount(modelValue);
  if (pieceCount <= pieces.size || pieces.Resize(pieceCount)) {
    pieces.num = pieceCount < pieces.size ? pieceCount : pieces.size;
  }

  for (int index = 0; index < GetTransformCount(modelValue); ++index) {
    idTraceModel traceModel;
    idVec3 position;
    idMat3 orientation;
    modelValue->GetPieceTransform(index, position, orientation);
    modelValue->GetPieceTraceModel(index, traceModel);
    traceModel.Shrink(0.1875f);

    const idEffectPhysicsProperties* const props =
        GetPropertiesForTraceModel(traceModel);
    idEffectPhysicsRigidBody* body =
        new (TAG_FXPHYSICS_RETAIL) idEffectPhysicsRigidBody;
    body->Setup(props);
    SetBodyPosition(body, position);
    SetBodyOrientation(body, orientation);
    ZeroBodyMomentum(body);
    body->SPObject.entityNum = entityNumber;

    pieces.list[index].rigidBody = body;
    body = nullptr;
    pieces.list[index].emitTime = 0;
    pieces.list[index].firstCollisionTime = 0;
  }
}
