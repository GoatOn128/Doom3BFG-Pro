#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_EMPTY_CTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_NOINLINE_ALLOC_ABI
#define WOLFTNO_RETAIL_IDLIST_NOINLINE_RESIZE_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_ALLOC_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_RESIZE_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_STATICLIST_NOINLINE_CTOR_ABI
#include "engine/gamelib/effects/lensflaremanager.h"

#pragma warning(push)
#pragma warning(disable : 4201 4324)
#include "engine/decls/declflare.h"
#include "engine/decls/decltable.h"
#include "engine/gamelib/pvs/pvs.h"
#include "engine/models/flares/rendermodelflare.h"
#include "engine/models/renderworld.h"
#include "engine/renderer/autorenderparms.h"
#pragma warning(pop)
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/text/str.h"

#undef WOLFTNO_RETAIL_STATICLIST_NOINLINE_CTOR_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_ALLOC_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_RESIZE_ABI
#undef WOLFTNO_RETAIL_IDLIST_NOINLINE_RESIZE_ABI
#undef WOLFTNO_RETAIL_IDLIST_NOINLINE_ALLOC_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#undef WOLFTNO_RETAIL_IDLIST_EMPTY_CTOR_ABI
#undef WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <new>

class idScenePoint {
public:
  virtual void ActivateTargets(idEntity* target);
};

class idGame {
public:
  const void* const* vtable;

  template <typename Function> Function Slot(int index) const {
    return reinterpret_cast<Function>(vtable[index]);
  }
};

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

extern bool forceIdListsToReallocateEveryAppend;
idGame* GetGame();
idRenderDebug* GetGameLibRenderDebug();

void operator delete[](void* memory, memTag_t tag) noexcept;

enum class lensFlareRenderTag_t : int {
  RenderModel = TAG_RENDERMODEL,
};

void operator delete(void* memory, lensFlareRenderTag_t tag) noexcept;

#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:??3@YAXPEAXW4lensFlareRenderTag_t@@@Z="                 \
    "??_V@YAXPEAXW4memTag_t@@@Z")

__forceinline void* operator new(std::size_t size,
                                 lensFlareRenderTag_t tag) noexcept {
  return idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size,
      static_cast<memTag_t>(tag), false, ALIGN_16, HEAP_DEFAULTHEAP);
}

idCVar g_showFlareDebug(
    "g_showFlareDebug", "0", CVAR_INTEGER,
    "1 = render flare debug, 2 = render flare debug depth-tested",
    static_cast<argCompletionRef_t>(nullptr));

idCVar r_flareDistanceCull(
    "r_flareDistanceCull", "1", CVAR_BOOL,
    "If flares should be culled by distance to player0",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

using LensFlare = idGlobalLensFlareManager::lensFlare_t;
using LensFlareList = idList<LensFlare, 5>;

#define LENS_FLARE_IS_SUN(flare)                                              \
  (((std::bit_cast<std::uint32_t>((flare).sunDirection.x) |                  \
     std::bit_cast<std::uint32_t>((flare).sunDirection.y) |                  \
     std::bit_cast<std::uint32_t>((flare).sunDirection.z)) &                 \
    0x7fffffffu) != 0)

#define LENS_FLARE_LOOKUP(handle, failureValue)                               \
  int index;                                                                  \
  do {                                                                        \
    if ((handle).value.allocId == allocId) {                                  \
      index = (handle).value.flareIndex;                                      \
      break;                                                                  \
    }                                                                         \
    const int lookupCount = lensFlares.num;                                   \
    index = 0;                                                                \
    if (lookupCount <= 0) {                                                   \
      return failureValue;                                                    \
    }                                                                         \
    lensFlare_t* lookupFlare = lensFlares.list;                               \
    while (lookupFlare->handle.value.handleId !=                              \
           (handle).value.handleId) {                                         \
      ++index;                                                                \
      ++lookupFlare;                                                          \
      if (index >= lookupCount) {                                             \
        return failureValue;                                                  \
      }                                                                       \
    }                                                                         \
    (handle).value.flareIndex = index;                                        \
    (handle).value.allocId = allocId;                                         \
    lookupFlare->handle.value.flareIndex = index;                             \
    lookupFlare->handle.value.allocId = allocId;                              \
  } while (false)

__forceinline bool UsesDirectPosition(const idRenderModel* model) {
  return (model->useDeferredPosition &
          (model->deferredPositionInitialized >> 1) & 1u) == 0;
}

__forceinline void SetModelOrigin(idRenderModel* model,
                                  const idVec3& origin) {
  if (UsesDirectPosition(model)) {
    model->g.origin = origin;
  }
  model->deferredOrigin = origin;
}

__forceinline void SetModelAxis(idRenderModel* model, const idMat3& axis) {
  if (UsesDirectPosition(model)) {
    model->g.axis = axis;
  }
  model->deferredAxis = axis;
}

__forceinline void UnlinkRenderModel(idRenderModel* model) {
  if ((model->unlinked & 8u) == 0) {
    model->CommitThisFrame();
    using Function = void (*)(idRenderModel*);
    (*reinterpret_cast<Function**>(model))[7](model);
    model->unlinked |= 8u;
  }
}

__forceinline void UnlinkAndDeleteRenderModel(idRenderModel* model) {
  model->CommitThisFrame();
  UnlinkRenderModel(model);
  using Function = void (*)(idRenderModel*);
  (*reinterpret_cast<Function**>(model))[8](model);
  model->deleteOnSync |= 4u;
}

} // namespace

void idGlobalLensFlareManager::SetupRenderModels(idFlareHandle& handle,
                                                 float quadSize) {
  LENS_FLARE_LOOKUP(handle, );
  if (index == -1) {
    return;
  }

  lensFlare_t& flare = lensFlares.list[index];
  const bool isSun = LENS_FLARE_IS_SUN(flare);
  flare.renderModelFlare->isSunFlare = isSun;
  flare.renderModelFlare->occlusionQuadModel =
      flare.renderModelFlareOcclusionQuad;

  idRenderModelFlareOcclusionQuad* const quad =
      flare.renderModelFlareOcclusionQuad;
  const float size = std::fmax(8.0f, quadSize);
  quad->g.scale.x = size;
  quad->g.scale.y = size;
  quad->g.scale.z = size;
  std::uint8_t* const flags =
      reinterpret_cast<std::uint8_t*>(&quad->g);
  flags[117] ^= (flags[117] ^
                 static_cast<std::uint8_t>(flare.isAutosprited << 6)) &
                0x40u;
  flags[118] &= static_cast<std::uint8_t>(~2u);
  flags[118] |= static_cast<std::uint8_t>(isSun ? 2u : 0u);
  quad->isSunFlare = isSun;
  if (isSun) {
    flags[117] |= 0x20u;
    quad->g.modelDepthSort = MODEL_DEPTH_SORT_LAST;
    flags[118] |= 0x40u;
  }
}

void idGlobalLensFlareManager::StartFlare(
    idFlareHandle& handle, const idVec3& origin, const idMat3& axis,
    const idVec4& color, float fadeStart, float fadeEnd) {
  LENS_FLARE_LOOKUP(handle, );
  if (index == -1) {
    return;
  }

  lensFlare_t& flare = lensFlares.list[index];
  if (LENS_FLARE_IS_SUN(flare)) {
    SetModelOrigin(flare.renderModelFlare, flare.sunDirection);
    SetModelAxis(flare.renderModelFlare, axis);
    SetModelOrigin(flare.renderModelFlareOcclusionQuad, flare.sunDirection);
  } else {
    flare.renderModelFlare->g.fadeVisibilityOver = fadeStart;
    flare.renderModelFlare->g.maxVisibleRange = fadeEnd;
    SetModelOrigin(flare.renderModelFlare, origin);
    SetModelAxis(flare.renderModelFlare, axis);
    flare.renderModelFlareOcclusionQuad->g.maxVisibleRange = fadeEnd;
    idVec3 quadOrigin;
    quadOrigin.x = origin.x + flare.occlusionOffset.x;
    quadOrigin.y = flare.occlusionOffset.y + origin.y;
    quadOrigin.z = flare.occlusionOffset.z + origin.z;
    SetModelOrigin(flare.renderModelFlareOcclusionQuad, quadOrigin);
  }
  SetModelAxis(flare.renderModelFlareOcclusionQuad, axis);
  flare.renderModelFlare->SetParm(rp_Color.ConstPtr(), color);
  flare.renderModelFlare->SetParm(rp_coverage.ConstPtr(), 1.0f);
  flare.enabled = true;
}

void idGlobalLensFlareManager::StopFlare(idFlareHandle& handle) {
  LENS_FLARE_LOOKUP(handle, );
  if (index == -1) {
    return;
  }

  lensFlare_t& flare = lensFlares.list[index];
  flare.renderModelFlare->SetParm(rp_coverage.ConstPtr(), 0.0f);
  UnlinkRenderModel(flare.renderModelFlare);
  UnlinkRenderModel(flare.renderModelFlareOcclusionQuad);
  *reinterpret_cast<std::uint16_t*>(&flare.inPlayerPVS) = 0;
}

void idGlobalLensFlareManager::Update(
    idFlareHandle& handle, float timeSeconds, const idVec3& origin,
    const idMat3& axis, bool updateOrigin, bool updateAxis) {
  LENS_FLARE_LOOKUP(handle, );
  if (index == -1) {
    return;
  }

  lensFlare_t& flare = lensFlares.list[index];
  if (!LENS_FLARE_IS_SUN(flare)) {
    if (updateOrigin) {
      SetModelOrigin(flare.renderModelFlare, origin);
      idVec3 quadOrigin;
      quadOrigin.x = origin.x + flare.occlusionOffset.x;
      quadOrigin.y = flare.occlusionOffset.y + origin.y;
      quadOrigin.z = flare.occlusionOffset.z + origin.z;
      SetModelOrigin(flare.renderModelFlareOcclusionQuad, quadOrigin);
    }
    if (updateAxis) {
      SetModelAxis(flare.renderModelFlare, axis);
      SetModelAxis(flare.renderModelFlareOcclusionQuad, axis);
    }
  }

  if (flare.colorModulateTbl != nullptr) {
    const float scale = flare.colorModulateTbl->LookupTable().TableLookup(
        timeSeconds * flare.modulateTimeScale, true);
    flare.renderModelFlare->SetParm(rp_lightScale.ConstPtr(), scale);
  }
}

void idGlobalLensFlareManager::UpdatePosition(
    idFlareHandle& handle, const idVec3& origin, const idMat3& axis,
    bool updateOrigin, bool updateAxis) {
  LENS_FLARE_LOOKUP(handle, );
  if (index == -1) {
    return;
  }

  lensFlare_t& flare = lensFlares.list[index];
  if (LENS_FLARE_IS_SUN(flare)) {
    return;
  }
  if (updateOrigin) {
    SetModelOrigin(flare.renderModelFlare, origin);
    idVec3 quadOrigin;
    quadOrigin.x = origin.x + flare.occlusionOffset.x;
    quadOrigin.y = flare.occlusionOffset.y + origin.y;
    quadOrigin.z = flare.occlusionOffset.z + origin.z;
    SetModelOrigin(flare.renderModelFlareOcclusionQuad, quadOrigin);
  }
  if (updateAxis) {
    SetModelAxis(flare.renderModelFlare, axis);
    SetModelAxis(flare.renderModelFlareOcclusionQuad, axis);
  }
}

idRenderModelFlareOcclusionQuad*
idGlobalLensFlareManager::GetFlareOcclusionModel(idFlareHandle& handle) {
  LENS_FLARE_LOOKUP(handle, nullptr);
  return index == -1 ? nullptr
                     : lensFlares.list[index].renderModelFlareOcclusionQuad;
}

void idGlobalLensFlareManager::SetEntity(idFlareHandle& handle,
                                         idEntity* entity) {
  LENS_FLARE_LOOKUP(handle, );
  if (index == -1) {
    return;
  }

  idGame* const game = GetGame();
  idSpawnId spawnId;
  using GetSpawnIdFunction = idSpawnId* (*)(idGame*, idSpawnId*, idEntity*);
  game->Slot<GetSpawnIdFunction>(137)(game, &spawnId, entity);
  lensFlares.list[index].entitySpawnId.value = spawnId.value;
}

void idGlobalLensFlareManager::SetColorModulateTable(
    idFlareHandle& handle, const idDeclTable* colorTable,
    float modulateTimeScale) {
  LENS_FLARE_LOOKUP(handle, );
  if (index == -1) {
    return;
  }
  lensFlare_t& flare = lensFlares.list[index];
  flare.modulateTimeScale = modulateTimeScale;
  flare.colorModulateTbl = colorTable;
}

void idGlobalLensFlareManager::SetFlareColor(idFlareHandle& handle,
                                             const idVec4& color) {
  LENS_FLARE_LOOKUP(handle, );
  if (index != -1) {
    lensFlares.list[index].renderModelFlare->SetParm(rp_Color.ConstPtr(),
                                                      color);
  }
}

void idGlobalLensFlareManager::SetFlareFade(idFlareHandle& handle,
                                            float fade) {
  LENS_FLARE_LOOKUP(handle, );
  if (index != -1) {
    lensFlares.list[index].renderModelFlare->SetParm(rp_coverage.ConstPtr(),
                                                      fade);
  }
}

#pragma warning(push)
#pragma warning(disable : 4700 4701)
void idGlobalLensFlareManager::GameUpdate(idGame* game, const idPVS& pvs,
                                          int gameMS) {
  if (game == nullptr) {
    return;
  }

  const bool distanceCull = r_flareDistanceCull.valueInteger != 0;
  const float timeSeconds = static_cast<float>(gameMS) * 0.001f;
  idVec3 playerPosition;
  if (distanceCull) {
    using GetEntityFunction = idEntity* (*)(idGame*, int);
    idEntity* const player = game->Slot<GetEntityFunction>(138)(game, 0);
    if (player != nullptr) {
      using EntityGetOriginFunction = void (*)(idGame*, idEntity*, idVec3*);
      game->Slot<EntityGetOriginFunction>(145)(game, player, &playerPosition);
    }
  }

  for (int index = 0; index < lensFlares.num; ++index) {
    lensFlare_t* const list = lensFlares.list;
    lensFlare_t& flare = list[index];
    if (!flare.enabled) {
      continue;
    }

    idVec3 origin;
    idMat3 axis;
    if (flare.entitySpawnId.value == 0x1fff) {
      origin = (flare.renderModelFlare->useDeferredPosition & 1u) != 0
                   ? flare.renderModelFlare->deferredOrigin
                   : flare.renderModelFlare->g.origin;
    } else {
      using GetEntityForSpawnIdFunction = idEntity* (*)(idGame*, int);
      idEntity* const entity = game->Slot<GetEntityForSpawnIdFunction>(136)(
          game, flare.entitySpawnId.value);
      if (entity == nullptr) {
        continue;
      }
      using EntityGetOriginAndAxisFunction =
          void (*)(idGame*, idEntity*, idVec3*, idMat3*);
      game->Slot<EntityGetOriginAndAxisFunction>(144)(game, entity, &origin,
                                                       &axis);
    }

    int pvsArea = pvs.GetPVSArea(origin);
    using IsInPlayerPVSFunction = bool (*)(idGame*, int*, int);
    bool visible =
        game->Slot<IsInPlayerPVSFunction>(16)(game, &pvsArea, 1);
    if (visible && distanceCull && flare.cullDistanceSqr > 0.0f) {
      const float deltaY = origin.y - playerPosition.y;
      const float deltaX = origin.x - playerPosition.x;
      const float deltaZ = origin.z - playerPosition.z;
      const float distanceSqr =
          (deltaY * deltaY + deltaX * deltaX) + deltaZ * deltaZ;
      visible = flare.cullDistanceSqr > distanceSqr;
    }

    if (flare.inPlayerPVS != visible) {
      if (!visible) {
        UnlinkRenderModel(flare.renderModelFlare);
        UnlinkRenderModel(flare.renderModelFlareOcclusionQuad);
      }
      flare.inPlayerPVS = visible;
    }
    if (!visible) {
      continue;
    }

    if (flare.entitySpawnId.value != 0x1fff) {
      Update(flare.handle, timeSeconds, origin, axis, true, true);
    }
    flare.renderModelFlare->CommitThisFrame();
    flare.renderModelFlareOcclusionQuad->CommitThisFrame();

    if (g_showFlareDebug.valueInteger != 0) {
      const char* entityName = "<NULL>";
      if (GetGame() != nullptr && flare.entitySpawnId.value != 0x1fff) {
        idGame* const lookupGame = GetGame();
        using GetEntityForSpawnIdFunction = idEntity* (*)(idGame*, int);
        idEntity* const entity =
            lookupGame->Slot<GetEntityForSpawnIdFunction>(136)(
                lookupGame, flare.entitySpawnId.value);
        if (entity != nullptr) {
          idGame* const nameGame = GetGame();
          using GetEntityNameFunction = const char* (*)(idGame*, idEntity*);
          entityName = nameGame->Slot<GetEntityNameFunction>(139)(nameGame,
                                                                   entity);
        }
      }

      idRenderModelFlareOcclusionQuad* const quad =
          flare.renderModelFlareOcclusionQuad;
      const bool depthTest = g_showFlareDebug.valueInteger == 2;
      const idVec3& quadPosition =
          (quad->useDeferredPosition & 1u) != 0 ? quad->deferredOrigin
                                                : quad->g.origin;
      idBox box;
      box.center.x = quadPosition.x + flare.occlusionOffset.x;
      box.center.y = flare.occlusionOffset.y + quadPosition.y;
      box.center.z = flare.occlusionOffset.z + quadPosition.z;
      box.extents = quad->g.scale;
      box.axis = mat3_identity;
      GetGameLibRenderDebug()->DebugBox(idColor::colorYellow, box, 0,
                                        depthTest);

      const idVec3& axisOrigin =
          (quad->useDeferredPosition & 1u) != 0 ? quad->deferredOrigin
                                                : quad->g.origin;
      const idMat3& quadAxis =
          (quad->useDeferredPosition & 1u) != 0 ? quad->deferredAxis
                                                : quad->g.axis;
      GetGameLibRenderDebug()->DebugAxis(axisOrigin, quadAxis, 0, depthTest);

      idRenderDebug* const renderDebug = GetGameLibRenderDebug();
      const idVec3& textOrigin =
          (quad->useDeferredPosition & 1u) != 0 ? quad->deferredOrigin
                                                : quad->g.origin;
      const int quadIndex = quad->GetIndex();
      const int flareIndex = flare.renderModelFlare->GetIndex();
      va label("flare( %d ) quad( %d ) entity( '%s' )", flareIndex,
               quadIndex, entityName);
      renderDebug->DebugText(label, textOrigin, 0.2f, idColor::colorYellow, 1,
                             0, depthTest, false);
    }
  }
}
#pragma warning(pop)

void idGlobalLensFlareManager::MapShutdown() {
  for (int index = 0; index < lensFlares.num; ++index) {
    lensFlare_t& flare = lensFlares.list[index];
    UnlinkAndDeleteRenderModel(flare.renderModelFlare);
    UnlinkAndDeleteRenderModel(flare.renderModelFlareOcclusionQuad);
  }
  if (lensFlares.listStatic == 0 || lensFlares.listStatic == 2) {
    if (lensFlares.list != nullptr) {
      idMem::Singleton().Free(lensFlares.list, ALIGN_16);
    }
    lensFlares.list = nullptr;
    lensFlares.size = 0;
  }
  lensFlares.num = 0;
}

template <>
bool idList<idGlobalLensFlareManager::lensFlare_t, 5>::RemoveIndexFast(
    int index) {
  if (index < 0 || index >= num) {
    return false;
  }
  const int oldNum = num;
  num = oldNum - 1;
  if (index != oldNum - 1) {
    const idGlobalLensFlareManager::lensFlare_t& source = list[oldNum - 1];
    idGlobalLensFlareManager::lensFlare_t& destination = list[index];
    destination.declFlare = source.declFlare;
    destination.renderModelFlare = source.renderModelFlare;
    destination.renderModelFlareOcclusionQuad =
        source.renderModelFlareOcclusionQuad;
    destination.colorModulateTbl = source.colorModulateTbl;
    destination.sunDirection = source.sunDirection;
    destination.occlusionOffset = source.occlusionOffset;
    destination.modulateTimeScale = source.modulateTimeScale;
    destination.cullDistanceSqr = source.cullDistanceSqr;
    destination.handle.value.handleId = source.handle.value.handleId;
    destination.handle.value.allocId = source.handle.value.allocId;
    destination.handle.value.flareIndex = source.handle.value.flareIndex;
    destination.entitySpawnId.value = source.entitySpawnId.value;
    destination.isAutosprited = source.isAutosprited;
    destination.inPlayerPVS = source.inPlayerPVS;
    destination.enabled = source.enabled;
  }
  return true;
}

idGlobalLensFlareManager::~idGlobalLensFlareManager() noexcept(false) {
  MapShutdown();
}

void idGlobalLensFlareManager::RemoveFlare(idFlareHandle handle) {
  LENS_FLARE_LOOKUP(handle, );
  if (index == -1) {
    return;
  }
  lensFlare_t& flare = lensFlares.list[index];
  UnlinkAndDeleteRenderModel(flare.renderModelFlare);
  UnlinkAndDeleteRenderModel(flare.renderModelFlareOcclusionQuad);
  lensFlares.RemoveIndexFast(index);
  ++allocId;
}

idGlobalLensFlareManager::idGlobalLensFlareManager()
    : lensFlares(), allocId(1) {}

template <>
__declspec(noinline) bool
idList<idGlobalLensFlareManager::lensFlare_t, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    if (listStatic == 0 || listStatic == 2) {
      if (list != nullptr) {
        idMem::Singleton().Free(list, ALIGN_16);
      }
      list = nullptr;
      size = 0;
    }
    num = 0;
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  if (size > 0 || newSize == granularity) {
    reinterpret_cast<idScenePoint*>(this)->idScenePoint::ActivateTargets(
        nullptr);
  }
  list = static_cast<idGlobalLensFlareManager::lensFlare_t*>(
      idListArrayResize<idGlobalLensFlareManager::lensFlare_t>(
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

template <>
__declspec(noinline) idGlobalLensFlareManager::lensFlare_t&
idList<idGlobalLensFlareManager::lensFlare_t, 5>::Alloc() {
  const int required = num + 1;
  if (required > size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (granularity != 0) {
      const int rounded = required + granularity - 1;
      newSize = rounded - rounded % granularity;
    } else {
      for (newSize = 1; newSize < required; newSize *= 2) {
      }
    }
    Resize(newSize);
  }

  const int index = num;
  if (index >= size) {
    return list[size - 1];
  }
  ++num;
  return list[index];
}

idFlareHandle idGlobalLensFlareManager::AddFlare(
    idRenderWorld* renderWorld, const idDeclFlare* declFlare,
    const idVec3& sunDirection, const idVec3& occlusionOffset,
    bool isAutosprited) {
  const int index = lensFlares.num;
  if (index == lensFlares.size) {
    return idFlareHandle{};
  }

  const float cullDistance = declFlare->cullDistance;
  const int handleId = allocId;
  const int nextAllocId = handleId + 1;
  allocId = nextAllocId;
  lensFlare_t& flare = lensFlares.Alloc();
  flare.declFlare = declFlare;
  flare.renderModelFlare =
      new (lensFlareRenderTag_t::RenderModel) idRenderModelFlare(declFlare);
  flare.renderModelFlareOcclusionQuad =
      new (lensFlareRenderTag_t::RenderModel)
          idRenderModelFlareOcclusionQuad();
  flare.colorModulateTbl = nullptr;
  flare.sunDirection = sunDirection;
  flare.occlusionOffset = occlusionOffset;
  flare.modulateTimeScale = 1.0f;
  flare.cullDistanceSqr =
      LENS_FLARE_IS_SUN(flare) ? 0.0f : cullDistance * cullDistance;
  flare.handle.value.handleId = handleId;
  flare.handle.value.allocId = nextAllocId;
  flare.handle.value.flareIndex = index;
  flare.entitySpawnId.value = 0x1fff;
  flare.isAutosprited = isAutosprited;
  *reinterpret_cast<std::uint16_t*>(&flare.inPlayerPVS) = 0;
  renderWorld->AddRenderModel(flare.renderModelFlare);
  renderWorld->AddRenderModel(flare.renderModelFlareOcclusionQuad);
  return idFlareHandle{flareGuid_t(handleId, nextAllocId, index)};
}

#undef LENS_FLARE_IS_SUN
#undef LENS_FLARE_LOOKUP
