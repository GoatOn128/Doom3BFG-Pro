#include "engine/renderer/jobs/render/renderdepth.h"

#include "engine/models/rendermodel.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/image.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/jobs/render/renderocclusion.h"
#include "engine/renderer/render.h"
#include "engine/renderer/rendermodelcommitted.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/math/plane.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

void RenderEventRecorderLightStartEvent_Impl(const char* name);
void RenderEventRecorderLightStopEvent_Impl();

namespace {
constexpr int kMaximumDrawSurfaces = 4096;
constexpr float kSortIncrement = 0.000099999997f;

const idScreenRect& ActiveScissor(const idRenderView& view) {
  if (view.testScissor.x1 <= view.testScissor.x2 &&
      view.testScissor.y1 <= view.testScissor.y2) {
    return view.testScissor;
  }
  return view.scissorRect;
}

void SetViewportAndScissor(const idRenderView& view) {
  GL_Viewport(view.viewport.x1, view.viewport.y1,
              view.viewport.x2 - view.viewport.x1 + 1,
              view.viewport.y2 - view.viewport.y1 + 1);
  const idScreenRect& scissor = ActiveScissor(view);
  GL_Scissor(scissor.x1, scissor.y1, scissor.x2 - scissor.x1 + 1,
             scissor.y2 - scissor.y1 + 1);
}

bool CullSurface(const renderDepthParms_t* parameters,
                 const idRenderModelCommitted* model,
                 const idRenderModelSurface* surface, bool cullSurface) {
  if (!cullSurface || (model->r.flags1 & 0x20u) != 0 ||
      surface->joints != nullptr ||
      parameters->settings->skipSurfaceCPUCulling) {
    return false;
  }
  std::uint8_t cullBits = 0;
  return idRenderMatrix::CullBoundsToMVPbits(
      model->mvpMatrix, surface->geometry->bounds, cullBits, false);
}

bool TestModelOcclusion(const idRenderModelCommitted* model,
                        bool requested) {
  return requested && (model->r.flags2 & 0x20u) == 0 &&
         ((model->r.flags1 & 0x20u) == 0 ||
          (model->r.flags2 & 0x40u) != 0);
}

std::uint64_t DepthDrawState(const renderDepthParms_t* parameters,
                             const idRenderModelCommitted* model,
                             const idMaterial* material,
                             const idDeclRenderProg* program,
                             bool skipDepthViewStencil) {
  std::uint64_t state = 0;
  if ((material->flags0 & 0x02u) != 0 ||
      parameters->settings->forceTwoSidedDepth) {
    state = UINT64_C(0x8000);
  }
  if ((model->r.flags0 & 0x10u) != 0 ||
      (material->flags1 & 0x20u) != 0) {
    state |= UINT64_C(0x40);
  }
  if (!skipDepthViewStencil) {
    if (model->r.modelDepthSort == MODEL_DEPTH_SORT_VIEW_BODY) {
      state |= UINT64_C(0x606FF0000000);
    } else if (model->r.modelDepthSort == MODEL_DEPTH_SORT_VIEW_HEAD &&
               ((state | program->data.glState) & UINT64_C(0x40)) == 0) {
      state |= UINT64_C(0x200FF0100000);
    }
  }
  return state;
}

void EvaluateDrawBlocks(const idRenderModelSurface* surface,
                        const idMaterial* material,
                        const idDeclRenderProg* program) {
  if (surface->parms != nullptr) {
    renderThreadParmState->VirtualEvaluateParmBlock(*surface->parms,
                                                    *surface->parms);
  }
  renderThreadParmState->VirtualEvaluateParmBlock(material->renderParmBlock,
                                                  material->renderParmBlock);
  renderThreadParmState->VirtualEvaluateParmBlock(program->data.parmBlock,
                                                  program->data.parmBlock);
}

void RollbackDrawBlocks(const idRenderModelSurface* surface,
                        const idMaterial* material,
                        const idDeclRenderProg* program) {
  renderThreadParmState->VirtualRollbackParmBlock(program->data.parmBlock,
                                                  program->data.parmBlock);
  renderThreadParmState->VirtualRollbackParmBlock(material->renderParmBlock,
                                                  material->renderParmBlock);
  if (surface->parms != nullptr) {
    renderThreadParmState->VirtualRollbackParmBlock(*surface->parms,
                                                    *surface->parms);
  }
}

drawSurf_t* AllocateDrawSurface(const renderDepthParms_t* parameters) {
  if (*parameters->numDrawSurfs == kMaximumDrawSurfaces)
    return nullptr;
  return &parameters->drawSurfs[(*parameters->numDrawSurfs)++];
}

void FillDrawSurface(drawSurf_t& output,
                     const idRenderModelCommitted* model,
                     const idRenderModelSurface* surface,
                     const idMaterial* material,
                     const idDeclRenderProg* program,
                     bool skipDetailTriangles, float sort) {
  output.sort = sort;
  output.isWorld = (model->r.flags0 & 0x08u) != 0;
  output.skipDetailTriangles = skipDetailTriangles;
  output.model = model;
  output.modelSurface = surface;
  output.material = material;
  output.prog = program;
}

void DrawSurfaceProgram(const idRenderModelSurface* surface,
                        const idMaterial* material,
                        const idDeclRenderProg* program,
                        std::uint64_t state, bool skipDetailTriangles) {
  const idODSObject<idDeclRenderProg> programObject{program};
  const idODSObject<idRenderModelSurface> surfaceObject{surface};
  EvaluateDrawBlocks(surface, material, program);
  GL_DrawElements(programObject, surfaceObject, state, skipDetailTriangles);
  RollbackDrawBlocks(surface, material, program);
}

}  // namespace

void idRenderMatrix::InverseTransformPlane(const idPlane& input,
                                           idPlane& output,
                                           bool normalize) const {
  float a = input.a * m[0] + input.b * m[4] + input.c * m[8] +
            input.d * m[12];
  float b = input.a * m[1] + input.b * m[5] + input.c * m[9] +
            input.d * m[13];
  float c = input.a * m[2] + input.b * m[6] + input.c * m[10] +
            input.d * m[14];
  float d = input.a * m[3] + input.b * m[7] + input.c * m[11] +
            input.d * m[15];
  if (normalize) {
    const float inverseLength =
        1.0f / std::sqrt(std::max(a * a + b * b + c * c, FLT_MIN));
    a *= inverseLength;
    b *= inverseLength;
    c *= inverseLength;
    d *= inverseLength;
  }
  output = {a, b, c, d};
}

void idParmState::SetModelMatrix(const idRenderMatrix& matrix) {
  const unsigned int stackMask =
      onParmBlockStack[parmIndexModelMatrixY] |
      onParmBlockStack[parmIndexModelMatrixW] |
      onParmBlockStack[parmIndexModelMatrixX] |
      onParmBlockStack[parmIndexModelMatrixZ];
  if ((stackMask & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);
  idRenderMatrix::CopyMatrix(
      matrix, *reinterpret_cast<idVec4*>(&values[parmIndexModelMatrixX]),
      *reinterpret_cast<idVec4*>(&values[parmIndexModelMatrixY]),
      *reinterpret_cast<idVec4*>(&values[parmIndexModelMatrixZ]),
      *reinterpret_cast<idVec4*>(&values[parmIndexModelMatrixW]));
  const unsigned char usage =
      parmUsageMask[parmIndexModelMatrixX] |
      parmUsageMask[parmIndexModelMatrixZ] |
      parmUsageMask[parmIndexModelMatrixY] |
      parmUsageMask[parmIndexModelMatrixW];
  ++renderParmVersion;
  vertexParmVersion += usage & 1;
  fragmentParmVersion += usage & 2;
  textureParmVersion += usage & 4;
  samplerParmVersion += usage & 8;
}

void idParmState::SetInverseModelMatrix(const idRenderMatrix& matrix) {
  const unsigned int stackMask =
      onParmBlockStack[parmIndexInverseModelMatrixY] |
      onParmBlockStack[parmIndexInverseModelMatrixW] |
      onParmBlockStack[parmIndexInverseModelMatrixX] |
      onParmBlockStack[parmIndexInverseModelMatrixZ];
  if ((stackMask & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);
  idRenderMatrix::CopyMatrix(
      matrix,
      *reinterpret_cast<idVec4*>(&values[parmIndexInverseModelMatrixX]),
      *reinterpret_cast<idVec4*>(&values[parmIndexInverseModelMatrixY]),
      *reinterpret_cast<idVec4*>(&values[parmIndexInverseModelMatrixZ]),
      *reinterpret_cast<idVec4*>(&values[parmIndexInverseModelMatrixW]));
  const unsigned char usage =
      parmUsageMask[parmIndexInverseModelMatrixX] |
      parmUsageMask[parmIndexInverseModelMatrixZ] |
      parmUsageMask[parmIndexInverseModelMatrixY] |
      parmUsageMask[parmIndexInverseModelMatrixW];
  ++renderParmVersion;
  vertexParmVersion += usage & 1;
  fragmentParmVersion += usage & 2;
  textureParmVersion += usage & 4;
  samplerParmVersion += usage & 8;
}

void idParmState::SetMVPMatrix(const idRenderMatrix& matrix) {
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

std::uint64_t GetSurfaceSortKey(const renderDepthParms_t* parameters,
                                drawSurf_t* drawSurface, float* depthKeyMin,
                                std::uint16_t* surfaceSequence) {
  const renderSettings_t& settings = *parameters->settings;
  const float sort = drawSurface->sort;
  if (!((sort >= settings.sortEmit && sort < settings.sortEmitOnly) ||
        (sort >= settings.sortLight && sort < settings.sortPerturber))) {
    return 0;
  }
  const idTriangles* geometry = drawSurface->modelSurface->geometry;
  if (geometry == nullptr || geometry->bounds.b[0].x > geometry->bounds.b[1].x)
    return 0;

  const idRenderModelCommitted* model = drawSurface->model;
  const idPlane globalSortPlane{0.0f, 0.0f, 1.0f,
                                -parameters->splitPlaneDistance};
  idPlane localSortPlane{};
  model->modelMatrix.InverseTransformPlane(globalSortPlane, localSortPlane,
                                            true);
  const bool viewerNegative =
      parameters->renderView->inverseViewMatrix.m[11] -
          parameters->splitPlaneDistance <
      0.0f;

  std::uint32_t planeCategory = 2;
  if ((model->r.flags4 & 0x40u) != 0) {
    planeCategory = 1;
  } else {
    const idVec3 center{
        (geometry->bounds.b[0].x + geometry->bounds.b[1].x) * 0.5f,
        (geometry->bounds.b[0].y + geometry->bounds.b[1].y) * 0.5f,
        (geometry->bounds.b[0].z + geometry->bounds.b[1].z) * 0.5f};
    const bool centerNegative =
        localSortPlane.a * center.x + localSortPlane.b * center.y +
            localSortPlane.c * center.z + localSortPlane.d <
        0.0f;
    if (centerNegative == viewerNegative)
      planeCategory = 0;
  }

  float depthSort = 0.0f;
  float depthMaximum = 0.0f;
  idRenderMatrix::DepthBoundsForBounds(depthSort, depthMaximum,
                                       model->mvpMatrix, geometry->bounds,
                                       true);
  depthSort *= 262144.0f;
  std::uint32_t previousDepthBits = 0;
  std::uint32_t depthSortBits = 0;
  std::memcpy(&previousDepthBits, depthKeyMin, sizeof(previousDepthBits));
  std::memcpy(&depthSortBits, &depthSort, sizeof(depthSortBits));
  const std::uint32_t selectedDepthBits =
      std::min(previousDepthBits, depthSortBits);
  std::memcpy(depthKeyMin, &selectedDepthBits, sizeof(selectedDepthBits));

  *surfaceSequence = std::min<std::uint16_t>(*surfaceSequence, 0x3FFFu);
  const std::uint32_t modelIndex = model->index.index >> 8;
  const std::uint64_t result =
      *surfaceSequence |
      ((((static_cast<std::uint64_t>(selectedDepthBits) |
          (static_cast<std::uint64_t>(planeCategory) << 32))
         << 16) |
        modelIndex)
       << 14);
  const std::uint32_t nextDepthBits =
      std::max<std::uint32_t>(selectedDepthBits, 1u) - 1u;
  std::memcpy(depthKeyMin, &nextDepthBits, sizeof(nextDepthBits));
  *surfaceSequence =
      static_cast<std::uint16_t>(std::max<std::uint16_t>(*surfaceSequence, 1u) -
                                 1u);
  return result;
}

void DrawDepthSurface(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* modelObject,
    const idODSObject<idRenderModelSurface>* surfaceObject, bool cullSurface,
    bool skipDetailTriangles, bool skipDepthViewStencil,
    bool skipPerforatedCullTest) {
  if (parameters->settings->skipOpaqueSurfaces)
    return;
  const idRenderModelSurface* surface = surfaceObject->objectPtr;
  const idMaterial* material = surface->material;
  if (material == nullptr || surface->geometry == nullptr)
    return;
  const materialCoverage_t coverage = material->coverage;
  if (coverage < MC_PERFORATED || coverage == MC_TRANSLUCENT ||
      (!skipPerforatedCullTest &&
       (coverage == MC_PERFORATED ||
        (coverage == MC_PERFORATED_FOR_MODELFADE &&
         modelObject->objectPtr->r.modelFade < 1.0f)))) {
    return;
  }
  if (CullSurface(parameters, modelObject->objectPtr, surface, cullSurface))
    return;

  const idDeclRenderProg* selectedProgram = nullptr;
  if ((modelObject->objectPtr->r.flags2 & 0x04u) != 0 &&
      (material->flags1 & 0x04u) != 0) {
    selectedProgram = parameters->progPreZWound;
  } else if (surface->stMap != nullptr) {
    selectedProgram = parameters->progPreZSecondary;
  } else if ((coverage == MC_PERFORATED_FOR_MODELFADE &&
              modelObject->objectPtr->r.modelFade != 1.0f) ||
             coverage == MC_PERFORATED) {
    selectedProgram = parameters->progPreZAlpha;
  } else {
    selectedProgram = parameters->progPreZ;
  }
  if (selectedProgram == nullptr)
    return;
  const idDeclRenderProg* program = selectedProgram->renamed;
  const std::uint64_t state =
      DepthDrawState(parameters, modelObject->objectPtr, material, program,
                     skipDepthViewStencil);
  DrawSurfaceProgram(surface, material, program, state, skipDetailTriangles);
}

const idDeclRenderProg* BuildDrawSurf(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* modelObject,
    const idODSObject<idRenderModelSurface>* surfaceObject, bool cullSurface,
    bool testOcclusion, bool skipDetailTriangles, float* sortOffset,
    float* depthKeyMin, std::uint16_t* surfaceSequence,
    bool skipSubSurfaceScattering, bool skipBuildDrawSurf) {
  const idRenderModelSurface* surface = surfaceObject->objectPtr;
  const idMaterial* material = surface->material;
  if (material == nullptr || surface->geometry == nullptr ||
      material->coverage == MC_INVISIBLE ||
      CullSurface(parameters, modelObject->objectPtr, surface, cullSurface)) {
    return nullptr;
  }

  if (!skipBuildDrawSurf) {
    const auto addPass = [&](int pass, bool depthSorted) {
      const idDeclRenderProg* program = material->progs[pass];
      if (program == nullptr)
        return;
      drawSurf_t* output = AllocateDrawSurface(parameters);
      if (output == nullptr)
        return;
      FillDrawSurface(*output, modelObject->objectPtr, surface, material,
                      program, skipDetailTriangles,
                      material->stageSort[pass] + material->stageSortAdjust +
                          *sortOffset);
      if (depthSorted) {
        output->depthKey = GetSurfaceSortKey(parameters, output, depthKeyMin,
                                             surfaceSequence);
      }
      *sortOffset += kSortIncrement;
    };

    if (!skipSubSurfaceScattering)
      addPass(3, false);
    addPass(5, true);
    addPass(6, true);
    const float passSevenSort =
        material->stageSort[7] + material->stageSortAdjust;
    if (!(passSevenSort >= parameters->settings->sortCoverage &&
          passSevenSort < parameters->settings->sortBackgroundFar)) {
      addPass(7, true);
    }
  }

  if ((material->flags0 & 0x04u) != 0 &&
      (((modelObject->objectPtr->r.flags0 & 0x08u) != 0) ||
       ((modelObject->objectPtr->r.flags1 & 0x10u) != 0)) &&
      parameters->settings->IsComboMap()) {
    return nullptr;
  }
  if (material->coverage == MC_TRANSLUCENT && !testOcclusion)
    return nullptr;
  const idDeclRenderProg* opaqueProgram = material->progs[0];
  if (opaqueProgram == nullptr)
    return nullptr;

  if (!skipBuildDrawSurf && material->progs[8] != nullptr &&
      modelObject->objectPtr->r.modelFade != 1.0f) {
    drawSurf_t* output = AllocateDrawSurface(parameters);
    if (output != nullptr) {
      FillDrawSurface(*output, modelObject->objectPtr, surface, material,
                      material->progs[8], skipDetailTriangles,
                      parameters->settings->sortPerturber + *sortOffset);
      *sortOffset += kSortIncrement;
    }
  }
  return opaqueProgram;
}

void DrawOpaqueSurface(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* modelObject,
    const idODSObject<idRenderModelSurface>* surfaceObject, bool cullSurface,
    bool testOcclusion, bool skipDetailTriangles, float* sortOffset,
    float* depthKeyMin, std::uint16_t* surfaceSequence,
    bool skipDepthViewStencil, bool skipSubSurfaceScattering,
    bool skipBuildDrawSurface) {
  const idDeclRenderProg* selectedProgram = BuildDrawSurf(
      parameters, modelObject, surfaceObject, cullSurface, testOcclusion,
      skipDetailTriangles, sortOffset, depthKeyMin, surfaceSequence,
      skipSubSurfaceScattering, skipBuildDrawSurface);
  if (selectedProgram == nullptr || parameters->settings->skipOpaqueSurfaces)
    return;
  const idRenderModelSurface* surface = surfaceObject->objectPtr;
  const idMaterial* material = surface->material;
  const idDeclRenderProg* program = selectedProgram->renamed;
  const std::uint64_t state =
      DepthDrawState(parameters, modelObject->objectPtr, material, program,
                     skipDepthViewStencil);
  DrawSurfaceProgram(surface, material, program, state, skipDetailTriangles);
}

bool GetSkipSubSurfaceScattering(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* modelObject) {
  bool result = parameters->settings->skipSubSurfaceScattering;
  if (!result &&
      !parameters->settings->subSurfaceScatteringSkipRangeCulling &&
      (modelObject->objectPtr->committedFlags0 & 0x40u) != 0) {
    float depthMinimum = 0.0f;
    float depthMaximum = 0.0f;
    idRenderMatrix::DepthBoundsForBounds(
        depthMinimum, depthMaximum, modelObject->objectPtr->mvpMatrix,
        modelObject->objectPtr->referenceBounds, true);
    const float z =
        -parameters->settings->subSurfaceScatteringMaxVisibilityRange;
    const idRenderMatrix& projection = parameters->renderView->projectionMatrix;
    const float projectedDepth =
        (z * projection.m[10] + projection.m[11]) /
        (z * projection.m[14] + projection.m[15]);
    result = depthMinimum > projectedDepth;
  }
  return result;
}

void BuildModelDrawsurfs(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* modelObject, float* sortOffset,
    bool testOcclusionRequested) {
  const idRenderModelCommitted* model = modelObject->objectPtr;
  const bool cullSurface = model->surfaces.num > 1;
  const bool skipSubSurfaceScattering =
      GetSkipSubSurfaceScattering(parameters, modelObject);
  const bool testOcclusion =
      TestModelOcclusion(model, testOcclusionRequested);
  std::uint32_t depthKeyBits = UINT32_MAX;
  std::uint16_t surfaceSequence = 0x3FFF;
  for (int surfaceIndex = 0; surfaceIndex < model->surfaces.num;
       ++surfaceIndex) {
    const int singleSurface = parameters->settings->singleSurface;
    if (singleSurface >= 0 && singleSurface != surfaceIndex)
      continue;
    const idODSObject<idRenderModelSurface> surface{
        &model->surfaces.list[surfaceIndex]};
    BuildDrawSurf(parameters, modelObject, &surface, cullSurface,
                  testOcclusion, false, sortOffset,
                  reinterpret_cast<float*>(&depthKeyBits), &surfaceSequence,
                  skipSubSurfaceScattering, false);
  }
}

void DrawModelOpaqueSurfaces(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* modelObject, float* sortOffset,
    bool testOcclusionRequested, std::uint16_t skipBuildDrawSurf) {
  const bool skipBuildDrawSurface =
      static_cast<std::uint8_t>(skipBuildDrawSurf) != 0;
  const bool skipDepthViewStencil = parameters->skipDepthViewStencil != 0;
  const idRenderModelCommitted* model = modelObject->objectPtr;
  const bool cullSurface = model->surfaces.num > 1;
  const bool skipSubSurfaceScattering =
      skipBuildDrawSurface ||
      GetSkipSubSurfaceScattering(parameters, modelObject);
  const bool testOcclusion =
      TestModelOcclusion(model, testOcclusionRequested);
  float postSpecularEnabled =
      parameters->settings->skipPostSpecular ? 0.0f : 1.0f;
  if (skipSubSurfaceScattering)
    postSpecularEnabled = 0.0f;

  renderThreadParmState->SetModelMatrix(model->modelMatrix);
  renderThreadParmState->SetInverseModelMatrix(model->inverseModelMatrix);
  renderThreadParmState->SetMVPMatrix(model->mvpMatrix);
  renderThreadParmState->SetApproximateLighting(
      model->approximateLighting[model->CurrentApproximateLighting()]);
  renderThreadParmState->SetHighlightColor(model->highlightColor);
  renderThreadParmState->VirtualEvaluateParmBlock(model->renderParmBlock,
                                                  model->renderParmBlock);
  renderThreadParmState->SetParmFloat(parameters->rpPostSpecularEnabled,
                                      postSpecularEnabled);

  if (testOcclusion) {
    GL_BeginQuery(&parameters->occlusionState
                       ->modelOcclusionQuery[model->index.index >> 8]);
  }
  std::uint32_t depthKeyBits = UINT32_MAX;
  std::uint16_t surfaceSequence = 0x3FFF;
  for (int surfaceIndex = 0; surfaceIndex < model->surfaces.num;
       ++surfaceIndex) {
    const int singleSurface = parameters->settings->singleSurface;
    if (singleSurface >= 0 && singleSurface != surfaceIndex)
      continue;
    const idODSObject<idRenderModelSurface> surface{
        &model->surfaces.list[surfaceIndex]};
    DrawOpaqueSurface(
        parameters, modelObject, &surface, cullSurface, testOcclusion, false,
        sortOffset, reinterpret_cast<float*>(&depthKeyBits), &surfaceSequence,
        skipDepthViewStencil, skipSubSurfaceScattering, skipBuildDrawSurface);
  }
  if (testOcclusion)
    GL_EndQuery();
  renderThreadParmState->VirtualRollbackParmBlock(model->renderParmBlock,
                                                  model->renderParmBlock);
}

void DrawModelDepthSurfaces(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* modelObject,
    bool skipPerforatedCullTest) {
  const idRenderModelCommitted* model = modelObject->objectPtr;
  renderThreadParmState->SetModelMatrix(model->modelMatrix);
  renderThreadParmState->SetInverseModelMatrix(model->inverseModelMatrix);
  renderThreadParmState->SetMVPMatrix(model->mvpMatrix);
  renderThreadParmState->VirtualEvaluateParmBlock(model->renderParmBlock,
                                                  model->renderParmBlock);
  const bool cullSurface = model->surfaces.num > 1;
  for (int surfaceIndex = 0; surfaceIndex < model->surfaces.num;
       ++surfaceIndex) {
    const int singleSurface = parameters->settings->singleSurface;
    if (singleSurface >= 0 && singleSurface != surfaceIndex)
      continue;
    const idODSObject<idRenderModelSurface> surface{
        &model->surfaces.list[surfaceIndex]};
    DrawDepthSurface(parameters, modelObject, &surface, cullSurface, false,
                     parameters->skipDepthViewStencil != 0,
                     skipPerforatedCullTest);
  }
  renderThreadParmState->VirtualRollbackParmBlock(model->renderParmBlock,
                                                  model->renderParmBlock);
}

void RenderDepth(const renderDepthParms_t* parameters) {
  GL_SetDefaultState();
  GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
  SetViewportAndScissor(*parameters->renderView);
  for (std::uint32_t clear = 0; clear < parameters->extraClears; ++clear) {
    const idVec3& color = parameters->settings->clearColor;
    GL_Clear(true, true, true, 0, color.x, color.y, color.z, 0.0f);
  }
  if (parameters->settings->skipWorld ||
      parameters->settings->skipAddAlways ||
      parameters->settings->skipOpaqueSurfaces) {
    const idVec3& color = parameters->settings->clearColor;
    GL_Clear(true, false, false, 0, color.x, color.y, color.z, 0.0f);
  }
  if (&ActiveScissor(*parameters->renderView) ==
      &parameters->renderView->scissorRect) {
    GL_Clear(false, true, true, 1, 0.0f, 0.0f, 0.0f, 0.0f);
  }
  GL_Flush();
  GL_BeginQueryBatch();
  GL_BindTexture(0, parameters->imgBlack, nullptr);
  renderThreadParmState->SetFragmentPositionToFeedback(
      parameters->renderView->renderWidth,
      parameters->renderView->renderHeight,
      parameters->renderView->feedbackWidth,
      parameters->renderView->feedbackHeight);

  const int preZMask = parameters->settings->doPreZMask;
  if (preZMask == 0)
    return;
  GL_SetRenderDestination({parameters->renderDestDepth}, 0, 0);
  SetViewportAndScissor(*parameters->renderView);

  if ((preZMask & 1) != 0) {
    RenderEventRecorderLightStartEvent_Impl("PreZ Scene Models");
    for (int rangeIndex = 1; rangeIndex <= 5; ++rangeIndex) {
      const int depthSort = rangeIndex - 6;
      if (parameters->skipFirstPersonModels != 0 &&
          (depthSort == MODEL_DEPTH_SORT_VIEW_HEAD ||
           depthSort == MODEL_DEPTH_SORT_VIEW_BODY)) {
        continue;
      }
      const renderPassDepthModels_t& range =
          parameters->renderViewModels[rangeIndex];
      for (int modelIndex = range.firstModel; modelIndex <= range.lastModel;
           ++modelIndex) {
        const idRenderModelCommitted* model =
            parameters->viewModels[modelIndex];
        if ((model->committedFlags1 & 0x01u) != 0 &&
            model->r.modelDepthSort <= MODEL_DEPTH_SORT_NORMAL) {
          const idODSObject<idRenderModelCommitted> modelObject{model};
          DrawModelDepthSurfaces(parameters, &modelObject, false);
        }
      }
    }
    RenderEventRecorderLightStopEvent_Impl();
  }

  if ((preZMask & 2) != 0) {
    const idRenderModelCommitted* worldModel = parameters->viewModels[0];
    const idODSObject<idRenderModelCommitted> worldModelObject{worldModel};
    renderThreadParmState->SetModelMatrix(worldModel->modelMatrix);
    renderThreadParmState->SetInverseModelMatrix(worldModel->inverseModelMatrix);
    renderThreadParmState->SetMVPMatrix(worldModel->mvpMatrix);
    RenderEventRecorderLightStartEvent_Impl("PreZ World areas");
    int globalSurfaceIndex = 0;
    int worldSurfaceIndex = 0;
    for (int areaIndex = 0; areaIndex < parameters->numViewWorldAreas;
         ++areaIndex) {
      const viewWorldArea_t& area = parameters->viewWorldAreas[areaIndex];
      const bool cullSurface = area.numSurfs > 1;
      if (area.skipDetailTriangles)
        break;
      for (int localSurface = 0; localSurface < area.numSurfs;
           ++localSurface, ++globalSurfaceIndex, ++worldSurfaceIndex) {
        const int singleSurface = parameters->settings->singleSurface;
        if (singleSurface >= 0 && singleSurface != globalSurfaceIndex)
          continue;
        const idRenderModelSurface* surface =
            parameters->viewWorldSurfaces[worldSurfaceIndex];
        if (parameters->binaryModelState[surface->binaryModelId]) {
          const idODSObject<idRenderModelSurface> surfaceObject{surface};
          DrawDepthSurface(parameters, &worldModelObject, &surfaceObject,
                           cullSurface, false, true, false);
        }
      }
    }
    RenderEventRecorderLightStopEvent_Impl();
  }
  if ((preZMask & 3) != 0)
    GL_Flush();

  if ((preZMask & 4) != 0) {
    RenderEventRecorderLightStartEvent_Impl("prez regular models");
    for (int rangeIndex = 6; rangeIndex <= 16; ++rangeIndex) {
      const renderPassDepthModels_t& range =
          parameters->renderViewModels[rangeIndex];
      for (int modelIndex = range.firstModel; modelIndex <= range.lastModel;
           ++modelIndex) {
        const idRenderModelCommitted* model =
            parameters->viewModels[modelIndex];
        if ((model->committedFlags1 & 0x01u) != 0 &&
            model->r.modelDepthSort <= MODEL_DEPTH_SORT_NORMAL) {
          const idODSObject<idRenderModelCommitted> modelObject{model};
          DrawModelDepthSurfaces(parameters, &modelObject, false);
        }
      }
    }
    RenderEventRecorderLightStopEvent_Impl();
    GL_Flush();
  }
}

void RenderDepthFirstPerson(const renderDepthParms_t* parameters) {
  GL_SetRenderDestination({parameters->renderDestDepth}, 0, 0);
  SetViewportAndScissor(*parameters->renderView);
  RenderEventRecorderLightStartEvent_Impl(
      "postz regular models (First Person)");
  for (int rangeIndex = 3; rangeIndex <= 4; ++rangeIndex) {
    const renderPassDepthModels_t& range =
        parameters->renderViewModels[rangeIndex];
    for (int modelIndex = range.firstModel; modelIndex <= range.lastModel;
         ++modelIndex) {
      const idODSObject<idRenderModelCommitted> modelObject{
          parameters->viewModels[modelIndex]};
      DrawModelDepthSurfaces(parameters, &modelObject, true);
    }
  }
  RenderEventRecorderLightStopEvent_Impl();
  GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
}

void RenderOpaque(const renderDepthParms_t* parameters) {
  GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
  SetViewportAndScissor(*parameters->renderView);
  float sortOffset = 0.0f;

  RenderEventRecorderLightStartEvent_Impl("Scene Models");
  for (int rangeIndex = 1; rangeIndex <= 5; ++rangeIndex) {
    const int depthSort = rangeIndex - 6;
    const bool buildOnly =
        parameters->skipFirstPersonModels != 0 &&
        (depthSort == MODEL_DEPTH_SORT_VIEW_HEAD ||
         depthSort == MODEL_DEPTH_SORT_VIEW_BODY);
    const renderPassDepthModels_t& range =
        parameters->renderViewModels[rangeIndex];
    for (int modelIndex = range.firstModel; modelIndex <= range.lastModel;
         ++modelIndex) {
      const idODSObject<idRenderModelCommitted> modelObject{
          parameters->viewModels[modelIndex]};
      if (buildOnly) {
        BuildModelDrawsurfs(parameters, &modelObject, &sortOffset,
                            parameters->settings->doPreZMask == 0);
      } else {
        DrawModelOpaqueSurfaces(parameters, &modelObject, &sortOffset,
                                parameters->settings->doPreZMask == 0, 0);
      }
    }
  }
  RenderEventRecorderLightStopEvent_Impl();

  const idRenderModelCommitted* worldModel = parameters->viewModels[0];
  const idODSObject<idRenderModelCommitted> worldModelObject{worldModel};
  renderThreadParmState->SetModelMatrix(worldModel->modelMatrix);
  renderThreadParmState->SetInverseModelMatrix(worldModel->inverseModelMatrix);
  renderThreadParmState->SetMVPMatrix(worldModel->mvpMatrix);
  renderThreadParmState->SetApproximateLighting(
      worldModel->approximateLighting[worldModel->CurrentApproximateLighting()]);
  renderThreadParmState->SetHighlightColor(worldModel->highlightColor);

  std::uint32_t worldDepthKeyBits = UINT32_MAX;
  std::uint16_t worldSurfaceSequence = 0x3FFF;
  const int worldPasses = std::max(parameters->renderExtraWorldPassDebug + 1, 1);
  for (int worldPass = 0; worldPass < worldPasses; ++worldPass) {
    RenderEventRecorderLightStartEvent_Impl("World areas");
    int globalSurfaceIndex = 0;
    int worldSurfaceIndex = 0;
    for (int areaIndex = 0; areaIndex < parameters->numViewWorldAreas;
         ++areaIndex) {
      const viewWorldArea_t& area = parameters->viewWorldAreas[areaIndex];
      const bool cullSurface = area.numSurfs > 1;
      if (worldPass == 0) {
        GL_BeginQuery(&parameters->occlusionState
                           ->worldAreaOcclusionQuery[area.areaNum]);
      }
      for (int localSurface = 0; localSurface < area.numSurfs;
           ++localSurface, ++globalSurfaceIndex, ++worldSurfaceIndex) {
        const int singleSurface = parameters->settings->singleSurface;
        if (singleSurface >= 0 && singleSurface != globalSurfaceIndex)
          continue;
        const idRenderModelSurface* surface =
            parameters->viewWorldSurfaces[worldSurfaceIndex];
        if (!parameters->binaryModelState[surface->binaryModelId])
          continue;
        const idODSObject<idRenderModelSurface> surfaceObject{surface};
        const idDeclRenderProg* selectedProgram = BuildDrawSurf(
            parameters, &worldModelObject, &surfaceObject, cullSurface, true,
            area.skipDetailTriangles, &sortOffset,
            reinterpret_cast<float*>(&worldDepthKeyBits),
            &worldSurfaceSequence, true, false);
        if (selectedProgram != nullptr &&
            !parameters->settings->skipOpaqueSurfaces) {
          const idMaterial* material = surface->material;
          const idDeclRenderProg* program = selectedProgram->renamed;
          std::uint64_t state = 0;
          if ((material->flags0 & 0x02u) != 0 ||
              parameters->settings->forceTwoSidedDepth) {
            state = UINT64_C(0x8000);
          }
          if ((worldModel->r.flags0 & 0x10u) != 0 ||
              (material->flags1 & 0x20u) != 0) {
            state |= UINT64_C(0x40);
          }
          DrawSurfaceProgram(surface, material, program, state,
                             area.skipDetailTriangles);
        }
      }
      if (worldPass == 0)
        GL_EndQuery();
    }
    RenderEventRecorderLightStopEvent_Impl();
  }
  GL_Flush();

  RenderEventRecorderLightStartEvent_Impl("base occlusion model");
  if (!parameters->settings->skipOcclusionBaseModel &&
      !parameters->settings->skipOpaqueSurfaces &&
      parameters->occlusionBaseTriangles != nullptr) {
    renderThreadParmState->SetMVPMatrix(worldModel->mvpMatrix);
    const idODSObject<idDeclRenderProg> program{
        parameters->progOccluderDepthOnly};
    const idODSObject<idTriangles> triangles{
        parameters->occlusionBaseTriangles};
    GL_DrawElements(program, triangles, UINT64_C(0x8000000000000780), false);
  }
  RenderEventRecorderLightStopEvent_Impl();

  RenderEventRecorderLightStartEvent_Impl("regular models");
  for (int localRangeIndex = 0; localRangeIndex <= 10; ++localRangeIndex) {
    const int rangeIndex = localRangeIndex + 6;
    const bool buildOnly = parameters->skipFirstPersonModels != 0 &&
                           localRangeIndex == 2;
    const renderPassDepthModels_t& range =
        parameters->renderViewModels[rangeIndex];
    for (int modelIndex = range.firstModel; modelIndex <= range.lastModel;
         ++modelIndex) {
      const idODSObject<idRenderModelCommitted> modelObject{
          parameters->viewModels[modelIndex]};
      if (buildOnly) {
        BuildModelDrawsurfs(parameters, &modelObject, &sortOffset, true);
      } else {
        DrawModelOpaqueSurfaces(parameters, &modelObject, &sortOffset, true,
                                0);
      }
    }
  }
  RenderEventRecorderLightStopEvent_Impl();
}

void RenderOpaqueFirstPerson(const renderDepthParms_t* parameters) {
  GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
  SetViewportAndScissor(*parameters->renderView);
  float sortOffset = 0.0f;
  RenderEventRecorderLightStartEvent_Impl(
      "regular models (First Person)");
  for (int rangeIndex = 3; rangeIndex <= 4; ++rangeIndex) {
    const renderPassDepthModels_t& range =
        parameters->renderViewModels[rangeIndex];
    for (int modelIndex = range.firstModel; modelIndex <= range.lastModel;
         ++modelIndex) {
      const idODSObject<idRenderModelCommitted> modelObject{
          parameters->viewModels[modelIndex]};
      DrawModelOpaqueSurfaces(parameters, &modelObject, &sortOffset, true, 1);
    }
  }
  RenderEventRecorderLightStopEvent_Impl();
}

void RenderOpaqueOcclusionTest(const renderDepthParms_t* parameters) {
  GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
  SetViewportAndScissor(*parameters->renderView);
  float sortOffset = 0.0f;
  RenderEventRecorderLightStartEvent_Impl("Occlusion Test Models");
  const renderPassDepthModels_t& range = parameters->renderViewModels[8];
  for (int modelIndex = range.firstModel; modelIndex <= range.lastModel;
       ++modelIndex) {
    const idODSObject<idRenderModelCommitted> modelObject{
        parameters->viewModels[modelIndex]};
    DrawModelOpaqueSurfaces(parameters, &modelObject, &sortOffset, true, 1);
  }
  RenderEventRecorderLightStopEvent_Impl();
}
