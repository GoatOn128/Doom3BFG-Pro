#pragma once

#include "engine/sound/soundbspfile.h"

#include <cstdint>

struct alignas(8) soundRoutingParms_t {
  int* route;
  flatSoundBSP_t flatBSP;
  int numAreas;
  int listenerAreaNum;
  idVec3 listenerOrigin;
  std::uint8_t* temp;
  int tempSize;
};

void SoundRoutingJob(soundRoutingParms_t* parms);

static_assert(sizeof(soundRoutingParms_t) == 136);
