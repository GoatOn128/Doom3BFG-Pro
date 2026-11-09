#pragma once

#include "engine/gamelib/effects/deferreddecalmanager.h"
#include "engine/gamelib/effects/effectsmodelmanager.h"
#include "engine/gamelib/effects/fxmodelrecycler_abi.h"
#include "engine/gamelib/effects/impactmanager.h"
#include "engine/gamelib/effects/ribbonmodelmanager_abi.h"
#include "engine/gamelib/effects/weapontracemanager.h"

#include <cstddef>
#include <cstdint>

class idClip;
class idGlobalLensFlareManager;
class idRenderModelBeam;
class idRenderModelDecal;
class idRenderModelEffects;
class idRenderWorld;
class idSerializer;

class idGameLibEffects {
public:
  idGameLibEffects();
  ~idGameLibEffects() noexcept(false);

  void Init(idRenderWorld* renderWorld, idClip* clip,
            idGlobalLensFlareManager* lensFlareManager, float diversity,
            int localPlayerIndex);
  __declspec(noinline) void Shutdown();
  bool Update(int currTime, int gameMillisecondsPerFrame,
              int serverCurrTime);
  void Serialize(idSerializer& serializer);

  idRenderWorld* GetWorld() { return world; }
  idRenderModelEffects* GetEffectsModel() { return effectsModel; }
  idRenderModelDecal* GetDecalModel() { return decalModel; }
  idRenderModelBeam* GetBeamModel() { return beamModel; }
  idRenderModelBeam* GetBeamDepthHackModel() { return beamDepthHackModel; }
  idEffectsModelManager* GetEffectsModelMgr() { return &effectsModelManager; }
  idDeferredDecalManager* GetDeferredDecalMgr() {
    return &deferredDecalManager;
  }
  idRibbonModelManager* GetRibbonModelMgr() { return &ribbonModelManager; }
  idWeaponTraceManager* GetWeaponTraceMgr() { return &weaponTraceManager; }
  idFXModelRecycler* GetFXModelRecycler() { return &fxModelRecycler; }
  idImpactManager* GetWeaponImpactMgr() { return &weaponImpactManager; }
  idImpactManager* GetImpactMgr() { return &impactManager; }
  const idGlobalLensFlareManager& GetLensFlareManager() {
    return *lensFlareManager;
  }

  idRenderWorld* world;
  idRenderModelEffects* effectsModel;
  idRenderModelDecal* decalModel;
  idRenderModelBeam* beamModel;
  idRenderModelBeam* beamDepthHackModel;
  idGlobalLensFlareManager* lensFlareManager;
  idEffectsModelManager effectsModelManager;
  idDeferredDecalManager deferredDecalManager;
  idRibbonModelManager ribbonModelManager;
  idWeaponTraceManager weaponTraceManager;
  idFXModelRecycler fxModelRecycler;
  idImpactManager weaponImpactManager;
  idImpactManager impactManager;
  bool initialized;
  std::uint8_t recoveredTail[7];
};

static_assert(sizeof(idGameLibEffects) == 0x40E8,
              "game-library effects ABI drift");
static_assert(offsetof(idGameLibEffects, effectsModelManager) == 0x30,
              "effects model manager offset drift");
static_assert(offsetof(idGameLibEffects, deferredDecalManager) == 0x19A0,
              "deferred decal manager offset drift");
static_assert(offsetof(idGameLibEffects, ribbonModelManager) == 0x1EB8,
              "ribbon model manager offset drift");
static_assert(offsetof(idGameLibEffects, weaponTraceManager) == 0x22B8,
              "weapon trace manager offset drift");
static_assert(offsetof(idGameLibEffects, fxModelRecycler) == 0x2650,
              "FX model recycler offset drift");
static_assert(offsetof(idGameLibEffects, weaponImpactManager) == 0x4050,
              "weapon impact manager offset drift");
static_assert(offsetof(idGameLibEffects, impactManager) == 0x4098,
              "impact manager offset drift");
static_assert(offsetof(idGameLibEffects, initialized) == 0x40E0,
              "game-library effects initialized offset drift");
