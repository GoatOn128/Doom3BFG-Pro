#pragma once

#include "engine/renderer/renderersystem.h"
#include "engine/renderer/rendersystem_commands.h"
#include "shared/idlib/color.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/metrics/gaugemetric.h"

#include <cstddef>
#include <cstdint>

class idAutoComplete;
class idDeclRenderParm;
class idDeclRenderProg;
class idDeclTable;
class idImage;
class idMaterial;
class idRenderDestination;
class idRenderModelGui;
class idRenderThread;
class idRenderVideoOverlay;
class idRenderWorld;
class idStaticModel;
class idTriangles;

struct performanceCounters_t {
  int c_modelCommits;
  int c_lightCommits;
  int c_modelReferences;
  int c_lightReferences;
  int c_approxLightModels;
  int c_approxLightModelsWant;
  int c_viewWorldAreas;
  int c_viewModels;
  int c_viewLights;
  int c_viewShadows;
  int c_viewDimShadows;
  int c_viewSelfShadows;
  int c_modelTypeCommits[33];
  std::uint32_t alignmentBeforeTimes;
  std::uint64_t startGPUTimeMicroSec;
  std::uint64_t endGPUTimeMicroSec;
  std::uint64_t startLatchTimeMicroSec;
  std::uint64_t endLatchTimeMicroSec;
  std::uint64_t startCPUTimeMicroSec;
  std::uint64_t endCPUTimeMicroSec;
  std::uint64_t startSwapMicroSec;
  std::uint64_t endSwapMicroSec;
  std::uint64_t startWaitForEndFrameMicroSec;
  std::uint64_t endWaitForEndFrameMicroSec;
};

class alignas(8) idRenderSystemLocal : public idRenderSystem {
public:
  struct ogl_pc_t {};

#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
  idRenderSystemLocal()
      : drawsMetric(idStr(), idStr()), trisMetric(idStr(), idStr()),
        vertsMetric(idStr(), idStr()) {}
  ~idRenderSystemLocal() override = default;
#else
  idRenderSystemLocal();
  ~idRenderSystemLocal() override;
#endif

  void Init() override;
  void Init2() override;
  void Shutdown() override;
  void RenderFrame(void* const hdc, const idRenderFrameInfo& renderFrameInfo,
                   bool allowSMP, bool skipSwapBuffers,
                   bool resolveDepth) override;
  void EndFrame(void* hdc, int windowWidth, int windowHeight,
                idRenderWorld* world, idRenderModel** guis, int numGuis,
                bool allowSMP, bool skipSwapBuffers, bool resolveDepth,
                int timeMsec) override;
  void ToolEndFrame(void* hdc, int windowWidth, int windowHeight,
                    idRenderWorld* world, const renderView_t& renderView,
                    idRenderModel* gui, int timeMsec) override;

  int FrameNumber() override;
  bool SyncRenderThread(bool) override;
  void SwapBuffers(void*, bool) override;
  int GetWidth() override;
  int GetHeight() override;
  bool IsFullscreen() override;
  bool IgnoreSizeMessages() override;
  stereoRenderMode_t GetStereoRenderMode() override;
  float GetPixelAspect() override;
  void SetGamma(std::uint16_t*, std::uint16_t*, std::uint16_t*) override;
  int GetNumMultisamples() override;
  int GetMaxSupportedMultisamples() override;
  bool GetUsingSMP() override;
  idThreadGraphicsContext* AllocThreadGraphicsContext() override;
  void DestroyThreadGraphicsContext(idThreadGraphicsContext*) override;
  void WindowSizeDragged(int, int) override;
  bool WindowChangeMultisamples(int, bool) override;
  bool EnumerateModes(int, int*, int*, int*) override;
  void (*ExtensionPointer(const char*))() override;
  void SetPixelFormat(void*) override;
  void ShowGameWindow() override;
  void HideGameWindow() override;
  void GoWindowed() override;
  void GoFullScreen() override;
  void SetStudioMode(bool) override;
  void InitContext() override;
  void Restart() override;
  void* GetGameWindow() override;
  bool MakeCurrent(void*) override;
  int GetCurrentMonitor() override;
  int GetNumMonitors() override;
  void GetMonitorSize(int, int*, int*) override;
  void BeginBinkVideo(void* hdc, const idVideoDesc* videoDesc,
                      videoFlags_t videoFlags, void* xAudioHardware,
                      int soundSystemSpeakerMask,
                      float binkVolumeDBFloat,
                      idRenderVideoOverlay* overlay, bool force) override;
  bool BinkVideoIsLoaded() override;
  void BinkVideoPause(bool pause) override;
  bool BinkVideoIsPlaying() override;
  bool BinkVideoPlay() override;
  void FreeBinkVideo() override;
  void SetRenderFlag(renderFlags_t flag) override;
  void RemoveRenderFlag(int flags) override;
  bool HasRenderFlag(int renderFlag) override;
  bool CanThreadContinue() override;
  void RestartFromLoading() override;
  void AllowBackgroundSwaps(bool allow) override;
  void BeginAutomaticBackgroundSwaps(void* hdc, bool showLoadingIcon,
                                     bool showOldFrame) override;
  void EndAutomaticBackgroundSwaps() override;
  void SetLoadingIconInfo(float posX, float posY, float scale, float speed,
                          bool render) override;
  idRenderWorld* AllocRenderWorld(const char* mapName, bool comboMap,
                                  bool toolMap) override;
  void FreeRenderWorld(idRenderWorld* world) override;
  void RegenerateReferences() override;
  void InhibitEndFrameRendering(int change) override;
  bool IsEndFrameRenderingInhibited() override;
  int GetNumRenderWorlds() override { return worlds.Num(); }
  idRenderWorld* RenderWorldForIndex(int index) override;
  bool IsAutomaticBackgroundSwapsEnabled() override;
  bool SwapTearAvailable() override;
  std::int64_t GetFreeVRamAtInit() override;
  void ClearOutOfMemory() override;
  bool CheckOutOfMemory() override;
  void SetBlackLevel(float blackLevel) override;
  float GetScreenAspect() override;
  void LogMetrics();
  void PrintStatistics();
  bool WaitForNextVideoFrame(bool isRenderThread);
  unsigned GetBinkWidth() const;
  unsigned GetBinkHeight() const;
  float GetBinkTime() const;
  void UpdateBinkTextures();
  void RestartRenderThread(void* hdc);
  void PreliminaryRenderSetup();
  void CreateMainThreadContext();
  void CreateRenderThreadContext();
  void StartRenderThread();
  void ShutdownSystem();
  void SetColorMappings(float gamma, float brightness, float blackLevel);
  void InitShadowMapTextures();
  void InitInGameGuiRenderTexture();
  void InitUnitSquare();
  void InitRenderTextures();
  static void ArgCompletion_ImageName(idAutoComplete& completion);
  bool CaptureCurrentRenderTargetForScreenShot(int width, int height,
                                                bool fillAlphaWith255,
                                                void* resultBuffer);
  void TakeRawScreenshot(const char* fileName, fsPath_t fsPath,
                         screenshotFormat_t format, bool fillAlphaWith255);
  void TakeScreenshotToBuffer(int width, int height,
                              idRenderWorld* world, idRenderModel** guiList,
                              int numGuis, int blends,
                              unsigned char* outputBuffer, int primes,
                              bool fillAlphaWith255,
                              bool skipPostProcessing);
  void TakeScreenshot(int width, int height, idRenderWorld* world,
                      idRenderModel** guiList, int numGuis,
                      const char* fileName, int blends, fsPath_t fsPath,
                      screenshotFormat_t format, int primes,
                      bool fillAlphaWith255, bool skipPostProcessing);
  void InitCommands();

  bool insideEndFrame;
  bool automaticBackgroundSwapsEnabled;
  bool renderingIsReadyForSwapbuffers;
  std::uint8_t alignmentBeforeRenderFlags;
  int renderFlags;
  bool allowBackgroundSwaps;
  volatile bool renderSynced;
  std::uint8_t alignmentBeforeFrameCount[2];
  int frameCount;
  int inhibitRendering;
  idList<idRenderWorld*, 5> worlds;
  idRenderWorld* primaryWorld;
  idImage* testImage;
  idImage* testCubeImage;
  const idMaterial* testMaterial;
  idRenderWorld* performanceCounterLogWorld;
  int performanceCounterLogWorldFrame;
  std::uint32_t alignmentBeforeCounters;
  performanceCounters_t pc;
  idGaugeMetric drawsMetric;
  idGaugeMetric trisMetric;
  idGaugeMetric vertsMetric;
  bool videoPlaying;
  std::uint8_t alignmentBeforeVideoOverlay[7];
  idRenderVideoOverlay* videoOverlay;
  idRenderModelGui* videoOverlayModel;
  idColor blockColor;
  int blockColorFrameNum;
  std::uint32_t alignmentBeforeMaterials;
  const idMaterial* mtrDefault;
  const idMaterial* mtrDefaultPointLight;
  const idMaterial* mtrDefaultProjectedLight;
  const idMaterial* mtrDefaultParallelLight;
  const idMaterial* mtrDefaultPointBlendLight;
  const idMaterial* mtrDefaultProjectedBlendLight;
  const idMaterial* mtrDefaultParallelBlendLight;
  const idMaterial* mtrDefaultPointSlowLight;
  const idMaterial* mtrDefaultProjectedSlowLight;
  const idMaterial* mtrDefaultParallelSlowLight;
  const idDeclRenderProg* progGauss;
  const idDeclRenderProg* progGlareScale;
  const idDeclRenderProg* progPostProcess;
  const idDeclRenderProg* progOverlappedDownSample;
  const idDeclRenderProg* progFeedbackDownSample;
  const idDeclRenderProg* progBloomThreshold;
  const idDeclRenderProg* progLuminosity;
  const idDeclRenderProg* progAdaptiveGlareGauss;
  const idDeclRenderProg* progFrameExposure;
  const idDeclRenderProg* progSlowDepthOfField;
  const idDeclRenderProg* progOverlappedDownSampleMaxAlpha;
  const idDeclRenderProg* progCalcDepthOfFieldValues;
  const idDeclRenderProg* progCalcDepthOfFieldMaxKernel;
  const idDeclRenderProg* progDepthOfFieldFilterPass;
  const idDeclRenderProg* progDepthOfFieldDoubleFilterPass;
  const idDeclRenderProg* progDepthOfFieldFilterPass_HQ;
  const idDeclRenderProg* progDepthOfFieldDoubleFilterPass_HQ;
  const idDeclRenderProg* progHazeFlareBlurPass1;
  const idDeclRenderProg* progHazeFlareBlurPass2;
  const idDeclRenderProg* progHazeFlare;
  const idDeclRenderProg* progScreenSpaceReflections;
  const idDeclRenderProg* progScreenSpaceReflectionsApply;
  const idDeclRenderProg* progDepthOnly;
  const idDeclRenderProg* progColorOnly;
  const idDeclRenderProg* progColorParmOnly;
  const idDeclRenderProg* progWireFrame;
  const idDeclRenderProg* progTextureOnly;
  const idDeclRenderProg* progCubeImageOnly;
  const idDeclRenderProg* progTestImage;
  const idDeclRenderProg* progTestImageShadow;
  const idDeclRenderProg* progTestImageMultisample;
  const idDeclRenderProg* progBasicBlend;
  const idDeclRenderProg* progEnvironmentBlend;
  const idDeclRenderProg* progColorCorrectionBlend;
  const idDeclRenderProg* progOccluderDepthOnly;
  const idDeclRenderProg* progOccluderUnique;
  const idDeclRenderProg* progLandUniqueBump;
  const idDeclRenderProg* progLandUniqueDiffuseOnly;
  const idDeclRenderProg* progBlendedLightDebugDraw;
  const idDeclRenderProg* progDeferredPointLight;
  const idDeclRenderProg* progDeferredSpotLight;
  const idDeclRenderProg* progDeferredParallelLight;
  const idDeclRenderProg* progInteractionPrePass;
  const idDeclRenderProg* progInteractionPostPass;
  const idDeclRenderProg* progVarianceShadowMap;
  const idDeclRenderProg* progDimShadowDrawMidQuality;
  const idDeclRenderProg* progGlobalFog;
  const idDeclRenderProg* progCreateGodRayMask;
  const idDeclRenderProg* progDrawGodRay;
  const idDeclRenderParm* rpGlareMap;
  const idDeclRenderParm* rpGlareStep;
  const idDeclRenderParm* rpDistortionMap;
  const idDeclRenderParm* rpGrainMap;
  const idDeclRenderParm* rpDynamicEnvMap;
  const idDeclRenderParm* rpPowerMip;
  const idDeclRenderParm* rpFrameNumber;
  const idDeclRenderParm* rpVirtualTextureFeedbackFloat;
  const idDeclRenderParm* rpPageTableMap;
  const idDeclRenderParm* rpVmtrPageTableMap;
  const idDeclRenderParm* rpDepthOfField2;
  const idDeclRenderParm* rpGodRayQuadScaleBias;
  const idDeclRenderParm* rpGodRaySourceScaleBias;
  const idDeclRenderParm* rpGodRaysColor;
  const idDeclRenderParm* rpGodRayViewPos;
  const idDeclRenderParm* rpGammaForBrightness;
  const idDeclRenderParm* rpAdaptiveGlareMap;
  const idDeclRenderParm* rpAdaptiveGlareMapAlt;
  const idDeclRenderParm* rpLuminosityMap;
  const idDeclRenderParm* rpLuminosityMapAlt;
  const idDeclRenderParm* rpAdaptiveLuminance;
  const idDeclRenderParm* rpAdaptiveGlareBlurMap;
  const idDeclRenderParm* rpAdaptiveGlareBlurStep;
  const idDeclRenderParm* rpAdaptiveGlareParms;
  const idDeclRenderParm* rpAdaptiveGlareColor;
  const idDeclRenderParm* rpFrameExposureMap;
  const idDeclRenderParm* rpFrameExposureParms1;
  const idDeclRenderParm* rpFrameExposureParms2;
  const idDeclTable* declHighlightTable;
  const idDeclTable* declHighlightExplodeTable;
  const idStaticModel* defaultCubeModel;
  const idStaticModel* unitCubeModel;
  const idStaticModel* zeroOneCubeModel;
  const idStaticModel* extrudeBoxModel;
  const idStaticModel* unitSquareModel;
  const idStaticModel* zeroOneSquareModel;
  idTriangles* unitSquareTris;
  idTriangles* unitSquareGuiTris;
  idImage* imgViewColor[2];
  idImage* imgViewDepth;
  idImage* imgColorGradingLUT[2];
  idImage* imgDistortion[2];
  idImage* imgScreenMip3;
  idImage* imgScreenMip3Alt;
  idImage* imgGui;
  idImage* imgDynamicEnvironment[2];
  idImage* imgDynamicColorCorrection[2];
  idImage* imgGrain;
  idImage* imgLuminosity;
  idImage* imgLuminosityAlt;
  idImage* imgFrameExposure;
  idImage* imgFrameExposureAlt;
  idImage* imgBloom;
  idImage* imgHazeFlare[2];
  idImage* imgViewPostScratch;
  idRenderDestination* renderDestViewColor[2];
  idRenderDestination* renderDestViewDepth;
  idRenderDestination* renderDestMip1;
  idRenderDestination* renderDestMip3;
  idRenderDestination* renderDestMip3Alt;
  idRenderDestination* renderDestDistortion[2];
  idRenderDestination* renderDestGui;
  idRenderDestination* renderDestDynamicEnv[2];
  idRenderDestination* renderDestDynamicColorCorrection[2];
  idRenderDestination* renderDestShadow;
  idRenderDestination* renderDestLuminosity;
  idRenderDestination* renderDestLuminosityAlt;
  idRenderDestination* renderDestFrameExposure;
  idRenderDestination* renderDestFrameExposureAlt;
  idRenderDestination* renderDestBloom;
  idRenderDestination* renderDestHazeFlare[2];
  idRenderDestination* renderDestViewPostScratch;
  float blackLevel[2];
  std::uint16_t gammaTable[256];
  bool updateGammaTable;
  bool reblendColorCorrection;
  std::uint8_t alignmentBeforeBrightness[2];
  float gammaForBrighness;
  idRenderThread* renderThread;
  ogl_pc_t ogl_pc;
};

#if defined(WOLF_RENDERER_RENDERWORLDCOMMITTED_TEST_ACCESS)
alignas(idRenderSystemLocal) extern unsigned char
    renderWorldCommittedTestRenderSystemStorage[sizeof(idRenderSystemLocal)];
#define tr (*reinterpret_cast<idRenderSystemLocal*>(                         \
    renderWorldCommittedTestRenderSystemStorage))
#elif defined(WOLF_RENDERER_RENDERTHREAD_TEST_ACCESS)
alignas(idRenderSystemLocal) extern unsigned char
    renderThreadTestRenderSystemStorage[sizeof(idRenderSystemLocal)];
#define tr (*reinterpret_cast<idRenderSystemLocal*>(                         \
    renderThreadTestRenderSystemStorage))
#else
extern idRenderSystemLocal tr;
#endif

static_assert(sizeof(performanceCounters_t) == 264,
              "performance counters ABI drift");
static_assert(offsetof(performanceCounters_t, startGPUTimeMicroSec) == 184,
              "performance counter timing offset drift");
static_assert(sizeof(idRenderSystemLocal) == 2480,
              "local render system ABI drift");
static_assert(offsetof(idRenderSystemLocal, frameCount) == 20,
              "local render system frame counter offset drift");
static_assert(offsetof(idRenderSystemLocal, pc) == 104,
              "local render system counters offset drift");
static_assert(offsetof(idRenderSystemLocal, mtrDefault) == 800,
              "local render system default material offset drift");
static_assert(offsetof(idRenderSystemLocal, mtrDefaultPointLight) == 808,
              "local render system default light offset drift");
static_assert(offsetof(idRenderSystemLocal, progGauss) == 880,
              "local render system program offset drift");
static_assert(offsetof(idRenderSystemLocal, rpGlareMap) == 1272,
              "local render system parm offset drift");
static_assert(offsetof(idRenderSystemLocal, imgViewColor) == 1576,
              "local render system image offset drift");
static_assert(offsetof(idRenderSystemLocal, renderDestViewColor) == 1760,
              "local render system destination offset drift");
static_assert(offsetof(idRenderSystemLocal, renderThread) == 2464,
              "local render system thread offset drift");
