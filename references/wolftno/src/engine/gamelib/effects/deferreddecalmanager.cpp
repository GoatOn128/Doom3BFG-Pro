#include "engine/gamelib/effects/deferreddecalmanager.h"

#include "engine/models/rendermodel.h"
#include "shared/idlib/lib.h"

#include <cstdint>
#include <cstring>

class idCollisionModelManager;
extern idCollisionModelManager* collisionModelManager;

extern "C" bool DeferredDecalGetStepMoveResult(
    idCollisionModelManager*, trace_t*, idCollisionQuery*, bool);

#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:DeferredDecalGetStepMoveResult="                          \
    "?GetStepMoveResult@idCollisionModelManager@@QEAA_NPEAUtrace_t@@"         \
    "AEAVidCollisionQuery@@_N@Z")

namespace {

constexpr const char* DEFERRED_DECAL_CLIP_SOURCE =
    "w:\\build\\tech5\\engine\\gamelib\\effects\\DeferredDecalManager.cpp(67) : Clip";
constexpr const char* NULL_DECAL_WARNING =
    "Null decal passed in to  idDeferredDecalManager::AddDecalFromLineTrace";

struct retailDecalParams_t {
  const idMaterial* decalMat;
  float depth;
  idVec2 size;
  float angle;
  int lifeTime;
  int fadeOutTime;
  int fadeInTime;
  bool quad;
  bool overlap;
  std::uint8_t padding[2];
  float overlapRadius;
  int cost;
};

static_assert(sizeof(retailDecalParams_t) == 0x30,
              "retail decal-call parameters ABI drift");

using addDecalFromPoint_t = void(__fastcall*)(
    idRenderModelDecal*, decalHandle_t*, const decalParams_t*, unsigned,
    const idVec3*, const idVec3*, renderJointIndex_t);

void AddDecalFromPointRetail(idRenderModelDecal* model,
                             const decalParams_t* params, unsigned time,
                             const idVec3* position,
                             const idVec3* direction) {
  auto** vtable = *reinterpret_cast<void***>(model);
  const auto addDecal = reinterpret_cast<addDecalFromPoint_t>(vtable[4]);
  alignas(decalHandle_t) std::uint8_t resultStorage[sizeof(decalHandle_t)];
  addDecal(model, reinterpret_cast<decalHandle_t*>(resultStorage), params,
           time, position, direction, renderJointIndex_t{-1});
}

} // namespace

idDeferredDecalQuery::idDeferredDecalQuery() { Reset(); }

idDeferredDecalManager::idDeferredDecalManager()
    : numLineQueries(0), clip(nullptr), decalModel(nullptr) {}

void idDeferredDecalManager::Init(idClip* clip_,
                                   idRenderModelDecal* decalModel_) {
  clip = clip_;
  decalModel = decalModel_;
}

void idDeferredDecalManager::Shutdown() {
  numLineQueries = 0;
  clip = nullptr;
  decalModel = nullptr;
  for (int index = 0; index < 16; ++index) {
    lineQueries[index].Reset();
  }
}

void idDeferredDecalManager::AddDecalFromLineTrace(
    const deferredDecalParams_t* decalParam, const idVec3& start,
    const idVec3& end, int clipMask) {
  if (clip == nullptr || decalModel == nullptr) {
    return;
  }

  const int queryIndex = numLineQueries;
  if (queryIndex >= 16) {
    idLib::Warning("Too many deferred decal queries this frame!");
    return;
  }
  if (decalParam == nullptr || decalParam->decalMat == nullptr) {
    idLib::Warning(NULL_DECAL_WARNING);
    return;
  }

  lineQueries[queryIndex].Reset();
  const idClipQuery translated = clip->Translation(
      nullptr, start, end, nullptr, mat3_identity, clipMask, 0x2000, false,
      DEFERRED_DECAL_CLIP_SOURCE);
  lineQueries[numLineQueries].theQuery.index = translated.index;
  std::memcpy(&lineQueries[numLineQueries].decalParams, decalParam,
              sizeof(*decalParam));
  ++numLineQueries;
}

void idDeferredDecalManager::Update(int time) {
  if (clip == nullptr || decalModel == nullptr) {
    return;
  }

  for (int index = 0; index < numLineQueries; ++index) {
    idDeferredDecalQuery& lineQuery = lineQueries[index];
    const std::uint64_t queryIndex = lineQuery.theQuery.index;
    idClip* const currentClip = clip;

    if (queryIndex >= currentClip->collisionQueryFirstSubmittedIndex &&
        queryIndex <= currentClip->collisionQueryLastSubmittedIndex) {
      idClip::idClipCollisionQuery& collisionQuery =
          currentClip->collisionQueries[queryIndex & 0xFFF];
      const std::uint64_t resultOffset =
          collisionQuery.query.offset & 0x3FFFFFFFFFFFFFULL;

      if (resultOffset >= idCollisionQuery::finalResultsFirstSubmittedOffset &&
          resultOffset < idCollisionQuery::finalResultsLastSubmittedOffset) {
        const std::uint32_t savedQueryIndex =
            static_cast<std::uint32_t>(queryIndex);
        lineQuery.theQuery.index = 0;

        alignas(trace_t) std::uint8_t traceStorage[sizeof(trace_t)];
        trace_t* const newTrace = reinterpret_cast<trace_t*>(traceStorage);
        DeferredDecalGetStepMoveResult(
            collisionModelManager, newTrace,
            &currentClip->collisionQueries[savedQueryIndex & 0xFFF].query,
            false);

        if (newTrace->fraction < 1.0f && clip != nullptr) {
          idRenderModelDecal* const currentModel = decalModel;
          if (currentModel != nullptr) {
            if (lineQuery.decalParams.decalMat == nullptr) {
              idLib::Warning(NULL_DECAL_WARNING);
            } else {
              retailDecalParams_t params;
              params.decalMat = lineQuery.decalParams.decalMat;
              params.depth = lineQuery.decalParams.depth;
              params.size = lineQuery.decalParams.size;
              params.angle = lineQuery.decalParams.angle;
              params.lifeTime = lineQuery.decalParams.lifetime;
              params.fadeOutTime = lineQuery.decalParams.fadetime;
              params.fadeInTime = lineQuery.decalParams.fadeInEndTime;
              params.quad = false;
              params.overlap = lineQuery.decalParams.overlap;
              params.overlapRadius = lineQuery.decalParams.overlapRadius;
              params.cost = 1;
              AddDecalFromPointRetail(
                  currentModel, reinterpret_cast<const decalParams_t*>(&params),
                  static_cast<unsigned>(time), &newTrace->endpos,
                  &newTrace->c.normal);
            }
          }
        }
      }
    }

    lineQuery.Reset();
  }
  numLineQueries = 0;
}
