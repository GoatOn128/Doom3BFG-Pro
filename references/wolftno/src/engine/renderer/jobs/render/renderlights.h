#pragma once

#include "engine/renderer/jobs/render/renderdrawsurf.h"
#include "engine/renderer/jobs/render/rendershadowbuffer.h"

#include <cstddef>
#include <cstdint>

class idDeclRenderParm;
class idDeclRenderProg;
class idRenderView;
class idTriangles;
class idRenderDestination;
struct idRenderLightCommitted;
struct idRenderModelCommitted;
class idRenderModelSurface;
struct renderSettings_t;

enum renderLightsMode_t : int {
  RENDERLIGHTS_INTERACTION_DIFFUSE_SIMPLE = 0x01,
  RENDERLIGHTS_INTERACTION_DIFFUSE_SSS = 0x02,
  RENDERLIGHTS_INTERACTION_SPECULAR_SIMPLE = 0x04,
  RENDERLIGHTS_INTERACTION_SPECULAR_SSS = 0x08,
  RENDERLIGHTS_BLENDED = 0x10,
  RENDERLIGHTS_NOTHING = 0x00,
  RENDERLIGHTS_INTERACTION_DIFFUSE = 0x03,
  RENDERLIGHTS_INTERACTION_SPECULAR = 0x0C,
  RENDERLIGHTS_INTERACTION_SSS = 0x0A,
  RENDERLIGHTS_INTERACTION = 0x0F,
  RENDERLIGHTS_ALL = 0x1F,
};

struct renderLightsParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  renderDrawSurfParms_t drawSurfParms;
  renderShadowBufferParms_t shadowBufferParms;
  const idRenderLightCommitted** viewLights;
  int numViewLights;
  std::uint32_t alignmentBeforeModels;
  const idRenderModelCommitted** viewModels;
  int numViewModels;
  std::uint32_t alignmentBeforeWorldSurfaces;
  const idRenderModelSurface** viewWorldSurfaces;
  int numViewWorldSurfaces;
  std::uint32_t alignmentBeforeRenderParms;
  const idDeclRenderParm* rpLightColor;
  const idDeclRenderParm* rpLightRotation;
  const idDeclRenderParm* rpLightTranslation;
  const idDeclRenderParm* rpLightScale;
  const idDeclRenderParm* rpLightOscillationAmp;
  const idDeclRenderParm* rpLightOscillationFreq;
  const idDeclRenderParm* rpLightOscillationPhase;
  const idDeclRenderParm* rpWindowPosToProjectedTexS;
  const idDeclRenderParm* rpWindowPosToProjectedTexT;
  const idDeclRenderParm* rpWindowPosToProjectedTexR;
  const idDeclRenderParm* rpWindowPosToProjectedTexQ;
  const idDeclRenderParm* rpWindowPosToProjectedShadowS;
  const idDeclRenderParm* rpWindowPosToProjectedShadowT;
  const idDeclRenderParm* rpWindowPosToProjectedShadowR;
  const idDeclRenderParm* rpWindowPosToProjectedShadowQ;
  const idDeclRenderParm* rpShadowProjectionS;
  const idDeclRenderParm* rpShadowProjectionT;
  const idDeclRenderParm* rpShadowProjectionR;
  const idDeclRenderParm* rpShadowProjectionQ;
  const idDeclRenderParm* rpGlobalLightOrigin;
  const idDeclRenderParm* rpStaticSpecularScale;
  const idDeclRenderParm* rpStaticSpecularVector;
  const idDeclRenderParm* rpLightDiffuseScale;
  const idDeclRenderParm* rpLightSpecularScale;
  const idDeclRenderParm* rpLightEnvironmentScale;
  const idDeclRenderParm* rpLightEyeDiffuseScale;
  const idDeclRenderParm* rpLightEyeSpecularScale;
  const idDeclRenderParm* rpLightEyeEnvironmentScale;
  const idDeclRenderParm* rpLightProjectionS;
  const idDeclRenderParm* rpLightProjectionT;
  const idDeclRenderParm* rpLightProjectionR;
  const idDeclRenderParm* rpLightProjectionQ;
  const idDeclRenderParm* rpLocalToLightS;
  const idDeclRenderParm* rpLocalToLightT;
  const idDeclRenderParm* rpLocalToLightR;
  const idDeclRenderParm* rpLocalToLightQ;
  const idDeclRenderParm* rpLightFade;
  const idDeclRenderParm* rpViewTarget;
  const idDeclRenderParm* rpViewTargetMS;
  const idDeclRenderProg* progDepthOnly;
  const idDeclRenderProg* progBlendedLightDebugDraw;
  const idDeclRenderProg* progDeferredPointLight;
  const idDeclRenderProg* progDeferredSpotLight;
  const idDeclRenderProg* progDeferredParallelLight;
  const idDeclRenderProg* progInteractionPrePass;
  const idDeclRenderProg* progInteractionPostPass;
  const idRenderDestination* renderDestDefault;
  const idRenderDestination* renderDestLinear;
  const idTriangles* zeroOneCubeTris;
  const bool* binaryModelState;
  int* numShadows;
};

class idLightRenderParmEvaluateScope {
public:
  explicit idLightRenderParmEvaluateScope(
      const idRenderLightCommitted* evaluatedLight = nullptr)
      : light(evaluatedLight) {}
  ~idLightRenderParmEvaluateScope();

  const idRenderLightCommitted* light;
};

void RenderInteractionLightsPrePostPass(
    const renderLightsParms_t* parameters,
    const idRenderDestination* current,
    const idRenderDestination* target,
    const idDeclRenderProg* passProgram);
void RenderLightsInternal(const renderLightsParms_t* parameters,
                          const std::uint16_t* viewLights,
                          int numViewLights,
                          const idRenderDestination* current,
                          renderLightsMode_t mode);
void RenderLights(const renderLightsParms_t* parameters,
                  renderLightsMode_t mode);

static_assert(offsetof(renderLightsParms_t, drawSurfParms) == 0x10,
              "render-lights draw parameters offset drift");
static_assert(offsetof(renderLightsParms_t, shadowBufferParms) == 0x1C8,
              "render-lights shadow parameters offset drift");
static_assert(offsetof(renderLightsParms_t, viewLights) == 0x258,
              "render-lights light-list offset drift");
static_assert(offsetof(renderLightsParms_t, viewModels) == 0x268,
              "render-lights model-list offset drift");
static_assert(offsetof(renderLightsParms_t, rpLightColor) == 0x288,
              "render-lights parameter table offset drift");
static_assert(offsetof(renderLightsParms_t, progDepthOnly) == 0x3C0,
              "render-lights program table offset drift");
static_assert(offsetof(renderLightsParms_t, renderDestDefault) == 0x3F8,
              "render-lights destination offset drift");
static_assert(offsetof(renderLightsParms_t, numShadows) == 0x418,
              "render-lights shadow-count offset drift");
static_assert(sizeof(renderLightsParms_t) == 0x420,
              "render-lights parameters ABI drift");
static_assert(sizeof(idLightRenderParmEvaluateScope) == 8,
              "light parameter scope ABI drift");
