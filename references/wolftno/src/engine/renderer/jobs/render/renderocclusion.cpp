#include "engine/renderer/jobs/render/renderocclusion.h"

#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderlightcommitted.h"
#include "engine/renderer/rendermodelcommitted.h"

void idParmState::SetMVPMatrixForBounds(const idRenderMatrix& matrix,
                                        const idBounds& bounds) {
  const unsigned int stackMask =
      onParmBlockStack[parmIndexMVPMatrixY] |
      onParmBlockStack[parmIndexMVPMatrixW] |
      onParmBlockStack[parmIndexMVPMatrixX] |
      onParmBlockStack[parmIndexMVPMatrixZ];
  if ((stackMask & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);

  idRenderMatrix::SetMVPForBounds(
      matrix, bounds,
      *reinterpret_cast<idVec4*>(&values[parmIndexMVPMatrixX]),
      *reinterpret_cast<idVec4*>(&values[parmIndexMVPMatrixY]),
      *reinterpret_cast<idVec4*>(&values[parmIndexMVPMatrixZ]),
      *reinterpret_cast<idVec4*>(&values[parmIndexMVPMatrixW]),
      currentMVPHasNegativeDeterminant);

  const unsigned char usage =
      parmUsageMask[parmIndexMVPMatrixW] |
      parmUsageMask[parmIndexMVPMatrixY] |
      parmUsageMask[parmIndexMVPMatrixZ] |
      parmUsageMask[parmIndexMVPMatrixX];
  ++renderParmVersion;
  vertexParmVersion += usage & 1;
  fragmentParmVersion += usage & 2;
  textureParmVersion += usage & 4;
  samplerParmVersion += usage & 8;
}

void RenderOcclusion(const renderOcclusionParms_t* parameters) {
  const idODSObject<idDeclRenderProg> depthProgram = {
      parameters->progDepthOnly};
  const idODSObject<idTriangles> unitCube = {parameters->unitCubeTris};
  const idODSObject<idTriangles> zeroOneCube = {
      parameters->zeroOneCubeTris};
  const idODSObject<idRenderDestination> destination = {
      parameters->renderDestDefault};

  GL_SetRenderDestination(destination, 0, 0);
  const idScreenRect& viewport = parameters->renderView->viewport;
  GL_Viewport(viewport.x1, viewport.y1,
              viewport.x2 - viewport.x1 + 1,
              viewport.y2 - viewport.y1 + 1);
  const idScreenRect& scissor = parameters->renderView->scissorRect;
  GL_Scissor(scissor.x1, scissor.y1,
             scissor.x2 - scissor.x1 + 1,
             scissor.y2 - scissor.y1 + 1);

  const std::uint64_t drawState =
      parameters->settings->showOcclusionBoxes
          ? UINT64_C(0x8000000000000440)
          : UINT64_C(0x80000000000007C0);

  if (!parameters->settings->skipNodeGPUCulling) {
    for (int test = 0; test < parameters->numOcclusionTestNodes; ++test) {
      const int node = parameters->occlusionTestNodes[test];
      if (node == -1)
        continue;
      const int queryIndex = parameters->areaNodeOcclusionIndex[node];
      renderThreadParmState->SetMVPMatrixForBounds(
          parameters->renderView->worldSpaceMVPMatrix,
          parameters->areaNodeBounds[node]);
      GL_BeginQuery(
          &parameters->occlusionState->nodeOcclusionQuery[queryIndex]);
      GL_DrawElements(depthProgram, unitCube, drawState, false);
      GL_EndQuery();
    }
  }

  if (!parameters->settings->skipAreaGPUCulling) {
    for (int test = 0; test < parameters->numOcclusionTestWorldAreas; ++test) {
      const int area = parameters->occlusionTestWorldAreas[test];
      renderThreadParmState->SetMVPMatrixForBounds(
          parameters->renderView->worldSpaceMVPMatrix,
          parameters->worldAreaGeometryBounds[area]);
      GL_BeginQuery(
          &parameters->occlusionState->worldAreaOcclusionQuery[area]);
      GL_DrawElements(depthProgram, unitCube, drawState, false);
      GL_EndQuery();
    }
  }

  if (!parameters->settings->skipModelGPUCulling) {
    for (int test = 0; test < parameters->numOcclusionTestModels; ++test) {
      const idRenderModelCommitted* model =
          parameters->occlusionTestModels[test];
      renderThreadParmState->SetMVPMatrixForBounds(model->mvpMatrix,
                                                   model->referenceBounds);
      const unsigned int queryIndex = model->index.index >> 8;
      GL_BeginQuery(
          &parameters->occlusionState->modelOcclusionQuery[queryIndex]);
      GL_DrawElements(depthProgram, unitCube, drawState, false);
      GL_EndQuery();
    }
  }

  if (!parameters->settings->skipLightGPUCulling) {
    for (int test = 0; test < parameters->numOcclusionTestLights; ++test) {
      const idRenderLightCommitted* light =
          parameters->occlusionTestLights[test];
      renderThreadParmState->SetMVPMatrixForInverseProject(
          parameters->renderView->worldSpaceMVPMatrix,
          light->inverseBaseLightProject);
      const unsigned int queryIndex = light->index.index >> 8;
      GL_BeginQuery(
          &parameters->occlusionState->lightOcclusionQuery[queryIndex]);
      GL_DrawElements(depthProgram, zeroOneCube, drawState, false);
      GL_EndQuery();
    }
  }
}
