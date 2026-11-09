#pragma once

#include "engine/models/particles/jobs/particlegen.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/atomicstring.h"

#include <cstddef>

class idParticleStageImposter {
public:
  struct prtStageInfo_t {
    idAtomicString name;
    idParticleStage* connection;
    int propHandle;
    int linkHandle;
    int stageHandle;
  };

  idParticleStageImposter();
  ~idParticleStageImposter();

  prtStageInfo_t* AllocStageInfo();
  prtLocation_t* AllocLocation();
  prtStageInfo_t* AddNewStage(const char* stageName,
                              idParticleStage* stage);
  prtLocation_t* AddNewSpawnlocation();

  idAtomicString name;
  idList<prtStageInfo_t*, 5> stageData;
  idList<idAtomicString, 5> locationNames;
  idList<prtLocation_t*, 5> locations;
};

static_assert(sizeof(idParticleStageImposter::prtStageInfo_t) == 32,
              "particle imposter stage info ABI drift");
static_assert(offsetof(idParticleStageImposter::prtStageInfo_t, name) == 0,
              "particle imposter stage name offset drift");
static_assert(
    offsetof(idParticleStageImposter::prtStageInfo_t, connection) == 8,
    "particle imposter stage connection offset drift");
static_assert(offsetof(idParticleStageImposter::prtStageInfo_t, propHandle) ==
                  16,
              "particle imposter property handle offset drift");
static_assert(offsetof(idParticleStageImposter::prtStageInfo_t, linkHandle) ==
                  20,
              "particle imposter link handle offset drift");
static_assert(offsetof(idParticleStageImposter::prtStageInfo_t, stageHandle) ==
                  24,
              "particle imposter stage handle offset drift");
static_assert(sizeof(idParticleStageImposter) == 80,
              "idParticleStageImposter ABI drift");
static_assert(offsetof(idParticleStageImposter, name) == 0,
              "particle imposter name offset drift");
static_assert(offsetof(idParticleStageImposter, stageData) == 8,
              "particle imposter stage data offset drift");
static_assert(offsetof(idParticleStageImposter, locationNames) == 32,
              "particle imposter location names offset drift");
static_assert(offsetof(idParticleStageImposter, locations) == 56,
              "particle imposter locations offset drift");
