#pragma once

#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/staticlist.h"

#include <cstddef>

class idDeclParticle;
class idRenderModelParticle;
class idRenderWorld;

// ABI surface consumed by the impact manager.  The manager implementation is
// owned by the later gamelibeffects compiland; retaining its retail extent here
// lets enclosing game objects use the recovered layout in the meantime.
class idEffectsModelManager {
public:
  struct fxPrtModel_t {
    idRenderModelParticle* pmodel;
    const idDeclParticle* pDecl;
  };

  idEffectsModelManager();
  ~idEffectsModelManager() noexcept(false);

  void Init(idRenderWorld* renderWorld);
  __declspec(noinline) void Shutdown();
  idRenderModelParticle*
  GetNextParticleEffectModel(const idDeclParticle* particleDecl);
  void RecycleParticleFxModel(const idDeclParticle* particleDecl,
                              idRenderModelParticle* model);

  idRenderWorld* world;
  idStaticList<fxPrtModel_t, 400> fxPrtModels;
  idHashIndex activePrtModelHash;
  idHashIndex inactivePrtModelHash;
};

static_assert(sizeof(idEffectsModelManager::fxPrtModel_t) == 16,
              "idEffectsModelManager particle-model entry ABI drift");
static_assert(sizeof(idEffectsModelManager) == 6512,
              "idEffectsModelManager ABI drift");
static_assert(offsetof(idEffectsModelManager, world) == 0,
              "idEffectsModelManager world offset drift");
static_assert(offsetof(idEffectsModelManager, fxPrtModels) == 8,
              "idEffectsModelManager particle-model list offset drift");
static_assert(offsetof(idEffectsModelManager, activePrtModelHash) == 6432,
              "idEffectsModelManager active hash offset drift");
static_assert(offsetof(idEffectsModelManager, inactivePrtModelHash) == 6472,
              "idEffectsModelManager inactive hash offset drift");
