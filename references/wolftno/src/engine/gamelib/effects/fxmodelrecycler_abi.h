#pragma once

#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/text/atomicstring.h"

#include <cstddef>

class idDeclParticle;
class idRenderModel;
class idRenderModelParticle;
class idRenderWorld;

class idFXModelRecycler {
public:
  struct fxPrtModel_t {
    idRenderModelParticle* pmodel;
    const idDeclParticle* pDecl;
  };

  struct fxStaticModel_t {
    fxStaticModel_t() : modelName() {}

    idRenderModel* rmodel;
    idAtomicString modelName;
  };

  idFXModelRecycler();
  ~idFXModelRecycler();

  void Init();
  void Shutdown();
  idRenderModelParticle* GetParticleFxModel(const idDeclParticle* particle,
                                            idRenderWorld* renderWorld);
  void RecycleParticleFxModel(const idDeclParticle* particle,
                              idRenderModelParticle* model);
  idRenderModel* GetStaticFxModel(const idAtomicString& modelName);
  void RecycleStaticFxModel(const idAtomicString& modelName,
                            idRenderModel* model);

  idStaticList<fxPrtModel_t, 256> fxPrtModels;
  idStaticList<int, 256> fxInactivePrtModels;
  idHashIndex activePrtModelHash;
  idHashIndex inactivePrtModelHash;
  idStaticList<fxStaticModel_t, 64> fxStaticModels;
  idStaticList<int, 64> fxInactiveStaticModels;
  idHashIndex activeStaticModelHash;
  idHashIndex inactiveStaticModelHash;
};

static_assert(sizeof(idFXModelRecycler::fxPrtModel_t) == 0x10,
              "FX particle recycler record ABI drift");
static_assert(sizeof(idFXModelRecycler::fxStaticModel_t) == 0x10,
              "FX static recycler record ABI drift");
static_assert(offsetof(idFXModelRecycler, fxInactivePrtModels) == 0x1018,
              "FX inactive particle-list offset drift");
static_assert(offsetof(idFXModelRecycler, activePrtModelHash) == 0x1430,
              "FX active particle-hash offset drift");
static_assert(offsetof(idFXModelRecycler, fxStaticModels) == 0x1480,
              "FX static model-list offset drift");
static_assert(offsetof(idFXModelRecycler, activeStaticModelHash) == 0x19B0,
              "FX active static-hash offset drift");
static_assert(sizeof(idFXModelRecycler) == 0x1A00,
              "FX model recycler ABI drift");
