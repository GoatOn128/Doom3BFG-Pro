#pragma once

#include "engine/models/particles/declparticle.h"
#include "engine/models/particles/jobs/particlegen.h"
#include "engine/models/rendermodel.h"

#include <cstddef>

class idRenderModelParticle : public idRenderModel {
public:
  struct stageCounts_t {
    int vertCount{};
    int indexOffset{};
  };

  static void DeclNameToModelName(const char* declName, idStr& modelName);
  static int EstimateVertAllocation(const idParticleStage* stage,
                                    int renderTime, float timeOffset,
                                    int stopTime, float deadTime,
                                    int lodLevel);

  explicit idRenderModelParticle(const idDeclParticle* decl);
  void SetMeshDistribution(const particleMeshDist_t* mesh);

  idList<deferredParticleGenParms_t, 5> genParms;
  particleRenderView_t* particleRenderView{};
  modelParticleParms_t* modelParms{};
  idList<stageCounts_t, 5> stageCounts[3];
  const idDeclParticle* particleDecl{};
  const particleMeshDist_t* particleMeshDist{};
  idParmBlock lightingFullParmBlock;
  idParmBlock lightingAverageParmBlock;

protected:
  ~idRenderModelParticle() override;

private:
  void DrawDistributionVolumes();
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;
};

static_assert(sizeof(idRenderModelParticle::stageCounts_t) == 8,
              "particle stage-count ABI drift");
static_assert(offsetof(idRenderModelParticle, genParms) == 2176,
              "particle model generation parms offset drift");
static_assert(offsetof(idRenderModelParticle, particleRenderView) == 2200,
              "particle model view offset drift");
static_assert(offsetof(idRenderModelParticle, modelParms) == 2208,
              "particle model parms offset drift");
static_assert(offsetof(idRenderModelParticle, stageCounts) == 2216,
              "particle model stage counts offset drift");
static_assert(offsetof(idRenderModelParticle, particleDecl) == 2288,
              "particle model declaration offset drift");
static_assert(offsetof(idRenderModelParticle, particleMeshDist) == 2296,
              "particle model mesh distribution offset drift");
static_assert(offsetof(idRenderModelParticle, lightingFullParmBlock) == 2304,
              "particle full-lighting parm block offset drift");
static_assert(offsetof(idRenderModelParticle, lightingAverageParmBlock) == 2352,
              "particle average-lighting parm block offset drift");
static_assert(sizeof(idRenderModelParticle) == 2400,
              "idRenderModelParticle ABI drift");
