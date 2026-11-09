#pragma once

#include "engine/gamelib/physics/clip.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

class idMaterial;
class idRenderModelDecal;

struct deferredDecalParams_t {
  deferredDecalParams_t()
      : depth(8.0f), size{32.0f, 32.0f}, angle(0.0f), lifetime(10000),
        fadetime(2000), fadeInEndTime(0), overlapRadius(32.0f) {}

  const idMaterial* decalMat;
  float depth;
  idVec2 size;
  float angle;
  int lifetime;
  int fadetime;
  int fadeInEndTime;
  bool overlap;
  std::uint8_t recoveredPadding[3];
  float overlapRadius;
  std::uint8_t recoveredTail[4];
};

class idDeferredDecalQuery {
public:
  idDeferredDecalQuery();
  ~idDeferredDecalQuery() {}

  __forceinline void Reset() {
    decalParams.decalMat = nullptr;
    decalParams.depth = 8.0f;
    decalParams.size.x = 32.0f;
    decalParams.size.y = 32.0f;
    decalParams.angle = 0.0f;
    decalParams.lifetime = 10000;
    decalParams.fadetime = 2000;
    decalParams.fadeInEndTime = 0;
    decalParams.overlap = false;
    decalParams.overlapRadius = 32.0f;
    theQuery.index = 0;
  }

  idClipQuery theQuery;
  deferredDecalParams_t decalParams;
  idVec3 origin;
  idVec3 normal;
};

class idDeferredDecalManager {
public:
  idDeferredDecalManager();
  ~idDeferredDecalManager();

  void Init(idClip* clip_, idRenderModelDecal* decalModel_);
  void Shutdown();
  void AddDecalFromLineTrace(const deferredDecalParams_t* decalParam,
                             const idVec3& start, const idVec3& end,
                             int clipMask);
  void Update(int time);

  idDeferredDecalQuery lineQueries[16];
  int numLineQueries;
  std::uint8_t recoveredPadding[4];
  idClip* clip;
  idRenderModelDecal* decalModel;
};

static_assert(sizeof(deferredDecalParams_t) == 0x30,
              "deferred decal parameters ABI drift");
static_assert(offsetof(deferredDecalParams_t, depth) == 0x08,
              "deferred decal depth offset drift");
static_assert(offsetof(deferredDecalParams_t, overlap) == 0x24,
              "deferred decal overlap offset drift");
static_assert(offsetof(deferredDecalParams_t, overlapRadius) == 0x28,
              "deferred decal radius offset drift");
static_assert(sizeof(idDeferredDecalQuery) == 0x50,
              "deferred decal query ABI drift");
static_assert(offsetof(idDeferredDecalQuery, decalParams) == 0x08,
              "deferred decal query parameter offset drift");
static_assert(offsetof(idDeferredDecalQuery, origin) == 0x38,
              "deferred decal query origin offset drift");
static_assert(offsetof(idDeferredDecalManager, numLineQueries) == 0x500,
              "deferred decal query-count offset drift");
static_assert(offsetof(idDeferredDecalManager, clip) == 0x508,
              "deferred decal clip offset drift");
static_assert(offsetof(idDeferredDecalManager, decalModel) == 0x510,
              "deferred decal model offset drift");
static_assert(sizeof(idDeferredDecalManager) == 0x518,
              "deferred decal manager ABI drift");
