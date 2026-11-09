#include "engine/renderer/jobs/render/renderlights.h"

#include "engine/models/rendermodel.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/renderlightcommitted.h"
#include "engine/renderer/renderlog.h"
#include "engine/renderer/rendermodelcommitted.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/math/mathlib.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace {

constexpr float kFloatEpsilon = 1.1920929e-7f;
constexpr std::uint64_t kBlendedBackFacesState =
    UINT64_C(0x8000280FF01107C0);
constexpr std::uint64_t kBlendedFrontFacesState =
    UINT64_C(0x8000440FF01007C0);
constexpr std::uint64_t kInteractionBackFacesState =
    UINT64_C(0x80000C00000107C0);
constexpr std::uint64_t kInteractionFrontFacesState =
    UINT64_C(0x80001000000007C0);
constexpr std::uint64_t kInteractionSurfaceState = UINT64_C(0x5FF0100000);

const idRenderMatrix kIdentityMatrix = {{
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f,
}};

const idBounds kZeroOneBounds = {{
    {0.0f, 0.0f, 0.0f},
    {1.0f, 1.0f, 1.0f},
}};

float SinPolynomial(float value) {
  const float square = value * value;
  return (((((0.0000027526f - square * 0.0000000239f) * square -
              0.000198409f) *
                 square +
             0.0083333319f) *
                square -
            0.16666667f) *
               square +
           1.0f) *
         value;
}

void SinCos16(float angle, float& sine, float& cosine) {
  float value = angle;
  if (value < 0.0f || value >= 6.2831855f)
    value -= std::floor(value * 0.15915494f) * 6.2831855f;

  float cosineSign = 1.0f;
  if (value < 3.1415927f) {
    if (value > 1.5707964f) {
      value = 3.1415927f - value;
      cosineSign = -1.0f;
    }
  } else if (value > 4.712389f) {
    value -= 6.2831855f;
  } else {
    value = 3.1415927f - value;
    cosineSign = -1.0f;
  }

  sine = SinPolynomial(value);
  const float square = value * value;
  cosine = cosineSign *
           (((((-0.0000002605f * square + 0.0000247609f) * square -
                0.0013888397f) *
                   square +
               0.041666641f) *
                  square -
              0.4999999963f) *
                 square +
             1.0f);
}

float Sin16(float angle) {
  float sine;
  float cosine;
  SinCos16(angle, sine, cosine);
  return sine;
}

void SetMatrixParms(const idRenderMatrix& matrix,
                    const idDeclRenderParm* rowS,
                    const idDeclRenderParm* rowT,
                    const idDeclRenderParm* rowR,
                    const idDeclRenderParm* rowQ) {
  renderThreadParmState->SetParmVector(rowS, matrix.m);
  renderThreadParmState->SetParmVector(rowT, matrix.m + 4);
  renderThreadParmState->SetParmVector(rowR, matrix.m + 8);
  renderThreadParmState->SetParmVector(rowQ, matrix.m + 12);
}

void SetFullViewportAndScissor(const idRenderView& view) {
  GL_Viewport(view.viewport.x1, view.viewport.y1,
              view.viewport.x2 - view.viewport.x1 + 1,
              view.viewport.y2 - view.viewport.y1 + 1);
  GL_Scissor(view.scissorRect.x1, view.scissorRect.y1,
             view.scissorRect.x2 - view.scissorRect.x1 + 1,
             view.scissorRect.y2 - view.scissorRect.y1 + 1);
}

void RestoreDestination(const renderLightsParms_t& parameters,
                        const idRenderDestination* current) {
  if (current == nullptr ||
      parameters.shadowBufferParms.renderDestDefault == current)
    return;
  GL_SetRenderDestination(idODSObject<idRenderDestination>{current}, 0, 0);
  SetFullViewportAndScissor(*parameters.renderView);
}

bool IsPrivateLight(lightClass_t lightClass) {
  return lightClass == LIGHT_PRIVATE_DYNAMIC_ONLY ||
         lightClass == LIGHT_PRIVATE_PRIME ||
         lightClass == LIGHT_SLOWLIGHT_PRIVATE_ONLY;
}

bool ModelReceivesLight(const idRenderLightCommitted& light,
                        const idRenderModelCommitted& model,
                        bool sssOnly) {
  if ((model.committedFlags0 & 0x20u) == 0 ||
      (model.r.flags1 & 0x01u) != 0)
    return false;
  if (sssOnly && (model.committedFlags0 & 0x40u) == 0)
    return false;

  if (!((light.r.lightGroupID == -1 && model.r.lightGroupID == -1) ||
        light.r.lightGroupID == model.r.lightGroupID))
    return false;

  const bool privateModel = (model.r.flags3 & 0x01u) != 0;
  if (IsPrivateLight(light.r.lightClass) != privateModel)
    return false;
  if (light.r.lightClass == LIGHT_PRELIGHT_ONLY &&
      (model.r.flags0 & 0x04u) == 0)
    return false;
  if (light.r.lightClass == LIGHT_DYNAMIC_ONLY &&
      (model.r.flags0 & 0x04u) != 0)
    return false;
  if ((light.r.lightClass == LIGHT_SLOWLIGHT_DYNAMIC_ONLY ||
       light.r.lightClass == LIGHT_SLOWLIGHT_PRIVATE_ONLY) &&
      (model.r.flags0 & (0x04u | 0x08u)) != 0)
    return false;
  if ((model.r.flags0 & 0x08u) == 0 &&
      light.CullModelBounds(model.modelMatrix, model.referenceBounds))
    return false;
  return (model.r.flags3 & 0x40u) == 0;
}

float ComputeLightFade(const renderLightsParms_t& parameters,
                       const idRenderLightCommitted& light) {
  if (parameters.settings->skipLightRangeCulling ||
      light.r.maxVisibleRange <= 0.0f)
    return 1.0f;

  const float x = parameters.settings->viewOrigin.x - light.maxVisibleRange.x;
  const float y = parameters.settings->viewOrigin.y - light.maxVisibleRange.y;
  const float z = parameters.settings->viewOrigin.z - light.maxVisibleRange.z;
  const float distanceSquared = x * x + y * y + z * z;
  const float fadeOverSquared =
      light.r.fadeVisibilityOver * light.r.fadeVisibilityOver;
  const float rangeSquared = light.r.maxVisibleRange * light.r.maxVisibleRange;
  float fade = 1.0f - (distanceSquared - fadeOverSquared) /
                          (rangeSquared - fadeOverSquared);
  if (fade >= 0.0f) {
    if (fade > 1.0f)
      fade = 1.0f;
  } else {
    fade = 0.0f;
  }
  return fade;
}

idRenderMatrix BuildAnimatedLightProject(
    const renderLightsParms_t& parameters,
    const idRenderLightCommitted& light) {
  idRenderMatrix lightProject = light.baseLightProject;

  const float rotation =
      renderThreadParmState->GetParmFloat(parameters.rpLightRotation);
  const idVec4& translationValue =
      renderThreadParmState->GetParmVec4(parameters.rpLightTranslation);
  const idVec4& scaleValue =
      renderThreadParmState->GetParmVec4(parameters.rpLightScale);
  const idVec4& frequencyValue =
      renderThreadParmState->GetParmVec4(parameters.rpLightOscillationFreq);
  const idVec4& amplitudeValue =
      renderThreadParmState->GetParmVec4(parameters.rpLightOscillationAmp);
  const idVec4& phaseValue =
      renderThreadParmState->GetParmVec4(parameters.rpLightOscillationPhase);

  if (rotation == 0.0f && translationValue.x == 0.0f &&
      translationValue.y == 0.0f && scaleValue.x == 1.0f &&
      scaleValue.y == 1.0f && frequencyValue.x == 0.0f &&
      frequencyValue.y == 0.0f)
    return lightProject;

  float oscillationX = 0.0f;
  float oscillationY = 0.0f;
  if (frequencyValue.x != 0.0f)
    oscillationX =
        Sin16(phaseValue.x + frequencyValue.x) * amplitudeValue.x;
  if (frequencyValue.y != 0.0f)
    oscillationY =
        Sin16(phaseValue.y + frequencyValue.y) * amplitudeValue.y;

  float sine = 0.0f;
  float cosine = 1.0f;
  float rotationOffsetX = 0.0f;
  float rotationOffsetY = 0.0f;
  if (rotation != 0.0f) {
    SinCos16(rotation * 0.017453292f, sine, cosine);
    rotationOffsetX =
        (sine * 0.5f - cosine * 0.5f + 0.5f) * scaleValue.x;
    rotationOffsetY =
        (-sine * 0.5f - cosine * 0.5f + 0.5f) * scaleValue.y;
  }

  idRenderMatrix textureMatrix{};
  textureMatrix.m[0] = cosine * (oscillationX + scaleValue.x);
  textureMatrix.m[1] = -sine * scaleValue.y;
  textureMatrix.m[3] = rotationOffsetX +
                       (0.5f - scaleValue.x * 0.5f + translationValue.x);
  textureMatrix.m[4] = sine * scaleValue.x;
  textureMatrix.m[5] = cosine * (oscillationY + scaleValue.y);
  textureMatrix.m[7] = rotationOffsetY +
                       (0.5f - scaleValue.y * 0.5f + translationValue.y);
  textureMatrix.m[10] = 1.0f;
  textureMatrix.m[15] = 1.0f;

  idRenderMatrix generatedProjection = light.baseLightProject;
  generatedProjection.m[8] = 0.0f;
  generatedProjection.m[9] = 0.0f;
  generatedProjection.m[10] = 0.0f;
  generatedProjection.m[11] = 0.0f;

  idRenderMatrix adjustedProjection;
  idRenderMatrix::Multiply(textureMatrix, generatedProjection,
                           adjustedProjection);
  std::copy_n(adjustedProjection.m, 8, lightProject.m);
  return lightProject;
}

bool BlendedLightHasNoEffect(const renderLightsParms_t& parameters,
                             const idRenderLightCommitted& light) {
  const idVec4& color =
      renderThreadParmState->GetParmVec4(parameters.rpLightColor);
  const unsigned int blendState =
      static_cast<unsigned int>(light.lightMtr->progs[7]->data.glState & 0x3Fu);
  if (blendState == 8 || blendState == 10 || blendState == 25)
    return color.x + color.y + color.z <= kFloatEpsilon;
  if (blendState == 2)
    return color.x + kFloatEpsilon >= 1.0f &&
           color.y + kFloatEpsilon >= 1.0f &&
           color.z + kFloatEpsilon >= 1.0f;
  return false;
}

void DrawCube(const idDeclRenderProg* program, const idTriangles* triangles,
              std::uint64_t state) {
  GL_DrawElements(idODSObject<idDeclRenderProg>{program},
                  idODSObject<idTriangles>{triangles}, state, false);
}

void SetLightScales(const renderLightsParms_t& parameters,
                    float diffuseScale, float specularScale,
                    float environmentScale, float eyeDiffuseScale,
                    float eyeSpecularScale, float eyeEnvironmentScale) {
  renderThreadParmState->SetParmFloat(parameters.rpLightDiffuseScale,
                                     diffuseScale);
  renderThreadParmState->SetParmFloat(parameters.rpLightSpecularScale,
                                     specularScale);
  renderThreadParmState->SetParmFloat(parameters.rpLightEnvironmentScale,
                                     environmentScale);
  renderThreadParmState->SetParmFloat(parameters.rpLightEyeDiffuseScale,
                                     eyeDiffuseScale);
  renderThreadParmState->SetParmFloat(parameters.rpLightEyeSpecularScale,
                                     eyeSpecularScale);
  renderThreadParmState->SetParmFloat(parameters.rpLightEyeEnvironmentScale,
                                     eyeEnvironmentScale);
}

}  // namespace

bool idRenderLightCommitted::CullModelBounds(
    const idRenderMatrix& modelMatrix, const idBounds& localBounds) const {
  idRenderMatrix modelLightProject;
  idRenderMatrix::Multiply(baseLightProject, modelMatrix, modelLightProject);
  unsigned char outBits;
  return idRenderMatrix::CullBoundsToMVPbits(modelLightProject, localBounds,
                                             outBits, true);
}

idRenderDrawSurf::~idRenderDrawSurf() {
  PopSurface();
  PopModel();
}

idLightRenderParmEvaluateScope::~idLightRenderParmEvaluateScope() {
  if (light != nullptr) {
    renderThreadParmState->VirtualRollbackParmBlock(
        light->lightMtr->renderParmBlock, light->lightMtr->renderParmBlock);
    renderThreadParmState->VirtualRollbackParmBlock(light->renderParmBlock,
                                                    light->renderParmBlock);
  }
}

void idParmState::SetMVPMatrixForInverseProject(
    const idRenderMatrix& matrix, const idRenderMatrix& inverseProject) {
  const unsigned int stackMask =
      onParmBlockStack[parmIndexMVPMatrixY] |
      onParmBlockStack[parmIndexMVPMatrixW] |
      onParmBlockStack[parmIndexMVPMatrixX] |
      onParmBlockStack[parmIndexMVPMatrixZ];
  if ((stackMask & parmStack.virtualParmBlockStackMask) != 0)
    RollbackVirtualStack(true);

  idRenderMatrix::SetMVPForInverseProject(
      matrix, inverseProject,
      *reinterpret_cast<idVec4*>(&values[parmIndexMVPMatrixX]),
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

void RenderInteractionLightsPrePostPass(
    const renderLightsParms_t* parameters,
    const idRenderDestination* current,
    const idRenderDestination* target,
    const idDeclRenderProg* passProgram) {
  if (current == nullptr || target == nullptr)
    return;

  GL_SetRenderDestination(idODSObject<idRenderDestination>{target}, 0, 0);
  GL_SetDefaultState();
  SetFullViewportAndScissor(*parameters->renderView);
  GL_Clear(true, false, false, 0, 0.0f, 0.0f, 0.0f, 0.0f);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      current->targetWidth, current->targetHeight, current->targetWidth,
      current->targetHeight);
  const idTextureSampler* source =
      reinterpret_cast<const idTextureSampler*>(current->targetImage[0]);
  renderThreadParmState->SetParmSampler(parameters->rpViewTarget, source);
  renderThreadParmState->SetParmSampler(parameters->rpViewTargetMS, source);
  renderThreadParmState->VirtualEvaluateParmBlock(passProgram->data.parmBlock,
                                                  passProgram->data.parmBlock);
  DrawCube(passProgram,
           parameters->drawSurfParms.captureParms.unitSquareTris, 0);
  renderThreadParmState->VirtualRollbackParmBlock(passProgram->data.parmBlock,
                                                  passProgram->data.parmBlock);
}

void RenderLightsInternal(const renderLightsParms_t* parameters,
                          const std::uint16_t* viewLights,
                          int numViewLights,
                          const idRenderDestination* current,
                          renderLightsMode_t mode) {
  if (mode == RENDERLIGHTS_NOTHING)
    return;

  const int interactionMode = static_cast<int>(mode) & 0x0F;
  const bool sssOnly = interactionMode == RENDERLIGHTS_INTERACTION_SSS;
  SetFullViewportAndScissor(*parameters->renderView);
  int numShadows = *parameters->numShadows;

  for (int lightNumber = 0; lightNumber < numViewLights; ++lightNumber) {
    const idRenderLightCommitted* light =
        parameters->viewLights[viewLights[lightNumber]];
    const bool blended = light->r.lightClass == LIGHT_BLENDED_ONLY;
    if (blended) {
      if ((static_cast<int>(mode) & RENDERLIGHTS_BLENDED) == 0)
        continue;
    } else {
      if (light->r.lightClass != LIGHT_SLOWLIGHT_DYNAMIC_ONLY &&
          light->r.lightClass != LIGHT_SLOWLIGHT_PRIVATE_ONLY &&
          parameters->settings->IsComboMap())
        continue;
      if (interactionMode == 0)
        continue;
    }

    renderLog.OpenBlock("----- Light %i : lightMtr %s-----\n",
                        light->index.index >> 8, light->lightMtr->name.str);
    renderThreadParmState->VirtualEvaluateParmBlock(light->renderParmBlock,
                                                    light->renderParmBlock);
    renderThreadParmState->VirtualEvaluateParmBlock(
        light->lightMtr->renderParmBlock, light->lightMtr->renderParmBlock);
    idLightRenderParmEvaluateScope lightScope(light);

    if (blended && BlendedLightHasNoEffect(*parameters, *light))
      continue;

    const idRenderMatrix lightProject =
        BuildAnimatedLightProject(*parameters, *light);
    renderThreadParmState->SetParmFloat(
        parameters->rpLightFade, ComputeLightFade(*parameters, *light));

    if (blended) {
      if (light->castsShadows &&
          !parameters->settings->skipBlendLightShadows) {
        lightShadow_t lightShadow;
        RenderShadowBuffer(&parameters->shadowBufferParms, light,
                           &lightShadow);
        RestoreDestination(*parameters, current);

        idRenderMatrix inverseShadowView;
        idRenderMatrix inverseShadowProjection;
        idRenderMatrix windowToShadow;
        idRenderMatrix::Multiply(lightShadow.worldPosToShadowMap,
                                 parameters->renderView->inverseViewMatrix,
                                 inverseShadowView);
        idRenderMatrix::Multiply(
            inverseShadowView, parameters->renderView->inverseProjectionMatrix,
            inverseShadowProjection);
        idRenderMatrix::Multiply(inverseShadowProjection,
                                 renderMatrix_windowSpaceToClipSpace,
                                 windowToShadow);
        SetMatrixParms(windowToShadow,
                       parameters->rpWindowPosToProjectedShadowS,
                       parameters->rpWindowPosToProjectedShadowT,
                       parameters->rpWindowPosToProjectedShadowR,
                       parameters->rpWindowPosToProjectedShadowQ);
        SetMatrixParms(lightShadow.shadowProjection,
                       parameters->rpShadowProjectionS,
                       parameters->rpShadowProjectionT,
                       parameters->rpShadowProjectionR,
                       parameters->rpShadowProjectionQ);
        ++numShadows;
      }

      if (parameters->settings->useLightDepthBoundsTest) {
        idRenderMatrix lightMvp;
        idRenderMatrix::Multiply(
            parameters->renderView->worldSpaceMVPMatrix,
            light->inverseBaseLightProject, lightMvp);
        float depthMinimum;
        float depthMaximum;
        idRenderMatrix::DepthBoundsForBounds(depthMinimum, depthMaximum,
                                             lightMvp, kZeroOneBounds, true);
        GL_DepthBoundsTest(depthMinimum, depthMaximum);
      }

      renderThreadParmState->SetMVPMatrixForInverseProject(
          parameters->renderView->worldSpaceMVPMatrix,
          light->inverseBaseLightProject);
      unsigned char outBits;
      if (!idRenderMatrix::CullBoundsToMVPbits(
              light->baseLightProject, parameters->settings->viewBounds,
              outBits, true)) {
        DrawCube(parameters->progDepthOnly, parameters->zeroOneCubeTris,
                 kBlendedBackFacesState);
      }
      DrawCube(parameters->progDepthOnly, parameters->zeroOneCubeTris,
               kBlendedFrontFacesState);

      idRenderMatrix inverseLightView;
      idRenderMatrix inverseLightProjection;
      idRenderMatrix windowToLight;
      idRenderMatrix::Multiply(lightProject,
                               parameters->renderView->inverseViewMatrix,
                               inverseLightView);
      idRenderMatrix::Multiply(
          inverseLightView, parameters->renderView->inverseProjectionMatrix,
          inverseLightProjection);
      idRenderMatrix::Multiply(inverseLightProjection,
                               renderMatrix_windowSpaceToClipSpace,
                               windowToLight);
      SetMatrixParms(windowToLight, parameters->rpWindowPosToProjectedTexS,
                     parameters->rpWindowPosToProjectedTexT,
                     parameters->rpWindowPosToProjectedTexR,
                     parameters->rpWindowPosToProjectedTexQ);
      renderThreadParmState->SetParmVec3(parameters->rpGlobalLightOrigin,
                                        light->globalLightOrigin);
      for (int pass = 0; pass < light->r.slowMultiPass; ++pass) {
        DrawCube(light->lightMtr->progs[7], parameters->zeroOneCubeTris,
                 UINT64_C(0x10000));
      }
      if (parameters->settings->useLightDepthBoundsTest)
        GL_DepthBoundsTest(0.0f, 0.0f);

      if (parameters->settings->showBlendedLights != 0) {
        std::uint64_t debugState = 0;
        if (parameters->settings->showBlendedLights == 1)
          debugState = 2048;
        else if (parameters->settings->showBlendedLights == 2)
          debugState = kInteractionSurfaceState;
        DrawCube(parameters->progBlendedLightDebugDraw,
                 parameters->zeroOneCubeTris, debugState);
      }
      continue;
    }

    int scissorX1 = parameters->renderView->scissorRect.x1;
    int scissorY1 = parameters->renderView->scissorRect.y1;
    int scissorX2 = parameters->renderView->scissorRect.x2;
    int scissorY2 = parameters->renderView->scissorRect.y2;
    float depthMinimum = 0.0f;
    float depthMaximum = 1.0f;
    if (parameters->settings->useLightScissors) {
      idRenderMatrix lightMvp;
      idRenderMatrix::Multiply(parameters->renderView->worldSpaceMVPMatrix,
                               light->inverseBaseLightProject, lightMvp);
      idBounds projectedBounds;
      idRenderMatrix::ProjectedBounds(projectedBounds, lightMvp,
                                      kZeroOneBounds, true);
      const float viewportWidth =
          static_cast<float>(parameters->renderView->viewport.x2 -
                             parameters->renderView->viewport.x1);
      const float viewportHeight =
          static_cast<float>(parameters->renderView->viewport.y2 -
                             parameters->renderView->viewport.y1);
      const int projectedX1 =
          std::max(static_cast<int>(viewportWidth * projectedBounds.b[0].x) -
                       1,
                   0);
      const int projectedY1 =
          std::max(static_cast<int>(viewportHeight * projectedBounds.b[0].y) -
                       1,
                   0);
      const int projectedX2 =
          std::min(static_cast<int>(viewportWidth * projectedBounds.b[1].x) +
                       1,
                   65534);
      const int projectedY2 =
          std::min(static_cast<int>(viewportHeight * projectedBounds.b[1].y) +
                       1,
                   65534);
      scissorX1 = std::max(scissorX1, projectedX1);
      scissorY1 = std::max(scissorY1, projectedY1);
      scissorX2 = std::min(scissorX2, projectedX2);
      scissorY2 = std::min(scissorY2, projectedY2);
      depthMinimum = projectedBounds.b[0].z;
      depthMaximum = projectedBounds.b[1].z;
    }
    if (scissorX1 > scissorX2 || scissorY1 > scissorY2)
      continue;

    std::array<const idRenderModelCommitted*, 8192> models{};
    int numModels = 0;
    for (int modelNumber = 0; modelNumber < parameters->numViewModels;
         ++modelNumber) {
      const idRenderModelCommitted* model =
          parameters->viewModels[modelNumber];
      if (ModelReceivesLight(*light, *model, sssOnly))
        models[numModels++] = model;
    }
    if (numModels == 0)
      continue;

    lightShadow_t lightShadow{kIdentityMatrix, kIdentityMatrix};
    if (light->castsShadows) {
      RenderShadowBuffer(&parameters->shadowBufferParms, light, &lightShadow);
      RestoreDestination(*parameters, current);
      SetMatrixParms(lightShadow.shadowProjection,
                     parameters->rpShadowProjectionS,
                     parameters->rpShadowProjectionT,
                     parameters->rpShadowProjectionR,
                     parameters->rpShadowProjectionQ);
      ++numShadows;
    }

    if (parameters->settings->useLightDepthBoundsTest)
      GL_DepthBoundsTest(depthMinimum, depthMaximum);
    GL_Scissor(scissorX1, scissorY1, scissorX2 - scissorX1 + 1,
               scissorY2 - scissorY1 + 1);
    if (parameters->settings->showLightScissors)
      GL_Clear(true, false, false, 0, 1.0f, 0.0f, 0.0f, 0.0f);
    GL_State(UINT64_C(0xFF0000000), false);
    GL_Clear(false, false, true, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    renderThreadParmState->SetMVPMatrixForInverseProject(
        parameters->renderView->worldSpaceMVPMatrix,
        light->inverseBaseLightProject);
    DrawCube(parameters->progDepthOnly, parameters->zeroOneCubeTris,
             kInteractionBackFacesState);
    DrawCube(parameters->progDepthOnly, parameters->zeroOneCubeTris,
             kInteractionFrontFacesState);

    renderThreadParmState->SetParmVec3(parameters->rpGlobalLightOrigin,
                                      light->globalLightOrigin);
    renderThreadParmState->SetParmFloat(parameters->rpStaticSpecularScale,
                                       light->r.staticSpecularScale);
    renderThreadParmState->SetParmVec3(parameters->rpStaticSpecularVector,
                                      light->r.staticSpecularVector);

    const float diffuseMode =
        (static_cast<int>(mode) & RENDERLIGHTS_INTERACTION_DIFFUSE) != 0
            ? 1.0f
            : 0.0f;
    const float specularMode =
        (static_cast<int>(mode) & RENDERLIGHTS_INTERACTION_SPECULAR) != 0
            ? 1.0f
            : 0.0f;
    const float baseDiffuse = diffuseMode * light->r.diffuseScale;
    const float baseSpecular = specularMode * light->r.specularScale;
    const float baseEnvironment = specularMode * light->r.environmentScale;
    const float baseEyeDiffuse = diffuseMode * light->r.eyeDiffuseScale;
    const float baseEyeSpecular = specularMode * light->r.eyeSpecularScale;
    const float baseEyeEnvironment =
        specularMode * light->r.eyeEnvironmentScale;
    SetLightScales(*parameters, baseDiffuse, baseSpecular, baseEnvironment,
                   baseEyeDiffuse, baseEyeSpecular, baseEyeEnvironment);

    {
      idRenderDrawSurf drawSurfaceRenderer;
      for (int modelNumber = 0; modelNumber < numModels; ++modelNumber) {
        const idRenderModelCommitted* model = models[modelNumber];
        idRenderMatrix modelLightProject;
        idRenderMatrix::Multiply(lightProject, model->modelMatrix,
                                 modelLightProject);
        SetMatrixParms(modelLightProject, parameters->rpLightProjectionS,
                       parameters->rpLightProjectionT,
                       parameters->rpLightProjectionR,
                       parameters->rpLightProjectionQ);

        if (light->castsShadows) {
          idRenderMatrix localToLight;
          idRenderMatrix::Multiply(lightShadow.worldPosToShadowMap,
                                   model->modelMatrix, localToLight);
          SetMatrixParms(localToLight, parameters->rpLocalToLightS,
                         parameters->rpLocalToLightT,
                         parameters->rpLocalToLightR,
                         parameters->rpLocalToLightQ);
        }

        const bool worldModel = (model->r.flags0 & 0x08u) != 0;
        const int numSurfaces = worldModel ? parameters->numViewWorldSurfaces
                                           : model->surfaces.num;
        for (int surfaceNumber = 0; surfaceNumber < numSurfaces;
             ++surfaceNumber) {
          const idRenderModelSurface* surface =
              worldModel ? parameters->viewWorldSurfaces[surfaceNumber]
                         : &model->surfaces.list[surfaceNumber];
          const idMaterial* material = surface->material;
          if (material == nullptr ||
              !parameters->binaryModelState[surface->binaryModelId])
            continue;

          const idDeclRenderProg* interactionProgram = material->progs[1];
          if (interactionProgram == nullptr)
            continue;
          if (numSurfaces != 1 && surface->joints == nullptr) {
            idRenderMatrix surfaceLightProject;
            idRenderMatrix::Multiply(light->baseLightProject,
                                     model->modelMatrix,
                                     surfaceLightProject);
            unsigned char outBits;
            if (idRenderMatrix::CullBoundsToMVPbits(
                    surfaceLightProject, surface->geometry->bounds, outBits,
                    true))
              continue;
          }

          const idDeclRenderProg* program = interactionProgram->renamed;
          if (light->castsShadows &&
              program->data.stageSort == parameters->settings->sortLight)
            program = program->versions.ptr[light->r.lightType];

          float surfaceDiffuse = 0.0f;
          float surfaceSpecular = 0.0f;
          const bool usesPostSpecular = (material->flags0 & 0x80u) != 0;
          if (material->progs[3] != nullptr) {
            surfaceDiffuse =
                (static_cast<int>(mode) &
                 RENDERLIGHTS_INTERACTION_DIFFUSE_SSS) != 0
                    ? 1.0f
                    : 0.0f;
            if ((static_cast<int>(mode) &
                 RENDERLIGHTS_INTERACTION_SPECULAR_SSS) != 0)
              surfaceSpecular = usesPostSpecular ? 0.0f : 1.0f;
            else if ((static_cast<int>(mode) &
                      RENDERLIGHTS_INTERACTION_SPECULAR_SIMPLE) != 0)
              surfaceSpecular = usesPostSpecular ? 1.0f : 0.0f;
          } else {
            surfaceDiffuse =
                (static_cast<int>(mode) &
                 RENDERLIGHTS_INTERACTION_DIFFUSE_SIMPLE) != 0
                    ? 1.0f
                    : 0.0f;
            surfaceSpecular =
                (static_cast<int>(mode) &
                 RENDERLIGHTS_INTERACTION_SPECULAR_SIMPLE) != 0
                    ? 1.0f
                    : 0.0f;
          }
          SetLightScales(
              *parameters, surfaceDiffuse * baseDiffuse,
              surfaceSpecular * baseSpecular,
              surfaceSpecular * baseEnvironment,
              surfaceDiffuse * baseEyeDiffuse,
              surfaceSpecular * baseEyeSpecular,
              surfaceSpecular * baseEyeEnvironment);

          drawSurf_t drawSurface{};
          drawSurface.model = model;
          drawSurface.modelSurface = surface;
          drawSurface.material = material;
          drawSurface.prog = program;
          drawSurfaceRenderer.RenderDrawSurf(&parameters->drawSurfParms,
                                             &drawSurface,
                                             kInteractionSurfaceState);
        }
      }
      GL_Scissor(parameters->renderView->scissorRect.x1,
                 parameters->renderView->scissorRect.y1,
                 parameters->renderView->scissorRect.x2 -
                         parameters->renderView->scissorRect.x1 +
                     1,
                 parameters->renderView->scissorRect.y2 -
                         parameters->renderView->scissorRect.y1 +
                     1);
      if (parameters->settings->useLightDepthBoundsTest)
        GL_DepthBoundsTest(0.0f, 0.0f);
    }
  }

  *parameters->numShadows = numShadows;
}

void RenderLights(const renderLightsParms_t* parameters,
                  renderLightsMode_t mode) {
  if (mode == RENDERLIGHTS_NOTHING)
    return;

  std::array<std::uint16_t, 8192> selectedLights{};
  int numSelectedLights = 0;
  int numInteractionLights = 0;
  for (int lightNumber = 0; lightNumber < parameters->numViewLights;
       ++lightNumber) {
    const idRenderLightCommitted* light = parameters->viewLights[lightNumber];
    if (light->r.lightClass == LIGHT_BLENDED_ONLY) {
      if (!parameters->settings->skipBlendLights &&
          (static_cast<int>(mode) & RENDERLIGHTS_BLENDED) != 0 &&
          light->lightMtr->progs[7] != nullptr) {
        selectedLights[numSelectedLights++] =
            static_cast<std::uint16_t>(lightNumber);
      }
    } else if ((light->r.lightClass == LIGHT_SLOWLIGHT_DYNAMIC_ONLY ||
                light->r.lightClass == LIGHT_SLOWLIGHT_PRIVATE_ONLY ||
                !parameters->settings->IsComboMap()) &&
               !parameters->settings->skipSlowLights &&
               (static_cast<int>(mode) & RENDERLIGHTS_INTERACTION) != 0) {
      ++numInteractionLights;
      selectedLights[numSelectedLights++] =
          static_cast<std::uint16_t>(lightNumber);
    }
  }

  if (numInteractionLights > 0) {
    RenderInteractionLightsPrePostPass(
        parameters, parameters->renderDestDefault,
        parameters->renderDestLinear, parameters->progInteractionPrePass);
    RenderLightsInternal(
        parameters, selectedLights.data(), numSelectedLights,
        parameters->renderDestLinear,
        static_cast<renderLightsMode_t>(static_cast<int>(mode) & 0x0F));
    RenderInteractionLightsPrePostPass(
        parameters, parameters->renderDestLinear,
        parameters->renderDestDefault, parameters->progInteractionPostPass);
  }
  RenderLightsInternal(
      parameters, selectedLights.data(), numSelectedLights,
      parameters->renderDestDefault,
      static_cast<renderLightsMode_t>(static_cast<int>(mode) & ~0x0F));
}
