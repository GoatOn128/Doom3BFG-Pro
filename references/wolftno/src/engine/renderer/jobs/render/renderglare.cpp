#include "engine/renderer/jobs/render/renderglare.h"

#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/renderdestination.h"

#include <limits>

namespace {
const idTextureSampler* AsSampler(const idImage* image) {
  return reinterpret_cast<const idTextureSampler*>(image);
}

void SetDestination(const idRenderDestination* destination) {
  const idODSObject<idRenderDestination> object = {destination};
  GL_SetRenderDestination(object, 0, 0);
}

void SetTargetViewport(const idRenderDestination* destination) {
  GL_Viewport(0, 0, destination->targetWidth, destination->targetHeight);
  GL_Scissor(0, 0, destination->targetWidth, destination->targetHeight);
}

void Draw(const idDeclRenderProg* program, const idTriangles* triangles) {
  const idODSObject<idDeclRenderProg> programObject = {program};
  const idODSObject<idTriangles> triangleObject = {triangles};
  GL_DrawElements(programObject, triangleObject, 0, false);
}

void ResolveNoOp(const idRenderDestination*) {
  // The OpenGL resolve call is linker-folded to 0x140001000 in retail.
}
} // namespace

void RenderGlare(const renderGlareParms_t* parameters) {
  const int targetWidth = parameters->renderDestMip3->targetWidth;
  const int targetHeight = parameters->renderDestMip3->targetHeight;
  renderThreadParmState->SetFragmentPositionToViewTexture(
      targetWidth, targetHeight, targetWidth, targetHeight);

  SetDestination(parameters->renderDestMip3Alt);
  GL_Viewport(0, 0, targetWidth, targetHeight);
  GL_Scissor(0, 0, targetWidth, targetHeight);
  renderThreadParmState->SetParmSampler(
      parameters->rpGlareMap,
      AsSampler(parameters->renderDestMip3->targetImage[0]));
  renderThreadParmState->SetParmVector(parameters->rpGlareStep,
                                       parameters->xScale, 0.0f, 0.0f, 0.0f);
  Draw(parameters->progGauss, parameters->unitSquareTris);
  ResolveNoOp(parameters->renderDestMip3Alt);

  SetDestination(parameters->renderDestMip3);
  GL_Viewport(0, 0, targetWidth, targetHeight);
  GL_Scissor(0, 0, targetWidth, targetHeight);
  renderThreadParmState->SetParmSampler(
      parameters->rpGlareMap,
      AsSampler(parameters->renderDestMip3Alt->targetImage[0]));
  renderThreadParmState->SetParmVector(parameters->rpGlareStep, 0.0f,
                                       parameters->yScale, 0.0f, 0.0f);
  Draw(parameters->progGauss, parameters->unitSquareTris);
  ResolveNoOp(parameters->renderDestMip3);

  SetDestination(parameters->renderDestDefault);
  const idScreenRect& viewport = parameters->renderView->viewport;
  GL_Viewport(viewport.x1, viewport.y1, viewport.x2 - viewport.x1 + 1,
              viewport.y2 - viewport.y1 + 1);
  const idScreenRect& scissor = parameters->renderView->scissorRect;
  GL_Scissor(scissor.x1, scissor.y1, scissor.x2 - scissor.x1 + 1,
             scissor.y2 - scissor.y1 + 1);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      parameters->renderView->renderWidth,
      parameters->renderView->renderHeight,
      parameters->renderView->windowWidth,
      parameters->renderView->windowHeight);
}

void RenderAdaptiveGlare(const renderAdapativeGlareParms_t* parameters) {
  const bool oddFrame = (parameters->frameNumber & 1) != 0;
  const idRenderDestination* luminosity =
      oddFrame ? parameters->renderDestLuminosity
               : parameters->renderDestLuminosityAlt;
  const idRenderDestination* previousLuminosity =
      oddFrame ? parameters->renderDestLuminosityAlt
               : parameters->renderDestLuminosity;
  const idRenderDestination* frameExposure =
      oddFrame ? parameters->renderDestFrameExposure
               : parameters->renderDestFrameExposureAlt;
  const idRenderDestination* previousFrameExposure =
      oddFrame ? parameters->renderDestFrameExposureAlt
               : parameters->renderDestFrameExposure;

  renderThreadParmState->SetParmVector(
      parameters->rpAdaptiveGlareParms, parameters->minThreshold,
      parameters->maxThreshold, parameters->avgBrightness,
      parameters->decayRate);
  renderThreadParmState->SetParmVector(parameters->rpAdaptiveGlareColor,
                                       &parameters->color.x);
  renderThreadParmState->SetParmVector(
      parameters->rpFrameExposureParms1,
      parameters->frameExposureSaturation, parameters->frameExposureDecay,
      parameters->frameExposureMin, parameters->frameExposureMax);
  renderThreadParmState->SetParmFloat(parameters->rpFrameExposureParms2,
                                      parameters->frameExposureAmount);

  SetDestination(luminosity);
  SetTargetViewport(luminosity);
  renderThreadParmState->SetSrcTexClamp(
      parameters->renderView->renderWidth,
      parameters->renderView->renderHeight,
      parameters->renderView->windowWidth,
      parameters->renderView->windowHeight);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      luminosity->targetWidth, luminosity->targetHeight,
      luminosity->targetWidth, luminosity->targetHeight);
  renderThreadParmState->SetParmSampler(parameters->rpViewColorMap,
                                        AsSampler(parameters->imgViewColor));
  renderThreadParmState->SetParmSampler(
      parameters->rpAdaptiveLuminance,
      AsSampler(previousLuminosity->targetImage[0]));
  Draw(parameters->progLuminosity, parameters->unitSquareTris);
  ResolveNoOp(luminosity);

  if (parameters->envBlend->frameExposureFinished < 4) {
    SetDestination(frameExposure);
    SetTargetViewport(frameExposure);
    renderThreadParmState->SetFragmentPositionToViewTexture(
        frameExposure->targetWidth, frameExposure->targetHeight,
        frameExposure->targetWidth, frameExposure->targetHeight);
    renderThreadParmState->SetParmSampler(parameters->rpViewColorMap,
                                          AsSampler(parameters->imgViewColor));
    renderThreadParmState->SetParmSampler(
        parameters->rpFrameExposureMap,
        AsSampler(previousFrameExposure->targetImage[0]));
    Draw(parameters->progFrameExposure, parameters->unitSquareTris);
    ResolveNoOp(frameExposure);
    if (parameters->frameExposureAmount <=
        std::numeric_limits<float>::epsilon())
      ++parameters->envBlend->frameExposureFinished;
  }

  const idTextureSampler* luminositySampler =
      AsSampler(luminosity->targetImage[0]);
  const idTextureSampler* frameExposureSampler =
      AsSampler(frameExposure->targetImage[0]);
  SetDestination(parameters->renderDestBloom);
  SetTargetViewport(parameters->renderDestBloom);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      parameters->renderDestBloom->targetWidth,
      parameters->renderDestBloom->targetHeight,
      parameters->renderDestBloom->targetWidth,
      parameters->renderDestBloom->targetHeight);
  renderThreadParmState->SetParmSampler(parameters->rpAdaptiveLuminance,
                                        luminositySampler);
  renderThreadParmState->SetParmSampler(parameters->rpViewColorMap,
                                        AsSampler(parameters->imgViewColor));
  renderThreadParmState->SetParmSampler(parameters->rpGlareMap,
                                        AsSampler(parameters->imgGlare));
  renderThreadParmState->SetParmSampler(parameters->rpFrameExposureMap,
                                        frameExposureSampler);
  Draw(parameters->progBloomThreshold, parameters->unitSquareTris);
  ResolveNoOp(parameters->renderDestBloom);

  SetDestination(parameters->renderDestAdaptiveGlareAlt);
  SetTargetViewport(parameters->renderDestAdaptiveGlareAlt);
  const int alternateWidth =
      parameters->renderDestAdaptiveGlareAlt->targetWidth;
  const int alternateHeight =
      parameters->renderDestAdaptiveGlareAlt->targetHeight;
  renderThreadParmState->SetSrcTexClamp(
      static_cast<int>(static_cast<float>(alternateWidth) *
                       parameters->xScale),
      static_cast<int>(static_cast<float>(alternateHeight) *
                       parameters->yScale),
      alternateWidth, alternateHeight);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      alternateWidth, alternateHeight, alternateWidth, alternateHeight);
  renderThreadParmState->SetParmSampler(
      parameters->rpAdaptiveGlareBlurMap,
      AsSampler(parameters->renderDestBloom->targetImage[0]));
  renderThreadParmState->SetParmVector(parameters->rpAdaptiveGlareBlurStep,
                                       parameters->xScale, 0.0f, 0.0f, 0.0f);
  Draw(parameters->progAdaptiveGlareGauss, parameters->unitSquareTris);
  ResolveNoOp(parameters->renderDestAdaptiveGlareAlt);

  SetDestination(parameters->renderDestAdaptiveGlare);
  SetTargetViewport(parameters->renderDestAdaptiveGlare);
  const int adaptiveWidth = parameters->renderDestAdaptiveGlare->targetWidth;
  const int adaptiveHeight = parameters->renderDestAdaptiveGlare->targetHeight;
  renderThreadParmState->SetFragmentPositionToViewTexture(
      adaptiveWidth, adaptiveHeight, adaptiveWidth, adaptiveHeight);
  renderThreadParmState->SetParmSampler(
      parameters->rpAdaptiveGlareBlurMap,
      AsSampler(parameters->renderDestAdaptiveGlareAlt->targetImage[0]));
  renderThreadParmState->SetParmVector(parameters->rpAdaptiveGlareBlurStep,
                                       0.0f, parameters->yScale, 0.0f, 0.0f);
  Draw(parameters->progAdaptiveGlareGauss, parameters->unitSquareTris);
  ResolveNoOp(parameters->renderDestAdaptiveGlare);

  SetDestination(parameters->renderDestDefault);
  const idScreenRect& viewport = parameters->renderView->viewport;
  GL_Viewport(viewport.x1, viewport.y1, viewport.x2 - viewport.x1 + 1,
              viewport.y2 - viewport.y1 + 1);
  const idScreenRect& scissor = parameters->renderView->scissorRect;
  GL_Scissor(scissor.x1, scissor.y1, scissor.x2 - scissor.x1 + 1,
             scissor.y2 - scissor.y1 + 1);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      parameters->renderView->renderWidth,
      parameters->renderView->renderHeight,
      parameters->renderView->windowWidth,
      parameters->renderView->windowHeight);
  renderThreadParmState->ResetSrcTexClamp();
}
