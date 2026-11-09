#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/physics/tracemodelcache.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include "engine/gamelib/physics/tracemodelcache_abi.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/lib.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>

extern bool forceIdListsToReallocateEveryAppend;

struct waterPoint_t {
  idVec3 xyz;
  float weight;
  float squareWeight;
};

static_assert(sizeof(waterPoint_t) == 0x14,
              "water-point ABI drift");

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

template <>
void* idListArrayResize<waterPoint_t>(void* oldPointer, int oldCount,
                                      int newCount, memTag_t tag,
                                      bool zeroBuffer);

namespace {

int Add32(int left, int right) {
  return static_cast<int>(static_cast<std::uint32_t>(left) +
                          static_cast<std::uint32_t>(right));
}

int Multiply32(int left, int right) {
  return static_cast<int>(static_cast<std::uint32_t>(left) *
                          static_cast<std::uint32_t>(right));
}

template <typename Type, int Tag>
void ClearListExact(idList<Type, Tag>& list) {
  if (list.listStatic == 0 || list.listStatic == 2) {
    if (list.list != nullptr) {
      idMem::Singleton().Free(list.list, ALIGN_16);
    }
    list.list = nullptr;
    list.size = 0;
  }
  list.num = 0;
}

bool ResizeWaterPointList(idList<waterPoint_t, 5>& list, int newSize) {
  if (newSize <= 0) {
    ClearListExact(list);
    return true;
  }
  if (newSize == list.size || list.listStatic != 0) {
    return true;
  }

  auto* const replacement = static_cast<waterPoint_t*>(
      idListArrayResize<waterPoint_t>(
          list.list, list.size, newSize,
          static_cast<memTag_t>(list.memTag), false));
  list.list = replacement;
  if (replacement == nullptr) {
    return false;
  }
  list.size = newSize;
  if (list.num > newSize) {
    list.num = newSize;
  }
  list.listStatic = 0;
  return true;
}

waterPoint_t* AllocWaterPoint(idList<waterPoint_t, 5>& list) {
  const int required = Add32(list.num, 1);
  if (required > list.size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (list.granularity != 0) {
      const int granularity = list.granularity;
      const int rounded = Add32(required, granularity - 1);
      newSize = rounded - rounded % granularity;
    } else {
      newSize = 1;
      while (newSize < required) {
        newSize = Multiply32(newSize, 2);
      }
    }
    ResizeWaterPointList(list, newSize);
  }

  if (list.num >= list.size) {
    return &list.list[list.size - 1];
  }
  return &list.list[list.num++];
}

bool ResizeCacheList(
    idList<idTraceModelCache::trmCache_t*, TAG_COLLISION>& list,
    int newSize) {
  if (newSize <= 0) {
    ClearListExact(list);
    return true;
  }
  if (newSize == list.size || list.listStatic != 0) {
    return true;
  }

  idMem& memory = idMem::Singleton();
  auto** replacement = static_cast<idTraceModelCache::trmCache_t**>(
      memory.AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : "
          "tag",
          static_cast<std::size_t>(static_cast<std::int64_t>(newSize) *
                                   sizeof(void*)),
          static_cast<memTag_t>(list.memTag), false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  if (replacement != nullptr) {
    const int copyCount = list.size < newSize ? list.size : newSize;
    for (int index = 0; index < copyCount; ++index) {
      replacement[index] = list.list[index];
    }
    memory.Free(list.list, ALIGN_16);
  }
  list.list = replacement;
  if (replacement == nullptr) {
    return false;
  }
  list.size = newSize;
  if (list.num > newSize) {
    list.num = newSize;
  }
  return true;
}

idTraceModelCache::trmCache_t** AllocCacheSlot(
    idList<idTraceModelCache::trmCache_t*, TAG_COLLISION>& list) {
  const int required = Add32(list.num, 1);
  if (required > list.size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (list.granularity != 0) {
      const int granularity = list.granularity;
      const int rounded = Add32(required, granularity - 1);
      newSize = rounded - rounded % granularity;
    } else {
      newSize = 1;
      while (newSize < required) {
        newSize = Multiply32(newSize, 2);
      }
    }
    ResizeCacheList(list, newSize);
  }

  if (list.num >= list.size) {
    return &list.list[list.size - 1];
  }
  return &list.list[list.num++];
}

void InitializeTraceModelTail(idTraceModel* traceModel) {
  auto* const bytes = reinterpret_cast<std::uint8_t*>(traceModel);
  *reinterpret_cast<std::uint64_t*>(bytes + 0x5C0) = 0;
  *reinterpret_cast<std::uint64_t*>(bytes + 0x5C8) = 0;
  *reinterpret_cast<std::uint64_t*>(bytes + 0x5D0) = 0;
  *reinterpret_cast<std::uint64_t*>(bytes + 0x5D8) = 0;
  traceModel->bounds[0].x = bounds_zero[0].x;
  traceModel->bounds[0].y = bounds_zero[0].y;
  traceModel->bounds[0].z = bounds_zero[0].z;
  traceModel->bounds[1].x = bounds_zero[1].x;
  traceModel->bounds[1].y = bounds_zero[1].y;
  traceModel->bounds[1].z = bounds_zero[1].z;
  *reinterpret_cast<std::uint64_t*>(bytes + 0x5F8) = 0;
}

} // namespace

template <>
__declspec(noinline) void* idListArrayResize<waterPoint_t>(
    void* oldPointer, int oldCount, int newCount, memTag_t tag,
    bool zeroBuffer) {
  if (newCount <= 0) {
    idMem::Singleton().Free(oldPointer, ALIGN_16);
    return nullptr;
  }

  idMem& memory = idMem::Singleton();
  auto* const replacement = static_cast<waterPoint_t*>(
      memory.AllocWithLocation(
          zeroBuffer
              ? "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(27) "
                ": tag"
              : "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) "
                ": tag",
          static_cast<std::size_t>(static_cast<std::int64_t>(newCount) *
                                   sizeof(waterPoint_t)),
          tag, zeroBuffer, ALIGN_16, HEAP_DEFAULTHEAP));
  if (replacement == nullptr) {
    return nullptr;
  }

  const int copyCount = oldCount < newCount ? oldCount : newCount;
  for (int index = 0; index < copyCount; ++index) {
    replacement[index] = static_cast<waterPoint_t*>(oldPointer)[index];
  }
  memory.Free(oldPointer, ALIGN_16);
  return replacement;
}

template <>
__declspec(noinline) void
idBlockAlloc<idTraceModelCache::trmCache_t, 64,
             TAG_COLLISION>::AllocNewBlock() {
  idBlock* const block = static_cast<idBlock*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\BlockAlloc.h(207) : "
          "_memTag_",
          sizeof(idBlock), TAG_COLLISION, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  block->next = blocks;
  blocks = block;
  for (int index = 0; index < 64; ++index) {
    block->elements[index].next = free;
    free = &block->elements[index];
  }
  total += 64;
}

template <>
__declspec(noinline) void
idBlockAlloc<idTraceModelCache::trmCache_t, 64,
             TAG_COLLISION>::Shutdown() {
  while (blocks != nullptr) {
    idBlock* const block = blocks;
    blocks = block->next;
    idMem::Singleton().Free(block, ALIGN_16);
  }
  blocks = nullptr;
  free = nullptr;
  total = 0;
  active = 0;
}

idTraceModelCache::trmCache_t::trmCache_t() : waterPoints() {
  trm = nullptr;
  refCount = 1;
  volume = 0.0f;
  centerOfMass.x = 0.0f;
  centerOfMass.y = 0.0f;
  centerOfMass.z = 0.0f;
  for (int row = 0; row < 3; ++row) {
    inertiaTensor[row].x = 0.0f;
    inertiaTensor[row].y = 0.0f;
    inertiaTensor[row].z = 0.0f;
  }
  material = nullptr;
  collisionModel = nullptr;
  hasWater = false;
}

void idTraceModelCache::FreeTraceModel(int traceModelIndex) {
  if (traceModelIndex < 0 || traceModelIndex >= cache.num ||
      cache.list[traceModelIndex]->refCount <= 0) {
    idLib::Warning(
        "idClipModel::FreeTraceModel: tried to free uncached trace model");
    return;
  }
  --cache.list[traceModelIndex]->refCount;
}

int idTraceModelCache::CopyTraceModel(int traceModelIndex) {
  if (traceModelIndex < 0 || traceModelIndex >= cache.num ||
      cache.list[traceModelIndex]->refCount <= 0) {
    idLib::Warning(
        "idClipModel::CopyTraceModel: tried to copy an uncached trace model");
    return -1;
  }
  ++cache.list[traceModelIndex]->refCount;
  return traceModelIndex;
}

void idTraceModelCache::GetMassProperties(
    int traceModelIndex, float density, float& mass, idVec3& center,
    idMat3& tensor) const {
  trmCache_t* const entry = cache.list[traceModelIndex];
  mass = density * entry->volume;
  center.x = entry->centerOfMass.x;
  center.y = entry->centerOfMass.y;
  center.z = entry->centerOfMass.z;

  const float m00 = density * entry->inertiaTensor[0].x;
  const float m10 = density * entry->inertiaTensor[1].x;
  const float m02 = density * entry->inertiaTensor[0].z;
  const float m01 = density * entry->inertiaTensor[0].y;
  const float m11 = density * entry->inertiaTensor[1].y;
  const float m12 = density * entry->inertiaTensor[1].z;
  const float m20 = density * entry->inertiaTensor[2].x;
  const float m21 = density * entry->inertiaTensor[2].y;
  const float m22 = density * entry->inertiaTensor[2].z;
  tensor[0].x = m00;
  tensor[1].x = m10;
  tensor[0].y = m01;
  tensor[0].z = m02;
  tensor[1].y = m11;
  tensor[1].z = m12;
  tensor[2].x = m20;
  tensor[2].y = m21;
  tensor[2].z = m22;
}

void idTraceModelCache::ClearTraceModelCache() {
  int index = 0;
  while (index < cache.num) {
    idCollisionModel* const collisionModel =
        cache.list[index]->collisionModel;
    if (collisionModel != nullptr) {
      delete collisionModel;
      cache.list[index]->collisionModel = nullptr;
    }

    trmCache_t* const entry = cache.list[index];
    ClearListExact(entry->waterPoints);
    if (cache.list[index]->trm != nullptr) {
      idTraceModel::operator delete(cache.list[index]->trm);
      cache.list[index]->trm = nullptr;
    }
    ++index;
  }

  cacheAllocator.Shutdown();
  ClearListExact(cache);
  hash.InternalFree();
}

void idTraceModelCache::SetupWaterPoints(trmCache_t* entry) {
  idStaticList<waterPoint_t, 256> points;
  idTraceModel* traceModel = entry->trm;
  if (!traceModel->isConvex) {
    entry->hasWater = false;
    return;
  }

  const float minX = traceModel->bounds[0].x;
  const float maxX = traceModel->bounds[1].x;
  if (minX >= maxX) {
    entry->hasWater = false;
    return;
  }
  const float minY = traceModel->bounds[0].y;
  const float maxY = traceModel->bounds[1].y;
  if (minY >= maxY) {
    entry->hasWater = false;
    return;
  }
  const float minZ = traceModel->bounds[0].z;
  const float maxZ = traceModel->bounds[1].z;
  if (minZ >= maxZ ||
      ((maxY - minY) * (maxX - minX)) * (maxZ - minZ) == 0.0f) {
    entry->hasWater = false;
    return;
  }

  const unsigned int numVertices = traceModel->numVerts;
  const int generatedTarget =
      Add32(64, -static_cast<int>(numVertices));
  float densityScale = 1.0f;
  float gridBase = static_cast<float>(generatedTarget);

  for (;;) {
    const float dimension = ::powf(gridBase * densityScale,
                                   0.33333334f);
    const float reciprocal = 1.0f / dimension;
    const float spacingZ =
        (entry->trm->bounds[1].z - entry->trm->bounds[0].z) * reciprocal;
    const float spacingY =
        (entry->trm->bounds[1].y - entry->trm->bounds[0].y) * reciprocal;
    const float spacingX =
        (entry->trm->bounds[1].x - entry->trm->bounds[0].x) * reciprocal;

    if (dimension > 0.0f) {
      int xIndex = 0;
      do {
        const float localX =
            (static_cast<float>(xIndex) + 0.5f) * spacingX;
        int yIndex = 0;
        do {
          const float localY =
              (static_cast<float>(yIndex) + 0.5f) * spacingY;
          int zIndex = 0;
          do {
            idVec3 point;
            point.x = localX + entry->trm->bounds[0].x;
            point.y = localY + entry->trm->bounds[0].y;
            point.z =
                (static_cast<float>(zIndex) + 0.5f) * spacingZ +
                entry->trm->bounds[0].z;
            if (entry->trm->ContainsPoint(point)) {
              waterPoint_t* const output = AllocWaterPoint(points);
              output->xyz = point;
              output->weight = 1.0f;
              output->squareWeight = 1.0f;
            }
            ++zIndex;
          } while (dimension > static_cast<float>(zIndex));
          ++yIndex;
        } while (dimension > static_cast<float>(yIndex));
        ++xIndex;
      } while (dimension > static_cast<float>(xIndex));
    }

    if (points.num >= generatedTarget) {
      break;
    }
    ClearListExact(static_cast<idList<waterPoint_t, 5>&>(points));
    densityScale *= 2.0f;
    if (densityScale > 16.0f) {
      entry->hasWater = false;
      return;
    }
  }

  const int totalPointCount =
      Add32(points.num, static_cast<int>(numVertices));
  const float pointWeight = 1.0f / static_cast<float>(totalPointCount);
  const float pointSquareWeight = pointWeight * pointWeight;
  for (int index = 0; index < points.num; ++index) {
    points.list[index].weight = pointWeight;
    points.list[index].squareWeight = pointSquareWeight;
  }

  while (points.num > generatedTarget) {
    int firstPoint = 0;
    int secondPoint = 0;
    float nearestDistance = 1.0e30f;
    for (int first = 0; first < points.num; ++first) {
      for (int second = first + 1; second < points.num; ++second) {
        const float deltaX =
            points.list[first].xyz.x - points.list[second].xyz.x;
        const float deltaY =
            points.list[first].xyz.y - points.list[second].xyz.y;
        const float deltaZ =
            points.list[first].xyz.z - points.list[second].xyz.z;
        const float distance =
            (((deltaX * deltaX) + (deltaY * deltaY)) +
             (deltaZ * deltaZ)) *
            points.list[first].squareWeight *
            points.list[second].squareWeight;
        if (nearestDistance > distance) {
          nearestDistance = distance;
          firstPoint = first;
          secondPoint = second;
        }
      }
    }

    points.list[firstPoint].xyz.x =
        (points.list[secondPoint].xyz.x +
         points.list[firstPoint].xyz.x) *
        0.5f;
    points.list[firstPoint].xyz.y =
        (points.list[secondPoint].xyz.y +
         points.list[firstPoint].xyz.y) *
        0.5f;
    points.list[firstPoint].xyz.z =
        (points.list[secondPoint].xyz.z +
         points.list[firstPoint].xyz.z) *
        0.5f;
    points.list[firstPoint].weight =
        points.list[secondPoint].weight + points.list[firstPoint].weight;
    points.list[firstPoint].squareWeight =
        points.list[firstPoint].weight * points.list[firstPoint].weight;

    if (secondPoint >= 0 && secondPoint < points.num) {
      --points.num;
      if (secondPoint != points.num) {
        points.list[secondPoint] = points.list[points.num];
      }
    }
  }

  for (unsigned int vertex = 0; vertex < numVertices; ++vertex) {
    waterPoint_t* const output = AllocWaterPoint(points);
    output->xyz.x = entry->trm->vertsX[vertex];
    output->xyz.y = entry->trm->vertsY[vertex];
    output->xyz.z = entry->trm->vertsZ[vertex];
    output->weight = pointWeight;
    output->squareWeight = pointSquareWeight;
  }

  if (entry->waterPoints.size < 64 &&
      entry->waterPoints.listStatic == 0) {
    auto* const replacement = static_cast<traceModelWater_t*>(
        idListArrayResize<traceModelWater_t>(
            entry->waterPoints.list, entry->waterPoints.size, 64,
            static_cast<memTag_t>(entry->waterPoints.memTag), false));
    entry->waterPoints.list = replacement;
    if (replacement != nullptr) {
      entry->waterPoints.size = 64;
      if (entry->waterPoints.num > 64) {
        entry->waterPoints.num = 64;
      }
    }
  }
  if (entry->waterPoints.size >= 64 ||
      entry->waterPoints.listStatic != 0) {
    entry->waterPoints.num =
        entry->waterPoints.size > 64 ? 64 : entry->waterPoints.size;
  }

  for (int index = 0; index < points.num; ++index) {
    entry->waterPoints.list[index].xyz.x = points.list[index].xyz.x;
    entry->waterPoints.list[index].xyz.y = points.list[index].xyz.y;
    entry->waterPoints.list[index].xyz.z = points.list[index].xyz.z;
    entry->waterPoints.list[index].weight = points.list[index].weight;
  }
  entry->hasWater = true;
}

int idTraceModelCache::AllocTraceModel(
    const idTraceModel& sourceTraceModel, const idMaterial* sourceMaterial) {
  const std::uint32_t minX =
      *reinterpret_cast<const std::uint32_t*>(&sourceTraceModel.bounds[0].x);
  const std::uint32_t minY =
      *reinterpret_cast<const std::uint32_t*>(&sourceTraceModel.bounds[0].y);
  const std::uint32_t minZ =
      *reinterpret_cast<const std::uint32_t*>(&sourceTraceModel.bounds[0].z);
  const int hashKey = static_cast<int>(
      minX ^ minY ^ minZ ^ sourceTraceModel.numPolys ^
      (static_cast<std::uint32_t>(sourceTraceModel.type) << 8) ^
      ((sourceTraceModel.numEdges ^ (sourceTraceModel.numVerts << 2)) <<
       2));

  std::int64_t lookup =
      static_cast<std::int64_t>(hash.lookupMask) &
      static_cast<std::int64_t>(hashKey) &
      static_cast<std::int64_t>(hash.hashMask);
  int traceModelIndex = hash.hash[lookup];
  while (traceModelIndex != -1) {
    trmCache_t* const entry = cache.list[traceModelIndex];
    if (entry->trm->Compare(sourceTraceModel) &&
        cache.list[traceModelIndex]->material == sourceMaterial) {
      ++cache.list[traceModelIndex]->refCount;
      return traceModelIndex;
    }
    lookup = static_cast<std::int64_t>(traceModelIndex) &
             static_cast<std::int64_t>(hash.lookupMask);
    traceModelIndex = hash.indexChain[lookup];
  }

  const int newIndex = cache.num;
  if (hash.hash == idHashIndex::INTERNAL_INVALID_INDEX) {
    int indexSize = hash.indexSize;
    if (newIndex >= indexSize) {
      indexSize = Add32(newIndex, 1);
    }
    hash.InternalAllocate(hash.hashSize, indexSize);
  } else if (newIndex >= hash.indexSize) {
    hash.InternalResizeIndex(Add32(newIndex, 1));
  }

  const int bucket = hashKey & hash.hashMask;
  hash.indexChain[newIndex] = hash.hash[bucket];
  hash.hash[bucket] = newIndex;

  trmCache_t* entry;
  if (cacheAllocator.free == nullptr) {
    if (!cacheAllocator.allowAllocs) {
      entry = nullptr;
      goto allocator_done;
    }
    cacheAllocator.AllocNewBlock();
  }

  {
    ++cacheAllocator.active;
    auto* const element = cacheAllocator.free;
    cacheAllocator.free = element->next;
    element->next = nullptr;
    if (cacheAllocator.clearAllocs) {
      std::memset(element->buffer, 0, sizeof(element->buffer));
    }
    entry = ::new (static_cast<void*>(element->buffer)) trmCache_t;
  }

allocator_done:
  *AllocCacheSlot(cache) = entry;

  auto* traceModel = static_cast<idTraceModel*>(
      idTraceModel::operator new(sizeof(idTraceModel)));
  if (traceModel != nullptr) {
    InitializeTraceModelTail(traceModel);
  }
  entry->trm = traceModel;
  *traceModel = sourceTraceModel;
  traceModel->ClearUnused();
  traceModel->GetMassProperties(1.0f, entry->volume,
                                entry->centerOfMass,
                                entry->inertiaTensor);
  entry->refCount = 1;
  entry->material = sourceMaterial;

  va entityName("traceModel%d", newIndex);
  {
    idStr modelName =
        idMapFile::GetMapModelName(mapName.c_str(), entityName.buffer);
    entry->collisionModel = collisionModelManager->ModelFromTrm(
        modelName.c_str(), *entry->trm, sourceMaterial);
  }
  SetupWaterPoints(entry);
  return newIndex;
}
