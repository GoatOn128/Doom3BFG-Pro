#pragma once

#include "engine/sound/soundbspfile.h"

struct alignas(8) soundOcclusionResult_t {
  idVec3 virtualSoundOrigin;
  float occludedDistance;
  idVec3* pathPoints;
  int numPathPoints;
  int maxPathPoints;
  int valid;
};

struct alignas(8) soundOcclusionParms_t {
  soundOcclusionResult_t result;
  flatSoundBSP_t flatBSP;
  int* listenerRoute;
  int numAreas;
  float* doors;
  int numDoors;
  int listenerAreaNum;
  idVec3 listenerOrigin;
  int soundAreaNum;
  idVec3 soundOrigin;
  float maxDistance;
};

bool SoundTrace_i(const flatSoundBSP_t* flatBSP,
                  const idVec3* start, const idVec3* end);
bool ExternalSoundTrace(const flatSoundBSP_t* flatBSP,
                        const idVec3* start, const idVec3* end);
idVec3* SoundFaceIntersection(idVec3* result, const flatSoundBSP_t* flatBSP,
                              const sbspFace_t* face, const idVec3* lineStart,
                              const idVec3* lineEnd);
void SoundOcclusionJob(soundOcclusionParms_t* parms);

static_assert(sizeof(soundOcclusionResult_t) == 40);
static_assert(sizeof(soundOcclusionParms_t) == 192);
