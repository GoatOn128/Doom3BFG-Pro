#pragma once

#include "engine/models/foliage/foliagemodel.h"
#include "engine/models/foliage/jobs/foliagegen.h"
#include "engine/models/rendermodel.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <cstddef>
#include <cstdint>

class idRenderView;
class idRenderModelUpdateTools;

struct foliageSegmentStats_t {
  int* quadsOffset;
  int* quadsAllocated;
  int* quadsUsed;
  int pad;
};

void FoliageGatherJob(const deferredFoliageGatherParms_t* parms);
void FoliageGenJob(const deferredFoliageGenParms_t* parms);

class alignas(16) idRenderModelFoliage final : public idRenderModel {
public:
  idRenderModelFoliage();
  ~idRenderModelFoliage() override;

  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;

  idFoliageModel* foliageBSP;
  deferredFoliageGatherParms_t* foliageGatherParms;
  deferredFoliageGenParms_t* foliageGenParms;
  foliageRenderView_t* viewData;
  foliageIndex_t* foliageIndices;
  std::int16_t* influenceSphereIndices;
  foliageSubTreeStats_t* subTreeFoliageStats;
};

extern idCVar r_useFoliageGenJobs;
extern idCVar r_showFoliageNodeBounds;
extern idCVar r_showFoliageSubTreeNodes;
extern idCVar r_showFoliageSubTreeBounds;

static_assert(sizeof(foliageSegmentStats_t) == 32);
static_assert(offsetof(idRenderModelFoliage, foliageBSP) == 2176);
static_assert(offsetof(idRenderModelFoliage, foliageGatherParms) == 2184);
static_assert(offsetof(idRenderModelFoliage, foliageGenParms) == 2192);
static_assert(offsetof(idRenderModelFoliage, viewData) == 2200);
static_assert(offsetof(idRenderModelFoliage, foliageIndices) == 2208);
static_assert(offsetof(idRenderModelFoliage, influenceSphereIndices) == 2216);
static_assert(offsetof(idRenderModelFoliage, subTreeFoliageStats) == 2224);
static_assert(sizeof(idRenderModelFoliage) == 2240);
