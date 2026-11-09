#include "engine/renderer/jobs/render/renderpasses.h"

#include "engine/models/rendermodel.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/rendermodelcommitted.h"

#include <array>
#include <cstdint>

namespace {

void SetViewViewportAndScissor(const idRenderView& view) {
  GL_Viewport(view.viewport.x1, view.viewport.y1,
              view.viewport.x2 - view.viewport.x1 + 1,
              view.viewport.y2 - view.viewport.y1 + 1);
  GL_Scissor(view.scissorRect.x1, view.scissorRect.y1,
             view.scissorRect.x2 - view.scissorRect.x1 + 1,
             view.scissorRect.y2 - view.scissorRect.y1 + 1);
}

void BindDefaultDestination(const renderPassParms_t& parameters) {
  GL_SetRenderDestination(
      idODSObject<idRenderDestination>{parameters.renderDestDefault}, 0, 0);
  SetViewViewportAndScissor(*parameters.renderView);
}

void RenderRangedPass(const renderPassParms_t& parameters,
                      renderPass_t pass, std::uint64_t extraState) {
  BindDefaultDestination(parameters);
  idRenderDrawSurf renderer;
  const renderPassSurfaces_t& range = parameters.renderPasses[pass];
  for (int index = range.firstSurface; index <= range.lastSurface; ++index) {
    const drawSurf_t* surface = parameters.sortedDrawSurfs[index];
    if (parameters.binaryModelState[surface->modelSurface->binaryModelId])
      renderer.RenderDrawSurf(&parameters.drawSurfParms, surface, extraState);
  }
}

float ProjectViewDistance(const idRenderMatrix& projection,
                          float distance) {
  const float viewZ = -distance;
  return (viewZ * projection.m[10] + projection.m[11]) /
         (viewZ * projection.m[14] + projection.m[15]);
}

float ComputeSSSFade(const renderPassParms_t& parameters,
                     const idRenderModelCommitted& model) {
  float depthMinimum;
  float depthMaximum;
  idRenderMatrix::DepthBoundsForBounds(depthMinimum, depthMaximum,
                                       model.mvpMatrix,
                                       model.referenceBounds, true);
  const float fadeStart = ProjectViewDistance(
      parameters.renderView->projectionMatrix,
      parameters.settings->subSurfaceScatteringFadeVisibilityRange);
  if (depthMinimum <= fadeStart)
    return 1.0f;

  const float fadeEnd = ProjectViewDistance(
      parameters.renderView->projectionMatrix,
      parameters.settings->subSurfaceScatteringMaxVisibilityRange);
  float fade = 1.0f - (depthMinimum - fadeStart) / (fadeEnd - fadeStart);
  if (fade >= 0.0f) {
    if (fade > 1.0f)
      fade = 1.0f;
  } else {
    fade = 0.0f;
  }
  return fade;
}

}  // namespace

void RenderClearPass(const renderPassParms_t* parameters) {
  BindDefaultDestination(*parameters);
  GL_State(UINT64_C(0x4000), false);
  GL_Clear(true, false, false, 0x80, 0.0f, 0.0f, 0.0f, 1.0f);
}

void RenderSSSPass(const renderPassParms_t* parameters) {
  const renderPassSurfaces_t& range =
      parameters->renderPasses[RENDERPASS_SSS];
  if (range.firstSurface > range.lastSurface)
    return;

  idRenderDrawSurf renderer;
  const idRenderDestination* viewColor =
      parameters->drawSurfParms.captureParms.renderDestViewColor;
  const idRenderDestination* defaultDestination =
      parameters->renderDestDefault;
  const idRenderDestination* temporaryDestination =
      parameters->renderDestTemp;
  const int renderWidth = parameters->renderView->renderWidth;
  const int renderHeight = parameters->renderView->renderHeight;
  const int windowWidth = parameters->renderView->windowWidth;
  const int windowHeight = parameters->renderView->windowHeight;

  renderThreadParmState->SetFragmentPositionToViewTexture(
      windowWidth, windowHeight, windowWidth, windowHeight);
  GL_SetRenderDestination(idODSObject<idRenderDestination>{viewColor}, 0, 0);
  GL_Viewport(0, 0, windowWidth, windowHeight);
  GL_Scissor(0, 0, windowWidth, windowHeight);
  GL_Clear(true, false, false, 0, 0.0f, 0.0f, 0.0f, 0.0f);
  GL_Viewport(0, 0, renderWidth, renderHeight);
  GL_Scissor(0, 0, renderWidth, renderHeight);

  for (int index = range.firstSurface; index <= range.lastSurface; ++index) {
    const drawSurf_t* surface = parameters->sortedDrawSurfs[index];
    if (surface->material != nullptr && surface->material->progs[4] != nullptr) {
      renderer.RenderDrawSurfProgram(&parameters->drawSurfParms, surface,
                                     surface->material->progs[4]);
    }
  }

  renderThreadParmState->SetParmVector(parameters->rpSSSDirection, 1.0f,
                                      0.0f, 0.0f, 0.0f);
  const idTextureSampler* defaultColor =
      reinterpret_cast<const idTextureSampler*>(
          defaultDestination->targetImage[0]);
  renderThreadParmState->SetParmSampler(parameters->rpViewTarget,
                                       defaultColor);
  renderThreadParmState->SetParmSampler(parameters->rpViewTargetMS,
                                       defaultColor);
  renderThreadParmState->SetParmSampler(
      parameters->drawSurfParms.captureParms.rpViewColorMap,
      reinterpret_cast<const idTextureSampler*>(viewColor->targetImage[0]));
  GL_SetRenderDestination(
      idODSObject<idRenderDestination>{temporaryDestination}, 0, 0);
  GL_Viewport(0, 0, windowWidth, windowHeight);
  GL_Scissor(0, 0, windowWidth, windowHeight);
  GL_Clear(true, false, false, 0, 0.0f, 0.0f, 0.0f, 1.0f);
  GL_Viewport(0, 0, renderWidth, renderHeight);
  GL_Scissor(0, 0, renderWidth, renderHeight);

  std::array<float, 8192> surfaceFade{};
  const idRenderModelCommitted* previousModel = nullptr;
  float fade = 1.0f;
  for (int index = range.firstSurface; index <= range.lastSurface; ++index) {
    const drawSurf_t* surface = parameters->sortedDrawSurfs[index];
    if (!parameters->settings->subSurfaceScatteringSkipRangeCulling &&
        surface->model != nullptr && surface->model != previousModel) {
      previousModel = surface->model;
      fade = ComputeSSSFade(*parameters, *surface->model);
    }
    surfaceFade[index] = fade;
    renderThreadParmState->SetParmVector(parameters->rpSSSDirection, 1.0f,
                                        0.0f, 0.0f, fade);
    renderer.RenderDrawSurf(&parameters->drawSurfParms, surface, 0);
  }

  const idTextureSampler* temporaryColor =
      reinterpret_cast<const idTextureSampler*>(
          temporaryDestination->targetImage[0]);
  renderThreadParmState->SetParmSampler(parameters->rpViewTarget,
                                       temporaryColor);
  renderThreadParmState->SetParmSampler(parameters->rpViewTargetMS,
                                       temporaryColor);
  renderThreadParmState->SetParmSampler(
      parameters->drawSurfParms.captureParms.rpViewColorMap,
      reinterpret_cast<const idTextureSampler*>(viewColor->targetImage[0]));
  GL_SetRenderDestination(
      idODSObject<idRenderDestination>{defaultDestination}, 0, 0);
  GL_Viewport(0, 0, renderWidth, renderHeight);
  GL_Scissor(0, 0, renderWidth, renderHeight);
  GL_ResetTextureState();
  for (int index = range.firstSurface; index <= range.lastSurface; ++index) {
    renderThreadParmState->SetParmVector(parameters->rpSSSDirection, 0.0f,
                                        1.0f, 0.0f, surfaceFade[index]);
    renderer.RenderDrawSurf(&parameters->drawSurfParms,
                            parameters->sortedDrawSurfs[index], 0);
  }
}

void RenderBackgroundPass(const renderPassParms_t* parameters) {
  RenderRangedPass(*parameters, RENDERPASS_BACKGROUND, UINT64_C(0x400));
}

void RenderBackgroundEmissivePass(const renderPassParms_t* parameters) {
  RenderRangedPass(*parameters, RENDERPASS_BACKGROUND_EMISSIVE,
                   UINT64_C(0x400));
}

void RenderEmissivePass(const renderPassParms_t* parameters) {
  RenderRangedPass(*parameters, RENDERPASS_EMISSIVE, UINT64_C(0x400));
}

void RenderEmissiveOnlyPass(const renderPassParms_t* parameters) {
  RenderRangedPass(*parameters, RENDERPASS_EMISSIVE_ONLY, 0);
}

void RenderBlendPass(const renderPassParms_t* parameters) {
  BindDefaultDestination(*parameters);
  idRenderDrawSurf renderer;
  const renderPassSurfaces_t& range =
      parameters->renderPasses[RENDERPASS_BLEND];
  for (int index = range.firstSurface; index <= range.lastSurface; ++index) {
    const drawSurf_t* surface = parameters->sortedDrawSurfs[index];
    if (!parameters->binaryModelState[surface->modelSurface->binaryModelId])
      continue;
    const std::uint64_t state =
        (surface->prog->data.glState & UINT64_C(0x38)) == 8
            ? UINT64_C(0x400)
            : 0;
    renderer.RenderDrawSurf(&parameters->drawSurfParms, surface, state);
  }
}

void RenderDepthSortedPass(const renderPassParms_t* parameters) {
  BindDefaultDestination(*parameters);
  idRenderDrawSurf renderer;
  int captureSurface = -1;
  for (int index = 0; index < *parameters->numDepthSortedDrawSurfs; ++index) {
    const drawSurf_t* surface = parameters->depthSortedDrawSurfs[index];
    if (!parameters->binaryModelState[surface->modelSurface->binaryModelId])
      continue;
    if (captureSurface < 0 &&
        (surface->depthKey & UINT64_C(0x4000000000000000)) != 0) {
      captureSurface = index;
      CaptureViewColor(&parameters->drawSurfParms.captureParms);
    }
    std::uint64_t state =
        surface->sort < parameters->settings->sortEmitOnly
            ? UINT64_C(0x400)
            : 0;
    if ((surface->prog->data.glState & UINT64_C(0x38)) == 8)
      state |= UINT64_C(0x400);
    renderer.RenderDrawSurf(&parameters->drawSurfParms, surface, state);
  }
}

void RenderDistortionPass(const renderPassParms_t* parameters) {
  const idRenderDestination* distortion = parameters->renderDestDistortion;
  GL_SetRenderDestination(idODSObject<idRenderDestination>{distortion}, 0, 0);
  const int targetWidth = distortion->targetWidth;
  const int targetHeight = distortion->targetHeight;
  renderThreadParmState->SetFragmentPositionToViewTexture(
      targetWidth, targetHeight, targetWidth, targetHeight);
  const int renderWidth = static_cast<int>(
      static_cast<float>(targetWidth) *
      (static_cast<float>(parameters->renderView->renderWidth) /
       static_cast<float>(parameters->renderView->windowWidth)));
  const int renderHeight = static_cast<int>(
      static_cast<float>(targetHeight) *
      (static_cast<float>(parameters->renderView->renderHeight) /
       static_cast<float>(parameters->renderView->windowHeight)));
  GL_Viewport(0, 0, targetWidth, targetHeight);
  GL_Scissor(0, 0, targetWidth, targetHeight);
  GL_State(UINT64_C(0x4000), false);
  GL_Clear(true, false, false, 0, 0.5f, 0.5f, 0.5f, 0.0f);
  GL_Viewport(0, 0, renderWidth, renderHeight);
  GL_Scissor(0, 0, renderWidth, renderHeight);

  {
    idRenderDrawSurf renderer;
    const renderPassSurfaces_t& range =
        parameters->renderPasses[RENDERPASS_DISTORTION];
    for (int index = range.firstSurface; index <= range.lastSurface; ++index) {
      const drawSurf_t* surface = parameters->sortedDrawSurfs[index];
      if (parameters->binaryModelState[surface->modelSurface->binaryModelId])
        renderer.RenderDrawSurf(&parameters->drawSurfParms, surface, 0);
    }
  }

  BindDefaultDestination(*parameters);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      parameters->renderView->renderWidth,
      parameters->renderView->renderHeight,
      parameters->renderView->windowWidth,
      parameters->renderView->windowHeight);
}

void RenderPass(const renderPassParms_t* parameters) {
  switch (parameters->pass) {
    case RENDERPASS_CLEAR:
      RenderClearPass(parameters);
      break;
    case RENDERPASS_SORTED:
      RenderDepthSortedPass(parameters);
      break;
    case RENDERPASS_BACKGROUND:
      RenderBackgroundPass(parameters);
      break;
    case RENDERPASS_BACKGROUND_EMISSIVE:
      RenderBackgroundEmissivePass(parameters);
      break;
    case RENDERPASS_SSS:
      RenderSSSPass(parameters);
      break;
    case RENDERPASS_EMISSIVE:
      RenderEmissivePass(parameters);
      break;
    case RENDERPASS_EMISSIVE_ONLY:
      RenderEmissiveOnlyPass(parameters);
      break;
    case RENDERPASS_BLEND:
      RenderBlendPass(parameters);
      break;
    case RENDERPASS_DISTORTION:
      RenderDistortionPass(parameters);
      break;
    default:
      break;
  }
}
