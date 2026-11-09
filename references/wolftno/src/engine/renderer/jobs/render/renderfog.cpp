#include "engine/renderer/jobs/render/renderfog.h"

#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/parmblock.h"

#include <cstddef>

void RenderFog(const renderFogParms_t* parameters) {
  GL_SetDefaultState();
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

  const auto* programBytes =
      reinterpret_cast<const std::byte*>(parameters->progGlobalFog);
  const idDeclRenderProg* renamed =
      *reinterpret_cast<const idDeclRenderProg* const*>(programBytes + 0x50);
  const auto* parmBlock = reinterpret_cast<const idParmBlock*>(
      reinterpret_cast<const std::byte*>(renamed) + 0xC0);
  renderThreadParmState->VirtualEvaluateParmBlock(*parmBlock, *parmBlock);
  const idODSObject<idDeclRenderProg> program = {renamed};
  const idODSObject<idTriangles> triangles = {parameters->unitSquareTris};
  GL_DrawElements(program, triangles, 0, false);
  renderThreadParmState->VirtualRollbackParmBlock(*parmBlock, *parmBlock);
}

