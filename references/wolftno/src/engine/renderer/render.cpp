#include "engine/renderer/render.h"

#include "engine/framework/coderesource.h"
#include "engine/framework/console.h"
#include "engine/decls/declenv.h"
#include "engine/decls/decltable.h"
#include "engine/models/rendermodelgui.h"
#include "engine/models/static/staticmodel.h"
#include "engine/models/renderworld.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/jobs/render/rendercull.h"
#include "engine/renderer/jobs/render/rendercapture.h"
#include "engine/renderer/jobs/render/renderdepth.h"
#include "engine/renderer/jobs/render/renderdimshadows.h"
#include "engine/renderer/jobs/render/renderdrawsurf.h"
#include "engine/renderer/jobs/render/renderdynamiccolorcorrection.h"
#include "engine/renderer/jobs/render/renderdynamicenv.h"
#include "engine/renderer/jobs/render/renderfog.h"
#include "engine/renderer/jobs/render/rendergather.h"
#include "engine/renderer/jobs/render/renderglare.h"
#include "engine/renderer/jobs/render/rendergodrays.h"
#include "engine/renderer/jobs/render/renderhazeflare.h"
#include "engine/renderer/jobs/render/renderlights.h"
#include "engine/renderer/jobs/render/renderocclusion.h"
#include "engine/renderer/jobs/render/renderpasses.h"
#include "engine/renderer/jobs/render/renderpostprocess.h"
#include "engine/renderer/jobs/render/renderscreenspacereflections.h"
#include "engine/renderer/jobs/render/renderselfshadows.h"
#include "engine/renderer/jobs/render/rendersort.h"
#include "engine/renderer/jobs/render/rendersortdepth.h"
#include "engine/renderer/jobs/render/renderwalkbsp.h"
#include "engine/renderer/jobs/render/setupcolorgrading.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/renderlocal.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/renderlightcommitted.h"
#include "engine/renderer/resolutionscale.h"
#include "engine/renderer/texturesampler.h"
#include "engine/renderer/virtualtexture.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/geometry/polytope.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>

extern idCVar r_clear;
extern idCVar r_forceFullVirtualTextureLoad;
extern idCVar r_forceTwoSided;
extern idCVar r_multiSamples;
extern idCVar r_showLights;
extern idCVar r_showModels;
extern idCVar r_skipEmissiveGlare;
extern idCVar r_skipPostProcess;
extern idCVar r_useDynamicEnvironment;
extern idCVar r_useFloatBuffers;
extern idCVar r_usePBOTextureFeedback;
extern idCVar r_zfar;
extern idCVar r_znear;
extern int deltaMicroseconds;
extern int lastUsercmdGenMicroSeconds;
extern int time_GPUFrameTime;

#define RENDER_CVAR(variable, defaultValue, cvarFlags, cvarDescription) \
  idCVar variable(#variable, defaultValue, cvarFlags, cvarDescription)
#define RENDER_CVAR_RANGE(variable, defaultValue, cvarFlags, cvarDescription, \
                          minimumValue, maximumValue)                         \
  idCVar variable(#variable, defaultValue, cvarFlags, cvarDescription,        \
                  minimumValue, maximumValue)

RENDER_CVAR(r_doPreZMask, "0", 2, "do pre-z pass mask (bit 0 = scenemodels, bit 1 = world areas, bit 2 = prez view models)");
RENDER_CVAR(r_skipWorld, "0", 1, "skip drawing all world geometry");
RENDER_CVAR(r_skipModels, "0", 1, "skip drawing all dynamic models");
RENDER_CVAR(r_skipLights, "0", 1, "skip drawing all dynamic lights");
RENDER_CVAR(r_skipBlendLightShadows, "0", 1, "skip shadows from blend lights");
RENDER_CVAR(r_skipShadows, "0", 1, "just use the existing depth texture contents");
RENDER_CVAR(r_skipShadowOccluders, "0", 1, "do the clear, but no triangle draws");
RENDER_CVAR(r_skipSuppress, "0", 1, "ignore the per-view suppressions");
RENDER_CVAR(r_skipFeedback, "0", 1, "skip the feedback rendering");
RENDER_CVAR(r_skipDynamic, "0", 1, "skip dynamic models");
RENDER_CVAR(r_skipAddAlways, "0", 1, "don't include add-always models");
RENDER_CVAR(r_skipOpaqueSurfaces, "0", 1, "skip opaque surfaces");
RENDER_CVAR(r_skipSubSurfaceScattering, "0", 1, "skip sub-surface scattering");
RENDER_CVAR(r_skipPostSpecular, "0", 1, "skips the specular in post pass and moves it to the base pass");
RENDER_CVAR(r_skipAutosprites, "0", 1, "skip the autosprites");
RENDER_CVAR(r_skipOcclusionBaseModel, "0", 1, "use sound BSP derived base occluder triangles");
RENDER_CVAR(r_skipDimShadows, "0", 1, "skip the dim-shadow rendering");
RENDER_CVAR(r_skipSelfShadows, "0", 1, "skip rendering three-point self-shadows");
RENDER_CVAR(r_skipAmbientOcclusion, "0", 1, "skip occlusion of the approximate dynamic ambient lighting");
RENDER_CVAR(r_skipBlendedSurfaces, "0", 1, "skip blended surfaces");
RENDER_CVAR(r_skipDistortionSurfaces, "0", 1, "skip perturb surfaces");
RENDER_CVAR(r_skipEmissiveSurfaces, "0", 1, "skip emissive surfaces");
RENDER_CVAR(r_skipOcclusionSurfaces, "0", 1, "skip occlusion surfaces");
RENDER_CVAR(r_skipGuis, "0", 2, "skip rendering guis, 1 = in-game, 2 = all");
RENDER_CVAR(r_skipFog, "0", 1, "skip fog");
RENDER_CVAR(r_skipGodRays, "0", 1, "skip godrays");
RENDER_CVAR(r_skipAdaptiveGlare, "0", 1, "skip Adaptive Glare");
RENDER_CVAR(r_screenSpaceReflections, "0", 2, "screen-space ray march reflections. 0 = disabled, 1 = enabled");
RENDER_CVAR(r_skipHazeFlare, "0", 1, "skip Haze/Flare post effect");
RENDER_CVAR(r_skipNodeCPUCulling, "0", 1, "skip CPU MVP culling of world nodes");
RENDER_CVAR(r_skipNodeGPUCulling, "0", 1, "skip GPU occlusion testing of world nodes");
RENDER_CVAR(r_skipAreaCPUCulling, "0", 1, "skip CPU MVP culling of world areas");
RENDER_CVAR(r_skipAreaGPUCulling, "0", 1, "skip GPU occlusion testing of world areas");
RENDER_CVAR(r_skipModelCPUCulling, "0", 1, "skip CPU MVP culling of models");
RENDER_CVAR(r_skipModelGPUCulling, "0", 1, "skip GPU occlusion testing of models");
RENDER_CVAR(r_skipLightCPUCulling, "0", 1, "skip CPU MVP culling of lights");
RENDER_CVAR(r_skipLightGPUCulling, "0", 1, "skip GPU occlusion testing of lights");
RENDER_CVAR(r_skipSurfaceCPUCulling, "0", 1, "skip CPU MVP culling of model surfaces");
RENDER_CVAR(r_skipModelRangeCulling, "0", 1, "allow models to be removed at distance");
RENDER_CVAR(r_skipLightRangeCulling, "0", 1, "allow lights to be removed at distance");
RENDER_CVAR(r_skipShadowModelCPUCulling, "0", 1, "skip CPU MVP culling of shadow casting models");
RENDER_CVAR(r_skipShadowSurfaceCPUCulling, "0", 1, "skip CPU MVP culling of shadow casting surfaces");
RENDER_CVAR(r_skipViewParms, "0", 1, "don't use the parmBlock in idRenderView");
RENDER_CVAR(r_showParms, "0", 1, "print all world / view parmBlock assignments");
RENDER_CVAR(r_skipAdditiveEnvParms, "0", 1, "Turn on/off additive render-view environment parms");
RENDER_CVAR(r_generateMipMaps, "1", 18, "0 = overlapped downsample, 1 = glGenerateMipMap");
RENDER_CVAR(r_lockView, "0", 1, "lock the current view");
RENDER_CVAR(r_singleWorldArea, "-1", 2, "suppress all but one world area");
RENDER_CVAR(r_singleLight, "-1", 2, "suppress all but one light");
RENDER_CVAR(r_singleModel, "-1", 2, "suppress all but one model");
RENDER_CVAR(r_singleSurface, "-1", 2, "suppress all but one surface on each model");
RENDER_CVAR(r_singleGuiSurface, "-1", 2, "suppress all but one surface on gui model");
RENDER_CVAR(r_singleDimShadow, "-1", 2, "only render the dim-shadow for a single model");
RENDER_CVAR(r_singleSelfShadow, "-1", 2, "only render self-shadows for a single model");
RENDER_CVAR(r_queryThreshold, "4", 2, "minimum return value from a query");
RENDER_CVAR(r_manifoldMaxDetailWidth, "32", 4, "");
RENDER_CVAR(r_detailDistancePixelThreshold, "3", 4, "pixel width to use when determining the maximum detail distance");
RENDER_CVAR(r_envBlendShow, "0", 2, "report the dynamic blend factors");
RENDER_CVAR(r_envBlendTime, "-1.0", 4, "if >= 0, the seconds to completely fade out an old environment");
RENDER_CVAR(r_envBlendAmbient, "1", 2, "use ambientEnvMap instead of userEnvMap for the lowest mip level");
RENDER_CVAR(r_envBlendSkip, "0", 1, "if true we ignore blending between environment maps and sets it directly instead");
RENDER_CVAR(r_skipSlowLights, "0", 1, "1 = don't draw slow lights");
RENDER_CVAR(r_skipBlendLights, "0", 1, "1 = don't draw blend lights");
RENDER_CVAR(r_useLightScissors, "1", 1, "use custom scissor rectangle for each light based on projection");
RENDER_CVAR(r_useLightDepthBoundsTest, "1", 1, "use depth bounds test on blended lights for early fragment rejection");
RENDER_CVAR(r_useDeferredSlowMapLighting, "0", 1, "");
RENDER_CVAR(r_sortBlendedSurfacesBackToFront, "1", 1, "Sort blended surfaces so that they draw back-to-front");
RENDER_CVAR(r_showLightScissors, "0", 1, "show light scissor rectangles");
RENDER_CVAR(r_showBlendedLights, "0", 2, "1 = visualize the blendlights boxes, 2 = visualize blendlights region");
RENDER_CVAR(r_slowMapWorldAmbient, "0.075", 4, "amount of ambient lighting to use on the world in slow map");
RENDER_CVAR(r_blendLightShadowSizeScaler, "1.0", 4, "Size of the shadowbuffer to use for blendlight shadows");
RENDER_CVAR(r_subSurfaceScatteringMaxVisibleRange, "512.0", 4, "Sub surface scattering are culled at this range from viewer");
RENDER_CVAR(r_subSurfaceScatteringFadeVisibilityRange, "384.0", 4, "Sub surface scattering start fading at this range from viewer");
RENDER_CVAR(r_subSurfaceScatteringSkipRangeCulling, "0", 1, "Sub surface scattering is never ranged culled");
RENDER_CVAR(r_dimShadowMaxLixelsPerUnit, "8.0", 4, "Cap shadow map size to a factor of bounding box radius.");
RENDER_CVAR(r_dimShadowLixelScale, "1.0", 4, "clip space to lixel scale factor, 0 disables");
RENDER_CVAR(r_selfShadowLixelScale, "1.0", 4, "clip space to lixel scale factor for selfshadows, 0 disables");
RENDER_CVAR(r_dimShadowDensity, "0.75", 4, "1.0 = full dark shadows, 0.0 = no shadows");
RENDER_CVAR(r_dimShadowUseQuery, "1", 1, "Use occlusion queries");
RENDER_CVAR(r_dimShadowForceHighQuality, "2", 2, "2 = all models use high quality dim-shadows, 1 = all models use mid quality dim-shadows, -1 = no models do");
RENDER_CVAR(r_showDimShadows, "0", 2, "visualize the dimShadow boxes");
RENDER_CVAR(r_skipDimShadowRangeCulling, "0", 1, "1 = skip dim-shadow range culling");
RENDER_CVAR(r_dimShadowMaxVisibleRange, "2200.0", 4, "dim-shadows are culled at this range from viewer");
RENDER_CVAR(r_dimShadowFadeVisibilityRange, "1800.0", 4, "dim-shadows start fading at this range from viewer");
RENDER_CVAR(r_selfShadowDensity, "1.0", 4, "1.0 = full dark shadows, 0.0 = no shadows");
RENDER_CVAR(r_skipSelfShadowRangeCulling, "0", 1, "1 = skip self-shadow range culling");
RENDER_CVAR(r_selfShadowMaxVisibleRange, "1600.0", 4, "self-shadows are culled at this range from viewer");
RENDER_CVAR(r_selfShadowFadeVisibilityRange, "1250.0", 4, "self-shadows start fading at this range from viewer");
RENDER_CVAR(r_dimShadowPolyOfsFactor, "2.0", 4, "polygon offset factor for drawing shadow buffer");
RENDER_CVAR(r_dimShadowPolyOfsUnits, "2000", 4, "polygon offset units for drawing shadow buffer");
RENDER_CVAR(r_selfShadowPolyOfsFactor, "5.0", 4, "polygon offset factor for drawing shadow buffer");
RENDER_CVAR(r_selfShadowPolyOfsUnits, "10000", 4, "polygon offset units for drawing shadow buffer");
RENDER_CVAR(r_selfShadowDebugImage, "0", 1, "Capture self-shadow buffer to debug scratch output");
RENDER_CVAR(r_dimShadowDepthBoundsTest, "1", 1, "use depth bounds test on dim shadows for early fragment rejection");
RENDER_CVAR(r_sb_polyOfsFactor, "2", 4, "polygon offset factor for drawing shadow buffer");
RENDER_CVAR(r_sb_polyOfsUnits, "2000", 4, "polygon offset units for drawing shadow buffer");
RENDER_CVAR(r_sb_parallelPolyOfsFactor, "2", 4, "polygon offset factor for drawing parallel light shadow buffer");
RENDER_CVAR(r_sb_parallelPolyOfsUnits, "2000", 4, "polygon offset units for drawing parallel light shadow buffer");
RENDER_CVAR_RANGE(r_sb_imageSize, "4096", 2, "Dimensions of the shadow buffer image (used for dimshadows and interaction light shadows)", 256.0f, 16384.0f);
RENDER_CVAR(r_dimShadowResolutionCap, "2048", 2, "Cap dimShadow size.");
RENDER_CVAR(r_selfShadowResolutionCap, "2048", 2, "Cap dimShadow size.");
RENDER_CVAR(r_sb_linearFilter, "1", 1, "use GL_LINEAR instead of GL_NEAREST on shadow maps");
RENDER_CVAR(r_sb_occluderFacing, "0", 2, "0 = front faces, 1 = back faces, 2 = front and back sides separately with polygon offset");
RENDER_CVAR(r_sb_singleSide, "-1", 2, "only draw a single side (0-5) of point lights");
RENDER_CVAR(r_sb_sliceStep, "512", 4, "initial ortho size for first slice");
RENDER_CVAR(r_sb_sliceRampScale, "2", 4, "larger values perform a faster triliner lerp between slices");
RENDER_CVAR(r_showGuiOverdraw, "0", 1, "");
RENDER_CVAR(r_showDepthOfFieldDebug, "0", 1, "Show depth of field calculations");
RENDER_CVAR(r_showGrainLuminanceDebug, "0", 1, "Show grains screen luminance scale calculations");
RENDER_CVAR(r_postProcessHQ, "0", 1, "Set high quality post process, for cutscene rendering");
RENDER_CVAR(r_postProcessDofMode, "5", 2, "0 = cheap, 1 = slow, 2 = box, 3 = box+blur, 4 = bokeh, 5 = bokeh+blur");
RENDER_CVAR(r_feedbackBGRA, "0", 16385, "use BGRA instead of RGBA for the feedback buffer");
RENDER_CVAR(r_showOcclusionBoxes, "0", 1, "");
RENDER_CVAR(r_extraClears, "1", 2, "To simulate slower rendering scenes, draw this many extra full screen clears. Negative value for purple clears");
RENDER_CVAR(r_purpleClear, "0", 2, "Clear with purple (also forces main buffer clear)");
RENDER_CVAR(r_extraWorldPassForSlowDown, "0", 2, "Render world areas multiple times to slow down rendering for debugging");
RENDER_CVAR(r_skipDepthViewStencil, "0", 1, "Skip stencil pass when rendering first-person head and body models.");
RENDER_CVAR(r_forceOcclusionBoxQueries, "0", 1, "force the use of occlusion query boxes");
RENDER_CVAR(r_adaptiveGlareTest, "0", 1, "Adaptive Glare test value overrides");
RENDER_CVAR(r_adaptiveGlareMinThreshold, "0.2", 4, "Adaptive Glare Min Threshold (r_adaptiveGlareTest must be on)");
RENDER_CVAR(r_adaptiveGlareMaxThreshold, "0.8", 4, "Adaptive Glare Max Threshold (r_adaptiveGlareTest must be on)");
RENDER_CVAR(r_adaptiveGlareAvgBrightness, "0.5", 4, "Adaptive Glare Avg Brightness (r_adaptiveGlareTest must be on)");
RENDER_CVAR(r_adaptiveGlareDecayRate, "0.1", 4, "Adaptive Glare Decay Rate (r_adaptiveGlareTest must be on)");
RENDER_CVAR(r_frameExposureMin, "0.5", 4, "Shutter speed / frame exposure min threshold");
RENDER_CVAR(r_frameExposureMax, "0.7", 4, "Shutter speed / frame exposure max threshold");
RENDER_CVAR(r_frameExposureDecay, "0.9", 4, "Shutter speed / frame exposure decay rate");
RENDER_CVAR(r_frameExposureSaturation, "0.15", 4, "Shutter speed / frame exposure color saturation");
RENDER_CVAR(r_frameExposureTest, "0", 1, "Shutter speed / frame exposure effect");
RENDER_CVAR(r_highlightSynchronized, "0", 1, "All highlights in phase");
RENDER_CVAR(r_highlightColor, "", 0, "default highlight color on 'useables'");
RENDER_CVAR(r_highlightEnable, "1", 9, "Highlights are enabled");
RENDER_CVAR(r_glFinishBeforeGetQuery, "0", 1, "run glFinish() before running the culling code (for testing)");
RENDER_CVAR(r_firstPersonModelsLast, "0", 1, "Render first person models after all other models");

#undef RENDER_CVAR_RANGE
#undef RENDER_CVAR

namespace {

#define RENDER_PARM(variable, resourceName) \
  idCodeResource<idDeclRenderParm> variable(resourceName)
#define RENDER_PROG(variable, resourceName) \
  idCodeResource<idDeclRenderProg> variable(resourceName)

RENDER_PARM(rpViewRandom, "viewRandom");
RENDER_PARM(rpCGShadow, "cbShadow");
RENDER_PARM(rpCGMidtone, "cbMidTone");
RENDER_PARM(rpCGHighlight, "cbHighlight");
RENDER_PARM(rpCGColorDodgeBlend, "cbColorDodge");
RENDER_PARM(rpCGColorBurnBlend, "cbColorBurn");
RENDER_PARM(rpCGColorMultiplyBlend, "cbColorMultiply");
RENDER_PARM(rpCGColorScreenBlend, "cbColorScreen");
RENDER_PARM(rpCGBlackLevel, "cbBlackLevel");
RENDER_PARM(rpColorGradingLUT, "cbConversionLUT");
RENDER_PARM(rpEnvBlendMip, "envBlendMip");
RENDER_PARM(rpAmbientEnvMap, "ambientEnvMap");
RENDER_PARM(rpUserEnvMap, "userEnvMap");
RENDER_PARM(rpEnvMapRotation, "envMapRotation");
RENDER_PARM(rpColor, "Color");
RENDER_PARM(rpEnvSMatrix, "envSMatrix");
RENDER_PARM(rpEnvTMatrix, "envTMatrix");
RENDER_PARM(rpEnvRMatrix, "envRMatrix");
RENDER_PARM(rpUserColorCorrection, "userColorCorrection");
RENDER_PROG(progShowGuiOverdraw, "showGuiOverdraw");
RENDER_PARM(rpWindowPosToGlobalX, "windowPosToGlobalX");
RENDER_PARM(rpWindowPosToGlobalY, "windowPosToGlobalY");
RENDER_PARM(rpWindowPosToGlobalZ, "windowPosToGlobalZ");
RENDER_PARM(rpWindowPosToGlobalW, "windowPosToGlobalW");
RENDER_PARM(rpLightRotation, "lightRotation");
RENDER_PARM(rpLightTranslation, "lightTranslation");
RENDER_PARM(rpLightScale, "lightScale");
RENDER_PARM(rpLightOscillationAmp, "lightOscillationAmp");
RENDER_PARM(rpLightOscillationFreq, "lightOscillationFreq");
RENDER_PARM(rpLightOscillationPhase, "lightOscillationPhase");
RENDER_PARM(rpWindowPosToProjectedTexS, "windowPosToProjectedTexS");
RENDER_PARM(rpWindowPosToProjectedTexT, "windowPosToProjectedTexT");
RENDER_PARM(rpWindowPosToProjectedTexR, "windowPosToProjectedTexR");
RENDER_PARM(rpWindowPosToProjectedTexQ, "windowPosToProjectedTexQ");
RENDER_PARM(rpWindowPosToProjectedShadowS, "windowPosToProjectedShadowS");
RENDER_PARM(rpWindowPosToProjectedShadowT, "windowPosToProjectedShadowT");
RENDER_PARM(rpWindowPosToProjectedShadowR, "windowPosToProjectedShadowR");
RENDER_PARM(rpWindowPosToProjectedShadowQ, "windowPosToProjectedShadowQ");
RENDER_PARM(rpShadowProjectionS, "shadowProjectionS");
RENDER_PARM(rpShadowProjectionT, "shadowProjectionT");
RENDER_PARM(rpShadowProjectionR, "shadowProjectionR");
RENDER_PARM(rpShadowProjectionQ, "shadowProjectionQ");
RENDER_PARM(rpParallelLightSliceScale, "parallelLightSliceScale");
RENDER_PARM(rpGlobalLightOrigin, "globalLightOrigin");
RENDER_PARM(rpStaticSpecularScale, "staticSpecularScale");
RENDER_PARM(rpStaticSpecularVector, "staticSpecularVector");
RENDER_PARM(rpLightDiffuseScale, "lightDiffuseScale");
RENDER_PARM(rpLightSpecularScale, "lightSpecularScale");
RENDER_PARM(rpLightEnvironmentScale, "lightEnvironmentScale");
RENDER_PARM(rpLightEyeDiffuseScale, "lightEyeDiffuseScale");
RENDER_PARM(rpLightEyeSpecularScale, "lightEyeSpecularScale");
RENDER_PARM(rpLightEyeEnvironmentScale, "lightEyeEnvironmentScale");
RENDER_PARM(rpLightProjectionS, "LightProjectionS");
RENDER_PARM(rpLightProjectionT, "lightProjectionT");
RENDER_PARM(rpLightProjectionR, "lightProjectionR");
RENDER_PARM(rpLightProjectionQ, "lightProjectionQ");
RENDER_PARM(rpLocalToLightS, "localToLightS");
RENDER_PARM(rpLocalToLightT, "localToLightT");
RENDER_PARM(rpLocalToLightR, "localToLightR");
RENDER_PARM(rpLocalToLightQ, "localToLightQ");
RENDER_PARM(rpLightFade, "lightFade");
RENDER_PARM(rpLightDepthCompareMap, "lightDepthCompareMap");
RENDER_PARM(rpDimShadowDepthMap, "dimShadowDepthMap");
RENDER_PARM(rpDimShadowFade, "dimShadowFade");
RENDER_PARM(rpDimShadowExtrude, "dimShadowExtrude");
RENDER_PARM(rpDimShadowClipPlane, "dimShadowClipPlane");
RENDER_PARM(rpWindowPosToDimShadowBlurS, "windowPosToDimShadowBlurS");
RENDER_PARM(rpWindowPosToDimShadowBlurT, "windowPosToDimShadowBlurT");
RENDER_PARM(rpWindowPosToDimShadowBlurR, "windowPosToDimShadowBlurR");
RENDER_PARM(rpWindowPosToDimShadowBlurQ, "windowPosToDimShadowBlurQ");
RENDER_PARM(rpVertexMorphScale, "vertexMorphScale");
RENDER_PARM(rpDimShadowBufferResolution, "dimShadowBufferResolution");
RENDER_PARM(rpModelPosToShadowMapS0, "modelPosToShadowMapS0");
RENDER_PARM(rpModelPosToShadowMapT0, "modelPosToShadowMapT0");
RENDER_PARM(rpModelPosToShadowMapR0, "modelPosToShadowMapR0");
RENDER_PARM(rpModelPosToShadowMapQ0, "modelPosToShadowMapQ0");
RENDER_PARM(rpModelPosToShadowMapS1, "modelPosToShadowMapS1");
RENDER_PARM(rpModelPosToShadowMapT1, "modelPosToShadowMapT1");
RENDER_PARM(rpModelPosToShadowMapR1, "modelPosToShadowMapR1");
RENDER_PARM(rpModelPosToShadowMapQ1, "modelPosToShadowMapQ1");
RENDER_PARM(rpModelPosToShadowMapS2, "modelPosToShadowMapS2");
RENDER_PARM(rpModelPosToShadowMapT2, "modelPosToShadowMapT2");
RENDER_PARM(rpModelPosToShadowMapR2, "modelPosToShadowMapR2");
RENDER_PARM(rpModelPosToShadowMapQ2, "modelPosToShadowMapQ2");
RENDER_PARM(rpModelPosToShadowMapS3, "modelPosToShadowMapS3");
RENDER_PARM(rpModelPosToShadowMapT3, "modelPosToShadowMapT3");
RENDER_PARM(rpModelPosToShadowMapR3, "modelPosToShadowMapR3");
RENDER_PARM(rpModelPosToShadowMapQ3, "modelPosToShadowMapQ3");
RENDER_PARM(rpInvShadowProjX0, "invShadowProjX0");
RENDER_PARM(rpInvShadowProjY0, "invShadowProjY0");
RENDER_PARM(rpInvShadowProjZ0, "invShadowProjZ0");
RENDER_PARM(rpInvShadowProjW0, "invShadowProjW0");
RENDER_PARM(rpInvShadowProjX1, "invShadowProjX1");
RENDER_PARM(rpInvShadowProjY1, "invShadowProjY1");
RENDER_PARM(rpInvShadowProjZ1, "invShadowProjZ1");
RENDER_PARM(rpInvShadowProjW1, "invShadowProjW1");
RENDER_PARM(rpInvShadowProjX2, "invShadowProjX2");
RENDER_PARM(rpInvShadowProjY2, "invShadowProjY2");
RENDER_PARM(rpInvShadowProjZ2, "invShadowProjZ2");
RENDER_PARM(rpInvShadowProjW2, "invShadowProjW2");
RENDER_PARM(rpInvShadowProjX3, "invShadowProjX3");
RENDER_PARM(rpInvShadowProjY3, "invShadowProjY3");
RENDER_PARM(rpInvShadowProjZ3, "invShadowProjZ3");
RENDER_PARM(rpInvShadowProjW3, "invShadowProjW3");
RENDER_PARM(rpShadowProjectionDepth, "shadowProjectionDepth");
RENDER_PARM(rpSelfShadowMap, "selfShadowMap");
RENDER_PARM(rpSelfShadowFade, "selfShadowFade");
RENDER_PARM(rpSelfShadowEnable, "selfShadowEnable");
RENDER_PARM(rpSSSDirection, "sssDirection");
RENDER_PARM(rpGodRaysSourceTexture, "godRaysSourceTexture");
RENDER_PARM(rpGodRayViewPos, "godRayViewPos");
RENDER_PARM(rpTransMap, "transMap");
RENDER_PARM(rpSkinSSSRenderEnabled, "skinSSSRenderEnabled");
RENDER_PROG(progDimShadowCreate, "dimShadowCreate");
RENDER_PROG(progDimShadowCreateAlpha, "dimShadowCreateAlpha");
RENDER_PROG(progDimShadowCreateWound, "dimShadowCreateWound");
RENDER_PROG(progDimShadowCreateSecondary, "dimShadowCreateSecondary");
RENDER_PROG(progDimShadowDraw, "dimShadowDraw");
RENDER_PROG(progDimShadowDrawHighQuality, "dimShadowDrawHighQuality");
RENDER_PROG(progDimShadowStencil, "dimShadowStencil");
RENDER_PROG(progDimShadowDebugDraw, "dimShadowDebugDraw");
RENDER_PROG(progSelfShadowSample, "selfShadowSample");
RENDER_PROG(progSelfShadowSampleAlpha, "selfShadowSampleAlpha");
RENDER_PROG(progSelfShadowSampleSecondary, "selfShadowSampleSecondary");
RENDER_PROG(progSelfShadowSampleWound, "selfShadowSampleWound");
RENDER_PARM(rpViewColorMap, "viewColorMap");
RENDER_PARM(rpViewTarget, "viewTarget");
RENDER_PARM(rpViewTargetMS, "viewTargetMS");
RENDER_PARM(rpMipMapSampler, "mipMapSampler");
RENDER_PROG(progViewColorIdentity, "viewColorIdentity");
RENDER_PROG(progCaptureDepth, "captureDepth");
RENDER_PARM(rpTargetDepth, "targetDepth");
RENDER_PROG(progDurangoCopyToMip01, "durangoCopyToMip01");
RENDER_PROG(progDurangoCopyToMip012, "durangoCopyToMip012");
RENDER_PROG(progDurangoCopyToMip123, "durangoCopyToMip123");
RENDER_PARM(rpMipGenSrcSizeLod, "mipGenSrcSizeLod");
RENDER_PARM(rpMipGenSrc, "mipGenSrc");
RENDER_PARM(rpMipGenDst0, "mipGenDst0");
RENDER_PARM(rpMipGenDst1, "mipGenDst1");
RENDER_PARM(rpMipGenDst2, "mipGenDst2");
RENDER_PROG(progFeedbackResolve, "feedbackResolve");
RENDER_PARM(rpFeedbackColor, "feedbackColorMap");
RENDER_PROG(progDepthResolve, "depthResolve");
RENDER_PARM(rpTargetDepthMS, "targetDepthMS");
RENDER_PARM(rpGrainScaleBias, "postGrainScaleBias");
RENDER_PARM(rpGrainParms, "postGrainParms");
RENDER_PARM(rpGrainAmount, "grainAmount");
RENDER_PARM(rpGrainHz, "grainHz");
RENDER_PARM(rpGrainLevelsMin, "grainLevelsMin");
RENDER_PARM(rpGrainLevelsMax, "grainLevelsMax");
RENDER_PARM(rpBloomMinThreshold, "bloomMinThreshold");
RENDER_PARM(rpBloomMaxThreshold, "bloomMaxThreshold");
RENDER_PARM(rpBloomBrightness, "bloomBrightness");
RENDER_PARM(rpBloomDecay, "bloomDecay");
RENDER_PARM(rpBloomColor, "bloomColor");
RENDER_PARM(rpFrameExposureAmount, "frameExposureAmount");
RENDER_PARM(rpShowDepthOfFieldDebug, "showDepthOfField");
RENDER_PARM(rpShowGrainLevelsDebug, "showGrainLevels");
RENDER_PARM(rpShadowFade, "shadowFade");
RENDER_PARM(rpShadowBufferResolution, "shadowBufferResolution");
RENDER_PARM(rpParticleStScaleBias, "particleStScaleBias");
RENDER_PARM(rpViewColorLastFrame, "viewColorLastFrameMap");
RENDER_PARM(rpDistortionLastFrame, "distortionLastFrameMap");
RENDER_PARM(rpDofOffset, "dofOffset");
RENDER_PARM(rpDofMipScale, "dofMipScale");
RENDER_PARM(rpDepthFilterParams, "depthFilterParams");
RENDER_PARM(rpDofNearEnd, "dofNearEnd");
RENDER_PARM(rpDofFocusBegin, "dofFocusBegin");
RENDER_PARM(rpDofFocusEnd, "dofFocusEnd");
RENDER_PARM(rpDofFarBegin, "dofFarBegin");
RENDER_PARM(rpDofRecipValues, "dofRecipValues");
RENDER_PARM(rpHazeFlareMap, "hazeFlareMap");
RENDER_PARM(rpSSRMap, "ssrMap");
RENDER_PARM(rpSSRParm0, "ssrParm0");
RENDER_PARM(rpSSRParm1, "ssrParm1");
RENDER_PARM(rpPrimeLightMin, "primeLightMin");
RENDER_PARM(rpShadowPrimeIncidence, "shadowPrimeIncidence");
RENDER_PARM(rpShadowIncidenceInfluence, "shadowIncidenceInfluence");
RENDER_PARM(rpNumMultisamples, "numMultisamples");
RENDER_PARM(rpTransSortAtlasDim, "transSortAtlasDim");

#undef RENDER_PROG
#undef RENDER_PARM

}  // namespace

void RenderEventRecorderLightStartEvent_Impl(const char* name);
void RenderEventRecorderLightStopEvent_Impl();

namespace {

class ScopedRenderRecorderEvent {
public:
  explicit ScopedRenderRecorderEvent(const char* name) {
    RenderEventRecorderLightStartEvent_Impl(name);
  }
  ~ScopedRenderRecorderEvent() { RenderEventRecorderLightStopEvent_Impl(); }
};

const idDeclRenderParm* Parm(const idCodeResource<idDeclRenderParm>& resource) {
  return resource.Get();
}

void SetMatrixParms(const idRenderMatrix& matrix,
                    const idDeclRenderParm* rowX,
                    const idDeclRenderParm* rowY,
                    const idDeclRenderParm* rowZ,
                    const idDeclRenderParm* rowW) {
  renderThreadParmState->SetParmVector(rowX, &matrix.m[0]);
  renderThreadParmState->SetParmVector(rowY, &matrix.m[4]);
  renderThreadParmState->SetParmVector(rowZ, &matrix.m[8]);
  renderThreadParmState->SetParmVector(rowW, &matrix.m[12]);
}

std::uint32_t NextViewRandom(std::uint32_t& seed) {
  seed = seed * 1103515245u + 12345u;
  return (seed >> 16) & 0x7FFFu;
}

std::uint32_t GrainRandom(const std::uint32_t* state, unsigned index) {
  std::uint32_t value = state[index % 624u];
  value ^= value >> 11;
  value ^= (value & 0xFF3A58ADu) << 7;
  value ^= (value & 0xFFFFDF8Cu) << 15;
  value ^= value >> 18;
  return value;
}

}  // namespace

void idRender::UpdateShadowBufferOptions() {
  const float logBase = std::log(2.0f);
  const float logarithmicScale = logBase * 1.442695f;
  const float sliceScale =
      2.0f /
      (r_sb_sliceStep.valueFloat *
       (1.0f / std::sqrt(std::max(2.0f, std::numeric_limits<float>::denorm_min()))));
  renderThreadParmState->SetParmVector(
      Parm(rpParallelLightSliceScale), sliceScale, logarithmicScale,
      1.0f / logarithmicScale, r_sb_sliceRampScale.valueFloat);
}

__declspec(noinline) void idRender::WaitAndSwap(void* hdc, bool force30Hz) {
  console->BeginTimeRange(RANGE_RENDER, &idColor::colorRed);
  {
    ScopedRenderRecorderEvent event("GL_WaitForEndFrame");
    tr.pc.startWaitForEndFrameMicroSec = Sys_Microseconds();
    GL_WaitForEndFrame();
    tr.pc.endWaitForEndFrameMicroSec = Sys_Microseconds();
  }

  console->BeginTimeRange(RANGE_RENDER, &idColor::colorYellow);
  {
    ScopedRenderRecorderEvent event("tr.SwapBuffers - vsync");
    if (tr.renderingIsReadyForSwapbuffers) {
      tr.SwapBuffers(hdc, force30Hz);
      tr.renderingIsReadyForSwapbuffers = false;
    }
  }

  constexpr int modifiedFlag = 0x20000;
  if ((r_multiSamples.flags & modifiedFlag) != 0 ||
      (r_useFloatBuffers.flags & modifiedFlag) != 0) {
    tr.WindowChangeMultisamples(r_multiSamples.valueInteger,
                                r_useFloatBuffers.valueInteger != 0);
    if ((r_useFloatBuffers.flags & modifiedFlag) != 0) {
      tr.InitRenderTextures();
      if (renderViewObject != nullptr) {
        renderViewObject->envBlend.cubeFinished = false;
        renderViewObject->envBlend.colorCorrectionFinished = false;
      }
    }
    r_multiSamples.flags &= ~modifiedFlag;
    r_useFloatBuffers.flags &= ~modifiedFlag;
  }
}

void idRender::SetRenderSize(idRenderView* renderView,
                             const idScreenRect& screenRect) {
  float fractionX = 1.0f;
  float fractionY = 1.0f;
  if (!tr.automaticBackgroundSwapsEnabled) {
    resolutionScale.GetCurrentResolutionScale(fractionX, fractionY);
  }

  renderView->renderWidth = static_cast<int>(
      static_cast<float>(screenRect.x2 - screenRect.x1 + 1) * fractionX);
  renderView->renderHeight = static_cast<int>(
      static_cast<float>(screenRect.y2 - screenRect.y1 + 1) * fractionY);
  renderView->viewport.x1 = 0;
  renderView->viewport.y1 = 0;
  renderView->viewport.x2 = renderView->renderWidth - 1;
  renderView->viewport.y2 = renderView->renderHeight - 1;
  renderView->scissorRect = renderViewObject->viewport;
}

int idRender::GetLastUsedRenderWidth() {
  return renderViewObject != nullptr ? renderViewObject->renderWidth : 0;
}

int idRender::GetLastUsedRenderHeight() {
  return renderViewObject != nullptr ? renderViewObject->renderHeight : 0;
}

void idRender::InitSettings(const idRenderWorldCommitted* renderWorld,
                            const idRenderView* renderView, int numViews) {
  renderViewObject = nullptr;
  settings.featureFlags &= static_cast<std::uint8_t>(~0x03u);
  if (renderWorld != nullptr && renderWorld->isComboMap) {
    settings.featureFlags |= 0x01u;
  }
  if (renderWorld != nullptr && renderWorld->isToolsWorld) {
    settings.featureFlags |= 0x02u;
  }

  settings.doPreZMask = r_doPreZMask.valueInteger;
  settings.skipWorld = r_skipWorld.valueInteger != 0;
  settings.skipModels = r_skipModels.valueInteger != 0;
  settings.skipLights = r_skipLights.valueInteger != 0;
  settings.skipBlendLightShadows = r_skipBlendLightShadows.valueInteger != 0;
  settings.skipSuppress = r_skipSuppress.valueInteger != 0;
  settings.skipFeedback = r_skipFeedback.valueInteger != 0;
  settings.skipDynamic = r_skipDynamic.valueInteger != 0;
  settings.skipAddAlways = r_skipAddAlways.valueInteger != 0;
  settings.skipOpaqueSurfaces = r_skipOpaqueSurfaces.valueInteger != 0;
  settings.skipSubSurfaceScattering =
      r_skipSubSurfaceScattering.valueInteger != 0;
  settings.skipAutosprites = r_skipAutosprites.valueInteger != 0;
  settings.skipOcclusionBaseModel = r_skipOcclusionBaseModel.valueInteger != 0;
  settings.skipBlendedSurfaces = r_skipBlendedSurfaces.valueInteger != 0;
  settings.skipDistortionSurfaces = r_skipDistortionSurfaces.valueInteger != 0;
  settings.skipEmissiveSurfaces = r_skipEmissiveSurfaces.valueInteger != 0;
  settings.skipOcclusionSurfaces = r_skipOcclusionSurfaces.valueInteger != 0;
  settings.skipGuis = r_skipGuis.valueInteger == 2;
  settings.skipInGameGuis = r_skipGuis.valueInteger == 1;
  settings.skipPostProcess =
      r_skipPostProcess.valueInteger != 0 ||
      (renderView != nullptr && renderView->r.skipPostProcess);
  settings.skipEmissiveGlare = r_skipEmissiveGlare.valueInteger != 0;
  settings.skipBlendLights = r_skipBlendLights.valueInteger != 0;
  settings.skipSlowLights = r_skipSlowLights.valueInteger != 0;
  settings.skipSelfShadows = r_skipSelfShadows.valueInteger != 0;
  settings.skipAmbientOcclusion = r_skipAmbientOcclusion.valueInteger != 0;
  settings.skipFog = r_skipFog.valueInteger != 0;
  settings.skipGodRays =
      r_skipGodRays.valueInteger != 0 || renderView == nullptr;
  settings.skipAdaptiveGlare = r_skipAdaptiveGlare.valueInteger != 0;
  settings.skipHazeFlare = r_skipHazeFlare.valueInteger != 0;
  settings.skipPostSpecular = r_skipPostSpecular.valueInteger != 0 ||
                              r_skipSubSurfaceScattering.valueInteger != 0;
  settings.skipNodeCPUCulling = r_skipNodeCPUCulling.valueInteger != 0;
  settings.skipNodeGPUCulling = r_skipNodeGPUCulling.valueInteger != 0;
  settings.skipAreaCPUCulling = r_skipAreaCPUCulling.valueInteger != 0;
  settings.skipAreaGPUCulling = r_skipAreaGPUCulling.valueInteger != 0;
  settings.skipModelCPUCulling = r_skipModelCPUCulling.valueInteger != 0;
  settings.skipModelGPUCulling = r_skipModelGPUCulling.valueInteger != 0;
  settings.skipLightCPUCulling = r_skipLightCPUCulling.valueInteger != 0;
  settings.skipLightGPUCulling = r_skipLightGPUCulling.valueInteger != 0;
  settings.skipSurfaceCPUCulling = r_skipSurfaceCPUCulling.valueInteger != 0;
  settings.skipModelRangeCulling = r_skipModelRangeCulling.valueInteger != 0;
  settings.skipLightRangeCulling = r_skipLightRangeCulling.valueInteger != 0;
  settings.screenSpaceReflections = r_screenSpaceReflections.valueInteger;
  settings.lockView = r_lockView.valueInteger != 0;
  settings.forceTwoSidedDepth = r_forceTwoSided.valueInteger == 2;
  settings.useAmbientEnv = r_envBlendAmbient.valueInteger != 0;
  settings.feedbackBGRA = r_feedbackBGRA.valueInteger != 0;
  settings.showOcclusionBoxes = r_showOcclusionBoxes.valueInteger != 0;
  settings.forceOcclusionBoxQueries =
      r_forceOcclusionBoxQueries.valueInteger != 0;
  settings.numViews = std::max(numViews, 1);
  settings.generateMipMaps =
      renderView != nullptr && renderView->r.useHQViewColorMipsGeneration
          ? 0
          : r_generateMipMaps.valueInteger;
  settings.guiFrameCount = idRenderModelGui::frameCount;
  settings.singleWorldArea = r_singleWorldArea.valueInteger;
  settings.singleLight = r_singleLight.valueInteger;
  settings.singleModel = r_singleModel.valueInteger;
  settings.singleSurface = r_singleSurface.valueInteger;
  settings.singleGuiSurface = r_singleGuiSurface.valueInteger;
  settings.singleDimShadow = r_singleDimShadow.valueInteger;
  settings.singleSelfShadow = r_singleSelfShadow.valueInteger;
  settings.queryThreshold = r_queryThreshold.valueInteger;

  float xScale = 1.0f;
  float yScale = 1.0f;
  resolutionScale.GetCurrentResolutionScale(xScale, yScale);
  const float minimumScale = std::fmin(xScale, yScale);
  settings.dimShadowUseQuery = r_dimShadowUseQuery.valueInteger != 0;
  settings.dimShadowDepthBoundsTest =
      r_dimShadowDepthBoundsTest.valueInteger != 0;
  settings.dimShadowResolution = r_sb_imageSize.valueInteger;
  std::uint32_t dimShadowResolutionCap = static_cast<std::uint32_t>(
      (static_cast<int>(r_dimShadowResolutionCap.valueInteger * minimumScale) +
       7) &
      ~7);
  const std::uint32_t shadowBufferResolution =
      static_cast<std::uint32_t>(r_sb_imageSize.valueInteger);
  if (shadowBufferResolution < dimShadowResolutionCap) {
    dimShadowResolutionCap = shadowBufferResolution;
  }
  settings.dimShadowResolutionCap =
      static_cast<int>(dimShadowResolutionCap);
  settings.dimShadowForceHighQuality =
      r_dimShadowForceHighQuality.valueInteger;
  settings.showDimShadows = r_showDimShadows.valueInteger;
  settings.dimShadowMaxLixelsPerUnit = r_dimShadowMaxLixelsPerUnit.valueFloat;
  settings.dimShadowLixelScale = r_dimShadowLixelScale.valueFloat;
  settings.dimShadowDensity = r_dimShadowDensity.valueFloat;
  settings.dimShadowPolyOfsUnits = r_dimShadowPolyOfsUnits.valueFloat;
  settings.dimShadowPolyOfsFactor = r_dimShadowPolyOfsFactor.valueFloat;
  settings.dimShadowMaxVisibleRange = r_dimShadowMaxVisibleRange.valueFloat;
  settings.dimShadowFadeVisibilityRange =
      r_dimShadowFadeVisibilityRange.valueFloat;
  settings.dimShadowSkipRangeCulling =
      r_skipDimShadowRangeCulling.valueInteger != 0;
  std::uint32_t selfShadowResolutionCap = static_cast<std::uint32_t>(
      (static_cast<int>(r_selfShadowResolutionCap.valueInteger * minimumScale) +
       7) &
      ~7);
  const std::uint32_t halfShadowBufferResolution =
      static_cast<std::uint32_t>(r_sb_imageSize.valueInteger / 2);
  if (halfShadowBufferResolution < selfShadowResolutionCap) {
    selfShadowResolutionCap = halfShadowBufferResolution;
  }
  settings.selfShadowResolutionCap =
      static_cast<int>(selfShadowResolutionCap);
  settings.selfShadowResolution = r_sb_imageSize.valueInteger;
  settings.selfShadowDensity = r_selfShadowDensity.valueFloat;
  settings.selfShadowPolyOfsUnits = r_selfShadowPolyOfsUnits.valueFloat;
  settings.selfShadowPolyOfsFactor = r_selfShadowPolyOfsFactor.valueFloat;
  settings.selfShadowMaxVisibleRange = r_selfShadowMaxVisibleRange.valueFloat;
  settings.selfShadowFadeVisibilityRange =
      r_selfShadowFadeVisibilityRange.valueFloat;
  settings.selfShadowLixelScale = r_selfShadowLixelScale.valueFloat;
  settings.selfShadowSkipRangeCulling =
      r_skipSelfShadowRangeCulling.valueInteger != 0;
  settings.subSurfaceScatteringMaxVisibilityRange =
      r_subSurfaceScatteringMaxVisibleRange.valueFloat;
  settings.subSurfaceScatteringFadeVisibilityRange =
      r_subSurfaceScatteringFadeVisibilityRange.valueFloat;
  settings.subSurfaceScatteringSkipRangeCulling =
      r_subSurfaceScatteringSkipRangeCulling.valueInteger != 0;
  settings.sortBlendedSurfacesBackToFront =
      r_sortBlendedSurfacesBackToFront.valueInteger != 0;
  settings.useLightScissors = r_useLightScissors.valueInteger != 0;
  settings.useLightDepthBoundsTest = r_useLightDepthBoundsTest.valueInteger != 0;
  settings.useDeferredSlowMapLighting =
      r_useDeferredSlowMapLighting.valueInteger != 0;
  settings.showLightScissors = r_showLightScissors.valueInteger != 0;
  settings.showBlendedLights = r_showBlendedLights.valueInteger;
  settings.postProcessDofMode = r_postProcessDofMode.valueInteger;
  if (settings.postProcessDofMode == 0 && r_postProcessHQ.valueInteger != 0) {
    settings.postProcessDofMode = 1;
  }

  settings.sortCoverage = renderThreadParmState->GetParmFloat(Parm(rp_sortCoverage));
  settings.sortSSS = renderThreadParmState->GetParmFloat(Parm(rp_sortSSS));
  settings.sortSkybox = renderThreadParmState->GetParmFloat(Parm(rp_sortSkybox));
  settings.sortBackgroundFar =
      renderThreadParmState->GetParmFloat(Parm(rp_sortBackgroundFar));
  settings.sortBackground =
      renderThreadParmState->GetParmFloat(Parm(rp_sortBackground));
  settings.sortBackgroundEmit =
      renderThreadParmState->GetParmFloat(Parm(rp_sortBackgroundEmit));
  settings.sortEmit = renderThreadParmState->GetParmFloat(Parm(rp_sortEmit));
  settings.sortEmitOnly =
      renderThreadParmState->GetParmFloat(Parm(rp_sortEmitOnly));
  settings.sortLight = renderThreadParmState->GetParmFloat(Parm(rp_sortLight));
  settings.sortDecal = renderThreadParmState->GetParmFloat(Parm(rp_sortDecal));
  settings.sortTransSort =
      renderThreadParmState->GetParmFloat(Parm(rp_sortTransSort));
  settings.sortTrans = renderThreadParmState->GetParmFloat(Parm(rp_sortTrans));
  settings.sortWater = renderThreadParmState->GetParmFloat(Parm(rp_sortWater));
  settings.sortPerturber =
      renderThreadParmState->GetParmFloat(Parm(rp_sortPerturber));
  settings.sortLast = 1.0e30f;
  settings.viewNearZ = r_znear.valueFloat;
  settings.viewFarZ = r_zfar.valueFloat;
  settings.progShowGuiOverdraw =
      r_showGuiOverdraw.valueInteger != 0 ? progShowGuiOverdraw.Get() : nullptr;

  if (r_purpleClear.valueInteger != 0) {
    settings.clearColor = {0.4f, 0.0f, 0.25f};
  } else if (r_clear.valueInteger == 2) {
    settings.clearColor = {0.0f, 0.0f, 0.0f};
  } else if (std::sscanf(r_clear.GetString(), "%f %f %f",
                         &settings.clearColor.x, &settings.clearColor.y,
                         &settings.clearColor.z) != 3) {
    settings.clearColor = {0.4f, 0.0f, 0.25f};
  }

  if (renderView != nullptr) {
    const idVec3& origin = renderView->r.vieworg;
    settings.viewOrigin = {origin.x, origin.y, origin.z, 0.0f};
    settings.viewBounds.b[0] = {origin.x - 16.0f, origin.y - 16.0f,
                                origin.z - 16.0f};
    settings.viewBounds.b[1] = {origin.x + 16.0f, origin.y + 16.0f,
                                origin.z + 16.0f};
    const float detailExtent =
        (static_cast<float>(renderView->windowWidth) * 0.5f) /
        (std::tan(renderView->r.fov_x * 0.5f * idMath::M_DEG2RAD) *
         r_znear.valueFloat *
         (r_detailDistancePixelThreshold.valueFloat - 1.0f)) *
        (r_manifoldMaxDetailWidth.valueFloat * r_znear.valueFloat);
    settings.detailBounds.b[0] = {origin.x - detailExtent,
                                  origin.y - detailExtent,
                                  origin.z - detailExtent};
    settings.detailBounds.b[1] = {origin.x + detailExtent,
                                  origin.y + detailExtent,
                                  origin.z + detailExtent};
  }
}

void idRender::PrintStats() const {
  if (r_showModels.valueInteger >= 2) {
    idLib::Printf("models: ");
    int total = 0;
    int worldModels = 0;
    int staticModels = 0;
    int dynamicModels = 0;
    for (int i = 0; i < numViewModels; ++i) {
      const idRenderModelCommitted* model = viewModels[i];
      if (model == &worldSpace) {
        ++worldModels;
      } else if ((model->r.flags0 & 4u) != 0) {
        ++staticModels;
      } else {
        ++dynamicModels;
      }
      idLib::Printf("%u ", model->index.index >> 8);
      ++total;
    }
    idLib::Printf("= %i total, %i world, %i static, %i dynamic\n", total,
                  worldModels, staticModels, dynamicModels);
  }

  if (r_showLights.valueInteger >= 2) {
    idLib::Printf("lights: ");
    int total = 0;
    for (int i = 0; i < numViewLights; ++i) {
      const idRenderLightCommitted* light = viewLights[i];
      if (light->r.lightClass == LIGHT_BLENDED_ONLY) {
        idLib::Printf("B");
      }
      if (light->r.lightClass == LIGHT_SLOWLIGHT_DYNAMIC_ONLY ||
          light->r.lightClass == LIGHT_SLOWLIGHT_PRIVATE_ONLY) {
        idLib::Printf(light->castsShadows ? "$" : "*");
      }
      idLib::Printf("%u ", light->index.index >> 8);
      ++total;
    }
    idLib::Printf("= %i total\n", total);
  }

  if (r_showDimShadows.valueInteger >= 2) {
    idLib::Printf("dimShadows: ");
    int total = 0;
    for (int i = 0; i < world->committedRenderModels.num; ++i) {
      const idRenderModelCommitted* model = world->committedRenderModels.list[i];
      if (model == nullptr) {
        continue;
      }
      const unsigned modelIndex = model->index.index >> 8;
      const std::uint8_t state = modelDimShadowState[modelIndex];
      if (state == 0) {
        continue;
      }
      if (state == 2) {
        idLib::Printf("%u(%s) ", modelIndex, model->name);
      } else {
        idLib::Printf("%u ", modelIndex);
      }
      ++total;
    }
    idLib::Printf("= %i total\n", total);
  }
}

void idRender::SetupCaptureParms(captureParms_t& parms,
                                 renderCapture_t capture,
                                 const idScreenRect& screenRect,
                                 int viewIndex, bool createMipMaps) {
  parms.settings = &settings;
  parms.capture = capture;
  if (renderViewObject != nullptr) {
    parms.renderWidth = renderViewObject->renderWidth;
    parms.renderHeight = renderViewObject->renderHeight;
    parms.windowWidth = renderViewObject->windowWidth;
    parms.windowHeight = renderViewObject->windowHeight;
    parms.forceFullVirtualTextureLoad =
        renderViewObject->r.forceFullVirtualTextureLoad;
  } else {
    parms.renderWidth = parms.windowWidth = screenRect.x2 - screenRect.x1 + 1;
    parms.renderHeight = parms.windowHeight = screenRect.y2 - screenRect.y1 + 1;
    parms.forceFullVirtualTextureLoad = false;
  }

  parms.viewIndex = viewIndex;
  parms.renderDestDefault = renderDestScaledColor;
  parms.imgViewDepth = tr.imgViewDepth;
  parms.renderDestViewColor = tr.renderDestViewColor[viewIndex];
  parms.renderDestViewDepth = tr.renderDestViewDepth;
  parms.renderDestViewNormal = tr.renderDestViewPostScratch;
  parms.renderDestFeedback =
      virtualTextureSystem.renderDestFeedback[renderSystem->FrameNumber() % 3];
  parms.feedbackBuffer = &virtualTextureSystem.feedbackBuffer;
  parms.feedbackBufferObject =
      r_usePBOTextureFeedback.valueInteger != 0
          ? virtualTextureSystem.feedbackBufferObject
          : nullptr;
  parms.rpFeedbackColor =
      renderDestScaledColor->targetImage[0]->opts.numSamples > 1
          ? Parm(rpFeedbackColor)
          : nullptr;
  parms.renderDestGui = tr.renderDestGui;
  parms.createMipMaps = createMipMaps;
  parms.renderDestMip = tr.renderDestMip1;
  parms.renderDestMip3 = tr.renderDestMip3;
  parms.rpGlareMap = tr.rpGlareMap;
  parms.progGlareScale = tr.progGlareScale;
  parms.progOverlappedDownSample = tr.progOverlappedDownSample;
  parms.progFeedbackDownSample =
      renderDestScaledColor->targetImage[0]->opts.numSamples > 1
          ? progFeedbackResolve.Get()
          : tr.progFeedbackDownSample;
  parms.progCaptureDepth =
      renderDestScaledColor->depthImage->opts.numSamples > 1
          ? progDepthResolve.Get()
          : progCaptureDepth.Get();
  parms.progCopyToMip01 = progDurangoCopyToMip01.Get();
  parms.progCopyToMip012 = progDurangoCopyToMip012.Get();
  parms.rpViewColorMap = Parm(rpViewColorMap);
  parms.rpMipMapSampler = Parm(rpMipMapSampler);
  for (int i = 0; i < 16; ++i) {
    parms.mipMapSamplers[i] = textureSamplerManager->mipMapSamplers[i];
  }
  parms.rpTargetDepth =
      renderDestScaledColor->depthImage->opts.numSamples > 1
          ? Parm(rpTargetDepthMS)
          : Parm(rpTargetDepth);
  parms.rpMipGenSrc = Parm(rpMipGenSrc);
  parms.rpMipGenDst0 = Parm(rpMipGenDst0);
  parms.rpMipGenDst1 = Parm(rpMipGenDst1);
  parms.rpMipGenDst2 = Parm(rpMipGenDst2);
  parms.unitSquareTris = tr.unitSquareModel->surfaces.list->geometry;
  parms.imgBlack = globalImages->blackImage;
}

void idRender::SetupRenderPassParms(renderPassParms_t& parms,
                                    renderPass_t pass,
                                    const idScreenRect& screenRect,
                                    bool allowInGameGUIs) {
  parms.pass = pass;
  parms.settings = &settings;
  parms.renderView = renderViewObject;
  parms.drawSurfParms.settings = &settings;
  parms.drawSurfParms.renderView = renderViewObject;
  parms.drawSurfParms.allowInGameGUIs = allowInGameGUIs;
  SetupCaptureParms(parms.drawSurfParms.captureParms, CAPTURE_GUI_IMAGE,
                    screenRect, renderViewObject->viewIndex, false);
  parms.drawSurfParms.renderDestDefault = renderDestScaledColor;
  parms.drawSurfParms.renderDestGui = tr.renderDestGui;
  parms.renderDestDefault = renderDestScaledColor;
  parms.renderDestTemp = renderDestScaledFeedback;
  parms.renderDestDistortion =
      tr.renderDestDistortion[renderViewObject->viewIndex];
  parms.rpViewTarget = Parm(rpViewTarget);
  parms.rpViewTargetMS = Parm(rpViewTargetMS);
  parms.rpSSSDirection = Parm(rpSSSDirection);
  parms.renderPasses = renderPasses;
  parms.sortedDrawSurfs = sortedDrawSurfs;
  parms.depthSortedDrawSurfs = depthSortedDrawSurfs;
  parms.numDepthSortedDrawSurfs = &numDepthSortedDrawSurfs;
  // Render.obj leaves binaryModelState untouched here. RenderSingleView fills
  // it after the common parameter block has been prepared.
}

void idRender::SetupAdditiveEnvironmentParms(
    envBlend_t& blend, const idEnvBlendParms* additive,
    const idParmBlock* blended, idParmBlock* final) {
  if (final == nullptr || blended == nullptr) {
    return;
  }
  final->CopyFrom(*blended);
  if (additive == nullptr || r_skipAdditiveEnvParms.valueInteger != 0) {
    return;
  }

  for (int i = 0; i < additive->parms.num; ++i) {
    const idDeclRenderParm* parm = additive->parms.list[i];
    if (parm->parmType != PT_VECTOR) {
      continue;
    }
    const idVec4 from = final->GetVector(parm, nullptr);
    const parmValue_t& target = additive->values.list[i];
    const float fraction = additive->blends.list[i];
    parmValue_t value{};
    for (int component = 0; component < 4; ++component) {
      value.value[component] =
          from[component] + (target.value[component] - from[component]) * fraction;
    }
    final->SetParm(parm, value);
  }

  if (final->GetImage(Parm(rpUserEnvMap), nullptr) !=
      blended->GetImage(Parm(rpUserEnvMap), nullptr)) {
    blend.cubeFinished = false;
  }
  if (final->GetImage(Parm(rpUserColorCorrection), nullptr) !=
      blended->GetImage(Parm(rpUserColorCorrection), nullptr)) {
    blend.colorCorrectionFinished = false;
  }
  if (final->GetFloat(Parm(rpFrameExposureAmount), nullptr) > 0.0f) {
    blend.frameExposureFinished = 0;
  }
}

void idRender::SetupDynamicEnvironment(
    const idVec3& viewOrigin, envBlend_t& blend, idParmBlock* blended,
    idParmBlock* previous, const idParmBlock* overrideParms) {
  renderThreadParmState->SetParmSampler(
      tr.rpDynamicEnvMap,
      reinterpret_cast<const idTextureSampler*>(
          tr.imgDynamicEnvironment[renderViewObject->viewIndex]));

  const renderView_t& renderView = renderViewObject->r;
  if (renderView.skipDynamicEnvironment ||
      r_useDynamicEnvironment.valueInteger == 0) {
    blended->Clear();
    return;
  }

  const idDeclEnv* environment = nullptr;
  for (int i = 0; i < world->dynamicEnvironments.num; ++i) {
    const dynamicEnvironment_t& candidate = world->dynamicEnvironments.list[i];
    const bool inBounds =
        viewOrigin.x >= candidate.bounds.b[0].x &&
        viewOrigin.y >= candidate.bounds.b[0].y &&
        viewOrigin.z >= candidate.bounds.b[0].z &&
        viewOrigin.x <= candidate.bounds.b[1].x &&
        viewOrigin.y <= candidate.bounds.b[1].y &&
        viewOrigin.z <= candidate.bounds.b[1].z;
    if (inBounds &&
        idPolytope::ConvexPolytopeContainsPoint(
            candidate.planes.ptr, candidate.numPlanes, viewOrigin, 0.0f)) {
      environment = candidate.env;
      break;
    }
  }
  if (world->testEnv != nullptr) {
    environment = world->testEnv;
  }
  if (environment == nullptr) {
    environment = static_cast<const idDeclEnv*>(
        idDeclEnv::resourceList.FindWithInheritance("default", false, true));
    if (environment == nullptr) {
      blended->Clear();
      return;
    }
  }

  float blendDuration = environment->blendDuration;
  if (r_envBlendTime.valueFloat >= 0.0f) {
    blendDuration = std::max(0.001f, r_envBlendTime.valueFloat);
  }
  blend.skipBlending = r_envBlendSkip.valueInteger != 0;

  const bool gainedOverride =
      overrideParms->ops.num > 0 && !blend.hasOverrideParms;
  const bool lostOverride =
      overrideParms->ops.num == 0 && blend.hasOverrideParms;
  const bool overrideStateChanged = gainedOverride || lostOverride;
  if (renderView.applyDynEnvOverride || overrideStateChanged) {
    bool overrideVisible = renderView.dynEnvOverrideModelIndex == -1;
    if (!overrideVisible) {
      const int modelIndex = renderView.dynEnvOverrideModelIndex;
      const occlusionState_t* state = renderViewObject->occlusionState;
      overrideVisible =
          state->modelVisibleFrameNumber[modelIndex] >=
              state->renderFrameNumber &&
          state->modelVisibleFrameCount[modelIndex] > 1 &&
          state->modelOcclusionQueryResult[modelIndex] > 1;
    }
    if (overrideVisible) {
      if (renderView.dynEnvOverride != nullptr) {
        environment = renderView.dynEnvOverride;
      }
      if (renderView.dynEnvOverrideDuration <= 0.0f) {
        blend.skipBlending = true;
      } else {
        blendDuration =
            std::max(0.001f, renderView.dynEnvOverrideDuration);
      }
    }
    if (overrideStateChanged) {
      blend.hasOverrideParms = !blend.hasOverrideParms;
    }
  }

  float renderTime = static_cast<float>(renderView.renderTime) * 0.001f;
  if (blend.blendStartTime > renderTime) {
    renderTime = blend.blendStartTime + blend.blendDuration;
  }

  const bool environmentChanged = blend.env != environment;
  if (environmentChanged || overrideStateChanged) {
    blend.blendStartTime = renderTime;
    blend.blendDuration = blendDuration;
    blend.cubeFinished = false;
    blend.colorCorrectionFinished = false;
    const idParmBlock* source = blended;
    if (blend.env == nullptr) {
      blend.blendDuration = 1.0f;
      blend.blendStartTime = renderTime - 2.0f;
      source = &environment->parmBlock;
    }
    previous->CopyFrom(*source);
    blend.env = environment;
  }

  const idImage* oldEnvironmentImage =
      blended->GetImage(Parm(rpUserEnvMap), nullptr);
  const idImage* oldColorCorrection =
      blended->GetImage(Parm(rpUserColorCorrection), nullptr);

  const idDeclEnv* modelEnvironment = renderViewObject->m_pModelEnvOverride;
  const float blendEnd = blend.blendStartTime + blend.blendDuration;
  if (modelEnvironment == nullptr && !blend.skipBlending &&
      renderTime < blendEnd) {
    const float fraction =
        (renderTime - blend.blendStartTime) / blend.blendDuration;
    const idParmBlock& environmentParms = blend.env->parmBlock;
    for (int i = 0; i < environmentParms.ops.num; ++i) {
      const idDeclRenderParm* destination = environmentParms.ops.list[i].GetDest();
      const idParmBlock* source = overrideParms->SetsRenderParm(destination)
                                      ? overrideParms
                                      : &environmentParms;
      if (destination->parmType == PT_VECTOR) {
        const idVec4 to = source->GetVector(destination, nullptr);
        const idVec4 from = previous->GetVector(destination, nullptr);
        parmValue_t value{};
        for (int component = 0; component < 4; ++component) {
          value.value[component] =
              to[component] * fraction + from[component] * (1.0f - fraction);
        }
        blended->SetParm(destination, value);
        continue;
      }

      if (destination->parmType < PT_TEXTURE_2D ||
          destination->parmType > PT_IMAGE_2D_WRITE) {
        continue;
      }
      // The retail loop indexes the selected block with the environment-op
      // index. Override blocks are laid out in that same destination order.
      const expOp_t& operation = source->ops.list[i];
      const int operationType = operation.type & 0x1F;
      if (operationType == OP_TYPE_UNMASKED_MOVE) {
        blended->SetImage(destination,
                          source->GetImage(operation.GetA(), nullptr));
      } else if (operationType == OP_TYPE_UNMASKED_MOVE_CONSTANT) {
        blended->SetImage(destination, source->constants.list[i].image);
      }
    }
  } else {
    if (modelEnvironment == nullptr) {
      modelEnvironment = blend.env;
    }
    blended->CopyFrom(modelEnvironment->parmBlock);
    blended->Append(*overrideParms);
  }

  if (oldEnvironmentImage != blended->GetImage(Parm(rpUserEnvMap), nullptr)) {
    blend.cubeFinished = false;
  }
  if (oldColorCorrection !=
      blended->GetImage(Parm(rpUserColorCorrection), nullptr)) {
    blend.colorCorrectionFinished = false;
  }
  if (blended->GetFloat(Parm(rpFrameExposureAmount), nullptr) > 0.0f) {
    blend.frameExposureFinished = 0;
  }
  if (r_envBlendShow.valueInteger != 0) {
    idLib::Printf("blended environment parms for %s:\n", blend.env->name.str);
    blended->Print();
  }
}

idRender::idRender() : renderViewObject(nullptr) {}

void idRender::CaptureToViewColor(const idScreenRect& screenRect,
                                  int viewIndex) {
  captureParms_t parms;
  SetupCaptureParms(parms, CAPTURE_VIEW_COLOR, screenRect, viewIndex, false);
  RenderCapture(&parms);
}

void idRender::RenderGuiModels(idRenderModelCommitted* const* guiModels,
                               int numGuiModels,
                               const idRenderDestination*,
                               const idScreenRect& screenRect,
                               float guiOriginOffset) {
  if (numGuiModels <= 0) {
    return;
  }
  settings.guiOriginOffset = guiOriginOffset;
  const int width = screenRect.x2 - screenRect.x1 + 1;
  const int height = screenRect.y2 - screenRect.y1 + 1;
  renderThreadParmState->SetFragmentPositionToViewTexture(width, height, width,
                                                          height);
  GL_SetDefaultState();
  GL_Clear(false, false, true, 1u, 0.0f, 0.0f, 0.0f, 0.0f);
  for (int i = 0; i < numGuiModels; ++i) {
    const idRenderModelCommitted* model = guiModels[i];
    if (model != nullptr && model->surfaces.num != 0) {
      RenderGui(model, &settings, renderViewObject, false);
    }
  }
  renderThreadParmState->SetFragmentPositionToViewTexture(width, height, width,
                                                          height);
}

void idRender::RenderSingleView(
    void* hdc, const idRenderWorldCommitted* renderWorld,
    idRenderView* renderView, const idScreenRect& screenRect) {
  numNodeWorldAreas = 0;
  numViewWorldAreas = 0;
  numViewWorldSurfaces = 0;
  numViewModels = 1;
  numViewModelsPreZ = 0;
  numViewSceneModels = 0;
  numViewGodrayModels = 0;
  numViewLights = 0;
  numOcclusionTestNodes = 0;
  numOcclusionTestWorldAreas = 0;
  numOcclusionTestModels = 0;
  numOcclusionTestLights = 0;
  numDrawSurfs = 0;
  numDepthSortedDrawSurfs = 0;
  numShadows = 0;
  numDimShadows = 0;
  numSelfShadows = 0;
  viewModels[0] = &worldSpace;
  std::memset(modelDimShadowState, 0, sizeof(modelDimShadowState));

  world = renderWorld;
  worldSpace.name = "_worldSpace";
  worldSpace.world = nullptr;
  worldSpace.index.index = 0x200001u;
  worldSpace.r.flags0 |= 0x0Cu;
  worldSpace.modelMatrix = {{1.0f, 0.0f, 0.0f, 0.0f,
                             0.0f, 1.0f, 0.0f, 0.0f,
                             0.0f, 0.0f, 1.0f, 0.0f,
                             0.0f, 0.0f, 0.0f, 1.0f}};
  worldSpace.inverseModelMatrix = worldSpace.modelMatrix;
  worldSpace.mvpMatrix = renderView->worldSpaceMVPMatrix;
  worldSpace.surfaces.Clear();

  viewSpace.name = "_viewSpace";
  viewSpace.modelMatrix = renderView->viewSpaceModelMatrix;
  viewSpace.inverseModelMatrix = renderView->viewSpaceInverseModelMatrix;
  viewSpace.mvpMatrix = renderView->viewSpaceMVPMatrix;
  renderViewObject = renderView;
  if (r_forceFullVirtualTextureLoad.valueInteger != 0) {
    renderViewObject->r.forceFullVirtualTextureLoad = true;
  }

  float splitPlaneDistance = 131072.0f;
  if (!world->isComboMap) {
    worldSpace.committedFlags0 |= 4u;
    approximateLighting_t& lighting =
        worldSpace.approximateLighting[worldSpace.CurrentApproximateLighting()];
    if (world->ambientLighting.ambientMap->useForcedColors) {
      for (int channel = 0; channel < 6; ++channel) {
        lighting.channelLighting[channel] = {
            world->ambientLighting.ambientMap->forcedColors.channels[channel][0] *
                (1.0f / 255.0f),
            world->ambientLighting.ambientMap->forcedColors.channels[channel][1] *
                (1.0f / 255.0f),
            world->ambientLighting.ambientMap->forcedColors.channels[channel][2] *
                (1.0f / 255.0f),
            lighting.channelLighting[channel].w};
      }
    } else {
      for (int channel = 0; channel < 6; ++channel) {
        lighting.channelLighting[channel].x = r_slowMapWorldAmbient.valueFloat;
        lighting.channelLighting[channel].y = r_slowMapWorldAmbient.valueFloat;
        lighting.channelLighting[channel].z = r_slowMapWorldAmbient.valueFloat;
      }
    }
  }

  SetMatrixParms(renderViewObject->projectionMatrix,
                 Parm(rp_projectionMatrixX), Parm(rp_projectionMatrixY),
                 Parm(rp_projectionMatrixZ), Parm(rp_projectionMatrixW));
  SetMatrixParms(renderViewObject->inverseProjectionMatrix,
                 Parm(rp_inverseProjectionMatrixX),
                 Parm(rp_inverseProjectionMatrixY),
                 Parm(rp_inverseProjectionMatrixZ),
                 Parm(rp_inverseProjectionMatrixW));
  SetMatrixParms(renderViewObject->viewMatrix, Parm(rp_viewMatrixX),
                 Parm(rp_viewMatrixY), Parm(rp_viewMatrixZ),
                 Parm(rp_viewMatrixW));
  SetMatrixParms(renderViewObject->inverseViewMatrix,
                 Parm(rp_inverseViewMatrixX), Parm(rp_inverseViewMatrixY),
                 Parm(rp_inverseViewMatrixZ), Parm(rp_inverseViewMatrixW));
  SetMatrixParms(renderViewObject->worldSpaceMVPMatrix,
                 Parm(rp_viewProjectionMatrixX),
                 Parm(rp_viewProjectionMatrixY),
                 Parm(rp_viewProjectionMatrixZ),
                 Parm(rp_viewProjectionMatrixW));
  SetMatrixParms(renderViewObject->worldSpaceInverseMVPMatrix,
                 Parm(rp_inverseMVPMatrixX), Parm(rp_inverseMVPMatrixY),
                 Parm(rp_inverseMVPMatrixZ), Parm(rp_inverseMVPMatrixW));
  renderThreadParmState->SetParmVec3(Parm(rp_globalViewOrigin),
                                     renderViewObject->r.vieworg);
  renderThreadParmState->SetParmVec3(Parm(rp_globalViewFwd),
                                     renderViewObject->r.viewaxis[0]);
  renderThreadParmState->SetParmVec3(Parm(rp_globalViewLeft),
                                     renderViewObject->r.viewaxis[1]);
  renderThreadParmState->SetParmVec3(Parm(rp_globalViewUp),
                                     renderViewObject->r.viewaxis[2]);

  idRenderMatrix windowToView{};
  idRenderMatrix windowToGlobal{};
  idRenderMatrix::Multiply(renderViewObject->inverseProjectionMatrix,
                           renderMatrix_windowSpaceToClipSpace, windowToView);
  idRenderMatrix::Multiply(renderViewObject->inverseViewMatrix, windowToView,
                           windowToGlobal);
  SetMatrixParms(windowToGlobal, Parm(rpWindowPosToGlobalX),
                 Parm(rpWindowPosToGlobalY), Parm(rpWindowPosToGlobalZ),
                 Parm(rpWindowPosToGlobalW));

  renderThreadParmState->SetFragmentPositionToViewTexture(
      renderViewObject->renderWidth, renderViewObject->renderHeight,
      renderViewObject->windowWidth, renderViewObject->windowHeight);
  renderThreadParmState->SetFragmentPositionToWindow(
      renderViewObject->renderWidth, renderViewObject->renderHeight,
      renderViewObject->windowWidth, renderViewObject->windowHeight);

  static std::uint32_t viewRandomSeed = 0;
  const float randomW = NextViewRandom(viewRandomSeed) * (1.0f / 32767.0f);
  const float randomZ = NextViewRandom(viewRandomSeed) * (1.0f / 32767.0f);
  const float randomY = NextViewRandom(viewRandomSeed) * (1.0f / 32767.0f);
  const float randomX = NextViewRandom(viewRandomSeed) * (1.0f / 32767.0f);
  renderThreadParmState->SetParmVector(Parm(rpViewRandom), randomX, randomY,
                                       randomZ, randomW);
  renderThreadParmState->SetParmFloat(
      Parm(rp_Time), renderViewObject->r.renderTime * 0.001f);
  renderThreadParmState->SetParmFloat(tr.rpFrameNumber,
                                      static_cast<float>(tr.frameCount));
  renderThreadParmState->SetParmVector(Parm(rpParticleStScaleBias),
                                       0.000015259255f, 0.5f, 0.0f, 0.0f);
  renderThreadParmState->SetParmSampler(
      Parm(rpSelfShadowMap),
      reinterpret_cast<const idTextureSampler*>(globalImages->whiteImage));
  renderThreadParmState->SetParmFloat(
      Parm(rpNumMultisamples),
      static_cast<float>(renderSystem->GetNumMultisamples()));
  const float atlasWidth =
      static_cast<float>(globalImages->transSortImageAtlas->ATLAS_WIDTH);
  const float atlasHeight =
      static_cast<float>(globalImages->transSortImageAtlas->ATLAS_HEIGHT);
  renderThreadParmState->SetParmVector(Parm(rpTransSortAtlasDim), atlasWidth,
                                       atlasHeight, 1.0f / atlasWidth,
                                       1.0f / atlasHeight);
  renderThreadParmState->SetParmFloat(
      Parm(rpSkinSSSRenderEnabled), settings.skipPostProcess ? 0.0f : 1.0f);

  if (r_skipViewParms.valueInteger == 0) {
    renderThreadParmState->VirtualEvaluateParmBlock(
        renderViewObject->r.parmBlock, renderViewObject->r.parmBlock);
    if (r_showParms.valueInteger != 0) {
      idLib::Printf("view parms:\n");
      renderViewObject->r.parmBlock.Print();
    }
  }

  UpdateShadowBufferOptions();
  virtualTextureSystem.UpdateFilterParms(false);
  const std::int64_t firstOcclusionQuery = GL_GetCurrentQueryNumber();
  idParmBlock* finalEnvironmentParms =
      &renderViewObject->finalEnvironmentParms;
  SetupDynamicEnvironment(renderViewObject->r.vieworg,
                          renderViewObject->envBlend,
                          &renderViewObject->blendedEnvironmentParms,
                          &renderViewObject->prevEnvBlendParms,
                          &renderViewObject->r.envOverrideParmBlock);
  SetupAdditiveEnvironmentParms(
      renderViewObject->envBlend, &renderViewObject->r.additiveParmBlock,
      &renderViewObject->blendedEnvironmentParms, finalEnvironmentParms);
  renderThreadParmState->VirtualEvaluateParmBlock(*finalEnvironmentParms,
                                                  *finalEnvironmentParms);

  const float primeIncidence = std::clamp(
      finalEnvironmentParms->GetFloat(Parm(rpShadowPrimeIncidence), nullptr),
      0.0f, 1.0f);
  const float incidenceInfluence = std::clamp(
      finalEnvironmentParms->GetFloat(Parm(rpShadowIncidenceInfluence), nullptr),
      0.0f, 1.0f);
  renderThreadParmState->SetParmVector(Parm(rpPrimeLightMin),
                                       -0.5f * primeIncidence,
                                       incidenceInfluence, 0.0f, 0.0f);

  setupColorGradingParms_t colorGradingParms{};
  colorGradingParms.rpCGShadow = Parm(rpCGShadow);
  colorGradingParms.rpCGMidtone = Parm(rpCGMidtone);
  colorGradingParms.rpCGHighlight = Parm(rpCGHighlight);
  colorGradingParms.rpCGColorDodgeBlend = Parm(rpCGColorDodgeBlend);
  colorGradingParms.rpCGColorBurnBlend = Parm(rpCGColorBurnBlend);
  colorGradingParms.rpCGColorMultiplyBlend = Parm(rpCGColorMultiplyBlend);
  colorGradingParms.rpCGColorScreenBlend = Parm(rpCGColorScreenBlend);
  colorGradingParms.rpCGBlackLevel = Parm(rpCGBlackLevel);
  colorGradingParms.rpColorGradingLUT = Parm(rpColorGradingLUT);
  colorGradingParms.imageLUT =
      tr.imgColorGradingLUT[renderViewObject->viewIndex];
  SetupColorGrading(&colorGradingParms);

  static std::uint32_t grainRandomState[624]{};
  static bool grainRandomInitialized = false;
  if (!grainRandomInitialized) {
    grainRandomInitialized = true;
    grainRandomState[0] = 0;
    for (std::uint32_t i = 1; i < 624; ++i) {
      const std::uint32_t previous = grainRandomState[i - 1];
      grainRandomState[i] =
          i + 1812433253u * (previous ^ (previous >> 30));
    }
  }
  const idVec4 grainAmount =
      renderThreadParmState->GetParmVec4(Parm(rpGrainAmount));
  const idVec4 grainLevelsMin =
      renderThreadParmState->GetParmVec4(Parm(rpGrainLevelsMin));
  const idVec4 grainLevelsMax =
      renderThreadParmState->GetParmVec4(Parm(rpGrainLevelsMax));
  const idVec4 grainHz = renderThreadParmState->GetParmVec4(Parm(rpGrainHz));
  int grainFrequency = static_cast<int>(grainHz.x);
  grainFrequency = std::max(grainFrequency, 1);
  const int grainFrame = grainFrequency < 60
                             ? renderViewObject->r.renderTime /
                                   (1000 / grainFrequency)
                             : tr.frameCount;
  const unsigned grainIndex = static_cast<unsigned>(grainFrame * 2);
  float grainWidth = 1.0f;
  float grainHeight = 1.0f;
  if (const idImage* grainImage =
          renderThreadParmState->GetParmImage(tr.rpGrainMap)) {
    grainWidth = static_cast<float>(grainImage->opts.width);
    grainHeight = static_cast<float>(grainImage->opts.height);
  }
  const float grainRandomX =
      static_cast<float>(static_cast<std::int32_t>(
          GrainRandom(grainRandomState, grainIndex))) *
      (1.0f / 4294967296.0f);
  const float grainRandomY =
      static_cast<float>(static_cast<std::int32_t>(
          GrainRandom(grainRandomState, grainIndex + 1))) *
      (1.0f / 4294967296.0f);
  renderThreadParmState->SetParmVector(
      Parm(rpGrainScaleBias), renderViewObject->renderWidth / grainWidth,
      renderViewObject->renderHeight / grainHeight,
      std::floor(grainRandomX * grainWidth) /
          std::max(1.0f, grainWidth),
      std::floor(grainRandomY * grainHeight) /
          std::max(1.0f, grainHeight));
  renderThreadParmState->SetParmVector(
      Parm(rpGrainParms), grainAmount.x * 2.0f, -grainAmount.x,
      grainLevelsMin.x - grainLevelsMax.x, grainLevelsMax.x);

  if (r_glFinishBeforeGetQuery.valueInteger != 0) {
    RenderEventRecorderLightStartEvent_Impl("optional PreOcclusion finish");
    GL_Finish();
    RenderEventRecorderLightStopEvent_Impl();
  }

  renderCullParms_t cullParms{};
  cullParms.settings = &settings;
  cullParms.renderView = renderViewObject;
  cullParms.areaNodeBounds = world->areaNodeBounds;
  cullParms.worldAreaSubspaceBounds = world->worldAreaSubspaceBounds;
  cullParms.worldAreaGeometryBounds = world->worldAreaGeometryBounds;
  cullParms.numAreaNodes = world->numAreaNodes;
  cullParms.numWorldAreas = world->numWorldAreas;
  cullParms.areaNodeBoundsCulled = areaNodeBoundsCulled;
  cullParms.worldAreaSubspaceBoundsCulled =
      worldAreaSubspaceBoundsCulled;
  cullParms.worldAreaGeometryBoundsCulled =
      worldAreaGeometryBoundsCulled;
  RenderCull(&cullParms);

  renderWalkBSPParms_t walkParms{};
  walkParms.settings = &settings;
  walkParms.renderView = renderViewObject;
  walkParms.areaNodeBoundsCulled = areaNodeBoundsCulled;
  walkParms.areaNodeOcclusionIndex = world->areaNodeOcclusionIndex;
  walkParms.areaNodeOcclusionQuery =
      renderViewObject->occlusionState->nodeOcclusionQuery;
  walkParms.areaNodeBounds = world->areaNodeBounds;
  walkParms.areaNodes = static_cast<const areaNode_t*>(world->areaNodes);
  walkParms.numAreaNodes = world->numAreaNodes;
  walkParms.numWorldAreas = world->numWorldAreas;
  walkParms.occlusionTestNodes = occlusionTestNodes;
  walkParms.numOcclusionTestNodes = &numOcclusionTestNodes;
  walkParms.nodeWorldAreas = nodeWorldAreas;
  walkParms.numNodeWorldAreas = &numNodeWorldAreas;
  RenderWalkBSP(&walkParms);

  const idLookupTable* highlightTable = nullptr;
  float highlightDuration = 1.0f;
  if (tr.declHighlightTable != nullptr) {
    highlightTable = tr.declHighlightTable->table;
    highlightDuration =
        tr.declHighlightTable->right - tr.declHighlightTable->left;
  }
  renderGatherParms_t gatherParms{};
  gatherParms.temp = tempConsidered;
  gatherParms.tempSize = 2048;
  gatherParms.settings = &settings;
  gatherParms.renderView = renderViewObject;
  gatherParms.committedRenderModels = world->committedRenderModels.list;
  gatherParms.numCommittedRenderModels = world->committedRenderModels.num;
  gatherParms.committedRenderLights = world->committedRenderLights.list;
  gatherParms.numCommittedRenderLights = world->committedRenderLights.num;
  gatherParms.nodeWorldAreas = nodeWorldAreas;
  gatherParms.worldAreaSubspaceBoundsCulled =
      worldAreaSubspaceBoundsCulled;
  gatherParms.worldAreaGeometryBoundsCulled =
      worldAreaGeometryBoundsCulled;
  gatherParms.numNodeWorldAreas = numNodeWorldAreas;
  gatherParms.worldAreas = world->worldAreas;
  gatherParms.numWorldAreas = world->numWorldAreas;
  gatherParms.addAlwaysModels = world->addAlwaysModels;
  gatherParms.binaryModelState = renderWorld->binaryModelState.list;
  gatherParms.highlightTable = highlightTable;
  gatherParms.highlightDuration = highlightDuration;
  gatherParms.highlightEnable = r_highlightEnable.valueInteger != 0;
  gatherParms.occlusionState = renderViewObject->occlusionState;
  gatherParms.occlusionTestNodes = occlusionTestNodes;
  gatherParms.numOcclusionTestNodes = &numOcclusionTestNodes;
  gatherParms.viewWorldAreas = viewWorldAreas;
  gatherParms.numViewWorldAreas = &numViewWorldAreas;
  gatherParms.viewWorldSurfaces = viewWorldSurfaces;
  gatherParms.numViewWorldSurfaces = &numViewWorldSurfaces;
  gatherParms.viewModels = viewModels;
  gatherParms.viewModelSortKeys = viewModelSortKeys;
  gatherParms.numViewModels = &numViewModels;
  gatherParms.viewModelsPreZ = viewModelsPreZ;
  gatherParms.numViewModelsPreZ = &numViewModelsPreZ;
  gatherParms.viewGodrayModels = viewGodrayModels;
  gatherParms.numViewGodrayModels = &numViewGodrayModels;
  gatherParms.viewLights = viewLights;
  gatherParms.numViewLights = &numViewLights;
  gatherParms.occlusionTestWorldAreas = occlusionTestWorldAreas;
  gatherParms.numOcclusionTestWorldAreas = &numOcclusionTestWorldAreas;
  gatherParms.occlusionTestModels = occlusionTestModels;
  gatherParms.numOcclusionTestModels = &numOcclusionTestModels;
  gatherParms.occlusionTestLights = occlusionTestLights;
  gatherParms.numOcclusionTestLights = &numOcclusionTestLights;
  gatherParms.splitPlaneDistance = &splitPlaneDistance;
  RenderGather(&gatherParms);

  renderSortDepthParms_t sortDepthParms{};
  sortDepthParms.settings = &settings;
  sortDepthParms.renderView = renderViewObject;
  sortDepthParms.viewModels = viewModels;
  sortDepthParms.viewModelSortKeys = viewModelSortKeys;
  sortDepthParms.numViewModels = numViewModels;
  sortDepthParms.sortedViewModels = viewModels;
  sortDepthParms.renderViewModels = renderViewModels;
  RenderSortDepth(&sortDepthParms);

  renderLightsParms_t lightParms{};
  lightParms.settings = &settings;
  lightParms.renderView = renderViewObject;
  lightParms.drawSurfParms.settings = &settings;
  lightParms.drawSurfParms.renderView = renderViewObject;
  lightParms.drawSurfParms.allowInGameGUIs = false;
  SetupCaptureParms(lightParms.drawSurfParms.captureParms, CAPTURE_GUI_IMAGE,
                    screenRect, renderViewObject->viewIndex, false);
  lightParms.drawSurfParms.renderDestDefault = renderDestScaledColor;
  lightParms.drawSurfParms.renderDestGui = tr.renderDestGui;

  renderShadowBufferParms_t& shadowParms = lightParms.shadowBufferParms;
  shadowParms.settings = &settings;
  shadowParms.renderView = renderViewObject;
  shadowParms.worldAreaSubspaceBounds = world->worldAreaSubspaceBounds;
  shadowParms.worldAreaGeometryBounds = world->worldAreaGeometryBounds;
  shadowParms.worldAreaSubspaceBoundsCulled = worldAreaSubspaceBoundsCulled;
  shadowParms.worldModel = world->hasWorldGeometry ? &worldSpace : nullptr;
  shadowParms.binaryModelState = renderWorld->binaryModelState.list;
  shadowParms.committedRenderModels = world->committedRenderModels.list;
  shadowParms.numCommittedRenderModels = world->committedRenderModels.num;
  shadowParms.rpShadowBufferResolution = Parm(rpShadowBufferResolution);
  shadowParms.renderDestDefault = renderDestScaledColor;
  shadowParms.renderDestShadow = tr.renderDestShadow;
  shadowParms.options.shadowBufferSize = static_cast<int>(
      r_sb_imageSize.valueInteger * r_blendLightShadowSizeScaler.valueFloat);
  shadowParms.options.occluderFacing = r_sb_occluderFacing.valueInteger;
  shadowParms.options.singleSide = r_sb_singleSide.valueInteger;
  shadowParms.options.sliceStep = r_sb_sliceStep.valueFloat;
  shadowParms.options.polyOfsFactor = r_sb_polyOfsFactor.valueFloat;
  shadowParms.options.polyOfsUnits = r_sb_polyOfsUnits.valueFloat;
  shadowParms.options.parallelPolyOfsFactor =
      r_sb_parallelPolyOfsFactor.valueFloat;
  shadowParms.options.parallelPolyOfsUnits =
      r_sb_parallelPolyOfsUnits.valueFloat;
  shadowParms.options.skipShadows = r_skipShadows.valueInteger != 0;
  shadowParms.options.skipShadowModelSort = false;
  shadowParms.options.skipShadowOccluders =
      r_skipShadowOccluders.valueInteger != 0;
  shadowParms.options.skipShadowModelCPUCulling =
      r_skipShadowModelCPUCulling.valueInteger != 0;
  shadowParms.options.skipShadowSurfaceCPUCulling =
      r_skipShadowSurfaceCPUCulling.valueInteger != 0;
  shadowParms.modelConsideredAsOccluder = tempConsidered;

  lightParms.viewLights = viewLights;
  lightParms.numViewLights = numViewLights;
  lightParms.viewModels = viewModels;
  lightParms.numViewModels = numViewModels;
  lightParms.viewWorldSurfaces = viewWorldSurfaces;
  lightParms.numViewWorldSurfaces = numViewWorldSurfaces;
  lightParms.rpLightColor = Parm(rp_lightColor);
  lightParms.rpLightRotation = Parm(rpLightRotation);
  lightParms.rpLightTranslation = Parm(rpLightTranslation);
  lightParms.rpLightScale = Parm(rpLightScale);
  lightParms.rpLightOscillationAmp = Parm(rpLightOscillationAmp);
  lightParms.rpLightOscillationFreq = Parm(rpLightOscillationFreq);
  lightParms.rpLightOscillationPhase = Parm(rpLightOscillationPhase);
  lightParms.rpWindowPosToProjectedTexS = Parm(rpWindowPosToProjectedTexS);
  lightParms.rpWindowPosToProjectedTexT = Parm(rpWindowPosToProjectedTexT);
  lightParms.rpWindowPosToProjectedTexR = Parm(rpWindowPosToProjectedTexR);
  lightParms.rpWindowPosToProjectedTexQ = Parm(rpWindowPosToProjectedTexQ);
  lightParms.rpWindowPosToProjectedShadowS =
      Parm(rpWindowPosToProjectedShadowS);
  lightParms.rpWindowPosToProjectedShadowT =
      Parm(rpWindowPosToProjectedShadowT);
  lightParms.rpWindowPosToProjectedShadowR =
      Parm(rpWindowPosToProjectedShadowR);
  lightParms.rpWindowPosToProjectedShadowQ =
      Parm(rpWindowPosToProjectedShadowQ);
  lightParms.rpShadowProjectionS = Parm(rpShadowProjectionS);
  lightParms.rpShadowProjectionT = Parm(rpShadowProjectionT);
  lightParms.rpShadowProjectionR = Parm(rpShadowProjectionR);
  lightParms.rpShadowProjectionQ = Parm(rpShadowProjectionQ);
  lightParms.rpGlobalLightOrigin = Parm(rpGlobalLightOrigin);
  lightParms.rpStaticSpecularScale = Parm(rpStaticSpecularScale);
  lightParms.rpStaticSpecularVector = Parm(rpStaticSpecularVector);
  lightParms.rpLightDiffuseScale = Parm(rpLightDiffuseScale);
  lightParms.rpLightSpecularScale = Parm(rpLightSpecularScale);
  lightParms.rpLightEnvironmentScale = Parm(rpLightEnvironmentScale);
  lightParms.rpLightEyeDiffuseScale = Parm(rpLightEyeDiffuseScale);
  lightParms.rpLightEyeSpecularScale = Parm(rpLightEyeSpecularScale);
  lightParms.rpLightEyeEnvironmentScale = Parm(rpLightEyeEnvironmentScale);
  lightParms.rpLightProjectionS = Parm(rpLightProjectionS);
  lightParms.rpLightProjectionT = Parm(rpLightProjectionT);
  lightParms.rpLightProjectionR = Parm(rpLightProjectionR);
  lightParms.rpLightProjectionQ = Parm(rpLightProjectionQ);
  lightParms.rpLocalToLightS = Parm(rpLocalToLightS);
  lightParms.rpLocalToLightT = Parm(rpLocalToLightT);
  lightParms.rpLocalToLightR = Parm(rpLocalToLightR);
  lightParms.rpLocalToLightQ = Parm(rpLocalToLightQ);
  lightParms.rpLightFade = Parm(rpLightFade);
  lightParms.rpViewTarget = Parm(rpViewTarget);
  lightParms.rpViewTargetMS = Parm(rpViewTargetMS);
  lightParms.progDepthOnly = tr.progDepthOnly;
  lightParms.progBlendedLightDebugDraw = tr.progBlendedLightDebugDraw;
  lightParms.progDeferredPointLight = tr.progDeferredPointLight;
  lightParms.progDeferredSpotLight = tr.progDeferredSpotLight;
  lightParms.progDeferredParallelLight = tr.progDeferredParallelLight;
  lightParms.progInteractionPrePass = tr.progInteractionPrePass;
  lightParms.progInteractionPostPass = tr.progInteractionPostPass;
  lightParms.renderDestDefault = renderDestScaledColor;
  lightParms.renderDestLinear = renderDestScaledLinear;
  lightParms.zeroOneCubeTris = tr.zeroOneCubeModel->surfaces.list->geometry;
  lightParms.binaryModelState = renderWorld->binaryModelState.list;
  lightParms.numShadows = &numShadows;

  WaitAndSwap(hdc, false);
  console->BeginTimeRange(RANGE_RENDER, &idColor::colorGreen);
  deltaMicroseconds =
      static_cast<int>(Sys_Microseconds()) - lastUsercmdGenMicroSeconds;
  console->BeginTimeRange(RANGE_RENDER, &idColor::colorGreen);
  GL_GetLastFrameTime(tr.pc.startGPUTimeMicroSec, tr.pc.endGPUTimeMicroSec);
  console->AddTimeRange(RANGE_GPU, &idColor::colorGreen,
                        tr.pc.startGPUTimeMicroSec, tr.pc.endGPUTimeMicroSec);
  const int gpuFrameTime = static_cast<int>(tr.pc.endGPUTimeMicroSec -
                                             tr.pc.startGPUTimeMicroSec);
  resolutionScale.SetCurrentGPUFrameTime(gpuFrameTime);
  time_GPUFrameTime = gpuFrameTime;
  SetRenderSize(renderViewObject, screenRect);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      renderViewObject->renderWidth, renderViewObject->renderHeight,
      renderViewObject->windowWidth, renderViewObject->windowHeight);
  renderThreadParmState->SetFragmentPositionToWindow(
      renderViewObject->renderWidth, renderViewObject->renderHeight,
      renderViewObject->windowWidth, renderViewObject->windowHeight);

  if (renderViewObject->r.radialBlurEnabled) {
    idVec4 eye{};
    idVec4 clip{};
    idRenderMatrix::TransformModelToClip(
        renderViewObject->r.radialBlurCenterPos, renderViewObject->viewMatrix,
        renderViewObject->projectionMatrix, eye, clip);
    renderThreadParmState->SetParmVector(
        Parm(rp_radialBlurCenter),
        ((clip.x / clip.w + 1.0f) * 0.5f * renderViewObject->renderWidth) /
            renderViewObject->windowWidth,
        ((clip.y / clip.w + 1.0f) * 0.5f * renderViewObject->renderHeight) /
            renderViewObject->windowHeight,
        0.0f, 0.0f);
  }
  renderThreadParmState->SetParmSampler(
      Parm(rpViewColorLastFrame),
      reinterpret_cast<const idTextureSampler*>(
          tr.imgViewColor[renderViewObject->viewIndex]));
  renderThreadParmState->SetParmSampler(
      Parm(rpDistortionLastFrame),
      reinterpret_cast<const idTextureSampler*>(
          tr.imgDistortion[renderViewObject->viewIndex]));
  GL_StartFrame(tr.FrameNumber());

  if (renderViewObject->envBlend.env != nullptr) {
    renderDynamicEnvParms_t dynamicEnvParms{};
    dynamicEnvParms.settings = &settings;
    dynamicEnvParms.renderView = renderViewObject;
    dynamicEnvParms.rpEnvBlendMip = Parm(rpEnvBlendMip);
    dynamicEnvParms.rpAmbientEnvMap = Parm(rpAmbientEnvMap);
    dynamicEnvParms.rpUserEnvMap = Parm(rpUserEnvMap);
    dynamicEnvParms.rpColor = Parm(rpColor);
    dynamicEnvParms.rpEnvSMatrix = Parm(rpEnvSMatrix);
    dynamicEnvParms.rpEnvTMatrix = Parm(rpEnvTMatrix);
    dynamicEnvParms.rpEnvRMatrix = Parm(rpEnvRMatrix);
    dynamicEnvParms.renderDestDynamicEnv =
        tr.renderDestDynamicEnv[renderViewObject->viewIndex];
    dynamicEnvParms.imgDynamicEnv =
        tr.imgDynamicEnvironment[renderViewObject->viewIndex];
    dynamicEnvParms.imgAmbientEnv = finalEnvironmentParms->GetImage(
        Parm(rpAmbientEnvMap), nullptr);
    dynamicEnvParms.imgUserEnv =
        finalEnvironmentParms->GetImage(Parm(rpUserEnvMap), nullptr);
    dynamicEnvParms.unitCubeTris =
        tr.unitCubeModel->surfaces.list->geometry;
    dynamicEnvParms.progBlend = tr.progEnvironmentBlend;
    const idVec4 rotation = finalEnvironmentParms->GetVector(
        Parm(rpEnvMapRotation), nullptr);
    dynamicEnvParms.envMapRotation =
        idAngles{rotation.x, rotation.y, rotation.z}.ToMat3();
    dynamicEnvParms.envBlend = &renderViewObject->envBlend;
    RenderDynamicEnv(&dynamicEnvParms);
  }

  if (renderViewObject->envBlend.env != nullptr) {
    renderDynamicColorCorrectionParms_t colorCorrectionParms{};
    colorCorrectionParms.settings = &settings;
    colorCorrectionParms.renderView = renderViewObject;
    colorCorrectionParms.rpUserColorCorrection = Parm(rpUserColorCorrection);
    colorCorrectionParms.rpColor = Parm(rpColor);
    colorCorrectionParms.renderDestDynamicColorCorrection =
        tr.renderDestDynamicColorCorrection[renderViewObject->viewIndex];
    colorCorrectionParms.imgDynamicColorCorrection =
        tr.imgDynamicColorCorrection[renderViewObject->viewIndex];
    colorCorrectionParms.imgUserColorCorrection =
        finalEnvironmentParms->GetImage(Parm(rpUserColorCorrection), nullptr);
    colorCorrectionParms.unitSquareTris =
        tr.unitSquareModel->surfaces.list->geometry;
    colorCorrectionParms.progBlend = tr.progColorCorrectionBlend;
    colorCorrectionParms.envBlend = &renderViewObject->envBlend;
    colorCorrectionParms.forceBlend = tr.reblendColorCorrection;
    colorCorrectionParms.rpGammaForBrightness = tr.rpGammaForBrightness;
    colorCorrectionParms.gammaForBrightness = tr.gammaForBrighness;
    tr.reblendColorCorrection = false;
    RenderDynamicColorCorrection(&colorCorrectionParms);
  }

  renderDepthParms_t depthParms{};
  depthParms.settings = &settings;
  depthParms.renderView = renderViewObject;
  depthParms.viewWorldAreas = viewWorldAreas;
  depthParms.numViewWorldAreas = numViewWorldAreas;
  depthParms.viewWorldSurfaces = viewWorldSurfaces;
  depthParms.numViewWorldSurfaces = numViewWorldSurfaces;
  depthParms.viewModels = viewModels;
  depthParms.renderViewModels = renderViewModels;
  depthParms.binaryModelState = renderWorld->binaryModelState.list;
  depthParms.renderDestDepth = renderDestScaledDepth;
  depthParms.renderDestDefault = renderDestScaledColorWithFeedback;
  depthParms.occlusionBaseTriangles =
      world->occlusionBaseModel != nullptr &&
              world->occlusionBaseModel->surfaces.num > 0
          ? world->occlusionBaseModel->surfaces.list->geometry
          : nullptr;
  depthParms.progOccluderDepthOnly = tr.progOccluderDepthOnly;
  depthParms.progPreZ = progDimShadowCreate.Get();
  depthParms.progPreZAlpha = progDimShadowCreateAlpha.Get();
  depthParms.progPreZSecondary = progDimShadowCreateSecondary.Get();
  depthParms.progPreZWound = progDimShadowCreateWound.Get();
  depthParms.imgBlack = globalImages->blackImage;
  depthParms.rpPostSpecularEnabled = Parm(rpSkinSSSRenderEnabled);
  depthParms.extraClears = static_cast<std::uint32_t>(std::max(
      r_extraClears.valueInteger, r_purpleClear.valueInteger != 0 ? 1 : 0));
  depthParms.skipDepthViewStencil =
      r_skipDepthViewStencil.valueInteger != 0;
  depthParms.skipFirstPersonModels =
      r_firstPersonModelsLast.valueInteger != 0;
  depthParms.renderExtraWorldPassDebug =
      r_extraWorldPassForSlowDown.valueInteger;
  depthParms.splitPlaneDistance = splitPlaneDistance;
  depthParms.occlusionState = renderViewObject->occlusionState;
  depthParms.drawSurfs = drawSurfs;
  depthParms.numDrawSurfs = &numDrawSurfs;
  RenderDepth(&depthParms);

  if (!settings.skipSelfShadows) {
    renderSelfShadowsParms_t selfShadowParms{};
    selfShadowParms.selfShadowModelSlaves = dimShadowModelSlaves;
    selfShadowParms.settings = &settings;
    selfShadowParms.renderView = renderViewObject;
    selfShadowParms.committedRenderModels = world->committedRenderModels.list;
    selfShadowParms.numCommittedRenderModels =
        world->committedRenderModels.num;
    selfShadowParms.dimShadowModels = world->dimShadowModels;
    selfShadowParms.renderDestSelfShadow = tr.renderDestShadow;
    selfShadowParms.renderDestDefault = renderDestScaledColor;
    SetupCaptureParms(selfShadowParms.captureParms, CAPTURE_VIEW_COLOR,
                      screenRect, renderViewObject->viewIndex, false);
    selfShadowParms.rpLightDepthCompareMap = Parm(rpLightDepthCompareMap);
    selfShadowParms.rpShadowBufferResolution =
        Parm(rpShadowBufferResolution);
    selfShadowParms.rpSelfShadowMap = Parm(rpSelfShadowMap);
    selfShadowParms.rpSelfShadowFade = Parm(rpSelfShadowFade);
    selfShadowParms.rpSelfShadowEnable = Parm(rpSelfShadowEnable);
    const idDeclRenderParm* modelToShadowS[4] = {
        Parm(rpModelPosToShadowMapS0), Parm(rpModelPosToShadowMapS1),
        Parm(rpModelPosToShadowMapS2), Parm(rpModelPosToShadowMapS3)};
    const idDeclRenderParm* modelToShadowT[4] = {
        Parm(rpModelPosToShadowMapT0), Parm(rpModelPosToShadowMapT1),
        Parm(rpModelPosToShadowMapT2), Parm(rpModelPosToShadowMapT3)};
    const idDeclRenderParm* modelToShadowR[4] = {
        Parm(rpModelPosToShadowMapR0), Parm(rpModelPosToShadowMapR1),
        Parm(rpModelPosToShadowMapR2), Parm(rpModelPosToShadowMapR3)};
    const idDeclRenderParm* modelToShadowQ[4] = {
        Parm(rpModelPosToShadowMapQ0), Parm(rpModelPosToShadowMapQ1),
        Parm(rpModelPosToShadowMapQ2), Parm(rpModelPosToShadowMapQ3)};
    const idDeclRenderParm* inverseX[4] = {
        Parm(rpInvShadowProjX0), Parm(rpInvShadowProjX1),
        Parm(rpInvShadowProjX2), Parm(rpInvShadowProjX3)};
    const idDeclRenderParm* inverseY[4] = {
        Parm(rpInvShadowProjY0), Parm(rpInvShadowProjY1),
        Parm(rpInvShadowProjY2), Parm(rpInvShadowProjY3)};
    const idDeclRenderParm* inverseZ[4] = {
        Parm(rpInvShadowProjZ0), Parm(rpInvShadowProjZ1),
        Parm(rpInvShadowProjZ2), Parm(rpInvShadowProjZ3)};
    const idDeclRenderParm* inverseW[4] = {
        Parm(rpInvShadowProjW0), Parm(rpInvShadowProjW1),
        Parm(rpInvShadowProjW2), Parm(rpInvShadowProjW3)};
    for (int i = 0; i < 4; ++i) {
      selfShadowParms.rpModelPosToShadowMapS[i] = modelToShadowS[i];
      selfShadowParms.rpModelPosToShadowMapT[i] = modelToShadowT[i];
      selfShadowParms.rpModelPosToShadowMapR[i] = modelToShadowR[i];
      selfShadowParms.rpModelPosToShadowMapQ[i] = modelToShadowQ[i];
      selfShadowParms.rpInvShadowProjX[i] = inverseX[i];
      selfShadowParms.rpInvShadowProjY[i] = inverseY[i];
      selfShadowParms.rpInvShadowProjZ[i] = inverseZ[i];
      selfShadowParms.rpInvShadowProjW[i] = inverseW[i];
    }
    selfShadowParms.rpShadowProjectionDepth =
        Parm(rpShadowProjectionDepth);
    selfShadowParms.progSelfShadowCreate = progDimShadowCreate.Get();
    selfShadowParms.progSelfShadowCreateAlpha =
        progDimShadowCreateAlpha.Get();
    selfShadowParms.progSelfShadowCreateWound =
        progDimShadowCreateWound.Get();
    selfShadowParms.progSelfShadowCreateSecondary =
        progDimShadowCreateSecondary.Get();
    selfShadowParms.progSelfShadowSample = progSelfShadowSample.Get();
    selfShadowParms.progSelfShadowSampleAlpha =
        progSelfShadowSampleAlpha.Get();
    selfShadowParms.progSelfShadowSampleSecondary =
        progSelfShadowSampleSecondary.Get();
    selfShadowParms.progSelfShadowSampleWound =
        progSelfShadowSampleWound.Get();
    selfShadowParms.imgBlack = globalImages->blackImage;
    selfShadowParms.numSelfShadows = &numSelfShadows;
    RenderSelfShadows(&selfShadowParms);
  }

  RenderOpaque(&depthParms);
  captureParms_t captureParms{};
  if (settings.screenSpaceReflections != 0) {
    SetupCaptureParms(captureParms, CAPTURE_VIEW_DEPTH, screenRect,
                      renderViewObject->viewIndex, false);
    captureParms.renderDestDefault = renderDestScaledColorWithFeedback;
    RenderCapture(&captureParms);
    SetupCaptureParms(captureParms, CAPTURE_VIEW_NORMAL, screenRect,
                      renderViewObject->viewIndex, false);
    captureParms.renderDestDefault = renderDestScaledColorWithFeedback;
    RenderCapture(&captureParms);
  }
  if (r_firstPersonModelsLast.valueInteger != 0) {
    RenderDepthFirstPerson(&depthParms);
    RenderOpaqueOcclusionTest(&depthParms);
  }

  if (!settings.skipOcclusionSurfaces) {
    renderOcclusionParms_t occlusionParms{};
    occlusionParms.settings = &settings;
    occlusionParms.renderView = renderViewObject;
    occlusionParms.occlusionTestNodes = occlusionTestNodes;
    occlusionParms.numOcclusionTestNodes = numOcclusionTestNodes;
    occlusionParms.occlusionTestWorldAreas = occlusionTestWorldAreas;
    occlusionParms.numOcclusionTestWorldAreas = numOcclusionTestWorldAreas;
    occlusionParms.occlusionTestModels = occlusionTestModels;
    occlusionParms.numOcclusionTestModels = numOcclusionTestModels;
    occlusionParms.occlusionTestLights = occlusionTestLights;
    occlusionParms.numOcclusionTestLights = numOcclusionTestLights;
    occlusionParms.areaNodeOcclusionIndex = world->areaNodeOcclusionIndex;
    occlusionParms.areaNodeBounds = world->areaNodeBounds;
    occlusionParms.worldAreaGeometryBounds = world->worldAreaGeometryBounds;
    occlusionParms.numWorldAreas = world->numWorldAreas;
    occlusionParms.renderDestDefault = renderDestScaledColor;
    occlusionParms.progDepthOnly = tr.progDepthOnly;
    occlusionParms.unitCubeTris = tr.unitCubeModel->surfaces.list->geometry;
    occlusionParms.zeroOneCubeTris =
        tr.zeroOneCubeModel->surfaces.list->geometry;
    occlusionParms.occlusionState = renderViewObject->occlusionState;
    RenderOcclusion(&occlusionParms);
  }

  renderSortParms_t sortParms{};
  sortParms.settings = &settings;
  sortParms.renderView = renderViewObject;
  sortParms.drawSurfs = drawSurfs;
  sortParms.numDrawSurfs = numDrawSurfs;
  sortParms.sortedDrawSurfs = sortedDrawSurfs;
  sortParms.depthSortedDrawSurfs = depthSortedDrawSurfs;
  sortParms.numDepthSortedDrawSurfs = &numDepthSortedDrawSurfs;
  sortParms.renderPasses = renderPasses;
  RenderSort(&sortParms);

  if (settings.screenSpaceReflections != 0) {
    renderScreenSpaceReflectionsParms_t reflectionParms{};
    reflectionParms.settings = &settings;
    reflectionParms.renderView = renderViewObject;
    reflectionParms.unitSquareTris =
        tr.unitSquareModel->surfaces.list->geometry;
    reflectionParms.progScreenSpaceReflections =
        tr.progScreenSpaceReflections;
    reflectionParms.progScreenSpaceReflectionsApply =
        tr.progScreenSpaceReflectionsApply;
    reflectionParms.rpSSRMap = Parm(rpSSRMap);
    reflectionParms.rpSSRParm0 = Parm(rpSSRParm0);
    reflectionParms.rpSSRParm1 = Parm(rpSSRParm1);
    reflectionParms.rpViewColorMap = Parm(rpViewColorMap);
    reflectionParms.imgViewColor = renderDestScaledColor->targetImage[0];
    reflectionParms.renderDestReflections = renderDestScaledColor2;
    reflectionParms.renderDestDefault = renderDestScaledColor;
    resolutionScale.GetCurrentResolutionScale(reflectionParms.xScale,
                                               reflectionParms.yScale);
    RenderScreenSpaceReflections(&reflectionParms,
                                 settings.screenSpaceReflections);
  }

  const int firstPersonModelsLast = r_firstPersonModelsLast.valueInteger;
  if (firstPersonModelsLast != 0) {
    RenderOpaqueFirstPerson(&depthParms);
  }
  if (settings.screenSpaceReflections == 0 || firstPersonModelsLast != 0) {
    SetupCaptureParms(captureParms, CAPTURE_VIEW_DEPTH, screenRect,
                      renderViewObject->viewIndex, false);
    captureParms.renderDestDefault = renderDestScaledColorWithFeedback;
    RenderCapture(&captureParms);
    SetupCaptureParms(captureParms, CAPTURE_VIEW_NORMAL, screenRect,
                      renderViewObject->viewIndex, false);
    captureParms.renderDestDefault = renderDestScaledColorWithFeedback;
    RenderCapture(&captureParms);
  }
  SetupCaptureParms(captureParms, CAPTURE_FEEDBACK, screenRect,
                    renderViewObject->viewIndex, false);
  captureParms.renderDestDefault = renderDestScaledColorWithFeedback;
  RenderCapture(&captureParms);

  renderLightsMode_t lightMode = RENDERLIGHTS_ALL;
  if (!settings.skipSubSurfaceScattering) {
    GL_SetRenderDestination({renderDestScaledColor}, 0, 0);
    RenderLights(&lightParms, RENDERLIGHTS_INTERACTION_SSS);
    lightMode = static_cast<renderLightsMode_t>(
        RENDERLIGHTS_BLENDED | RENDERLIGHTS_INTERACTION_SPECULAR_SIMPLE |
        RENDERLIGHTS_INTERACTION_DIFFUSE_SIMPLE);

    renderPassParms_t sssPass{};
    SetupRenderPassParms(sssPass, RENDERPASS_SSS, screenRect, false);
    SetupCaptureParms(sssPass.drawSurfParms.captureParms, CAPTURE_VIEW_COLOR,
                      screenRect, renderViewObject->viewIndex, false);
    sssPass.drawSurfParms.captureParms.renderDestViewColor =
        tr.renderDestViewPostScratch;
    sssPass.renderDestTemp = renderDestScaledColor2;
    RenderPass(&sssPass);
  }

  SetupCaptureParms(captureParms, CAPTURE_VIEW_COLOR, screenRect,
                    renderViewObject->viewIndex, false);
  captureParms.renderDestDefault = renderDestScaledColorWithFeedback;
  RenderCapture(&captureParms);
  if (renderViewObject->r.forceFullVirtualTextureLoad) {
    virtualTextureSystem.PushFeedback(
        virtualTextureSystem.feedbackBuffer.buffer, 160, 120,
        virtualTextureSystem.feedbackBufferPitch, false);
    virtualTextureSystem.FinishFeedback(false, 0);
  }

  GL_SetRenderDestination({renderDestScaledColor}, 0, 0);
  if (r_skipDimShadows.valueInteger == 0 &&
      !renderViewObject->r.skipDimShadows) {
    renderDimShadowsParms_t dimShadowParms{};
    dimShadowParms.dimShadowModelSlaves = dimShadowModelSlaves;
    dimShadowParms.settings = &settings;
    dimShadowParms.renderView = renderViewObject;
    dimShadowParms.committedRenderModels = world->committedRenderModels.list;
    dimShadowParms.numCommittedRenderModels = world->committedRenderModels.num;
    dimShadowParms.dimShadowModels = world->dimShadowModels;
    dimShadowParms.extrudeBox = tr.extrudeBoxModel->surfaces.list->geometry;
    dimShadowParms.renderDestDimShadow = tr.renderDestShadow;
    dimShadowParms.renderDestDefault = renderDestScaledColor;
    dimShadowParms.imgViewColor =
        tr.imgViewColor[renderViewObject->viewIndex];
    dimShadowParms.rpViewColorMap = Parm(rpViewColorMap);
    dimShadowParms.rpDimShadowDepthMap = Parm(rpDimShadowDepthMap);
    dimShadowParms.rpDimShadowFade = Parm(rpDimShadowFade);
    dimShadowParms.rpDimShadowExtrude = Parm(rpDimShadowExtrude);
    dimShadowParms.rpDimShadowClipPlane = Parm(rpDimShadowClipPlane);
    dimShadowParms.rpWindowPosToDimShadowBlurS =
        Parm(rpWindowPosToDimShadowBlurS);
    dimShadowParms.rpWindowPosToDimShadowBlurT =
        Parm(rpWindowPosToDimShadowBlurT);
    dimShadowParms.rpWindowPosToDimShadowBlurR =
        Parm(rpWindowPosToDimShadowBlurR);
    dimShadowParms.rpWindowPosToDimShadowBlurQ =
        Parm(rpWindowPosToDimShadowBlurQ);
    dimShadowParms.rpVertexMorphScale = Parm(rpVertexMorphScale);
    dimShadowParms.rpDimShadowBufferResolution =
        Parm(rpDimShadowBufferResolution);
    dimShadowParms.progDimShadowCreate = progDimShadowCreate.Get();
    dimShadowParms.progDimShadowCreateWound = progDimShadowCreateWound.Get();
    dimShadowParms.progDimShadowCreateAlpha = progDimShadowCreateAlpha.Get();
    dimShadowParms.progDimShadowCreateSecondary =
        progDimShadowCreateSecondary.Get();
    dimShadowParms.progDimShadowDraw = progDimShadowDraw.Get();
    dimShadowParms.progDimShadowDrawMidQuality =
        tr.progDimShadowDrawMidQuality;
    dimShadowParms.progDimShadowDrawHighQuality =
        progDimShadowDrawHighQuality.Get();
    dimShadowParms.progDimShadowStencil = progDimShadowStencil.Get();
    dimShadowParms.progDimShadowDebugDraw = progDimShadowDebugDraw.Get();
    dimShadowParms.occlusionState = renderViewObject->occlusionState;
    dimShadowParms.numDimShadows = &numDimShadows;
    dimShadowParms.modelDimShadowState = modelDimShadowState;
    RenderDimShadows(&dimShadowParms);
  }

  RenderLights(&lightParms, lightMode);
  if (!settings.skipFog) {
    renderFogParms_t fogParms{};
    fogParms.settings = &settings;
    fogParms.renderView = renderViewObject;
    fogParms.progGlobalFog = tr.progGlobalFog;
    fogParms.unitSquareTris = tr.unitSquareModel->surfaces.list->geometry;
    RenderFog(&fogParms);
  }

  auto renderPass = [&](renderPass_t pass, bool allowInGameGUIs,
                        const idRenderDestination* destination = nullptr) {
    renderPassParms_t parms{};
    SetupRenderPassParms(parms, pass, screenRect, allowInGameGUIs);
    parms.binaryModelState = renderWorld->binaryModelState.list;
    if (destination != nullptr) {
      parms.renderDestDefault = destination;
    }
    RenderPass(&parms);
  };

  if (!settings.skipBlendedSurfaces) {
    renderPass(RENDERPASS_BACKGROUND, false);
  }
  if (settings.sortBlendedSurfacesBackToFront) {
    if (!settings.skipBlendedSurfaces) {
      renderPass(RENDERPASS_SORTED, true);
    }
  } else if (!settings.skipBlendedSurfaces) {
    renderPass(RENDERPASS_EMISSIVE, false);
    if (!settings.skipBlendedSurfaces) {
      renderPass(RENDERPASS_BLEND, true);
    }
  }

  if (!settings.skipGodRays) {
    renderGodRaysParms_t godRayParms{};
    godRayParms.settings = &settings;
    godRayParms.renderView = renderViewObject;
    godRayParms.progCreateGodRays = tr.progCreateGodRayMask;
    godRayParms.progDrawGodRays = tr.progDrawGodRay;
    godRayParms.renderDestDefault = renderDestScaledColor;
    godRayParms.renderDestOffScreen =
        tr.renderDestDistortion[renderViewObject->viewIndex];
    godRayParms.imgOffScreen =
        tr.imgDistortion[renderViewObject->viewIndex];
    godRayParms.unitSquareTris =
        tr.unitSquareModel->surfaces.list->geometry;
    godRayParms.viewGodrayModels = viewGodrayModels;
    godRayParms.numViewGodrayModels = numViewGodrayModels;
    godRayParms.rpGodRayMap = tr.rpDistortionMap;
    RenderGodRays(&godRayParms);
  }

  if (!settings.skipEmissiveGlare) {
    renderPass(RENDERPASS_CLEAR, false, renderDestScaledFeedback);
    if (!settings.skipEmissiveGlare && !settings.skipEmissiveSurfaces) {
      renderPass(RENDERPASS_BACKGROUND_EMISSIVE, false,
                 renderDestScaledFeedback);
    }
    if (!settings.skipEmissiveGlare && !settings.skipEmissiveSurfaces) {
      renderPass(RENDERPASS_EMISSIVE, false, renderDestScaledFeedback);
    }
    if (!settings.skipEmissiveGlare && !settings.skipEmissiveSurfaces) {
      renderPass(RENDERPASS_EMISSIVE_ONLY, false,
                 renderDestScaledFeedback);
    }
    if (!settings.skipEmissiveGlare) {
      SetupCaptureParms(captureParms, CAPTURE_GLARE_MAP, screenRect,
                        renderViewObject->viewIndex, true);
      captureParms.renderDestDefault = renderDestScaledFeedback;
      RenderCapture(&captureParms);
    }
  }
  if (!settings.skipDistortionSurfaces) {
    renderPass(RENDERPASS_DISTORTION, false);
  }

  SetupCaptureParms(captureParms, CAPTURE_VIEW_COLOR, screenRect,
                    renderViewObject->viewIndex, true);
  CaptureViewColor(&captureParms);

  if (!settings.skipEmissiveGlare) {
    if (settings.skipAdaptiveGlare) {
      renderGlareParms_t glareParms{};
      glareParms.settings = &settings;
      glareParms.renderView = renderViewObject;
      resolutionScale.GetCurrentResolutionScale(glareParms.xScale,
                                                 glareParms.yScale);
      glareParms.renderDestDefault = renderDestScaledColor;
      glareParms.renderDestViewColor =
          tr.renderDestViewColor[renderViewObject->viewIndex];
      glareParms.renderDestMip3 = tr.renderDestMip3;
      glareParms.renderDestMip3Alt = tr.renderDestMip3Alt;
      glareParms.imgBlack = globalImages->blackImage;
      glareParms.rpGlareMap = tr.rpGlareMap;
      glareParms.rpGlareStep = tr.rpGlareStep;
      glareParms.progGauss = tr.progGauss;
      glareParms.progGlareScale = tr.progGlareScale;
      glareParms.unitSquareTris =
          tr.unitSquareModel->surfaces.list->geometry;
      RenderGlare(&glareParms);
    } else {
      renderAdapativeGlareParms_t adaptiveParms{};
      adaptiveParms.settings = &settings;
      adaptiveParms.renderView = renderViewObject;
      adaptiveParms.frameNumber = renderSystem->FrameNumber();
      resolutionScale.GetCurrentResolutionScale(adaptiveParms.xScale,
                                                 adaptiveParms.yScale);
      if (r_adaptiveGlareTest.valueInteger != 0) {
        adaptiveParms.minThreshold = r_adaptiveGlareMinThreshold.valueFloat;
        adaptiveParms.maxThreshold = r_adaptiveGlareMaxThreshold.valueFloat;
        adaptiveParms.avgBrightness = r_adaptiveGlareAvgBrightness.valueFloat;
        adaptiveParms.decayRate = r_adaptiveGlareDecayRate.valueFloat;
        adaptiveParms.color = {1.0f, 1.0f, 1.0f, 0.0f};
      } else {
        adaptiveParms.minThreshold = finalEnvironmentParms->GetFloat(
            Parm(rpBloomMinThreshold), nullptr);
        adaptiveParms.maxThreshold = finalEnvironmentParms->GetFloat(
            Parm(rpBloomMaxThreshold), nullptr);
        adaptiveParms.avgBrightness = finalEnvironmentParms->GetFloat(
            Parm(rpBloomBrightness), nullptr);
        adaptiveParms.decayRate = finalEnvironmentParms->GetFloat(
            Parm(rpBloomDecay), nullptr);
        adaptiveParms.color = finalEnvironmentParms->GetVector(
            Parm(rpBloomColor), nullptr);
      }
      adaptiveParms.frameExposureMin = r_frameExposureMin.valueFloat;
      adaptiveParms.frameExposureMax = r_frameExposureMax.valueFloat;
      adaptiveParms.frameExposureDecay = r_frameExposureDecay.valueFloat;
      adaptiveParms.frameExposureAmount =
          r_frameExposureTest.valueInteger != 0
              ? 1.0f
              : finalEnvironmentParms->GetFloat(
                    Parm(rpFrameExposureAmount), nullptr);
      adaptiveParms.frameExposureSaturation =
          r_frameExposureSaturation.valueFloat;
      adaptiveParms.envBlend = &renderViewObject->envBlend;
      adaptiveParms.renderDestDefault = renderDestScaledColor;
      adaptiveParms.renderDestViewColor =
          tr.renderDestViewColor[renderViewObject->viewIndex];
      adaptiveParms.renderDestBloom = tr.renderDestBloom;
      adaptiveParms.renderDestAdaptiveGlare = tr.renderDestMip3;
      adaptiveParms.renderDestAdaptiveGlareAlt = tr.renderDestMip3Alt;
      adaptiveParms.renderDestLuminosity = tr.renderDestLuminosity;
      adaptiveParms.renderDestLuminosityAlt = tr.renderDestLuminosityAlt;
      adaptiveParms.renderDestFrameExposure = tr.renderDestFrameExposure;
      adaptiveParms.renderDestFrameExposureAlt =
          tr.renderDestFrameExposureAlt;
      adaptiveParms.rpViewColorMap = Parm(rpViewColorMap);
      adaptiveParms.rpGlareMap = tr.rpGlareMap;
      adaptiveParms.rpAdaptiveGlareMap = tr.rpAdaptiveGlareMap;
      adaptiveParms.rpAdaptiveGlareMapAlt = tr.rpAdaptiveGlareMapAlt;
      adaptiveParms.rpDownSample2x2Map = nullptr;
      adaptiveParms.rpLuminosityMap = tr.rpLuminosityMap;
      adaptiveParms.rpLuminosityMapAlt = tr.rpLuminosityMapAlt;
      adaptiveParms.rpAdaptiveLuminance = tr.rpAdaptiveLuminance;
      adaptiveParms.rpAdaptiveGlareBlurMap = tr.rpAdaptiveGlareBlurMap;
      adaptiveParms.rpAdaptiveGlareBlurStep = tr.rpAdaptiveGlareBlurStep;
      adaptiveParms.rpAdaptiveGlareParms = tr.rpAdaptiveGlareParms;
      adaptiveParms.rpAdaptiveGlareColor = tr.rpAdaptiveGlareColor;
      adaptiveParms.rpFrameExposureMap = tr.rpFrameExposureMap;
      adaptiveParms.rpFrameExposureParms1 = tr.rpFrameExposureParms1;
      adaptiveParms.rpFrameExposureParms2 = tr.rpFrameExposureParms2;
      adaptiveParms.progLuminosity = tr.progLuminosity;
      adaptiveParms.progBloomThreshold = tr.progBloomThreshold;
      adaptiveParms.progAdaptiveGlareGauss = tr.progAdaptiveGlareGauss;
      adaptiveParms.progFrameExposure = tr.progFrameExposure;
      adaptiveParms.imgViewColor =
          tr.imgViewColor[renderViewObject->viewIndex];
      adaptiveParms.imgGlare = tr.imgScreenMip3;
      adaptiveParms.imgLuminosity = tr.imgLuminosity;
      adaptiveParms.imgLuminosityAlt = tr.imgLuminosityAlt;
      adaptiveParms.imgFrameExposure = tr.imgFrameExposure;
      adaptiveParms.imgFrameExposureAlt = tr.imgFrameExposureAlt;
      adaptiveParms.imgBlack = globalImages->blackImage;
      adaptiveParms.unitSquareTris =
          tr.unitSquareModel->surfaces.list->geometry;
      RenderAdaptiveGlare(&adaptiveParms);
    }
  }

  if (!settings.skipPostProcess) {
    renderPostProcessParms_t postProcessParms{};
    postProcessParms.settings = &settings;
    postProcessParms.renderView = renderViewObject;
    postProcessParms.rpViewColorMap = Parm(rpViewColorMap);
    postProcessParms.rpGlareMap = tr.rpGlareMap;
    postProcessParms.rpAdaptiveGlareMap = tr.rpAdaptiveGlareMap;
    postProcessParms.rpAdaptiveGlareLuminance = tr.rpAdaptiveLuminance;
    postProcessParms.rpDistortionMap = tr.rpDistortionMap;
    postProcessParms.progPostProcess = tr.progPostProcess;
    postProcessParms.imgViewColor =
        tr.imgViewColor[renderViewObject->viewIndex];
    postProcessParms.imgGlare = tr.imgScreenMip3;
    postProcessParms.imgAdaptiveGlare = tr.imgScreenMip3Alt;
    idRenderDestination* luminosityDestination = tr.renderDestLuminosity;
    if (renderSystem->FrameNumber() % 2 == 0) {
      luminosityDestination = tr.renderDestLuminosityAlt;
    }
    postProcessParms.imgAdaptiveGlareLuminance =
        luminosityDestination->targetImage[0];
    postProcessParms.imgBlack = globalImages->blackImage;
    postProcessParms.imgDistortion =
        tr.imgDistortion[renderViewObject->viewIndex];
    postProcessParms.unitSquareTris =
        tr.unitSquareModel->surfaces.list->geometry;
    postProcessParms.rpDofMipScale = Parm(rpDofMipScale);
    if (settings.postProcessDofMode == 1 &&
        tr.progSlowDepthOfField == nullptr) {
      tr.progSlowDepthOfField = static_cast<const idDeclRenderProg*>(
          idDeclRenderProg::resourceList.FindWithInheritance(
              "slowDepthOfField", false, true));
    }
    postProcessParms.progSlowDepthOfField = tr.progSlowDepthOfField;
    postProcessParms.progOverlappedDownSampleMaxAlpha =
        tr.progOverlappedDownSampleMaxAlpha;
    postProcessParms.rpGlobalBlur = Parm(rp_globalBlur);
    postProcessParms.rpDofOffset = Parm(rpDofOffset);
    postProcessParms.progCalcDepthOfFieldValues =
        tr.progCalcDepthOfFieldValues;
    postProcessParms.progCalcDepthOfFieldMaxKernel =
        tr.progCalcDepthOfFieldMaxKernel;
    if (r_postProcessHQ.valueInteger != 0) {
      postProcessParms.progDepthFilterPass =
          tr.progDepthOfFieldFilterPass_HQ;
      postProcessParms.progDepthFilterDoublePass =
          tr.progDepthOfFieldDoubleFilterPass_HQ;
    } else {
      postProcessParms.progDepthFilterPass = tr.progDepthOfFieldFilterPass;
      postProcessParms.progDepthFilterDoublePass =
          tr.progDepthOfFieldDoubleFilterPass;
    }
    postProcessParms.rpDepthFilterParams = Parm(rpDepthFilterParams);
    postProcessParms.renderDestViewColor =
        tr.renderDestViewColor[renderViewObject->viewIndex];
    postProcessParms.renderDestViewPostScratch =
        tr.renderDestViewPostScratch;
    postProcessParms.renderDestDefault = renderDestScaledColor;
    postProcessParms.imgViewPostScratch = tr.imgViewPostScratch;
    postProcessParms.renderDestMip = tr.renderDestMip1;
    postProcessParms.rpMipMapSampler = Parm(rpMipMapSampler);
    postProcessParms.progOverlappedDownSample =
        tr.progOverlappedDownSample;

    const float dofNearEnd = finalEnvironmentParms->GetFloat(
        Parm(rpDofNearEnd), nullptr);
    const float dofFocusBegin = finalEnvironmentParms->GetFloat(
        Parm(rpDofFocusBegin), nullptr);
    const float dofFocusEnd = finalEnvironmentParms->GetFloat(
        Parm(rpDofFocusEnd), nullptr);
    const float dofFarBegin = finalEnvironmentParms->GetFloat(
        Parm(rpDofFarBegin), nullptr);
    const float nearRange = dofFocusBegin - dofNearEnd;
    const float farRange = dofFarBegin - dofFocusEnd;
    const float inverseNear =
        idMath::FLT_SMALLEST_NON_DENORMAL <= nearRange
            ? 1.0f / nearRange
            : 0.0f;
    const float inverseFar =
        idMath::FLT_SMALLEST_NON_DENORMAL <= farRange
            ? 1.0f / farRange
            : 1.0f;
    renderThreadParmState->SetParmVector(Parm(rpDofRecipValues), inverseNear,
                                         inverseFar, 0.0f, 0.0f);
    renderThreadParmState->SetParmFloat(
        Parm(rpShowDepthOfFieldDebug),
        r_showDepthOfFieldDebug.valueInteger != 0 ? 1.0f : 0.0f);
    renderThreadParmState->SetParmFloat(
        Parm(rpShowGrainLevelsDebug),
        r_showGrainLuminanceDebug.valueInteger != 0 ? 1.0f : 0.0f);
    RenderPostProcess(&postProcessParms);
  }

  if (!settings.skipHazeFlare) {
    renderHazeFlareParms_t hazeFlareParms{};
    hazeFlareParms.settings = &settings;
    hazeFlareParms.renderView = renderViewObject;
    hazeFlareParms.unitSquareTris =
        tr.unitSquareModel->surfaces.list->geometry;
    hazeFlareParms.progHazeFlareBlurPass1 = tr.progHazeFlareBlurPass1;
    hazeFlareParms.progHazeFlareBlurPass2 = tr.progHazeFlareBlurPass2;
    hazeFlareParms.progHazeFlare = tr.progHazeFlare;
    hazeFlareParms.rpHazeFlareMap = Parm(rpHazeFlareMap);
    hazeFlareParms.rpMipMapSampler = Parm(rpMipMapSampler);
    hazeFlareParms.rpGlareMap = tr.rpGlareMap;
    hazeFlareParms.imgViewColor =
        tr.imgViewColor[renderViewObject->viewIndex];
    hazeFlareParms.imgGlare = settings.skipAdaptiveGlare
                                  ? tr.imgScreenMip3
                                  : tr.imgScreenMip3Alt;
    hazeFlareParms.imgHazeFlare[0] = tr.imgHazeFlare[0];
    hazeFlareParms.imgHazeFlare[1] = tr.imgHazeFlare[1];
    hazeFlareParms.renderDestHazeFlare[0] = tr.renderDestHazeFlare[0];
    hazeFlareParms.renderDestHazeFlare[1] = tr.renderDestHazeFlare[1];
    hazeFlareParms.renderDestDefault = renderDestScaledColor;
    RenderHazeFlare(&hazeFlareParms);
  }

  renderThreadParmState->VirtualRollbackParmBlock(*finalEnvironmentParms,
                                                  *finalEnvironmentParms);
  renderViewObject->firstOcclusionQueryInPreviousFrame = firstOcclusionQuery;
  if (renderViewObject->firstOcclusionQueryInPreviousFrame < 1) {
    renderViewObject->firstOcclusionQueryInPreviousFrame = 1;
  }
  ++renderViewObject->occlusionState->renderFrameNumber;
  tr.pc.c_viewWorldAreas = numViewWorldAreas;
  tr.pc.c_viewModels = numViewModels;
  tr.pc.c_viewLights = numViewLights;
  tr.pc.c_viewShadows = numShadows;
  tr.pc.c_viewDimShadows = numDimShadows;
  tr.pc.c_viewSelfShadows = numSelfShadows;
  if (r_skipViewParms.valueInteger == 0) {
    renderThreadParmState->VirtualRollbackParmBlock(
        renderViewObject->r.parmBlock, renderViewObject->r.parmBlock);
  }
  PrintStats();
}
