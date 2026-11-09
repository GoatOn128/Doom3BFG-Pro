#pragma once

#include "engine/renderer/jobs/render/render_types.h"

#include <cstddef>

struct renderSettings_t;
class idRenderView;

struct renderSortParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  drawSurf_t* drawSurfs;
  int numDrawSurfs;
  std::uint8_t pointerAlignmentPadding[4];
  drawSurf_t** sortedDrawSurfs;
  drawSurf_t** depthSortedDrawSurfs;
  int* numDepthSortedDrawSurfs;
  renderPassSurfaces_t* renderPasses;
};

void DrawSurfQuickSort(drawSurf_t** surfaces, std::int64_t count);
void DepthDrawSurfQuickSort(drawSurf_t** surfaces, std::int64_t count);
void RenderSort(const renderSortParms_t* parameters);

static_assert(offsetof(renderSortParms_t, numDrawSurfs) == 0x18,
              "render-sort count offset drift");
static_assert(offsetof(renderSortParms_t, sortedDrawSurfs) == 0x20,
              "render-sort output offset drift");
static_assert(offsetof(renderSortParms_t, renderPasses) == 0x38,
              "render-sort pass offset drift");
static_assert(sizeof(renderSortParms_t) == 0x40,
              "render-sort parameters ABI drift");
