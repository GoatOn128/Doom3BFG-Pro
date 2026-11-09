#include "engine/renderer/jobs/render/renderdimshadows.h"

#include "engine/models/rendermodel.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/jobs/render/renderocclusion.h"
#include "engine/renderer/jobs/render/setupmodelmvp.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/renderlog.h"
#include "engine/renderer/rendermodelcommitted.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/math/plane.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {
constexpr int kMaximumDimShadowModels = 256;
constexpr int kMaximumDimShadowSurfaces = 256;

float InverseLength(float x, float y, float z) {
  return 1.0f /
         std::sqrt(std::max(x * x + y * y + z * z, FLT_MIN));
}

idVec3 Normalize(const idVec3& value) {
  const float scale = InverseLength(value.x, value.y, value.z);
  return {value.x * scale, value.y * scale, value.z * scale};
}

float ProjectViewDepth(const idRenderMatrix& projection, float distance) {
  const float z = -distance;
  return (z * projection.m[10] + projection.m[11]) /
         (z * projection.m[14] + projection.m[15]);
}

const idTextureSampler* AsSampler(const idImage* image) {
  return reinterpret_cast<const idTextureSampler*>(image);
}

struct dimShadowSurface_t {
  const idRenderModelSurface* surface;
  const idMaterial* material;
  const idDeclRenderProg* program;
};

struct dimShadowSurfaceBucket_t {
  std::array<const dimShadowSurface_t*, kMaximumDimShadowSurfaces> entries{};
  int count{};

  void Add(const dimShadowSurface_t* entry) {
    if (count < static_cast<int>(entries.size()))
      entries[count++] = entry;
  }
};
}  // namespace

void idRenderLog::OpenBlock(const char*, ...) {}

void CalculateLightMatrix(idRenderMatrix& lightMatrix,
                          const idVec3& primeLightDirection,
                          const idRenderMatrix& modelMatrix,
                          const idBounds& referenceBounds) {
  lightMatrix.m[8] = -primeLightDirection.x;
  lightMatrix.m[9] = -primeLightDirection.y;
  lightMatrix.m[10] = -primeLightDirection.z;
  lightMatrix.m[11] = 0.0f;

  if (std::fabs(primeLightDirection.z) >= 0.99000001f) {
    const float yz = primeLightDirection.y + primeLightDirection.z;
    lightMatrix.m[4] = -yz * primeLightDirection.x;
    lightMatrix.m[5] = 1.0f - yz * primeLightDirection.y;
    lightMatrix.m[6] = 1.0f - yz * primeLightDirection.z;
  } else {
    lightMatrix.m[4] =
        primeLightDirection.z * primeLightDirection.x;
    lightMatrix.m[5] =
        primeLightDirection.z * primeLightDirection.y;
    lightMatrix.m[6] =
        primeLightDirection.z * primeLightDirection.z - 1.0f;
  }
  const float rowScale = InverseLength(lightMatrix.m[4], lightMatrix.m[5],
                                       lightMatrix.m[6]);
  lightMatrix.m[4] *= rowScale;
  lightMatrix.m[5] *= rowScale;
  lightMatrix.m[6] *= rowScale;
  lightMatrix.m[7] = 0.0f;

  lightMatrix.m[0] = lightMatrix.m[10] * lightMatrix.m[5] -
                     lightMatrix.m[9] * lightMatrix.m[6];
  lightMatrix.m[1] = lightMatrix.m[8] * lightMatrix.m[6] -
                     lightMatrix.m[10] * lightMatrix.m[4];
  lightMatrix.m[2] = lightMatrix.m[9] * lightMatrix.m[4] -
                     lightMatrix.m[8] * lightMatrix.m[5];
  lightMatrix.m[3] = 0.0f;
  lightMatrix.m[12] = 0.0f;
  lightMatrix.m[13] = 0.0f;
  lightMatrix.m[14] = 0.0f;
  lightMatrix.m[15] = 1.0f;

  idRenderMatrix modelLightMatrix{};
  idRenderMatrix::Multiply(lightMatrix, modelMatrix, modelLightMatrix);
  idBounds boundsInLightSpace{};
  boundsInLightSpace.FromModelMatrixBounds(referenceBounds, modelLightMatrix);

  const float minimumX =
      boundsInLightSpace.b[0].x * lightMatrix.m[0] +
      boundsInLightSpace.b[0].y * lightMatrix.m[4] +
      boundsInLightSpace.b[0].z * lightMatrix.m[8];
  const float minimumY =
      boundsInLightSpace.b[0].x * lightMatrix.m[1] +
      boundsInLightSpace.b[0].y * lightMatrix.m[5] +
      boundsInLightSpace.b[0].z * lightMatrix.m[9];
  const float minimumZ =
      boundsInLightSpace.b[0].x * lightMatrix.m[2] +
      boundsInLightSpace.b[0].y * lightMatrix.m[6] +
      boundsInLightSpace.b[0].z * lightMatrix.m[10];

  const float scaleX =
      2.0f / (boundsInLightSpace.b[1].x - boundsInLightSpace.b[0].x);
  lightMatrix.m[0] *= scaleX;
  lightMatrix.m[1] *= scaleX;
  lightMatrix.m[2] *= scaleX;
  lightMatrix.m[3] = -1.0f - minimumX * lightMatrix.m[0] -
                     minimumY * lightMatrix.m[1] -
                     minimumZ * lightMatrix.m[2];

  const float scaleY =
      2.0f / (boundsInLightSpace.b[1].y - boundsInLightSpace.b[0].y);
  lightMatrix.m[4] *= scaleY;
  lightMatrix.m[5] *= scaleY;
  lightMatrix.m[6] *= scaleY;
  lightMatrix.m[7] = -1.0f - minimumX * lightMatrix.m[4] -
                     minimumY * lightMatrix.m[5] -
                     minimumZ * lightMatrix.m[6];

  const float scaleZ =
      2.0f / (boundsInLightSpace.b[1].z - boundsInLightSpace.b[0].z);
  lightMatrix.m[8] *= scaleZ;
  lightMatrix.m[9] *= scaleZ;
  lightMatrix.m[10] *= scaleZ;
  lightMatrix.m[11] = -1.0f - minimumX * lightMatrix.m[8] -
                      minimumY * lightMatrix.m[9] -
                      minimumZ * lightMatrix.m[10];
}

float DistanceToBox(const idBounds& box, const idVec3& point) {
  const auto axisDistance = [](float minimum, float maximum, float value) {
    if (value < minimum)
      return minimum - value;
    if (value > maximum)
      return value - maximum;
    return 0.0f;
  };
  const float x = axisDistance(box.b[0].x, box.b[1].x, point.x);
  const float y = axisDistance(box.b[0].y, box.b[1].y, point.y);
  const float z = axisDistance(box.b[0].z, box.b[1].z, point.z);
  return std::sqrt(std::max(x * x + y * y + z * z, FLT_MIN));
}

float ComputeShadowLODDistance(const idBounds& bounds,
                               const idVec3& camera,
                               const idVec3& lightDirection) {
  const idVec3 center{(bounds.b[0].x + bounds.b[1].x) * 0.5f,
                      (bounds.b[0].y + bounds.b[1].y) * 0.5f,
                      (bounds.b[0].z + bounds.b[1].z) * 0.5f};
  const idVec3 delta = center - camera;
  const idVec3 size = bounds.b[1] - bounds.b[0];
  const float radius =
      std::sqrt(std::max(size.LengthSqr(), FLT_MIN)) * 0.5f;
  const float crossDistance = std::sqrt(std::max(
      delta.Cross(lightDirection).LengthSqr(), FLT_MIN));
  const float alongLight = delta * lightDirection;
  const float shadowDistance = std::max(
      std::max(0.0f, crossDistance - radius * 0.5f),
      std::max(-alongLight, std::max(0.0f, alongLight - radius * 3.0f)));
  return std::max(std::min(DistanceToBox(bounds, camera), shadowDistance),
                  8.0f);
}

void RenderDimShadows(const renderDimShadowsParms_t* parameters) {
  std::memset(parameters->dimShadowModelSlaves, 0xFF,
              8192 * sizeof(parameters->dimShadowModelSlaves[0]));

  std::array<const idRenderModelCommitted*, kMaximumDimShadowModels>
      shadowModels{};
  int numShadowModels = 0;
  for (const idRenderModelCommitted* model = parameters->dimShadowModels;
       model != nullptr; model = model->nextDimShadowModel) {
    if (model->references.num <= 0)
      continue;
    if (model->r.suppressShadowInViewID != 0 &&
        model->r.suppressShadowInViewID == parameters->renderView->r.viewID) {
      continue;
    }
    const int modelIndex = static_cast<int>(model->index.index >> 8);
    if (parameters->settings->singleDimShadow != -1 &&
        parameters->settings->singleDimShadow != modelIndex) {
      continue;
    }

    int groupMasterIndex = model->r.groupMasterIndex;
    if (groupMasterIndex == -1 || (model->r.flags4 & 0x10u) == 0) {
      const approximateLighting_t& lighting =
          model->approximateLighting[model->CurrentApproximateLighting()];
      const idVec4& direction = lighting.primeLightDir;
      if ((direction.x != 0.0f || direction.y != 0.0f ||
           direction.z != 0.0f) &&
          (lighting.lightingModel.x == 1.0f ||
           lighting.lightColor[0].w <= 0.94999999f)) {
        if (numShadowModels < kMaximumDimShadowModels) {
          shadowModels[numShadowModels++] = model;
        } else {
          renderLog.OpenBlock("More than %d models with dim-shadows");
        }
      }
    } else {
      const idRenderModelCommitted* current =
          parameters->committedRenderModels[groupMasterIndex];
      while (current != nullptr) {
        const int nextMaster = current->r.groupMasterIndex;
        if (nextMaster < 0)
          break;
        groupMasterIndex = nextMaster;
        current = parameters->committedRenderModels[nextMaster];
      }
      parameters->dimShadowModelSlaves[modelIndex] =
          parameters->dimShadowModelSlaves[groupMasterIndex];
      parameters->dimShadowModelSlaves[groupMasterIndex] =
          static_cast<std::int16_t>(modelIndex);
    }
  }

  const float maximumDepth =
      ProjectViewDepth(parameters->renderView->projectionMatrix,
                       parameters->settings->dimShadowMaxVisibleRange);
  const float fadeDepth =
      ProjectViewDepth(parameters->renderView->projectionMatrix,
                       parameters->settings->dimShadowFadeVisibilityRange);
  const idRenderView& view = *parameters->renderView;
  GL_Viewport(view.viewport.x1, view.viewport.y1,
              view.viewport.x2 - view.viewport.x1 + 1,
              view.viewport.y2 - view.viewport.y1 + 1);
  GL_Scissor(view.scissorRect.x1, view.scissorRect.y1,
             view.scissorRect.x2 - view.scissorRect.x1 + 1,
             view.scissorRect.y2 - view.scissorRect.y1 + 1);
  renderThreadParmState->SetParmSampler(parameters->rpViewColorMap,
                                        AsSampler(parameters->imgViewColor));
  renderThreadParmState->SetFragmentPositionToViewTexture(
      view.renderWidth, view.renderHeight, view.windowWidth, view.windowHeight);
  GL_BeginQueryBatch();

  int renderedDimShadows = 0;
  for (int shadowIndex = 0; shadowIndex < numShadowModels; ++shadowIndex) {
    const idRenderModelCommitted* model = shadowModels[shadowIndex];
    const approximateLighting_t& lighting =
        model->approximateLighting[model->CurrentApproximateLighting()];
    const idVec3 primeLightDirection{lighting.primeLightDir.x,
                                     lighting.primeLightDir.y,
                                     lighting.primeLightDir.z};
    const bool upwardShadow = primeLightDirection.z < 0.0f;
    const float directionSign = upwardShadow ? -1.0f : 1.0f;
    if (upwardShadow && (model->r.flags0 & 0x02u) == 0)
      continue;

    float clipZ = 0.0f;
    if (model->r.dimShadowClipZ == 0.0f) {
      const float minimumZ = model->globalReferenceBounds.b[0].z;
      const float maximumZ = model->globalReferenceBounds.b[1].z;
      if (!upwardShadow) {
        clipZ = minimumZ - (maximumZ - minimumZ);
        if (clipZ > maximumZ)
          continue;
        if (clipZ + 1.0f > minimumZ)
          clipZ = minimumZ - 1.0f;
      } else {
        clipZ = maximumZ + (maximumZ - minimumZ);
        if (minimumZ > clipZ)
          continue;
        if (maximumZ > clipZ - 1.0f)
          clipZ = maximumZ + 1.0f;
      }
    } else {
      clipZ = model->r.origin.z + model->r.dimShadowClipZ * directionSign;
    }
    if ((!upwardShadow && primeLightDirection.z < FLT_MIN) ||
        (upwardShadow && primeLightDirection.z > -FLT_MIN)) {
      continue;
    }

    occlusionState_t* viewOcclusionState =
        static_cast<occlusionState_t*>(view.occlusionState);
    const int modelIndex = static_cast<int>(model->index.index >> 8);
    if (viewOcclusionState->modelVisibleFrameNumber[modelIndex] <=
        viewOcclusionState->renderFrameNumber) {
      SetupModelMVPMatrix(&const_cast<idRenderModelCommitted*>(model)->mvpMatrix,
                          &model->r, &model->modelMatrix, &view,
                          parameters->settings->viewNearZ,
                          parameters->settings->viewFarZ,
                          parameters->settings->guiOriginOffset);
    }

    const idVec3 worldExtrudeDirection = Normalize(-primeLightDirection);
    idVec3 localExtrudeDirection{
        worldExtrudeDirection.x * model->inverseModelMatrix.m[0] +
            worldExtrudeDirection.y * model->inverseModelMatrix.m[1] +
            worldExtrudeDirection.z * model->inverseModelMatrix.m[2],
        worldExtrudeDirection.x * model->inverseModelMatrix.m[4] +
            worldExtrudeDirection.y * model->inverseModelMatrix.m[5] +
            worldExtrudeDirection.z * model->inverseModelMatrix.m[6],
        worldExtrudeDirection.x * model->inverseModelMatrix.m[8] +
            worldExtrudeDirection.y * model->inverseModelMatrix.m[9] +
            worldExtrudeDirection.z * model->inverseModelMatrix.m[10]};
    localExtrudeDirection = Normalize(localExtrudeDirection);
    const idPlane worldClipPlane{0.0f, 0.0f, -1.0f, clipZ};
    idPlane localClipPlane{};
    model->modelMatrix.InverseTransformPlane(worldClipPlane, localClipPlane,
                                             true);
    std::uint8_t cullBits = 0;
    if (idRenderMatrix::CullExtrudedBoundsToMVPbits(
            model->mvpMatrix, model->referenceBounds, localExtrudeDirection,
            localClipPlane, cullBits, false)) {
      continue;
    }

    float depthMinimum = 0.0f;
    float depthMaximum = 0.0f;
    idRenderMatrix::DepthBoundsForExtrudedBounds(
        depthMinimum, depthMaximum, model->mvpMatrix, model->referenceBounds,
        localExtrudeDirection, localClipPlane, true);
    if (!parameters->settings->dimShadowSkipRangeCulling &&
        (model->r.flags1 & 0x08u) == 0 && depthMinimum > maximumDepth) {
      continue;
    }
    if (parameters->settings->showDimShadows != 0)
      parameters->modelDimShadowState[modelIndex] = 1;

    idRenderMatrix boundedMvp{};
    idRenderMatrix::OffsetScaleForBounds(model->mvpMatrix,
                                         model->referenceBounds, boundedMvp);
    renderThreadParmState->SetMVPMatrix(boundedMvp);
    idRenderMatrix boundedModel{};
    idRenderMatrix boundedInverseModel{};
    idRenderMatrix::OffsetScaleForBounds(model->modelMatrix,
                                         model->referenceBounds,
                                         boundedModel);
    idRenderMatrix::InverseOffsetScaleForBounds(
        model->inverseModelMatrix, model->referenceBounds,
        boundedInverseModel);
    idVec3 boundedLightDirection{
        worldExtrudeDirection.x * boundedInverseModel.m[0] +
            worldExtrudeDirection.y * boundedInverseModel.m[1] +
            worldExtrudeDirection.z * boundedInverseModel.m[2],
        worldExtrudeDirection.x * boundedInverseModel.m[4] +
            worldExtrudeDirection.y * boundedInverseModel.m[5] +
            worldExtrudeDirection.z * boundedInverseModel.m[6],
        worldExtrudeDirection.x * boundedInverseModel.m[8] +
            worldExtrudeDirection.y * boundedInverseModel.m[9] +
            worldExtrudeDirection.z * boundedInverseModel.m[10]};
    boundedLightDirection = Normalize(boundedLightDirection);

    idPlane boundedClipPlane{
        -boundedModel.m[8] + boundedModel.m[12] * clipZ,
        -boundedModel.m[9] + boundedModel.m[13] * clipZ,
        -boundedModel.m[10] + boundedModel.m[14] * clipZ,
        -boundedModel.m[11] + boundedModel.m[15] * clipZ};
    const float clipScale = InverseLength(
        boundedClipPlane.a, boundedClipPlane.b, boundedClipPlane.c);
    boundedClipPlane.a *= clipScale;
    boundedClipPlane.b *= clipScale;
    boundedClipPlane.c *= clipScale;
    boundedClipPlane.d *= clipScale;
    renderThreadParmState->SetParmVec3(parameters->rpDimShadowExtrude,
                                       boundedLightDirection);
    renderThreadParmState->SetParmVector(parameters->rpDimShadowClipPlane,
                                         &boundedClipPlane.a);
    renderLog.OpenBlock("dimShadow test %i:%s");

    if (parameters->settings->dimShadowDepthBoundsTest)
      GL_DepthBoundsTest(depthMinimum, depthMaximum);
    int queryResult = 0;
    if (parameters->settings->lockView) {
      queryResult = parameters->occlusionState
                        ->modelOcclusionQueryResult[modelIndex];
    } else {
      const std::int64_t queryNumber =
          parameters->occlusionState->modelOcclusionQuery[modelIndex];
      if (queryNumber >= view.firstOcclusionQueryInPreviousFrame)
        queryResult = GL_GetDeferredQueryResult(queryNumber);
      parameters->occlusionState->modelOcclusionQueryResult[modelIndex] =
          queryResult;
    }
    GL_BeginQuery(
        &parameters->occlusionState->modelOcclusionQuery[modelIndex]);
    const idODSObject<idDeclRenderProg> debugProgram{
        parameters->progDimShadowDebugDraw};
    const idODSObject<idTriangles> extrudeBox{parameters->extrudeBox};
    if (depthMinimum < 0.1f) {
      GL_DrawElements(debugProgram, extrudeBox,
                      UINT64_C(0x8000440FF01027C0), false);
    }
    GL_DrawElements(debugProgram, extrudeBox,
                    UINT64_C(0x8000440FF01107C0), false);
    GL_EndQuery();
    if (parameters->settings->dimShadowDepthBoundsTest)
      GL_DepthBoundsTest(0.0f, 0.0f);

    if (queryResult <= 0 && depthMinimum >= 0.1f &&
        parameters->settings->dimShadowUseQuery &&
        (model->r.flags3 & 0x40u) == 0) {
      continue;
    }

    ++renderedDimShadows;
    bool highQuality = parameters->settings->dimShadowForceHighQuality > 0;
    int shadowResolution = parameters->settings->dimShadowResolution;
    if (parameters->settings->dimShadowLixelScale > 0.0f) {
      const idVec3 center{
          (model->globalReferenceBounds.b[0].x +
           model->globalReferenceBounds.b[1].x) *
              0.5f,
          (model->globalReferenceBounds.b[0].y +
           model->globalReferenceBounds.b[1].y) *
              0.5f,
          (model->globalReferenceBounds.b[0].z +
           model->globalReferenceBounds.b[1].z) *
              0.5f};
      const idVec3 radiusVector = model->globalReferenceBounds.b[1] - center;
      const float radius =
          std::sqrt(std::max(radiusVector.LengthSqr(), FLT_MIN));
      const float lodDistance = ComputeShadowLODDistance(
          model->globalReferenceBounds, view.r.vieworg, primeLightDirection);
      const idRenderMatrix& projection = view.projectionMatrix;
      const float projectedSize =
          (radius * projection.m[0] - lodDistance * projection.m[2] +
           projection.m[3]) /
          (radius * projection.m[8] - lodDistance * projection.m[10] +
           projection.m[11]);
      if (projectedSize < 0.02f) {
        renderLog.OpenBlock("screenLen culled model dimShadow %i:%s\n");
        continue;
      }
      const int densityResolution = static_cast<int>(
          radius * parameters->settings->dimShadowMaxLixelsPerUnit);
      const int screenResolution = static_cast<int>(
          static_cast<float>(view.renderWidth) *
          parameters->settings->dimShadowLixelScale * 0.5f * projectedSize);
      shadowResolution = std::min(densityResolution, screenResolution) & ~1;
      if (shadowResolution > parameters->settings->dimShadowResolutionCap) {
        shadowResolution = parameters->settings->dimShadowResolutionCap;
        highQuality =
            highQuality ||
            (model->r.castDimShadows == static_cast<dimShadow_t>(2) &&
             (model->r.flags0 & 0x01u) != 0);
      }
    }

    idRenderDestination* shadowDestination = parameters->renderDestDimShadow;
    renderThreadParmState->SetParmSampler(
        parameters->rpDimShadowDepthMap,
        AsSampler(shadowDestination->depthImage));
    renderLog.OpenBlock("dimShadow draw %i:%s");
    if (parameters->settings->showDimShadows != 0)
      parameters->modelDimShadowState[modelIndex] = 2;
    const int destinationResolution = shadowDestination->targetWidth;
    if (destinationResolution != parameters->settings->dimShadowResolution) {
      shadowDestination->Resize(destinationResolution, destinationResolution);
    }
    const float inverseDestinationResolution =
        1.0f / static_cast<float>(destinationResolution);
    renderThreadParmState->SetParmVector(
        parameters->rpDimShadowBufferResolution,
        static_cast<float>(destinationResolution),
        inverseDestinationResolution, 0.0f, 0.0f);

    idRenderMatrix lightMatrix{};
    CalculateLightMatrix(lightMatrix, primeLightDirection, model->modelMatrix,
                         model->referenceBounds);
    GL_SetRenderDestination({shadowDestination}, 0, 0);
    const int viewportOffset =
        (destinationResolution - shadowResolution) >> 1;
    GL_Scissor(viewportOffset, viewportOffset, shadowResolution,
               shadowResolution);
    GL_Viewport(viewportOffset, viewportOffset, shadowResolution,
                shadowResolution);
    GL_State(parameters->progDimShadowCreate->data.glState, false);
    GL_Clear(false, true, false, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    GL_PolygonOffset(parameters->settings->dimShadowPolyOfsFactor,
                     parameters->settings->dimShadowPolyOfsUnits, true);

    int renderModelIndex = modelIndex;
    while (renderModelIndex != -1) {
      const idRenderModelCommitted* renderModel =
          parameters->committedRenderModels[renderModelIndex];
      idRenderMatrix creationMvp{};
      idRenderMatrix::Multiply(lightMatrix, renderModel->modelMatrix,
                               creationMvp);
      renderThreadParmState->SetMVPMatrix(creationMvp);
      renderThreadParmState->VirtualEvaluateParmBlock(
          renderModel->renderParmBlock, renderModel->renderParmBlock);

      std::array<dimShadowSurface_t, kMaximumDimShadowSurfaces> records{};
      int numRecords = 0;
      dimShadowSurfaceBucket_t standard;
      dimShadowSurfaceBucket_t secondary;
      dimShadowSurfaceBucket_t alpha;
      dimShadowSurfaceBucket_t wound;
      for (int surfaceIndex = 0;
           surfaceIndex < renderModel->surfaces.num &&
           numRecords < kMaximumDimShadowSurfaces;
           ++surfaceIndex) {
        const idRenderModelSurface* surface =
            &renderModel->surfaces.list[surfaceIndex];
        const idMaterial* material = surface->material;
        if (material == nullptr || (material->flags0 & 0x01u) == 0)
          continue;
        dimShadowSurface_t& record = records[numRecords++];
        record.surface = surface;
        if ((renderModel->r.flags2 & 0x04u) != 0 &&
            (material->flags1 & 0x04u) != 0) {
          record.program = parameters->progDimShadowCreateWound;
          wound.Add(&record);
        } else if (surface->stMap != nullptr) {
          record.program = parameters->progDimShadowCreateSecondary;
          secondary.Add(&record);
        } else if (material->coverage == MC_PERFORATED ||
                   (material->coverage == MC_PERFORATED_FOR_MODELFADE &&
                    model->r.modelFade != 1.0f)) {
          record.material = material;
          record.program = parameters->progDimShadowCreateAlpha;
          alpha.Add(&record);
        } else {
          record.program = parameters->progDimShadowCreate;
          standard.Add(&record);
        }
      }

      const std::array<const dimShadowSurfaceBucket_t*, 4> buckets = {
          &standard, &secondary, &alpha, &wound};
      for (const dimShadowSurfaceBucket_t* bucket : buckets) {
        for (int entryIndex = 0; entryIndex < bucket->count; ++entryIndex) {
          const dimShadowSurface_t& entry = *bucket->entries[entryIndex];
          if (entry.material != nullptr) {
            renderThreadParmState->VirtualEvaluateParmBlock(
                entry.material->renderParmBlock,
                entry.material->renderParmBlock);
          }
          GL_DrawElements(idODSObject<idDeclRenderProg>{entry.program},
                          idODSObject<idRenderModelSurface>{entry.surface},
                          UINT64_C(0x1000), false);
          if (entry.material != nullptr) {
            renderThreadParmState->VirtualRollbackParmBlock(
                entry.material->renderParmBlock,
                entry.material->renderParmBlock);
          }
        }
      }
      renderThreadParmState->VirtualRollbackParmBlock(
          renderModel->renderParmBlock, renderModel->renderParmBlock);
      renderModelIndex = parameters->dimShadowModelSlaves[renderModelIndex];
    }

    GL_PolygonOffset(0.0f, 0.0f, false);
    renderLog.OpenBlock("dimShadow sample %u:%s");
    GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
    GL_Viewport(view.viewport.x1, view.viewport.y1,
                view.viewport.x2 - view.viewport.x1 + 1,
                view.viewport.y2 - view.viewport.y1 + 1);
    GL_Scissor(view.scissorRect.x1, view.scissorRect.y1,
               view.scissorRect.x2 - view.scissorRect.x1 + 1,
               view.scissorRect.y2 - view.scissorRect.y1 + 1);

    idRenderMatrix sampleTransform{};
    idRenderMatrix temporary{};
    idRenderMatrix::Multiply(lightMatrix, view.inverseViewMatrix, temporary);
    idRenderMatrix::Multiply(temporary, view.inverseProjectionMatrix,
                             sampleTransform);
    const float sampleScale =
        static_cast<float>(shadowResolution) * inverseDestinationResolution;
    for (int index = 0; index < 8; ++index)
      sampleTransform.m[index] *= sampleScale;
    idRenderMatrix windowTransform{};
    idRenderMatrix::Multiply(sampleTransform,
                             renderMatrix_windowSpaceToClipSpace,
                             windowTransform);
    renderThreadParmState->SetParmVector(
        parameters->rpWindowPosToDimShadowBlurS, &windowTransform.m[0]);
    renderThreadParmState->SetParmVector(
        parameters->rpWindowPosToDimShadowBlurT, &windowTransform.m[4]);
    renderThreadParmState->SetParmVector(
        parameters->rpWindowPosToDimShadowBlurR, &windowTransform.m[8]);
    renderThreadParmState->SetParmVector(
        parameters->rpWindowPosToDimShadowBlurQ, &windowTransform.m[12]);

    float lightFade = 1.0f - lighting.lightColor[0].w;
    if (lighting.lightingModel.x == 1.0f)
      lightFade = 1.0f;
    float shadowFade = parameters->settings->dimShadowDensity *
                       model->r.modelFade * lightFade;
    if (!parameters->settings->dimShadowSkipRangeCulling &&
        (model->r.flags1 & 0x08u) == 0) {
      const float rangeFade = std::clamp(
          1.0f - (depthMinimum - fadeDepth) /
                     (maximumDepth - fadeDepth),
          0.0f, 1.0f);
      shadowFade *= rangeFade;
    }
    renderThreadParmState->SetParmFloat(parameters->rpDimShadowFade,
                                        shadowFade);
    renderThreadParmState->SetMVPMatrix(boundedMvp);
    if (parameters->settings->dimShadowDepthBoundsTest)
      GL_DepthBoundsTest(depthMinimum, depthMaximum);

    const idDeclRenderProg* drawProgram = nullptr;
    const int forceQuality =
        parameters->settings->dimShadowForceHighQuality;
    if (!highQuality || forceQuality == -1) {
      drawProgram = parameters->progDimShadowDraw;
    } else if (forceQuality != 2 &&
               parameters->progDimShadowDrawMidQuality != nullptr) {
      drawProgram = parameters->progDimShadowDrawMidQuality;
    } else {
      drawProgram = parameters->progDimShadowDrawHighQuality;
    }
    GL_DrawElements(idODSObject<idDeclRenderProg>{drawProgram->renamed},
                    extrudeBox, 0, false);
    if (parameters->settings->dimShadowDepthBoundsTest)
      GL_DepthBoundsTest(0.0f, 0.0f);
    if (parameters->settings->showDimShadows != 0) {
      GL_DrawElements(
          idODSObject<idDeclRenderProg>{parameters->progDimShadowDebugDraw},
          extrudeBox, UINT64_C(0x800), false);
    }
  }
  *parameters->numDimShadows = renderedDimShadows;
}
