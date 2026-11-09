#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/effects/gamelibeffects.h"

#pragma warning(push)
#pragma warning(disable : 4201 4324)
#include "engine/models/particles/rendermodeleffects.h"
#include "engine/models/rendermodelbeam.h"
#include "engine/models/rendermodeldecal.h"
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "shared/idlib/sys/sys_alloc.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <cstddef>
#include <new>

void operator delete[](void* memory, memTag_t tag) noexcept;

enum class gameLibEffectsRenderTag_t : int {
  Renderer = TAG_RENDERER,
  RenderModel = TAG_RENDERMODEL,
};

void operator delete(void* memory, gameLibEffectsRenderTag_t tag) noexcept;

#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:??3@YAXPEAXW4gameLibEffectsRenderTag_t@@@Z="             \
    "??_V@YAXPEAXW4memTag_t@@@Z")

__forceinline void* operator new(std::size_t size,
                                 gameLibEffectsRenderTag_t tag) noexcept {
  return idMem::Singleton().AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size,
      static_cast<memTag_t>(tag), false, ALIGN_16, HEAP_DEFAULTHEAP);
}

namespace {

void AddRenderModel(idRenderWorld* world, idRenderModel* model) {
  world->AddRenderModel(model);
}

void RetireRenderModel(idRenderModel*& model) {
  if (model == nullptr) {
    return;
  }

  model->CommitThisFrame();
  using RenderModelFunction = void (*)(idRenderModel*);
  RenderModelFunction* const vtable =
      *reinterpret_cast<RenderModelFunction**>(model);
  if ((model->unlinked & 8u) == 0u) {
    model->CommitThisFrame();
    vtable[7](model);
    model->unlinked |= 8u;
  }
  vtable[8](model);
  model->deleteOnSync |= 4u;
  model = nullptr;
}

}  // namespace

bool idGameLibEffects::Update(int currTime, int gameMillisecondsPerFrame,
                              int serverCurrTime) {
  if (!initialized) {
    return false;
  }

  deferredDecalManager.Update(currTime);
  if (effectsModel != nullptr) {
    effectsModel->Update(currTime, gameMillisecondsPerFrame);
  }
  if (beamModel != nullptr) {
    beamModel->Update(currTime);
  }
  if (beamDepthHackModel != nullptr) {
    beamDepthHackModel->Update(currTime);
  }
  weaponTraceManager.Update();
  weaponImpactManager.Update(vec3_origin, mat3_identity, currTime,
                             serverCurrTime);
  impactManager.Update(vec3_origin, mat3_identity, currTime, serverCurrTime);
  return true;
}

idDeferredDecalManager::~idDeferredDecalManager() = default;

void idGameLibEffects::Shutdown() {
  initialized = false;
  effectsModelManager.Shutdown();
  deferredDecalManager.Shutdown();
  ribbonModelManager.Shutdown();
  weaponTraceManager.ShutDown();
  fxModelRecycler.Shutdown();
  weaponImpactManager.Shutdown();
  impactManager.Shutdown();

  idRenderModel* model = effectsModel;
  RetireRenderModel(model);
  effectsModel = static_cast<idRenderModelEffects*>(model);

  model = beamModel;
  RetireRenderModel(model);
  beamModel = static_cast<idRenderModelBeam*>(model);

  model = beamDepthHackModel;
  RetireRenderModel(model);
  beamDepthHackModel = static_cast<idRenderModelBeam*>(model);
}

void idGameLibEffects::Init(idRenderWorld* renderWorld, idClip* clip,
                            idGlobalLensFlareManager* lensFlareManager_,
                            float diversity, int localPlayerIndex) {
  if (initialized) {
    Shutdown();
  }

  world = renderWorld;

  effectsModel =
      new (gameLibEffectsRenderTag_t::RenderModel) idRenderModelEffects();
  AddRenderModel(renderWorld, effectsModel);
  effectsModel->CommitThisFrame();

  decalModel = renderWorld->DecalModel();
  decalModel->CommitThisFrame();

  beamModel = new (gameLibEffectsRenderTag_t::RenderModel)
      idRenderModelBeam(true);
  AddRenderModel(renderWorld, beamModel);
  beamModel->CommitThisFrame();

  beamDepthHackModel =
      new (gameLibEffectsRenderTag_t::Renderer) idRenderModelBeam(true);
  AddRenderModel(renderWorld, beamDepthHackModel);
  beamDepthHackModel->g.modelDepthHack = 0.1f;
  beamDepthHackModel->CommitThisFrame();

  lensFlareManager = lensFlareManager_;
  effectsModelManager.Init(renderWorld);
  deferredDecalManager.Init(clip, decalModel);
  ribbonModelManager.Init();
  weaponTraceManager.ShutDown();
  fxModelRecycler.Init();
  weaponImpactManager.Init(&effectsModelManager, diversity, 64,
                           localPlayerIndex);
  impactManager.Init(&effectsModelManager, diversity, 32, localPlayerIndex);
  initialized = true;
}

idWeaponTraceManager::~idWeaponTraceManager() = default;

idGameLibEffects::~idGameLibEffects() noexcept(false) {
  effectsModel = nullptr;
  decalModel = nullptr;
  beamModel = nullptr;
  beamDepthHackModel = nullptr;
  lensFlareManager = nullptr;
}

idGameLibEffects::idGameLibEffects()
    : effectsModel(nullptr), decalModel(nullptr), beamModel(nullptr),
      beamDepthHackModel(nullptr), lensFlareManager(nullptr) {
  initialized = false;
}
