#pragma once

#include "engine/renderer/parmblock.h"
#include "engine/renderer/renderindex.h"
#include "engine/renderer/renderlightparms.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/geometry/rendermatrix.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

class idMaterial;
class idRenderLight;
class idRenderWorldCommitted;
struct worldArea_t;

struct alignas(16) idRenderLightCommitted {
  void PostCommit();
  bool CullModelBounds(const idRenderMatrix& modelMatrix,
                       const idBounds& localBounds) const;

  const char* name;
  idRenderWorldCommitted* world;
  idRenderIndex index;
  idRenderLightParms r;
  alignas(16) idStaticParmBlock<32> renderParmBlock;
  idList<worldArea_t*, 62> references;
  bool needsReferences;
  idBounds globalReferenceBounds;
  idVec3 globalLightOrigin;
  idVec3 currentColor;
  idVec4 maxVisibleRange;
  idRenderMatrix baseLightProject;
  idRenderMatrix inverseBaseLightProject;
  const idMaterial* lightMtr;
  bool castsShadows;

#if defined(WOLF_RENDERER_RENDERLIGHTCOMMITTED_TEST_ACCESS)
public:
#else
private:
#endif
  friend class idRenderLight;

  idRenderLightCommitted();
  idRenderLightCommitted(const idRenderLightCommitted&) = delete;
  idRenderLightCommitted& operator=(const idRenderLightCommitted&) = delete;
  ~idRenderLightCommitted() noexcept(false);
};

static_assert(offsetof(idRenderLightCommitted, r) == 0x18,
              "committed-light parms offset drift");
static_assert(offsetof(idRenderLightCommitted, renderParmBlock) == 0x120,
              "committed-light parm-block offset drift");
static_assert(offsetof(idRenderLightCommitted, references) == 0x450,
              "committed-light references offset drift");
static_assert(offsetof(idRenderLightCommitted, needsReferences) == 0x468,
              "committed-light reference-state offset drift");
static_assert(offsetof(idRenderLightCommitted, globalReferenceBounds) == 0x46C,
              "committed-light reference-bounds offset drift");
static_assert(offsetof(idRenderLightCommitted, globalLightOrigin) == 0x484,
              "committed-light global-origin offset drift");
static_assert(offsetof(idRenderLightCommitted, currentColor) == 0x490,
              "committed-light current-color offset drift");
static_assert(offsetof(idRenderLightCommitted, maxVisibleRange) == 0x49C,
              "committed-light visibility-range offset drift");
static_assert(offsetof(idRenderLightCommitted, baseLightProject) == 0x4AC,
              "committed-light project offset drift");
static_assert(offsetof(idRenderLightCommitted, inverseBaseLightProject) == 0x4EC,
              "committed-light inverse-project offset drift");
static_assert(offsetof(idRenderLightCommitted, lightMtr) == 0x530,
              "committed-light material offset drift");
static_assert(offsetof(idRenderLightCommitted, castsShadows) == 0x538,
              "committed-light shadow-state offset drift");
static_assert(sizeof(idRenderLightCommitted) == 0x540,
              "committed-light ABI drift");
