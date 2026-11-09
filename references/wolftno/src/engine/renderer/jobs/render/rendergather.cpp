#include "engine/renderer/jobs/render/rendergather.h"

#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/odsstreamedoutputarray.h"
#include "engine/renderer/jobs/render/renderocclusion.h"
#include "engine/renderer/jobs/render/setupmodelmvp.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderlightcommitted.h"
#include "engine/renderer/renderlog.h"
#include "engine/renderer/rendermodelcommitted.h"
#include "engine/renderer/renderworldcommitted.h"
#include "shared/idlib/lookuptable.h"
#include "shared/idlib/parallelism/paralleljoblist.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {
constexpr int kMaximumViewEntries = 8192;
constexpr int kMaximumOcclusionAreas = 4096;
constexpr int kMaximumOcclusionModels = 4096;
constexpr int kMaximumOcclusionLights = 512;
constexpr int kMaximumBlendLights = 64;
constexpr int kMaximumGodrayModels = 14;
constexpr std::uint32_t kLastModelSequence = 0xFFFFFFu;
constexpr float kTransparentDepthBase = 131072.0f;

bool IsIndexMarked(const std::uint8_t* bits, unsigned int renderIndex) {
  const unsigned int byteIndex = renderIndex >> 11;
  const std::uint8_t mask = static_cast<std::uint8_t>(renderIndex);
  return (bits[byteIndex] & mask) != 0;
}

void MarkIndex(std::uint8_t* bits, unsigned int renderIndex) {
  bits[renderIndex >> 11] |= static_cast<std::uint8_t>(renderIndex);
}

bool BoundsIntersect(const idBounds& left, const idBounds& right) {
  return left.b[0].x <= right.b[1].x && right.b[0].x <= left.b[1].x &&
         left.b[0].y <= right.b[1].y && right.b[0].y <= left.b[1].y &&
         left.b[0].z <= right.b[1].z && right.b[0].z <= left.b[1].z;
}

bool OutsideVisibleRange(const idVec4& range, const idVec4& viewOrigin) {
  return std::fabs(viewOrigin.x - range.x) > range.w ||
         std::fabs(viewOrigin.y - range.y) > range.w ||
         std::fabs(viewOrigin.z - range.z) > range.w ||
         std::fabs(viewOrigin.w - range.w) > range.w;
}

std::uint32_t FloatBits(float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

std::uint64_t BuildModelSortKey(float depth, std::uint32_t sequence,
                                modelDepthSort_t sort) {
  const std::uint32_t high =
      (sequence & 0xFFFFFFu) |
      (static_cast<std::uint32_t>(static_cast<int>(sort) + 6) << 24);
  return static_cast<std::uint64_t>(FloatBits(depth)) |
         (static_cast<std::uint64_t>(high) << 32);
}

bool ModelIsStatic(const idRenderModelCommitted& model) {
  return (model.r.flags0 & 0x04u) != 0;
}

bool ModelIsGodray(const idRenderModelCommitted& model) {
  return (model.r.flags0 & 0x40u) != 0;
}

bool ModelNoGpuOcclusion(const idRenderModelCommitted& model) {
  return (model.r.flags2 & 0x08u) != 0 ||
         (model.committedFlags1 & 0x08u) != 0;
}

bool ModelNeverOcclusionCull(const idRenderModelCommitted& model) {
  return (model.r.flags2 & 0x10u) != 0;
}

bool ModelAlwaysOcclusionCull(const idRenderModelCommitted& model) {
  return (model.r.flags2 & 0x20u) != 0;
}

bool ModelAddAlwaysForceQuery(const idRenderModelCommitted& model) {
  return (model.r.flags2 & 0x40u) != 0;
}

bool ModelUsesOcclusionResult(const idRenderModelCommitted& model) {
  return (model.r.flags2 & 0x80u) != 0;
}

bool ModelIsSortModel(const idRenderModelCommitted& model) {
  return (model.r.flags4 & 0x40u) != 0;
}

bool ModelPassesVisibleFrameGate(const renderGatherParms_t* parameters,
                                 const idRenderModelCommitted* model,
                                 unsigned int modelIndex) {
  occlusionState_t* state = parameters->occlusionState;
  const int frameNumber = state->renderFrameNumber;
  int visibleFrameCount = 0;
  if (state->modelVisibleFrameNumber[modelIndex] >= frameNumber)
    visibleFrameCount = state->modelVisibleFrameCount[modelIndex];
  state->modelVisibleFrameNumber[modelIndex] = frameNumber + 1;
  state->modelVisibleFrameCount[modelIndex] = visibleFrameCount + 1;
  return ModelIsStatic(*model) ||
         visibleFrameCount + 1 > model->r.firstVisibleFrameCount;
}

idVec4 HighlightBaseColor(highlightColor_t type) {
  switch (type) {
    case HIGHLIGHT_COLOR_CORPSE:
      return {0.0f, 0.0f, 0.0f, 0.0f};
    case HIGHLIGHT_COLOR_MAXED_HEALTH:
      return {0.53400004f, 0.27060002f, 0.066f, 1.0f};
    case HIGHLIGHT_COLOR_MAXED_AMMO:
      return {0.5f, 0.75f, 0.75f, 1.0f};
    default:
      return {0.96862751f, 0.94117653f, 0.70588237f, 1.0f};
  }
}
} // namespace

void RenderGather(const renderGatherParms_t* parameters) {
  std::uint8_t* const modelBits = parameters->temp;
  std::uint8_t* const lightBits = parameters->temp + 1024;
  std::memset(parameters->temp, 0, 2048);

  idODSStreamedOutputArray<viewWorldArea_t, 128> viewWorldAreas(
      parameters->viewWorldAreas, parameters->numViewWorldAreas,
      kMaximumViewEntries);
  idODSStreamedOutputArray<const idRenderModelSurface*, 128>
      viewWorldSurfaces(parameters->viewWorldSurfaces,
                        parameters->numViewWorldSurfaces,
                        kMaximumViewEntries);
  idODSStreamedOutputArray<const idRenderModelCommitted*, 128> viewModels(
      parameters->viewModels, parameters->numViewModels, kMaximumViewEntries);
  idODSStreamedOutputArray<const idRenderModelCommitted*, 128> viewModelsPreZ(
      parameters->viewModelsPreZ, parameters->numViewModelsPreZ,
      kMaximumViewEntries);
  idODSStreamedOutputArray<std::uint64_t, 128> viewModelSortKeys(
      parameters->viewModelSortKeys, parameters->numViewModels,
      kMaximumViewEntries);
  idODSStreamedOutputArray<const idRenderModelCommitted*, 128>
      viewGodrayModels(parameters->viewGodrayModels,
                       parameters->numViewGodrayModels, 16);
  idODSStreamedOutputArray<const idRenderLightCommitted*, 128> viewLights(
      parameters->viewLights, parameters->numViewLights, kMaximumViewEntries);
  idODSStreamedOutputArray<int, 128> occlusionTestWorldAreas(
      parameters->occlusionTestWorldAreas,
      parameters->numOcclusionTestWorldAreas, kMaximumOcclusionAreas);
  idODSStreamedOutputArray<const idRenderModelCommitted*, 128>
      occlusionTestModels(parameters->occlusionTestModels,
                          parameters->numOcclusionTestModels,
                          kMaximumOcclusionModels);
  idODSStreamedOutputArray<const idRenderLightCommitted*, 128>
      occlusionTestLights(parameters->occlusionTestLights,
                          parameters->numOcclusionTestLights,
                          kMaximumOcclusionLights);

  viewModelSortKeys.Append(0);
  viewModels.Append(parameters->viewModels[0]);
  viewModelsPreZ.Append(parameters->viewModels[0]);

  const idRenderLightCommitted* blendViewLights[kMaximumBlendLights]{};
  int numBlendViewLights = 0;
  std::uint32_t areaSequence = 0;

  for (int nodeAreaIndex = 0;
       nodeAreaIndex < parameters->numNodeWorldAreas;
       ++nodeAreaIndex, ++areaSequence) {
    const nodeWorldArea_t& nodeArea = parameters->nodeWorldAreas[nodeAreaIndex];
    const int areaNum = nodeArea.areaNum;
    renderLog.OpenBlock("areaNum %i\n", areaNum);
    const renderSettings_t* settings = parameters->settings;
    const bool skipAreaCpuCulling = settings->skipAreaCPUCulling;
    if (!skipAreaCpuCulling &&
        parameters->worldAreaSubspaceBoundsCulled[areaNum] != 0) {
      renderLog.OpenBlock("MVP culled\n");
      continue;
    }

    const worldArea_t& area = parameters->worldAreas[areaNum];
    if (!settings->skipWorld) {
      renderLog.OpenBlock("world surfaces\n");
      if (!skipAreaCpuCulling &&
          parameters->worldAreaGeometryBoundsCulled[areaNum] == 3) {
        renderLog.OpenBlock("MVP culled\n");
      } else {
        occlusionState_t* state = parameters->occlusionState;
        bool visible;
        int queryResult = settings->queryThreshold;
        if (settings->lockView) {
          visible = state->worldAreaOcclusionQueryResult[areaNum] >=
                    settings->queryThreshold;
        } else {
          const bool forciblyAccepted =
              parameters->worldAreaGeometryBoundsCulled[areaNum] == 0 ||
              settings->skipAreaGPUCulling;
          if (parameters->renderView->r.forceFullVirtualTextureLoad ||
              parameters->renderView->r.discontinuousViewPosition ||
              forciblyAccepted) {
            renderLog.OpenBlock("in bounds or forcibly accepted\n");
          } else if (state->worldAreaOcclusionQuery[areaNum] >=
                     parameters->renderView
                         ->firstOcclusionQueryInPreviousFrame) {
            queryResult = GL_GetDeferredQueryResult(
                state->worldAreaOcclusionQuery[areaNum]);
            if (queryResult < 0) {
              renderLog.OpenBlock(
                  "occlusion test overflow, assumed visible\n");
            } else if (queryResult < settings->queryThreshold) {
              renderLog.OpenBlock("occlusion test failed\n");
            } else {
              renderLog.OpenBlock("occlusion test passed\n");
            }
          } else {
            renderLog.OpenBlock(
                "no query on previous frame, assumed visible\n");
          }
          visible = queryResult < 0 || queryResult >= settings->queryThreshold;
          state->worldAreaOcclusionQueryResult[areaNum] = queryResult;
        }

        bool binaryModelHidden =
            area.binaryModelId >= 0 &&
            !parameters->binaryModelState[area.binaryModelId];
        if (area.binaryModelId == -2) {
          binaryModelHidden = true;
          for (int surfaceIndex = 0; surfaceIndex < area.worldSurfaces.num;
               ++surfaceIndex) {
            if (parameters->binaryModelState[
                    area.worldSurfaces.list[surfaceIndex].binaryModelId]) {
              binaryModelHidden = false;
              break;
            }
          }
        }

        if (!visible || binaryModelHidden) {
          if (occlusionTestWorldAreas.Num() < kMaximumOcclusionAreas)
            occlusionTestWorldAreas.Append(areaNum);
          renderLog.OpenBlock("added for occlusion test only\n");
        } else {
          state->worldAreaVisibleFrameNumber[areaNum] =
              state->renderFrameNumber + 1;
          if (nodeArea.nodeOcclusionTestIndex >= 0) {
            parameters->occlusionTestNodes[nodeArea.nodeOcclusionTestIndex] =
                -1;
          }

          if (viewWorldAreas.Num() < kMaximumViewEntries &&
              viewWorldSurfaces.Num() + area.worldSurfaces.num <=
                  kMaximumViewEntries) {
            for (int surfaceIndex = 0; surfaceIndex < area.worldSurfaces.num;
                 ++surfaceIndex) {
              viewWorldSurfaces.Append(
                  &area.worldSurfaces.list[surfaceIndex]);
            }
            viewWorldArea_t& outputArea = viewWorldAreas.Alloc();
            outputArea.areaNum = areaNum;
            outputArea.numSurfs =
                static_cast<std::int16_t>(area.worldSurfaces.num);
            outputArea.skipDetailTriangles =
                parameters->worldAreaGeometryBoundsCulled[areaNum] == 2;
            outputArea.alignmentPadding = 0;
          }
          renderLog.OpenBlock("added to view\n");
        }
      }
    }

    if (!settings->skipModels) {
      for (int referenceIndex = 0; referenceIndex < area.modelRefs.num;
           ++referenceIndex) {
        const unsigned int encodedIndex =
            area.modelRefs.list[referenceIndex].index;
        if (IsIndexMarked(modelBits, encodedIndex))
          continue;
        MarkIndex(modelBits, encodedIndex);

        const unsigned int modelIndex = encodedIndex >> 8;
        if (settings->singleModel >= 0 &&
            settings->singleModel != static_cast<int>(modelIndex)) {
          continue;
        }
        const idRenderModelCommitted* model =
            parameters->committedRenderModels[modelIndex];
        if (!settings->skipSuppress) {
          if (model->r.allowSurfaceOnlyInViewID != 0 &&
              model->r.allowSurfaceOnlyInViewID !=
                  parameters->renderView->r.viewID) {
            continue;
          }
          if (model->r.suppressSurfaceInViewID != 0 &&
              model->r.suppressSurfaceInViewID ==
                  parameters->renderView->r.viewID) {
            continue;
          }
        }
        if (!settings->skipModelRangeCulling &&
            OutsideVisibleRange(model->maxVisibleRange,
                                settings->viewOrigin)) {
          renderLog.OpenBlock("outside max visible range\n");
          continue;
        }

        idRenderMatrix modelMvp{};
        SetupModelMVPMatrix(&modelMvp, &model->r, &model->modelMatrix,
                            parameters->renderView, settings->viewNearZ,
                            settings->viewFarZ, settings->guiOriginOffset);
        unsigned char cullBits = 0;
        if (!settings->skipModelCPUCulling &&
            idRenderMatrix::CullBoundsToMVPbits(
                modelMvp, model->referenceBounds, cullBits, false)) {
          renderLog.OpenBlock("MVP culled\n");
          continue;
        }
        idRenderModelCommitted* mutableModel =
            const_cast<idRenderModelCommitted*>(model);
        mutableModel->mvpMatrix = modelMvp;

        occlusionState_t* state = parameters->occlusionState;
        const std::int64_t query = state->modelOcclusionQuery[modelIndex];
        const bool insideViewBounds =
            BoundsIntersect(model->globalReferenceBounds,
                            settings->viewBounds);
        const bool forciblyAccepted = insideViewBounds ||
                                      settings->skipModelGPUCulling ||
                                      ModelNoGpuOcclusion(*model);
        bool visible;
        if (settings->lockView) {
          visible = state->modelOcclusionQueryResult[modelIndex] >=
                    settings->queryThreshold;
        } else {
          int queryResult = settings->queryThreshold;
          if (ModelUsesOcclusionResult(*model)) {
            queryResult = GL_GetDeferredQueryResult(query);
          } else if (parameters->renderView->r.forceFullVirtualTextureLoad ||
                     parameters->renderView->r.discontinuousViewPosition ||
                     forciblyAccepted) {
            renderLog.OpenBlock("in bounds or forcibly accepted\n");
          } else if (query < parameters->renderView
                                 ->firstOcclusionQueryInPreviousFrame) {
            renderLog.OpenBlock(
                "no query on previous frame, assumed visible\n");
          } else {
            queryResult = GL_GetDeferredQueryResult(query);
          }

          if (queryResult < 0) {
            renderLog.OpenBlock(
                "occlusion test overflow, assumed visible\n");
          } else if (queryResult < settings->queryThreshold) {
            renderLog.OpenBlock("occlusion test failed\n");
          } else if (queryResult != settings->queryThreshold ||
                     ModelUsesOcclusionResult(*model)) {
            renderLog.OpenBlock("occlusion test passed\n");
          }
          state->modelOcclusionQueryResult[modelIndex] = queryResult;
          visible = queryResult < 0 || queryResult >= settings->queryThreshold;
        }

        bool addToView = false;
        if (visible) {
          if (ModelPassesVisibleFrameGate(parameters, model, modelIndex)) {
            addToView = true;
          } else {
            renderLog.OpenBlock(
                "dynamic model not rendered because geometry is potentially stale\n");
          }
        }
        if (!addToView && ModelNeverOcclusionCull(*model))
          addToView = true;

        if (!addToView) {
          if (occlusionTestModels.Num() < kMaximumOcclusionModels)
            occlusionTestModels.Append(model);
          renderLog.OpenBlock("added for occlusion test only\n", query);
          continue;
        }

        if (ModelAlwaysOcclusionCull(*model) &&
            occlusionTestModels.Num() < kMaximumOcclusionModels) {
          occlusionTestModels.Append(model);
        }

        float minimumDepth = 0.0f;
        float maximumDepth = 0.0f;
        idRenderMatrix::DepthBoundsForBounds(minimumDepth, maximumDepth,
                                             modelMvp,
                                             model->referenceBounds, true);
        modelDepthSort_t depthSort = model->r.modelDepthSort;
        std::uint32_t sequence = areaSequence;
        if (depthSort == MODEL_DEPTH_SORT_TRANSPARENT) {
          sequence = kLastModelSequence - areaSequence;
          maximumDepth = kTransparentDepthBase - maximumDepth;
        } else if (depthSort == MODEL_DEPTH_SORT_NORMAL &&
                   insideViewBounds) {
          depthSort = MODEL_DEPTH_SORT_SCENE_MODEL;
        }
        viewModelSortKeys.Append(
            BuildModelSortKey(maximumDepth, sequence, depthSort));
        viewModels.Append(model);
        viewModelsPreZ.Append(model);
        renderLog.OpenBlock("added to view\n");
      }
    }

    if (!settings->skipLights) {
      for (int referenceIndex = 0; referenceIndex < area.drawLightRefs.num;
           ++referenceIndex) {
        const unsigned int encodedIndex =
            area.drawLightRefs.list[referenceIndex].index;
        if (IsIndexMarked(lightBits, encodedIndex))
          continue;
        MarkIndex(lightBits, encodedIndex);

        const unsigned int lightIndex = encodedIndex >> 8;
        if (settings->singleLight >= 0 &&
            settings->singleLight != static_cast<int>(lightIndex)) {
          continue;
        }
        const idRenderLightCommitted* light =
            parameters->committedRenderLights[lightIndex];
        if (!settings->skipSuppress) {
          if (light->r.allowLightOnlyInViewID != 0 &&
              light->r.allowLightOnlyInViewID !=
                  parameters->renderView->r.viewID) {
            continue;
          }
          if (light->r.suppressLightInViewID != 0 &&
              light->r.suppressLightInViewID ==
                  parameters->renderView->r.viewID) {
            continue;
          }
        }
        renderLog.OpenBlock("light %u\n", lightIndex);
        if (!settings->skipLightRangeCulling &&
            OutsideVisibleRange(light->maxVisibleRange,
                                settings->viewOrigin)) {
          renderLog.OpenBlock("outside max visible range\n");
          continue;
        }

        if (!settings->skipLightCPUCulling) {
          idRenderMatrix lightMvp{};
          idRenderMatrix::Multiply(parameters->renderView->worldSpaceMVPMatrix,
                                   light->inverseBaseLightProject, lightMvp);
          idBounds unitBounds{};
          unitBounds.b[0] = {0.0f, 0.0f, 0.0f};
          unitBounds.b[1] = {1.0f, 1.0f, 1.0f};
          unsigned char cullBits = 0;
          if (idRenderMatrix::CullBoundsToMVPbits(lightMvp, unitBounds,
                                                  cullBits, false)) {
            renderLog.OpenBlock("MVP culled\n");
            continue;
          }
        }

        occlusionState_t* state = parameters->occlusionState;
        const bool forciblyAccepted =
            BoundsIntersect(light->globalReferenceBounds,
                            settings->viewBounds) ||
            settings->skipLightGPUCulling;
        bool visible;
        if (settings->lockView) {
          visible = state->lightOcclusionQueryResult[lightIndex] >=
                    settings->queryThreshold;
        } else {
          int queryResult = settings->queryThreshold;
          if (parameters->renderView->r.forceFullVirtualTextureLoad ||
              parameters->renderView->r.discontinuousViewPosition ||
              forciblyAccepted) {
            renderLog.OpenBlock("in bounds or forcibly accepted\n");
          } else if (state->lightOcclusionQuery[lightIndex] <
                     parameters->renderView
                         ->firstOcclusionQueryInPreviousFrame) {
            renderLog.OpenBlock(
                "no query on previous frame, assumed visible\n");
          } else {
            queryResult = GL_GetDeferredQueryResult(
                state->lightOcclusionQuery[lightIndex]);
            if (queryResult < 0) {
              renderLog.OpenBlock(
                  "occlusion test overflow, assumed visible\n");
            } else if (queryResult < settings->queryThreshold) {
              renderLog.OpenBlock("occlusion test failed\n");
            } else {
              renderLog.OpenBlock("occlusion test passed\n");
            }
          }
          state->lightOcclusionQueryResult[lightIndex] = queryResult;
          visible = queryResult < 0 || queryResult >= settings->queryThreshold;
        }

        if (occlusionTestLights.Num() < kMaximumOcclusionLights)
          occlusionTestLights.Append(light);
        if (visible) {
          if (light->r.lightClass == LIGHT_BLENDED_ONLY) {
            if (numBlendViewLights < kMaximumBlendLights)
              blendViewLights[numBlendViewLights++] = light;
          } else if (viewLights.Num() < kMaximumViewEntries) {
            viewLights.Append(light);
          }
          renderLog.OpenBlock("added to view\n");
        }
      }
    }
  }

  if (!parameters->settings->skipAddAlways) {
    const idRenderModelCommitted* model = parameters->addAlwaysModels;
    while (model != nullptr) {
      const idRenderModelCommitted* nextModel = model->nextAddAlwaysModel;
      MarkIndex(modelBits, model->index.index);
      if (ModelIsGodray(*model)) {
        if (viewGodrayModels.Num() < kMaximumGodrayModels)
          viewGodrayModels.Append(model);
      } else {
        const unsigned int modelIndex = model->index.index >> 8;
        if (ModelPassesVisibleFrameGate(parameters, model, modelIndex)) {
          idRenderMatrix modelMvp{};
          SetupModelMVPMatrix(
              &modelMvp, &model->r, &model->modelMatrix,
              parameters->renderView, parameters->settings->viewNearZ,
              parameters->settings->viewFarZ,
              parameters->settings->guiOriginOffset);
          idRenderModelCommitted* mutableModel =
              const_cast<idRenderModelCommitted*>(model);
          mutableModel->mvpMatrix = modelMvp;

          occlusionState_t* state = parameters->occlusionState;
          if (ModelAddAlwaysForceQuery(*model)) {
            const std::int64_t query = state->modelOcclusionQuery[modelIndex];
            state->modelOcclusionQueryResult[modelIndex] =
                query >= parameters->renderView
                             ->firstOcclusionQueryInPreviousFrame
                    ? GL_GetDeferredQueryResult(query)
                    : 0;
          } else {
            state->modelOcclusionQueryResult[modelIndex] = 0;
          }

          float minimumDepth = 0.0f;
          float maximumDepth = 0.0f;
          idRenderMatrix::DepthBoundsForBounds(minimumDepth, maximumDepth,
                                               modelMvp,
                                               model->referenceBounds, true);
          std::uint32_t sequence = kLastModelSequence;
          const modelDepthSort_t depthSort = model->r.modelDepthSort;
          if (depthSort == MODEL_DEPTH_SORT_TRANSPARENT) {
            sequence = 0;
            maximumDepth = kTransparentDepthBase - maximumDepth;
          }
          viewModelSortKeys.Append(
              BuildModelSortKey(maximumDepth, sequence, depthSort));
          viewModels.Append(model);
          viewModelsPreZ.Append(model);
        }
      }
      model = nextModel;
    }
  }

  if (parameters->splitPlaneDistance != nullptr) {
    float splitDistance = -131072.0f;
    for (int modelIndex = 0; modelIndex < viewModels.Num(); ++modelIndex) {
      const idRenderModelCommitted* model = parameters->viewModels[modelIndex];
      if (ModelIsSortModel(*model)) {
        splitDistance = model->modelMatrix.m[11];
        break;
      }
    }
    *parameters->splitPlaneDistance = splitDistance;
  }

  const float highlightDuration =
      std::max(1.1920929e-7f, parameters->highlightDuration);
  const float renderTimeSeconds =
      static_cast<float>(parameters->renderView->r.renderTime) * 0.001f;
  for (int modelIndex = 0; modelIndex < viewModels.Num(); ++modelIndex) {
    idRenderModelCommitted* model = const_cast<idRenderModelCommitted*>(
        parameters->viewModels[modelIndex]);
    if (model->r.highlightColor == HIGHLIGHT_COLOR_NONE) {
      model->highlightColor = {0.0f, 0.0f, 0.0f, 1.0f};
      continue;
    }

    const std::uint32_t randomState =
        UINT32_C(1103515245) * (model->index.index >> 8) + UINT32_C(12345);
    const int randomValue =
        (static_cast<std::int32_t>(randomState) >> 16) & 0x7FFF;
    const float lookupTime =
        ((static_cast<float>(randomValue) * 0.000030518509f *
          highlightDuration) +
         renderTimeSeconds) /
        highlightDuration;
    float intensity = 0.0f;
    if (parameters->highlightEnable && parameters->highlightTable != nullptr) {
      intensity = const_cast<idLookupTable*>(parameters->highlightTable)
                      ->TableLookup(lookupTime, false);
    }
    intensity = std::max(0.0f, intensity);
    if (model->r.highlightColor == HIGHLIGHT_COLOR_CONSTANT)
      intensity = 1.0f;

    const idVec4 baseColor = HighlightBaseColor(model->r.highlightColor);
    const float deltaX = model->modelMatrix.m[3] -
                         parameters->settings->viewOrigin.x;
    const float deltaY = model->modelMatrix.m[7] -
                         parameters->settings->viewOrigin.y;
    const float deltaZ = model->modelMatrix.m[11] -
                         parameters->settings->viewOrigin.z;
    const float distanceSquared =
        deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ;
    if (distanceSquared > 10000.0f) {
      if (distanceSquared <= 4000000.0f) {
        intensity *=
            ((distanceSquared - 10000.0f) * 39.0f) / 3990000.0f + 1.0f;
      } else {
        intensity *= 40.0f;
      }
    }
    model->highlightColor = {intensity * baseColor.x,
                             intensity * baseColor.y,
                             intensity * baseColor.z,
                             intensity * baseColor.w};
  }

  const int numBlendLightsToAppend =
      std::min(numBlendViewLights, kMaximumViewEntries - viewLights.Num());
  for (int lightIndex = 0; lightIndex < numBlendLightsToAppend; ++lightIndex)
    viewLights.Append(blendViewLights[lightIndex]);

  if (parameters->settings->forceOcclusionBoxQueries) {
    for (int areaIndex = 0; areaIndex < viewWorldAreas.Num(); ++areaIndex) {
      if (occlusionTestWorldAreas.Num() < kMaximumOcclusionAreas) {
        occlusionTestWorldAreas.Append(
            parameters->viewWorldAreas[areaIndex].areaNum);
      }
    }
    for (int modelIndex = 1; modelIndex < viewModels.Num(); ++modelIndex) {
      if (occlusionTestModels.Num() < kMaximumOcclusionModels)
        occlusionTestModels.Append(parameters->viewModels[modelIndex]);
    }
  }
}

idParallelJobRegistration register_RenderGather(
    reinterpret_cast<jobRun_t>(RenderGather), "RenderGather");
