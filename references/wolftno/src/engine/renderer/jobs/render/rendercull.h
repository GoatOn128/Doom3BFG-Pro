#pragma once

#include "engine/renderer/render.h"

#include <cstddef>

struct renderCullParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idBounds* areaNodeBounds;
  const idBounds* worldAreaSubspaceBounds;
  const idBounds* worldAreaGeometryBounds;
  int numAreaNodes;
  int numWorldAreas;
  unsigned char* areaNodeBoundsCulled;
  unsigned char* worldAreaSubspaceBoundsCulled;
  unsigned char* worldAreaGeometryBoundsCulled;
};

void RenderCull(const renderCullParms_t* parameters);

static_assert(offsetof(renderCullParms_t, numAreaNodes) == 0x28,
              "render-cull area count offset drift");
static_assert(offsetof(renderCullParms_t, areaNodeBoundsCulled) == 0x30,
              "render-cull output offset drift");
static_assert(sizeof(renderCullParms_t) == 0x48,
              "render-cull parameters ABI drift");

