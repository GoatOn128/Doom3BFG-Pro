#pragma once

#include "engine/models/particles/declparticle.h"
#include "engine/models/rendermodel.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/text/atomicstring.h"

#include <cstddef>
#include <cstdint>

class idRenderView;
class idRenderModelUpdateTools;

class idRenderModelDust final : public idRenderModel {
public:
  idRenderModelDust();
  ~idRenderModelDust() override;

  void SetParams(const idDeclParticle* const decl,
                 const idBounds& fullVolume, const idBounds& coreVolume,
                 float spacing, float edgeFadeCurve,
                 const dustParameters_t& dustParms);

private:
  bool UpdateInView(const idRenderView* currentView,
                    const idRenderView* nextView,
                    idRenderModelUpdateTools* updateTools) override;

public:
  int numParticles;
  dustParticle_t* particles;
  int lastRenderTime;
  deferredDustGenParms_t parms;
  const idDeclParticle* particleDecl;
};

extern idCVar r_useDustGenJobs;
extern idCVar r_skipDust;

static_assert(offsetof(idRenderModelDust, numParticles) == 2176,
              "idRenderModelDust particle count offset drift");
static_assert(offsetof(idRenderModelDust, particles) == 2184,
              "idRenderModelDust particles offset drift");
static_assert(offsetof(idRenderModelDust, lastRenderTime) == 2192,
              "idRenderModelDust render time offset drift");
static_assert(offsetof(idRenderModelDust, parms) == 2200,
              "idRenderModelDust deferred parms offset drift");
static_assert(offsetof(idRenderModelDust, particleDecl) == 2408,
              "idRenderModelDust declaration offset drift");
static_assert(sizeof(idRenderModelDust) == 2416,
              "idRenderModelDust ABI drift");
