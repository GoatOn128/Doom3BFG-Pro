#include "engine/renderer/renderlocal.h"

#include "engine/framework/console.h"
#include "engine/models/renderlight.h"
#include "engine/models/rendermodel.h"
#include "engine/models/rendermodelgui.h"
#include "engine/models/renderworld.h"
#include "engine/renderer/bufferobject.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/glconfig.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/rendersystem_ogl_pc.h"
#include "engine/renderer/rendersystem_runtime.h"
#include "engine/renderer/renderthread.h"
#include "engine/renderer/resolutionscale.h"
#include "engine/renderer/virtualtexture.h"
#include "engine/framework/coderesource.h"
#include "shared/idlib/csystems/autocomplete.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
#include <type_traits>

extern idCVar forge_logPerformanceMetrics;

idRenderSystemLocal tr;
idRenderSystem* renderSystem = &tr;

// All 89 cvars below are owned by RenderSystem.obj. Names, defaults, flags,
// ranges, descriptions, and completion callbacks match its initializer code.
idCVar r_swapInterval(
    "r_swapInterval", "-1", 18,
    "changes wglSwapInterval. 0 = off, 1 = vsync 60Hz, -1 = swap-tear 60Hz");
idCVar r_shadows("r_shadows", "1", 1, "enable shadows");
idCVar r_skipPostProcess("r_skipPostProcess", "0", 2,
                         "skip post processing");
idCVar r_skipEmissiveGlare("r_skipEmissiveGlare", "0", 2,
                           "skip emissive glare processing");
idCVar r_testMegaTexture("r_testMegaTexture", "0", 1,
                         "draw the entire megatexture as a flat quad");
idCVar r_showTotalSurfaceArea(
    "r_showTotalSurfaceArea", "0", 2,
    "print the total surface area of all static surfaces (2=include non-statics too)");
idCVar r_useDynamicEnvironment(
    "r_useDynamicEnvironment", "1", 2,
    "generate a new environment map every frame");
idCVar r_useSMP("r_useSMP", "1", 1,
                "1 = run the renderer in a separate thread");
idCVar r_usePBOTextureFeedback(
    "r_usePBOTextureFeedback", "1", 1,
    "use async pixel buffer read for texture feedback query");
idCVar r_singleTriangle("r_singleTriangle", "0", 1,
                        "only draw a single triangle per primitive");
idCVar r_checkBounds("r_checkBounds", "0", 1,
                     "compare all surface bounds with precalculated ones");
idCVar r_znear("r_znear", "3", 4, "near Z clip plane distance", 0.001F,
               200.0F);
idCVar r_zfar("r_zfar", "0", 4,
              "far Z clip plane distance, 0 = infinity", 0.0F, 200000.0F);
idCVar r_gamma("r_gamma", "1.0", 4, "changes gamma tables", 0.5F, 3.0F);
idCVar r_brightness("r_brightness", "1.0", 65540, "changes gamma tables",
                    0.5F, 2.0F);
idCVar r_jitter("r_jitter", "0", 1,
                "randomly subpixel jitter the projection matrix");
idCVar r_skipCommits(
    "r_skipCommits", "0", 1,
    "1 = don't accept any entity or light updates, making everything static");
idCVar r_logFile("r_logFile", "0", 2,
                 "number of frames to emit render logs");
idCVar r_clear(
    "r_clear", "2", 0,
    "force screen clear every frame, 1 = purple, 2 = black, 'r g b' = custom");
idCVar r_testGamma("r_testGamma", "0", 4,
                   "if > 0 draw a grid pattern to test gamma levels", 0.0F,
                   195.0F);
idCVar r_testGammaBias("r_testGammaBias", "0", 4,
                       "if > 0 draw a grid pattern to test gamma levels");
idCVar r_calibrateGamma(
    "r_calibrateGamma", "0", 4,
    "if > 0 draw a swatch at a specific luminance for calibrating gamma");
idCVar r_calibrateGammaMode(
    "r_calibrateGammaMode", "0", 4,
    "0=measure total luminance, 1=measure red, 2=measure green, 3=measure blue");
idCVar r_calibrateGammaLum(
    "r_calibrateGammaLum", "0", 4,
    "luminance to display for calibrating gamma [0,255]");
idCVar r_skipVmtrs("r_skipVmtrs", "0", 1, "1 = skip loading of vmtrs");
idCVar r_showVertexColor("r_showVertexColor", "0", 1,
                         "draws all triangles with the solid vertex color");
idCVar r_showCommits("r_showCommits", "0", 2,
                     "report model and light commits and ref counts");
idCVar r_showRenderModelCommits(
    "r_showRenderModelCommits", "0", 2,
    "report model and light commits and ref counts, 1 = print stats in console, 2 = also show rendermodel commits by type");
idCVar r_showDynamic("r_showDynamic", "0", 2,
                     "report stats on dynamic surface generation");
idCVar r_showTrace("r_showTrace", "0", 2,
                   "show the intersection of an eye trace with the world",
                   0.0F, 2.0F,
                   idCmdSystem::ArgCompletion_Integer<0, 2>);
idCVar r_showIntensity(
    "r_showIntensity", "0", 1,
    "draw the screen colors based on intensity, red = 0, green = 128, blue = 255");
idCVar r_showDestinationAlpha("r_showDestinationAlpha", "0", 1,
                              "draw destination alpha");
idCVar r_showStats(
    "r_showStats", "0", 2,
    "report render stats. 0 = off, 1 = show and update, 2 = show but don't update",
    0.0F, 2.0F);
idCVar r_showDeltaStats("r_showDeltaStats", "0", 2,
                        "report render stat deltas");
idCVar r_showRenderLog("r_showRenderLog", "0", 2, "report stats");
idCVar r_showQueries("r_showQueries", "0", 1,
                     "reports the number of occlusion queries per frame");
idCVar r_showLights(
    "r_showLights", "0", 2,
    "1 = just print volumes numbers, highlighting ones covering the view, 2 = also draw planes of each volume, 3 = also draw edges of each volume",
    0.0F, 3.0F, idCmdSystem::ArgCompletion_Integer<0, 3>);
idCVar r_showModels(
    "r_showModels", "0", 2,
    "1 = displays the bounding boxes of all view models, 2 = print index numbers");
idCVar r_showEnvs("r_showEnvs", "0", 1,
                  "displays the bounding boxes of all the dynamic environments");
idCVar r_showSurfaceBounds(
    "r_showSurfaceBounds", "0", 2,
    "1 = displays the bounding boxes of all model surfaces");
idCVar r_showAmbientLighting(
    "r_showAmbientLighting", "0", 2,
    "Draw boxes for all the ambient samples, and lines from all dynamic models to their chosen ambients");
idCVar r_showDynamicLightingSingleModel(
    "r_showDynamicLightingSingleModel", "-1", 2,
    "suppress show dynamic lighting on all but one model specified with this model index");
idCVar r_showOcclusionBaseModel("r_showOcclusionBaseModel", "0", 2,
                                "show sound BSP base occlusion model");
idCVar r_showManifolds(
    "r_showManifolds", "0", 2,
    "0 = off, 1 = all manifolds, 2 = significant manifolds, 3 = omni-significant manifolds, 4 = show detail manifolds, 5 = show pipe manifolds, 6 = subdivided world manifolds");
idCVar r_showTJunctions(
    "r_showTJunctions", "0", 2,
    "0 = off, 1 = all t-junctions, 2 = t-junctions on significant occluders, 3 = j-junctions with vertices between two colinear edges, 4 =  = j-junctions with vertices between two colinear edges on significant occluders");
idCVar r_showDuplicateTriangles("r_showDuplicateTriangles", "0", 1,
                                "show duplicate triangles");
idCVar r_showSpecularTriangles("r_showSpecularTriangles", "0", 1,
                               "show specular triangles");
idCVar r_showWorldAreas(
    "r_showWorldAreas", "0", 2,
    "0 = 0ff, 1 = show world areas, 2 = lock view and show world areas");
idCVar r_showLightCount(
    "r_showLightCount", "0", 2,
    "1 = colors surfaces based on light count, 2 = also print overdraw",
    0.0F, 3.0F, idCmdSystem::ArgCompletion_Integer<0, 2>);
idCVar r_showTris(
    "r_showTris", "0", 2,
    "enables wireframe rendering of the world, 1 = only draw visible ones, 2 = draw all front facing, 3 = draw all, 4 = only visible ones with blended lines",
    -4.0F, 4.0F, idCmdSystem::ArgCompletion_Integer<-4, 4>);
idCVar r_showDrawColor("r_showDrawColor", "1 1 1 0.4", 0x10000,
                       "specifies the draw color for r_showTris mode 4");
idCVar r_showTwoSided("r_showTwoSided", "0", 2,
                      "outline two sided geometry");
idCVar r_showBlendedSurfaces(
    "r_showBlendedSurfaces", "0", 2,
    "outline and draw blended geometry, 1 = all, 2 = emissive pass, 3 = blended pass, 4 = background sort, 5 = emit sort, 6 = light sort, 7 = decal sort, 8 = trans sort, 9 = water sort");
idCVar r_showSurfaceInfo("r_showSurfaceInfo", "0", 2,
                         "show surface material name under crosshair");
idCVar r_showNoDrawSurfaceInfo(
    "r_showNoDrawSurfaceInfo", "0", 1,
    "allow r_showSurfaceInfo to intersect materials that don't draw ( \"isdrawn 0\" )");
idCVar r_showTraceWorld("r_showTraceWorld", "0", 2,
                        "show traceworld info under crosshair");
idCVar r_showNormals("r_showNormals", "0", 4, "draws wireframe normals");
idCVar r_showDepth("r_showDepth", "0", 1,
                   "display the contents of the depth buffer and the depth range");
idCVar r_showCracks("r_showCracks", "0", 2,
                    "display cracks less that the given number of pixels wide");
idCVar r_showStencil("r_showStencil", "0", 1,
                     "display the contents of the stencil buffer");
idCVar r_showPrimitives("r_showPrimitives", "0", 2,
                        "report drawsurf/index/vertex counts");
idCVar r_showEdges("r_showEdges", "0", 1, "draw the sil edges");
idCVar r_showNonTwoManifoldEdges(
    "r_showNonTwoManifoldEdges", "0", 1,
    "draw edges that are not shared by exactly two triangles");
idCVar r_showTexelDensity(
    "r_showTexelDensity", "0", 2,
    "shade triangles by texel density.  Set > 0 to scale range by specified amount.  Colors mean: 0: black, 1: purple, 2: blue, 3: green, 4: yellow, 5: orange, 6: red, 7+: white.");
idCVar r_showTexturePolarity("r_showTexturePolarity", "0", 1,
                             "shade triangles by texture area polarity");
idCVar r_showTangentSpace(
    "r_showTangentSpace", "0", 2,
    "shade triangles by tangent space, 1 = use 1st tangent vector, 2 = use 2nd tangent vector, 3 = use normal vector",
    0.0F, 3.0F, idCmdSystem::ArgCompletion_Integer<0, 3>);
idCVar r_showTextureSpace("r_showTextureSpace", "0", 2,
                          "shade triangles by texture space");
idCVar r_showTextureDistortion("r_showTextureDistortion", "0", 2,
                               "shade triangles based on UV stretching");
idCVar r_showTriangleInfo(
    "r_showTriangleInfo", "0", 2,
    "show vertex numbers and tangents of the triangle the crosshair is over",
    0.0F, 2.0F, idCmdSystem::ArgCompletion_Integer<0, 2>);
idCVar r_showTransSortOverdraw(
    "r_showTransSortOverdraw", "0", 2,
    "show overdraw from transparency sorted surfaces, 1:heat pallet, 2; stagering red pallet",
    0.0F, 2.0F, idCmdSystem::ArgCompletion_Integer<0, 2>);
idCVar r_showBlendedSurfOverdraw(
    "r_showBlendedSurfOverdraw", "0", 2,
    "show overdraw from all blended surfaces: 1 = all, 2 = emissive pass, 3 = blended pass, 4 = background sort, 5 = emit sort, 6 = light sort, 7 = decal sort, 8 = trans sort, 9 = water sort",
    0.0F, 9.0F, idCmdSystem::ArgCompletion_Integer<0, 9>);
idCVar r_testVMTR(
    "r_testVMTR", "0", 4,
    " if > 0 displays the vmtr specified by r_currentMaterial centered on the screen");
idCVar r_testVMTRLod("r_testVMTRLod", "0", 2,
                     "which LOD of the vmtr to display");
idCVar r_testAutosprites("r_testAutosprites", "0", 1,
                         "display the autosprited geometry");
idCVar r_debugLineDepthTest("r_debugLineDepthTest", "0", 65537,
                            "perform depth test on debug lines");
idCVar r_debugPolyDepthTest("r_debugPolyDepthTest", "0", 65537,
                            "perform depth test on debug polys");
idCVar r_debugLineWidth("r_debugLineWidth", "1", 65538,
                        "width of debug lines");
idCVar r_debugArrowStep(
    "r_debugArrowStep", "120", 65538,
    "step size of arrow cone line rotation in degrees", 0.0F, 120.0F);
idCVar r_debugPolygonFilled("r_debugPolygonFilled", "1", 1,
                            "draw a filled polygon");
idCVar r_debugDrawAlpha(
    "r_debugDrawAlpha", "0.5", 4,
    "alpha for r_show* debug tools, where implemented and applicable", 0.0F,
    1.0F);
idCVar r_forceFullVirtualTextureLoad(
    "r_forceFullVirtualTextureLoad", "0", 1,
    "force each frame to do a full virtual texture load for capturing video, etc * SLOW *");
idCVar r_skipUpdateInView("r_skipUpdateInView", "0", 1,
                          "skip world update in view call");
idCVar r_flatShadeOnMissingMegas(
    "r_flatShadeOnMissingMegas", "1", 1,
    "draw missing megatextures as flat shaded upon loading map");
idCVar r_flatShadeOnMapLoad("r_flatShadeOnMapLoad", "0", 65537,
                            "set to flat shade on all map loads");
idCVar r_defaultPolyOfsUnits("r_defaultPolyOfsUnits", "-100.0", 4,
                             "default polygonOffset units");
idCVar r_defaultPolyOfsFactor("r_defaultPolyOfsFactor", "-10.0", 4,
                              "default polygonOffset factor");
idCVar r_currentMaterial("r_currentMaterial", "", 0,
                         "current material name from r_showSurfaceInfo");
idCVar r_forceAspectRatio(
    "r_forceAspectRatio", "0", 4,
    "1.777 for 16:9, 1.6 for 16:10, 1.333 for 4:3, 0 to disable");
idCVar r_useFloatBuffers(
    "r_useFloatBuffers", "0", 1,
    "Use float format for render buffers, for higher range");

idCodeResource<idDeclRenderParm> rpCBBlackLevel("cbBlackLevel");

namespace {

constexpr const char* renderModelTypeNames[RENDERMODEL_TYPE_MAX] = {
    "RENDERMODEL_DEFAULT",
    "RENDERMODEL_CLOTH",
    "RENDERMODEL_DETAIL",
    "RENDERMODEL_DETAILEDIT",
    "RENDERMODEL_DISCRETEANIMATION",
    "RENDERMODEL_MANYBOXES",
    "RENDERMODEL_FLARE",
    "RENDERMODEL_FLAREOCCLUSIONQUAD",
    "RENDERMODEL_FOLIAGE",
    "RENDERMODEL_FOLIAGEEDIT",
    "RENDERMODEL_GLASS",
    "RENDERMODEL_EFFECTS",
    "RENDERMODEL_PARTICLE",
    "RENDERMODEL_TREEANIMATOR",
    "RENDERMODEL_STATIC",
    "RENDERMODEL_TRANSPARENCY",
    "RENDERMODEL_WATERSURFACE",
    "RENDERMODEL_ZIPLINE",
    "RENDERMODEL_BEAM",
    "RENDERMODEL_DECAL",
    "RENDERMODEL_GUI",
    "RENDERMODEL_IMPOSTER",
    "RENDERMODEL_TREEIMPOSTER",
    "RENDERMODEL_OCEAN",
    "RENDERMODEL_OCEAN2",
    "RENDERMODEL_LOD",
    "RENDERMODEL_CHAIN",
    "RENDERMODEL_CUTTABLEBEAMS",
    "RENDERMODEL_CUTTABLEDYNAMIC",
    "RENDERMODEL_CUTTABLESURFACE",
    "RENDERMODEL_GODRAYS",
    "RENDERMODEL_DUST",
    "RENDERMODEL_CABLE",
};

} // namespace

bool idRenderSystemLocal::IsEndFrameRenderingInhibited() {
  return inhibitRendering != 0;
}

int idRenderSystemLocal::FrameNumber() { return frameCount; }

bool idRenderSystemLocal::SwapTearAvailable() {
  return glConfig.swapControlTearAvailable;
}

std::int64_t idRenderSystemLocal::GetFreeVRamAtInit() {
  return glConfig.freeVRamAtInit;
}

bool idRenderSystemLocal::CheckOutOfMemory() { return GL_OutOfMemory(); }

void idRenderSystemLocal::ClearOutOfMemory() { GL_ClearOutOfMemory(); }

idRenderSystem::~idRenderSystem() = default;

void idRenderSystemLocal::SetColorMappings(float gamma, float, float) {
  gammaForBrighness = gamma;
  reblendColorCorrection = true;
}

idRenderWorld* idRenderSystemLocal::RenderWorldForIndex(int index) {
  return worlds[index];
}

void idRenderSystemLocal::InhibitEndFrameRendering(int change) {
  inhibitRendering += (std::max)(change, -inhibitRendering);
}

void idRenderSystemLocal::LogMetrics() {
  if (!forge_logPerformanceMetrics.GetBool()) {
    return;
  }
  const wrapperStats_t stats = GL_GetCurrentStats();
  drawsMetric.Log(static_cast<float>(stats.c_drawElements), false);
  trisMetric.Log(static_cast<float>(stats.c_drawIndices / 3), false);
  vertsMetric.Log(static_cast<float>(stats.c_drawVertices), false);
}

void idRenderSystemLocal::FreeRenderWorld(idRenderWorld* world) {
  tr.SyncRenderThread(true);
  virtualTextureSystem.ClearFeedback();
  r_showWorldAreas.SetInteger(0, true);
  if (tr.primaryWorld == world) {
    tr.primaryWorld = nullptr;
  }
  tr.worlds.RemoveFast(world);
}

idRenderWorld* idRenderSystemLocal::AllocRenderWorld(const char* mapName,
                                                      bool comboMap,
                                                      bool toolMap) {
  idRenderWorld* world = idRenderWorld::Alloc();
  std::unique_ptr<idRenderWorld> autoWorld(world);
  if (!world->InitFromMap(mapName, comboMap, toolMap)) {
    return nullptr;
  }
  idMem* memory = idMem::Singleton();
  memory->PushHeap(HEAP_SYSTEMHEAP);
  try {
    worlds.Append(world);
  } catch (...) {
    memory->PopHeap();
    throw;
  }
  memory->PopHeap();
  return autoWorld.release();
}

void idRenderSystemLocal::RegenerateReferences() {
  for (int worldIndex = 0; worldIndex < worlds.Num(); ++worldIndex) {
    idRenderWorld* world = worlds[worldIndex];
    idList<idRenderModel*, 62> models;
    idList<idRenderLight*, 62> lights;

    for (int modelIndex = 0; modelIndex < world->GetNumRenderModels();
         ++modelIndex) {
      idRenderModel* model = world->RenderModelForIndex(modelIndex);
      if (model != nullptr && (model->unlinked & 8U) == 0U) {
        models.Append(model);
      }
    }
    for (int lightIndex = 0; lightIndex < world->GetNumRenderLights();
         ++lightIndex) {
      idRenderLight* light = world->RenderLightForIndex(lightIndex);
      if (light != nullptr && !light->unlinked) {
        lights.Append(light);
      }
    }

    for (int index = 0; index < models.Num(); ++index) {
      idRenderModel* model = models[index];
      if ((model->unlinked & 8U) == 0U) {
        model->CommitThisFrame();
        model->UnlinkSubclass();
        model->unlinked |= 8U;
      }
    }
    for (int index = 0; index < lights.Num(); ++index) {
      lights[index]->Unlink();
    }
    for (int index = 0; index < models.Num(); ++index) {
      models[index]->CommitThisFrame();
    }
    for (int index = 0; index < lights.Num(); ++index) {
      lights[index]->CommitThisFrame();
    }
  }
}

template <>
const char* idRenderStat<int>::GetValueString(bool showDelta,
                                               bool showThousands) {
  int sum = 0;
  for (const int value : values) {
    sum += value;
  }
  const int average = (sum + 8) / 16;
  valueString[0] = '\0';
  idStr::Append(valueString, sizeof(valueString),
                average > maxValue ? "^1" : "^2");
  int used = idStr::Length(valueString);
  if (showThousands) {
    idStr::snPrintf(valueString + used,
                    static_cast<int>(sizeof(valueString)) - used, "%4dk",
                    average / 1000);
  } else {
    idStr::snPrintf(valueString + used,
                    static_cast<int>(sizeof(valueString)) - used, "%5d",
                    average);
  }
  if (showDelta) {
    const int delta = average - deltaFromValue;
    idStr::Append(valueString, sizeof(valueString), "^3 (");
    used = idStr::Length(valueString);
    if (showThousands) {
      idStr::snPrintf(valueString + used,
                      static_cast<int>(sizeof(valueString)) - used, "%4dk",
                      delta / 1000);
    } else {
      idStr::snPrintf(valueString + used,
                      static_cast<int>(sizeof(valueString)) - used, "%5d",
                      delta);
    }
    idStr::Append(valueString, sizeof(valueString), ")");
  }
  idStr::Append(valueString, sizeof(valueString), "^7 / ");
  used = idStr::Length(valueString);
  if (showThousands) {
    idStr::snPrintf(valueString + used,
                    static_cast<int>(sizeof(valueString)) - used, "%4dk",
                    maxValue / 1000);
  } else {
    idStr::snPrintf(valueString + used,
                    static_cast<int>(sizeof(valueString)) - used, "%5d",
                    maxValue);
  }
  return valueString;
}

template <>
const char* idRenderStat<float>::GetValueString(bool showDelta, bool) {
  float sum = 0.0F;
  for (const float value : values) {
    sum += value;
  }
  const float average = sum * (1.0F / 16.0F);
  const float shownAverage = std::fabs(average) < 0.1F ? 0.0F : average;
  valueString[0] = '\0';
  idStr::Append(valueString, sizeof(valueString),
                average > maxValue ? "^1" : "^2");
  int used = idStr::Length(valueString);
  idStr::snPrintf(valueString + used,
                  static_cast<int>(sizeof(valueString)) - used, "%5.1f",
                  shownAverage);
  if (showDelta) {
    float delta = average - deltaFromValue;
    if (std::fabs(delta) < 0.1F) {
      delta = 0.0F;
    }
    idStr::Append(valueString, sizeof(valueString), "^3 (");
    used = idStr::Length(valueString);
    idStr::snPrintf(valueString + used,
                    static_cast<int>(sizeof(valueString)) - used, "%5.1f",
                    delta);
    idStr::Append(valueString, sizeof(valueString), ")");
  }
  float shownMax = std::fabs(maxValue) < 0.1F ? 0.0F : maxValue;
  idStr::Append(valueString, sizeof(valueString), "^7 / ");
  used = idStr::Length(valueString);
  idStr::snPrintf(valueString + used,
                  static_cast<int>(sizeof(valueString)) - used, "%5.1f",
                  shownMax);
  return valueString;
}

idRenderSystemLocal::idRenderSystemLocal()
    : drawsMetric(idStr("draws"), idStr("draw calls in a frame")),
      trisMetric(idStr("triangles"), idStr("drawn triangles in the scene")),
      vertsMetric(idStr("vertices"), idStr("vertices count in the scene")) {
  blockColor = {-1.0F, -1.0F, -1.0F, -1.0F};
  renderFlags = 0;
  insideEndFrame = false;
  automaticBackgroundSwapsEnabled = false;
  renderingIsReadyForSwapbuffers = false;
  allowBackgroundSwaps = true;
  renderSynced = true;
  frameCount = 0;
  inhibitRendering = 0;
  primaryWorld = nullptr;
  testImage = nullptr;
  testCubeImage = nullptr;
  testMaterial = nullptr;
  videoPlaying = false;
  videoOverlay = nullptr;
  videoOverlayModel = nullptr;
  performanceCounterLogWorld = nullptr;
  performanceCounterLogWorldFrame = 0;
  std::memset(&pc, 0, sizeof(pc));
  GL_ClearStats();

  mtrDefault = nullptr;
  mtrDefaultPointLight = nullptr;
  mtrDefaultProjectedLight = nullptr;
  mtrDefaultParallelLight = nullptr;
  mtrDefaultPointBlendLight = nullptr;
  mtrDefaultProjectedBlendLight = nullptr;
  mtrDefaultParallelBlendLight = nullptr;
  mtrDefaultPointSlowLight = nullptr;
  mtrDefaultProjectedSlowLight = nullptr;
  mtrDefaultParallelSlowLight = nullptr;
  progGauss = nullptr;
  progGlareScale = nullptr;
  progOverlappedDownSample = nullptr;
  progFeedbackDownSample = nullptr;
  progPostProcess = nullptr;
  progDepthOnly = nullptr;
  progColorOnly = nullptr;
  progColorParmOnly = nullptr;
  progWireFrame = nullptr;
  progTextureOnly = nullptr;
  progCubeImageOnly = nullptr;
  progTestImage = nullptr;
  progBasicBlend = nullptr;
  progEnvironmentBlend = nullptr;
  progColorCorrectionBlend = nullptr;
  progOccluderUnique = nullptr;
  progLandUniqueBump = nullptr;
  progLandUniqueDiffuseOnly = nullptr;
  progBlendedLightDebugDraw = nullptr;
  progVarianceShadowMap = nullptr;
  progDimShadowDrawMidQuality = nullptr;
  progCreateGodRayMask = nullptr;
  progDrawGodRay = nullptr;
  rpGlareMap = nullptr;
  rpGlareStep = nullptr;
  rpDistortionMap = nullptr;
  rpDynamicEnvMap = nullptr;
  rpPowerMip = nullptr;
  rpFrameNumber = nullptr;
  rpVirtualTextureFeedbackFloat = nullptr;
  rpPageTableMap = nullptr;
  rpVmtrPageTableMap = nullptr;
  rpDepthOfField2 = nullptr;
  rpGodRayQuadScaleBias = nullptr;
  rpGodRaySourceScaleBias = nullptr;
  rpGodRaysColor = nullptr;
  rpGodRayViewPos = nullptr;
  progBloomThreshold = nullptr;
  progLuminosity = nullptr;
  rpAdaptiveGlareMap = nullptr;
  rpAdaptiveGlareMapAlt = nullptr;
  rpLuminosityMap = nullptr;
  rpLuminosityMapAlt = nullptr;
  rpAdaptiveLuminance = nullptr;
  rpAdaptiveGlareBlurMap = nullptr;
  rpAdaptiveGlareBlurStep = nullptr;
  rpAdaptiveGlareParms = nullptr;
  rpAdaptiveGlareColor = nullptr;
  rpFrameExposureMap = nullptr;
  rpFrameExposureParms1 = nullptr;
  rpFrameExposureParms2 = nullptr;
  declHighlightTable = nullptr;
  declHighlightExplodeTable = nullptr;
  defaultCubeModel = nullptr;
  unitCubeModel = nullptr;
  zeroOneCubeModel = nullptr;
  extrudeBoxModel = nullptr;
  unitSquareModel = nullptr;
  zeroOneSquareModel = nullptr;
  unitSquareTris = nullptr;

  for (int index = 0; index < 2; ++index) {
    imgViewColor[index] = nullptr;
    imgColorGradingLUT[index] = nullptr;
    imgDistortion[index] = nullptr;
    imgDynamicEnvironment[index] = nullptr;
    imgDynamicColorCorrection[index] = nullptr;
    renderDestViewColor[index] = nullptr;
    renderDestDistortion[index] = nullptr;
    renderDestDynamicEnv[index] = nullptr;
    renderDestDynamicColorCorrection[index] = nullptr;
  }
  imgScreenMip3 = nullptr;
  imgScreenMip3Alt = nullptr;
  imgGui = nullptr;
  renderDestViewDepth = nullptr;
  renderDestMip1 = nullptr;
  renderDestMip3 = nullptr;
  renderDestMip3Alt = nullptr;
  renderDestGui = nullptr;
  renderDestShadow = nullptr;
  imgLuminosity = nullptr;
  imgLuminosityAlt = nullptr;
  imgBloom = nullptr;
  renderDestLuminosity = nullptr;
  renderDestLuminosityAlt = nullptr;
  renderDestBloom = nullptr;
  std::memset(gammaTable, 0, sizeof(gammaTable));
  blackLevel[0] = 0.0F;
  blackLevel[1] = 0.0F;
  updateGammaTable = false;
  reblendColorCorrection = false;
  gammaForBrighness = 1.0F;
  blockColorFrameNum = 0;
}

idRenderSystemLocal::~idRenderSystemLocal() = default;

void idRenderSystemLocal::SetRenderFlag(renderFlags_t flag) {
  renderFlags |= flag;
}

void idRenderSystemLocal::RemoveRenderFlag(int flags) {
  renderFlags &= ~flags;
}

bool idRenderSystemLocal::HasRenderFlag(int renderFlag) {
  return (renderFlags & renderFlag) != 0;
}

bool idRenderSystemLocal::IsAutomaticBackgroundSwapsEnabled() {
  return automaticBackgroundSwapsEnabled;
}

void idRenderSystemLocal::SetBlackLevel(float blackLevelValue) {
  blackLevel[0] = blackLevelValue;
}

float idRenderSystemLocal::GetScreenAspect() {
  return (GetPixelAspect() * static_cast<float>(GetWidth())) /
         static_cast<float>(GetHeight());
}

void idRenderSystemLocal::PrintStatistics() {
  if (r_showPrimitives.GetInteger() != 0) {
    const wrapperStats_t stats = GL_GetCurrentStats();
    idLib::Printf("draws:%i tris:%i verts:%i\n", stats.c_drawElements,
                  stats.c_drawIndices / 3, stats.c_drawVertices);
  }

  if (r_showQueries.GetInteger() != 0) {
    const wrapperStats_t stats = GL_GetCurrentStats();
    idLib::Printf("%d issued, %d passed, %d too old, %1.2f spin ms\n",
                  stats.c_queriesIssued, stats.c_queriesPassed,
                  stats.c_queriesTooOld,
                  static_cast<float>(stats.c_queriesWaitTime) * 0.001F);
  }

  if (r_showCommits.GetInteger() != 0) {
    idLib::Printf(
        "modelCommits:%i  modelRefs:%i  lightCommits:%i  lightRefs:%i\n",
        pc.c_modelCommits, pc.c_modelReferences, pc.c_lightCommits,
        pc.c_lightReferences);
  }

  if (r_showTotalSurfaceArea.GetInteger() != 0) {
    float totalSurfaceArea = 0.0F;
    for (int modelIndex = 0;
         modelIndex < primaryWorld->GetNumRenderModels(); ++modelIndex) {
      idRenderModel* model = primaryWorld->RenderModelForIndex(modelIndex);
      if (model == nullptr ||
          (r_showTotalSurfaceArea.GetInteger() != 2 &&
           (model->g.flags0 & 0x04U) == 0)) {
        continue;
      }

      for (int surfaceIndex = 0; surfaceIndex < model->surfaces.Num();
           ++surfaceIndex) {
        const idRenderModelSurface& surface = model->surfaces[surfaceIndex];
        if (surface.material == nullptr ||
            (surface.material->Coverage() != MC_INVISIBLE &&
             !surface.material->IsVMTRVirtualTexture()) ||
            surface.geometry == nullptr || surface.geometry->verts == nullptr ||
            surface.geometry->indexes == nullptr) {
          continue;
        }
        const float surfaceArea = surface.geometry->GetSurfaceArea();
        idLib::Printf("%.0f %s\n", surfaceArea,
                      surface.material->GetName());
        totalSurfaceArea += surfaceArea;
      }
    }
    idLib::Printf("Total Surface Area: %.0f million square units\n",
                  totalSurfaceArea / 1000000.0F);
    r_showTotalSurfaceArea.SetBool(false, true);
  }

  if (r_showRenderModelCommits.GetInteger() != 0) {
    static idOverlayHandle modelCommitOverlay{-1, 0};
    idStr text;
    for (int typeIndex = 0; typeIndex < RENDERMODEL_TYPE_MAX; ++typeIndex) {
      text += va("%s%4d%s commits of type %s%30s\n", "^3",
                 pc.c_modelTypeCommits[typeIndex], "^7", "^3",
                 renderModelTypeNames[typeIndex]);
    }
    console->PrintOverlay(&modelCommitOverlay, 1, 1.0F, 1.0F, text.data);
  }

  if (r_showStats.GetInteger() != 0) {
    const bool freeze = r_showStats.GetInteger() < 0;
    const bool showDelta = r_showDeltaStats.GetInteger() != 0;
    const bool setDelta =
        (r_showDeltaStats.flags & CVAR_MODIFIED) != 0 && showDelta;
    if ((r_showDeltaStats.flags & CVAR_MODIFIED) != 0) {
      r_showDeltaStats.flags &= ~CVAR_MODIFIED;
    }

    const wrapperStats_t& drawStats =
        showDelta ? renderThread->wrapperStatsExcludingFullScreenGuis
                  : renderThread->wrapperStatsFull;

    if (std::abs(r_showStats.GetInteger()) >= 2 && tr.primaryWorld != nullptr) {
      idRenderWorldCommitted* committedWorld =
          tr.primaryWorld->GetCommittedWorld();
      const int committedModels = committedWorld->GetNumCommittedModels();
      const int committedLights = committedWorld->GetNumCommittedLights();
      const int addAlwaysModels = committedWorld->GetNumAddAlwaysModels();

      static idRenderStat<int> numModelCommits(32);
      static idRenderStat<int> numLightCommits(32);
      static idRenderStat<int> numModelsTotal(8192);
      static idRenderStat<int> numLightsTotal(8192);
      static idRenderStat<int> numApproxLightModels(64);
      static idRenderStat<int> numAddAlwaysModels(10);
      static idRenderStat<int> numTransSortQuads(10240);
      static idRenderStat<int> numTransAddQuads(10240);
      static idOverlayHandle commitOverlay{-1, 0};

      numModelCommits.Update(pc.c_modelCommits, freeze, setDelta);
      numLightCommits.Update(pc.c_lightCommits, freeze, setDelta);
      numModelsTotal.Update(committedModels, freeze, setDelta);
      numLightsTotal.Update(committedLights, freeze, setDelta);
      numApproxLightModels.Update(pc.c_approxLightModelsWant, freeze,
                                  setDelta);
      numAddAlwaysModels.Update(addAlwaysModels, freeze, setDelta);

      console->PrintOverlay(
          &commitOverlay, 1, 1.0F, 1.0F,
          "--------- COMMIT ---------\n"
          "%s model commits      \n"
          "%s light commits      \n"
          "%s models total       \n"
          "%s lights total       \n"
          "%s approx-light models\n"
          "%s add-always models  \n"
          "%s trans-sort quads   \n"
          "%s trans-add quads    \n",
          numModelCommits.GetValueString(showDelta, false),
          numLightCommits.GetValueString(showDelta, false),
          numModelsTotal.GetValueString(showDelta, false),
          numLightsTotal.GetValueString(showDelta, false),
          numApproxLightModels.GetValueString(showDelta, false),
          numAddAlwaysModels.GetValueString(showDelta, false),
          numTransSortQuads.GetValueString(showDelta, true),
          numTransAddQuads.GetValueString(showDelta, true));
    }

    static idRenderStat<int> numWorldAreas(500);
    static idRenderStat<int> numModels(100);
    static idRenderStat<int> numLights(12);
    static idRenderStat<int> numShadows(1);
    static idRenderStat<int> numDimShadows(16);
    static idRenderStat<int> numSelfShadows(2);
    static idRenderStat<int> numSurfaces(1000);
    static idRenderStat<int> numTriangles(350000);
    static idRenderStat<int> numVertices(350000);
    static idOverlayHandle viewOverlay{-1, 0};

    numWorldAreas.Update(pc.c_viewWorldAreas, freeze, setDelta);
    numModels.Update(pc.c_viewModels, freeze, setDelta);
    numLights.Update(pc.c_viewLights, freeze, setDelta);
    numShadows.Update(pc.c_viewShadows, freeze, setDelta);
    numDimShadows.Update(pc.c_viewDimShadows, freeze, setDelta);
    numSelfShadows.Update(pc.c_viewSelfShadows, freeze, setDelta);
    numSurfaces.Update(drawStats.c_drawElements, freeze, setDelta);
    numTriangles.Update(drawStats.c_drawIndices / 3, freeze, setDelta);
    numVertices.Update(drawStats.c_drawVertices, freeze, setDelta);

    console->PrintOverlay(
        &viewOverlay, 1, 1.0F, 1.0F,
        "---------- VIEW ----------\n"
        "%s world chunks\n"
        "%s models      \n"
        "%s lights      \n"
        "%s shadows     \n"
        "%s dim-shadows \n"
        "%s self-shadows\n"
        "%s surfaces    \n"
        "%s triangles   \n"
        "%s vertices    \n",
        numWorldAreas.GetValueString(showDelta, false),
        numModels.GetValueString(showDelta, false),
        numLights.GetValueString(showDelta, false),
        numShadows.GetValueString(showDelta, false),
        numDimShadows.GetValueString(showDelta, false),
        numSelfShadows.GetValueString(showDelta, false),
        numSurfaces.GetValueString(showDelta, false),
        numTriangles.GetValueString(showDelta, true),
        numVertices.GetValueString(showDelta, true));

    // GPU pass timing was compiled out of the shipping-retail renderer. The
    // authority initializes and presents the retained stat slots with zeros.
    static idRenderStat<float> gpuPrezPass(4.0F);
    static idRenderStat<float> gpuDepthPass(4.0F);
    static idRenderStat<float> gpuBasePass(8.0F);
    static idRenderStat<float> gpuLights(1.0F);
    static idRenderStat<float> gpuDimShadows(1.0F);
    static idRenderStat<float> gpuSelfShadows(1.0F);
    static idRenderStat<float> gpuEffects(3.5F);
    static idRenderStat<float> gpuPostProcess(1.5F);
    static idRenderStat<float> gpuGuis(0.5F);
    static idRenderStat<float> gpuMisc(1.0F);
    static idRenderStat<float> gpuTotal(16.0F);
    static idOverlayHandle gpuOverlay{-1, 0};

    gpuPrezPass.Update(0.0F, freeze, setDelta);
    gpuDepthPass.Update(0.0F, freeze, setDelta);
    gpuBasePass.Update(0.0F, freeze, setDelta);
    gpuLights.Update(0.0F, freeze, setDelta);
    gpuDimShadows.Update(0.0F, freeze, setDelta);
    gpuSelfShadows.Update(0.0F, freeze, setDelta);
    gpuEffects.Update(0.0F, freeze, setDelta);
    gpuPostProcess.Update(0.0F, freeze, setDelta);
    gpuGuis.Update(0.0F, freeze, setDelta);
    gpuMisc.Update(0.0F, freeze, setDelta);
    gpuTotal.Update(0.0F, freeze, setDelta);

    console->PrintOverlay(
        &gpuOverlay, 1, 1.0F, 1.0F,
        "----------- GPU ----------\n"
        "%s ms (prez)      \n"
        "%s ms (opaque)    \n"
        "%s ms base pass   \n"
        "%s ms lights      \n"
        "%s ms dim-shadows \n"
        "%s ms self-shadows\n"
        "%s ms effects     \n"
        "%s ms post-process\n"
        "%s ms guis        \n"
        "%s ms misc        \n"
        "%s ms total       \n",
        gpuPrezPass.GetValueString(showDelta, false),
        gpuDepthPass.GetValueString(showDelta, false),
        gpuBasePass.GetValueString(showDelta, false),
        gpuLights.GetValueString(showDelta, false),
        gpuDimShadows.GetValueString(showDelta, false),
        gpuSelfShadows.GetValueString(showDelta, false),
        gpuEffects.GetValueString(showDelta, false),
        gpuPostProcess.GetValueString(showDelta, false),
        gpuGuis.GetValueString(showDelta, false),
        gpuMisc.GetValueString(showDelta, false),
        gpuTotal.GetValueString(showDelta, false));
  }

  if (r_showRenderLog.GetInteger() != 0) {
    static float gpuTimes[52]{};
    static float cpuTimes[52]{};
    static idOverlayHandle renderLogOverlay{-1, 0};
    if (r_showRenderLog.GetInteger() == 1) {
      std::memset(&gpuTimes[1], 0, 51 * sizeof(gpuTimes[0]));
      std::memset(&cpuTimes[1], 0, 51 * sizeof(cpuTimes[0]));
    }

    idStr text;
    text += va("%40s CPU  GPU\n", "label");
    for (int index = 0; index < 51; ++index) {
      text += va("%-40s %4.1f %4.1f\n", "", cpuTimes[index + 1],
                 gpuTimes[index + 1]);
    }
    console->PrintOverlay(&renderLogOverlay, 0, 1.0F, 1.0F, text.data);
  }
}

idScreenViewCommitted::idScreenViewCommitted() {
  screenRect.Clear();
}

idScreenViewCommitted::~idScreenViewCommitted() = default;

idScreenViewCommitted& idScreenViewCommitted::operator=(
    const idScreenViewCommitted& other) {
  screenRect = other.screenRect;
  usePreviousRendering = other.usePreviousRendering;
  world = other.world;
  renderView = other.renderView;
  guiOriginOffset = other.guiOriginOffset;
  committedGuis = other.committedGuis;
  for (int index = 0; index < 16; ++index) {
    committedGuis.staticList[index] = other.committedGuis.staticList[index];
  }
  return *this;
}

void idRenderSystemLocal::ToolEndFrame(void* hdc, int windowWidth,
                                       int windowHeight, idRenderWorld* world,
                                       const renderView_t& renderView,
                                       idRenderModel* gui, int timeMsec) {
  world->SetNumRenderViews(1);
  idRenderView* worldView = world->RenderViewForIndex(0);
  worldView->g = renderView;
  idRenderModel* guis[1] = {gui};
  EndFrame(hdc, windowWidth, windowHeight, world, guis,
           gui != nullptr ? 1 : 0, false, true, true, timeMsec);
}

template <typename T>
T* idListArrayNew(int num, unsigned tag, bool zeroBuffer) {
  idMem* memory = idMem::Singleton();
  void* allocation = memory->AllocWithLocation(
      zeroBuffer ? "w:\\build\\tech5\\shared\\idlib\\containers/List.h(27) : tag"
                 : "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag",
      sizeof(T) * static_cast<std::size_t>(num), static_cast<memTag_t>(tag),
      zeroBuffer, ALIGN_16, HEAP_DEFAULTHEAP);
  if (allocation == nullptr) {
    return nullptr;
  }

  T* result = static_cast<T*>(allocation);
  int constructed = 0;
  try {
    for (; constructed < num; ++constructed) {
      new (&result[constructed]) T();
    }
  } catch (...) {
    while (constructed > 0) {
      result[--constructed].~T();
    }
    memory->Free(result, ALIGN_16);
    throw;
  }
  return result;
}

template <typename T> void idListArrayDelete(T* pointer, int num) {
  if (pointer != nullptr) {
    for (int index = 0; index < num; ++index) {
      pointer[index].~T();
    }
  }
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

template <typename T>
T* idListArrayResize(T* oldPointer, int oldNum, int newNum, unsigned tag,
                     bool zeroBuffer) {
  T* replacement = nullptr;
  if (newNum > 0) {
    replacement = idListArrayNew<T>(newNum, tag, zeroBuffer);
    if (replacement == nullptr) {
      return nullptr;
    }
    const int copyCount = (std::min)(oldNum, newNum);
    for (int index = 0; index < copyCount; ++index) {
      replacement[index] = oldPointer[index];
    }
  }
  idListArrayDelete(oldPointer, oldNum);
  return replacement;
}

template <>
bool idList<idScreenViewCommitted, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    if (listStatic == 0 || listStatic == 2) {
      if (list != nullptr) {
        idListArrayDelete(list, size);
      }
      list = nullptr;
      size = 0;
    }
    num = 0;
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  list = idListArrayResize(list, size, newSize, memTag, false);
  if (list == nullptr) {
    return false;
  }
  size = newSize;
  num = (std::min)(num, newSize);
  return true;
}

template idScreenViewCommitted*
idListArrayNew<idScreenViewCommitted>(int, unsigned, bool);
template void idListArrayDelete<idScreenViewCommitted>(
    idScreenViewCommitted*, int);
template idScreenViewCommitted* idListArrayResize<idScreenViewCommitted>(
    idScreenViewCommitted*, int, int, unsigned, bool);

void idRenderFrameInfo::Commit(idRenderFrameInfoCommitted& committed) const {
  if (!idRenderModelGui::IsInitialized()) {
    idRenderModelGui::Init();
    idRenderModelGui::StartFrame();
  }

  if (!r_skipUpdateInView.GetBool()) {
    if (worldViews.Num() == 1) {
      const idScreenView& source = worldViews[0];
      idRenderView* view = source.world->RenderViewForIndex(source.viewIndex);
      view->g = source.g;
      const int width = source.screenRect.x2 - source.screenRect.x1 + 1;
      const int height = source.screenRect.y2 - source.screenRect.y1 + 1;
      view->CommitParameters(width, height, width, height);
      source.world->UpdateInView(view, view);
      source.world->Commit(false);
    } else if (worldViews.Num() == 2) {
      const idScreenView& firstSource = worldViews[0];
      const idScreenView& secondSource = worldViews[1];
      idRenderView* first =
          firstSource.world->RenderViewForIndex(firstSource.viewIndex);
      idRenderView* second =
          secondSource.world->RenderViewForIndex(secondSource.viewIndex);
      first->g = firstSource.g;
      second->g = secondSource.g;
      const int firstWidth =
          firstSource.screenRect.x2 - firstSource.screenRect.x1 + 1;
      const int firstHeight =
          firstSource.screenRect.y2 - firstSource.screenRect.y1 + 1;
      const int secondWidth =
          secondSource.screenRect.x2 - secondSource.screenRect.x1 + 1;
      const int secondHeight =
          secondSource.screenRect.y2 - secondSource.screenRect.y1 + 1;
      first->CommitParameters(firstWidth, firstHeight, firstWidth, firstHeight);
      second->CommitParameters(secondWidth, secondHeight, secondWidth,
                               secondHeight);
      idRenderView* updateFirst = first;
      idRenderView* updateSecond = second;
      if (firstSource.usePreviousRendering) {
        std::swap(updateFirst, updateSecond);
      }
      firstSource.world->UpdateInView(updateFirst, updateSecond);
      firstSource.world->Commit(false);
    }
  }

  committed.renderTime = 0;
  committed.captureFrame = captureFrame;
  committed.committedViews.SetNum(screenViews.Num());

  idStaticList<idRenderModel*, 48> allGuis;
  for (int viewIndex = 0; viewIndex < screenViews.Num(); ++viewIndex) {
    const idScreenView& source = screenViews[viewIndex];
    idScreenViewCommitted& destination = committed.committedViews[viewIndex];
    destination.screenRect = source.screenRect;
    destination.usePreviousRendering = source.usePreviousRendering;
    destination.world = nullptr;
    destination.renderView = nullptr;
    destination.guiOriginOffset = source.guiOriginOffset;
    destination.committedGuis.ClearFree();

    if (source.world != nullptr && !source.usePreviousRendering) {
      destination.world = source.world->GetCommittedWorld();
      destination.renderView =
          source.world->RenderViewForIndex(source.viewIndex);
      destination.renderView->g = source.g;
      const int width = source.screenRect.x2 - source.screenRect.x1 + 1;
      const int height = source.screenRect.y2 - source.screenRect.y1 + 1;
      destination.renderView->CommitParameters(width, height, width, height);
      committed.renderTime = source.g.renderTime;
    }

    for (int guiIndex = 0; guiIndex < source.viewGuis.Num(); ++guiIndex) {
      idRenderModel* gui = source.viewGuis[guiIndex];
      if (gui == nullptr) {
        continue;
      }
      destination.committedGuis.Append(gui->committed);
      bool alreadyAdded = false;
      for (int existing = 0; existing < allGuis.Num(); ++existing) {
        if (allGuis[existing] == gui) {
          alreadyAdded = true;
          break;
        }
      }
      if (!alreadyAdded) {
        allGuis.Append(gui);
      }
    }
  }

  for (int index = 0; index < allGuis.Num(); ++index) {
    allGuis[index]->UpdateInView(nullptr, nullptr, nullptr);
    allGuis[index]->Commit();
  }
  idRenderModelGui::StartFrame();
}

void idRenderSystemLocal::RenderFrame(
    void* const hdc, const idRenderFrameInfo& renderFrameInfo, bool allowSMP,
    bool skipSwapBuffers, bool resolveDepth) {
  if (insideEndFrame) {
    return;
  }
  insideEndFrame = true;

  if (renderFrameInfo.screenViews.Num() > 0 &&
      renderFrameInfo.screenViews[0].world != nullptr) {
    tr.primaryWorld = renderFrameInfo.screenViews[0].world;
  }

  SyncRenderThread(false);
  PrintStatistics();
  LogMetrics();
  virtualTextureSystem.LogMetrics();
  resolutionScale.LogMetrics();
  std::memset(&pc, 0, sizeof(pc));
  GL_ClearStats();
  ++frameCount;
  globalImages->CheckCvars();

  if (blackLevel[0] != blackLevel[1]) {
    blackLevel[1] = blackLevel[0];
    updateGammaTable = true;
    renderThreadParmState->SetParmFloat(rpCBBlackLevel.Get(), blackLevel[0]);
  }

  pc.startLatchTimeMicroSec = Sys_Microseconds();
  renderFrameInfo.Commit(renderThread->committedRenderFrameInfo);
  pc.endLatchTimeMicroSec = Sys_Microseconds();
  UnbindBufferObjects();
  globalImages->UnbindAll();

  if (inhibitRendering != 0 || idLib::headless) {
    UnbindBufferObjects();
  } else if (renderSystem->GetUsingSMP()) {
    GL_Flush();
    RestartRenderThread(hdc);
    if (!allowSMP) {
      SyncRenderThread(false);
    }
  } else {
    MakeCurrent(hdc);
    renderThread->RenderFrameInternal(renderThread->renderMode, hdc,
                                      skipSwapBuffers, resolveDepth);
  }
  insideEndFrame = false;
}

void idRenderSystemLocal::EndFrame(
    void* hdc, int windowWidth, int windowHeight, idRenderWorld* world,
    idRenderModel** guis, int numGuis, bool allowSMP, bool skipSwapBuffers,
    bool resolveDepth, int timeMsec) {
  idRenderFrameInfo frameInfo;
  idScreenView* screenView = frameInfo.screenViews.Alloc();
  if (world != nullptr) {
    screenView->g = world->RenderViewForIndex(0)->g;
    screenView->g.renderTime = timeMsec;
  }
  screenView->screenRect.x1 = 0;
  screenView->screenRect.y1 = 0;
  screenView->screenRect.x2 = windowWidth - 1;
  screenView->screenRect.y2 = windowHeight - 1;
  screenView->world = world;

  if (world != nullptr) {
    idScreenView* worldView = frameInfo.worldViews.Alloc();
    *worldView = *screenView;
  }
  for (int index = 0; index < numGuis; ++index) {
    screenView->viewGuis.Append(guis[index]);
  }
  RenderFrame(hdc, frameInfo, allowSMP, skipSwapBuffers, resolveDepth);
}

template void idCmdSystem::ArgCompletion_Integer<-4, 4>(idAutoComplete*);
template void idCmdSystem::ArgCompletion_Integer<0, 9>(idAutoComplete*);
