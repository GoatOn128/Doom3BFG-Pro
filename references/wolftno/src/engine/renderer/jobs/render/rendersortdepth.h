#pragma once

#include "engine/renderer/jobs/render/render_types.h"

#include <cstddef>
#include <cstdint>

struct idRenderModelCommitted;
class idRenderView;
struct renderSettings_t;

struct renderSortDepthParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idRenderModelCommitted** viewModels;
  std::uint64_t* viewModelSortKeys;
  int numViewModels;
  std::uint32_t alignmentBeforeSortedModels;
  const idRenderModelCommitted** sortedViewModels;
  renderPassDepthModels_t* renderViewModels;
};

void DepthModelQuickSort(const idRenderModelCommitted** viewModels,
                         char* keys, std::int64_t count);
void RenderSortDepth(const renderSortDepthParms_t* parameters);

static_assert(offsetof(renderSortDepthParms_t, sortedViewModels) == 0x28,
              "render-sort-depth sorted model offset drift");
static_assert(offsetof(renderSortDepthParms_t, renderViewModels) == 0x30,
              "render-sort-depth range offset drift");
static_assert(sizeof(renderSortDepthParms_t) == 56,
              "render-sort-depth parameters ABI drift");
