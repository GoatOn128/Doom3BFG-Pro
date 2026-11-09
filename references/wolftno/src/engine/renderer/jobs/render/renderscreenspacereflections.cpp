#include "engine/renderer/jobs/render/renderscreenspacereflections.h"

#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/parmblock.h"
#include "engine/renderer/renderdestination.h"

#include <cstddef>

namespace {
constexpr std::uint64_t kFullscreenState = 0x5FF0100000ull;

const idParmBlock& ProgramParmBlock(const idDeclRenderProg* program) {
  return *reinterpret_cast<const idParmBlock*>(
      reinterpret_cast<const std::byte*>(program) + 0xC0);
}

void DrawFullscreen(const idDeclRenderProg* program,
                    const idTriangles* triangles) {
  const idParmBlock& block = ProgramParmBlock(program);
  renderThreadParmState->VirtualEvaluateParmBlock(block, block);
  const idODSObject<idDeclRenderProg> programObject = {program};
  const idODSObject<idTriangles> triangleObject = {triangles};
  GL_DrawElements(programObject, triangleObject, kFullscreenState, false);
  renderThreadParmState->VirtualRollbackParmBlock(block, block);
}
} // namespace

void RenderScreenSpaceReflections(
    const renderScreenSpaceReflectionsParms_t* parameters, int quality) {
  const int targetWidth = parameters->renderDestReflections->targetWidth;
  const int targetHeight = parameters->renderDestReflections->targetHeight;
  const int renderWidth = parameters->renderView->renderWidth;
  const int renderHeight = parameters->renderView->renderHeight;
  const int windowWidth = parameters->renderView->windowWidth;
  const int windowHeight = parameters->renderView->windowHeight;
  if (targetWidth == 0)
    return;

  GL_SetDefaultState();
  const idODSObject<idRenderDestination> reflectionsDestination = {
      parameters->renderDestReflections};
  GL_SetRenderDestination(reflectionsDestination, 0, 0);
  GL_Viewport(0, 0, targetWidth, targetHeight);
  GL_Scissor(0, 0, targetWidth, targetHeight);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      targetWidth, targetHeight, targetWidth, targetHeight);
  renderThreadParmState->SetSrcTexClamp(renderWidth, renderHeight, windowWidth,
                                        windowHeight);
  renderThreadParmState->SetParmSampler(
      parameters->rpViewColorMap,
      reinterpret_cast<const idTextureSampler*>(parameters->imgViewColor));

  const float projectionParameters[4] = {
      static_cast<float>(targetWidth) / static_cast<float>(targetHeight), 1.0f,
      static_cast<float>(targetWidth), static_cast<float>(targetHeight)};
  renderThreadParmState->SetParmVector(parameters->rpSSRParm0,
                                       projectionParameters);

  const float qualityParameters[4] = {
      quality <= 1 ? 8.0f : 12.0f, quality <= 1 ? 128.0f : 192.0f, 0.0f,
      0.0f};
  renderThreadParmState->SetParmVector(parameters->rpSSRParm1,
                                       qualityParameters);
  DrawFullscreen(parameters->progScreenSpaceReflections,
                 parameters->unitSquareTris);

  // The OpenGL resolve call is linker-folded to the retail no-op at
  // 0x140001000.
  renderThreadParmState->ResetSrcTexClamp();

  const idODSObject<idRenderDestination> defaultDestination = {
      parameters->renderDestDefault};
  GL_SetRenderDestination(defaultDestination, 0, 0);
  const idScreenRect& viewport = parameters->renderView->viewport;
  GL_Viewport(viewport.x1, viewport.y1, viewport.x2 - viewport.x1 + 1,
              viewport.y2 - viewport.y1 + 1);
  GL_Scissor(viewport.x1, viewport.y1, viewport.x2 - viewport.x1 + 1,
             viewport.y2 - viewport.y1 + 1);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      renderWidth, renderHeight, windowWidth, windowHeight);
  renderThreadParmState->SetSrcTexClamp(renderWidth, renderHeight, windowWidth,
                                        windowHeight);
  renderThreadParmState->SetParmSampler(
      parameters->rpSSRMap,
      reinterpret_cast<const idTextureSampler*>(
          parameters->renderDestReflections->targetImage[0]));
  DrawFullscreen(parameters->progScreenSpaceReflectionsApply,
                 parameters->unitSquareTris);
  renderThreadParmState->ResetSrcTexClamp();
}
