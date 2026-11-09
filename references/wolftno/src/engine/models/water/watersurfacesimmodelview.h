#pragma once

#include "engine/renderer/rendermodelparms.h"
#include "shared/idlib/bv/bounds.h"

#include <cstddef>
#include <cstdint>

class idRenderModel;

// Prefix of idRenderModel consumed by the water simulation. Keeping this
// models-side view avoids coupling the simulation validator to renderer and
// declaration implementations while retaining the PDB-exact field offsets.
struct idWaterSurfaceRenderModelView {
  std::byte vtable[8];
  std::byte name[8];
  const char* debugName;
  idVec3 deferredOrigin;
  idMat3 deferredAxis;
  std::uint8_t useDeferredPosition;
  std::uint8_t deferredPositionInitialized;
  std::uint8_t deleteOnSync;
  std::uint8_t unlinked;
  std::uint8_t forceUpdateInView;
  std::uint8_t flagPadding[3];
  idBounds referenceBounds;
  idRenderModelParms g;

  static idWaterSurfaceRenderModelView& From(idRenderModel* model) {
    return *reinterpret_cast<idWaterSurfaceRenderModelView*>(model);
  }
  static const idWaterSurfaceRenderModelView& From(
      const idRenderModel* model) {
    return *reinterpret_cast<const idWaterSurfaceRenderModelView*>(model);
  }
};

static_assert(offsetof(idWaterSurfaceRenderModelView, deferredOrigin) == 0x18);
static_assert(offsetof(idWaterSurfaceRenderModelView, deferredAxis) == 0x24);
static_assert(offsetof(idWaterSurfaceRenderModelView, useDeferredPosition) ==
              0x48);
static_assert(offsetof(idWaterSurfaceRenderModelView, g) == 0x68);
static_assert(sizeof(idWaterSurfaceRenderModelView) == 0x140);
