#define WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_DELETECONTENTS_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_RIBBON_ALLOC_REF_ABI
#define WOLFTNO_RETAIL_STATICLIST_FORCEINLINE_CTOR_ABI
#define WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#include "engine/gamelib/effects/ribbonmanager.h"

#include "engine/decls/declsimple.h"
#include "engine/gamelib/effects/ribbon.h"
#include "engine/models/rendermodelbeam.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#undef WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#undef WOLFTNO_RETAIL_STATICLIST_FORCEINLINE_CTOR_ABI
#undef WOLFTNO_RETAIL_RIBBON_ALLOC_REF_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_DELETECONTENTS_ABI
#undef WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI

#include <cstdint>
#include <new>

class idEntity;
class idScenePoint {
public:
  virtual void ActivateTargets(idEntity* target);
};

template <typename Type>
void idListArrayDelete(void* pointer, int count);

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

extern bool forceIdListsToReallocateEveryAppend;

void operator delete[](void* memory, memTag_t tag) noexcept;
void operator delete(void* memory, memTag_t tag) noexcept;

#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:??3@YAXPEAXW4memTag_t@@@Z="                             \
    "??_V@YAXPEAXW4memTag_t@@@Z")

__forceinline void* operator new(std::size_t size, memTag_t tag) noexcept {
  return idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size, tag,
      false, ALIGN_16, HEAP_DEFAULTHEAP);
}

using RibbonEffect = idRibbonModelManager::ribbonEffects_t;
using RibbonEffectList = idList<RibbonEffect, 5>;

template <>
RibbonEffectList::~idList() {
  if (listStatic == 0 || listStatic == 2) {
    if (list != nullptr) {
      idListArrayDelete<RibbonEffect>(list, size);
    }
    list = nullptr;
    size = 0;
  }
  num = 0;
}

template <>
__declspec(noinline) bool RibbonEffectList::Resize(int newSize) {
  if (newSize <= 0) {
    if (listStatic == 0 || listStatic == 2) {
      if (list != nullptr) {
        idListArrayDelete<RibbonEffect>(list, size);
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

  list = static_cast<RibbonEffect*>(idListArrayResize<RibbonEffect>(
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
__declspec(noinline) RibbonEffect& RibbonEffectList::Alloc() {
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

namespace {

template <typename Type>
__forceinline bool ResizeRibbonValueList(idList<Type, 5>* values,
                                         int newSize) {
  if (newSize <= 0) {
    if (values->listStatic == 0 || values->listStatic == 2) {
      if (values->list != nullptr) {
        idMem::Singleton().Free(values->list, ALIGN_16);
      }
      values->list = nullptr;
      values->size = 0;
    }
    values->num = 0;
    return true;
  }

  if (newSize == values->size || values->listStatic != 0) {
    return true;
  }
  if (values->size > 0 || newSize == values->granularity) {
    reinterpret_cast<idScenePoint*>(values)->idScenePoint::ActivateTargets(
        nullptr);
  }

  values->list = static_cast<Type*>(idListArrayResize<Type>(
      values->list, values->size, newSize,
      static_cast<memTag_t>(values->memTag), false));
  if (values->list == nullptr) {
    return false;
  }
  values->size = newSize;
  if (values->num > newSize) {
    values->num = newSize;
  }
  return true;
}

} // namespace

template <>
__declspec(noinline) bool idList<idRibbon*, 5>::Resize(int newSize) {
  return ResizeRibbonValueList(this, newSize);
}

template <>
__declspec(noinline) bool idList<int, 5>::Resize(int newSize) {
  return ResizeRibbonValueList(this, newSize);
}

template <>
__declspec(noinline) void idList<idRibbon*, 5>::DeleteContents() {
  for (int index = 0; index < num; ++index) {
    idRibbon* const ribbon = list[index];
    if (ribbon != nullptr) {
      ribbon->ribbonDecl = nullptr;
      idMem::Singleton().Free(ribbon, ALIGN_16);
    }
    list[index] = nullptr;
  }

  if (listStatic == 0 || listStatic == 2) {
    if (list != nullptr) {
      idMem::Singleton().Free(list, ALIGN_16);
    }
    list = nullptr;
    size = 0;
  }
  num = 0;
}

namespace {

} // namespace

#define RIBBON_LOOKUP_EXACT(variable, manager)                                 \
  idRibbon* variable = nullptr;                                                \
  do {                                                                         \
    const int lookupType = (manager)->ribbonHandle.type;                       \
    if (lookupType == -1) {                                                    \
      break;                                                                   \
    }                                                                          \
    const int lookupIndex = (manager)->ribbonHandle.index;                     \
    if (lookupIndex == -1) {                                                   \
      break;                                                                   \
    }                                                                          \
    const int lookupId = (manager)->ribbonHandle.id;                           \
    if (lookupId == -1) {                                                      \
      break;                                                                   \
    }                                                                          \
    RibbonEffect& lookupEffect =                                               \
        (manager)->modelManager->ribbonEffects.list[lookupType];               \
    if (lookupEffect.handles.list[lookupIndex] != lookupId) {                  \
      break;                                                                   \
    }                                                                          \
    variable = lookupEffect.ribbons.list[lookupIndex];                         \
  } while (false)

template <>
__declspec(noinline) idRibbon*& idList<idRibbon*, 5>::Alloc() {
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

template <>
__declspec(noinline) int& idList<int, 5>::Alloc() {
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

void idRibbonModelManager::Init() { handleId = 0; }

idRibbonManager::idRibbonManager()
{
  ribbonMustBeNullDontEverUse = nullptr;
  ribbonHandle.type = -1;
  ribbonHandle.index = -1;
  ribbonHandle.id = -1;
  ribbonDecl = nullptr;
  beamModel = nullptr;
  modelManager = nullptr;
  isInitialized = false;
}

void idRibbonManager::StopRibbon() {
  RIBBON_LOOKUP_EXACT(ribbon, this);
  if (ribbon != nullptr) {
    ribbon->ribbonState = RIBBON_DEACTIVATING;
  }
}

bool idRibbonManager::UpdateRibbon(const int time, const idVec3& origin,
                                   const idMat3& axis,
                                   const idVec3& velocity,
                                   const idVec4& color,
                                   const idVec3& translate) {
  RIBBON_LOOKUP_EXACT(ribbon, this);
  if (ribbon == nullptr) {
    return false;
  }
  ribbon->RemoveOldNodes(time);
  ribbon->UpdatePosition(time, origin, axis, velocity, color);
  ribbon->UpdateGeometry(time, beamModel, axis, translate);
  return ribbon->ribbonState != RIBBON_INACTIVE;
}

int idRibbonManager::GetRemainingTime(const int currentTime) const {
  RIBBON_LOOKUP_EXACT(ribbon, this);
  if (ribbon != nullptr) {
    ribbon->GetRemainingTime(currentTime);
  }
  return 0;
}

__declspec(noinline) ribbonHandle_t
idRibbonModelManager::AllocRibbonEffectModel(
    const idDeclRibbon* ribbonDecl) {
  int type = 0;
  RibbonEffect* const effects = ribbonEffects.list;
  for (; type < ribbonEffects.num; ++type) {
    RibbonEffect& effect = effects[type];
    if (effect.ribbons.num > 0) {
      idRibbon* const ribbon = effect.ribbons.list[0];
      if (ribbon != nullptr && ribbon->ribbonDecl == ribbonDecl) {
        break;
      }
    }
  }

  if (type >= ribbonEffects.num || type == -1) {
    idLib::Warning(
        "idRibbonModelManager::GetNextRibbonEffectModel: Ribbon '%s' not in "
        "list",
        ribbonDecl->name.str);
    ribbonHandle_t result;
    return result;
  }

  RibbonEffect& effect = effects[type];
  const int index = effect.next;
  const int id = handleId;
  effect.handles.list[index] = id;
  effect.next = (index + 1) % effect.ribbons.num;
  handleId = (handleId + 1) % 0x7FFFFFFF;
  ribbonHandle_t result;
  result.type = type;
  result.index = index;
  result.id = id;
  return result;
}

void idRibbonManager::Shutdown() {
  StopRibbon();
  ribbonHandle.type = -1;
  ribbonHandle.index = -1;
  isInitialized = false;
  ribbonHandle.id = -1;
}

void idRibbonManager::StartRibbon(const int spawnTime,
                                  const idVec3& spawnOrigin) {
  if (ribbonDecl == nullptr || modelManager == nullptr) {
    return;
  }

  ribbonHandle = modelManager->AllocRibbonEffectModel(ribbonDecl);
  RIBBON_LOOKUP_EXACT(ribbon, this);
  if (ribbon != nullptr) {
    ribbon->StartRibbon(spawnTime, spawnOrigin);
  }
}

idRibbonManager::~idRibbonManager() {
  StopRibbon();
  ribbonHandle.type = -1;
  ribbonHandle.index = -1;
  isInitialized = false;
  ribbonDecl = nullptr;
  beamModel = nullptr;
  modelManager = nullptr;
  ribbonHandle.id = -1;
}

void idRibbonModelManager::Shutdown() {
  for (int index = 0; index < ribbonEffects.num; ++index) {
    ribbonEffects.list[index].ribbons.DeleteContents();
  }

  if (ribbonEffects.listStatic == 0 || ribbonEffects.listStatic == 2) {
    RibbonEffect* const allocation = ribbonEffects.list;
    if (allocation != nullptr) {
      for (int index = 0; index < ribbonEffects.size; ++index) {
        allocation[index].~ribbonEffects_t();
      }
      idMem::Singleton().Free(allocation, ALIGN_16);
    }
    ribbonEffects.list = nullptr;
    ribbonEffects.size = 0;
  }
  ribbonEffects.num = 0;
}

idRibbonModelManager::~idRibbonModelManager() noexcept(false) { Shutdown(); }

idRibbonModelManager::idRibbonModelManager()
    : ribbonEffects(), handleId(0) {}

__declspec(noinline) void idRibbonModelManager::CreateRibbonEffectModelType(
    const idDeclRibbon* ribbonDecl) {
  int type = 0;
  const int numEffects = ribbonEffects.num;
  for (; type < numEffects; ++type) {
    RibbonEffect& effect = ribbonEffects.list[type];
    if (effect.ribbons.num > 0) {
      idRibbon* const ribbon = effect.ribbons.list[0];
      if (ribbon != nullptr && ribbon->ribbonDecl == ribbonDecl) {
        break;
      }
    }
  }

  if (type >= numEffects || type == -1) {
    if (numEffects >= ribbonEffects.size) {
      idLib::Warning(
          "idRibbonModelManager::CreateRibbonEffectModelType: No more free "
          "effect spots, could not add '%s'",
          ribbonDecl->name.str);
      return;
    }
    type = numEffects;
    ribbonEffects.Alloc();
  }

  RibbonEffect& effect = ribbonEffects.list[type];
  const int capacity = effect.ribbons.size;
  if (effect.ribbons.num < capacity) {
    for (int index = 0; index < effect.ribbons.size; ++index) {
      idRibbon* const ribbon = new (TAG_PARTICLE) idRibbon(ribbonDecl);
      if (ribbon != nullptr) {
        effect.ribbons.Alloc() = ribbon;
        effect.handles.Alloc() = -1;
      }
    }
  }
}

void idRibbonManager::Init(const idDeclRibbon* newRibbonDecl,
                           idRenderModelBeam* newBeamModel,
                           idRibbonModelManager* newModelManager) {
  if (newRibbonDecl != nullptr && newBeamModel != nullptr &&
      newModelManager != nullptr) {
    ribbonHandle.type = -1;
    ribbonHandle.index = -1;
    ribbonDecl = newRibbonDecl;
    beamModel = newBeamModel;
    ribbonHandle.id = -1;
    modelManager = newModelManager;
    newModelManager->CreateRibbonEffectModelType(newRibbonDecl);
    isInitialized = true;
  }
}

#undef RIBBON_LOOKUP_EXACT
