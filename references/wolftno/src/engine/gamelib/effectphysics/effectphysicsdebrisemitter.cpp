#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/effectphysics/effectphysicsdebrisemitter.h"

#include "engine/decls/declbreakable.h"
#include "engine/gamelib/physics/clip.h"
#pragma warning(push)
#pragma warning(disable : 4324)
#include "engine/models/discreteanimation/rendermodeldiscreteanimation.h"
#pragma warning(pop)
#include "shared/idlib/lib.h"
#include "shared/idlib/networking/bitmsg.h"
#include "shared/idlib/sys/sys_alloc.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <cstdint>
#include <new>

void operator delete[](void* memory, memTag_t tag) noexcept;

enum class debrisRenderTag_t : int {
  RenderModel = TAG_RENDERMODEL,
};

void operator delete(void* memory, debrisRenderTag_t tag) noexcept;

#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:??3@YAXPEAXW4debrisRenderTag_t@@@Z="                    \
    "??_V@YAXPEAXW4memTag_t@@@Z")

__forceinline void* operator new(std::size_t size,
                                 debrisRenderTag_t tag) noexcept {
  return idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size,
      static_cast<memTag_t>(tag), false, ALIGN_16, HEAP_DEFAULTHEAP);
}

namespace {

constexpr float kRandomToUnit = 0.000030518509f;

std::uint32_t NextRandom(std::uint32_t value) {
  return 1664525u * value + 1013904223u;
}

float Random15(std::uint32_t value) {
  return static_cast<float>((value >> 10) & 0x7fffu);
}

idTraceModelCache* GetTraceModelCache(idClip* clip) {
  return *reinterpret_cast<idTraceModelCache**>(
      reinterpret_cast<std::uint8_t*>(clip) + 0x400F8);
}

void AddRenderModel(idRenderWorld* world, idRenderModel* model) {
  using Function = bool (*)(idRenderWorld*, idRenderModel*);
  void** const vtable = *reinterpret_cast<void***>(world);
  reinterpret_cast<Function>(vtable[0x80 / sizeof(void*)])(world, model);
}

}  // namespace

void idEffectPhysicsDebrisEmitter::Init(
    const idDeclBreakable* breakableDebris, int entityNumber,
    idClip* clip, idRenderWorld* world,
    idRenderModelEffects* renderModelEffects) {
  if (breakableDebris == nullptr) {
    return;
  }

  idRenderModelDiscreteAnimation* const model =
      new (debrisRenderTag_t::RenderModel)
          idRenderModelDiscreteAnimation(breakableDebris->modelName.str,
                                         breakableDebris);

  AddRenderModel(world, model);
  if (model == nullptr) {
    idLib::Warning(
        " idEffectPhysicsDebrisEmitter::Init(): Model not found for breakable %s.",
        breakableDebris->name.str);
    return;
  }

  if (model->renderModelType != RENDERMODEL_DISCRETEANIMATION) {
    idLib::Warning(
        " idEffectPhysicsDebrisEmitter::Init(): %s is not a breakable model.",
        model->name.str);
    return;
  }

  piecePhysics.Init(clip, GetTraceModelCache(clip), model,
                    renderModelEffects, nullptr, entityNumber);
  piecePhysics.pieceLifeTime = 2500;
  piecePhysics.pieceFriction = 15.0f;
  piecePhysics.UpdateModel({1.0f, 1.0f, 1.0f});

  if ((model->useDeferredPosition &
       (model->deferredPositionInitialized >> 1u) & 1u) == 0u) {
    model->g.origin = vec3_origin;
  }
  model->deferredOrigin = vec3_origin;
  if ((model->useDeferredPosition &
       (model->deferredPositionInitialized >> 1u) & 1u) == 0u) {
    model->g.axis = mat3_identity;
  }
  model->deferredAxis = mat3_identity;
  *reinterpret_cast<std::uint64_t*>(&model->g.fovScale) = 0x3f800000ull;

  *reinterpret_cast<std::uint64_t*>(&emitBufferPos) = 0;
  *reinterpret_cast<std::uint16_t*>(&skipFirstFrame) = 0x0101u;
  random.seed = 0;
}

void idEffectPhysicsDebrisEmitter::EmitDebris(
    idVec3 pos, idVec3 velocity, idVec3 normal, int count,
    int currentTime) {
  if (!active) {
    return;
  }

  int emitCount = count;
  if (count >= 16) {
    emitCount = 15;
  }
  emitBuffer[emitBufferPos].pos = pos;
  emitBuffer[emitBufferPos].normal = normal;
  emitBuffer[emitBufferPos].count = emitCount;
  emitBufferPos = (emitBufferPos + 1) % EMIT_DEBRIS_BUFFER_SIZE;

  if (emitCount <= 0) {
    return;
  }

  for (unsigned int remaining = static_cast<unsigned int>(emitCount);
       remaining != 0; --remaining) {
    idAngles angles;
    angles.yaw = 0.0f;
    angles.roll = 0.0f;
    const std::uint32_t random1 = NextRandom(random.seed);
    random.seed = random1;
    angles.pitch = ((Random15(random1) * kRandomToUnit) * 45.0f) +
                   45.0f;
    const idVec3 direction = angles.ToForward();

    const std::uint32_t random2 = NextRandom(random.seed);
    const std::uint32_t random3 = NextRandom(random2);
    const std::uint32_t random4 = NextRandom(random3);
    const std::uint32_t random5 = NextRandom(random4);
    random.seed = random5;

    const float randomZ =
        ((Random15(random2) * kRandomToUnit) * 8.0f) - 4.0f;
    const float randomY =
        ((Random15(random3) * kRandomToUnit) * 8.0f) - 4.0f;
    const float randomX =
        ((Random15(random4) * kRandomToUnit) * 8.0f) - 4.0f;
    const float projection =
        ((direction.y * randomY) + (direction.x * randomX)) +
        (direction.z * randomZ);
    const float offsetZ = randomZ - (direction.z * projection);
    const float offsetY = randomY - (direction.y * projection);
    const float offsetX = randomX - (direction.x * projection);

    const float speed =
        (Random15(random5) * kRandomToUnit) * 125.0f;
    idVec3 launchVelocity;
    launchVelocity.x = (direction.x * speed) + 0.0f;
    launchVelocity.y = (direction.y * speed) + 0.0f;
    launchVelocity.z = (direction.z * speed) + 200.0f;

    idVec3 impulse;
    impulse.x = ((launchVelocity.x * 10.0f) + velocity.x) +
                (normal.x * 1000.0f);
    impulse.y = ((launchVelocity.y * 10.0f) + velocity.y) +
                (normal.y * 1000.0f);
    impulse.z = ((launchVelocity.z * 10.0f) + velocity.z) +
                (normal.z * 1000.0f);

    const idVec3 emitPosition{offsetX + pos.x, offsetY + pos.y,
                              offsetZ + pos.z};
    piecePhysics.EmitPiece(currentTime, emitPosition, mat3_identity,
                           pos, impulse);
  }
}

void idEffectPhysicsDebrisEmitter::Update(
    const idVec3& origin, const idMat3& axis,
    const idVec3& drawScale3D, int currentTime,
    int gameMsPerFrame) {
  while (serializeEmitCount > 0) {
    --serializeEmitCount;
    const int bufferPosition = emitBufferPos;
    const idVec3 normal = emitBuffer[bufferPosition].normal;
    const idVec3 velocity = vec3_origin;
    const idVec3 pos = emitBuffer[bufferPosition].pos;
    EmitDebris(pos, velocity, normal, emitBuffer[bufferPosition].count,
               currentTime);
  }
  --serializeEmitCount;

  if (piecePhysics.numPiecesInUse != 0) {
    piecePhysics.UpdateSimulation(origin, axis, currentTime,
                                  gameMsPerFrame);
    piecePhysics.UpdateModel(drawScale3D);
    piecePhysics.SubmitCollisionQueries(false);
  }
}

template <>
void idBitMsg::WriteQuantizedVector<idVec3, 32768, 12>(
    const idVec3& vector) {
  for (int index = 0; index < 3; ++index) {
    int value = static_cast<int>(vector[index] * 0.062469482f);
    if (value < -2047) {
      value = -2047;
    } else if (value > 2047) {
      value = 2047;
    }
    WriteBits(value, -12);
  }
}
