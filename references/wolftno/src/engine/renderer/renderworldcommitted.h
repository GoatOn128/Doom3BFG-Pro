#pragma once

#include "engine/models/rendermodel.h"
#include "engine/renderer/renderindex.h"
#include "shared/idlib/containers/list.h"

#include <cstddef>
#include <cstdint>

struct worldArea_t {
  worldArea_t();
  ~worldArea_t();

  idList<idRenderIndex, 62> modelRefs;
  idList<idRenderIndex, 62> drawLightRefs;
  idList<idRenderIndex, 62> approximatedLightRefs;
  idList<idRenderIndex, 62> approximatedLightPrivateLightRefs;
  idList<idRenderModelSurface, 62> worldSurfaces;
  int areaNum;
  int firstSurfaceInWorldModel;
  int binaryModelId;
  std::uint32_t alignmentTail;
};

static_assert(offsetof(worldArea_t, drawLightRefs) == 0x18,
              "world-area draw-light list offset drift");
static_assert(offsetof(worldArea_t, worldSurfaces) == 0x60,
              "world-area surface list offset drift");
static_assert(offsetof(worldArea_t, binaryModelId) == 0x80,
              "world-area binary-model ID offset drift");
static_assert(sizeof(worldArea_t) == 0x88, "world-area ABI drift");
