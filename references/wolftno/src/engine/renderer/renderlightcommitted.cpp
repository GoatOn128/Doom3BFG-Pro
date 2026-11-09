#include "engine/renderer/renderlightcommitted.h"

#include "engine/models/renderworld.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/declmaterial.h"

#include <cmath>

idRenderLightCommitted::idRenderLightCommitted()
    : name(nullptr),
      world(nullptr),
      index{static_cast<unsigned int>(-1)},
      r(),
      renderParmBlock(),
      references(),
      needsReferences(false),
      globalReferenceBounds(bounds_zero),
      globalLightOrigin{},
      currentColor{},
      maxVisibleRange{0.0f, 0.0f, 0.0f, 262144.0f},
      baseLightProject{},
      inverseBaseLightProject{},
      lightMtr(nullptr),
      castsShadows(false) {}

idRenderLightCommitted::~idRenderLightCommitted() noexcept(false) {
  if (world != nullptr) {
    world->UnlinkLightFromTree(this);
  }
}

void idRenderLightCommitted::PostCommit() {
  r.ComputeWorldToLightMatrix(baseLightProject, inverseBaseLightProject);
  r.ComputeLightGlobalOrigin(globalLightOrigin);

  const idVec4 color = lightMtr->renderParmBlock.GetVector(
      rp_lightColor.Get(), &renderParmBlock);
  const idVec4 exposure = lightMtr->renderParmBlock.GetVector(
      rp_exposure.Get(), &renderParmBlock);
  currentColor.x = exposure.x * color.x;
  currentColor.y = exposure.y * color.y;
  currentColor.z = exposure.z * color.z;

  if (!needsReferences) {
    return;
  }

  world->UnlinkLightFromTree(this);
  if (r.lightClass != LIGHT_BLENDED_ONLY && color.x == 0.0f &&
      color.y == 0.0f && color.z == 0.0f) {
    globalReferenceBounds.b[0] = r.origin;
    globalReferenceBounds.b[1] = r.origin;
    maxVisibleRange = {0.0f, 0.0f, 0.0f, 262144.0f};
  } else {
    idRenderMatrix::ProjectedBounds(globalReferenceBounds,
                                    inverseBaseLightProject,
                                    bounds_zeroOneCube, false);
    world->LinkLightIntoTree(this, inverseBaseLightProject,
                             bounds_zeroOneCube);

    if (r.maxVisibleRange > 0.0f) {
      const idVec3 center = {
          (globalReferenceBounds.b[1].x +
           globalReferenceBounds.b[0].x) *
              0.5f,
          (globalReferenceBounds.b[1].y +
           globalReferenceBounds.b[0].y) *
              0.5f,
          (globalReferenceBounds.b[1].z +
           globalReferenceBounds.b[0].z) *
              0.5f,
      };
      float radius = 0.0f;
      for (int point = 0; point < 2; ++point) {
        for (int axis = 0; axis < 3; ++axis) {
          radius = std::fmax(
              radius,
              globalReferenceBounds.b[point][axis] - center[axis]);
        }
      }
      maxVisibleRange = {center.x, center.y, center.z,
                         radius + r.maxVisibleRange};
    } else {
      maxVisibleRange = {0.0f, 0.0f, 0.0f, 262144.0f};
    }
  }

  needsReferences = false;
}
