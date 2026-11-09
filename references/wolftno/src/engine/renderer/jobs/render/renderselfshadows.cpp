#include "engine/renderer/jobs/render/renderselfshadows.h"

#include "engine/models/rendermodel.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/jobs/render/renderdimshadows.h"
#include "engine/renderer/jobs/render/renderocclusion.h"
#include "engine/renderer/jobs/render/rendershadowbuffer.h"
#include "engine/renderer/jobs/render/setupmodelmvp.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/renderlog.h"
#include "engine/renderer/rendermodelcommitted.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

constexpr int kMaximumSelfShadowModels = 256;
constexpr int kMaximumSelfShadowSurfaces = 64;
constexpr float kSpotLightMinimumNear = 8.0f;

float InverseLength(float x, float y, float z) {
  return 1.0f /
         std::sqrt(std::max(x * x + y * y + z * z, FLT_MIN));
}

idRenderMatrix IdentityMatrix() {
  return {{1.0f, 0.0f, 0.0f, 0.0f,
           0.0f, 1.0f, 0.0f, 0.0f,
           0.0f, 0.0f, 1.0f, 0.0f,
           0.0f, 0.0f, 0.0f, 1.0f}};
}

idVec3 TransformPoint(const idRenderMatrix& matrix, const idVec3& point) {
  return {
      point.x * matrix.m[0] + point.y * matrix.m[1] +
          point.z * matrix.m[2] + matrix.m[3],
      point.x * matrix.m[4] + point.y * matrix.m[5] +
          point.z * matrix.m[6] + matrix.m[7],
      point.x * matrix.m[8] + point.y * matrix.m[9] +
          point.z * matrix.m[10] + matrix.m[11],
  };
}

float ProjectViewDepth(const idRenderMatrix& projection, float distance) {
  const float z = -distance;
  return (z * projection.m[10] + projection.m[11]) /
         (z * projection.m[14] + projection.m[15]);
}

int VisibleFrameCount(const idRenderView& view,
                      const idRenderModelCommitted& model) {
  const auto* occlusion =
      static_cast<const occlusionState_t*>(view.occlusionState);
  const int modelIndex = static_cast<int>(model.index.index >> 8);
  return occlusion->modelVisibleFrameNumber[modelIndex] <
                 occlusion->renderFrameNumber
             ? 0
             : occlusion->modelVisibleFrameCount[modelIndex];
}

bool IsSelfShadowVisible(const idRenderView& view,
                         const idRenderModelCommitted& model) {
  return VisibleFrameCount(view, model) > model.r.firstVisibleFrameCount + 1;
}

const idTextureSampler* AsSampler(const idImage* image) {
  return reinterpret_cast<const idTextureSampler*>(image);
}

struct selfShadowSurface_t {
  const idRenderModelSurface* surface{};
  const idMaterial* material{};
  const idDeclRenderProg* program{};
};

struct selfShadowSurfaceBucket_t {
  std::array<const selfShadowSurface_t*, kMaximumSelfShadowSurfaces> entries{};
  int count{};

  void Add(const selfShadowSurface_t* entry) {
    if (count < static_cast<int>(entries.size()))
      entries[count++] = entry;
  }
};

void DrawBucket(const selfShadowSurfaceBucket_t& bucket,
                std::uint64_t extraState) {
  for (int index = 0; index < bucket.count; ++index) {
    const selfShadowSurface_t& entry = *bucket.entries[index];
    if (entry.material != nullptr) {
      renderThreadParmState->VirtualEvaluateParmBlock(
          entry.material->renderParmBlock, entry.material->renderParmBlock);
    }
    GL_DrawElements(idODSObject<idDeclRenderProg>{entry.program},
                    idODSObject<idRenderModelSurface>{entry.surface},
                    extraState, false);
    if (entry.material != nullptr) {
      renderThreadParmState->VirtualRollbackParmBlock(
          entry.material->renderParmBlock, entry.material->renderParmBlock);
    }
  }
}

void DrawCreationSurfaces(const renderSelfShadowsParms_t& parameters,
                          const idRenderModelCommitted& renderModel,
                          const idRenderModelCommitted& shadowModel,
                          std::uint64_t extraState) {
  std::array<selfShadowSurface_t, kMaximumSelfShadowSurfaces> records{};
  int numRecords = 0;
  selfShadowSurfaceBucket_t standard;
  selfShadowSurfaceBucket_t secondary;
  selfShadowSurfaceBucket_t alpha;
  selfShadowSurfaceBucket_t wound;

  for (int surfaceIndex = 0;
       surfaceIndex < renderModel.surfaces.num &&
       numRecords < static_cast<int>(records.size());
       ++surfaceIndex) {
    const idRenderModelSurface& surface =
        renderModel.surfaces.list[surfaceIndex];
    const idMaterial* material = surface.material;
    if (material == nullptr || (material->flags0 & 0x01u) == 0)
      continue;

    selfShadowSurface_t& record = records[numRecords];
    record.surface = &surface;
    if ((renderModel.r.flags2 & 0x04u) != 0 &&
        (material->flags1 & 0x04u) != 0) {
      record.program = parameters.progSelfShadowCreateWound;
      if (record.program != nullptr) {
        wound.Add(&record);
        ++numRecords;
      }
    } else if (surface.stMap != nullptr) {
      record.material = material;
      record.program = parameters.progSelfShadowCreateSecondary;
      if (record.program != nullptr) {
        secondary.Add(&record);
        ++numRecords;
      }
    } else if (material->coverage == MC_PERFORATED_FOR_MODELFADE) {
      if (shadowModel.r.modelFade == 1.0f) {
        record.program = parameters.progSelfShadowCreate;
        standard.Add(&record);
      } else {
        record.material = material;
        record.program = parameters.progSelfShadowCreateAlpha;
        alpha.Add(&record);
      }
      ++numRecords;
    } else if (material->coverage == MC_PERFORATED) {
      record.material = material;
      record.program = parameters.progSelfShadowCreateAlpha;
      if (record.program != nullptr) {
        alpha.Add(&record);
        ++numRecords;
      }
    } else {
      record.program = parameters.progSelfShadowCreate;
      if (record.program != nullptr) {
        standard.Add(&record);
        ++numRecords;
      }
    }
  }

  DrawBucket(standard, extraState);
  DrawBucket(secondary, extraState);
  DrawBucket(alpha, extraState);
  DrawBucket(wound, extraState);
}

void DrawSampleSurfaces(const renderSelfShadowsParms_t& parameters,
                        const idRenderModelCommitted& renderModel,
                        const idRenderModelCommitted& shadowModel) {
  std::array<selfShadowSurface_t, kMaximumSelfShadowSurfaces> records{};
  int numRecords = 0;
  selfShadowSurfaceBucket_t standard;
  selfShadowSurfaceBucket_t secondary;
  selfShadowSurfaceBucket_t alpha;
  selfShadowSurfaceBucket_t wound;

  for (int surfaceIndex = 0;
       surfaceIndex < renderModel.surfaces.num &&
       numRecords < static_cast<int>(records.size());
       ++surfaceIndex) {
    const idRenderModelSurface& surface =
        renderModel.surfaces.list[surfaceIndex];
    const idMaterial* material = surface.material;
    if (material == nullptr || (material->flags0 & 0x01u) == 0 ||
        material->coverage == MC_TRANSLUCENT) {
      continue;
    }

    selfShadowSurface_t& record = records[numRecords];
    record.surface = &surface;
    if ((renderModel.r.flags2 & 0x04u) != 0 &&
        (material->flags1 & 0x04u) != 0) {
      record.program = parameters.progSelfShadowSampleWound;
      if (record.program != nullptr) {
        wound.Add(&record);
        ++numRecords;
      }
    } else if (surface.stMap != nullptr) {
      record.material = material;
      record.program = parameters.progSelfShadowSampleSecondary;
      if (record.program != nullptr) {
        secondary.Add(&record);
        ++numRecords;
      }
    } else if (material->coverage == MC_PERFORATED_FOR_MODELFADE) {
      if (shadowModel.r.modelFade == 1.0f) {
        record.program = parameters.progSelfShadowSample;
        standard.Add(&record);
      } else {
        record.material = material;
        record.program = parameters.progSelfShadowSampleAlpha;
        alpha.Add(&record);
      }
      ++numRecords;
    } else if (material->coverage == MC_PERFORATED) {
      record.material = material;
      record.program = parameters.progSelfShadowSampleAlpha;
      if (record.program != nullptr) {
        alpha.Add(&record);
        ++numRecords;
      }
    } else {
      record.program = parameters.progSelfShadowSample;
      if (record.program != nullptr) {
        standard.Add(&record);
        ++numRecords;
      }
    }
  }

  DrawBucket(standard, 0);
  DrawBucket(secondary, 0);
  DrawBucket(alpha, 0);
  DrawBucket(wound, 0);
}

}  // namespace

void SetupShadowProjection(const idVec3& localLightOrigin,
                           const idBounds& localModelBounds,
                           idRenderMatrix& shadowProjection,
                           idRenderMatrix& localShadowProjection,
                           float& depth) {
  const idVec3 center{
      (localModelBounds.b[0].x + localModelBounds.b[1].x) * 0.5f,
      (localModelBounds.b[0].y + localModelBounds.b[1].y) * 0.5f,
      (localModelBounds.b[0].z + localModelBounds.b[1].z) * 0.5f};
  idVec3 direction = localLightOrigin - center;

  idRenderMatrix localLightMatrix{};
  float minimumX = FLT_MAX;
  float minimumY = FLT_MAX;
  float minimumDepth = FLT_MAX;
  float maximumX = -FLT_MAX;
  float maximumY = -FLT_MAX;
  float maximumDepth = -FLT_MAX;

  for (int iteration = 0; iteration < 4; ++iteration) {
    const float directionScale =
        InverseLength(direction.x, direction.y, direction.z);
    direction *= directionScale;

    idVec3 vertical;
    if (std::fabs(direction.z) >= 0.99000001f) {
      const float yz = direction.z + direction.y;
      vertical = {-direction.x * yz, 1.0f - direction.y * yz,
                  1.0f - direction.z * yz};
    } else {
      vertical = {direction.z * direction.x,
                  direction.z * direction.y,
                  direction.z * direction.z - 1.0f};
    }
    vertical *= InverseLength(vertical.x, vertical.y, vertical.z);
    const idVec3 horizontal = vertical.Cross(direction);

    localLightMatrix = {{
        horizontal.x, horizontal.y, horizontal.z,
        -(horizontal * localLightOrigin),
        vertical.x, vertical.y, vertical.z,
        -(vertical * localLightOrigin),
        direction.x, direction.y, direction.z,
        -(direction * localLightOrigin),
        0.0f, 0.0f, 0.0f, 1.0f,
    }};

    minimumX = FLT_MAX;
    minimumY = FLT_MAX;
    minimumDepth = FLT_MAX;
    maximumX = -FLT_MAX;
    maximumY = -FLT_MAX;
    maximumDepth = -FLT_MAX;
    for (int corner = 0; corner < 8; ++corner) {
      const idVec3 point{
          localModelBounds.b[corner & 1].x,
          localModelBounds.b[(corner >> 1) & 1].y,
          localModelBounds.b[(corner >> 2) & 1].z};
      const idVec3 projected = TransformPoint(localLightMatrix, point);
      const float viewDepth = -projected.z;
      if (viewDepth >= kSpotLightMinimumNear) {
        const float inverseDepth = 1.0f / viewDepth;
        const float x = projected.x * inverseDepth;
        const float y = projected.y * inverseDepth;
        minimumX = std::min(minimumX, x);
        minimumY = std::min(minimumY, y);
        minimumDepth = std::min(minimumDepth, viewDepth);
        maximumX = std::max(maximumX, x);
        maximumY = std::max(maximumY, y);
        maximumDepth = std::max(maximumDepth, viewDepth);
      } else {
        minimumDepth = kSpotLightMinimumNear;
      }
    }

    const float xAsymmetry = std::fabs(maximumX + minimumX);
    const float yAsymmetry = std::fabs(maximumY + minimumY);
    if (std::max(xAsymmetry, yAsymmetry) > 0.99f)
      break;

    const float horizontalCorrection =
        0.353553385f * (maximumX + minimumX);
    const float verticalCorrection =
        0.353553385f * (maximumY + minimumY);
    direction -= horizontal * horizontalCorrection;
    direction -= vertical * verticalCorrection;
  }

  GetSpotShadowProjectionMatrix(
      minimumDepth, maximumDepth,
      std::max(-minimumX, maximumX),
      std::max(-minimumY, maximumY), shadowProjection);
  idRenderMatrix::Multiply(shadowProjection, localLightMatrix,
                           localShadowProjection);
  depth = maximumDepth - minimumDepth;
}

void RenderSelfShadows(const renderSelfShadowsParms_t* parameters) {
  const renderSettings_t& settings = *parameters->settings;
  const idRenderView& view = *parameters->renderView;
  const int halfResolution = settings.selfShadowResolution / 2;
  if (parameters->renderDestSelfShadow->targetWidth !=
      settings.selfShadowResolution) {
    parameters->renderDestSelfShadow->Resize(settings.selfShadowResolution,
                                             settings.selfShadowResolution);
  }
  renderThreadParmState->SetParmVector(
      parameters->rpShadowBufferResolution,
      static_cast<float>(settings.selfShadowResolution),
      1.0f / static_cast<float>(settings.selfShadowResolution), 0.5f,
      0.0f);

  std::memset(parameters->selfShadowModelSlaves, 0xFF, 0x4000);
  std::array<const idRenderModelCommitted*, kMaximumSelfShadowModels>
      shadowModels{};
  int numShadowModels = 0;
  for (const idRenderModelCommitted* model = parameters->dimShadowModels;
       model != nullptr; model = model->nextDimShadowModel) {
    if (model->references.num <= 0)
      continue;
    if (model->r.suppressShadowInViewID != 0 &&
        model->r.suppressShadowInViewID == view.r.viewID) {
      continue;
    }
    const int modelIndex = static_cast<int>(model->index.index >> 8);
    if (settings.singleSelfShadow != -1 &&
        settings.singleSelfShadow != modelIndex) {
      continue;
    }

    int groupMasterIndex = model->r.groupMasterIndex;
    if (groupMasterIndex == -1) {
      const approximateLighting_t& lighting =
          model->approximateLighting[model->CurrentApproximateLighting()];
      if (lighting.lightingModel.x == 1.0f &&
          lighting.lightingModel.y == 1.0f &&
          IsSelfShadowVisible(view, *model)) {
        if (numShadowModels < static_cast<int>(shadowModels.size())) {
          shadowModels[numShadowModels++] = model;
        } else {
          renderLog.OpenBlock("More than %d models with self-shadows", 256);
        }
      }
    } else {
      const idRenderModelCommitted* master =
          parameters->committedRenderModels[groupMasterIndex];
      while (master != nullptr) {
        const int nextMaster = master->r.groupMasterIndex;
        if (nextMaster < 0)
          break;
        groupMasterIndex = nextMaster;
        master = parameters->committedRenderModels[nextMaster];
      }
      SetupModelMVPMatrix(
          &const_cast<idRenderModelCommitted*>(model)->mvpMatrix, &model->r,
          &model->modelMatrix, &view, settings.viewNearZ, settings.viewFarZ,
          settings.guiOriginOffset);
      parameters->selfShadowModelSlaves[modelIndex] =
          parameters->selfShadowModelSlaves[groupMasterIndex];
      parameters->selfShadowModelSlaves[groupMasterIndex] =
          static_cast<std::int16_t>(modelIndex);
    }
  }

  if (numShadowModels <= 0)
    return;

  GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
  GL_Viewport(view.viewport.x1, view.viewport.y1,
              view.viewport.x2 - view.viewport.x1 + 1,
              view.viewport.y2 - view.viewport.y1 + 1);
  GL_Scissor(view.scissorRect.x1, view.scissorRect.y1,
             view.scissorRect.x2 - view.scissorRect.x1 + 1,
             view.scissorRect.y2 - view.scissorRect.y1 + 1);
  GL_State(UINT64_C(0x4000), false);
  GL_Clear(true, false, false, 0, 1.0f, 1.0f, 1.0f, 1.0f);

  const float maximumDepth = ProjectViewDepth(
      view.projectionMatrix, settings.selfShadowMaxVisibleRange);
  const float fadeDepth = ProjectViewDepth(
      view.projectionMatrix, settings.selfShadowFadeVisibilityRange);
  int renderedSelfShadows = 0;

  for (int shadowIndex = 0; shadowIndex < numShadowModels; ++shadowIndex) {
    const idRenderModelCommitted& shadowModel = *shadowModels[shadowIndex];
    float depthMinimum = 0.0f;
    float depthMaximum = 0.0f;
    idRenderMatrix::DepthBoundsForBounds(
        depthMinimum, depthMaximum, shadowModel.mvpMatrix,
        shadowModel.referenceBounds, true);
    if (!settings.selfShadowSkipRangeCulling &&
        (shadowModel.r.flags1 & 0x08u) == 0 &&
        depthMinimum > maximumDepth) {
      continue;
    }

    int tileResolution = halfResolution;
    if (settings.selfShadowLixelScale > 0.0f) {
      const idBounds& bounds = shadowModel.globalReferenceBounds;
      const idVec3 center{(bounds.b[0].x + bounds.b[1].x) * 0.5f,
                          (bounds.b[0].y + bounds.b[1].y) * 0.5f,
                          (bounds.b[0].z + bounds.b[1].z) * 0.5f};
      const idVec3 radiusVector = bounds.b[1] - center;
      const float radius = std::sqrt(std::max(radiusVector.LengthSqr(),
                                               FLT_MIN));
      const float lodDistance = ComputeShadowLODDistance(
          bounds, view.r.vieworg, {0.0f, 0.0f, -1.0f});
      const float z = -lodDistance;
      const float screenLength =
          (radius * view.projectionMatrix.m[0] +
           z * view.projectionMatrix.m[2] + view.projectionMatrix.m[3]) /
          (radius * view.projectionMatrix.m[8] +
           z * view.projectionMatrix.m[10] + view.projectionMatrix.m[11]);
      if (screenLength < 0.02f) {
        renderLog.OpenBlock("screenLen culled model selfShadow %i:%s\n",
                            static_cast<unsigned>(shadowModel.index.index) >> 8,
                            shadowModel.name);
        continue;
      }
      tileResolution =
          static_cast<int>(static_cast<float>(view.renderWidth) *
                           settings.selfShadowLixelScale * 0.5f *
                           screenLength) &
          ~1;
      if (tileResolution > settings.selfShadowResolutionCap)
        tileResolution = settings.selfShadowResolutionCap;
    }

    ++renderedSelfShadows;
    idRenderDestination& selfShadowDestination =
        *parameters->renderDestSelfShadow;
    renderThreadParmState->SetParmSampler(
        parameters->rpLightDepthCompareMap,
        AsSampler(selfShadowDestination.depthImage));
    const int destinationResolution = selfShadowDestination.targetWidth;
    const float inverseDestinationResolution =
        1.0f / static_cast<float>(destinationResolution);
    renderLog.OpenBlock("self-shadow draw %u:%s",
                        static_cast<unsigned>(shadowModel.index.index) >> 8,
                        shadowModel.name);
    GL_SetRenderDestination({&selfShadowDestination}, 0, 0);
    GL_Viewport(0, 0, 2 * tileResolution, 2 * tileResolution);
    GL_Scissor(0, 0, 2 * tileResolution, 2 * tileResolution);
    GL_State(UINT64_C(0x4000), false);
    GL_Clear(false, true, false, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    GL_PolygonOffset(settings.selfShadowPolyOfsFactor,
                     settings.selfShadowPolyOfsUnits, true);

    std::array<float, 4> projectionDepth{};
    std::array<idRenderMatrix, 4> shadowProjection{};
    std::array<idRenderMatrix, 4> worldPosToShadowMap{};
    std::array<float, 4> selfShadowEnable{};
    const approximateLighting_t& lighting = shadowModel.approximateLighting[
        shadowModel.CurrentApproximateLighting()];
    const std::array<idVec4, 4> lightOrigins = {
        lighting.lightOrigin[0], lighting.lightOrigin[1],
        lighting.lightOrigin[2], settings.viewOrigin};
    const std::array<idVec4, 4> lightColors = {
        lighting.lightColor[0], lighting.lightColor[1],
        lighting.lightColor[2], lighting.channelLighting[0]};
    const int numProjections = settings.skipAmbientOcclusion ? 3 : 4;
    if (settings.skipAmbientOcclusion)
      shadowProjection[3] = IdentityMatrix();
    int numActiveProjections = 0;

    for (int projectionIndex = 0; projectionIndex < numProjections;
         ++projectionIndex) {
      const idVec4& lightOrigin = lightOrigins[projectionIndex];
      const idVec4& lightColor = lightColors[projectionIndex];
      if (lightColor.x + lightColor.y + lightColor.z <= FLT_EPSILON ||
          lightOrigin.w == 1.0f) {
        shadowProjection[projectionIndex] = IdentityMatrix();
        continue;
      }

      selfShadowEnable[projectionIndex] = 1.0f;
      ++numActiveProjections;
      idVec3 localLightOrigin = TransformPoint(
          shadowModel.inverseModelMatrix,
          {lightOrigin.x, lightOrigin.y, lightOrigin.z});
      idBounds expandedBounds = shadowModel.referenceBounds;
      expandedBounds.b[0].x -= 4.0f;
      expandedBounds.b[0].y -= 4.0f;
      expandedBounds.b[0].z -= 4.0f;
      expandedBounds.b[1].x += 4.0f;
      expandedBounds.b[1].y += 4.0f;
      expandedBounds.b[1].z += 4.0f;
      if (localLightOrigin.x >= expandedBounds.b[0].x &&
          localLightOrigin.x <= expandedBounds.b[1].x &&
          localLightOrigin.y >= expandedBounds.b[0].y &&
          localLightOrigin.y <= expandedBounds.b[1].y &&
          localLightOrigin.z >= expandedBounds.b[0].z &&
          localLightOrigin.z <= expandedBounds.b[1].z) {
        const idVec3 expandedCenter{
            (expandedBounds.b[0].x + expandedBounds.b[1].x) * 0.5f,
            (expandedBounds.b[0].y + expandedBounds.b[1].y) * 0.5f,
            (expandedBounds.b[0].z + expandedBounds.b[1].z) * 0.5f};
        idVec3 rayDirection = expandedCenter - localLightOrigin;
        rayDirection *= InverseLength(rayDirection.x, rayDirection.y,
                                      rayDirection.z);
        float scale1 = 0.0f;
        float scale2 = 0.0f;
        if (expandedBounds.RayIntersection(localLightOrigin, rayDirection,
                                           scale1, scale2)) {
          localLightOrigin += rayDirection * scale1;
        }
      }

      idRenderMatrix localShadowProjection{};
      SetupShadowProjection(localLightOrigin, shadowModel.referenceBounds,
                            shadowProjection[projectionIndex],
                            localShadowProjection,
                            projectionDepth[projectionIndex]);
      idRenderMatrix worldShadowProjection{};
      idRenderMatrix::Multiply(localShadowProjection,
                               shadowModel.inverseModelMatrix,
                               worldShadowProjection);
      const int x = tileResolution * (projectionIndex & 1);
      const int y = tileResolution * ((projectionIndex >> 1) & 1);
      const float atlasScale =
          static_cast<float>(tileResolution >> 1) *
          inverseDestinationResolution;
      SetupWorldPosToShadowMap(
          worldShadowProjection, atlasScale,
          static_cast<float>(x) * inverseDestinationResolution + atlasScale,
          static_cast<float>(y) * inverseDestinationResolution + atlasScale,
          worldPosToShadowMap[projectionIndex]);
      GL_Viewport(x, y, tileResolution, tileResolution);
      GL_Scissor(x, y, tileResolution, tileResolution);
      GL_State(parameters->progSelfShadowCreate->data.glState, false);

      const std::uint64_t extraState =
          projectionIndex == 3 ? 0 : UINT64_C(0x1000);
      int renderModelIndex =
          static_cast<int>(shadowModel.index.index >> 8);
      while (renderModelIndex != -1) {
        const idRenderModelCommitted& renderModel =
            *parameters->committedRenderModels[renderModelIndex];
        idRenderMatrix creationMvp{};
        idRenderMatrix::Multiply(worldShadowProjection,
                                 renderModel.modelMatrix, creationMvp);
        renderThreadParmState->SetMVPMatrix(creationMvp);
        renderThreadParmState->VirtualEvaluateParmBlock(
            renderModel.renderParmBlock, renderModel.renderParmBlock);
        DrawCreationSurfaces(*parameters, renderModel, shadowModel,
                             extraState);
        renderThreadParmState->VirtualRollbackParmBlock(
            renderModel.renderParmBlock, renderModel.renderParmBlock);
        renderModelIndex =
            parameters->selfShadowModelSlaves[renderModelIndex];
      }
    }

    GL_PolygonOffset(0.0f, 0.0f, false);
    GL_State(UINT64_C(0x4000), false);
    if (numActiveProjections <= 0) {
      renderLog.OpenBlock("self-shadow sample %u:%s skipped\n",
                          static_cast<unsigned>(shadowModel.index.index) >> 8,
                          shadowModel.name);
      continue;
    }

    renderLog.OpenBlock("self-shadow sample %u:%s",
                        static_cast<unsigned>(shadowModel.index.index) >> 8,
                        shadowModel.name);
    GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
    GL_Viewport(view.viewport.x1, view.viewport.y1,
                view.viewport.x2 - view.viewport.x1 + 1,
                view.viewport.y2 - view.viewport.y1 + 1);
    GL_Scissor(view.scissorRect.x1, view.scissorRect.y1,
               view.scissorRect.x2 - view.scissorRect.x1 + 1,
               view.scissorRect.y2 - view.scissorRect.y1 + 1);

    float shadowFade = settings.selfShadowDensity * shadowModel.r.modelFade;
    if (!settings.selfShadowSkipRangeCulling &&
        (shadowModel.r.flags1 & 0x08u) == 0) {
      float rangeFade =
          1.0f - (depthMinimum - fadeDepth) / (maximumDepth - fadeDepth);
      if (rangeFade >= 0.0f) {
        if (rangeFade > 1.0f)
          rangeFade = 1.0f;
      } else {
        rangeFade = 0.0f;
      }
      shadowFade *= rangeFade;
    }
    renderThreadParmState->SetParmFloat(parameters->rpSelfShadowFade,
                                        shadowFade);
    renderThreadParmState->SetParmVector(parameters->rpSelfShadowEnable,
                                         selfShadowEnable.data());

    for (int projectionIndex = 0; projectionIndex < 4;
         ++projectionIndex) {
      idRenderMatrix inverseProjection{};
      idRenderMatrix::Inverse(shadowProjection[projectionIndex],
                              inverseProjection);
      renderThreadParmState->SetParmVector(
          parameters->rpInvShadowProjX[projectionIndex],
          &inverseProjection.m[0]);
      renderThreadParmState->SetParmVector(
          parameters->rpInvShadowProjY[projectionIndex],
          &inverseProjection.m[4]);
      renderThreadParmState->SetParmVector(
          parameters->rpInvShadowProjZ[projectionIndex],
          &inverseProjection.m[8]);
      renderThreadParmState->SetParmVector(
          parameters->rpInvShadowProjW[projectionIndex],
          &inverseProjection.m[12]);
    }

    int renderModelIndex =
        static_cast<int>(shadowModel.index.index >> 8);
    while (renderModelIndex != -1) {
      const idRenderModelCommitted& renderModel =
          *parameters->committedRenderModels[renderModelIndex];
      if (IsSelfShadowVisible(view, renderModel)) {
        for (int projectionIndex = 0; projectionIndex < 4;
             ++projectionIndex) {
          idRenderMatrix modelToShadow{};
          if (selfShadowEnable[projectionIndex] != 0.0f) {
            idRenderMatrix::Multiply(worldPosToShadowMap[projectionIndex],
                                     renderModel.modelMatrix, modelToShadow);
          } else {
            modelToShadow.m[12] =
                static_cast<float>(destinationResolution - 1) *
                inverseDestinationResolution;
            modelToShadow.m[13] = modelToShadow.m[12];
            modelToShadow.m[15] = 1.0f;
          }
          renderThreadParmState->SetParmVector(
              parameters->rpModelPosToShadowMapS[projectionIndex],
              &modelToShadow.m[0]);
          renderThreadParmState->SetParmVector(
              parameters->rpModelPosToShadowMapT[projectionIndex],
              &modelToShadow.m[4]);
          renderThreadParmState->SetParmVector(
              parameters->rpModelPosToShadowMapR[projectionIndex],
              &modelToShadow.m[8]);
          renderThreadParmState->SetParmVector(
              parameters->rpModelPosToShadowMapQ[projectionIndex],
              &modelToShadow.m[12]);
        }
        renderThreadParmState->SetParmVector(
            parameters->rpShadowProjectionDepth, projectionDepth.data());
        renderThreadParmState->SetMVPMatrix(renderModel.mvpMatrix);
        renderThreadParmState->VirtualEvaluateParmBlock(
            renderModel.renderParmBlock, renderModel.renderParmBlock);
        DrawSampleSurfaces(*parameters, renderModel, shadowModel);
        renderThreadParmState->VirtualRollbackParmBlock(
            renderModel.renderParmBlock, renderModel.renderParmBlock);
      }
      renderModelIndex =
          parameters->selfShadowModelSlaves[renderModelIndex];
    }
  }

  *parameters->numSelfShadows = renderedSelfShadows;
  RenderCapture(&const_cast<renderSelfShadowsParms_t*>(parameters)->captureParms);
  renderThreadParmState->SetParmSampler(
      parameters->rpLightDepthCompareMap,
      AsSampler(parameters->renderDestSelfShadow->depthImage));
  renderThreadParmState->SetParmSampler(
      parameters->rpSelfShadowMap,
      AsSampler(parameters->captureParms.renderDestViewColor->targetImage[0]));
}
