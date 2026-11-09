#include "engine/renderer/renderlocal.h"
#include "engine/renderer/rendersystem_init.h"

#include "engine/decls/decltable.h"
#include "engine/framework/common.h"
#include "engine/models/rendermodelgui.h"
#include "engine/models/static/staticmodel.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/ingamevideo.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/renderthread.h"
#include "engine/renderer/texturesampler.h"
#include "engine/renderer/triangles.h"
#include "engine/renderer/virtualtexture.h"
#include "engine/sys/sys_public.h"
#include "shared/idlib/containers/heaparray.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/text/str.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <type_traits>

extern idCVar r_sb_imageSize;
extern idCVar r_useFloatBuffers;
extern idCVar r_gamma;
extern idCVar r_brightness;

idCVar r_mip3Border("r_mip3Border", "1", 65537,
                    "use clampToBorder for the mip3 buffers");

__forceinline void* operator new(std::size_t size, memTag_t tag) noexcept {
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size, tag,
      false, ALIGN_16, HEAP_DEFAULTHEAP);
}

__forceinline void operator delete(void* pointer, memTag_t) noexcept {
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

namespace {

#if defined(WOLF_RENDERER_RENDERSYSTEM_INIT_TEST_ACCESS)
RenderSystemInitTestHooks testHooks{};
#endif

template <typename T> __forceinline T* AllocateRendererObject() {
  return new (TAG_RENDERER) T();
}

__forceinline idRenderDestination*
DestinationForImages(idImage* color, idImage* depth,
                     idImage* stencil = nullptr) {
  idRenderDestination* const destination =
      AllocateRendererObject<idRenderDestination>();
  if (destination != nullptr)
    destination->CreateFromImages(color, depth, stencil);
  return destination;
}

__forceinline std::uint8_t EncodeDirection(float value) {
  const int encoded = static_cast<int>((value + 1.0F) * 127.5F + 0.5F);
  return static_cast<std::uint8_t>(std::clamp(encoded, 0, 255));
}

__forceinline void SetSquareGeometry(idTriangles* triangles, bool guiSquare) {
  triangles->numVerts = 4;
  triangles->numIndexes = 6;
  idTriangles::AllocStaticTriSurfVerts(triangles, triangles->numVerts);
  idTriangles::AllocStaticTriSurfIndexes(triangles, triangles->numIndexes);

  const std::uint16_t indexes[6]{3, 0, 2, 2, 0, 1};
  std::memcpy(triangles->indexes, indexes, sizeof(indexes));
  idTriangles::UpdateIndexBuffer(triangles);

  const idVec3 positions[4]{{-1.0F, -1.0F, 0.0F},
                            {1.0F, -1.0F, 0.0F},
                            {1.0F, 1.0F, 0.0F},
                            {-1.0F, 1.0F, 0.0F}};
  const idVec2 texCoords[4]{{0.0F, 0.0F}, {1.0F, 0.0F},
                            {1.0F, 1.0F}, {0.0F, 1.0F}};
  const idVec3 normal = guiSquare ? idVec3{0.0F, 0.0F, 0.0F}
                                  : idVec3{0.0F, 0.0F, 1.0F};
  const idVec3 tangent{1.0F, 0.0F, 0.0F};
  const std::uint32_t guiTangent =
      idColor::PackColor({0.5F, 0.5F, 0.5F, 0.5F});
  for (int index = 0; index < 4; ++index) {
    idDrawVert& vertex = triangles->verts[index];
    vertex.xyz = positions[index];
    vertex.st = texCoords[index];
    vertex.normal[0] = EncodeDirection(normal.x);
    vertex.normal[1] = EncodeDirection(normal.y);
    vertex.normal[2] = EncodeDirection(normal.z);
    if (guiSquare) {
      std::memcpy(vertex.tangent, &guiTangent, sizeof(guiTangent));
    } else {
      vertex.tangent[0] = EncodeDirection(tangent.x);
      vertex.tangent[1] = EncodeDirection(tangent.y);
      vertex.tangent[2] = EncodeDirection(tangent.z);
    }
    std::memset(vertex.color, 0xFF, sizeof(vertex.color));
  }

  triangles->bounds[0] = {-1.0F, -1.0F, 0.0F};
  triangles->bounds[1] = {1.0F, 1.0F, 0.0F};
  idTriangles::UpdateVertexBuffer(triangles);
}

struct DefaultRenderParm {
  const char* name;
  float value;
};

constexpr DefaultRenderParm defaultRenderParms[]{
    {"contents_NonSolid", 0.0F},
    {"contents_Solid", 1.0F},
    {"contents_Opaque", 2.0F},
    {"contents_Water", 4.0F},
    {"contents_PlayerClip", 8.0F},
    {"contents_MonsterClip", 16.0F},
    {"contents_MoveableClip", 64.0F},
    {"contents_ShotClip", 128.0F},
    {"contents_IKClip", 256.0F},
    {"contents_AIAware", 512.0F},
    {"contents_AI", 1024.0F},
    {"contents_Projectile", 2048.0F},
    {"contents_Corpse", 4096.0F},
    {"contents_Trigger", 16384.0F},
    {"contents_Obstacle", 131072.0F},
    {"contents_Player", 32768.0F},
    {"contents_PlayerCoverClip", 524288.0F},
    {"contents_MonsterCoverClip", 1048576.0F},
    {"contents_Breakable", 8192.0F},
    {"contents_NoCover", 1073741824.0F},
    {"contents_PlayerFocus", 2097152.0F},
    {"contents_Shield", 8388608.0F},
    {"contents_AASSolid", 67108864.0F},
    {"contents_AASObstacle", 134217728.0F},
    {"contents_AASClusterPortal", 268435456.0F},
    {"contents_AASWalkableWall", 536870912.0F},
    {"contents_AASFly", 33554432.0F},
    {"toolsflag_NoCSG", 1.0F},
    {"toolsflag_SoundDoor", 8.0F},
    {"toolsflag_SoundClip", 2.0F},
    {"toolsflag_SoundClipTrans", 4.0F},
    {"toolsflag_SoundPortal", 16.0F},
    {"toolsflag_PVSClip", 32.0F},
    {"toolsflag_ManifoldHint", 128.0F},
    {"surf_NoDamage", 16.0F},
    {"surf_Slick", 32.0F},
    {"surf_Collision", 64.0F},
    {"surf_Ladder", 128.0F},
    {"surf_NoImpact", 256.0F},
    {"surf_NoSteps", 512.0F},
    {"surf_Stairs", 1024.0F},
    {"surf_OcclusionTest", 2048.0F},
    {"surf_ShadowCaster", 4096.0F},
    {"surf_NullNormal", 8192.0F},
    {"surf_NoAreas", 16384.0F},
    {"surf_non_penetrable", 32768.0F},
    {"surf_NoShoot", 65536.0F},
    {"surf_Ladder_Top", 1073741824.0F},
    {"surfType_None", 0.0F},
    {"surfType_Metal", 1.0F},
    {"surfType_Stone", 2.0F},
    {"surfType_Flesh", 3.0F},
    {"surfType_Wood", 4.0F},
    {"surfType_Cardboard", 5.0F},
    {"surfType_Liquid", 6.0F},
    {"surfType_Glass", 7.0F},
    {"surfType_Plastic", 8.0F},
    {"surfType_Asphalt", 9.0F},
    {"surfType_Dirt", 10.0F},
    {"surfType_Concrete", 11.0F},
    {"surfType_Foliage", 12.0F},
    {"surfType_Linoleum", 13.0F},
    {"surfType_Fabric", 14.0F},
    {"surfType_Rubber", 15.0F},
    {"surfType_Rock", 16.0F},
    {"surfType_SteamPipe", 17.0F},
    {"surfType_WaterPipe", 18.0F},
    {"surfType_Armor", 19.0F},
    {"surfType_Sludge", 20.0F},
    {"surfType_ImpNest", 21.0F},
    {"surfType_HollowMetal", 22.0F},
    {"surfType_FleshPlayer", 23.0F},
    {"surfType_FleshDarkAngel", 24.0F},
    {"surfType_BloodPool", 25.0F},
    {"surfType_Ash", 26.0F},
    {"surfType_FleshMutant", 27.0F},
    {"surfType_ThickPadding", 28.0F},
    {"surfType_PlayerAPCMetal", 29.0F},
    {"surfType_RicketyMetal", 30.0F},
    {"surfType_RicketyWood", 31.0F},
    {"surfType_SheetMetal", 32.0F},
    {"surfType_Porcelain", 33.0F},
    {"surfType_Plaster", 34.0F},
    {"surfType_VentMetal", 35.0F},
    {"surfType_Electronics", 36.0F},
    {"surfType_OilPipe", 37.0F},
    {"surfType_PanzerMetal", 38.0F},
    {"surfType_BreakableArmor", 39.0F},
    {"surfType_ElectricShield", 40.0F},
    {"fracType_None", 0.0F},
    {"fracType_Small", 1.0F},
    {"fracType_Medium", 2.0F},
    {"fracType_Large", 3.0F},
    {"fracContent_None", 0.0F},
    {"fracContent_Liquid", 256.0F},
    {"fracFlag_None", 0.0F},
    {"fracFlag_Fragile", 16777216.0F},
};

template <typename T>
__forceinline T* FindDecl(const char* name, bool makeDefault = true,
                          bool remap = true) {
#if defined(WOLF_RENDERER_RENDERSYSTEM_INIT_TEST_ACCESS)
  renderSystemInitDeclType_t type = RSID_TABLE;
  if constexpr (std::is_same_v<T, idDeclRenderParm>) {
    type = RSID_RENDER_PARM;
  } else if constexpr (std::is_same_v<T, idMaterial>) {
    type = RSID_MATERIAL;
  } else if constexpr (std::is_same_v<T, idDeclRenderProg>) {
    type = RSID_RENDER_PROG;
  }
  return testHooks.findDecl != nullptr
             ? static_cast<T*>(
                   testHooks.findDecl(type, name, makeDefault, remap))
             : nullptr;
#else
  return static_cast<T*>(
      T::resourceList.FindWithInheritance(name, makeDefault, remap));
#endif
}

struct RenderParmBinding {
  const char* name;
  const idDeclRenderParm* idRenderSystemLocal::*member;
};

constexpr RenderParmBinding renderParmBindings[]{
    {"powerMip", &idRenderSystemLocal::rpPowerMip},
    {"frameNumber", &idRenderSystemLocal::rpFrameNumber},
    {"glareStep", &idRenderSystemLocal::rpGlareStep},
    {"glareMap", &idRenderSystemLocal::rpGlareMap},
    {"postDistortionMap", &idRenderSystemLocal::rpDistortionMap},
    {"grainMap", &idRenderSystemLocal::rpGrainMap},
    {"dynamicEnvMap", &idRenderSystemLocal::rpDynamicEnvMap},
    {"virtualTextureFeedbackFloat",
     &idRenderSystemLocal::rpVirtualTextureFeedbackFloat},
    {"pageTableMap", &idRenderSystemLocal::rpPageTableMap},
    {"vmtrPageTableMap", &idRenderSystemLocal::rpVmtrPageTableMap},
    {"godRayQuadScaleBias", &idRenderSystemLocal::rpGodRayQuadScaleBias},
    {"godRaySourceScaleBias", &idRenderSystemLocal::rpGodRaySourceScaleBias},
    {"godRayColor", &idRenderSystemLocal::rpGodRaysColor},
    {"godRayViewPos", &idRenderSystemLocal::rpGodRayViewPos},
    {"gammaForBrightness", &idRenderSystemLocal::rpGammaForBrightness},
    {"adaptiveGlareMap", &idRenderSystemLocal::rpAdaptiveGlareMap},
    {"adaptiveGlareMapAlt", &idRenderSystemLocal::rpAdaptiveGlareMapAlt},
    {"luminosityMap", &idRenderSystemLocal::rpLuminosityMap},
    {"luminosityMapAlt", &idRenderSystemLocal::rpLuminosityMapAlt},
    {"adaptiveLuminance", &idRenderSystemLocal::rpAdaptiveLuminance},
    {"adaptiveGlareBlurMap", &idRenderSystemLocal::rpAdaptiveGlareBlurMap},
    {"adaptiveGlareBlurStep", &idRenderSystemLocal::rpAdaptiveGlareBlurStep},
    {"adaptiveGlareParms", &idRenderSystemLocal::rpAdaptiveGlareParms},
    {"adaptiveGlareColor", &idRenderSystemLocal::rpAdaptiveGlareColor},
    {"frameExposureMap", &idRenderSystemLocal::rpFrameExposureMap},
    {"frameExposureParms1", &idRenderSystemLocal::rpFrameExposureParms1},
    {"frameExposureParms2", &idRenderSystemLocal::rpFrameExposureParms2},
};

struct MaterialBinding {
  const char* name;
  const idMaterial* idRenderSystemLocal::*member;
};

constexpr MaterialBinding materialBindings[]{
    {"_default", &idRenderSystemLocal::mtrDefault},
    {"lights/solidWhite", &idRenderSystemLocal::mtrDefaultPointLight},
    {"lights/solidWhite", &idRenderSystemLocal::mtrDefaultProjectedLight},
    {"lights/solidWhite", &idRenderSystemLocal::mtrDefaultParallelLight},
    {"lights/solidWhite", &idRenderSystemLocal::mtrDefaultPointSlowLight},
    {"lights/solidWhite", &idRenderSystemLocal::mtrDefaultProjectedSlowLight},
    {"lights/solidWhite", &idRenderSystemLocal::mtrDefaultParallelSlowLight},
    {"lights_blended/defaultPointLight",
     &idRenderSystemLocal::mtrDefaultPointBlendLight},
    {"lights_blended/defaultProjectedLight",
     &idRenderSystemLocal::mtrDefaultProjectedBlendLight},
    {"lights_blended/defaultParallelLight",
     &idRenderSystemLocal::mtrDefaultParallelBlendLight},
};

struct ProgramBinding {
  const char* name;
  const idDeclRenderProg* idRenderSystemLocal::*member;
  bool makeDefault;
};

constexpr ProgramBinding earlyProgramBindings[]{
    {"gauss15", &idRenderSystemLocal::progGauss, true},
    {"postProcess", &idRenderSystemLocal::progPostProcess, true},
    {"glareScale", &idRenderSystemLocal::progGlareScale, true},
    {"overlappedDownSample", &idRenderSystemLocal::progOverlappedDownSample,
     true},
    {"feedbackDownSample", &idRenderSystemLocal::progFeedbackDownSample,
     true},
    {"buildGodRays", &idRenderSystemLocal::progCreateGodRayMask, true},
    {"drawGodRay", &idRenderSystemLocal::progDrawGodRay, true},
    {"depthOnly", &idRenderSystemLocal::progDepthOnly, true},
    {"colorOnly", &idRenderSystemLocal::progColorOnly, true},
    {"colorParmOnly", &idRenderSystemLocal::progColorParmOnly, true},
    {"wireFrame", &idRenderSystemLocal::progWireFrame, true},
    {"textureOnly", &idRenderSystemLocal::progTextureOnly, true},
    {"cubeImageOnly", &idRenderSystemLocal::progCubeImageOnly, true},
    {"testImage", &idRenderSystemLocal::progTestImage, true},
    {"testImageShadow", &idRenderSystemLocal::progTestImageShadow, true},
    {"testImageMultisample", &idRenderSystemLocal::progTestImageMultisample,
     true},
    {"basicBlend", &idRenderSystemLocal::progBasicBlend, true},
    {"environmentBlend", &idRenderSystemLocal::progEnvironmentBlend, true},
    {"colorCorrectionBlend", &idRenderSystemLocal::progColorCorrectionBlend,
     true},
    {"occluderDepthOnly", &idRenderSystemLocal::progOccluderDepthOnly, true},
    {"occluderUnique", &idRenderSystemLocal::progOccluderUnique, true},
    {"landUniqueBump", &idRenderSystemLocal::progLandUniqueBump, true},
    {"landUniqueDiffuseOnly", &idRenderSystemLocal::progLandUniqueDiffuseOnly,
     true},
    {"blendLightDebugDraw", &idRenderSystemLocal::progBlendedLightDebugDraw,
     true},
    {"deferredPointLight", &idRenderSystemLocal::progDeferredPointLight, true},
    {"deferredSpotLight", &idRenderSystemLocal::progDeferredSpotLight, true},
    {"deferredParallelLight", &idRenderSystemLocal::progDeferredParallelLight,
     true},
    {"interactionPrePass", &idRenderSystemLocal::progInteractionPrePass, true},
    {"interactionPostPass", &idRenderSystemLocal::progInteractionPostPass,
     true},
    {"varianceShadowMap", &idRenderSystemLocal::progVarianceShadowMap, true},
    {"dimShadowDrawMidQuality",
     &idRenderSystemLocal::progDimShadowDrawMidQuality, false},
    {"globalFog", &idRenderSystemLocal::progGlobalFog, true},
};

constexpr ProgramBinding lateProgramBindings[]{
    {"bloomThreshold", &idRenderSystemLocal::progBloomThreshold, true},
    {"luminosity", &idRenderSystemLocal::progLuminosity, true},
    {"adaptiveGlareGauss15", &idRenderSystemLocal::progAdaptiveGlareGauss,
     true},
    {"frameExposureThreshold", &idRenderSystemLocal::progFrameExposure, true},
    {"overlappedDownSampleMaxAlpha",
     &idRenderSystemLocal::progOverlappedDownSampleMaxAlpha, true},
    {"calcDepthOfFieldValues", &idRenderSystemLocal::progCalcDepthOfFieldValues,
     true},
    {"calcDepthOfFieldMaxKernel",
     &idRenderSystemLocal::progCalcDepthOfFieldMaxKernel, true},
    {"depthOfFieldFilterPass", &idRenderSystemLocal::progDepthOfFieldFilterPass,
     true},
    {"depthOfFieldDoubleFilterPass",
     &idRenderSystemLocal::progDepthOfFieldDoubleFilterPass, true},
    {"depthOfFieldFilterPass_HQ",
     &idRenderSystemLocal::progDepthOfFieldFilterPass_HQ, true},
    {"depthOfFieldDoubleFilterPass_HQ",
     &idRenderSystemLocal::progDepthOfFieldDoubleFilterPass_HQ, true},
    {"hazeFlareBlurPass1", &idRenderSystemLocal::progHazeFlareBlurPass1,
     true},
    {"hazeFlareBlurPass2", &idRenderSystemLocal::progHazeFlareBlurPass2,
     true},
    {"hazeFlare", &idRenderSystemLocal::progHazeFlare, true},
    {"screenSpaceReflections",
     &idRenderSystemLocal::progScreenSpaceReflections, true},
    {"screenSpaceReflectionsApply",
     &idRenderSystemLocal::progScreenSpaceReflectionsApply, true},
};

static_assert(std::size(defaultRenderParms) == 97);
static_assert(std::size(renderParmBindings) == 27);
static_assert(std::size(materialBindings) == 10);
static_assert(std::size(earlyProgramBindings) == 32);
static_assert(std::size(lateProgramBindings) == 16);

__forceinline idImageOpts BaseTargetOptions(textureFormat_t format, int width,
                                             int height) {
  idImageOpts opts{};
  opts.textureType = TT_2D;
  opts.format = format;
  opts.colorFormat = CFM_DEFAULT;
  opts.width = width;
  opts.height = height;
  opts.packedTail = true;
  opts.textureBindType = TB_RENDER_TARGET;
  opts.textureUsage = TU_DEFAULT;
  opts.textureOptions = TO_NONE;
  opts.sampler.filter = TF_LINEAR;
  opts.sampler.repeat = TR_REPEAT;
  opts.sampler.lodMaxClamp = 13.0F;
  return opts;
}

} // namespace

#if defined(WOLF_RENDERER_RENDERSYSTEM_INIT_TEST_ACCESS)
void RenderSystemInitTestAccess::SetHooks(
    const RenderSystemInitTestHooks& hooks) {
  testHooks = hooks;
}
#endif

__declspec(noinline) void idRenderSystemLocal::InitShadowMapTextures() {
  if (renderDestShadow != nullptr)
    return;

  idImageOpts opts{};
  opts.textureType = TT_2D;
  opts.format = FMT_DEPTH;
  opts.colorFormat = CFM_DEFAULT;
  opts.width = r_sb_imageSize.GetInteger();
  opts.height = r_sb_imageSize.GetInteger();
  opts.numLevels = 1;
  opts.packedTail = true;
  opts.textureBindType = TB_DEPTH_STENCIL_TARGET;
  opts.textureUsage = TU_DEFAULT;
  opts.textureOptions = TO_NONE;
  opts.sampler.filter = TF_LINEAR;
  opts.sampler.repeat = TR_CLAMP;
  opts.sampler.lodMaxClamp = 13.0F;
  opts.sampler.depthCompareMode = true;
  idImage* const image =
      globalImages->ScratchImage("_shadowBuffer_d", opts);
  renderDestShadow = DestinationForImages(nullptr, image);
}

__declspec(noinline) void idRenderSystemLocal::InitInGameGuiRenderTexture() {
  idImageOpts opts{};
  opts.textureType = TT_2D;
  opts.format = FMT_RGBA8;
  opts.colorFormat = CFM_DEFAULT;
  opts.width = 768;
  opts.height = 768;
  opts.numLevels = 5;
  opts.packedTail = true;
  opts.textureBindType = TB_RENDER_TARGET;
  opts.textureUsage = TU_DEFAULT;
  opts.textureOptions = TO_GENERATE_MIPS;
  opts.sampler.filter = TF_DEFAULT;
  opts.sampler.repeat = TR_CLAMP;
  opts.sampler.lodBias = -1.0F;
  opts.sampler.lodMaxClamp = 4.0F;
  imgGui = globalImages->ScratchImage("_gui", opts);
  renderDestGui = DestinationForImages(imgGui, nullptr);
}

__declspec(noinline) void idRenderSystemLocal::InitUnitSquare() {
  unitSquareTris = AllocateRendererObject<idTriangles>();
  SetSquareGeometry(unitSquareTris, false);
  unitSquareGuiTris = AllocateRendererObject<idTriangles>();
  SetSquareGeometry(unitSquareGuiTris, true);
}

__declspec(noinline) void idRenderSystemLocal::Shutdown() {
  common->SetRefreshOnPrint(false);
  idLib::Printf("idRenderSystem::Shutdown()\n");
  idRenderModelGui::Shutdown();
  videoManager->Shutdown();
  if (!idLib::headless)
    textureSamplerManager->Shutdown();

  if (unitSquareTris != nullptr) {
    idTriangles* const triangles = unitSquareTris;
    triangles->~idTriangles();
    idMem::Singleton()->Free(triangles, ALIGN_16);
  }
  unitSquareTris = nullptr;
  if (unitSquareGuiTris != nullptr) {
    idTriangles* const triangles = unitSquareGuiTris;
    triangles->~idTriangles();
    idMem::Singleton()->Free(triangles, ALIGN_16);
  }
  unitSquareGuiTris = nullptr;
  virtualTexturePreloader.DestroyThread();
  virtualTextureSystem.Shutdown();
  ShutdownSystem();
}

__declspec(noinline) void idRenderSystemLocal::Init2() {
  idLib::Printf("------- Initializing renderSystem media --------\n");
  InitCommands();

  for (const DefaultRenderParm& defaultParm : defaultRenderParms) {
    idDeclRenderParm* const parm =
        FindDecl<idDeclRenderParm>(defaultParm.name, false, false);
    if (parm != nullptr && parm->parmType == PT_VECTOR) {
      parm->declaredValue.value[0] = defaultParm.value;
      parm->declaredValue.value[1] = defaultParm.value;
      parm->declaredValue.value[2] = defaultParm.value;
      parm->declaredValue.value[3] = defaultParm.value;
    }
  }

  for (const RenderParmBinding& binding : renderParmBindings) {
    this->*(binding.member) =
        FindDecl<idDeclRenderParm>(binding.name, true, true);
  }

  staticModelManager->Init();
  videoManager->Init();
  SyncRenderThread(false);
  MakeCurrent(GetGameWindow());
  SetColorMappings(r_gamma.GetFloat(), r_brightness.GetFloat(), blackLevel[1]);

  for (const MaterialBinding& binding : materialBindings) {
    this->*(binding.member) = FindDecl<idMaterial>(binding.name, true, true);
  }
  for (const ProgramBinding& binding : earlyProgramBindings) {
    this->*(binding.member) = FindDecl<idDeclRenderProg>(
        binding.name, binding.makeDefault, true);
  }

  declHighlightTable =
      FindDecl<idDeclTable>("highlightTable", true, true);
  declHighlightExplodeTable =
      FindDecl<idDeclTable>("highlightTable_explode", true, true);
  defaultCubeModel = staticModelManager->FindStaticModel("_default", true);
  unitCubeModel = staticModelManager->FindStaticModel("_unitCube", true);
  zeroOneCubeModel =
      staticModelManager->FindStaticModel("_zeroOneCube", true);
  extrudeBoxModel = staticModelManager->FindStaticModel("_extrudeBox", true);
  zeroOneSquareModel =
      staticModelManager->FindStaticModel("_zeroOneSquare", true);
  unitSquareModel = staticModelManager->FindStaticModel("_unitSquare", true);
  imgGrain = globalImages->ImageFromFile("nearest textures/common/postgrain",
                                         nullptr, true);

  for (const ProgramBinding& binding : lateProgramBindings) {
    this->*(binding.member) = FindDecl<idDeclRenderProg>(
        binding.name, binding.makeDefault, true);
  }

  idLib::Printf("renderSystem initialized.\n");
  idLib::Printf("--------------------------------------\n");
}

__declspec(noinline) void idRenderSystemLocal::InitRenderTextures() {
  idImageOpts opts{};
  opts.textureType = TT_CUBIC;
  opts.format = FMT_RGBA8;
  opts.colorFormat = CFM_DEFAULT;
  opts.width = 128;
  opts.height = 128;
  opts.numLevels = 5;
  opts.packedTail = true;
  opts.textureBindType = TB_RENDER_TARGET;
  opts.textureUsage = TU_DEFAULT;
  opts.textureOptions = TO_NONE;
  opts.sampler.filter = TF_TRILINEAR;
  opts.sampler.repeat = TR_REPEAT;
  opts.sampler.lodMaxClamp = 4.0F;
  opts.sampler.forceBias = true;
  imgDynamicEnvironment[0] =
      globalImages->ScratchImage("_environment", opts);
  imgDynamicEnvironment[1] =
      globalImages->ScratchImage("_environment1", opts);
  for (int index = 0; index < 2; ++index) {
    renderDestDynamicEnv[index] =
        DestinationForImages(imgDynamicEnvironment[index], nullptr);
  }

  opts = BaseTargetOptions(FMT_RGBA8, 256, 16);
  opts.numLevels = 1;
  opts.sampler.filter = TF_DEFAULT;
  opts.sampler.lodMaxClamp = 13.0F;
  imgDynamicColorCorrection[0] =
      globalImages->ScratchImage("_colorCorrection", opts);
  imgDynamicColorCorrection[1] =
      globalImages->ScratchImage("_colorCorrection1", opts);
  for (int index = 0; index < 2; ++index) {
    renderDestDynamicColorCorrection[index] =
        DestinationForImages(imgDynamicColorCorrection[index], nullptr);
  }

  opts = {};
  opts.textureType = TT_2D;
  opts.format = FMT_RGBA8;
  opts.colorFormat = CFM_DEFAULT;
  opts.width = 256;
  opts.height = 1;
  opts.numLevels = 1;
  opts.packedTail = true;
  opts.textureBindType = TB_SHADER;
  opts.textureUsage = TU_DEFAULT;
  opts.textureOptions = TO_NONE;
  opts.sampler.filter = TF_NEAREST;
  opts.sampler.repeat = TR_CLAMP;
  opts.sampler.lodMaxClamp = 13.0F;
  imgColorGradingLUT[0] =
      globalImages->ScratchImage("_colorBalanceLUT", opts);
  imgColorGradingLUT[1] =
      globalImages->ScratchImage("_colorBalanceLUT1", opts);

  const bool useFloatBuffers = r_useFloatBuffers.GetBool();
  const textureFormat_t colorFormat =
      useFloatBuffers ? FMT_RGBA16F : FMT_RGBA8;
  const renderClass_t renderClass =
      useFloatBuffers ? TC_FLOAT16_RGBA : TC_RGBA;

  opts = {};
  opts.textureType = TT_2D;
  opts.format = FMT_DEPTH_STENCIL;
  opts.width = GetWidth();
  opts.height = GetHeight();
  opts.packedTail = true;
  opts.textureBindType = TB_DEPTH_STENCIL_TARGET;
  opts.sampler.filter = TF_NEAREST;
  opts.sampler.repeat = TR_CLAMP;
  opts.sampler.lodMaxClamp = 13.0F;
  imgViewDepth = globalImages->ScratchImage("_viewDepth", opts);
  renderDestViewDepth = DestinationForImages(nullptr, imgViewDepth);

  for (int index = 0; index < 2; ++index) {
    opts = BaseTargetOptions(colorFormat, GetWidth(), GetHeight());
    opts.numLevels = 10;
    opts.textureOptions = TO_GENERATE_MIPS;
    opts.sampler.filter = TF_TRILINEAR;
    opts.sampler.repeat = TR_CLAMP;
    opts.sampler.lodMaxClamp = 9.0F;
    va name("_viewColor%d", index);
    imgViewColor[index] = globalImages->ScratchImage(name.buffer, opts);
    renderDestViewColor[index] =
        DestinationForImages(imgViewColor[index], nullptr);
  }

  opts = BaseTargetOptions(colorFormat, GetWidth(), GetHeight());
  opts.numLevels = 10;
  opts.textureOptions = TO_GENERATE_MIPS;
  opts.sampler.filter = TF_TRILINEAR;
  opts.sampler.repeat = TR_CLAMP;
  opts.sampler.lodMaxClamp = 9.0F;
  imgViewPostScratch = globalImages->ScratchImage("_postScratch", opts);
  renderDestViewPostScratch =
      DestinationForImages(imgViewPostScratch, nullptr);

  opts = BaseTargetOptions(colorFormat, GetWidth() / 8, GetHeight() / 8);
  opts.sampler.filter = TF_LINEAR;
  opts.sampler.repeat =
      r_mip3Border.GetBool() ? TR_CLAMP_TO_BORDER : TR_CLAMP;
  imgScreenMip3 = globalImages->ScratchImage("_screenMip3", opts);
  imgScreenMip3Alt = globalImages->ScratchImage("_screenMip3_alt", opts);
  renderDestMip1 = AllocateRendererObject<idRenderDestination>();
  renderDestMip1->Create("_screenMip1", GetWidth() / 2, GetHeight() / 2,
                         renderClass);
  renderDestMip3 = DestinationForImages(imgScreenMip3, nullptr);
  renderDestMip3Alt = DestinationForImages(imgScreenMip3Alt, nullptr);

  opts = BaseTargetOptions(colorFormat, GetWidth() / 4, GetHeight() / 4);
  opts.numLevels = 1;
  opts.sampler.filter = TF_LINEAR;
  opts.sampler.repeat = TR_CLAMP;
  opts.sampler.lodMaxClamp = 0.0F;
  imgDistortion[0] = globalImages->ScratchImage("_distortion0", opts);
  imgDistortion[1] = globalImages->ScratchImage("_distortion1", opts);
  renderDestDistortion[0] =
      DestinationForImages(imgDistortion[0], nullptr);
  renderDestDistortion[1] =
      DestinationForImages(imgDistortion[1], nullptr);

  opts = BaseTargetOptions(colorFormat, 1, 1);
  opts.sampler.filter = TF_NEAREST;
  opts.sampler.repeat = TR_CLAMP;
  imgLuminosity = globalImages->ScratchImage("_luminosityImage", opts);
  renderDestLuminosity = DestinationForImages(imgLuminosity, nullptr);
  imgLuminosityAlt =
      globalImages->ScratchImage("_luminosityImageAlt", opts);
  renderDestLuminosityAlt =
      DestinationForImages(imgLuminosityAlt, nullptr);

  opts = BaseTargetOptions(colorFormat, GetWidth() / 8, GetHeight() / 8);
  opts.sampler.filter = TF_LINEAR;
  opts.sampler.repeat =
      r_mip3Border.GetBool() ? TR_CLAMP_TO_BORDER : TR_CLAMP;
  imgFrameExposure = globalImages->ScratchImage("_frameExposure", opts);
  renderDestFrameExposure =
      DestinationForImages(imgFrameExposure, nullptr);
  imgFrameExposureAlt =
      globalImages->ScratchImage("_frameExposure_alt", opts);
  renderDestFrameExposureAlt =
      DestinationForImages(imgFrameExposureAlt, nullptr);

  opts = BaseTargetOptions(colorFormat, renderSystem->GetWidth() / 8,
                           renderSystem->GetHeight() / 8);
  opts.numLevels = 1;
  opts.sampler.filter = TF_LINEAR;
  opts.sampler.repeat =
      r_mip3Border.GetBool() ? TR_CLAMP_TO_BORDER : TR_CLAMP;
  imgBloom = globalImages->ScratchImage("_bloom", opts);
  renderDestBloom = DestinationForImages(imgBloom, nullptr);

  opts = BaseTargetOptions(colorFormat, GetWidth() / 8, GetHeight() / 8);
  opts.numLevels = 1;
  opts.sampler.filter = TF_LINEAR;
  opts.sampler.repeat = TR_CLAMP;
  opts.sampler.lodMaxClamp = 0.0F;
  for (int index = 0; index < 2; ++index) {
    va name("_hazeFlare%d", index);
    imgHazeFlare[index] = globalImages->ScratchImage(name.buffer, opts);
    renderDestHazeFlare[index] =
        DestinationForImages(imgHazeFlare[index], nullptr);
  }

  for (idImage* image : imgViewColor) {
    const unsigned int byteCount = static_cast<unsigned int>(
        image->opts.width * image->opts.height *
        image->BitsForInternalFormat() / 8);
    idHeapArray<std::uint8_t> black;
    black.Alloc(byteCount);
    std::memset(black.buffer, 0, byteCount);
    image->SubImageUpload(0, 0, 0, 0, image->opts.width,
                          image->opts.height, black.buffer, 0);
  }
}

__declspec(noinline) void idRenderSystemLocal::Init() {
  idLib::Printf("------- Initializing renderSystem --------\n");
  renderFlags = RENDERFLAG_BLOCKLOADINGICON;
  if (!idLib::headless) {
    PreliminaryRenderSetup();
    if (!renderSystem->GetUsingSMP()) {
      InitContext();
      ShowGameWindow();
    }
  }
  StartRenderThread();
  if (renderSystem->GetUsingSMP()) {
    RestartRenderThread(nullptr);
    renderThread->WaitForThread();
    MakeCurrent(GetGameWindow());
  }
  sys->GenerateEvents();
  textureSamplerManager->Init();
  globalImages->Init();
  InitInGameGuiRenderTexture();
  InitShadowMapTextures();
  InitRenderTextures();
  virtualTextureSystem.Init();
  virtualTexturePreloader.SpawnThread();
  InitUnitSquare();
}
