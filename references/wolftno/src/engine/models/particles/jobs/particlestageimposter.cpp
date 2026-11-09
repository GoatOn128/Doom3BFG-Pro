#include "engine/models/particles/jobs/particlestageimposter.h"

#include "shared/idlib/sys/sys_alloc.h"

#include <cstdio>
#include <immintrin.h>
#include <new>

namespace {

const __m128 vector_float_abs_mask = _mm_castsi128_ps(
    _mm_setr_epi32(0x7fffffff, 0x7fffffff, 0x7fffffff, 0x7fffffff));
const __m128 vector_float_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u),
                   static_cast<int>(0x80000000u)));
const __m128 vector_float_last_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(0, 0, 0, static_cast<int>(0x80000000u)));
const __m128 vector_float_first_sign_bit = _mm_castsi128_ps(
    _mm_setr_epi32(static_cast<int>(0x80000000u), 0, 0, 0));
const __m128 vector_float_clear_last = _mm_castsi128_ps(
    _mm_setr_epi32(-1, -1, -1, 0));
const __m128 DC_OFFSET_V128 =
    _mm_castsi128_ps(_mm_set1_epi32(0x26901d7d));

constexpr memTag_t kParticleMemoryTag = static_cast<memTag_t>(0x4f);

void InitializeLocationParm(idParticleParm& parm) {
  parm.val0 = 0.0f;
  parm.val1 = 0.0f;
  parm.variance = 0.0f;
  parm.tableIdx = -1;
  parm.table2Idx = -1;
  parm.calcType = PARTICLE_CALC_NONE;
}

void SetParticleParmAsConstant(idParticleParm& parm, float value,
                               float variance) {
  parm.val0 = value;
  parm.val1 = value;
  parm.variance = variance;
  parm.calcType = PARTICLE_CALC_GENERIC;
}

} // namespace

idParticleStageImposter::prtStageInfo_t*
idParticleStageImposter::AllocStageInfo() {
  void* storage = idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\models\\particles\\jobs\\ParticleStageImposter.cpp(89) : TAG_PARTICLE",
      sizeof(prtStageInfo_t), kParticleMemoryTag, false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  if (storage == nullptr)
    return nullptr;

  prtStageInfo_t* stageInfo = static_cast<prtStageInfo_t*>(storage);
  new (&stageInfo->name) idAtomicString;
  return stageInfo;
}

prtLocation_t* idParticleStageImposter::AllocLocation() {
  prtLocation_t* location =
      static_cast<prtLocation_t*>(idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\models\\particles\\jobs\\ParticleStageImposter.cpp(98) : TAG_PARTICLE",
          sizeof(prtLocation_t), kParticleMemoryTag, false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  if (location != nullptr) {
    InitializeLocationParm(location->loc[0]);
    InitializeLocationParm(location->loc[1]);
    InitializeLocationParm(location->loc[2]);
  }
  return location;
}

idParticleStageImposter::idParticleStageImposter() {
  stageData.granularity = 5;
  locationNames.granularity = 5;
  locations.granularity = 5;
  locations.ClearFree();
  stageData.ClearFree();
  name.Set("DEF");
}

idParticleStageImposter::~idParticleStageImposter() {
  for (int index = 0; index < locations.Num(); ++index)
    idMem::Singleton()->Free(locations[index], ALIGN_16);
  for (int index = 0; index < stageData.Num(); ++index)
    idMem::Singleton()->Free(stageData[index], ALIGN_16);
  locations.ClearFree();
  stageData.ClearFree();
}

idParticleStageImposter::prtStageInfo_t*
idParticleStageImposter::AddNewStage(const char* stageName,
                                     idParticleStage* stage) {
  prtStageInfo_t* stageInfo = AllocStageInfo();
  const int index = stageData.Append(stageInfo);
  stageData[index]->name.Set(stageName);
  stageData[index]->connection = stage;
  stageData[index]->propHandle = -1;
  return stageData[index];
}

prtLocation_t* idParticleStageImposter::AddNewSpawnlocation() {
  prtLocation_t* location = AllocLocation();
  const int index = locations.Append(location);

  char locationName[64];
  std::snprintf(locationName, sizeof(locationName), "Spawnlocation_%d",
                index + 1);
  idAtomicString atomicLocationName;
  atomicLocationName.Set(locationName);
  locationNames.Append(atomicLocationName);

  prtLocation_t* result = locations[index];
  SetParticleParmAsConstant(result->loc[0], 0.0f, 0.0f);
  SetParticleParmAsConstant(result->loc[1], 0.0f, 0.0f);
  SetParticleParmAsConstant(result->loc[2], 0.0f, 0.0f);
  result->diversity = 0;
  return result;
}
