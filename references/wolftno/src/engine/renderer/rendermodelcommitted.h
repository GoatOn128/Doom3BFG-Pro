#pragma once

#include "engine/models/rendermodel.h"
#include "engine/renderer/approximatelighting.h"
#include "engine/renderer/parmblock.h"
#include "engine/renderer/rendermodelparms.h"
#include "engine/renderer/renderindex.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/geometry/rendermatrix.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

class idRenderWorldCommitted;
struct worldArea_t;

struct alignas(16) idRenderModelCommitted {
private:
  friend class idRender;
  friend class idRenderModel;
#ifdef WOLF_RENDERER_RENDERMODELCOMMITTED_TEST_ACCESS
public:
#endif
  idRenderModelCommitted();
  ~idRenderModelCommitted() noexcept(false);

public:
  void LocalPointToGlobal(const idVec3& in, idVec3& out) const;
  void SetFlagsFromSurfaces();
  void PostCommit();
  void CheckBounds() const;
  idVec3 GetLightingPoint() const;
  float GetLightingRadius() const;

  const char* name;
  idRenderWorldCommitted* world;
  idRenderIndex index;
  idBounds referenceBounds;
  std::uint8_t alignmentBeforeParms[4];
  idRenderModelParms r;
  std::uint8_t alignmentBeforeParmBlock[8];
  idStaticParmBlock<32> renderParmBlock;
  idList<idRenderModelSurface, 62> surfaces;
  idList<worldArea_t*, 62> references;
  idRenderModelCommitted* nextAddAlwaysModel;
  idRenderModelCommitted* nextDimShadowModel;
  idRenderModelCommitted* nextApproxLightModel;
  idRenderModelCommitted* referenceModel;
  int guiFrameCount;
  idVec4 maxVisibleRange;
  idBounds globalReferenceBounds;
  idVec3 localLightingPoint;
  float localLightingRadius;
  idRenderMatrix modelMatrix;
  idRenderMatrix inverseModelMatrix;
  approximateLighting_t approximateLighting[2];
  union {
    std::uint32_t approximateState;
    struct {
      std::uint32_t currentApprox : 1;
      std::uint32_t queuedApprox : 1;
      std::uint32_t renderModelType : 30;
    };
  };
  union {
    std::uint8_t committedFlags0;
    struct {
      std::uint8_t lightCenterInitialized : 1;
      std::uint8_t needsReferences : 1;
      std::uint8_t needsApproximatedLighting : 1;
      std::uint8_t mayBeDrawn : 1;
      std::uint8_t mayCastShadows : 1;
      std::uint8_t mayMakeInteractions : 1;
      std::uint8_t mayMakeSubSurfaceScattering : 1;
      std::uint8_t hasUniqueVirtualTexture : 1;
    };
  };
  union {
    std::uint8_t committedFlags1;
    struct {
      std::uint8_t mayContributeToPreZ : 1;
      std::uint8_t forceApproximateLightingCalculation : 1;
      std::uint8_t preventApproximateLightingCalculation : 1;
      std::uint8_t forceNoGPUocclusionTest : 1;
      std::uint8_t committedFlags1Unused : 4;
    };
  };
  std::uint8_t alignmentBeforeMvp[2];
  idRenderMatrix mvpMatrix;
  idVec4 highlightColor;

  int CurrentApproximateLighting() const {
    return static_cast<int>(approximateState & 1u);
  }
};

static_assert(offsetof(idRenderModelCommitted, index) == 0x10,
              "committed-model index offset drift");
static_assert(offsetof(idRenderModelCommitted, referenceBounds) == 0x14,
              "committed-model reference-bounds offset drift");
static_assert(offsetof(idRenderModelCommitted, renderParmBlock) == 0x110,
              "committed-model parm-block offset drift");
static_assert(offsetof(idRenderModelCommitted, surfaces) == 0x440,
              "committed-model surfaces offset drift");
static_assert(offsetof(idRenderModelCommitted, modelMatrix) == 0x4CC,
              "committed-model matrix offset drift");
static_assert(offsetof(idRenderModelCommitted, inverseModelMatrix) == 0x50C,
              "committed-model inverse-matrix offset drift");
static_assert(offsetof(idRenderModelCommitted, approximateLighting) == 0x54C,
              "committed-model approximate-lighting offset drift");
static_assert(offsetof(idRenderModelCommitted, committedFlags0) == 0x710,
              "committed-model primary flags offset drift");
static_assert(offsetof(idRenderModelCommitted, committedFlags1) == 0x711,
              "committed-model secondary flags offset drift");
static_assert(offsetof(idRenderModelCommitted, mvpMatrix) == 0x714,
              "committed-model MVP offset drift");
static_assert(offsetof(idRenderModelCommitted, highlightColor) == 0x754,
              "committed-model highlight offset drift");
static_assert(sizeof(idRenderModelCommitted) == 0x770,
              "committed-model ABI drift");
