#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/effects/impactmanager.h"

#include "engine/gamelib/effects/effectsmodelmanager.h"
#pragma warning(push)
#pragma warning(disable : 4201 4324)
#include "engine/models/particles/declparticle.h"
#include "engine/models/particles/rendermodelparticle.h"
#include "engine/models/renderworld.h"
#pragma warning(pop)
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/declrenderparm.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <cstdint>
#include <new>

idRenderDebug* GetGameLibRenderDebug();

idCVar g_debugImpactMgr(
    "g_debugImpactMgr", "0", CVAR_INTEGER, "impact manager debug",
    static_cast<argCompletionRef_t>(nullptr));
idCodeResource<idDeclRenderParm> rp_particleLOD("particleLOD");

namespace {

template <typename T>
bool ResizeImpactList(idList<T, 5>* list, int newSize) {
  if (newSize <= 0) {
    if (list->listStatic == 0 || list->listStatic == 2) {
      if (list->list != nullptr) {
        idMem::Singleton()->Free(list->list, ALIGN_16);
      }
      list->list = nullptr;
      list->size = 0;
    }
    list->num = 0;
    return true;
  }
  if (newSize == list->size || list->listStatic != 0) {
    return true;
  }

  idMem* const memory = &idMem::Singleton();
  T* const replacement = static_cast<T*>(memory->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag",
      sizeof(T) * static_cast<std::size_t>(newSize),
      static_cast<memTag_t>(list->memTag), false, ALIGN_16,
      HEAP_DEFAULTHEAP));
  if (replacement == nullptr) {
    return false;
  }
  for (int index = 0; index < newSize; ++index) {
    ::new (static_cast<void*>(&replacement[index])) T;
  }
  int copyCount = list->size;
  if (newSize < copyCount) {
    copyCount = newSize;
  }
  for (int index = 0; index < copyCount; ++index) {
    replacement[index] = list->list[index];
  }
  memory->Free(list->list, ALIGN_16);
  list->list = replacement;
  list->size = newSize;
  if (list->num > newSize) {
    list->num = newSize;
  }
  return true;
}

} // namespace

template <>
__declspec(noinline) bool
idList<idImpactManager::impactState_t, 5>::Resize(int newSize) {
  return ResizeImpactList(this, newSize);
}

template <>
__declspec(noinline) bool
idList<idImpactManager::impactBindState_t, 5>::Resize(int newSize) {
  return ResizeImpactList(this, newSize);
}

namespace {

bool UsesDeferredTransform(const idRenderModelParticle* model) {
  return (model->useDeferredPosition &
          (model->deferredPositionInitialized >> 1) & 1u) != 0;
}

void UnlinkSubclassRetail(idRenderModelParticle* model) {
  using UnlinkFunction = void (*)(idRenderModel*);
  auto* const vtable = *reinterpret_cast<UnlinkFunction**>(model);
  vtable[7](model);
}

void UnlinkImpactModel(idRenderModelParticle* model) {
  if ((model->unlinked & 8u) == 0) {
    model->CommitThisFrame();
    UnlinkSubclassRetail(model);
    model->unlinked |= 8u;
  }
}

void SetImpactOrigin(idRenderModelParticle* model, const idVec3& origin) {
  if (!UsesDeferredTransform(model)) {
    model->g.origin = origin;
  }
  model->deferredOrigin = origin;
}

void SetImpactAxis(idRenderModelParticle* model, const idMat3& axis) {
  if (!UsesDeferredTransform(model)) {
    model->g.axis = axis;
  }
  model->deferredAxis = axis;
}

} // namespace

idImpactManager::idImpactManager()
    : next(0), random{}, initialized(false), effectsModelManager(nullptr) {}

idImpactManager::~idImpactManager() {
  Shutdown();
  effectsModelManager = nullptr;
}

void idImpactManager::StopImpacts() {
  for (int index = 0; index < impacts.num; ++index) {
    impactState_t& impact = impacts.list[index];
    idRenderModelParticle* const model = impact.pmodel;
    if (model != nullptr) {
      UnlinkImpactModel(model);
      model->SetMeshDistribution(nullptr);
      idEffectsModelManager* const manager = effectsModelManager;
      if (manager != nullptr) {
        manager->RecycleParticleFxModel(model->particleDecl, model);
        impact.pmodel = nullptr;
      }
    }
    impact.startTime = 0;
    impact.endTime = 0;
  }
}

void idImpactManager::Update(const idVec3& parentOrigin,
                             const idMat3& parentAxis, int localTime,
                             int playerIndex) {
  static_cast<void>(playerIndex);
  for (int index = 0; index < impacts.num; ++index) {
    impactState_t& impact = impacts.list[index];
    idRenderModelParticle* const model = impact.pmodel;
    if (model == nullptr) {
      continue;
    }

    if (impact.endTime != 0 && localTime < impact.endTime) {
      impactBindState_t& bind = impactBinds.list[index];
      if (bind.bindToParent) {
        const float x = bind.relOrg.x;
        const float y = bind.relOrg.y;
        const float z = bind.relOrg.z;
        idVec3 origin;
        origin.x = (y * parentAxis.mat[1].x + x * parentAxis.mat[0].x) +
                   z * parentAxis.mat[2].x + parentOrigin.x;
        origin.y = (x * parentAxis.mat[0].y + y * parentAxis.mat[1].y) +
                   z * parentAxis.mat[2].y + parentOrigin.y;
        origin.z = (x * parentAxis.mat[0].z + y * parentAxis.mat[1].z) +
                   z * parentAxis.mat[2].z + parentOrigin.z;
        SetImpactOrigin(model, origin);

        const idMat3 axis = bind.relRot * parentAxis;
        SetImpactAxis(model, axis);
        model->CommitThisFrame();
      }
    } else if ((model->unlinked & 8u) == 0) {
      model->CommitThisFrame();
      UnlinkSubclassRetail(model);
      model->unlinked |= 8u;
      model->SetMeshDistribution(nullptr);
      effectsModelManager->RecycleParticleFxModel(model->particleDecl, model);
      impact.pmodel = nullptr;
    }
  }
}

void idImpactManager::UseImpact(
    const idVec3& origin, const idMat3& axis,
    const idDeclParticle* particleDecl, int startTime, bool bindToParent,
    const idVec3& parentOrigin, const idMat3& parentAxis, int playerIndex,
    float distSqr, const idColor& color, int numCycles,
    const particleMeshDist_t* mesh) {
  static_cast<void>(playerIndex);
  if (g_debugImpactMgr.valueInteger > 0) {
    GetGameLibRenderDebug()->DebugAxis(origin, axis, 200, false);
  }
  if (!initialized || particleDecl == nullptr) {
    return;
  }

  impactState_t& impact = impacts.list[next];
  impactBindState_t& bind = impactBinds.list[next];
  idRenderModelParticle* model = impact.pmodel;
  if (model != nullptr) {
    UnlinkImpactModel(model);
    model->SetMeshDistribution(nullptr);
    idEffectsModelManager* const manager = effectsModelManager;
    if (manager != nullptr) {
      manager->RecycleParticleFxModel(model->particleDecl, model);
      impact.pmodel = nullptr;
    }
  }

  idEffectsModelManager* const manager = effectsModelManager;
  if (manager != nullptr) {
    impact.pmodel =
        manager->GetNextParticleEffectModel(particleDecl);
  }
  model = impact.pmodel;
  if (model == nullptr) {
    return;
  }

  impact.startTime = startTime;
  const std::uint32_t duration =
      static_cast<std::uint32_t>(numCycles) *
          static_cast<std::uint32_t>(particleDecl->maxSystemDuration) +
      static_cast<std::uint32_t>(startTime) + 1000u;
  impact.endTime = static_cast<std::int32_t>(duration);
  bind.bindToParent = bindToParent;
  if (bindToParent) {
    const float deltaX = origin.x - parentOrigin.x;
    const float deltaY = origin.y - parentOrigin.y;
    const float deltaZ = origin.z - parentOrigin.z;
    bind.relOrg.x =
        (deltaY * parentAxis.mat[0].y + deltaX * parentAxis.mat[0].x) +
        deltaZ * parentAxis.mat[0].z;
    bind.relOrg.y =
        (deltaY * parentAxis.mat[1].y + deltaX * parentAxis.mat[1].x) +
        deltaZ * parentAxis.mat[1].z;
    bind.relOrg.z =
        (deltaY * parentAxis.mat[2].y + deltaX * parentAxis.mat[2].x) +
        deltaZ * parentAxis.mat[2].z;

    idMat3 inverseParent = parentAxis;
    inverseParent.InverseSelf();
    bind.relRot = axis * inverseParent;
  }

  model->SetMeshDistribution(mesh);
  SetImpactOrigin(model, origin);
  SetImpactAxis(model, axis);

  const std::uint32_t seed = random.seed * 1664525u + 1013904223u;
  random.seed = seed;
  const float diversity =
      static_cast<float>((seed >> 10) & 0x7FFFu) * 0.000030518509f;
  model->SetParm(rp_Diversity.ConstPtr(), diversity);
  model->SetParm(rp_TimeStop.ConstPtr(), 0.0f);
  model->SetParm(rp_TimeOffset.ConstPtr(),
                 static_cast<float>(startTime) * 0.001f);
  const idVec4 parmColor{color.r, color.g, color.b, color.a};
  model->SetParm(rp_Color.ConstPtr(), parmColor);
  model->SetParm(rp_NoShadows.ConstPtr(), 0.0f);
  const int lod = particleDecl->CalcLodForDistance(distSqr);
  model->SetParm(rp_particleLOD.ConstPtr(), static_cast<float>(lod));
  model->CommitThisFrame();

  ++next;
  if (next >= impacts.num) {
    next = 0;
  }
}

void idImpactManager::Shutdown() {
  StopImpacts();
  impacts.ClearFree();
  impactBinds.ClearFree();
  effectsModelManager = nullptr;
  next = 0;
  initialized = false;
}

void idImpactManager::Init(idEffectsModelManager* manager, float diversity,
                           int maxImpacts, int playerIndex) {
  static_cast<void>(playerIndex);
  if (initialized) {
    return;
  }

  effectsModelManager = manager;
  next = 0;
  random.seed = static_cast<std::uint32_t>(
      static_cast<std::int64_t>(diversity * 65535.0f));

  if (maxImpacts > impacts.size) {
    impacts.ResizeForPort(maxImpacts);
  }
  impacts.listStatic = 2;
  if (maxImpacts > impactBinds.size) {
    impactBinds.ResizeForPort(maxImpacts);
  }
  impactBinds.listStatic = 2;

  for (int index = 0; index < impacts.size; ++index) {
    impactState_t& impact = impacts.Alloc();
    impact.pmodel = nullptr;
    impact.startTime = 0;
    impact.endTime = 0;
  }
  for (int index = 0; index < impactBinds.size; ++index) {
    impactBindState_t& bind = impactBinds.Alloc();
    bind.bindToParent = false;
    bind.relOrg = vec3_origin;
    bind.relRot = mat3_identity;
  }
  initialized = true;
}
