#pragma once

#include "engine/renderer/jobs/render/renderdrawsurf.h"

#include <cstddef>
#include <cstdint>

class idDeclRenderParm;
class idRenderView;
class idRenderDestination;
struct renderSettings_t;

enum renderPass_t : int {
  RENDERPASS_CLEAR = 0,
  RENDERPASS_SORTED = 1,
  RENDERPASS_BACKGROUND = 2,
  RENDERPASS_BACKGROUND_EMISSIVE = 3,
  RENDERPASS_SSS = 4,
  RENDERPASS_EMISSIVE = 5,
  RENDERPASS_EMISSIVE_ONLY = 6,
  RENDERPASS_BLEND = 7,
  RENDERPASS_DISTORTION = 8,
  NUM_RENDER_PASSES = 9,
};

struct renderPassParms_t {
  renderPass_t pass;
  std::uint32_t alignmentBeforeSettings;
  const renderSettings_t* settings;
  const idRenderView* renderView;
  renderDrawSurfParms_t drawSurfParms;
  const idRenderDestination* renderDestDefault;
  const idRenderDestination* renderDestTemp;
  const idRenderDestination* renderDestDistortion;
  const idDeclRenderParm* rpViewTarget;
  const idDeclRenderParm* rpViewTargetMS;
  const idDeclRenderParm* rpSSSDirection;
  const renderPassSurfaces_t* renderPasses;
  drawSurf_t** sortedDrawSurfs;
  drawSurf_t** depthSortedDrawSurfs;
  const int* numDepthSortedDrawSurfs;
  const bool* binaryModelState;
};

void RenderClearPass(const renderPassParms_t* parameters);
void RenderSSSPass(const renderPassParms_t* parameters);
void RenderBackgroundPass(const renderPassParms_t* parameters);
void RenderBackgroundEmissivePass(const renderPassParms_t* parameters);
void RenderEmissivePass(const renderPassParms_t* parameters);
void RenderEmissiveOnlyPass(const renderPassParms_t* parameters);
void RenderBlendPass(const renderPassParms_t* parameters);
void RenderDepthSortedPass(const renderPassParms_t* parameters);
void RenderDistortionPass(const renderPassParms_t* parameters);
void RenderPass(const renderPassParms_t* parameters);

static_assert(offsetof(renderPassParms_t, settings) == 0x08,
              "render-pass settings offset drift");
static_assert(offsetof(renderPassParms_t, drawSurfParms) == 0x18,
              "render-pass draw parameters offset drift");
static_assert(offsetof(renderPassParms_t, renderDestDefault) == 0x1D0,
              "render-pass default destination offset drift");
static_assert(offsetof(renderPassParms_t, rpSSSDirection) == 0x1F8,
              "render-pass SSS direction offset drift");
static_assert(offsetof(renderPassParms_t, renderPasses) == 0x200,
              "render-pass ranges offset drift");
static_assert(offsetof(renderPassParms_t, binaryModelState) == 0x220,
              "render-pass binary-state offset drift");
static_assert(sizeof(renderPassParms_t) == 0x228,
              "render-pass parameters ABI drift");
