#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#include "engine/gamelib/physics/buoyancy.h"

#include "engine/framework/common.h"
#include "engine/gamelib/physics/tracemodelcache.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <emmintrin.h>
#include <new>

idCVar buoy_damping("buoy_damping", "0.002", CVAR_FLOAT,
                    "velocity damping in buoyancy",
                    static_cast<argCompletionRef_t>(nullptr));

namespace {

template <typename Function, typename Object>
Function VirtualFunction(Object* object, std::size_t byteOffset) {
  const auto table =
      *reinterpret_cast<const void* const* const*>(object);
  return reinterpret_cast<Function>(
      const_cast<void*>(table[byteOffset / sizeof(void*)]));
}

idTraceModelCache::trmCache_t* TraceModelCacheFor(
    const idClipModel* clipModel) {
  const int traceModelIndex = *clipModel->traceModels.list;
  return clipModel->clip->traceModelCache->cache.list[traceModelIndex];
}

traceModelWater_t* WaterPointsFor(const idClipModel* clipModel) {
  if (clipModel->traceModels.num > 0) {
    idTraceModelCache::trmCache_t* const cache =
        TraceModelCacheFor(clipModel);
    if (cache->hasWater) {
      return cache->waterPoints.list;
    }
  }
  return nullptr;
}

idClipModel* PhysicsGetClipModel(idPhysics* physics, int bodyId) {
  using Function = idClipModel* (*)(idPhysics*, int);
  return VirtualFunction<Function>(physics, 0x10)(physics, bodyId);
}

const idBounds* PhysicsGetBounds(idPhysics* physics, int bodyId) {
  using Function = const idBounds* (*)(idPhysics*, int);
  return VirtualFunction<Function>(physics, 0x50)(physics, bodyId);
}

const idBounds* PhysicsGetAbsBounds(idPhysics* physics, int bodyId) {
  using Function = const idBounds* (*)(idPhysics*, int);
  return VirtualFunction<Function>(physics, 0x58)(physics, bodyId);
}

const idVec3* PhysicsGetOrigin(idPhysics* physics, int bodyId) {
  using Function = const idVec3* (*)(idPhysics*, int);
  return VirtualFunction<Function>(physics, 0x80)(physics, bodyId);
}

void PhysicsGetLinearVelocity(idPhysics* physics, idVec3* result,
                              int bodyId) {
  using Function = void (*)(idPhysics*, idVec3*, int);
  VirtualFunction<Function>(physics, 0xB0)(physics, result, bodyId);
}

void PhysicsGetAngularVelocity(idPhysics* physics, idVec3* result,
                               int bodyId) {
  using Function = void (*)(idPhysics*, idVec3*, int);
  VirtualFunction<Function>(physics, 0xB8)(physics, result, bodyId);
}

const idVec3* PhysicsGetGravity(idPhysics* physics) {
  using Function = const idVec3* (*)(idPhysics*);
  return VirtualFunction<Function>(physics, 0xC8)(physics);
}

void PhysicsSetWaterLevel(idPhysics* physics, float level, int bodyId,
                          bool ignoreSurfaceForMovement, int waterEntity,
                          float surfaceHeight) {
  using Function = void (*)(idPhysics*, float, int, bool, int, float);
  VirtualFunction<Function>(physics, 0xD8)(
      physics, level, bodyId, ignoreSurfaceForMovement, waterEntity,
      surfaceHeight);
}

void PhysicsSetWaterViscosity(idPhysics* physics, float viscosity,
                              int bodyId) {
  using Function = void (*)(idPhysics*, float, int);
  VirtualFunction<Function>(physics, 0xE8)(physics, viscosity, bodyId);
}

float PhysicsGetBuoyancyScale(idPhysics* physics) {
  using Function = float (*)(idPhysics*);
  return VirtualFunction<Function>(physics, 0x108)(physics);
}

void PhysicsApplyImpulse(idPhysics* physics, int bodyId,
                         const idVec3* point, const idVec3* impulse) {
  using Function = void (*)(idPhysics*, int, const idVec3*, const idVec3*);
  VirtualFunction<Function>(physics, 0x118)(physics, bodyId, point, impulse);
}

bool PhysicsIsPushable(idPhysics* physics, int clipMask) {
  using Function = bool (*)(idPhysics*, int);
  return VirtualFunction<Function>(physics, 0x140)(physics, clipMask);
}

void CallbacksApplyDamage(idPhysicsCallbacks* callbacks, int physicsId,
                          int bodyId, const idDeclDamage* damage) {
  using Function = void (*)(idPhysicsCallbacks*, int, int,
                            const idDeclDamage*);
  VirtualFunction<Function>(callbacks, 0x40)(callbacks, physicsId, bodyId,
                                             damage);
}

void CallbacksApplyWaterEffects(idPhysicsCallbacks* callbacks, int physicsId,
                                int bodyId) {
  using Function = void (*)(idPhysicsCallbacks*, int, int);
  VirtualFunction<Function>(callbacks, 0x58)(callbacks, physicsId, bodyId);
}

void CallbacksApplyWaterSplashEffects(idPhysicsCallbacks* callbacks,
                                      int physicsId, int bodyId,
                                      surfTypes_t surface,
                                      waterSplash_t splash) {
  using Function = void (*)(idPhysicsCallbacks*, int, int, surfTypes_t,
                            waterSplash_t);
  VirtualFunction<Function>(callbacks, 0x60)(callbacks, physicsId, bodyId,
                                             surface, splash);
}

int CollisionModelGetPolytopes(idCollisionModel* model, int* numPlanes,
                               int maxPolytopes, idPlane* planes,
                               int maxPlanes) {
  using Function = int (*)(idCollisionModel*, int*, int, idPlane*, int);
  return VirtualFunction<Function>(model, 0x78)(
      model, numPlanes, maxPolytopes, planes, maxPlanes);
}

struct retailGame_t {
  const void* const* vtable;
};

retailGame_t* CommonGame() {
  using Function = retailGame_t* (*)(idCommon*);
  return VirtualFunction<Function>(common, 0x1F8)(common);
}

void* GameGetEntity(retailGame_t* game, int entityNumber) {
  using Function = void* (*)(retailGame_t*, int);
  return VirtualFunction<Function>(game, 0x450)(game, entityNumber);
}

const char* GameGetEntityName(retailGame_t* game, void* entity) {
  using Function = const char* (*)(retailGame_t*, void*);
  return VirtualFunction<Function>(game, 0x458)(game, entity);
}

bool ContainsBuoyantModel(const idList<buoyantClipModel_t, 5>& list,
                          const buoyantClipModel_t& model) {
  for (int index = 0; index < list.num; ++index) {
    if (list.list[index].physicsId == model.physicsId &&
        list.list[index].bodyId == model.bodyId) {
      return true;
    }
  }
  return false;
}

__m128 HorizontalSumRetail(__m128 value) {
  value = _mm_add_ps(
      value, _mm_castsi128_ps(_mm_shuffle_epi32(
                 _mm_castps_si128(value), 0x4E)));
  return _mm_add_ps(
      _mm_castsi128_ps(
          _mm_shuffle_epi32(_mm_castps_si128(value), 0xB1)),
      value);
}

} // namespace

template <>
idList<buoyantClipModel_t, 5>&
idList<buoyantClipModel_t, 5>::operator=(
    const idList<buoyantClipModel_t, 5>& other) {
  if (this == &other) {
    return *this;
  }

  if (listStatic == 0 || listStatic == 2) {
    if (list != nullptr) {
      idMem::Singleton().Free(list, ALIGN_16);
    }
    list = nullptr;
    size = 0;
  }
  num = 0;

  if (listStatic != 0) {
    num = other.num < size ? other.num : size;
  } else {
    num = other.num;
    size = other.size;
    granularity = other.granularity;
    memTag = other.memTag;
    if (size > 0) {
      idMem& memory = idMem::Singleton();
      buoyantClipModel_t* replacement =
          static_cast<buoyantClipModel_t*>(memory.AllocWithLocation(
              "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : tag",
              sizeof(buoyantClipModel_t) * static_cast<std::size_t>(size),
              static_cast<memTag_t>(memTag), false, ALIGN_16,
              HEAP_DEFAULTHEAP));
      if (replacement != nullptr) {
        for (int index = 0; index < size; ++index) {
          ::new (static_cast<void*>(&replacement[index])) buoyantClipModel_t;
        }
      }
      list = replacement;
    }
  }

  for (int index = 0; index < num; ++index) {
    list[index] = other.list[index];
  }
  return *this;
}

void idBuoyancy::CalculateBuoyancyWeights(
    const int* polytopeNumPlanes, const idPlane* polytopePlanes,
    int numPolytopes, const idClipModel* clipModel, float* const weights) {
  traceModelWater_t* const waterPoints = WaterPointsFor(clipModel);
  alignas(16) float waterPointWeights[64];
  alignas(16) float weightsForPolytope[64];

  for (int index = 0; index < 64; ++index) {
    waterPointWeights[index] = waterPoints[index].weight;
  }
  std::memset(weights, 0, sizeof(float) * 64);

  const __m128 xyzMask = _mm_castsi128_ps(_mm_set_epi32(0, -1, -1, -1));
  const __m128 wMask = _mm_castsi128_ps(_mm_set_epi32(-1, 0, 0, 0));
  const __m128 zero = _mm_setzero_ps();
  const __m128 modelOrigin =
      _mm_and_ps(_mm_loadu_ps(&clipModel->origin.x), xyzMask);
  const __m128 modelAxis0 =
      _mm_and_ps(_mm_loadu_ps(&clipModel->axis.mat[0].x), xyzMask);
  const __m128 modelAxis1 =
      _mm_and_ps(_mm_loadu_ps(&clipModel->axis.mat[1].x), xyzMask);
  const __m128 modelAxis2 =
      _mm_and_ps(_mm_loadu_ps(&clipModel->axis.mat[2].x), xyzMask);

  int firstPlane = 0;
  for (int polytope = 0; polytope < numPolytopes; ++polytope) {
    std::memmove(weightsForPolytope, waterPointWeights,
                 sizeof(weightsForPolytope));
    const int numPlanes = polytopeNumPlanes[polytope];

    for (int planeIndex = 0; planeIndex < numPlanes;
         ++planeIndex, ++firstPlane) {
      __m128 localPlane = _mm_load_ps(
          &polytopePlanes[firstPlane].a);
      const __m128 translatedDistance = _mm_and_ps(
          HorizontalSumRetail(_mm_mul_ps(localPlane, modelOrigin)), wMask);
      localPlane = _mm_add_ps(localPlane, translatedDistance);

      const __m128 planeDistance =
          _mm_shuffle_ps(localPlane, localPlane, 0xFF);
      const __m128 planeX =
          HorizontalSumRetail(_mm_mul_ps(localPlane, modelAxis0));
      const __m128 planeY =
          HorizontalSumRetail(_mm_mul_ps(localPlane, modelAxis1));
      const __m128 planeZ =
          HorizontalSumRetail(_mm_mul_ps(localPlane, modelAxis2));

      for (int point = 0; point < 64; point += 4) {
        const __m128 x = _mm_set_ps(
            waterPoints[point + 3].xyz.x, waterPoints[point + 2].xyz.x,
            waterPoints[point + 1].xyz.x, waterPoints[point].xyz.x);
        const __m128 y = _mm_set_ps(
            waterPoints[point + 3].xyz.y, waterPoints[point + 2].xyz.y,
            waterPoints[point + 1].xyz.y, waterPoints[point].xyz.y);
        const __m128 z = _mm_set_ps(
            waterPoints[point + 3].xyz.z, waterPoints[point + 2].xyz.z,
            waterPoints[point + 1].xyz.z, waterPoints[point].xyz.z);
        const __m128 distance = _mm_add_ps(
            _mm_add_ps(_mm_add_ps(_mm_mul_ps(x, planeX), planeDistance),
                       _mm_mul_ps(y, planeY)),
            _mm_mul_ps(z, planeZ));
        __m128 currentWeights = _mm_load_ps(&weightsForPolytope[point]);
        currentWeights =
            _mm_and_ps(_mm_cmplt_ps(distance, zero), currentWeights);
        _mm_store_ps(&weightsForPolytope[point], currentWeights);
      }
    }

    for (int point = 0; point < 64; ++point) {
      weights[point] = weightsForPolytope[point] + weights[point];
    }
  }
}

void idBuoyancy::ApplyBuoyancy(
    idClip* clip, idPhysics* physics, float timeStep,
    const idDeclDamage* waterDamage, const idVec3& waterCurrent,
    float waterDensity, float waterViscosity,
    const idVec3& gravityVector, int clipMask,
    surfTypes_t surfOverride, bool ignoreSurfaceForMovement) {
  idStaticList<buoyantClipModel_t, 128> newBuoyantClipModels;
  idClipModel* clipModels[8192];

  const idBounds* const absoluteBounds = PhysicsGetAbsBounds(physics, -1);
  const int numTouching = clip->GetClipModelsTouchingBounds(
      *absoluteBounds, clipMask, clipModels, 0x2000);

  int numBuoyant = 0;
  if (numTouching > 0) {
    for (int index = 0; index < numTouching; ++index) {
      idClipModel* const clipModel = clipModels[index];
      if (clipModel->traceModels.num <= 0) {
        continue;
      }
      idTraceModelCache::trmCache_t* const cache =
          TraceModelCacheFor(clipModel);
      if (!cache->hasWater || cache->waterPoints.list == nullptr) {
        continue;
      }
      idPhysics* const buoyantPhysics =
          idPhysics::GetPhysicsForId(clipModel->physicsId);
      if (buoyantPhysics == nullptr || buoyantPhysics->type == PHYSICS_STATIC ||
          buoyantPhysics->type == PHYSICS_STATICMULTI ||
          !(PhysicsGetBuoyancyScale(buoyantPhysics) > idMath::FLOAT_EPSILON) ||
          !PhysicsIsPushable(buoyantPhysics, clipMask)) {
        continue;
      }
      clipModels[numBuoyant++] = clipModel;
    }

    if (numBuoyant == 0) {
      return;
    }

    idClipModel* const firstWaterClipModel = PhysicsGetClipModel(physics, 0);
    idCollisionModel* collisionModel = nullptr;
    if (firstWaterClipModel != nullptr) {
      collisionModel = firstWaterClipModel->collisionModel;
      if (collisionModel == nullptr) {
        if (firstWaterClipModel->traceModels.num <= 0) {
          collisionModel = firstWaterClipModel->NoTraceModelWarning();
        } else {
          collisionModel = TraceModelCacheFor(firstWaterClipModel)->collisionModel;
        }
      }
    }

    if (collisionModel == nullptr) {
      retailGame_t* const firstGame = CommonGame();
      void* const entity = GameGetEntity(firstGame, physics->entityNumber);
      const char* entityName = "<unknown>";
      if (entity != nullptr) {
        retailGame_t* const secondGame = CommonGame();
        entityName = GameGetEntityName(secondGame, entity);
      }
      idLib::Warning(
          "Entity '%s' does not have a valid collision model set for buoyancy",
          entityName);
      return;
    }

    int polytopeNumPlanes[32];
    alignas(16) idPlane polytopePlanes[192];
    const int numPolytopes = CollisionModelGetPolytopes(
        collisionModel, polytopeNumPlanes, 32, polytopePlanes, 192);
    if (numPolytopes == 0) {
      return;
    }

    idClipModel* const originClipModel = PhysicsGetClipModel(physics, 0);
    idClipModel* const axisClipModel = PhysicsGetClipModel(physics, 0);
    int flatPlane = 0;
    for (int polytope = 0; polytope < numPolytopes; ++polytope) {
      for (int planeIndex = 0; planeIndex < polytopeNumPlanes[polytope];
           ++planeIndex, ++flatPlane) {
        idPlane& plane = polytopePlanes[flatPlane];
        const float a = plane.a;
        const float b = plane.b;
        const float c = plane.c;
        plane.d = plane.d -
                  ((b * originClipModel->origin.y +
                    a * originClipModel->origin.x) +
                   c * originClipModel->origin.z);
        plane.a = (a * axisClipModel->axis.mat[0].x +
                   b * axisClipModel->axis.mat[1].x) +
                  c * axisClipModel->axis.mat[2].x;
        plane.b = (a * axisClipModel->axis.mat[0].y +
                   b * axisClipModel->axis.mat[1].y) +
                  c * axisClipModel->axis.mat[2].y;
        plane.c = (a * axisClipModel->axis.mat[0].z +
                   b * axisClipModel->axis.mat[1].z) +
                  c * axisClipModel->axis.mat[2].z;
      }
    }

    const float damping = buoy_damping.valueFloat;
    for (int index = 0; index < numBuoyant; ++index) {
      idClipModel* const clipModel = clipModels[index];
      traceModelWater_t* const waterPoints = WaterPointsFor(clipModel);
      idPhysics* const buoyantPhysics =
          idPhysics::GetPhysicsForId(clipModel->physicsId);
      const float buoyancyScale = PhysicsGetBuoyancyScale(buoyantPhysics);

      alignas(16) float weights[64];
      CalculateBuoyancyWeights(polytopeNumPlanes, polytopePlanes,
                               numPolytopes, clipModel, weights);

      const float volume = clipModel->traceModels.num <= 0
                               ? 0.0f
                               : TraceModelCacheFor(clipModel)->volume;
      const float displacedMass = volume * waterDensity;
      const idVec3* const gravity = buoyantPhysics->type == PHYSICS_PLAYER
                                        ? PhysicsGetGravity(physics)
                                        : &gravityVector;
      const float buoyancyZ =
          timeStep * (waterCurrent.z - displacedMass * gravity->z);
      const float buoyancyY =
          timeStep * (waterCurrent.y - displacedMass * gravity->y);
      const float buoyancyX =
          timeStep * (waterCurrent.x - displacedMass * gravity->x);

      idVec3 linearVelocity;
      idVec3 angularVelocity;
      PhysicsGetLinearVelocity(buoyantPhysics, &linearVelocity,
                               clipModel->bodyId);
      PhysicsGetAngularVelocity(buoyantPhysics, &angularVelocity,
                                clipModel->bodyId);
      const idVec3 physicsOrigin =
          *PhysicsGetOrigin(buoyantPhysics, clipModel->bodyId);

      float totalWeight = 0.0f;
      for (int pointIndex = 0; pointIndex < 64; ++pointIndex) {
        const float weight = weights[pointIndex];
        if (!(weight > 0.0f)) {
          continue;
        }

        const idVec3& localPoint = waterPoints[pointIndex].xyz;
        idVec3 point;
        point.z = ((localPoint.x * clipModel->axis.mat[0].z +
                    localPoint.y * clipModel->axis.mat[1].z) +
                   localPoint.z * clipModel->axis.mat[2].z) +
                  clipModel->origin.z;
        point.y = ((localPoint.x * clipModel->axis.mat[0].y +
                    localPoint.y * clipModel->axis.mat[1].y) +
                   localPoint.z * clipModel->axis.mat[2].y) +
                  clipModel->origin.y;
        point.x = ((localPoint.y * clipModel->axis.mat[1].x +
                    localPoint.x * clipModel->axis.mat[0].x) +
                   localPoint.z * clipModel->axis.mat[2].x) +
                  clipModel->origin.x;

        const float relativeZ = point.z - physicsOrigin.z;
        const float relativeY = point.y - physicsOrigin.y;
        const float relativeX = point.x - physicsOrigin.x;
        const float velocityX =
            (angularVelocity.y * relativeZ -
             angularVelocity.z * relativeY) +
            linearVelocity.x;
        const float velocityY =
            linearVelocity.y +
            (angularVelocity.z * relativeX -
             relativeZ * angularVelocity.x);
        const float velocityZ =
            linearVelocity.z +
            (relativeY * angularVelocity.x -
             angularVelocity.y * relativeX);

        idVec3 impulse;
        impulse.x =
            (weight * buoyancyX - velocityX * damping) * buoyancyScale;
        impulse.y =
            (weight * buoyancyY - velocityY * damping) * buoyancyScale;
        impulse.z =
            (weight * buoyancyZ - velocityZ * damping) * buoyancyScale;
        PhysicsApplyImpulse(buoyantPhysics, clipModel->bodyId, &point,
                            &impulse);
        totalWeight = totalWeight + weight;
      }

      const idBounds* const waterBounds = PhysicsGetBounds(physics, -1);
      const idVec3* const waterOrigin = PhysicsGetOrigin(physics, 0);
      const float waterSurfaceHeight =
          waterOrigin->z + waterBounds->b[1].z;
      PhysicsSetWaterLevel(buoyantPhysics, totalWeight, clipModel->bodyId,
                           ignoreSurfaceForMovement, physics->entityNumber,
                           waterSurfaceHeight);
      PhysicsSetWaterViscosity(buoyantPhysics, waterViscosity,
                               clipModel->bodyId);
      CallbacksApplyDamage(buoyantPhysics->callbacks, physics->physicsId,
                           clipModel->bodyId, waterDamage);

      if (totalWeight > 0.0f) {
        CallbacksApplyWaterEffects(buoyantPhysics->callbacks,
                                   physics->physicsId, clipModel->bodyId);
        const buoyantClipModel_t current{clipModel->physicsId,
                                         clipModel->bodyId};
        newBuoyantClipModels.Append(current);
        if (!ContainsBuoyantModel(buoyantClipModels, current)) {
          CallbacksApplyWaterSplashEffects(
              buoyantPhysics->callbacks, physics->physicsId,
              clipModel->bodyId, surfOverride, WATER_ENTERED);
        }
      }
    }
  }

  for (int index = 0; index < buoyantClipModels.num; ++index) {
    const buoyantClipModel_t& previous = buoyantClipModels.list[index];
    if (ContainsBuoyantModel(newBuoyantClipModels, previous)) {
      continue;
    }
    idPhysics* const previousPhysics =
        idPhysics::GetPhysicsForId(previous.physicsId);
    if (previousPhysics != nullptr) {
      PhysicsSetWaterLevel(previousPhysics, 0.0f, previous.bodyId, false,
                           physics->entityNumber, 0.0f);
      PhysicsSetWaterViscosity(previousPhysics, 0.0f, previous.bodyId);
    }
  }

  static_cast<idList<buoyantClipModel_t, 5>&>(buoyantClipModels) =
      newBuoyantClipModels;
  for (int index = 0; index < 128; ++index) {
    buoyantClipModels.staticList[index] =
        newBuoyantClipModels.staticList[index];
  }
}
