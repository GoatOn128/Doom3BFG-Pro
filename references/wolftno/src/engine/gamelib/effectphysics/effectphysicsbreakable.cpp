#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_DELETECONTENTS_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#include "engine/gamelib/effectphysics/effectphysicsbreakable.h"

#include "engine/decls/declsimple.h"
#include "engine/framework/common.h"
#include "engine/gamelib/effectphysics/effectphysicsbroadphase_abi.h"
#include "engine/gamelib/effects/gamelibeffects.h"
#include "engine/gamelib/physics/physics.h"
#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/models/discreteanimation/rendermodeldiscreteanimation.h"
#include "engine/models/particles/rendermodeleffects.h"
#include "engine/models/rendermodelbeam.h"
#include "engine/models/rendermodeldecal.h"
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "engine/sound/soundsystem.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/geometry/tracemodel.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/sys/sys_alloc.h"

#undef WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#undef WOLFTNO_RETAIL_PHYSICS_STATIC_CONST_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_DELETECONTENTS_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <xmmintrin.h>

idRenderDebug* GetGameLibRenderDebug();

idCVar g_skipBreakableParticles(
    "g_skipBreakableParticles", "0", CVAR_BOOL, "",
    static_cast<argCompletionRef_t>(nullptr));
idCVar g_breakableSpin("g_breakableSpin", "250", CVAR_FLOAT, "",
                       static_cast<argCompletionRef_t>(nullptr));
idCVar g_breakableRayUpPercent(
    "g_breakableRayUpPercent", "10", CVAR_FLOAT, "",
    static_cast<argCompletionRef_t>(nullptr));
idCVar g_breakableDebugIslands(
    "g_breakableDebugIslands", "0", CVAR_BOOL, "",
    static_cast<argCompletionRef_t>(nullptr));
idCVar g_debugBreakableDecals(
    "g_breakableDebugDecals", "0", CVAR_BOOL, "",
    static_cast<argCompletionRef_t>(nullptr));

template <typename Type>
void idListArrayDelete(void* pointer, int count);

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

extern bool forceIdListsToReallocateEveryAppend;

namespace {

constexpr memTag_t TAG_FXPHYSICS_RETAIL = static_cast<memTag_t>(59);
constexpr float RANDOM_TO_UNIT = 0.000030518509f;

struct breakableIsland_t {
  int index;
  float lowestPoint;
};

template <typename Type>
void ClearRetailTrivialList(idList<Type, 59>& values) {
  if (values.listStatic == 0 || values.listStatic == 2) {
    if (values.list != nullptr) {
      idMem::Singleton().Free(values.list, ALIGN_16);
    }
    values.list = nullptr;
    values.size = 0;
  }
  values.num = 0;
}

template <typename Type>
void ClearRetailObjectList(idList<Type, 59>& values) {
  if (values.listStatic == 0 || values.listStatic == 2) {
    if (values.list != nullptr) {
      idListArrayDelete<Type>(values.list, values.size);
    }
    values.list = nullptr;
    values.size = 0;
  }
  values.num = 0;
}

template <typename Type, bool ObjectElements>
bool ResizeRetailList(idList<Type, 59>& values, int newSize) {
  if (newSize <= 0) {
    if constexpr (ObjectElements) {
      ClearRetailObjectList(values);
    } else {
      ClearRetailTrivialList(values);
    }
    return true;
  }
  if (newSize == values.size || values.listStatic != 0) {
    return true;
  }
  values.list = static_cast<Type*>(idListArrayResize<Type>(
      values.list, values.size, newSize,
      static_cast<memTag_t>(values.memTag), false));
  if (values.list == nullptr) {
    return false;
  }
  values.size = newSize;
  if (values.num > newSize) {
    values.num = newSize;
  }
  return true;
}

} // namespace

template <>
__declspec(noinline) bool
idList<idEffectPhysicsCollision, 59>::Resize(int newSize) {
  return ResizeRetailList<idEffectPhysicsCollision, false>(*this, newSize);
}

template <>
__declspec(noinline) bool
idList<idEffectPhysicsBreakable::idBreakableParticle, 59>::Resize(
    int newSize) {
  return ResizeRetailList<idEffectPhysicsBreakable::idBreakableParticle,
                          false>(*this, newSize);
}

template <>
__declspec(noinline) bool
idList<idEffectPhysicsBreakable::idSpark, 59>::Resize(int newSize) {
  return ResizeRetailList<idEffectPhysicsBreakable::idSpark, false>(
      *this, newSize);
}

template <>
__declspec(noinline) bool
idList<idEffectPhysicsBreakable::idDecalFX::decalEntry_t, 59>::Resize(
    int newSize) {
  return ResizeRetailList<
      idEffectPhysicsBreakable::idDecalFX::decalEntry_t, false>(
      *this, newSize);
}

template <>
__declspec(noinline) bool
idList<idEffectPhysicsBreakable::idTrail::trailEntry_t, 59>::Resize(
    int newSize) {
  return ResizeRetailList<idEffectPhysicsBreakable::idTrail::trailEntry_t,
                          false>(*this, newSize);
}

template <>
__declspec(noinline) bool
idList<idEffectPhysicsBreakable::idExplosion, 59>::Resize(int newSize) {
  return ResizeRetailList<idEffectPhysicsBreakable::idExplosion, true>(
      *this, newSize);
}

template <>
__declspec(noinline) bool
idList<idEffectPhysicsBreakable::idTrail, 59>::Resize(int newSize) {
  return ResizeRetailList<idEffectPhysicsBreakable::idTrail, true>(
      *this, newSize);
}

template <>
__declspec(noinline) bool
idList<idEffectPhysicsBreakable::idDecalFX, 59>::Resize(int newSize) {
  return ResizeRetailList<idEffectPhysicsBreakable::idDecalFX, true>(
      *this, newSize);
}

template <>
__declspec(noinline) bool idList<idFXManager, 59>::Resize(int newSize) {
  return ResizeRetailList<idFXManager, true>(*this, newSize);
}

template <>
__declspec(noinline)
idList<idFXManager, 59>::~idList() {
  ClearRetailObjectList(*this);
}

template <>
__declspec(noinline)
idList<idEffectPhysicsBreakable::idExplosion, 59>::~idList() {
  ClearRetailObjectList(*this);
}

template <>
__declspec(noinline)
idList<idEffectPhysicsBreakable::idTrail, 59>::~idList() {
  ClearRetailObjectList(*this);
}

template <>
__declspec(noinline)
idList<idEffectPhysicsBreakable::idDecalFX, 59>::~idList() {
  ClearRetailObjectList(*this);
}

template <>
__declspec(noinline) int
idList<idEffectPhysicsBreakable::idBreakableParticle, 59>::Append(
    const idEffectPhysicsBreakable::idBreakableParticle& value) {
  if (list == nullptr) {
    const int initialSize = granularity > 0 ? granularity : 1;
    if (!Resize(initialSize)) {
      return -1;
    }
  }
  if (num == size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = size + 1;
    } else if (granularity != 0) {
      newSize = size + granularity - (size + granularity) % granularity;
    } else if (size <= 0x3fffffff) {
      newSize = size * 2;
      if (newSize == 0) {
        newSize = 1;
      }
    } else {
      newSize = 0x7fffffff;
    }
    if (!Resize(newSize)) {
      return -1;
    }
  }
  if (num >= size) {
    return num - 1;
  }
  list[num] = value;
  return num++;
}

template <>
__declspec(noinline)
idList<idEffectPhysicsBreakable::idExplosion, 59>::AllocResult
idList<idEffectPhysicsBreakable::idExplosion, 59>::Alloc() {
  const int required = num + 1;
  if (required > size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (granularity != 0) {
      newSize = required + granularity - 1 -
                (required + granularity - 1) % granularity;
    } else {
      for (newSize = 1; newSize < required; newSize *= 2) {
      }
    }
    Resize(newSize);
  }
  if (num >= size) {
    return &list[size - 1];
  }
  return &list[num++];
}

template <>
__declspec(noinline) void
idList<idEffectPhysicsProperties*, 59>::DeleteContents() {
  for (int index = 0; index < num; ++index) {
    idEffectPhysicsProperties* const value = list[index];
    if (value != nullptr) {
      value->~idEffectPhysicsProperties();
      idMem::Singleton().Free(value, ALIGN_16);
    }
    list[index] = nullptr;
  }
  ClearRetailTrivialList(*this);
}

template <>
__declspec(noinline) void
idList<idEffectPhysicsRigidBody*, 59>::DeleteContents() {
  for (int index = 0; index < num; ++index) {
    idEffectPhysicsRigidBody* const value = list[index];
    if (value != nullptr) {
      value->~idEffectPhysicsRigidBody();
      idMem::Singleton().Free(value, ALIGN_16);
    }
    list[index] = nullptr;
  }
  ClearRetailTrivialList(*this);
}

namespace {

__forceinline float InverseLength(float squaredLength) {
  return _mm_cvtss_f32(_mm_div_ps(
      _mm_set1_ps(1.0f),
      _mm_sqrt_ps(_mm_max_ps(_mm_set_ss(squaredLength),
                             idMath::vector_float_smallestNonDenorm))));
}

__forceinline float ClampedLength(float squaredLength) {
  return _mm_cvtss_f32(
      _mm_sqrt_ps(_mm_max_ps(_mm_set_ss(squaredLength),
                             idMath::vector_float_smallestNonDenorm)));
}

__forceinline float LengthSquared(const idVec3& value) {
  return (value.y * value.y + value.x * value.x) + value.z * value.z;
}

__forceinline float Dot(const idVec3& left, const idVec3& right) {
  return (left.y * right.y + left.x * right.x) + left.z * right.z;
}

__forceinline idVec3 Cross(const idVec3& left, const idVec3& right) {
  return {left.y * right.z - left.z * right.y,
          left.z * right.x - left.x * right.z,
          left.x * right.y - left.y * right.x};
}

__forceinline bool HasNonZeroBits(const idVec3& value) {
  const auto* words = reinterpret_cast<const std::uint32_t*>(&value);
  return ((words[0] | words[1] | words[2]) & 0x7fffffffu) != 0u;
}

__forceinline float NextRandom(idRandom& random) {
  return static_cast<float>(random.RandomInt()) * RANDOM_TO_UNIT;
}

__forceinline float RandomCentered(idRandom& random, float scale) {
  return ((NextRandom(random) * 2.0f) - 1.0f) * scale;
}

idVec3 TransformPoint(const idVec3& point, const idVec3& origin,
                      const idMat3& axis) {
  return {
      ((point.y * axis.mat[1].x + point.x * axis.mat[0].x) +
       point.z * axis.mat[2].x) +
          origin.x,
      ((point.x * axis.mat[0].y + point.y * axis.mat[1].y) +
       point.z * axis.mat[2].y) +
          origin.y,
      ((point.x * axis.mat[0].z + point.y * axis.mat[1].z) +
       point.z * axis.mat[2].z) +
          origin.z,
  };
}

idVec3 TransformVector(const idVec3& value, const idMat3& axis) {
  return {
      (value.y * axis.mat[1].x + value.x * axis.mat[0].x) +
          value.z * axis.mat[2].x,
      (value.x * axis.mat[0].y + value.y * axis.mat[1].y) +
          value.z * axis.mat[2].y,
      (value.x * axis.mat[0].z + value.y * axis.mat[1].z) +
          value.z * axis.mat[2].z,
  };
}

void SetBodyPosition(idEffectPhysicsRigidBody* body,
                     const idVec3& position) {
  body->currentState.position = position;
  body->currentState.positionFiltered = position;
  body->properties->broadPhase->SetObjectPosition(
      &body->SPObject, body->currentState.position,
      body->currentState.orientation, nullptr);
}

void ZeroBodyMomentum(idEffectPhysicsRigidBody* body) {
  const idEffectPhysicsProperties* properties = body->properties;
  body->currentState.linearMomentum = {
      vec3_origin.x * properties->mass,
      vec3_origin.y * properties->mass,
      vec3_origin.z * properties->mass,
  };
  body->currentState.angularMomentum = {
      (vec3_origin.y * properties->inertiaTensor.mat[1].x +
       vec3_origin.x * properties->inertiaTensor.mat[0].x) +
          vec3_origin.z * properties->inertiaTensor.mat[2].x,
      (vec3_origin.y * properties->inertiaTensor.mat[1].y +
       vec3_origin.x * properties->inertiaTensor.mat[0].y) +
          vec3_origin.z * properties->inertiaTensor.mat[2].y,
      (vec3_origin.y * properties->inertiaTensor.mat[1].z +
       vec3_origin.x * properties->inertiaTensor.mat[0].z) +
          vec3_origin.z * properties->inertiaTensor.mat[2].z,
  };
}

void TransformInactiveBody(idEffectPhysicsRigidBody* body,
                           const idVec3& origin, const idMat3& axis) {
  SetBodyPosition(body,
                  TransformPoint(body->currentState.positionFiltered,
                                 origin, axis));
  idMat3 orientation = body->currentState.orientationFiltered * axis;
  body->SetOrientation(orientation);
}

void ActivateBody(idEffectPhysicsRigidBody* body, const idVec3& origin,
                  const idMat3& axis) {
  if (body->active) {
    return;
  }
  TransformInactiveBody(body, origin, axis);
  body->simulationTimeFirstContact = -1.0f;
  body->simulationTime = 0.0f;
  body->active = true;
  body->SPObject.motion.ignore = false;
}

idBounds ExpandShortBounds(const idBoundsShort& shortBounds) {
  idBounds result;
  result[0] = {static_cast<float>(shortBounds.b[0][0]),
               static_cast<float>(shortBounds.b[0][1]),
               static_cast<float>(shortBounds.b[0][2])};
  result[1] = {static_cast<float>(shortBounds.b[1][0]),
               static_cast<float>(shortBounds.b[1][1]),
               static_cast<float>(shortBounds.b[1][2])};
  return result;
}

bool BoundsIntersectExpanded(const idBounds& first, const idBounds& second,
                             float expansion) {
  return first[0].x - expansion <= second[1].x &&
         second[0].x <= first[1].x + expansion &&
         first[0].y - expansion <= second[1].y &&
         second[0].y <= first[1].y + expansion &&
         first[0].z - expansion <= second[1].z &&
         second[0].z <= first[1].z + expansion;
}

int ScaledGameMilliseconds() {
  idGame* const game = common->Game();
  using Function = int (*)(idGame*, int);
  void** const vtable = *reinterpret_cast<void***>(game);
  return reinterpret_cast<Function>(vtable[47])(game, 1);
}

void DeleteRenderModel(idRenderModelDiscreteAnimation* model) {
  if (model == nullptr) {
    return;
  }
  idRenderModel* const renderModel = model;
  renderModel->CommitThisFrame();
  using Function = void (*)(idRenderModel*);
  Function* const vtable = *reinterpret_cast<Function**>(renderModel);
  if ((renderModel->unlinked & 8u) == 0u) {
    renderModel->CommitThisFrame();
    vtable[7](renderModel);
    renderModel->unlinked |= 8u;
  }
  vtable[8](renderModel);
  renderModel->deleteOnSync |= 4u;
}

idCollisionModel* GetCachedCollisionModel(idTraceModelCache* cache,
                                           int traceModelIndex) {
  void** const entries = *reinterpret_cast<void***>(
      reinterpret_cast<std::uint8_t*>(cache) + 0x48);
  return *reinterpret_cast<idCollisionModel**>(
      static_cast<std::uint8_t*>(entries[traceModelIndex]) + 0x48);
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

template <typename Type> Type* AllocateEffectObject() {
  void* const storage = idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(Type), TAG_FXPHYSICS_RETAIL, false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  return storage != nullptr ? ::new (storage) Type : nullptr;
}

void* AllocateEffectScratch(std::size_t size) {
  return idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(113) : tag", size,
      TAG_FXPHYSICS_RETAIL, false, ALIGN_16, HEAP_DEFAULTHEAP);
}

void FreeEffectScratch(void* allocation) {
  idMem::Singleton().Free(allocation, ALIGN_16);
}

float RandomRange(idRandom& random, float minimum, float maximum) {
  return (maximum - minimum) * NextRandom(random) + minimum;
}

void AddRandomExplosionVelocity(
    idEffectPhysicsRigidBody* body, const idVec3& randomLinear,
    const idVec3& randomAngular) {
  const idEffectPhysicsProperties* const properties = body->properties;
  body->currentState.linearMomentum.x =
      properties->mass *
      (properties->inverseMass * body->currentState.linearMomentum.x +
       randomLinear.x);
  body->currentState.linearMomentum.y =
      properties->mass *
      (properties->inverseMass * body->currentState.linearMomentum.y +
       randomLinear.y);
  body->currentState.linearMomentum.z =
      properties->mass *
      (properties->inverseMass * body->currentState.linearMomentum.z +
       randomLinear.z);

  const idVec3& angularMomentum = body->currentState.angularMomentum;
  const idMat3& inverseInertia = properties->inverseInertiaTensor;
  const idVec3 angularVelocity{
      (((angularMomentum.y * inverseInertia.mat[1].x +
         angularMomentum.x * inverseInertia.mat[0].x) +
        angularMomentum.z * inverseInertia.mat[2].x) *
           6.0f) +
          randomAngular.x,
      (((angularMomentum.x * inverseInertia.mat[0].y +
         angularMomentum.y * inverseInertia.mat[1].y) +
        angularMomentum.z * inverseInertia.mat[2].y) *
           6.0f) +
          randomAngular.y,
      (((angularMomentum.x * inverseInertia.mat[0].z +
         angularMomentum.y * inverseInertia.mat[1].z) +
        angularMomentum.z * inverseInertia.mat[2].z) *
           6.0f) +
          randomAngular.z,
  };
  const idMat3& inertia = properties->inertiaTensor;
  body->currentState.angularMomentum = {
      (angularVelocity.y * inertia.mat[1].x +
       angularVelocity.x * inertia.mat[0].x) +
          angularVelocity.z * inertia.mat[2].x,
      (angularVelocity.y * inertia.mat[1].y +
       angularVelocity.x * inertia.mat[0].y) +
          angularVelocity.z * inertia.mat[2].y,
      (angularVelocity.y * inertia.mat[1].z +
       angularVelocity.x * inertia.mat[0].z) +
          angularVelocity.z * inertia.mat[2].z,
  };
}

void ApplyExplosionImpulse(idEffectPhysicsRigidBody* body,
                           const idVec3& impulse,
                           const idVec3& randomLinear,
                           const idVec3& randomAngular, float angular,
                           idRandom& random) {
  AddRandomExplosionVelocity(body, randomLinear, randomAngular);
  const float pointZ =
      body->currentState.positionFiltered.z + RandomCentered(random, angular);
  const float pointY =
      body->currentState.positionFiltered.y + RandomCentered(random, angular);
  const float pointX =
      body->currentState.positionFiltered.x + RandomCentered(random, angular);
  body->ApplyImpulse({pointX, pointY, pointZ}, impulse);
  body->simulationTimeFirstContact = -1.0f;
  body->simulationTime = 0.0f;
  body->active = true;
  body->SPObject.motion.ignore = false;
}

std::uint8_t FadeByte(float fade) {
  const int converted = static_cast<int>(fade * 255.0f);
  if (converted < 0) {
    return 0;
  }
  if (converted > 255) {
    return 255;
  }
  return static_cast<std::uint8_t>(converted);
}

} // namespace

idVec3 idBounds::GetMaxExtents() const {
  idVec3 result;
  result.z = std::fmax(std::fabs(b[0].z), std::fabs(b[1].z));
  result.y = std::fmax(std::fabs(b[0].y), std::fabs(b[1].y));
  result.x = std::fmax(std::fabs(b[0].x), std::fabs(b[1].x));
  return result;
}

idEffectPhysicsBreakable::idEffectPhysicsBreakable()
    : properties(), rigidBodies(), collisions(), basePiece(nullptr),
      explosions(), explosionFX(), trails(), damage(), particles(), decals(),
      sparks(), pieceActivationTimes(), broadPhase(), random(0),
      gameLibEffects(nullptr), model(nullptr), decl(nullptr), soundInfo() {}

idEffectPhysicsBreakable::~idEffectPhysicsBreakable() noexcept(false) {
  if (model != nullptr) {
    const int traceModelCount = model->modelData != nullptr
                                    ? model->modelData->traceModels.num
                                    : 0;
    for (int index = 0; index < traceModelCount; ++index) {
      broadPhase.traceModelCache->FreeTraceModel(
          rigidBodies.list[index]->properties->traceModelIndex);
    }
    DeleteRenderModel(model);
    model = nullptr;
  }
  rigidBodies.DeleteContents();
  properties.DeleteContents();
}

void idEffectPhysicsBreakable::UpdateProperties(
    idEffectPhysicsProperties* props) {
  if (decl == nullptr) {
    return;
  }
  props->linearFriction = decl->settings.linearFriction;
  props->angularFriction = decl->settings.angularFriction;
  props->contactFriction = decl->settings.contactFriction;
  props->linearFrictionWater = decl->settings.linearFrictionWater;
  props->angularFrictionWater = decl->settings.angularFrictionWater;
  props->bouncyness = decl->settings.bouncyness;
  props->gravityVector = decl->settings.gravityVector;
  props->worldCollisionOnly = decl->settings.worldCollisionOnly;
  props->clipMask = decl->settings.clipMask;
  props->maxSimulationTime = static_cast<int>(decl->settings.maxSimulationTime);
  props->maxLinearVelocity = decl->settings.maxLinearVelocity;
  props->maxAngularVelocity = decl->settings.maxAngularVelocity;
}

void idEffectPhysicsBreakable::StopExplosionFX(int currentTime) {
  for (int index = 0; index < explosionFX.num; ++index) {
    explosionFX.list[index].StopAllFX(currentTime, true);
  }
}

void idEffectPhysicsBreakable::UpdateModel(
    const idVec3& origin, const idMat3& axis,
    int gameMillisecondsPerFrame) {
  if (model == nullptr) {
    return;
  }
  for (int index = 0; index < rigidBodies.num; ++index) {
    idEffectPhysicsRigidBody* const body = rigidBodies.list[index];
    if (body->active) {
      model->SetPieceTransform(index, body->currentState.positionFiltered,
                               body->currentState.orientationFiltered);
    } else {
      const idVec3 position = TransformPoint(
          body->currentState.positionFiltered, origin, axis);
      const idMat3 orientation =
          body->currentState.orientationFiltered * axis;
      model->SetPieceTransform(index, position, orientation);
    }
  }
  model->SetBasePiecePosition(origin, axis);
  model->Update();

  if (gameLibEffects == nullptr) {
    return;
  }
  if (particles.num > 0 && g_skipBreakableParticles.valueInteger == 0) {
    for (int index = particles.num - 1; index >= 0; --index) {
      std::uint32_t color = 0xffffffffu;
      gameLibEffects->effectsModel->AddParticles(
          particles.list[index].particle, particles.list[index].startTime,
          gameMillisecondsPerFrame, NextRandom(random),
          particles.list[index].pos, mat3_identity, vec3_origin, color,
          false);
    }
  }
  for (int index = 0; index < sparks.num; ++index) {
    idEffectPhysicsRigidBody* const body = rigidBodies.list[index];
    if (body == nullptr) {
      continue;
    }
    const idEffectPhysicsProperties* const props = body->properties;
    const idVec3 velocity{
        props->inverseMass * body->currentState.linearMomentum.x,
        props->inverseMass * body->currentState.linearMomentum.y,
        props->inverseMass * body->currentState.linearMomentum.z,
    };
    const float inverseLength = InverseLength(LengthSquared(velocity));
    const idVec3 direction{velocity.x * inverseLength,
                           velocity.y * inverseLength,
                           velocity.z * inverseLength};
    const float velocityLength = ClampedLength(LengthSquared(velocity));
    const float length = std::fmax(
        sparks.list[index].size,
        velocityLength * sparks.list[index].velocityScale);
    const idVec3 start{
        body->currentState.positionFiltered.x - direction.x * length,
        body->currentState.positionFiltered.y - direction.y * length,
        body->currentState.positionFiltered.z - direction.z * length,
    };
    gameLibEffects->effectsModel->AddTracer(
        sparks.list[index].material, start, direction, vec3_origin, 0.0f,
        length, sparks.list[index].size, false, 0.050000001f, 0.0f);
  }
}

void idEffectPhysicsBreakable::AddRadiusImpact(
    int, const idPhysics* physics, const idVec3& position,
    float radius, float impulsePower) {
  for (int index = 0; index < rigidBodies.num; ++index) {
    idEffectPhysicsRigidBody* const body = rigidBodies.list[index];
    idBounds bounds = ExpandShortBounds(body->SPObject.absBounds);
    if (!body->active) {
      idBounds worldBounds;
      worldBounds.FromTransformedBounds(bounds, physics->GetOrigin(0),
                                        physics->GetAxis(0));
      bounds = worldBounds;
    }
    idSphere sphere;
    sphere.origin = position;
    sphere.radius = radius;
    if (!bounds.SphereIntersection(sphere)) {
      continue;
    }
    ActivateBody(body, physics->GetOrigin(0), physics->GetAxis(0));
    const idVec3 delta{
        body->currentState.positionFiltered.x - position.x,
        body->currentState.positionFiltered.y - position.y,
        body->currentState.positionFiltered.z - position.z,
    };
    const float inverseLength = InverseLength(LengthSquared(delta));
    const idVec3 impulse{impulsePower * delta.x * inverseLength,
                         impulsePower * delta.y * inverseLength,
                         impulsePower * delta.z * inverseLength};
    const float pointZ =
        position.z + RandomCentered(random, g_breakableSpin.valueFloat);
    const float pointY =
        position.y + RandomCentered(random, g_breakableSpin.valueFloat);
    const float pointX =
        position.x + RandomCentered(random, g_breakableSpin.valueFloat);
    const idVec3 point{pointX, pointY, pointZ};
    body->ApplyImpulse(point, impulse);
  }
}

void idEffectPhysicsBreakable::ApplyImpulseToBody(
    int bodyIndex, const idPhysics* physics, const idVec3& point,
    const idVec3& impulse) {
  if (physics == nullptr || bodyIndex < 0 || bodyIndex >= rigidBodies.num) {
    return;
  }
  idEffectPhysicsRigidBody* const body = rigidBodies.list[bodyIndex];
  ActivateBody(body, physics->GetOrigin(0), physics->GetAxis(0));
  body->ApplyImpulse(point, impulse);
}

bool idEffectPhysicsBreakable::IsAtRest() {
  for (int index = 0; index < rigidBodies.num; ++index) {
    const idEffectPhysicsRigidBody* const body = rigidBodies.list[index];
    if (body->enabled && body->active && !body->settled) {
      return false;
    }
  }
  return true;
}

bool idEffectPhysicsBreakable::ActivateNoisy(
    int pieceIndex, const idVec3& origin, const idMat3& axis) {
  idEffectPhysicsRigidBody* const body = rigidBodies.list[pieceIndex];
  if (body->active) {
    return false;
  }
  const float impulseZ = RandomCentered(random, 1000.0f);
  const float impulseY = RandomCentered(random, 1500.0f);
  const float impulseX = RandomCentered(random, 1500.0f);
  const idVec3 impulse{impulseX, impulseY, impulseZ};
  TransformInactiveBody(body, origin, axis);
  body->ApplyImpulse(body->currentState.positionFiltered, impulse);
  body->simulationTimeFirstContact = -1.0f;
  body->simulationTime = 0.0f;
  body->active = true;
  body->SPObject.motion.ignore = false;
  return true;
}

void idEffectPhysicsBreakable::ApplyForce(
    const idVec3& origin, const idMat3& axis, const idVec3& force) {
  for (int index = 0; index < rigidBodies.num; ++index) {
    idEffectPhysicsRigidBody* const body = rigidBodies.list[index];
    if (!body->active) {
      TransformInactiveBody(body, origin, axis);
      body->ApplyImpulse(body->currentState.positionFiltered, force);
      body->simulationTimeFirstContact = -1.0f;
      body->simulationTime = 0.0f;
      body->SPObject.motion.ignore = false;
      body->active = true;
    }
  }
}

void idEffectPhysicsBreakable::DisablePiece(int index) {
  rigidBodies.list[index]->Disable();
}

void idEffectPhysicsBreakable::UpdateSimulation(
    const idVec3& origin, const idMat3& axis, int currentTime,
    int gameMillisecondsPerFrame, int serverCurrentTime,
    float attenuation) {
  (void)attenuation;
  if (gameLibEffects == nullptr) {
    return;
  }

  for (int explosionIndex = 0; explosionIndex < explosions.num;
       ++explosionIndex) {
    idExplosion& explosion = explosions.list[explosionIndex];
    idFXManager* fx = nullptr;
    if (explosion.fxIndex >= 0 && explosion.fxIndex < explosionFX.num) {
      fx = &explosionFX.list[explosion.fxIndex];
    }
    if (explosion.startTime == 0) {
      continue;
    }
    const int effectStartTime =
        explosion.startTime + explosion.decl.delay;
    if (currentTime < effectStartTime) {
      continue;
    }
    if (currentTime > effectStartTime + explosion.decl.duration) {
      if (fx != nullptr) {
        fx->StopFX(currentTime, FX_NONE, false);
      }
      continue;
    }

    if (!explosion.effectsEnabled) {
      if (explosion.localSpace) {
        explosion.worldPosition =
            TransformPoint(explosion.decl.position, origin, axis);
      }
      explosion.effectsEnabled = true;
    }
    if (explosion.decl.particle != nullptr &&
        g_skipBreakableParticles.valueInteger == 0) {
      std::uint32_t color = 0xffffffffu;
      gameLibEffects->effectsModel->AddParticles(
          explosion.decl.particle, effectStartTime,
          gameMillisecondsPerFrame, NextRandom(random),
          explosion.worldPosition, mat3_identity, vec3_origin, color,
          false);
    }
    if (fx != nullptr) {
      fx->Update(origin, axis, vec3_origin, currentTime,
                 gameMillisecondsPerFrame, serverCurrentTime, 1.0f,
                 0.0f);
    }

    for (int pieceListIndex = 0;
         pieceListIndex < explosion.decl.pieces.num;
         ++pieceListIndex) {
      const int pieceIndex = explosion.decl.pieces.list[pieceListIndex];
      idEffectPhysicsRigidBody* const body = rigidBodies.list[pieceIndex];
      if (pieceListIndex < pieceActivationTimes.num &&
          currentTime < explosion.startTime +
                            pieceListIndex *
                                pieceActivationTimes.list[pieceListIndex]) {
        continue;
      }
      if (explosion.forceApplied &&
          (body->active || explosion.decl.activateAllDelay == -1)) {
        continue;
      }

      const idVec3 center = body->active ? explosion.worldPosition
                                         : explosion.decl.position;
      idVec3 direction{
          body->currentState.positionFiltered.x - center.x,
          body->currentState.positionFiltered.y - center.y,
          body->currentState.positionFiltered.z - center.z,
      };
      const float directionLengthSquared = LengthSquared(direction);
      const float inverseDirectionLength =
          InverseLength(directionLengthSquared);
      direction.x *= inverseDirectionLength;
      direction.y *= inverseDirectionLength;
      direction.z *= inverseDirectionLength;
      const float scaledDistance =
          inverseDirectionLength * directionLengthSquared + 1.0f;

      idVec3 randomLinear;
      idVec3 randomAngular;
      randomLinear.x = RandomRange(random, explosion.decl.randomLinear.p[0],
                                   explosion.decl.randomLinear.p[3]);
      randomAngular.x =
          RandomRange(random, explosion.decl.randomAngular.p[0],
                      explosion.decl.randomAngular.p[3]);
      randomLinear.y = RandomRange(random, explosion.decl.randomLinear.p[1],
                                   explosion.decl.randomLinear.p[4]);
      randomAngular.y =
          RandomRange(random, explosion.decl.randomAngular.p[1],
                      explosion.decl.randomAngular.p[4]);
      randomLinear.z = RandomRange(random, explosion.decl.randomLinear.p[2],
                                   explosion.decl.randomLinear.p[5]);
      randomAngular.z =
          RandomRange(random, explosion.decl.randomAngular.p[2],
                      explosion.decl.randomAngular.p[5]);

      if (explosion.decl.activateAllDelay != -1) {
        const int activationDelay =
            explosion.decl.activateAllDelay +
            static_cast<int>(scaledDistance *
                             explosion.decl.distTimeScaleDelay);
        if (currentTime > effectStartTime + activationDelay) {
          if (!body->active) {
            TransformInactiveBody(body, origin, axis);
            direction = TransformVector(direction, axis);
          }
          idVec3 impulse = vec3_origin;
          if (activationDelay == 0) {
            const float impulseScale =
                1.0f / (scaledDistance * explosion.decl.falloff);
            impulse = {
                impulseScale * explosion.decl.impulse * direction.x,
                impulseScale * explosion.decl.impulse * direction.y,
                impulseScale * explosion.decl.impulse * direction.z,
            };
          }
          ApplyExplosionImpulse(body, impulse, randomLinear, randomAngular,
                                explosion.decl.angular, random);
        }
      }

      if (!explosion.forceApplied) {
        const idBounds bounds = ExpandShortBounds(body->SPObject.absBounds);
        bool intersects = false;
        if (explosion.decl.useLinear) {
          intersects = bounds.LineIntersection(
              center, explosion.decl.endLinear);
        } else {
          idSphere sphere;
          sphere.origin = center;
          sphere.radius = explosion.decl.radius;
          intersects = bounds.SphereIntersection(sphere);
        }
        if (intersects) {
          if (!body->active) {
            idVec3 activatedPosition = TransformPoint(
                body->currentState.positionFiltered, origin, axis);
            activatedPosition.z += 5.0f;
            SetBodyPosition(body, activatedPosition);
            const idMat3 orientation =
                body->currentState.orientationFiltered * axis;
            body->SetOrientation(orientation);
            direction = TransformVector(direction, axis);
          }
          const float impulseScale =
              1.0f / (scaledDistance * explosion.decl.falloff);
          const idVec3 impulse{
              impulseScale * explosion.decl.impulse * direction.x,
              impulseScale * explosion.decl.impulse * direction.y,
              impulseScale * explosion.decl.impulse * direction.z,
          };
          ApplyExplosionImpulse(body, impulse, randomLinear, randomAngular,
                                explosion.decl.angular, random);
        }
      }
    }
    explosion.forceApplied = true;
  }

  const float deltaTime =
      static_cast<float>(gameMillisecondsPerFrame) * 0.001f;
  for (int bodyIndex = 0; bodyIndex < rigidBodies.num; ++bodyIndex) {
    rigidBodies.list[bodyIndex]->Update(deltaTime,
                                        collisions.list[bodyIndex]);
  }

  if (g_skipBreakableParticles.valueInteger == 0) {
    for (int trailIndex = 0; trailIndex < trails.num; ++trailIndex) {
      idTrail& trail = trails.list[trailIndex];
      const float minimumTrailVelocitySquared =
          trail.decl.minTrailVelocity * trail.decl.minTrailVelocity;
      const float minimumBounceVelocitySquared =
          trail.decl.minBounceVelocity * trail.decl.minBounceVelocity;
      for (int entryIndex = 0; entryIndex < trail.decl.pieces.num;
           ++entryIndex) {
        const int pieceIndex = trail.decl.pieces.list[entryIndex];
        idEffectPhysicsRigidBody* const body = rigidBodies.list[pieceIndex];
        if (!body->active) {
          continue;
        }
        idTrail::trailEntry_t& entry = trail.entries.list[entryIndex];
        if (entry.pieceStartTime < 0) {
          if (trail.decl.particleDie != nullptr) {
            std::uint32_t color = 0xffffffffu;
            gameLibEffects->effectsModel->AddParticles(
                trail.decl.particleDie, -entry.pieceStartTime,
                gameMillisecondsPerFrame, NextRandom(random),
                body->currentState.positionFiltered, mat3_identity,
                vec3_origin, color, false);
          }
          continue;
        }
        if (entry.pieceStartTime == 0) {
          entry.pieceStartTime = currentTime;
          entry.pieceLastParticleDropPos =
              body->currentState.positionFiltered;
        }
        const int trailStartTime = entry.pieceStartTime + trail.decl.delay;
        if (currentTime < trailStartTime) {
          entry.pieceLastParticleDropPos =
              body->currentState.positionFiltered;
          continue;
        }

        const idEffectPhysicsProperties* const bodyProperties =
            body->properties;
        const idVec3 bodyVelocity{
            bodyProperties->inverseMass * body->currentState.linearMomentum.x,
            bodyProperties->inverseMass * body->currentState.linearMomentum.y,
            bodyProperties->inverseMass * body->currentState.linearMomentum.z,
        };
        if (currentTime > trailStartTime + trail.decl.duration ||
            LengthSquared(bodyVelocity) < minimumTrailVelocitySquared) {
          entry.pieceStartTime = -currentTime;
          continue;
        }

        if (trail.decl.glowQuadMaterial != nullptr) {
          beamNodeParms_t node{};
          const float halfSize = trail.decl.glowQuadSize * 0.5f;
          node.startPos = body->currentState.positionFiltered;
          node.startPos.z += halfSize;
          node.endPos = body->currentState.positionFiltered;
          node.endPos.z -= halfSize;
          node.orientVec = vec3_origin;
          node.sMinMax = {0.0f, 1.0f};
          node.tMinMax = {0.0f, 1.0f};
          node.halfWidth = halfSize;
          float fade = 1.0f;
          const float time = static_cast<float>(currentTime);
          const float start = static_cast<float>(trailStartTime);
          const float fadeInEnd =
              start + static_cast<float>(trail.decl.fadeInDuration);
          const float end =
              start + static_cast<float>(trail.decl.duration);
          const float fadeOutStart =
              end - static_cast<float>(trail.decl.fadeOutDuration);
          if (fadeInEnd >= time) {
            fade = (time - start) / (fadeInEnd - start);
          }
          if (time >= fadeOutStart) {
            fade *= 1.0f -
                    ((time - fadeOutStart) / (end - fadeOutStart));
          }
          const std::uint8_t alpha = FadeByte(fade);
          for (int endIndex = 0; endIndex < 2; ++endIndex) {
            node.color[endIndex][0] = 255;
            node.color[endIndex][1] = 255;
            node.color[endIndex][2] = 255;
            node.color[endIndex][3] = alpha;
          }
          gameLibEffects->beamModel->DrawBeam(
              node, trail.decl.glowQuadMaterial, BEAM_ORIENT_VIEWER);
        }

        idEffectPhysicsCollision& collision = collisions.list[pieceIndex];
        const float collisionVelocitySquared =
            LengthSquared(collision.collisionNormalVelocity);
        if (trail.decl.particleBounce != nullptr &&
            collisionVelocitySquared > minimumBounceVelocitySquared) {
          const idMat3 particleAxis =
              collision.collisionNormalVelocity.ToZAxis();
          std::uint32_t color = 0xffffffffu;
          gameLibEffects->effectsModel->AddParticles(
              trail.decl.particleBounce, currentTime,
              gameMillisecondsPerFrame, NextRandom(random),
              collision.collisionPoint, particleAxis, vec3_origin, color,
              false);
        }

        if (soundInfo.soundWorld != nullptr &&
            soundInfo.impactTable != nullptr &&
            currentTime >= soundInfo.nextSoundTime &&
            collisionVelocitySquared > soundInfo.minBounceVelSqr) {
          const idSoundShader* const shader =
              soundInfo.impactTable->GetImpactSoundForMaterial(
                  collision.collisionSurfaceType);
          if (shader != nullptr) {
            PlayPositionedShaderDirectly(
                soundInfo.soundWorld, shader,
                body->currentState.positionFiltered, mat3_identity, 0.0f);
            soundInfo.nextSoundTime =
                currentTime + soundInfo.impactTable->soundInterval;
          }
        }

        if (trail.decl.particleTrail != nullptr) {
          const idVec3 distance{
              body->currentState.positionFiltered.x -
                  entry.pieceLastParticleDropPos.x,
              body->currentState.positionFiltered.y -
                  entry.pieceLastParticleDropPos.y,
              body->currentState.positionFiltered.z -
                  entry.pieceLastParticleDropPos.z,
          };
          const float distanceSquared = LengthSquared(distance);
          const float inverseDistance = InverseLength(distanceSquared);
          const idVec3 direction{distance.x * inverseDistance,
                                 distance.y * inverseDistance,
                                 distance.z * inverseDistance};
          const float distanceLength = inverseDistance * distanceSquared;
          if (distanceLength > trail.decl.trailSpacing) {
            const idMat3 particleAxis = direction.ToZAxis();
            const int dropCount = static_cast<int>(
                distanceLength / trail.decl.trailSpacing);
            const idVec3 particleVelocity{
                bodyVelocity.x * trail.decl.trailVelocityScale,
                bodyVelocity.y * trail.decl.trailVelocityScale,
                bodyVelocity.z * trail.decl.trailVelocityScale,
            };
            for (int dropIndex = 1; dropIndex <= dropCount;
                 ++dropIndex) {
              const float dropDistance =
                  static_cast<float>(dropIndex) * trail.decl.trailSpacing;
              const idVec3 particlePosition{
                  entry.pieceLastParticleDropPos.x +
                      direction.x * dropDistance,
                  entry.pieceLastParticleDropPos.y +
                      direction.y * dropDistance,
                  entry.pieceLastParticleDropPos.z +
                      direction.z * dropDistance,
              };
              std::uint32_t color = 0xffffffffu;
              gameLibEffects->effectsModel->AddParticles(
                  trail.decl.particleTrail, currentTime,
                  gameMillisecondsPerFrame, NextRandom(random),
                  particlePosition, particleAxis, particleVelocity, color,
                  false);
            }
            const float advancedDistance =
                static_cast<float>(dropCount) * trail.decl.trailSpacing;
            entry.pieceLastParticleDropPos.x +=
                direction.x * advancedDistance;
            entry.pieceLastParticleDropPos.y +=
                direction.y * advancedDistance;
            entry.pieceLastParticleDropPos.z +=
                direction.z * advancedDistance;
          }
        }
      }
    }
  }

  if (gameLibEffects == nullptr || decals.num <= 0) {
    return;
  }
  for (int decalIndex = 0; decalIndex < decals.num; ++decalIndex) {
    idDecalFX& decal = decals.list[decalIndex];
    const float minimumBounceVelocitySquared =
        decal.decl.minBounceVelocity * decal.decl.minBounceVelocity;
    for (int entryIndex = 0; entryIndex < decal.decl.pieces.num;
         ++entryIndex) {
      const int pieceIndex = decal.decl.pieces.list[entryIndex];
      idEffectPhysicsRigidBody* const body = rigidBodies.list[pieceIndex];
      if (!body->active) {
        continue;
      }
      idDecalFX::decalEntry_t& entry = decal.entries.list[entryIndex];
      if (entry.pieceStartTime == 0) {
        entry.pieceStartTime = currentTime;
        entry.pieceLastDecalDropPos =
            body->currentState.positionFiltered;
      }
      const idVec3 distance{
          entry.pieceLastDecalDropPos.x -
              body->currentState.positionFiltered.x,
          entry.pieceLastDecalDropPos.y -
              body->currentState.positionFiltered.y,
          entry.pieceLastDecalDropPos.z -
              body->currentState.positionFiltered.z,
      };
      if (decal.decl.decalSpacing > LengthSquared(distance)) {
        continue;
      }

      const idEffectPhysicsCollision& collision =
          collisions.list[pieceIndex];
      idVec3 collisionPoint = collision.collisionPoint;
      idVec3 collisionNormal = collision.collisionNormalVelocity;
      bool splatter =
          decal.decl.splatterOnBounce &&
          LengthSquared(collisionNormal) > minimumBounceVelocitySquared;
      if (decal.decl.splatterOnRest && body->settled) {
        if (!HasNonZeroBits(collisionNormal)) {
          collisionPoint = entry.lastCollisionPos;
          collisionNormal = entry.lastCollisionNormal;
        }
        splatter = true;
      }

      if (splatter) {
        const int materialCount = decal.decl.materials.num;
        int materialIndex = 0;
        if (materialCount != 0) {
          materialIndex = random.RandomInt() % materialCount;
        }
        decalParams_t parameters;
        parameters.decalMat = decal.decl.materials.list[materialIndex];
        parameters.depth = decal.decl.depth;
        const float sizeRandom = NextRandom(random);
        parameters.size.x =
            (decal.decl.maxSize.x - decal.decl.minSize.x) * sizeRandom +
            decal.decl.minSize.x;
        parameters.size.y =
            (decal.decl.maxSize.y - decal.decl.minSize.y) * sizeRandom +
            decal.decl.minSize.y;
        parameters.angle = 0.0f;
        parameters.lifeTime = decal.decl.lifetime;
        parameters.fadeOutTime = decal.decl.fadeOutStartTime;
        parameters.fadeInTime = decal.decl.fadeInEndTime;
        parameters.quad = false;
        parameters.overlap = true;
        parameters.overlapRadius = 0.0f;
        parameters.cost = 1;
        const float inverseNormalLength =
            InverseLength(LengthSquared(collisionNormal));
        const idVec3 normalizedNormal{
            collisionNormal.x * inverseNormalLength,
            collisionNormal.y * inverseNormalLength,
            collisionNormal.z * inverseNormalLength,
        };
        gameLibEffects->decalModel->AddDecalFromPoint(
            &parameters, currentTime, collisionPoint, normalizedNormal);
        entry.pieceLastDecalDropPos =
            body->currentState.positionFiltered;
        if (g_debugBreakableDecals.valueInteger != 0 &&
            GetGameLibRenderDebug() != nullptr) {
          const idVec3 end{
              collisionPoint.x + normalizedNormal.x * 15.0f,
              collisionPoint.y + normalizedNormal.y * 15.0f,
              collisionPoint.z + normalizedNormal.z * 15.0f,
          };
          GetGameLibRenderDebug()->DebugLine(
              idColor::colorRed, body->currentState.positionFiltered, end,
              10000, false);
        }
      }
      if (HasNonZeroBits(collisionNormal)) {
        entry.lastCollisionNormal = collisionNormal;
        entry.lastCollisionPos = collisionPoint;
      }
    }
  }
}

bool idEffectPhysicsBreakable::ActivateAllNow(
    const idVec3& origin, const idMat3& axis) {
  bool activated = false;
  for (int index = 0; index < rigidBodies.num; ++index) {
    if (!rigidBodies.list[index]->active) {
      ActivateNoisy(index, origin, axis);
      activated = true;
    }
  }
  return activated;
}

void idEffectPhysicsBreakable::CollideWith(
    const idVec3& impactPoint, const idPhysics* ownerPhysics,
    const idPhysics* attacker, float collisionScale) {
  if (ownerPhysics == nullptr || rigidBodies.num == 0) {
    return;
  }

  const float attackerMass = attacker->GetMass(-1);
  const idVec3 attackerVelocity = attacker->GetLinearVelocity(0);
  const idVec3 attackerMomentum{
      attackerMass * attackerVelocity.x,
      attackerMass * attackerVelocity.y,
      attackerMass * attackerVelocity.z,
  };
  const float momentumLengthSquared = LengthSquared(attackerMomentum);
  const float inverseMomentumLength =
      InverseLength(momentumLengthSquared);
  const float momentumLength =
      inverseMomentumLength * momentumLengthSquared;
  const idVec3 momentumDirection{
      attackerMomentum.x * inverseMomentumLength,
      attackerMomentum.y * inverseMomentumLength,
      attackerMomentum.z * inverseMomentumLength,
  };

  idList<int, 59> collisionPieces;
  for (int index = 0; index < rigidBodies.num; ++index) {
    idEffectPhysicsRigidBody* const body = rigidBodies.list[index];
    idBounds bodyBounds = ExpandShortBounds(body->SPObject.absBounds);
    if (!body->active) {
      idBounds transformed;
      transformed.FromTransformedBounds(bodyBounds,
                                        ownerPhysics->GetOrigin(0),
                                        ownerPhysics->GetAxis(0));
      bodyBounds = transformed;
    }
    const idBounds& attackerBounds = attacker->GetAbsBounds(-1);
    const idVec3 attackerExtents = attackerBounds.GetMaxExtents();
    const float expansion =
        (std::fmax(std::fmax(attackerExtents.x, attackerExtents.y),
                   attackerExtents.z) *
         collisionScale) +
        0.5f;
    if (BoundsIntersectExpanded(bodyBounds, attackerBounds, expansion) &&
        !body->active) {
      ActivateBody(body, ownerPhysics->GetOrigin(0),
                   ownerPhysics->GetAxis(0));
      collisionPieces.Append(index);
    }
  }

  const int activatedCount = collisionPieces.num;
  if (activatedCount == 0) {
    return;
  }
  const float impulseScale =
      ((15.0f * momentumLength) * 0.1f) /
      static_cast<float>(activatedCount);
  for (int listIndex = 0; listIndex < activatedCount; ++listIndex) {
    idEffectPhysicsRigidBody* const body =
        rigidBodies.list[collisionPieces.list[listIndex]];
    const idVec3 radial{
        body->currentState.positionFiltered.x - impactPoint.x,
        body->currentState.positionFiltered.y - impactPoint.y,
        body->currentState.positionFiltered.z - impactPoint.z,
    };
    const float inverseRadialLength = InverseLength(LengthSquared(radial));
    const float inverseOwnerMass =
        1.0f / ownerPhysics->GetMass(listIndex);
    const idVec3 impulse{
        ((radial.x * inverseRadialLength + momentumDirection.x * 9.0f) *
         impulseScale) *
            inverseOwnerMass,
        ((radial.y * inverseRadialLength + momentumDirection.y * 9.0f) *
         impulseScale) *
            inverseOwnerMass,
        ((radial.z * inverseRadialLength + momentumDirection.z * 9.0f) *
         impulseScale) *
            inverseOwnerMass,
    };
    const float pointZ =
        impactPoint.z + RandomCentered(random, g_breakableSpin.valueFloat);
    const float pointY =
        impactPoint.y + RandomCentered(random, g_breakableSpin.valueFloat);
    const float pointX =
        impactPoint.x + RandomCentered(random, g_breakableSpin.valueFloat);
    body->ApplyImpulse({pointX, pointY, pointZ}, impulse);
  }
}

bool idEffectPhysicsBreakable::ActivateFloatingPieces(
    const idVec3& origin, const idMat3& axis, float floor) {
  if (basePiece != nullptr || rigidBodies.num == 0) {
    return false;
  }

  idList<breakableIsland_t, 59> islands;
  int* const pending = static_cast<int*>(AllocateEffectScratch(
      sizeof(int) * static_cast<std::size_t>(rigidBodies.num)));
  int* const islandForBody = static_cast<int*>(AllocateEffectScratch(
      sizeof(int) * static_cast<std::size_t>(rigidBodies.num)));
  std::memset(islandForBody, 0xff,
              sizeof(int) * static_cast<std::size_t>(rigidBodies.num));

  for (int bodyIndex = 0; bodyIndex < rigidBodies.num; ++bodyIndex) {
    if (rigidBodies.list[bodyIndex]->active ||
        islandForBody[bodyIndex] != -1) {
      continue;
    }
    const int islandIndex = islands.num;
    islands.Append({islandIndex, 1.0e30f});
    int pendingCount = 1;
    pending[0] = bodyIndex;
    islandForBody[bodyIndex] = islandIndex;
    for (int pendingIndex = 0; pendingIndex < pendingCount;
         ++pendingIndex) {
      const int currentIndex = pending[pendingIndex];
      idBounds currentBounds =
          ExpandShortBounds(rigidBodies.list[currentIndex]->SPObject.absBounds);
      currentBounds[0].x += origin.x;
      currentBounds[0].y += origin.y;
      currentBounds[0].z += origin.z;
      currentBounds[1].x += origin.x;
      currentBounds[1].y += origin.y;
      currentBounds[1].z += origin.z;
      if (islands.list[islandIndex].lowestPoint > currentBounds[0].z) {
        islands.list[islandIndex].lowestPoint = currentBounds[0].z;
      }
      for (int candidateIndex = 0; candidateIndex < rigidBodies.num;
           ++candidateIndex) {
        if (candidateIndex == bodyIndex ||
            islandForBody[candidateIndex] != -1 ||
            rigidBodies.list[candidateIndex]->active) {
          continue;
        }
        idBounds candidateBounds = ExpandShortBounds(
            rigidBodies.list[candidateIndex]->SPObject.absBounds);
        candidateBounds[0].x += origin.x;
        candidateBounds[0].y += origin.y;
        candidateBounds[0].z += origin.z;
        candidateBounds[1].x += origin.x;
        candidateBounds[1].y += origin.y;
        candidateBounds[1].z += origin.z;
        if (BoundsIntersectExpanded(currentBounds, candidateBounds,
                                    0.050000001f)) {
          islandForBody[candidateIndex] = islandIndex;
          pending[pendingCount++] = candidateIndex;
        }
      }
    }
  }

  if (g_breakableDebugIslands.valueInteger != 0 && islands.num > 0) {
    static const idColor* const colors[] = {
        &idColor::colorBrown,  &idColor::colorRed,
        &idColor::colorGreen,  &idColor::colorWhite,
        &idColor::colorBlue,   &idColor::colorOrange,
        &idColor::colorPurple, &idColor::colorCyan,
    };
    for (int bodyIndex = 0; bodyIndex < rigidBodies.num; ++bodyIndex) {
      const int islandIndex = islandForBody[bodyIndex];
      if (islandIndex == -1) {
        continue;
      }
      idBounds bounds =
          ExpandShortBounds(rigidBodies.list[bodyIndex]->SPObject.absBounds);
      idBounds worldBounds;
      worldBounds.FromTransformedBounds(bounds, origin, axis);
      const idColor* const color =
          colors[islandIndex < 7 ? islandIndex : 7];
      GetGameLibRenderDebug()->DebugBounds(
          *reinterpret_cast<const idVec4*>(color), worldBounds,
          vec3_origin, 0, false);
    }
  }

  bool activated = false;
  for (int bodyIndex = 0; bodyIndex < rigidBodies.num; ++bodyIndex) {
    const int islandIndex = islandForBody[bodyIndex];
    if (islandIndex != -1 && !rigidBodies.list[bodyIndex]->active &&
        islands.list[islandIndex].lowestPoint > floor + 1.0f) {
      ActivateNoisy(bodyIndex, origin, axis);
      activated = true;
    }
  }
  FreeEffectScratch(islandForBody);
  FreeEffectScratch(pending);
  return activated;
}

void idEffectPhysicsBreakable::DrawCollisionModels(
    int single, const idVec3& offset, const idMat3& axis) {
  int first = 0;
  int end = rigidBodies.num;
  if (single >= 0) {
    first = single;
    end = single + 1;
  }
  for (int index = first; index < end; ++index) {
    idEffectPhysicsRigidBody* const body = rigidBodies.list[index];
    idVec3 bodyOrigin = body->currentState.positionFiltered;
    idMat3 bodyAxis = body->currentState.orientationFiltered;
    if (!body->active) {
      bodyOrigin = TransformPoint(bodyOrigin, offset, axis);
      bodyAxis *= axis;
    }
    collisionModelManager->DrawCollisionModel(
        GetCachedCollisionModel(broadPhase.traceModelCache,
                                body->properties->traceModelIndex),
        nullptr, nullptr, bodyOrigin, bodyAxis, vec3_origin, mat3_identity,
        0.0f, 1.0f, 0);
    if (GetGameLibRenderDebug() != nullptr) {
      va label("%d", index + 1);
      GetGameLibRenderDebug()->DebugText(
          label, bodyOrigin, 0.1f, idColor::colorWhite, 1, 0, false,
          false);
    }
  }
}

idEffectPhysicsProperties*
idEffectPhysicsBreakable::GetPropertiesForTraceModel(
    const idTraceModel& traceModel) {
  const int traceModelIndex = broadPhase.traceModelCache->AllocTraceModel(
      traceModel, model->surfaces.list[0].material);
  for (int index = 0; index < properties.num; ++index) {
    if (properties.list[index]->traceModelIndex == traceModelIndex) {
      return properties.list[index];
    }
  }
  idEffectPhysicsProperties* const result =
      AllocateEffectObject<idEffectPhysicsProperties>();
  result->Setup(&broadPhase, traceModelIndex, 10.0f);
  UpdateProperties(result);
  properties.Append(result);
  return result;
}

void idEffectPhysicsBreakable::AddParticle(
    const idDeclParticle* particle, int startTime,
    const idVec3& position) {
  if (particle != nullptr) {
    idBreakableParticle entry;
    entry.particle = particle;
    entry.pos = position;
    entry.startTime = startTime;
    particles.Append(entry);
  }
}

void idEffectPhysicsBreakable::EmitSmokeForEachPiece(
    const idDeclParticle* particle, int startTime) {
  for (int index = 0; index < rigidBodies.num; ++index) {
    if (rigidBodies.list[index]->active && particle != nullptr) {
      idBreakableParticle entry;
      entry.particle = particle;
      entry.pos = rigidBodies.list[index]->currentState.positionFiltered;
      entry.startTime = startTime;
      particles.Append(entry);
    }
  }
}

void idEffectPhysicsBreakable::StartExplosions(
    const idVec3& origin, const idMat3& axis, int currentTime) {
  if (decl == nullptr) {
    idLib::Warning(
        "idEffectPhysicsBreakable::StartExplosions() called with NULL decl");
    return;
  }
  for (int index = 0; index < explosions.num; ++index) {
    idExplosion& explosion = explosions.list[index];
    explosion.startTime = currentTime;
    if (explosion.localSpace) {
      explosion.worldPosition =
          TransformPoint(explosion.decl.position, origin, axis);
    }
    if (explosion.fxIndex >= 0 && explosion.fxIndex < explosionFX.num) {
      explosionFX.list[explosion.fxIndex].StartFX(
          explosion.worldPosition, mat3_identity, currentTime,
          explosion.fxCondition);
    }
  }
  const idVec3& deathPosition = explosions.num > 0
                                    ? explosions.list[0].worldPosition
                                    : vec3_origin;
  AddParticle(decl->settings.deathParticle, currentTime, deathPosition);
}

bool idEffectPhysicsBreakable::DamagePiece(
    int currentTime, int pieceIndex, float amount,
    const idVec3& position) {
  if (decl == nullptr) {
    return false;
  }
  if (!rigidBodies.list[pieceIndex]->active) {
    const int armor = decl->armoredPieces.num <= pieceIndex
                          ? 0
                          : decl->armoredPieces.list[pieceIndex];
    amount -= static_cast<float>(armor);
    if (amount <= 0.0f) {
      AddParticle(decl->settings.harmlessParticle, currentTime, position);
      return false;
    }
    if (decl->healthPieces.num <= pieceIndex) {
      AddParticle(decl->settings.impactParticle, currentTime, position);
      return true;
    }
    if (decl->healthPieces.list[pieceIndex] == -1) {
      AddParticle(decl->settings.harmlessParticle, currentTime, position);
      return false;
    }
    const int health = decl->healthPieces.list[pieceIndex];
    if (health > 0) {
      if (damage.num == 0) {
        damage.SetNum(rigidBodies.num, 0.0f);
      }
      damage.list[pieceIndex] += amount;
      if (static_cast<float>(health) > damage.list[pieceIndex]) {
        return false;
      }
    }
  }
  AddParticle(decl->settings.impactParticle, currentTime, position);
  return true;
}

void idEffectPhysicsBreakable::RemoveExplosions() {
  explosions.ClearFree();
  explosionFX.ClearFree();
}

void idEffectPhysicsBreakable::AddRayImpact(
    int currentTime, const idVec3& origin, const idMat3& axis,
    const idVec3& start, const idVec3& end, bool ignoreActivePieces) {
  const idVec3 ray{end.x - start.x, end.y - start.y, end.z - start.z};
  const float rayLengthSquared = LengthSquared(ray);
  const float inverseRayLength = InverseLength(rayLengthSquared);
  const idVec3 rayDirection{ray.x * inverseRayLength,
                            ray.y * inverseRayLength,
                            ray.z * inverseRayLength};
  const float rayLength = inverseRayLength * rayLengthSquared;
  if (rayLength == 0.0f) {
    AddParticle(decl != nullptr ? decl->settings.harmlessParticle : nullptr,
                currentTime, start);
    return;
  }

  for (int index = 0; index < rigidBodies.num; ++index) {
    idEffectPhysicsRigidBody* const body = rigidBodies.list[index];
    if (ignoreActivePieces && body->active) {
      continue;
    }
    idBounds bounds = ExpandShortBounds(body->SPObject.absBounds);
    if (!body->active) {
      idBounds transformed;
      transformed.FromTransformedBounds(bounds, origin, axis);
      bounds = transformed;
    }
    if (!bounds.LineIntersection(start, end)) {
      continue;
    }
    if (!body->active &&
        !DamagePiece(currentTime, index, rayLength, start)) {
      continue;
    }
    ActivateBody(body, origin, axis);
    const idVec3 radial{
        body->currentState.positionFiltered.x - origin.x,
        body->currentState.positionFiltered.y - origin.y,
        body->currentState.positionFiltered.z - origin.z,
    };
    const float inverseRadialLength = InverseLength(LengthSquared(radial));
    const idVec3 radialDirection{radial.x * inverseRadialLength,
                                 radial.y * inverseRadialLength,
                                 radial.z * inverseRadialLength};
    idVec3 impactDirection = rayDirection;
    if (Dot(radialDirection, rayDirection) < 0.0f) {
      idVec3 perpendicular = Cross(radialDirection, rayDirection);
      perpendicular.x = -perpendicular.x;
      perpendicular.y = -perpendicular.y;
      perpendicular.z = -perpendicular.z;
      const float inversePerpendicularLength =
          InverseLength(LengthSquared(perpendicular));
      impactDirection = {perpendicular.x * inversePerpendicularLength,
                         perpendicular.y * inversePerpendicularLength,
                         perpendicular.z * inversePerpendicularLength};
    }
    const idVec3 impulse{
        (impactDirection.x * 10.0f +
        g_breakableRayUpPercent.valueFloat * 0.0f) *
            rayLength,
        (impactDirection.y * 10.0f +
         g_breakableRayUpPercent.valueFloat * 0.0f) *
            rayLength,
        (impactDirection.z * 10.0f +
         g_breakableRayUpPercent.valueFloat * 1.0f) *
            rayLength,
    };
    body->ApplyImpulse(start, impulse);
  }
}

void idEffectPhysicsBreakable::AddExplosion(
    const idDeclParticle* particle, int startTime,
    const idVec3& position, float radius, float impulse, float falloff,
    float angular, int delay, int duration, bool startNow,
    int activateAllDelay, float distanceTimeScaleDelay, bool useLinear,
    const idVec3& endLinear, bool localSpace) {
  idExplosion* const explosion = explosions.Alloc();
  explosion->decl.particle = particle;
  explosion->decl.fx = nullptr;
  explosion->decl.position = position;
  explosion->decl.radius = radius;
  explosion->decl.impulse = impulse * 10.0f;
  explosion->decl.falloff = falloff;
  explosion->decl.angular = angular;
  explosion->decl.delay = delay;
  explosion->decl.duration = duration;
  explosion->decl.pieces.SetNum(rigidBodies.num);
  explosion->decl.activateAllDelay = activateAllDelay;
  explosion->decl.distTimeScaleDelay = distanceTimeScaleDelay;
  explosion->decl.useLinear = useLinear;
  explosion->decl.endLinear = endLinear;
  explosion->decl.randomLinear = {};
  explosion->decl.randomAngular = {};
  for (int index = 0; index < rigidBodies.num; ++index) {
    explosion->decl.pieces.list[index] = index;
  }
  explosion->startTime = startNow ? startTime : 0;
  explosion->localSpace = localSpace;
  explosion->worldPosition = position;
  explosion->effectsEnabled = false;
  explosion->forceApplied = false;
  explosion->fxIndex = -1;
  explosion->fxCondition = FX_NONE;
}

void idEffectPhysicsBreakable::InitExplosionsAndTrails(
    breakablePieceSound_t* pieceSound, idGameLibEffects* effects,
    float diversity) {
  if (decl == nullptr || model == nullptr) {
    return;
  }

  idRenderWorld* const renderWorld = common->RW();
  explosions.SetNum(decl->explosions.num);
  for (int index = 0; index < decl->explosions.num; ++index) {
    idExplosion& explosion = explosions.list[index];
    explosion.decl = decl->explosions.list[index];
    explosion.startTime = 0;
    explosion.worldPosition = explosion.decl.position;
    explosion.effectsEnabled = false;
    explosion.forceApplied = false;
    explosion.localSpace = true;
    explosion.fxIndex = -1;
    explosion.fxCondition = FX_NONE;
  }

  explosionFX.SetNum(decl->explosions.num);
  for (int index = 0; index < explosionFX.num; ++index) {
    fxEmitterSound_t fxSoundInfo;
    if (pieceSound != nullptr) {
      fxSoundInfo.emitter = pieceSound->emitter;
      fxSoundInfo.channel = pieceSound->channel;
    }
    explosionFX.list[index].Init(
        explosions.list[index].decl.fx, renderWorld, &fxSoundInfo, effects,
        diversity, nullptr, idStr(""));
    explosions.list[index].fxIndex = index;
  }

  trails.SetNum(decl->trails.num);
  for (int index = 0; index < decl->trails.num; ++index) {
    idTrail& trail = trails.list[index];
    trail.decl = decl->trails.list[index];
    trail.entries.SetNum(trail.decl.pieces.num);
    for (int entryIndex = 0; entryIndex < trail.entries.num;
         ++entryIndex) {
      trail.entries.list[entryIndex].pieceStartTime = 0;
      trail.entries.list[entryIndex].pieceLastParticleDropPos = vec3_origin;
    }
  }

  decals.SetNum(decl->decals.num);
  for (int index = 0; index < decl->decals.num; ++index) {
    idDecalFX& decal = decals.list[index];
    decal.decl = decl->decals.list[index];
    decal.entries.SetNum(decal.decl.pieces.num);
    for (int entryIndex = 0; entryIndex < decal.entries.num;
         ++entryIndex) {
      idDecalFX::decalEntry_t& entry = decal.entries.list[entryIndex];
      entry.pieceStartTime = 0;
      entry.pieceLastDecalDropPos = vec3_origin;
      entry.lastCollisionPos = vec3_origin;
      entry.lastCollisionNormal = vec3_origin;
    }
  }

  if (decl->sparks.materials.num > 0) {
    sparks.SetNum(rigidBodies.num);
    for (int index = 0; index < sparks.num; ++index) {
      sparks.list[index].size =
          (decl->sparks.sizeMax - decl->sparks.sizeMin) *
              NextRandom(random) +
          decl->sparks.sizeMin;
      sparks.list[index].velocityScale =
          (decl->sparks.velocityScaleMax -
           decl->sparks.velocityScaleMin) *
              NextRandom(random) +
          decl->sparks.velocityScaleMin;
      int materialIndex = 0;
      if (decl->sparks.materials.num > 1) {
        materialIndex = random.RandomInt(decl->sparks.materials.num);
      }
      sparks.list[index].material =
          decl->sparks.materials.list[materialIndex];
    }
  }
}

void idEffectPhysicsBreakable::Reset(bool skipModelReset) {
  if (model == nullptr) {
    return;
  }
  if (!skipModelReset) {
    model->Reset();
  }
  for (int index = 0; index < rigidBodies.num; ++index) {
    idVec3 position;
    idMat3 orientation;
    model->GetPieceTransform(index, position, orientation);
    idEffectPhysicsRigidBody* const body = rigidBodies.list[index];
    body->active = false;
    SetBodyPosition(body, position);
    body->SetOrientation(orientation);
    ZeroBodyMomentum(body);
  }
  for (int index = 0; index < properties.num; ++index) {
    UpdateProperties(properties.list[index]);
  }
  random.SetSeed(1103515245 * ScaledGameMilliseconds() + 12345);
  InitExplosionsAndTrails(&soundInfo, gameLibEffects,
                          static_cast<float>((random.GetSeed() >> 16) & 0x7fff) *
                              RANDOM_TO_UNIT);
}

void idEffectPhysicsBreakable::Init(
    idClip* clip, idTraceModelCache* traceModelCache,
    idGameLibEffects* effects,
    idRenderModelDiscreteAnimation* renderModel, int entityNumber,
    breakablePieceSound_t* pieceSound) {
  broadPhase.Init(clip, traceModelCache);
  gameLibEffects = effects;
  if (pieceSound != nullptr) {
    soundInfo = *pieceSound;
  }
  model = renderModel;
  decl = model->breakableDecl;
  properties.DeleteContents();
  rigidBodies.DeleteContents();

  const int pieceCount = model->modelData != nullptr
                             ? model->modelData->traceModels.num
                             : 0;
  rigidBodies.SetNum(pieceCount);
  collisions.SetNum(pieceCount);
  for (int index = 0; index < pieceCount; ++index) {
    idVec3 position;
    idMat3 orientation;
    idTraceModel traceModel;
    model->GetPieceTransform(index, position, orientation);
    model->GetPieceTraceModel(index, traceModel);
    traceModel.Shrink(0.1875f);
    const idEffectPhysicsProperties* const pieceProperties =
        GetPropertiesForTraceModel(traceModel);
    idEffectPhysicsRigidBody* const body =
        AllocateEffectObject<idEffectPhysicsRigidBody>();
    body->Setup(pieceProperties);
    SetBodyPosition(body, position);
    body->SetOrientation(orientation);
    ZeroBodyMomentum(body);
    body->SPObject.entityNum = entityNumber;
    rigidBodies.list[index] = body;
  }
  basePiece = model->modelData->baseModel;
  random.SetSeed(1103515245 * ScaledGameMilliseconds() + 12345);
  InitExplosionsAndTrails(&soundInfo, gameLibEffects,
                          static_cast<float>((random.GetSeed() >> 16) & 0x7fff) *
                              RANDOM_TO_UNIT);
}
