#pragma once

#include "engine/gamelib/idspawnid_abi.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

class idDeclFlare;
class idDeclTable;
class idEntity;
class idGame;
class idPVS;
class idRenderModelFlare;
class idRenderModelFlareOcclusionQuad;
class idRenderWorld;

struct flareGuid_t {
  int handleId;
  int allocId;
  int flareIndex;

  constexpr flareGuid_t(int value = -1)
      : handleId(value), allocId(value), flareIndex(value) {}
  constexpr flareGuid_t(int handle, int allocation, int index)
      : handleId(handle), allocId(allocation), flareIndex(index) {}
  constexpr bool operator==(const flareGuid_t&) const = default;
};

enum invalidFlareId_t : int { INVALID_FLARE_ID = -1 };

template <typename T, typename InvalidEnum, int InvalidValue> class idHandle {
public:
  T value{InvalidValue};
};

using idFlareHandle = idHandle<flareGuid_t, invalidFlareId_t, -1>;

class idGlobalLensFlareManager {
public:
  struct lensFlare_t {
    using idListAllocReturnsReferenceMarker = void;

    __forceinline lensFlare_t() noexcept : handle(), entitySpawnId() {}

    const idDeclFlare* declFlare;
    idRenderModelFlare* renderModelFlare;
    idRenderModelFlareOcclusionQuad* renderModelFlareOcclusionQuad;
    const idDeclTable* colorModulateTbl;
    idVec3 sunDirection;
    idVec3 occlusionOffset;
    float modulateTimeScale;
    float cullDistanceSqr;
    idFlareHandle handle;
    idSpawnId entitySpawnId;
    bool isAutosprited;
    bool inPlayerPVS;
    bool enabled;
  };

  idGlobalLensFlareManager();
  ~idGlobalLensFlareManager() noexcept(false);

  void SetupRenderModels(idFlareHandle& handle, float quadSize);
  void StartFlare(idFlareHandle& handle, const idVec3& origin,
                  const idMat3& axis, const idVec4& color, float fadeStart,
                  float fadeEnd);
  void StopFlare(idFlareHandle& handle);
  void UpdatePosition(idFlareHandle& handle, const idVec3& origin,
                      const idMat3& axis, bool updateOrigin, bool updateAxis);
  idRenderModelFlareOcclusionQuad*
  GetFlareOcclusionModel(idFlareHandle& handle);
  void SetEntity(idFlareHandle& handle, idEntity* entity);
  void SetColorModulateTable(idFlareHandle& handle,
                             const idDeclTable* colorTable,
                             float modulateTimeScale);
  void SetFlareColor(idFlareHandle& handle, const idVec4& color);
  void SetFlareFade(idFlareHandle& handle, float fade);
  void GameUpdate(idGame* game, const idPVS& pvs, int gameMS);
  void MapShutdown();
  void RemoveFlare(idFlareHandle handle);
  idFlareHandle AddFlare(idRenderWorld* renderWorld,
                         const idDeclFlare* declFlare,
                         const idVec3& sunDirection,
                         const idVec3& occlusionOffset, bool isAutosprited);

  idStaticList<lensFlare_t, 1024> lensFlares;
  int allocId;

protected:
  void Update(idFlareHandle& handle, float timeSeconds,
              const idVec3& origin, const idMat3& axis, bool updateOrigin,
              bool updateAxis);
};

static_assert(sizeof(flareGuid_t) == 0x0c, "flare GUID ABI drift");
static_assert(sizeof(idFlareHandle) == 0x0c, "flare handle ABI drift");
static_assert(offsetof(idGlobalLensFlareManager::lensFlare_t, declFlare) ==
                  0x00,
              "lens flare declaration offset drift");
static_assert(offsetof(idGlobalLensFlareManager::lensFlare_t,
                       renderModelFlare) == 0x08,
              "lens flare model offset drift");
static_assert(offsetof(idGlobalLensFlareManager::lensFlare_t,
                       renderModelFlareOcclusionQuad) == 0x10,
              "lens flare quad offset drift");
static_assert(offsetof(idGlobalLensFlareManager::lensFlare_t,
                       colorModulateTbl) == 0x18,
              "lens flare table offset drift");
static_assert(offsetof(idGlobalLensFlareManager::lensFlare_t, sunDirection) ==
                  0x20,
              "lens flare sun-direction offset drift");
static_assert(offsetof(idGlobalLensFlareManager::lensFlare_t,
                       occlusionOffset) == 0x2c,
              "lens flare occlusion offset drift");
static_assert(offsetof(idGlobalLensFlareManager::lensFlare_t,
                       modulateTimeScale) == 0x38,
              "lens flare time-scale offset drift");
static_assert(offsetof(idGlobalLensFlareManager::lensFlare_t,
                       cullDistanceSqr) == 0x3c,
              "lens flare cull-distance offset drift");
static_assert(offsetof(idGlobalLensFlareManager::lensFlare_t, handle) == 0x40,
              "lens flare handle offset drift");
static_assert(offsetof(idGlobalLensFlareManager::lensFlare_t, entitySpawnId) ==
                  0x4c,
              "lens flare spawn-id offset drift");
static_assert(offsetof(idGlobalLensFlareManager::lensFlare_t, isAutosprited) ==
                  0x50,
              "lens flare autosprite offset drift");
static_assert(sizeof(idGlobalLensFlareManager::lensFlare_t) == 0x58,
              "lens flare entry ABI drift");
static_assert(offsetof(idGlobalLensFlareManager, allocId) == 0x16018,
              "lens flare allocation-id offset drift");
static_assert(sizeof(idGlobalLensFlareManager) == 0x16020,
              "lens flare manager ABI drift");
