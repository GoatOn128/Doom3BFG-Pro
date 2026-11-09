#include "engine/renderer/jobs/render/renderdynamicenv.h"

#include "engine/renderer/image.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"

#include <array>

namespace {
constexpr int kCubeAxis[6][4] = {
    {-3, -2, 1, 1}, {3, -2, -1, -1}, {1, 3, 2, 2},
    {1, -3, -2, -2}, {1, -2, 3, 3}, {-1, -2, -3, -3}};

std::array<idRenderMatrix, 6> MakeCubeAxes() {
  std::array<idRenderMatrix, 6> axes{};
  for (int face = 0; face < 6; ++face) {
    for (int row = 0; row < 4; ++row) {
      const int encodedAxis = kCubeAxis[face][row];
      const int column = (encodedAxis < 0 ? -encodedAxis : encodedAxis) - 1;
      axes[face].m[row * 4 + column] = encodedAxis < 0 ? -1.0f : 1.0f;
    }
  }
  return axes;
}
} // namespace

void RenderDynamicEnv(const renderDynamicEnvParms_t* parameters) {
  envBlend_t* blendState = parameters->envBlend;
  if (blendState->cubeFinished)
    return;

  float blend = 1.0f;
  const float currentTime =
      static_cast<float>(parameters->renderView->r.renderTime) * 0.001f;
  if (blendState->skipBlending ||
      currentTime >= blendState->blendStartTime + blendState->blendDuration) {
    blendState->cubeFinished = true;
  } else {
    blend = (currentTime - blendState->blendStartTime) /
            (blendState->blendDuration * 2.5f);
  }

  const std::array<idRenderMatrix, 6> cubeAxes = MakeCubeAxes();
  GL_SetDefaultState();
  int width = parameters->imgDynamicEnv->opts.width;
  for (int mipLevel = 0; mipLevel < 5; ++mipLevel) {
    renderThreadParmState->SetParmFloat(parameters->rpEnvBlendMip,
                                        static_cast<float>(mipLevel));
    for (int side = 0; side < 6; ++side) {
      const idODSObject<idRenderDestination> destination = {
          parameters->renderDestDynamicEnv};
      GL_SetRenderDestination(destination, mipLevel, side);
      GL_Viewport(0, 0, width, width);
      GL_Scissor(0, 0, width, width);
      renderThreadParmState->SetMVPMatrix(cubeAxes[side]);
      renderThreadParmState->SetParmVec3(parameters->rpEnvSMatrix,
                                         parameters->envMapRotation[0]);
      renderThreadParmState->SetParmVec3(parameters->rpEnvTMatrix,
                                         parameters->envMapRotation[1]);
      renderThreadParmState->SetParmVec3(parameters->rpEnvRMatrix,
                                         parameters->envMapRotation[2]);
      renderThreadParmState->SetParmFloat(parameters->rpColor, blend);
      const idImage* source =
          (mipLevel == 4 && parameters->settings->UseAmbientEnv())
              ? parameters->imgAmbientEnv
              : parameters->imgUserEnv;
      renderThreadParmState->SetParmSampler(
          parameters->rpUserEnvMap,
          reinterpret_cast<const idTextureSampler*>(source));
      const idODSObject<idDeclRenderProg> program = {parameters->progBlend};
      const idODSObject<idTriangles> triangles = {parameters->unitCubeTris};
      GL_DrawElements(program, triangles, 0, false);
      // The backend resolve call folds to the retail no-op at 0x140001000.
    }
    width >>= 1;
  }
  renderThreadParmState->SetParmFloat(parameters->rpColor, 1.0f);
}
