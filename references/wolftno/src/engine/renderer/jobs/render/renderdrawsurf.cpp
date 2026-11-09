#include "engine/renderer/jobs/render/renderdrawsurf.h"

#include "engine/models/rendermodel.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/jobs/render/setupmodelmvp.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/renderlog.h"
#include "engine/renderer/rendermodelcommitted.h"

void idRenderDrawSurf::PopSurface() {
  if (currentSurface == nullptr)
    return;

  if (currentSurface->parms != nullptr) {
    renderThreadParmState->VirtualRollbackParmBlock(*currentSurface->parms,
                                                    *currentSurface->parms);
  }
  currentSurface = nullptr;
}

void idRenderDrawSurf::PopModel() {
  if (currentModel == nullptr)
    return;

  renderThreadParmState->VirtualRollbackParmBlock(currentModel->renderParmBlock,
                                                  currentModel->renderParmBlock);
  currentModel = nullptr;
}

void idParmState::SetMVPMatrixFlipped(const idRenderMatrix& matrix) {
  const unsigned int stackMask =
      onParmBlockStack[parmIndexMVPMatrixY] |
      onParmBlockStack[parmIndexMVPMatrixW] |
      onParmBlockStack[parmIndexMVPMatrixX] |
      onParmBlockStack[parmIndexMVPMatrixZ];
  if ((stackMask & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);

  idRenderMatrix::SetMVP(
      matrix, *reinterpret_cast<idVec4*>(&values[parmIndexMVPMatrixX]),
      *reinterpret_cast<idVec4*>(&values[parmIndexMVPMatrixY]),
      *reinterpret_cast<idVec4*>(&values[parmIndexMVPMatrixZ]),
      *reinterpret_cast<idVec4*>(&values[parmIndexMVPMatrixW]),
      currentMVPHasNegativeDeterminant);
  values[parmIndexMVPMatrixY].swizzle[1] ^= static_cast<int>(0x80000000u);
  values[parmIndexMVPMatrixY].swizzle[3] ^= static_cast<int>(0x80000000u);

  const unsigned char usage =
      parmUsageMask[parmIndexMVPMatrixX] |
      parmUsageMask[parmIndexMVPMatrixZ] |
      parmUsageMask[parmIndexMVPMatrixY] |
      parmUsageMask[parmIndexMVPMatrixW];
  ++renderParmVersion;
  vertexParmVersion += usage & 1;
  fragmentParmVersion += usage & 2;
  textureParmVersion += usage & 4;
  samplerParmVersion += usage & 8;
}

void RenderGui(const idRenderModelCommitted* model,
               const renderSettings_t* settings,
               const idRenderView* renderView, bool inGameGui) {
  if (settings->skipGuis)
    return;

  idRenderMatrix mvpMatrix{};
  SetupModelMVPMatrix(&mvpMatrix, &model->r, &model->modelMatrix, renderView,
                      settings->viewNearZ, settings->viewFarZ,
                      settings->guiOriginOffset);
  if (inGameGui)
    renderThreadParmState->SetMVPMatrixFlipped(mvpMatrix);
  else
    renderThreadParmState->SetMVPMatrix(mvpMatrix);

  renderThreadParmState->VirtualEvaluateParmBlock(model->renderParmBlock,
                                                  model->renderParmBlock);
  for (int surfaceIndex = 0; surfaceIndex < model->surfaces.num;
       ++surfaceIndex) {
    if (settings->singleGuiSurface != -1 &&
        settings->singleGuiSurface != surfaceIndex) {
      continue;
    }

    const idRenderModelSurface& surface = model->surfaces.list[surfaceIndex];
    const idTriangles* geometry = surface.geometry;
    const idMaterial* material = surface.material;
    if (geometry == nullptr || material == nullptr)
      continue;

    renderLog.OpenBlock("Entering material %s\n", material->name.str);
    const idDeclRenderProg* program = material->progs[7];
    std::uint64_t extraState = surface.extraGLState;
    if (settings->progShowGuiOverdraw != nullptr) {
      program = settings->progShowGuiOverdraw;
      extraState = 0;
    }

    renderThreadParmState->VirtualEvaluateParmBlock(material->renderParmBlock,
                                                    material->renderParmBlock);
    renderThreadParmState->VirtualEvaluateParmBlock(program->data.parmBlock,
                                                    program->data.parmBlock);
    GL_DrawElements(idODSObject<idDeclRenderProg>{program},
                    idODSObject<idTriangles>{geometry}, extraState, false);
    renderThreadParmState->VirtualRollbackParmBlock(program->data.parmBlock,
                                                    program->data.parmBlock);
    renderThreadParmState->VirtualRollbackParmBlock(material->renderParmBlock,
                                                    material->renderParmBlock);
  }
  renderThreadParmState->VirtualRollbackParmBlock(model->renderParmBlock,
                                                  model->renderParmBlock);

  if (settings->progShowGuiOverdraw != nullptr) {
    GL_State(UINT64_C(0x380), false);
    GL_Clear(true, false, false, 0, 0.0f, 0.0f, 0.0f, 1.0f);
  }
}

void idRenderDrawSurf::PushModel(const renderDrawSurfParms_t* parameters,
                                 const idRenderModelCommitted* model) {
  renderLog.OpenBlock("Entering model %i: %s\n", model->index.index >> 8,
                      model->name);
  currentModel = model;

  if (model->referenceModel != nullptr &&
      !parameters->settings->skipInGameGuis) {
    GL_SetRenderDestination(
        idODSObject<idRenderDestination>{parameters->renderDestGui}, 0, 0);
    GL_Viewport(0, 0, parameters->renderDestGui->targetWidth,
                parameters->renderDestGui->targetHeight);
    GL_Scissor(0, 0, parameters->renderDestGui->targetWidth,
               parameters->renderDestGui->targetHeight);
    GL_Clear(true, false, false, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    RenderGui(model->referenceModel, parameters->settings,
              parameters->renderView, true);
    CaptureGuiImage(&parameters->captureParms);

    GL_SetRenderDestination(
        idODSObject<idRenderDestination>{parameters->renderDestDefault}, 0,
        0);
    const idScreenRect& viewport = parameters->renderView->viewport;
    GL_Viewport(viewport.x1, viewport.y1, viewport.x2 - viewport.x1 + 1,
                viewport.y2 - viewport.y1 + 1);
    const idScreenRect& scissor = parameters->renderView->scissorRect;
    GL_Scissor(scissor.x1, scissor.y1, scissor.x2 - scissor.x1 + 1,
               scissor.y2 - scissor.y1 + 1);
  }

  renderThreadParmState->SetModelMatrix(model->modelMatrix);
  renderThreadParmState->SetInverseModelMatrix(model->inverseModelMatrix);
  renderThreadParmState->SetMVPMatrix(model->mvpMatrix);
  renderThreadParmState->SetApproximateLighting(
      model->approximateLighting[model->CurrentApproximateLighting()]);
  renderThreadParmState->SetHighlightColor(model->highlightColor);
  renderThreadParmState->VirtualEvaluateParmBlock(model->renderParmBlock,
                                                  model->renderParmBlock);
}

void idRenderDrawSurf::RenderDrawSurf(
    const renderDrawSurfParms_t* parameters, const drawSurf_t* surface,
    std::uint64_t extraState) {
  const idRenderModelCommitted* model = surface->model;
  if (model->referenceModel != nullptr && !parameters->allowInGameGUIs)
    return;

  if (model != currentModel) {
    PopModel();
    PushModel(parameters, model);
  }

  const idRenderModelSurface* modelSurface = surface->modelSurface;
  if (modelSurface->parms != nullptr) {
    renderThreadParmState->VirtualEvaluateParmBlock(*modelSurface->parms,
                                                    *modelSurface->parms);
  }
  currentSurface = modelSurface;

  const idMaterial* material = surface->material;
  renderLog.OpenBlock("Entering material %s\n", material->name.str);
  renderThreadParmState->VirtualEvaluateParmBlock(material->renderParmBlock,
                                                  material->renderParmBlock);
  if ((renderThreadParmState->values[renderThreadParmState->parmIndexIf]
           .swizzle[0] &
       0x7FFFFFFF) != 0) {
    const idDeclRenderProg* program = surface->prog->renamed;
    const bool polygonOffset =
        (renderThreadParmState
             ->values[renderThreadParmState->parmIndexPolygonOffset]
             .swizzle[0] &
         0x7FFFFFFF) != 0;
    if (polygonOffset) {
      GL_PolygonOffset(
          0.0f,
          renderThreadParmState
              ->values[renderThreadParmState->parmIndexPolygonOffset]
              .value[0],
          true);
    }

    renderThreadParmState->VirtualEvaluateParmBlock(program->data.parmBlock,
                                                    program->data.parmBlock);
    GL_DrawElements(idODSObject<idDeclRenderProg>{program},
                    idODSObject<idRenderModelSurface>{modelSurface},
                    extraState, surface->skipDetailTriangles);
    renderThreadParmState->VirtualRollbackParmBlock(program->data.parmBlock,
                                                    program->data.parmBlock);
    if (polygonOffset)
      GL_PolygonOffset(0.0f, 0.0f, false);
  }

  renderThreadParmState->VirtualRollbackParmBlock(material->renderParmBlock,
                                                  material->renderParmBlock);
  PopSurface();
}

void idRenderDrawSurf::RenderDrawSurfProgram(
    const renderDrawSurfParms_t* parameters, const drawSurf_t* surface,
    const idDeclRenderProg* program) {
  drawSurf_t drawSurface{};
  drawSurface.sort = surface->sort;
  drawSurface.isWorld = surface->isWorld;
  drawSurface.skipDetailTriangles = surface->skipDetailTriangles;
  drawSurface.model = surface->model;
  drawSurface.modelSurface = surface->modelSurface;
  drawSurface.material = surface->material;
  drawSurface.prog = program;
  RenderDrawSurf(parameters, &drawSurface, 0);
}
