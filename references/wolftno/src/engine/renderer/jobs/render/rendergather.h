#pragma once

#include "engine/renderer/jobs/render/renderdepth.h"

#include <cstddef>
#include <cstdint>

class idLookupTable;
class idRenderView;
struct idRenderLightCommitted;
struct idRenderModelCommitted;
class idRenderModelSurface;
struct occlusionState_t;
struct renderSettings_t;
struct worldArea_t;

struct renderGatherParms_t {
  std::uint8_t* temp;
  int tempSize;
  std::uint32_t alignmentBeforeSettings;
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idRenderModelCommitted* const* committedRenderModels;
  int numCommittedRenderModels;
  std::uint32_t alignmentBeforeLights;
  const idRenderLightCommitted* const* committedRenderLights;
  int numCommittedRenderLights;
  std::uint32_t alignmentBeforeAreas;
  const nodeWorldArea_t* nodeWorldAreas;
  const std::uint8_t* worldAreaSubspaceBoundsCulled;
  const std::uint8_t* worldAreaGeometryBoundsCulled;
  int numNodeWorldAreas;
  std::uint32_t alignmentBeforeWorldAreas;
  const worldArea_t* worldAreas;
  int numWorldAreas;
  std::uint32_t alignmentBeforeAddAlways;
  const idRenderModelCommitted* addAlwaysModels;
  const bool* binaryModelState;
  const idLookupTable* highlightTable;
  float highlightDuration;
  bool highlightEnable;
  std::uint8_t alignmentBeforeOcclusionState[3];
  occlusionState_t* occlusionState;
  int* occlusionTestNodes;
  int* numOcclusionTestNodes;
  viewWorldArea_t* viewWorldAreas;
  int* numViewWorldAreas;
  const idRenderModelSurface** viewWorldSurfaces;
  int* numViewWorldSurfaces;
  const idRenderModelCommitted** viewModels;
  std::uint64_t* viewModelSortKeys;
  int* numViewModels;
  const idRenderModelCommitted** viewModelsPreZ;
  int* numViewModelsPreZ;
  const idRenderModelCommitted** viewGodrayModels;
  int* numViewGodrayModels;
  const idRenderLightCommitted** viewLights;
  int* numViewLights;
  int* occlusionTestWorldAreas;
  int* numOcclusionTestWorldAreas;
  const idRenderModelCommitted** occlusionTestModels;
  int* numOcclusionTestModels;
  const idRenderLightCommitted** occlusionTestLights;
  int* numOcclusionTestLights;
  float* splitPlaneDistance;
};

void RenderGather(const renderGatherParms_t* parameters);

static_assert(offsetof(renderGatherParms_t, settings) == 0x10,
              "render-gather settings offset drift");
static_assert(offsetof(renderGatherParms_t, nodeWorldAreas) == 0x40,
              "render-gather node-area offset drift");
static_assert(offsetof(renderGatherParms_t, addAlwaysModels) == 0x70,
              "render-gather add-always offset drift");
static_assert(offsetof(renderGatherParms_t, occlusionState) == 0x90,
              "render-gather occlusion-state offset drift");
static_assert(offsetof(renderGatherParms_t, viewModels) == 0xC8,
              "render-gather view-model offset drift");
static_assert(offsetof(renderGatherParms_t, occlusionTestWorldAreas) == 0x110,
              "render-gather occlusion-world-area offset drift");
static_assert(offsetof(renderGatherParms_t, splitPlaneDistance) == 0x140,
              "render-gather split-plane offset drift");
static_assert(sizeof(renderGatherParms_t) == 0x148,
              "render-gather parameters ABI drift");
