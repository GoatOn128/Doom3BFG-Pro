#include "engine/renderer/jobs/render/renderdynamiccolorcorrection.h"

#include "engine/renderer/image.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"

namespace {
constexpr idRenderMatrix kIdentity =
    {{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}};
} // namespace

void RenderDynamicColorCorrection(
    const renderDynamicColorCorrectionParms_t* parameters) {
  envBlend_t* blendState = parameters->envBlend;
  if (blendState->colorCorrectionFinished && !parameters->forceBlend)
    return;

  float blend = 1.0f;
  const float currentTime =
      static_cast<float>(parameters->renderView->r.renderTime) * 0.001f;
  if (blendState->skipBlending ||
      currentTime >= blendState->blendStartTime + blendState->blendDuration) {
    blendState->colorCorrectionFinished = true;
  } else {
    blend = (currentTime - blendState->blendStartTime) /
            (blendState->blendDuration * 2.5f);
  }

  GL_SetDefaultState();
  const int width = parameters->imgDynamicColorCorrection->opts.width;
  const int height = parameters->imgDynamicColorCorrection->opts.height;
  const idODSObject<idRenderDestination> destination = {
      parameters->renderDestDynamicColorCorrection};
  GL_SetRenderDestination(destination, 0, 0);
  GL_Viewport(0, 0, width, height);
  GL_Scissor(0, 0, width, height);
  renderThreadParmState->SetMVPMatrix(kIdentity);
  renderThreadParmState->SetParmFloat(parameters->rpColor, blend);
  renderThreadParmState->SetParmSampler(
      parameters->rpUserColorCorrection,
      reinterpret_cast<const idTextureSampler*>(
          parameters->imgUserColorCorrection));
  const idODSObject<idDeclRenderProg> program = {parameters->progBlend};
  const idODSObject<idTriangles> triangles = {parameters->unitSquareTris};
  GL_DrawElements(program, triangles, 0, false);

  // The OpenGL retail target following the draw is linker-folded to the
  // no-op at 0x140001000; no resolve operation is required on this backend.
  renderThreadParmState->SetParmFloat(parameters->rpColor, 1.0f);
}
