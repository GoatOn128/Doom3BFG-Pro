#pragma once

#include "engine/renderer/jobs/render/render_types.h"
#include "engine/renderer/parmblock.h"
#include "engine/renderer/rendermodelcommitted.h"
#include "engine/renderer/rendermodelparms.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/geometry/rendermatrix.h"

#include <cstddef>
#include <cstdint>

class idDeclEnv;
class idDeclRenderParm;
class idDeclRenderProg;
class idParmBlock;
class idRenderDestination;
struct idRenderLightCommitted;
class idRenderModelSurface;
class idRenderWorldCommitted;
struct captureParms_t;
struct occlusionState_t;
struct renderPassParms_t;
enum renderCapture_t : int;
enum renderPass_t : int;

extern const idRenderMatrix renderMatrix_windowSpaceToClipSpace;

struct idViewBypass {
  bool allowBypass;
  std::uint8_t alignmentBeforeAngles[3];
  idVec3 deltaViewAngles;
  idMat3 angleTransform;
  idVec3 baseOrigin;
  float neckX;
  float neckZ;
};

struct idEnvBlendParms {
  ~idEnvBlendParms() noexcept(false) = default;

  void RemoveRenderParm(const idDeclRenderParm* parm);
  void Clear();
  void CopyFrom(const idEnvBlendParms& other);
  void SetFloat(const idDeclRenderParm* parm, float value, float blend);

  idList<const idDeclRenderParm*, 5> parms;
  idList<parmValue_t, 5> values;
  idList<float, 5> blends;
};

template <int Capacity>
struct idStaticEnvBlendParms : public idEnvBlendParms {
  idStaticEnvBlendParms() {
    parms.list = staticParms;
    parms.num = 0;
    parms.size = Capacity;
    parms.granularity = 1;
    parms.memTag = 5;
    parms.listStatic = 1;
    values.list = staticValues;
    values.num = 0;
    values.size = Capacity;
    values.granularity = 1;
    values.memTag = 5;
    values.listStatic = 1;
    blends.list = staticBlends;
    blends.num = 0;
    blends.size = Capacity;
    blends.granularity = 1;
    blends.memTag = 5;
    blends.listStatic = 1;
  }

  const idDeclRenderParm* staticParms[Capacity];
  parmValue_t staticValues[Capacity];
  float staticBlends[Capacity];
};

struct renderView_t {
  renderView_t();
  ~renderView_t() = default;
  void BuildProjectionMatrix(idRenderMatrix& projectionMatrix, int jitterWidth,
                             int jitterHeight) const;
  void BuildViewMatrix(idRenderMatrix& viewMatrix) const;

  int renderTime;
  int viewID;
  bool forceFullVirtualTextureLoad;
  bool discontinuousViewPosition;
  bool skipDimShadows;
  bool skipPostProcess;
  bool skipDynamicEnvironment;
  bool forceFullResolution;
  bool useHQViewColorMipsGeneration;
  bool inhibitModelFovScale;
  float fov_x;
  float fov_y;
  bool cramZNear;
  std::uint8_t alignmentBeforeProjection[3];
  idRenderMatrix explicitProjectionMatrix;
  bool useExplicitProjectionMatrix;
  std::uint8_t alignmentBeforeOrigin[3];
  idVec3 vieworg;
  idMat3 viewaxis;
  idViewBypass viewBypass;
  bool forceIdentityViewMatrix;
  std::uint8_t alignmentBeforeParmBlock[7];
  idStaticParmBlock<80> parmBlock;
  const idDeclEnv* dynEnvOverride;
  float dynEnvOverrideDuration;
  int dynEnvOverrideModelIndex;
  bool applyDynEnvOverride;
  std::uint8_t alignmentBeforeOverrideBlock[15];
  alignas(16) idStaticParmBlock<80> envOverrideParmBlock;
  idStaticEnvBlendParms<80> additiveParmBlock;
  bool radialBlurEnabled;
  std::uint8_t alignmentBeforeRadialCenter[3];
  idVec3 radialBlurCenterPos;
  std::uint8_t alignmentTail[8];
};

struct envBlend_t {
  const idDeclEnv* env;
  float blendStartTime;
  float blendDuration;
  int frameExposureFinished;
  bool cubeFinished;
  bool colorCorrectionFinished;
  bool hasOverrideParms;
  bool skipBlending;
};

struct alignas(16) renderSettings_t {
  std::uint8_t featureFlags;
  bool skipWorld;
  bool skipModels;
  bool skipLights;
  bool skipBlendLightShadows;
  bool skipSuppress;
  bool skipFeedback;
  bool skipDynamic;
  bool skipAddAlways;
  bool skipOpaqueSurfaces;
  bool skipSubSurfaceScattering;
  bool skipAutosprites;
  bool skipOcclusionBaseModel;
  bool skipBlendedSurfaces;
  bool skipDistortionSurfaces;
  bool skipEmissiveSurfaces;
  bool skipOcclusionSurfaces;
  bool skipGuis;
  bool skipInGameGuis;
  bool skipAdaptiveGlare;
  bool skipPostProcess;
  bool skipEmissiveGlare;
  bool skipBlendLights;
  bool skipSlowLights;
  bool skipSelfShadows;
  bool skipAmbientOcclusion;
  bool skipFog;
  bool skipGodRays;
  bool skipHazeFlare;
  bool skipPostSpecular;
  bool skipNodeCPUCulling;
  bool skipNodeGPUCulling;
  bool skipAreaCPUCulling;
  bool skipAreaGPUCulling;
  bool skipModelCPUCulling;
  bool skipModelGPUCulling;
  bool skipLightCPUCulling;
  bool skipLightGPUCulling;
  bool skipSurfaceCPUCulling;
  bool skipModelRangeCulling;
  bool skipLightRangeCulling;
  std::uint8_t alignmentBeforeSelectors[3];
  int screenSpaceReflections;
  int singleWorldArea;
  int singleLight;
  int singleModel;
  int singleSurface;
  int singleGuiSurface;
  int singleDimShadow;
  int singleSelfShadow;
  bool lockView;
  bool forceTwoSidedDepth;
  bool useAmbientEnv;
  bool feedbackBGRA;
  bool showOcclusionBoxes;
  bool forceOcclusionBoxQueries;
  std::uint8_t alignmentBeforeViewCounts[2];
  int numViews;
  int generateMipMaps;
  int guiFrameCount;
  int queryThreshold;
  int doPreZMask;
  bool dimShadowUseQuery;
  bool dimShadowDepthBoundsTest;
  std::uint8_t alignmentBeforeDimShadowSettings[2];
  int dimShadowResolution;
  int dimShadowResolutionCap;
  int dimShadowForceHighQuality;
  int showDimShadows;
  float dimShadowMaxLixelsPerUnit;
  float dimShadowLixelScale;
  float dimShadowDensity;
  float dimShadowPolyOfsUnits;
  float dimShadowPolyOfsFactor;
  float dimShadowMaxVisibleRange;
  float dimShadowFadeVisibilityRange;
  bool dimShadowSkipRangeCulling;
  std::uint8_t alignmentBeforeSelfShadowSettings[3];
  int selfShadowResolution;
  int selfShadowResolutionCap;
  float selfShadowDensity;
  float selfShadowPolyOfsUnits;
  float selfShadowPolyOfsFactor;
  float selfShadowMaxVisibleRange;
  float selfShadowFadeVisibilityRange;
  float selfShadowLixelScale;
  bool selfShadowSkipRangeCulling;
  std::uint8_t alignmentBeforeSubSurfaceSettings[3];
  float subSurfaceScatteringMaxVisibilityRange;
  float subSurfaceScatteringFadeVisibilityRange;
  bool subSurfaceScatteringSkipRangeCulling;
  bool sortBlendedSurfacesBackToFront;
  bool useLightScissors;
  bool useLightDepthBoundsTest;
  bool useDeferredSlowMapLighting;
  bool showLightScissors;
  std::uint8_t alignmentBeforePostProcessSettings[2];
  int showBlendedLights;
  int postProcessDofMode;
  float sortCoverage;
  float sortSSS;
  float sortSkybox;
  float sortBackgroundFar;
  float sortBackground;
  float sortBackgroundEmit;
  float sortEmit;
  float sortEmitOnly;
  float sortLight;
  float sortDecal;
  float sortTransSort;
  float sortTrans;
  float sortWater;
  float sortPerturber;
  float sortLast;
  float viewNearZ;
  float viewFarZ;
  float guiOriginOffset;
  const idDeclRenderProg* progShowGuiOverdraw;
  idVec3 clearColor;
  alignas(16) idVec4 viewOrigin;
  idBounds viewBounds;
  alignas(16) idBounds detailBounds;

  bool IsComboMap() const { return (featureFlags & 0x01u) != 0; }
  bool IsToolsWorld() const { return (featureFlags & 0x02u) != 0; }
  bool UseAmbientEnv() const { return useAmbientEnv; }
};

class alignas(16) idRenderView {
public:
  idRenderView();
  ~idRenderView() noexcept(false);

  void CommitParameters(int windowWidth, int windowHeight, int renderWidth,
                        int renderHeight);
  int GetModelOcclusionResult(int modelIndex) const;

  renderView_t g;
  const idDeclEnv* m_pModelEnvOverride;
  int viewIndex;
  std::uint8_t alignmentBeforeBlend[4];
  envBlend_t envBlend;
  idParmBlock prevEnvBlendParms;
  idParmBlock blendedEnvironmentParms;
  idParmBlock finalEnvironmentParms;
  std::uint8_t alignmentBeforeRenderView[8];
  renderView_t r;
  idRenderMatrix projectionMatrix;
  idRenderMatrix inverseProjectionMatrix;
  idRenderMatrix viewMatrix;
  idRenderMatrix inverseViewMatrix;
  idRenderMatrix worldSpaceMVPMatrix;
  idRenderMatrix worldSpaceInverseMVPMatrix;
  idRenderMatrix viewSpaceModelMatrix;
  idRenderMatrix viewSpaceInverseModelMatrix;
  idRenderMatrix viewSpaceMVPMatrix;
  int windowWidth;
  int windowHeight;
  int renderWidth;
  int renderHeight;
  int feedbackWidth;
  int feedbackHeight;
  idScreenRect viewport;
  idScreenRect scissorRect;
  idScreenRect testScissor;
  std::int64_t firstOcclusionQueryInPreviousFrame;
  occlusionState_t* occlusionState;
  std::uint8_t alignmentTail[8];
};

static_assert(sizeof(renderView_t) == 6528, "render-view parms ABI drift");
static_assert(offsetof(idRenderView, g) == 0,
              "render-view parms offset drift");
static_assert(sizeof(idRenderView) == 13920, "render-view ABI drift");

class idRender {
public:
  idRender();

  void WaitAndSwap(void* hdc, bool force30Hz);
  int GetLastUsedRenderWidth();
  int GetLastUsedRenderHeight();
  void InitSettings(const idRenderWorldCommitted* renderWorld,
                    const idRenderView* renderView, int numViews);
  void CaptureToViewColor(const idScreenRect& rect, int viewIndex);
  void RenderGuiModels(idRenderModelCommitted* const* models, int numModels,
                       const idRenderDestination* destination,
                       const idScreenRect& rect, float guiOriginOffset);
  void RenderSingleView(void* hdc,
                        const idRenderWorldCommitted* renderWorld,
                        idRenderView* renderView, const idScreenRect& rect);

private:
  void UpdateShadowBufferOptions();
  void SetRenderSize(idRenderView* renderView, const idScreenRect& rect);
  void PrintStats() const;
  void SetupCaptureParms(captureParms_t& parms, renderCapture_t capture,
                         const idScreenRect& rect, int viewIndex,
                         bool createMipMaps);
  void SetupRenderPassParms(renderPassParms_t& parms, renderPass_t pass,
                            const idScreenRect& rect, bool allowInGameGUIs);
  void SetupAdditiveEnvironmentParms(envBlend_t& blend,
                                     const idEnvBlendParms* additive,
                                     const idParmBlock* previous,
                                     idParmBlock* final);
  void SetupDynamicEnvironment(const idVec3& viewOrigin, envBlend_t& blend,
                               idParmBlock* blended, idParmBlock* previous,
                               const idParmBlock* overrideParms);

public:
  std::uint8_t tempConsidered[2048];
  std::uint8_t areaNodeBoundsCulled[8192];
  std::uint8_t worldAreaSubspaceBoundsCulled[8192];
  std::uint8_t worldAreaGeometryBoundsCulled[8192];
  std::uint8_t modelDimShadowState[8192];
  nodeWorldArea_t nodeWorldAreas[8192];
  viewWorldArea_t viewWorldAreas[8192];
  const idRenderModelSurface* viewWorldSurfaces[8192];
  std::uint64_t viewModelSortKeys[8192];
  const idRenderModelCommitted* viewModels[8192];
  const idRenderModelCommitted* viewModelsPreZ[8192];
  const idRenderLightCommitted* viewLights[8192];
  const idRenderModelCommitted* viewGodrayModels[16];
  int occlusionTestNodes[64];
  int occlusionTestWorldAreas[4096];
  const idRenderModelCommitted* occlusionTestModels[4096];
  const idRenderLightCommitted* occlusionTestLights[512];
  drawSurf_t drawSurfs[4096];
  drawSurf_t* sortedDrawSurfs[4096];
  drawSurf_t* depthSortedDrawSurfs[4096];
  std::int16_t dimShadowModelSlaves[8192];
  int numNodeWorldAreas;
  int numViewWorldAreas;
  int numViewWorldSurfaces;
  int numViewModels;
  int numViewModelsPreZ;
  int numViewSceneModels;
  int numViewGodrayModels;
  int numViewLights;
  int numOcclusionTestNodes;
  int numOcclusionTestWorldAreas;
  int numOcclusionTestModels;
  int numOcclusionTestLights;
  int numDrawSurfs;
  int numDepthSortedDrawSurfs;
  int numShadows;
  int numDimShadows;
  int numSelfShadows;
  renderPassDepthModels_t renderViewModels[17];
  renderPassSurfaces_t renderPasses[9];
  std::uint8_t alignmentBeforeSettings[12];
  alignas(16) renderSettings_t settings;
  idRenderView* renderViewObject;
  const idRenderWorldCommitted* world;
  idRenderModelCommitted worldSpace;
  idRenderModelCommitted viewSpace;
};

static_assert(sizeof(idViewBypass) == 72, "view bypass ABI drift");
static_assert(offsetof(idViewBypass, angleTransform) == 16,
              "view bypass transform offset drift");
static_assert(sizeof(idEnvBlendParms) == 72, "environment blend parms ABI drift");
static_assert(sizeof(idStaticEnvBlendParms<80>) == 2312,
              "static environment blend parms ABI drift");
static_assert(sizeof(idStaticParmBlock<80>) == 1968,
              "static parm block ABI drift");
static_assert(sizeof(renderView_t) == 6528, "render view parameters ABI drift");
static_assert(offsetof(renderView_t, parmBlock) == 224,
              "render view parm-block offset drift");
static_assert(offsetof(renderView_t, envOverrideParmBlock) == 2224,
              "render view override block offset drift");
static_assert(offsetof(renderView_t, additiveParmBlock) == 4192,
              "render view additive block offset drift");
static_assert(offsetof(renderView_t, radialBlurEnabled) == 6504,
              "render view radial-blur offset drift");
static_assert(sizeof(envBlend_t) == 24, "environment blend ABI drift");
static_assert(sizeof(renderSettings_t) == 400, "render settings ABI drift");
static_assert(offsetof(renderSettings_t, singleWorldArea) == 48,
              "render settings selector offset drift");
static_assert(offsetof(renderSettings_t, sortCoverage) == 216,
              "render settings sort offset drift");
static_assert(offsetof(renderSettings_t, viewOrigin) == 320,
              "render settings origin offset drift");
static_assert(offsetof(renderSettings_t, detailBounds) == 368,
              "render settings detail bounds offset drift");
static_assert(sizeof(idRenderView) == 13920, "render-view ABI drift");
static_assert(offsetof(idRenderView, envBlend) == 6544,
              "render-view environment blend offset drift");
static_assert(offsetof(idRenderView, r) == 6720,
              "render-view render payload offset drift");
static_assert(offsetof(idRenderView, projectionMatrix) == 13248,
              "render-view projection offset drift");
static_assert(offsetof(idRenderView, worldSpaceMVPMatrix) == 13504,
              "render-view world MVP offset drift");
static_assert(offsetof(idRenderView, windowWidth) == 13824,
              "render-view dimensions offset drift");
static_assert(offsetof(idRenderView, viewport) == 13848,
              "render-view viewport offset drift");
static_assert(offsetof(idRenderView, occlusionState) == 13904,
              "render-view occlusion-state offset drift");
static_assert(sizeof(idRender) == 797472, "renderer frame state ABI drift");
static_assert(offsetof(idRender, settings) == 793248,
              "renderer settings offset drift");
static_assert(offsetof(idRender, renderViewObject) == 793648,
              "renderer view pointer offset drift");
static_assert(offsetof(idRender, worldSpace) == 793664,
              "renderer world-space model offset drift");
static_assert(offsetof(idRender, viewSpace) == 795568,
              "renderer view-space model offset drift");
