#include "engine/renderer/jobs/render/rendershadowbuffer.h"

#include "engine/models/rendermodel.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/jobs/render/renderocclusion.h"
#include "engine/renderer/jobs/render/setupmodelmvp.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/renderlightcommitted.h"
#include "engine/renderer/rendermodelcommitted.h"
#include "engine/renderer/renderlog.h"
#include "engine/renderer/renderworldcommitted.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <utility>

namespace {

constexpr idRenderMatrix kFlipToOpenGL =
    {{0.0f, -1.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 1.0f, 0.0f,
      -1.0f, 0.0f, 0.0f, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f}};
constexpr std::array<int, 4> kParallelSlotX = {0, 1, 0, 1};
constexpr std::array<int, 4> kParallelSlotY = {0, 0, 1, 1};
constexpr std::array<int, 6> kPointSlotX = {0, 1, 0, 1, 0, 1};
constexpr std::array<int, 6> kPointSlotY = {0, 0, 1, 1, 2, 2};
constexpr idBounds kUnitCube =
    {{{-1.0f, -1.0f, -1.0f}, {1.0f, 1.0f, 1.0f}}};

float InverseLength(float x, float y, float z) {
  return 1.0f /
         std::sqrt(std::max(x * x + y * y + z * z, FLT_MIN));
}

idRenderMatrix TranslationMatrix(const idVec3& translation) {
  return {{1.0f, 0.0f, 0.0f, translation.x,
           0.0f, 1.0f, 0.0f, translation.y,
           0.0f, 0.0f, 1.0f, translation.z,
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

bool IsPrivateLight(lightClass_t lightClass) {
  return lightClass == LIGHT_PRIVATE_DYNAMIC_ONLY ||
         lightClass == LIGHT_PRIVATE_PRIME ||
         lightClass == LIGHT_SLOWLIGHT_PRIVATE_ONLY;
}

void SwapCaster(shadowCaster_t& left, shadowCaster_t& right,
                std::uint32_t& leftDistance,
                std::uint32_t& rightDistance) {
  std::swap(left, right);
  std::swap(leftDistance, rightDistance);
}

float CasterDistance(const idRenderModelCommitted& model,
                     const idBounds& bounds,
                     const idVec3& globalLightOrigin) {
  const idVec3 center{(bounds.b[0].x + bounds.b[1].x) * 0.5f,
                      (bounds.b[0].y + bounds.b[1].y) * 0.5f,
                      (bounds.b[0].z + bounds.b[1].z) * 0.5f};
  const idVec3 localLightOrigin =
      TransformPoint(model.inverseModelMatrix, globalLightOrigin);
  idVec3 direction = center - localLightOrigin;
  direction *= InverseLength(direction.x, direction.y, direction.z);
  const idVec3 extents = bounds.b[1] - center;
  return std::fabs(extents.y * direction.y) +
         std::fabs(extents.x * direction.x) +
         std::fabs(extents.z * direction.z) + direction * center;
}

}  // namespace

void GetPointLightViewMatrix(int side, idRenderMatrix& lightViewMatrix) {
  idRenderMatrix baseMatrix{};
  baseMatrix.m[15] = 1.0f;
  switch (side) {
    case 0:
      baseMatrix.m[0] = 1.0f;
      baseMatrix.m[6] = -1.0f;
      baseMatrix.m[9] = 1.0f;
      break;
    case 1:
      baseMatrix.m[0] = -1.0f;
      baseMatrix.m[6] = -1.0f;
      baseMatrix.m[9] = 1.0f;
      break;
    case 2:
      baseMatrix.m[1] = 1.0f;
      baseMatrix.m[4] = -1.0f;
      baseMatrix.m[10] = 1.0f;
      break;
    case 3:
      baseMatrix.m[1] = -1.0f;
      baseMatrix.m[4] = -1.0f;
      baseMatrix.m[10] = 1.0f;
      break;
    case 4:
      baseMatrix.m[2] = 1.0f;
      baseMatrix.m[4] = -1.0f;
      baseMatrix.m[9] = 1.0f;
      break;
    case 5:
      baseMatrix.m[2] = -1.0f;
      baseMatrix.m[4] = -1.0f;
      baseMatrix.m[9] = 1.0f;
      break;
    default:
      break;
  }
  idRenderMatrix::Multiply(kFlipToOpenGL, baseMatrix, lightViewMatrix);
}

void GetSpotLightViewMatrix(const idVec3& lightTarget,
                            const idVec3& lightRight,
                            const idVec3& lightUp,
                            idRenderMatrix& lightViewMatrix) {
  const float targetScale =
      InverseLength(lightTarget.x, lightTarget.y, lightTarget.z);
  const float rightScale =
      InverseLength(lightRight.x, lightRight.y, lightRight.z);
  const float upScale = InverseLength(lightUp.x, lightUp.y, lightUp.z);
  const idRenderMatrix baseMatrix = {{
      lightTarget.x * targetScale, lightTarget.y * targetScale,
      lightTarget.z * targetScale, 0.0f,
      -lightRight.x * rightScale, -lightRight.y * rightScale,
      -lightRight.z * rightScale, 0.0f,
      lightUp.x * upScale, lightUp.y * upScale, lightUp.z * upScale, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f,
  }};
  idRenderMatrix::Multiply(kFlipToOpenGL, baseMatrix, lightViewMatrix);
}

void GetParallelLightViewMatrix(const idVec3& direction,
                                idRenderMatrix& lightViewMatrix) {
  const float directionScale =
      InverseLength(direction.x, direction.y, direction.z);
  const idVec3 normalized = direction * directionScale;
  float referenceY = 0.0f;
  float referenceZ = -1.0f;
  float projection = -normalized.z;
  if (std::fabs(projection) > 0.99000001f) {
    referenceY = 1.0f;
    referenceZ = 1.0f;
    projection = normalized.y + normalized.z;
  }
  idVec3 vertical{-normalized.x * projection,
                  referenceY - normalized.y * projection,
                  referenceZ - normalized.z * projection};
  vertical *= InverseLength(vertical.x, vertical.y, vertical.z);
  const idVec3 horizontal = normalized.Cross(vertical);
  const idRenderMatrix baseMatrix = {{
      normalized.x, normalized.y, normalized.z, 0.0f,
      horizontal.x, horizontal.y, horizontal.z, 0.0f,
      vertical.x, vertical.y, vertical.z, 0.0f,
      0.0f, 0.0f, 0.0f, 1.0f,
  }};
  idRenderMatrix::Multiply(kFlipToOpenGL, baseMatrix, lightViewMatrix);
}

void GetSpotShadowProjectionMatrix(float zNear, float zFar, float tanFovX,
                                   float tanFovY,
                                   idRenderMatrix& projectionMatrix) {
  (void)zFar;
  projectionMatrix = {};
  projectionMatrix.m[0] =
      (zNear * 2.0f) / ((zNear * tanFovX) - -(zNear * tanFovX));
  projectionMatrix.m[5] =
      (zNear * 2.0f) / ((zNear * tanFovY) - -(zNear * tanFovY));
  projectionMatrix.m[10] = -1.0f;
  projectionMatrix.m[11] = zNear * -2.0f;
  projectionMatrix.m[14] = -1.0f;
}

void SetupWorldPosToShadowMap(const idRenderMatrix& shadowProjection,
                              float scale, float sBias, float tBias,
                              idRenderMatrix& worldPosToShadowMap) {
  for (int index = 0; index < 4; ++index) {
    worldPosToShadowMap.m[index] =
        scale * shadowProjection.m[index] +
        sBias * shadowProjection.m[12 + index];
    worldPosToShadowMap.m[4 + index] =
        scale * shadowProjection.m[4 + index] +
        tBias * shadowProjection.m[12 + index];
    worldPosToShadowMap.m[8 + index] =
        (shadowProjection.m[8 + index] +
         shadowProjection.m[12 + index]) *
        0.5f;
    worldPosToShadowMap.m[12 + index] =
        shadowProjection.m[12 + index];
  }
}

int SetupShadowBufferMatrices(const idRenderLightParms& lightParms,
                              const idBounds& lightBounds,
                              const idVec3& viewOrigin,
                              const shadowBufferOptions_t& options,
                              idRenderMatrix* shadowViewProjection,
                              lightShadow_t& lightShadow) {
  int shadowBufferSize = options.shadowBufferSize;
  if (lightParms.lightType == LIGHT_POINT)
    shadowBufferSize /= 3;
  else if (lightParms.lightType == LIGHT_PARALLEL)
    shadowBufferSize /= 2;

  std::memset(shadowViewProjection, 0, 6 * sizeof(idRenderMatrix));
  lightShadow = {};

  if (lightParms.lightType == LIGHT_POINT) {
    idRenderMatrix lightTransform{};
    idRenderMatrix inverseLightTransform{};
    idRenderMatrix::CreateFromOriginAxis(lightParms.origin, lightParms.axis,
                                         lightTransform);
    idRenderMatrix::Inverse(lightTransform, inverseLightTransform);
    const idRenderMatrix lightCenter =
        TranslationMatrix(-lightParms.lightCenter);
    idRenderMatrix baseLightTransform{};
    idRenderMatrix::Multiply(lightCenter, inverseLightTransform,
                             baseLightTransform);

    const float tanHalfFov = std::tan(3.1415927f * 93.0f / 360.0f);
    idRenderMatrix projection{};
    GetSpotShadowProjectionMatrix(8.0f, 0.0f, tanHalfFov, tanHalfFov,
                                  projection);
    for (int side = 0; side < 6; ++side) {
      idRenderMatrix lightView{};
      idRenderMatrix sideTransform{};
      GetPointLightViewMatrix(side, lightView);
      idRenderMatrix::Multiply(lightView, baseLightTransform,
                               sideTransform);
      idRenderMatrix::Multiply(projection, sideTransform,
                               shadowViewProjection[side]);
    }
    lightShadow.worldPosToShadowMap = baseLightTransform;
    SetupWorldPosToShadowMap(projection, 0.5f, 0.0f, 0.0f,
                             lightShadow.shadowProjection);
    return shadowBufferSize;
  }

  idRenderMatrix lightMatrix{};
  idRenderMatrix lightView{};
  if (lightParms.lightType == LIGHT_SPOT) {
    const float targetLength = std::sqrt(std::max(
        lightParms.target.LengthSqr(), FLT_MIN));
    const idVec3 targetDirection = lightParms.target / targetLength;
    const float zNear =
        std::max(targetDirection * lightParms.start, 8.0f);
    const float rightLength = std::sqrt(std::max(
        lightParms.right.LengthSqr(), FLT_MIN));
    const float upLength =
        std::sqrt(std::max(lightParms.up.LengthSqr(), FLT_MIN));

    idRenderMatrix::CreateFromOriginAxis(lightParms.origin, lightParms.axis,
                                         lightMatrix);
    idRenderMatrix::Inverse(lightMatrix, lightView);
    idRenderMatrix spotView{};
    GetSpotLightViewMatrix(lightParms.target, lightParms.right,
                           lightParms.up, spotView);
    idRenderMatrix lightTransform{};
    idRenderMatrix::Multiply(spotView, lightView, lightTransform);
    idRenderMatrix projection{};
    GetSpotShadowProjectionMatrix(
        zNear, 0.0f, rightLength * 1.05f / targetLength,
        upLength * 1.05f / targetLength, projection);
    idRenderMatrix::Multiply(projection, lightTransform,
                             shadowViewProjection[0]);
    const float atlasScale =
        (static_cast<float>(shadowBufferSize) * 0.5f) /
        static_cast<float>(options.shadowBufferSize);
    SetupWorldPosToShadowMap(shadowViewProjection[0], atlasScale,
                             atlasScale, atlasScale,
                             lightShadow.worldPosToShadowMap);
    return shadowBufferSize;
  }

  const idVec3 lightDirection = -lightParms.lightCenter;
  idRenderMatrix::CreateFromOriginAxis(viewOrigin, lightParms.axis,
                                       lightMatrix);
  idRenderMatrix::Inverse(lightMatrix, lightView);
  idRenderMatrix parallelView{};
  GetParallelLightViewMatrix(lightDirection, parallelView);
  idRenderMatrix baseProjection{};
  idRenderMatrix::Multiply(parallelView, lightView, baseProjection);
  const float snapStep =
      16.0f * options.sliceStep / static_cast<float>(shadowBufferSize);
  baseProjection.m[3] =
      std::floor(baseProjection.m[3] / snapStep) * snapStep;
  baseProjection.m[7] =
      std::floor(baseProjection.m[7] / snapStep) * snapStep;
  baseProjection.m[11] =
      std::floor(baseProjection.m[11] / snapStep) * snapStep;

  float depthMinimum = 0.0f;
  float depthMaximum = 0.0f;
  idRenderMatrix::DepthBoundsForBounds(depthMinimum, depthMaximum,
                                       baseProjection, lightBounds, false);
  const float depthScale = -2.0f / (depthMaximum - depthMinimum);
  const float depthBias =
      (depthMinimum + depthMaximum) / (depthMaximum - depthMinimum);
  for (int slice = 0; slice < 4; ++slice) {
    const float extent = std::pow(2.0f, static_cast<float>(slice)) *
                         options.sliceStep;
    idRenderMatrix sliceProjection = {{
        1.0f / extent, 0.0f, 0.0f, -0.0f,
        0.0f, -1.0f / extent, 0.0f, -0.0f,
        0.0f, 0.0f, depthScale, depthBias,
        0.0f, 0.0f, 0.0f, 1.0f,
    }};
    idRenderMatrix::Multiply(sliceProjection, baseProjection,
                             shadowViewProjection[slice]);
  }
  const float atlasScale =
      (static_cast<float>(shadowBufferSize) * 0.5f) /
      static_cast<float>(options.shadowBufferSize);
  SetupWorldPosToShadowMap(shadowViewProjection[0], atlasScale, 0.0f,
                           0.0f, lightShadow.worldPosToShadowMap);
  return shadowBufferSize;
}

void ShadowCasterQuickSort(shadowCaster_t* casters,
                           std::uint32_t* distances, std::int64_t count) {
  if (count <= 1)
    return;

  struct range_t {
    std::int64_t low;
    std::int64_t high;
  };
  std::array<range_t, 128> stack{};
  int stackDepth = 0;
  stack[stackDepth++] = {0, count - 1};
  while (stackDepth > 0) {
    const range_t range = stack[--stackDepth];
    std::int64_t low = range.low;
    std::int64_t high = range.high;
    while (high - low >= 4 && stackDepth < 127) {
      const std::uint32_t pivot = distances[(low + high) / 2];
      std::int64_t left = low;
      std::int64_t right = high;
      do {
        while (distances[left] < pivot)
          ++left;
        while (distances[right] > pivot)
          --right;
        if (left > right)
          break;
        SwapCaster(casters[left], casters[right], distances[left],
                   distances[right]);
        ++left;
        --right;
      } while (left < right);
      while (right > low && distances[right] == pivot)
        --right;
      while (left < high && distances[left] == pivot)
        ++left;
      stack[stackDepth++] = {left, high};
      high = right;
    }

    for (std::int64_t end = low + 1; end <= high; ++end) {
      std::int64_t maximum = low;
      for (std::int64_t index = low + 1; index <= end; ++index) {
        if (distances[index] > distances[maximum])
          maximum = index;
      }
      SwapCaster(casters[maximum], casters[end], distances[maximum],
                 distances[end]);
    }
  }
}

void RenderShadowBuffer(const renderShadowBufferParms_t* parameters,
                        const idRenderLightCommitted* light,
                        lightShadow_t* lightShadow) {
  if (parameters->options.skipShadows)
    return;

  idRenderDestination* shadowDestination =
      const_cast<idRenderDestination*>(parameters->renderDestShadow);
  const int destinationResolution = parameters->settings->dimShadowResolution;
  if (shadowDestination->targetWidth != destinationResolution) {
    shadowDestination->Resize(destinationResolution, destinationResolution);
  }

  std::array<idRenderMatrix, 6> shadowViewProjection{};
  const int slotSize = SetupShadowBufferMatrices(
      light->r, light->globalReferenceBounds,
      parameters->renderView->r.vieworg, parameters->options,
      shadowViewProjection.data(), *lightShadow);
  if (light->r.lightType != LIGHT_POINT) {
    const float scale = static_cast<float>(slotSize) /
                        static_cast<float>(destinationResolution);
    for (int index = 0; index < 4; ++index) {
      lightShadow->worldPosToShadowMap.m[index] *= scale;
      lightShadow->worldPosToShadowMap.m[4 + index] *= scale;
    }
  }
  renderThreadParmState->SetParmVector(
      parameters->rpShadowBufferResolution,
      static_cast<float>(destinationResolution),
      1.0f / static_cast<float>(destinationResolution),
      static_cast<float>(slotSize) /
          static_cast<float>(destinationResolution),
      0.0f);

  std::uint8_t lightOriginCullBits = 0;
  idRenderMatrix::CullPointToMVPbits(
      parameters->renderView->worldSpaceMVPMatrix,
      light->globalLightOrigin, lightOriginCullBits, false);
  std::memset(parameters->modelConsideredAsOccluder, 0, 128);

  std::array<shadowCaster_t, 4096> shadowCasters{};
  std::array<std::uint32_t, 4096> distances{};
  int numShadowCasters = 0;
  const auto addShadowCaster =
      [&](const idRenderModelCommitted& model, const idBounds& bounds,
          const idRenderModelSurface* surfaces, int numSurfaces) {
        if (numShadowCasters >= static_cast<int>(shadowCasters.size()))
          return;
        if (!parameters->options.skipShadowModelCPUCulling) {
          idRenderMatrix shadowMvp{};
          std::uint8_t cullBits = 0;
          idRenderMatrix::Multiply(light->baseLightProject,
                                   model.modelMatrix, shadowMvp);
          if (idRenderMatrix::CullBoundsToMVPbits(shadowMvp, bounds,
                                                 cullBits, true)) {
            return;
          }
          if ((model.r.flags0 & 0x08u) == 0) {
            auto* occlusion = static_cast<occlusionState_t*>(
                parameters->renderView->occlusionState);
            const unsigned modelIndex = model.index.index >> 8;
            if (occlusion->modelVisibleFrameNumber[modelIndex] <=
                occlusion->renderFrameNumber) {
              SetupModelMVPMatrix(
                  &const_cast<idRenderModelCommitted&>(model).mvpMatrix,
                  &model.r, &model.modelMatrix, parameters->renderView,
                  parameters->settings->viewNearZ,
                  parameters->settings->viewFarZ,
                  parameters->settings->guiOriginOffset);
            }
          }
          std::uint8_t modelCullBits = 0;
          idRenderMatrix::CullBoundsToMVPbits(model.mvpMatrix, bounds,
                                              modelCullBits, false);
          if (((lightOriginCullBits ^ modelCullBits) & modelCullBits) != 0)
            return;
        }

        shadowCasters[numShadowCasters] = {
            &model, &bounds, surfaces, numSurfaces, 0};
        const float distance =
            CasterDistance(model, bounds, light->globalLightOrigin);
        std::memcpy(&distances[numShadowCasters], &distance,
                    sizeof(distance));
        ++numShadowCasters;
      };
  for (int referenceIndex = 0;
       referenceIndex < light->references.num && numShadowCasters < 4096;
       ++referenceIndex) {
    const worldArea_t& area = *light->references.list[referenceIndex];
    const int areaNumber = area.areaNum;
    const std::uint8_t areaCullBits =
        parameters->worldAreaSubspaceBoundsCulled[areaNumber];
    if (((lightOriginCullBits ^ areaCullBits) & areaCullBits) != 0)
      continue;

    if (parameters->worldModel != nullptr) {
      const idRenderModelCommitted& model = *parameters->worldModel;
      const idBounds& bounds =
          parameters->worldAreaGeometryBounds[areaNumber];
      addShadowCaster(model, bounds, area.worldSurfaces.list,
                      area.worldSurfaces.num);
      continue;
    }

    for (int modelReference = 0;
         modelReference < area.modelRefs.num && numShadowCasters < 4096;
         ++modelReference) {
      const unsigned index = area.modelRefs.list[modelReference].index;
      const unsigned modelIndex = index >> 8;
      const unsigned consideredByte = index >> 11;
      const std::uint8_t consideredBit = static_cast<std::uint8_t>(index);
      if ((parameters->modelConsideredAsOccluder[consideredByte] &
           consideredBit) != 0) {
        continue;
      }
      parameters->modelConsideredAsOccluder[consideredByte] |= consideredBit;

      const idRenderModelCommitted* model =
          parameters->committedRenderModels[modelIndex];
      if ((model->committedFlags0 & 0x10u) == 0 ||
          (model->r.flags0 & 0x80u) != 0) {
        continue;
      }
      if ((model->r.suppressShadowInViewID != 0 &&
           model->r.suppressShadowInViewID == parameters->renderView->r.viewID) ||
          model->r.suppressShadowInLightIndex ==
              static_cast<int>(light->index.index >> 8)) {
        continue;
      }
      if (IsPrivateLight(light->r.lightClass) !=
          ((model->r.flags3 & 0x01u) != 0)) {
        continue;
      }

      addShadowCaster(*model, model->referenceBounds, model->surfaces.list,
                      model->surfaces.num);
    }
  }

  if (!parameters->options.skipShadowModelSort) {
    ShadowCasterQuickSort(shadowCasters.data(), distances.data(),
                          numShadowCasters);
  }

  int firstSide = 0;
  int endSide = 0;
  if (light->r.lightType == LIGHT_POINT) {
    if (parameters->options.singleSide == -1) {
      endSide = 6;
    } else {
      firstSide = parameters->options.singleSide;
      endSide = firstSide + 1;
    }
  } else if (light->r.lightType == LIGHT_SPOT) {
    endSide = 1;
  } else if (parameters->options.singleSide == -1) {
    endSide = 4;
  } else {
    firstSide = parameters->options.singleSide;
    endSide = firstSide + 1;
  }

  int occluderFacing = parameters->options.occluderFacing;
  if (light->r.forceTwoSidedShadows)
    occluderFacing = 2;
  const bool parallel = light->r.lightType == LIGHT_PARALLEL;
  const float polygonFactor =
      parallel ? parameters->options.parallelPolyOfsFactor
               : parameters->options.polyOfsFactor;
  const float polygonUnits =
      parallel ? parameters->options.parallelPolyOfsUnits
               : parameters->options.polyOfsUnits;
  const bool backFacing = occluderFacing == 1;
  const std::uint64_t facingState =
      backFacing ? UINT64_C(0x10000) : 0;
  GL_PolygonOffset(backFacing ? -polygonFactor : polygonFactor,
                   backFacing ? -polygonUnits : polygonUnits, true);
  GL_SetRenderDestination({parameters->renderDestShadow}, 0, 0);
  GL_Viewport(0, 0, parameters->options.shadowBufferSize,
              parameters->options.shadowBufferSize);
  GL_Scissor(0, 0, parameters->options.shadowBufferSize,
             parameters->options.shadowBufferSize);
  GL_State(UINT64_C(0x4000), false);
  GL_Clear(false, true, false, 0, 0.0f, 0.0f, 0.0f, 0.0f);

  for (int side = firstSide; side < endSide; ++side) {
    idRenderMatrix inverseShadowMvp{};
    idRenderMatrix unitCubeMvp{};
    idRenderMatrix::Inverse(shadowViewProjection[side], inverseShadowMvp);
    idRenderMatrix::Multiply(parameters->renderView->worldSpaceMVPMatrix,
                             inverseShadowMvp, unitCubeMvp);
    std::uint8_t sideCullBits = 0;
    if (idRenderMatrix::CullBoundsToMVPbits(unitCubeMvp, kUnitCube,
                                            sideCullBits, false)) {
      renderLog.OpenBlock("Shadow side %i culled\n", side);
      continue;
    }

    int viewportX = 0;
    int viewportY = 0;
    if (light->r.lightType == LIGHT_POINT) {
      viewportX = slotSize * kPointSlotX[side];
      viewportY = slotSize * kPointSlotY[side];
    } else if (light->r.lightType == LIGHT_PARALLEL) {
      viewportX = slotSize * kParallelSlotX[side];
      viewportY = slotSize * kParallelSlotY[side];
    }

    if (parameters->options.skipShadowOccluders)
      continue;
    GL_Viewport(viewportX, viewportY, slotSize, slotSize);
    GL_Scissor(viewportX, viewportY, slotSize, slotSize);
    GL_State(UINT64_C(0x4000), false);

    for (int casterIndex = 0; casterIndex < numShadowCasters;
         ++casterIndex) {
      const shadowCaster_t& caster = shadowCasters[casterIndex];
      idRenderMatrix shadowMvp{};
      idRenderMatrix::Multiply(shadowViewProjection[side],
                               caster.model->modelMatrix, shadowMvp);
      std::uint8_t modelCullBits = 0;
      if (!parameters->options.skipShadowModelCPUCulling &&
          idRenderMatrix::CullBoundsToMVPbits(
              shadowMvp, *caster.bounds, modelCullBits, false)) {
        continue;
      }
      renderLog.OpenBlock("Entering model %i: %s\n",
                          caster.model->index.index >> 8,
                          caster.model->name);
      renderThreadParmState->SetMVPMatrix(shadowMvp);

      for (int surfaceIndex = 0; surfaceIndex < caster.numSurfaces;
           ++surfaceIndex) {
        const idRenderModelSurface& surface = caster.surfaces[surfaceIndex];
        if (surface.geometry == nullptr || surface.material == nullptr ||
            !parameters->binaryModelState[surface.binaryModelId] ||
            (surface.material->flags0 & 0x01u) == 0) {
          continue;
        }
        const idDeclRenderProg* program = surface.material->progs[2];
        if (program == nullptr)
          continue;
        std::uint8_t surfaceCullBits = 0;
        if (!parameters->options.skipShadowSurfaceCPUCulling &&
            surface.joints == nullptr &&
            idRenderMatrix::CullBoundsToMVPbits(
                shadowMvp, surface.geometry->bounds, surfaceCullBits,
                false)) {
          continue;
        }

        renderLog.OpenBlock("Entering material %s\n",
                            surface.material->name.str);
        renderThreadParmState->VirtualEvaluateParmBlock(
            surface.material->renderParmBlock,
            surface.material->renderParmBlock);
        renderThreadParmState->VirtualEvaluateParmBlock(
            program->data.parmBlock, program->data.parmBlock);
        GL_DrawElements(idODSObject<idDeclRenderProg>{program},
                        idODSObject<idRenderModelSurface>{&surface},
                        facingState | UINT64_C(0x1000), false);
        if (occluderFacing == 2) {
          GL_PolygonOffset(-polygonFactor, -polygonUnits, true);
          GL_DrawElements(idODSObject<idDeclRenderProg>{program},
                          idODSObject<idRenderModelSurface>{&surface},
                          UINT64_C(0x11000), false);
          GL_PolygonOffset(polygonFactor, polygonUnits, true);
        }
        renderThreadParmState->VirtualRollbackParmBlock(
            program->data.parmBlock, program->data.parmBlock);
        renderThreadParmState->VirtualRollbackParmBlock(
            surface.material->renderParmBlock,
            surface.material->renderParmBlock);
      }
    }
  }

  GL_SetRenderDestination({parameters->renderDestDefault}, 0, 0);
  const idRenderView& view = *parameters->renderView;
  GL_Viewport(view.viewport.x1, view.viewport.y1,
              view.viewport.x2 - view.viewport.x1 + 1,
              view.viewport.y2 - view.viewport.y1 + 1);
  GL_Scissor(view.scissorRect.x1, view.scissorRect.y1,
             view.scissorRect.x2 - view.scissorRect.x1 + 1,
             view.scissorRect.y2 - view.scissorRect.y1 + 1);
  GL_PolygonOffset(0.0f, 0.0f, false);
}
