#include "engine/renderer/jobs/render/rendergodrays.h"

#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/parmblock.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/rendermodelcommitted.h"

#include <cstddef>

namespace {
const idParmBlock& ProgramParmBlock(const idDeclRenderProg* program) {
  return *reinterpret_cast<const idParmBlock*>(
      reinterpret_cast<const std::byte*>(program) + 0xC0);
}
} // namespace

void RenderGodRays(const renderGodRaysParms_t* parameters) {
  if (parameters->numViewGodrayModels == 0)
    return;

  GL_SetDefaultState();
  const idODSObject<idRenderDestination> offScreenDestination = {
      parameters->renderDestOffScreen};
  GL_SetRenderDestination(offScreenDestination, 0, 0);
  GL_Viewport(0, 0, parameters->renderDestOffScreen->targetWidth,
              parameters->renderDestOffScreen->targetHeight);
  GL_Scissor(0, 0, parameters->renderDestOffScreen->targetWidth,
             parameters->renderDestOffScreen->targetHeight);
  GL_Clear(true, false, false, 0, 0.0f, 0.0f, 0.0f, 0.0f);
  renderThreadParmState->SetSrcTexClamp(
      parameters->renderView->renderWidth,
      parameters->renderView->renderHeight,
      parameters->renderView->windowWidth,
      parameters->renderView->windowHeight);

  const idParmBlock& createProgramBlock =
      ProgramParmBlock(parameters->progCreateGodRays);
  renderThreadParmState->VirtualEvaluateParmBlock(createProgramBlock,
                                                   createProgramBlock);
  for (int modelIndex = 0; modelIndex < parameters->numViewGodrayModels;
       ++modelIndex) {
    const idParmBlock& modelBlock =
        parameters->viewGodrayModels[modelIndex]->renderParmBlock;
    renderThreadParmState->VirtualEvaluateParmBlock(modelBlock, modelBlock);
    const idODSObject<idDeclRenderProg> program = {
        parameters->progCreateGodRays};
    const idODSObject<idTriangles> triangles = {parameters->unitSquareTris};
    GL_DrawElements(program, triangles, 0, false);
    renderThreadParmState->VirtualRollbackParmBlock(modelBlock, modelBlock);
  }
  renderThreadParmState->VirtualRollbackParmBlock(createProgramBlock,
                                                   createProgramBlock);

  // The OpenGL resolve is linker-folded to the retail no-op at 0x140001000.
  const idODSObject<idRenderDestination> defaultDestination = {
      parameters->renderDestDefault};
  GL_SetRenderDestination(defaultDestination, 0, 0);
  const idScreenRect& viewport = parameters->renderView->viewport;
  GL_Viewport(viewport.x1, viewport.y1, viewport.x2 - viewport.x1 + 1,
              viewport.y2 - viewport.y1 + 1);
  const idScreenRect& scissor = parameters->renderView->scissorRect;
  GL_Scissor(scissor.x1, scissor.y1, scissor.x2 - scissor.x1 + 1,
             scissor.y2 - scissor.y1 + 1);
  renderThreadParmState->SetParmSampler(
      parameters->rpGodRayMap,
      reinterpret_cast<const idTextureSampler*>(parameters->imgOffScreen));

  const idParmBlock& drawProgramBlock =
      ProgramParmBlock(parameters->progDrawGodRays);
  renderThreadParmState->VirtualEvaluateParmBlock(drawProgramBlock,
                                                   drawProgramBlock);
  const idODSObject<idDeclRenderProg> drawProgram = {
      parameters->progDrawGodRays};
  const idODSObject<idTriangles> triangles = {parameters->unitSquareTris};
  GL_DrawElements(drawProgram, triangles, 0, false);
  renderThreadParmState->VirtualRollbackParmBlock(drawProgramBlock,
                                                   drawProgramBlock);
}

