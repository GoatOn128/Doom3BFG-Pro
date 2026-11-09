#include "engine/renderer/jobs/render/renderhazeflare.h"

#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/parmblock.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/texturesampler.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {
const idParmBlock& ProgramParmBlock(const idDeclRenderProg* program) {
  return *reinterpret_cast<const idParmBlock*>(
      reinterpret_cast<const std::byte*>(program) + 0xC0);
}

const idTextureSampler* MipSamplerForScale(int scale) {
  const float scaleAsFloat = static_cast<float>(scale);
  std::uint32_t bits = 0;
  std::memcpy(&bits, &scaleAsFloat, sizeof(bits));
  const int index = static_cast<int>((bits >> 23) & 0xFFu) - 127;
  return textureSamplerManager->mipMapSamplers[index];
}

void DrawFullscreen(const idDeclRenderProg* program,
                    const idTriangles* triangles) {
  const idParmBlock& block = ProgramParmBlock(program);
  renderThreadParmState->VirtualEvaluateParmBlock(block, block);
  const idODSObject<idDeclRenderProg> programObject = {program};
  const idODSObject<idTriangles> triangleObject = {triangles};
  GL_DrawElements(programObject, triangleObject, 0, false);
  renderThreadParmState->VirtualRollbackParmBlock(block, block);
}
} // namespace

void RenderHazeFlare(const renderHazeFlareParms_t* parameters) {
  const int targetWidth = parameters->renderDestHazeFlare[0]->targetWidth;
  const int targetHeight = parameters->renderDestHazeFlare[0]->targetHeight;
  const int renderWidth = parameters->renderView->renderWidth;
  const int renderHeight = parameters->renderView->renderHeight;
  const int windowWidth = parameters->renderView->windowWidth;
  const int windowHeight = parameters->renderView->windowHeight;
  if (targetWidth == 0)
    return;

  GL_SetDefaultState();
  const idODSObject<idRenderDestination> firstDestination = {
      parameters->renderDestHazeFlare[0]};
  GL_SetRenderDestination(firstDestination, 0, 0);
  GL_Viewport(0, 0, targetWidth, targetHeight);
  GL_Scissor(0, 0, targetWidth, targetHeight);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      targetWidth, targetHeight, targetWidth, targetHeight);
  renderThreadParmState->SetSrcTexClamp(renderWidth, renderHeight, windowWidth,
                                        windowHeight);
  renderThreadParmState->SetParmSampler(
      parameters->rpHazeFlareMap,
      reinterpret_cast<const idTextureSampler*>(parameters->imgViewColor));
  renderThreadParmState->SetParmSampler(
      parameters->rpGlareMap,
      reinterpret_cast<const idTextureSampler*>(parameters->imgGlare));
  renderThreadParmState->SetParmSampler(
      parameters->rpMipMapSampler,
      MipSamplerForScale(renderWidth / targetWidth));
  DrawFullscreen(parameters->progHazeFlareBlurPass1,
                 parameters->unitSquareTris);
  // OpenGL resolve folds to the retail no-op at 0x140001000.
  renderThreadParmState->ResetSrcTexClamp();

  const idODSObject<idRenderDestination> secondDestination = {
      parameters->renderDestHazeFlare[1]};
  GL_SetRenderDestination(secondDestination, 0, 0);
  GL_Viewport(0, 0, targetWidth, targetHeight);
  GL_Scissor(0, 0, targetWidth, targetHeight);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      targetWidth, targetHeight, targetWidth, targetHeight);
  renderThreadParmState->SetSrcTexClamp(renderWidth, renderHeight, windowWidth,
                                        windowHeight);
  renderThreadParmState->SetParmSampler(
      parameters->rpHazeFlareMap,
      reinterpret_cast<const idTextureSampler*>(parameters->imgHazeFlare[0]));
  renderThreadParmState->SetParmSampler(
      parameters->rpMipMapSampler, textureSamplerManager->mipMapSamplers[0]);
  DrawFullscreen(parameters->progHazeFlareBlurPass2,
                 parameters->unitSquareTris);
  // OpenGL resolve folds to the retail no-op at 0x140001000.
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
      parameters->rpHazeFlareMap,
      reinterpret_cast<const idTextureSampler*>(parameters->imgHazeFlare[1]));
  DrawFullscreen(parameters->progHazeFlare, parameters->unitSquareTris);
  renderThreadParmState->ResetSrcTexClamp();
}

