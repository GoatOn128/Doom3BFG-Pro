#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_ALLOC_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#include "engine/gamelib/effectphysics/effectphysicsbroadphase.h"

#include "engine/gamelib/effectphysics/effectphysicsbroadphase_abi.h"
#include "engine/gamelib/physics/tracemodelcache.h"

#undef WOLFTNO_RETAIL_IDROTATION_CLASS_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_ALLOC_ABI

#include <cstdint>
#include <cstring>
#include <emmintrin.h>
#include <xmmintrin.h>

#define WOLFTNO_STORE_SHORT_BOUNDS(destination, source)                         \
  do {                                                                          \
    const __m128 recoveredBiasFloat = _mm_set1_ps(32768.0f);                    \
    const __m128i recoveredBiasInt = _mm_set1_epi32(32768);                     \
    const __m128i recoveredZero = _mm_setzero_si128();                          \
    const __m128 recoveredMinimum =                                             \
        _mm_set_ps(0.0f, (source).b[0].z, (source).b[0].y,                     \
                   (source).b[0].x);                                            \
    const __m128 recoveredMaximum =                                             \
        _mm_set_ps(0.0f, (source).b[1].z, (source).b[1].y,                     \
                   (source).b[1].x);                                            \
    const __m128i recoveredPackedMinimum = _mm_packs_epi32(                    \
        _mm_sub_epi32(_mm_cvttps_epi32(_mm_add_ps(                             \
                          recoveredMinimum, recoveredBiasFloat)),              \
                      recoveredBiasInt),                                        \
        recoveredZero);                                                         \
    const __m128i recoveredPackedMaximum = _mm_packs_epi32(                    \
        _mm_add_epi32(_mm_cvttps_epi32(_mm_sub_ps(                             \
                          recoveredMaximum, recoveredBiasFloat)),              \
                      recoveredBiasInt),                                        \
        recoveredZero);                                                         \
    const __m128i recoveredBounds = _mm_or_si128(                              \
        recoveredPackedMinimum, _mm_slli_si128(recoveredPackedMaximum, 6));    \
    std::memcpy(&(destination), &recoveredBounds, sizeof(destination));         \
  } while (false)

void idEffectPhysicsBroadPhase::Init(idClip* clipValue,
                                     idTraceModelCache* cacheValue) {
  clip = clipValue;
  traceModelCache = cacheValue;
}

bool idEffectPhysicsBroadPhase::GetMotionResult(trace_t* result,
                                                idSPObject* object) {
  const bool motionResult = collisionModelManager->GetStepMoveResult(
      result, object->query, false);
  object->origin = result->endpos;
  object->axis = result->endAxis;
  object->axis.OrthoNormalizeSelf();
  return motionResult;
}

int idClipModel::GetNumCollisionModels() const {
  return collisionModel != nullptr ? 1 : traceModels.num;
}

idCollisionModel* idClipModel::GetCollisionModel(int index) const {
  if (collisionModel != nullptr) {
    return collisionModel;
  }
  if (index >= traceModels.num) {
    return NoTraceModelWarning();
  }
  return clip->traceModelCache->cache.list[traceModels.list[index]]
      ->collisionModel;
}

void idEffectPhysicsBroadPhase::RemoveOverlappingPair(idSPObject* first,
                                                      idSPObject* second) {
  int index = 0;
  if (first->overlap.num > 0) {
    while (first->overlap.list[index] != static_cast<short>(second->id)) {
      if (++index >= first->overlap.num) {
        break;
      }
    }
    if (index < first->overlap.num) {
      const int oldCount = first->overlap.num--;
      if (index < oldCount - 1) {
        first->overlap.list[index] = first->overlap.list[oldCount - 1];
      }
    }
  }

  index = 0;
  if (second->overlap.num > 0) {
    while (second->overlap.list[index] != static_cast<short>(first->id)) {
      if (++index >= second->overlap.num) {
        return;
      }
    }
    const int oldCount = second->overlap.num--;
    if (index < oldCount - 1) {
      second->overlap.list[index] = second->overlap.list[oldCount - 1];
    }
  }
}

int idEffectPhysicsBroadPhase::GetPositionedModels(
    const idSPObject* object, idPositionedCollisionModel* models,
    int maxModels) const {
  if (traceModelCache == nullptr) {
    return 0;
  }

  int numModels = 0;
  for (int overlapIndex = 0; overlapIndex < object->overlap.num;
       ++overlapIndex) {
    const int objectIndex = object->overlap.list[overlapIndex];
    idSPObject* const overlapObject = objects.list[objectIndex];
    if (overlapObject == nullptr || overlapObject->traceModelIndex == -1 ||
        overlapObject->motion.ignore) {
      continue;
    }

    idPositionedCollisionModel& model = models[numModels++];
    model.model =
        traceModelCache->cache.list[overlapObject->traceModelIndex]
            ->collisionModel;
    model.modelOrigin = overlapObject->origin;
    model.modelAxis = overlapObject->axis;
    model.modelEntityNum = 0x1FFF;
    model.modelPhysicsId = objectIndex;
    model.modelBodyId = 0;
    model.modelQuery = overlapObject->query;
    if (numModels >= maxModels) {
      break;
    }
  }
  return numModels;
}

void idEffectPhysicsBroadPhase::SubmitQueries(
    bool worldCollisionModelOnly) {
  if (traceModelCache == nullptr || clip == nullptr) {
    return;
  }

  idPositionedCollisionModel models[128];
  for (idPositionedCollisionModel& model : models) {
    model.modelQuery.offset = 0;
  }

  int numModels = 0;
  if (worldCollisionModelOnly) {
    idClipModel* const world = clip->world;
    for (int modelIndex = 0;
         modelIndex < world->GetNumCollisionModels(); ++modelIndex) {
      idPositionedCollisionModel& model = models[numModels++];
      model.model = world->GetCollisionModel(modelIndex);
      model.modelOrigin = world->origin;
      model.modelAxis = world->axis;
      model.modelEntityNum =
          static_cast<std::int32_t>(world->flags << 15) >> 17;
      model.modelPhysicsId = world->physicsId;
      model.modelBodyId = world->bodyId;
      model.modelContentsOverride = world->contents;
    }
  }

  for (int objectIndex = 0; objectIndex < objects.num; ++objectIndex) {
    idSPObject* const object = objects.list[objectIndex];
    if (object == nullptr || !object->motion.valid) {
      continue;
    }

    const idTraceModel* traceModels[1] = {
        traceModelCache->cache.list[object->traceModelIndex]->trm,
    };
    idBounds motionBounds;
    motionBounds.b[0].x = static_cast<float>(object->absBounds.b[0][0]);
    motionBounds.b[0].y = static_cast<float>(object->absBounds.b[0][1]);
    motionBounds.b[0].z = static_cast<float>(object->absBounds.b[0][2]);
    motionBounds.b[1].x = static_cast<float>(object->absBounds.b[1][0]);
    motionBounds.b[1].y = static_cast<float>(object->absBounds.b[1][1]);
    motionBounds.b[1].z = static_cast<float>(object->absBounds.b[1][2]);

    if (!worldCollisionModelOnly) {
      numModels = clip->GetPositionedModelsInternal(
          motionBounds.b[0], motionBounds.b[1], false, object->clipMask,
          object->entityNum, 0x1FFF, -1, 0, models, 128);
    }
    if (!object->motion.worldCollisionOnly) {
      numModels += GetPositionedModels(object, &models[numModels],
                                       128 - numModels);
    }

    struct RetailRotationStorage {
      idVec3 origin;
      idVec3 vec;
      float angle;
      idMat3 axis;
      bool axisValid;
      std::uint8_t padding[3];
    } rotationStorage;
    static_assert(sizeof(RetailRotationStorage) == sizeof(idRotation));

    rotationStorage.origin = object->origin;
    rotationStorage.vec = object->motion.rotationVec;
    rotationStorage.angle = object->motion.rotationAngle;
    rotationStorage.axis = mat3_identity;
    rotationStorage.axisValid = false;
    const idRotation& rotation =
        reinterpret_cast<const idRotation&>(rotationStorage);

    idVec3 end;
    end.x = object->origin.x + object->motion.translation.x;
    end.y = object->origin.y + object->motion.translation.y;
    end.z = object->origin.z + object->motion.translation.z;
    const idCollisionQuery query = collisionModelManager->Motion(
        nullptr, object->origin, end, rotation, motionBounds, traceModels, 1,
        object->axis, object->clipMask, models, numModels,
        "w:\\build\\tech5\\engine\\gamelib\\effectphysics\\EffectPhysicsBroadPhase.cpp(478) : Motion");
    object->motion.valid = false;
    object->query.offset = query.offset;
  }
}

idEffectPhysicsBroadPhase::idEffectPhysicsBroadPhase()
    : objects(), markers{} {
  clip = nullptr;
  traceModelCache = nullptr;
}

idEffectPhysicsBroadPhase::~idEffectPhysicsBroadPhase() {
  clip = nullptr;
  traceModelCache = nullptr;
}

void idEffectPhysicsBroadPhase::RemoveObject(idSPObject* object) {
  objects.list[object->id] = nullptr;
  const std::uint64_t resultOffset =
      object->query.offset & 0x003FFFFFFFFFFFFFULL;
  if (resultOffset >= idCollisionQuery::finalResultsFirstSubmittedOffset &&
      resultOffset < idCollisionQuery::finalResultsLastSubmittedOffset) {
    object->query.offset = 0;
  }

  if (object->motion.worldCollisionOnly) {
    return;
  }

  for (int axis = 0; axis < 3; ++axis) {
    idList<idSPMarker, 59>& axisMarkers = markers[axis];
    const int minimumIndex = object->index[0][axis];
    const int maximumIndex = object->index[1][axis];

    for (int sourceIndex = minimumIndex + 1;
         sourceIndex < maximumIndex; ++sourceIndex) {
      const idSPMarker shifted = axisMarkers.list[sourceIndex];
      const int shiftedObjectIndex = shifted.data & 0x7FFFu;
      const int shiftedEndpoint = shifted.data >> 15;
      objects.list[shiftedObjectIndex]->index[shiftedEndpoint][axis] =
          sourceIndex - 1;
      axisMarkers.list[sourceIndex - 1] = shifted;
      if (shiftedEndpoint != 0) {
        RemoveOverlappingPair(object, objects.list[shiftedObjectIndex]);
      }
    }

    for (int sourceIndex = maximumIndex + 1;
         sourceIndex < axisMarkers.num; ++sourceIndex) {
      const idSPMarker shifted = axisMarkers.list[sourceIndex];
      const int shiftedObjectIndex = shifted.data & 0x7FFFu;
      const int shiftedEndpoint = shifted.data >> 15;
      objects.list[shiftedObjectIndex]->index[shiftedEndpoint][axis] =
          sourceIndex - 2;
      axisMarkers.list[sourceIndex - 2] = shifted;
      if (shiftedEndpoint != 0) {
        RemoveOverlappingPair(object, objects.list[shiftedObjectIndex]);
      }
    }

    axisMarkers.SetNum(axisMarkers.num - 2);
    object->index[0][axis] = -1;
  }
}

void idEffectPhysicsBroadPhase::AddOverlappingPair(idSPObject* first,
                                                   idSPObject* second) {
  const __m128i firstMinimum =
      _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&first->absBounds));
  const __m128i firstMaximum = _mm_srli_si128(_mm_loadl_epi64(
                                                   reinterpret_cast<const __m128i*>(
                                                       &first->absBounds.b[0][2])),
                                               2);
  const __m128i secondMinimum =
      _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&second->absBounds));
  const __m128i secondMaximum = _mm_srli_si128(_mm_loadl_epi64(
                                                    reinterpret_cast<const __m128i*>(
                                                        &second->absBounds.b[0][2])),
                                                2);
  const __m128i separated = _mm_or_si128(
      _mm_cmpgt_epi16(secondMinimum, firstMaximum),
      _mm_cmpgt_epi16(firstMinimum, secondMaximum));
  if ((_mm_cvtsi128_si32(_mm_packs_epi16(separated, separated)) &
       0x00FFFFFF) != 0) {
    return;
  }

  const short secondId = static_cast<short>(second->id);
  int index = 0;
  while (index < first->overlap.num &&
         first->overlap.list[index] != secondId) {
    ++index;
  }
  if (index >= first->overlap.num) {
    first->overlap.Append(secondId);
  }

  const short firstId = static_cast<short>(first->id);
  index = 0;
  while (index < second->overlap.num &&
         second->overlap.list[index] != firstId) {
    ++index;
  }
  if (index >= second->overlap.num) {
    second->overlap.Append(firstId);
  }
}

void idEffectPhysicsBroadPhase::AddObject(idSPObject* object,
                                          const idVec3& origin,
                                          const idMat3& axis,
                                          const idBounds* bounds) {
  const idBounds* objectBounds = bounds;
  if (objectBounds == nullptr) {
    if (traceModelCache == nullptr) {
      return;
    }
    objectBounds =
        &traceModelCache->cache.list[object->traceModelIndex]->trm->bounds;
  }

  idBounds absoluteBounds;
  absoluteBounds.FromTransformedBounds(*objectBounds, origin, axis);
  object->origin = origin;
  object->axis = axis;
  WOLFTNO_STORE_SHORT_BOUNDS(object->absBounds, absoluteBounds);

  int objectIndex = 0;
  while (objectIndex < objects.num && objects.list[objectIndex] != nullptr) {
    ++objectIndex;
  }
  if (objectIndex >= objects.num) {
    objectIndex = objects.Append(object);
  } else {
    objects.list[objectIndex] = object;
  }
  object->id = objectIndex;

  if (object->motion.worldCollisionOnly) {
    return;
  }

  for (int axisIndex = 0; axisIndex < 3; ++axisIndex) {
    idList<idSPMarker, 59>& axisMarkers = markers[axisIndex];
    axisMarkers.Alloc();
    axisMarkers.Alloc();

    const short minimumValue = object->absBounds.b[0][axisIndex];
    const short maximumValue = object->absBounds.b[1][axisIndex];
    int maximumIndex = axisMarkers.num - 1;
    while (maximumIndex > 1 &&
           maximumValue < axisMarkers.list[maximumIndex - 2].value) {
      const idSPMarker shifted = axisMarkers.list[maximumIndex - 2];
      const int shiftedObjectIndex = shifted.data & 0x7FFFu;
      const int shiftedEndpoint = shifted.data >> 15;
      objects.list[shiftedObjectIndex]->index[shiftedEndpoint][axisIndex] =
          maximumIndex;
      axisMarkers.list[maximumIndex] = shifted;
      if (shiftedEndpoint != 0) {
        AddOverlappingPair(object, objects.list[shiftedObjectIndex]);
      } else {
        RemoveOverlappingPair(object, objects.list[shiftedObjectIndex]);
      }
      --maximumIndex;
    }

    int minimumIndex = maximumIndex - 1;
    while (minimumIndex > 0 &&
           minimumValue < axisMarkers.list[minimumIndex - 1].value) {
      const idSPMarker shifted = axisMarkers.list[minimumIndex - 1];
      const int shiftedObjectIndex = shifted.data & 0x7FFFu;
      const int shiftedEndpoint = shifted.data >> 15;
      objects.list[shiftedObjectIndex]->index[shiftedEndpoint][axisIndex] =
          minimumIndex;
      axisMarkers.list[minimumIndex] = shifted;
      if (shiftedEndpoint != 0) {
        AddOverlappingPair(object, objects.list[shiftedObjectIndex]);
      }
      --minimumIndex;
    }

    object->index[0][axisIndex] = minimumIndex;
    object->index[1][axisIndex] = maximumIndex;
    axisMarkers.list[minimumIndex].value = minimumValue;
    axisMarkers.list[minimumIndex].data =
        static_cast<unsigned short>(objectIndex);
    axisMarkers.list[maximumIndex].value = maximumValue;
    axisMarkers.list[maximumIndex].data =
        static_cast<unsigned short>(objectIndex) | 0x8000u;
  }
}

void idEffectPhysicsBroadPhase::MoveObject(idSPObject* object) {
  for (int axis = 0; axis < 3; ++axis) {
    idList<idSPMarker, 59>& axisMarkers = markers[axis];
    int minimumIndex = object->index[0][axis];
    int maximumIndex = object->index[1][axis];
    const short minimumValue = object->absBounds.b[0][axis];
    const short maximumValue = object->absBounds.b[1][axis];
    axisMarkers.list[minimumIndex].value = minimumValue;
    axisMarkers.list[maximumIndex].value = maximumValue;

    while (minimumIndex > 0 &&
           minimumValue < axisMarkers.list[minimumIndex - 1].value) {
      const idSPMarker shifted = axisMarkers.list[minimumIndex - 1];
      const int shiftedObjectIndex = shifted.data & 0x7FFFu;
      const int shiftedEndpoint = shifted.data >> 15;
      objects.list[shiftedObjectIndex]->index[shiftedEndpoint][axis] =
          minimumIndex;
      axisMarkers.list[minimumIndex - 1] = axisMarkers.list[minimumIndex];
      axisMarkers.list[minimumIndex] = shifted;
      if (shiftedEndpoint != 0) {
        AddOverlappingPair(object, objects.list[shiftedObjectIndex]);
      }
      --minimumIndex;
    }

    while (maximumIndex < axisMarkers.num - 1 &&
           maximumValue > axisMarkers.list[maximumIndex + 1].value) {
      const idSPMarker shifted = axisMarkers.list[maximumIndex + 1];
      const int shiftedObjectIndex = shifted.data & 0x7FFFu;
      const int shiftedEndpoint = shifted.data >> 15;
      objects.list[shiftedObjectIndex]->index[shiftedEndpoint][axis] =
          maximumIndex;
      axisMarkers.list[maximumIndex + 1] = axisMarkers.list[maximumIndex];
      axisMarkers.list[maximumIndex] = shifted;
      if (shiftedEndpoint == 0) {
        AddOverlappingPair(object, objects.list[shiftedObjectIndex]);
      }
      ++maximumIndex;
    }

    while (minimumValue > axisMarkers.list[minimumIndex + 1].value) {
      const idSPMarker shifted = axisMarkers.list[minimumIndex + 1];
      const int shiftedObjectIndex = shifted.data & 0x7FFFu;
      const int shiftedEndpoint = shifted.data >> 15;
      objects.list[shiftedObjectIndex]->index[shiftedEndpoint][axis] =
          minimumIndex;
      axisMarkers.list[minimumIndex + 1] = axisMarkers.list[minimumIndex];
      axisMarkers.list[minimumIndex] = shifted;
      if (shiftedEndpoint != 0) {
        RemoveOverlappingPair(object, objects.list[shiftedObjectIndex]);
      }
      ++minimumIndex;
    }

    while (maximumValue < axisMarkers.list[maximumIndex - 1].value) {
      const idSPMarker shifted = axisMarkers.list[maximumIndex - 1];
      const int shiftedObjectIndex = shifted.data & 0x7FFFu;
      const int shiftedEndpoint = shifted.data >> 15;
      objects.list[shiftedObjectIndex]->index[shiftedEndpoint][axis] =
          maximumIndex;
      axisMarkers.list[maximumIndex - 1] = axisMarkers.list[maximumIndex];
      axisMarkers.list[maximumIndex] = shifted;
      if (shiftedEndpoint == 0) {
        RemoveOverlappingPair(object, objects.list[shiftedObjectIndex]);
      }
      --maximumIndex;
    }

    object->index[0][axis] = minimumIndex;
    object->index[1][axis] = maximumIndex;
  }
}

void idEffectPhysicsBroadPhase::SetObjectPosition(
    idSPObject* object, const idVec3& origin, const idMat3& axis,
    const idBounds* bounds) {
  const idBounds* objectBounds = bounds;
  if (objectBounds == nullptr) {
    if (traceModelCache == nullptr) {
      return;
    }
    objectBounds =
        &traceModelCache->cache.list[object->traceModelIndex]->trm->bounds;
  }

  idBounds absoluteBounds;
  absoluteBounds.FromTransformedBounds(*objectBounds, origin, axis);
  WOLFTNO_STORE_SHORT_BOUNDS(object->absBounds, absoluteBounds);
  object->origin = origin;
  object->axis = axis;
  if (!object->motion.worldCollisionOnly) {
    MoveObject(object);
  }
  object->motion.valid = false;

  const std::uint64_t resultOffset =
      object->query.offset & 0x003FFFFFFFFFFFFFULL;
  if (resultOffset >= idCollisionQuery::finalResultsFirstSubmittedOffset &&
      resultOffset < idCollisionQuery::finalResultsLastSubmittedOffset) {
    object->query.offset = 0;
  }
}

void idEffectPhysicsBroadPhase::ApplyMotion(idSPObject* object,
                                            const idVec3& translation,
                                            const idRotation& rotation) {
  if (traceModelCache == nullptr) {
    return;
  }

  idBounds motionBounds;
  motionBounds.FromBoundsRotation(
      traceModelCache->cache.list[object->traceModelIndex]->trm->bounds,
      object->origin, object->axis, rotation);

  if (translation.x >= 0.0f) {
    motionBounds.b[1].x += translation.x;
  } else {
    motionBounds.b[0].x += translation.x;
  }
  motionBounds.b[0].x -= 1.0f;
  motionBounds.b[1].x += 1.0f;
  if (translation.y >= 0.0f) {
    motionBounds.b[1].y += translation.y;
  } else {
    motionBounds.b[0].y += translation.y;
  }
  motionBounds.b[0].y -= 1.0f;
  motionBounds.b[1].y += 1.0f;
  if (translation.z >= 0.0f) {
    motionBounds.b[1].z += translation.z;
  } else {
    motionBounds.b[0].z += translation.z;
  }
  motionBounds.b[0].z -= 1.0f;
  motionBounds.b[1].z += 1.0f;

  WOLFTNO_STORE_SHORT_BOUNDS(object->absBounds, motionBounds);
  if (!object->motion.worldCollisionOnly) {
    MoveObject(object);
  }
  object->motion.valid = true;
  object->motion.translation = translation;
  object->motion.rotationVec = rotation.vec;
  object->motion.rotationAngle = rotation.angle;
}

#undef WOLFTNO_STORE_SHORT_BOUNDS
