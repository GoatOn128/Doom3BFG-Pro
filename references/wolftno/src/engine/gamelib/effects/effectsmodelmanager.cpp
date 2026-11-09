#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_HASHINDEX_INVALID_ARRAY_ABI
#define WOLFTNO_RETAIL_HASHINDEX_MEMBER_ABI
#define WOLFTNO_RETAIL_HASHINDEX_OUTOFLINE_MUTATORS_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDLIST_EMPTY_CTOR_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI
#define WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#include "engine/gamelib/effects/effectsmodelmanager.h"

#pragma warning(push)
#pragma warning(disable : 4201 4324)
#include "engine/models/particles/declparticle.h"
#include "engine/models/particles/rendermodelparticle.h"
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/str.h"
#undef WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#undef WOLFTNO_RETAIL_EFFECTS_STATICLIST_CTOR_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_EMPTY_CTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_HASHINDEX_OUTOFLINE_MUTATORS_ABI
#undef WOLFTNO_RETAIL_HASHINDEX_MEMBER_ABI
#undef WOLFTNO_RETAIL_HASHINDEX_INVALID_ARRAY_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>

void operator delete[](void* memory, memTag_t tag) noexcept;

enum class effectsRenderTag_t : int {
  RenderModel = TAG_RENDERMODEL,
};

void operator delete(void* memory, effectsRenderTag_t tag) noexcept;

#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:??3@YAXPEAXW4effectsRenderTag_t@@@Z="                    \
    "??_V@YAXPEAXW4memTag_t@@@Z")

__forceinline void* operator new(std::size_t size,
                                 effectsRenderTag_t tag) noexcept {
  return idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size,
      static_cast<memTag_t>(tag), false, ALIGN_16, HEAP_DEFAULTHEAP);
}

idCVar g_showEffectsModelRecyclerStats(
    "g_showEffectsModelRecyclerStats", "0", CVAR_BOOL, "",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

__forceinline int* RetailInvalidHashIndex() {
  return idHashIndex::RetailInvalidIndexAddress();
}

__forceinline void ClearHashBuckets(idHashIndex& hashIndex) {
  if (hashIndex.hash != RetailInvalidHashIndex()) {
    std::memset(hashIndex.hash, 0xff,
                sizeof(int) * static_cast<std::size_t>(hashIndex.hashSize));
  }
}

__forceinline void ClearParticleModelList(
    idStaticList<idEffectsModelManager::fxPrtModel_t, 400>& models) {
  if (models.listStatic == 0 || models.listStatic == 2) {
    if (models.list != nullptr) {
      idMem::Singleton().Free(models.list, ALIGN_16);
    }
    models.list = nullptr;
    models.size = 0;
  }
  models.num = 0;
}

__forceinline void UnlinkSubclassRetail(idRenderModelParticle* model) {
  using Function = void (*)(idRenderModel*);
  Function* const vtable = *reinterpret_cast<Function**>(model);
  vtable[7](model);
}

__forceinline void DeleteSubclassRetail(idRenderModelParticle* model) {
  using Function = void (*)(idRenderModel*);
  Function* const vtable = *reinterpret_cast<Function**>(model);
  vtable[8](model);
}

__forceinline bool AddRenderModelRetail(idRenderWorld* world,
                                        idRenderModelParticle* model) {
  using Function = bool (*)(idRenderWorld*, idRenderModel*);
  void** const vtable = *reinterpret_cast<void***>(world);
  return reinterpret_cast<Function>(vtable[0x80 / sizeof(void*)])(world,
                                                                  model);
}

__forceinline void AddActiveHashExact(idHashIndex& hashIndex, int key,
                                      int index) {
  if (hashIndex.hash == RetailInvalidHashIndex()) {
    hashIndex.InternalAllocate(hashIndex.hashSize,
                               std::max(hashIndex.indexSize, index + 1));
  } else if (index >= hashIndex.indexSize) {
    hashIndex.InternalResizeIndex(index + 1);
  }

  const int bucket = key & hashIndex.hashMask;
  hashIndex.indexChain[index] = hashIndex.hash[bucket];
  hashIndex.hash[bucket] = index;
}

} // namespace

idEffectsModelManager::idEffectsModelManager()
    : fxPrtModels(), activePrtModelHash(), inactivePrtModelHash() {
  world = nullptr;
}

idEffectsModelManager::~idEffectsModelManager() noexcept(false) { Shutdown(); }

void idEffectsModelManager::Init(idRenderWorld* renderWorld) {
  world = renderWorld;
  ClearParticleModelList(fxPrtModels);
  ClearHashBuckets(activePrtModelHash);
  activePrtModelHash.memTag = TAG_PARTICLE;
  ClearHashBuckets(inactivePrtModelHash);
  inactivePrtModelHash.memTag = TAG_PARTICLE;
}

void idEffectsModelManager::Shutdown() {
  ClearHashBuckets(activePrtModelHash);
  ClearHashBuckets(inactivePrtModelHash);

  for (int index = 0; index < fxPrtModels.num; ++index) {
    idRenderModelParticle* const model = fxPrtModels.list[index].pmodel;
    if (model == nullptr) {
      continue;
    }

    model->CommitThisFrame();
    if ((model->unlinked & 8u) == 0u) {
      model->CommitThisFrame();
      UnlinkSubclassRetail(model);
      model->unlinked |= 8u;
    }
    DeleteSubclassRetail(model);
    model->deleteOnSync |= 4u;
    fxPrtModels.list[index].pmodel = nullptr;
  }

  ClearParticleModelList(fxPrtModels);
}

void idEffectsModelManager::RecycleParticleFxModel(
    const idDeclParticle* particleDecl, idRenderModelParticle* model) {
  if (model == nullptr || particleDecl == nullptr) {
    idLib::Warning("idFXModelRecycler::RecycleEffectModel : invalid model");
    return;
  }

  const int hash = idStr::Hash(particleDecl->name.str);
  const int activeKey = hash & activePrtModelHash.hashMask;
  const std::int64_t lookupMask = activePrtModelHash.lookupMask;
  int index = activePrtModelHash
                  .hash[lookupMask & activeKey & activePrtModelHash.hashMask];
  while (index != -1) {
    const fxPrtModel_t& entry = fxPrtModels.list[index];
    if (entry.pDecl == particleDecl && entry.pmodel == model) {
      activePrtModelHash.Remove(activeKey, index);
      inactivePrtModelHash.Add(hash & inactivePrtModelHash.hashMask, index);
      break;
    }
    index = activePrtModelHash.indexChain[index & lookupMask];
  }

  if (index == -1) {
    if (g_showEffectsModelRecyclerStats.valueInteger == 0) {
      return;
    }
    idLib::Printf(
        "idEffectsModelManager::RecycleParticleFxModel : model %s not found "
        "in active list for recycling\n",
        particleDecl->name.str);
  }

  if (g_showEffectsModelRecyclerStats.valueInteger != 0) {
    const int totalModels = *reinterpret_cast<volatile int*>(&fxPrtModels.num);
    const char* const declName =
        *reinterpret_cast<const char* const volatile*>(&particleDecl->name.str);
    const int activeSpread = activePrtModelHash.GetSpread();
    const int inactiveSpread = inactivePrtModelHash.GetSpread();
    idLib::Printf(
        "Recycling model %s, total models allocated %d inactive models "
        "spread %d active models spread %d\n",
        declName, totalModels, inactiveSpread, activeSpread);
  }
}

idRenderModelParticle* idEffectsModelManager::GetNextParticleEffectModel(
    const idDeclParticle* particleDecl) {
  if (particleDecl == nullptr) {
    idLib::Warning(
        "idEffectsModelManager::GetNextParticleEffectModel : particleDecl "
        "NULL");
    return nullptr;
  }

  const int hash = idStr::Hash(particleDecl->name.str);
  const int inactiveKey = hash & inactivePrtModelHash.hashMask;
  const std::int64_t lookupMask = inactivePrtModelHash.lookupMask;
  int index = inactivePrtModelHash.hash[lookupMask & inactiveKey &
                                        inactivePrtModelHash.hashMask];
  while (index != -1) {
    const fxPrtModel_t& entry = fxPrtModels.list[index];
    if (entry.pDecl == particleDecl && entry.pmodel != nullptr) {
      inactivePrtModelHash.Remove(inactiveKey, index);
      AddActiveHashExact(activePrtModelHash,
                         hash & activePrtModelHash.hashMask, index);

      if (g_showEffectsModelRecyclerStats.valueInteger != 0) {
        const int totalModels =
            *reinterpret_cast<volatile int*>(&fxPrtModels.num);
        const char* const declName =
            *reinterpret_cast<const char* const volatile*>(
                &particleDecl->name.str);
        const int activeSpread = activePrtModelHash.GetSpread();
        const int inactiveSpread = inactivePrtModelHash.GetSpread();
        idLib::Printf(
            "Found model %s in inactive list, total models allocated %d "
            "inactive models spread %d active models spread %d\n",
            declName, totalModels, inactiveSpread, activeSpread);
      }
      return fxPrtModels.list[index].pmodel;
    }
    index = inactivePrtModelHash.indexChain[index & lookupMask];
  }

  if (fxPrtModels.num >= fxPrtModels.size) {
    idLib::Warning(
        "idEffectsModelManager::GetNextParticleEffectModel: Could not "
        "allocate model %s, out of slots",
        particleDecl->name.str);
    return nullptr;
  }

  idStr particleDeclName(particleDecl->name.str);
  particleDeclName.SetFileExtension("prt");
  if (world == nullptr) {
    idLib::Warning(
        "idEffectsModelManager::GetNextParticleEffectModel: Could not "
        "allocate a rendermodel for '%s'",
        particleDecl->name.str);
    return nullptr;
  }

  idRenderModelParticle* const model =
      new (effectsRenderTag_t::RenderModel) idRenderModelParticle(particleDecl);

  AddRenderModelRetail(world, model);
  if (model == nullptr) {
    idLib::Warning(
        "idEffectsModelManager::GetNextParticleEffectModel: Could not "
        "allocate a rendermodel for '%s'",
        particleDecl->name.str);
    return nullptr;
  }

  const fxPrtModel_t particleModel{model, particleDecl};
  index = fxPrtModels.Append(particleModel);
  activePrtModelHash.Add(hash & activePrtModelHash.hashMask, index);

  if (g_showEffectsModelRecyclerStats.valueInteger != 0) {
    const int totalModels = *reinterpret_cast<volatile int*>(&fxPrtModels.num);
    const char* const declName =
        *reinterpret_cast<const char* const volatile*>(&particleDecl->name.str);
    const int activeSpread = activePrtModelHash.GetSpread();
    const int inactiveSpread = inactivePrtModelHash.GetSpread();
    idLib::Printf(
        "Allocated new model %s for active list, total models allocated %d "
        "inactive models spread %d active models spread %d\n",
        declName, totalModels, inactiveSpread, activeSpread);
  }

  return fxPrtModels.list[index].pmodel;
}
