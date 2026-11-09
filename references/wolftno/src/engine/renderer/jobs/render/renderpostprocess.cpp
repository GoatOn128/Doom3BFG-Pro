#include "engine/renderer/jobs/render/renderpostprocess.h"

#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/jobs/render/rendercapture.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/texturesampler.h"

#include <cmath>

namespace {

bool pass1 = true;
bool pass1_mips = true;
bool pass2 = true;
bool output1 = false;

void SetViewportAndScissor(int width, int height) {
  GL_Viewport(0, 0, width, height);
  GL_Scissor(0, 0, width, height);
}

void SetViewViewport(const idRenderView& view) {
  GL_Viewport(view.viewport.x1, view.viewport.y1,
              view.viewport.x2 - view.viewport.x1 + 1,
              view.viewport.y2 - view.viewport.y1 + 1);
  GL_Scissor(view.viewport.x1, view.viewport.y1,
             view.viewport.x2 - view.viewport.x1 + 1,
             view.viewport.y2 - view.viewport.y1 + 1);
}

void DrawFullScreen(const idDeclRenderProg* program,
                    const idTriangles* triangles) {
  GL_DrawElements(idODSObject<idDeclRenderProg>{program},
                  idODSObject<idTriangles>{triangles}, 0, false);
}

}  // namespace

void RegenerateColorMips(const renderPostProcessParms_t* parameters,
                         const idRenderDestination* texture, bool maxAlpha) {
  captureParms_t capture{};
  capture.settings = parameters->settings;
  capture.renderWidth = parameters->renderView->renderWidth;
  capture.renderHeight = parameters->renderView->renderHeight;
  capture.windowWidth = parameters->renderView->windowWidth;
  capture.windowHeight = parameters->renderView->windowHeight;
  capture.renderDestDefault = parameters->renderDestDefault;
  capture.renderDestMip = parameters->renderDestMip;
  capture.progOverlappedDownSample =
      maxAlpha ? parameters->progOverlappedDownSampleMaxAlpha
               : parameters->progOverlappedDownSample;
  capture.rpViewColorMap = parameters->rpViewColorMap;
  capture.rpMipMapSampler = parameters->rpMipMapSampler;
  for (int index = 0; index < 16; ++index)
    capture.mipMapSamplers[index] =
        textureSamplerManager->mipMapSamplers[index];
  capture.unitSquareTris = parameters->unitSquareTris;
  CreateColorMips(&capture, texture);
}

void RenderPostProcess_SlowDepthBlur(
    const renderPostProcessParms_t* parameters) {
  GL_SetRenderDestination(
      idODSObject<idRenderDestination>{parameters->renderDestViewColor}, 0,
      0);
  SetViewViewport(*parameters->renderView);
  DrawFullScreen(parameters->progCalcDepthOfFieldMaxKernel,
                 parameters->unitSquareTris);
  RegenerateColorMips(parameters, parameters->renderDestViewColor, true);
  renderThreadParmState->SetParmSampler(
      parameters->rpViewColorMap,
      reinterpret_cast<const idTextureSampler*>(parameters->imgViewColor));

  GL_SetRenderDestination(idODSObject<idRenderDestination>{
                              parameters->renderDestViewPostScratch},
                          0, 0);
  SetViewViewport(*parameters->renderView);
  DrawFullScreen(parameters->progSlowDepthOfField,
                 parameters->unitSquareTris);

  GL_SetRenderDestination(
      idODSObject<idRenderDestination>{parameters->renderDestDefault}, 0, 0);
  RegenerateColorMips(parameters, parameters->renderDestViewPostScratch,
                      false);
  SetViewViewport(*parameters->renderView);
  renderThreadParmState->SetParmSampler(
      parameters->rpViewColorMap,
      reinterpret_cast<const idTextureSampler*>(
          parameters->imgViewPostScratch));
}

void RenderPostProcess_SeparatedDepthBlur(
    const renderPostProcessParms_t* parameters, bool boxBlur,
    bool sampleMipMaps) {
  const int colorWidth = parameters->renderDestViewColor->targetWidth;
  const int colorHeight = parameters->renderDestViewColor->targetHeight;
  const int scratchWidth =
      parameters->renderDestViewPostScratch->targetWidth;
  const int scratchHeight =
      parameters->renderDestViewPostScratch->targetHeight;
  const idDeclRenderProg* filterProgram = parameters->progDepthFilterPass;

  GL_SetRenderDestination(
      idODSObject<idRenderDestination>{parameters->renderDestViewColor}, 0,
      0);
  SetViewportAndScissor(colorWidth, colorHeight);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      colorWidth, colorHeight, colorWidth, colorHeight);
  DrawFullScreen(parameters->progCalcDepthOfFieldValues,
                 parameters->unitSquareTris);

  float mipSampleScale = 0.2f;
  if (pass1) {
    renderThreadParmState->SetParmSampler(
        parameters->rpViewColorMap,
        reinterpret_cast<const idTextureSampler*>(parameters->imgViewColor));
    GL_SetRenderDestination(idODSObject<idRenderDestination>{
                                parameters->renderDestViewPostScratch},
                            0, 0);
    SetViewportAndScissor(scratchWidth, scratchHeight);
    renderThreadParmState->SetFragmentPositionToViewTexture(
        scratchWidth, scratchHeight, scratchWidth, scratchHeight);
    const float filterParams[4] = {
        std::cos(0.0f), std::sin(0.0f), 0.0f,
        sampleMipMaps ? mipSampleScale : 0.0f};
    renderThreadParmState->SetParmVector(parameters->rpDepthFilterParams,
                                        filterParams);
    DrawFullScreen(filterProgram, parameters->unitSquareTris);
  }

  if (sampleMipMaps) {
    if (pass1_mips)
      RegenerateColorMips(parameters,
                          parameters->renderDestViewPostScratch, false);
    else
      sampleMipMaps = false;
  }

  if (pass2) {
    float angle;
    if (boxBlur) {
      angle = 3.1415927f * 0.5f;
    } else {
      filterProgram = parameters->progDepthFilterDoublePass;
      angle = 3.1415927f / 3.0f;
    }
    renderThreadParmState->SetParmSampler(
        parameters->rpViewColorMap,
        reinterpret_cast<const idTextureSampler*>(
            parameters->imgViewPostScratch));
    GL_SetRenderDestination(
        idODSObject<idRenderDestination>{parameters->renderDestViewColor}, 0,
        0);
    SetViewportAndScissor(colorWidth, colorHeight);
    renderThreadParmState->SetFragmentPositionToViewTexture(
        colorWidth, colorHeight, colorWidth, colorHeight);
    const float filterParams[4] = {
        std::cos(angle), std::sin(angle), 0.0f,
        sampleMipMaps ? mipSampleScale : 0.0f};
    renderThreadParmState->SetParmVector(parameters->rpDepthFilterParams,
                                        filterParams);
    DrawFullScreen(filterProgram, parameters->unitSquareTris);
  }

  RegenerateColorMips(parameters, parameters->renderDestViewColor, false);
  GL_SetRenderDestination(
      idODSObject<idRenderDestination>{parameters->renderDestDefault}, 0, 0);
  SetViewViewport(*parameters->renderView);
  renderThreadParmState->SetParmSampler(
      parameters->rpViewColorMap,
      reinterpret_cast<const idTextureSampler*>(
          output1 ? parameters->imgViewPostScratch : parameters->imgViewColor));
}

void RenderPostProcess(const renderPostProcessParms_t* parameters) {
  GL_SetDefaultState();
  GL_Viewport(parameters->renderView->viewport.x1,
              parameters->renderView->viewport.y1,
              parameters->renderView->viewport.x2 -
                      parameters->renderView->viewport.x1 +
                  1,
              parameters->renderView->viewport.y2 -
                      parameters->renderView->viewport.y1 +
                  1);
  GL_Scissor(parameters->renderView->scissorRect.x1,
             parameters->renderView->scissorRect.y1,
             parameters->renderView->scissorRect.x2 -
                     parameters->renderView->scissorRect.x1 +
                 1,
             parameters->renderView->scissorRect.y2 -
                     parameters->renderView->scissorRect.y1 +
                 1);

  renderThreadParmState->SetParmSampler(
      parameters->rpGlareMap,
      reinterpret_cast<const idTextureSampler*>(
          parameters->settings->skipEmissiveGlare ? parameters->imgBlack
                                                  : parameters->imgGlare));
  const idImage* adaptiveGlare =
      parameters->settings->skipAdaptiveGlare ? parameters->imgBlack
                                              : parameters->imgAdaptiveGlare;
  const idImage* adaptiveLuminance =
      parameters->settings->skipAdaptiveGlare
          ? parameters->imgBlack
          : parameters->imgAdaptiveGlareLuminance;
  renderThreadParmState->SetParmSampler(
      parameters->rpAdaptiveGlareMap,
      reinterpret_cast<const idTextureSampler*>(adaptiveGlare));
  renderThreadParmState->SetParmSampler(
      parameters->rpAdaptiveGlareLuminance,
      reinterpret_cast<const idTextureSampler*>(adaptiveLuminance));

  const float globalBlur =
      renderThreadParmState->GetParmFloat(parameters->rpGlobalBlur);
  renderThreadParmState->SetParmVector(
      parameters->rpDofOffset,
      static_cast<float>(static_cast<double>(parameters->renderView->renderWidth) /
                         1920.0),
      globalBlur, 0.0f, 0.0f);

  switch (parameters->settings->postProcessDofMode) {
    case 0:
      renderThreadParmState->SetParmFloat(parameters->rpDofMipScale, 1.0f);
      break;
    case 1:
      renderThreadParmState->SetParmFloat(parameters->rpDofMipScale, 0.0f);
      if (parameters->progSlowDepthOfField != nullptr)
        RenderPostProcess_SlowDepthBlur(parameters);
      break;
    case 2:
      renderThreadParmState->SetParmFloat(parameters->rpDofMipScale, 0.0f);
      RenderPostProcess_SeparatedDepthBlur(parameters, true, false);
      break;
    case 3:
      renderThreadParmState->SetParmFloat(parameters->rpDofMipScale, 0.0f);
      RenderPostProcess_SeparatedDepthBlur(parameters, true, true);
      break;
    case 4:
      renderThreadParmState->SetParmFloat(parameters->rpDofMipScale, 0.0f);
      RenderPostProcess_SeparatedDepthBlur(parameters, false, false);
      break;
    case 5:
      renderThreadParmState->SetParmFloat(parameters->rpDofMipScale, 0.0f);
      RenderPostProcess_SeparatedDepthBlur(parameters, false, true);
      break;
    default:
      renderThreadParmState->SetParmFloat(parameters->rpDofMipScale, 0.0f);
      break;
  }

  renderThreadParmState->SetParmSampler(
      parameters->rpDistortionMap,
      reinterpret_cast<const idTextureSampler*>(parameters->imgDistortion));
  const idDeclRenderProg* program = parameters->progPostProcess->renamed;
  renderThreadParmState->VirtualEvaluateParmBlock(program->data.parmBlock,
                                                  program->data.parmBlock);
  DrawFullScreen(program, parameters->unitSquareTris);
  renderThreadParmState->VirtualRollbackParmBlock(program->data.parmBlock,
                                                  program->data.parmBlock);
}
