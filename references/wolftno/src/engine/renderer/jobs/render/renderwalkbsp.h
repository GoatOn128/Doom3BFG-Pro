#pragma once

#include "engine/renderer/jobs/render/render_types.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/math/plane.h"

#include <cstddef>
#include <cstdint>

class idRenderView;
struct renderSettings_t;

struct areaNode_t {
  idPlane plane;
  int children[2];
};

struct renderWalkBSPParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const std::uint8_t* areaNodeBoundsCulled;
  const int* areaNodeOcclusionIndex;
  const std::int64_t* areaNodeOcclusionQuery;
  const idBounds* areaNodeBounds;
  const areaNode_t* areaNodes;
  int numAreaNodes;
  int numWorldAreas;
  int* occlusionTestNodes;
  int* numOcclusionTestNodes;
  nodeWorldArea_t* nodeWorldAreas;
  int* numNodeWorldAreas;
};

void RenderWalkBSP(const renderWalkBSPParms_t* parameters);

static_assert(sizeof(areaNode_t) == 0x18, "area-node ABI drift");
static_assert(offsetof(renderWalkBSPParms_t, numAreaNodes) == 0x38,
              "walk-BSP node count offset drift");
static_assert(offsetof(renderWalkBSPParms_t, occlusionTestNodes) == 0x40,
              "walk-BSP query output offset drift");
static_assert(offsetof(renderWalkBSPParms_t, nodeWorldAreas) == 0x50,
              "walk-BSP area output offset drift");
static_assert(sizeof(renderWalkBSPParms_t) == 0x60,
              "walk-BSP parameters ABI drift");
