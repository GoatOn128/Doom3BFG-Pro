#include "engine/renderer/renderthread.h"

#include "engine/framework/coderesource.h"
#include "engine/framework/common.h"
#include "engine/framework/console.h"
#include "engine/framework/session.h"
#include "engine/models/rendermodelgui.h"
#include "engine/models/renderworld.h"
#include "engine/models/static/staticmodel.h"
#include "engine/renderer/autorenderparms.h"
#include "engine/renderer/bufferobject.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/gl_api.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/ingamevideo.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/render.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/renderlocal.h"
#include "engine/renderer/renderlog.h"
#include "engine/renderer/rendervideooverlay.h"
#include "engine/renderer/resolutionscale.h"
#include "engine/renderer/virtualtexture.h"
#include "engine/sys/sys_savegame.h"
#include "engine/sys/win32/win_main.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/geometry/rendermatrix.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/metrics/recoverymetrics.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cstdint>
#include <new>

extern idCVar r_brightness;
extern idCVar r_defaultPolyOfsFactor;
extern idCVar r_defaultPolyOfsUnits;
extern idCVar r_gamma;
extern idCVar r_singleTriangle;
extern int lastUsercmdGenMicroSeconds;

idCVar r_waitForQuery("r_waitForQuery", "1", 1,
                      "Stall until occlusion query returns");
idCVar r_forceTwoSided("r_forceTwoSided", "0", 2,
                       "Force two sided rendering for everything");
idCVar r_useStateCaching("r_useStateCaching", "1", 1,
                         "avoid redundant state changes in GL_*() calls");
idCVar r_lazyBindPrograms("r_lazyBindPrograms", "1", 1,
                          "lazy program binds");
idCVar r_lazyBindParms("r_lazyBindParms", "1", 1,
                       "lazy update of render parms");
idCVar r_lazyBindTextures("r_lazyBindTextures", "1", 1,
                          "lazy texture binds");
idCVar r_stripFragmentBranches("r_stripFragmentBranches", "1", 1,
                               "strip branches from fragment programs");
idCVar r_skipAnalyze("r_skipAnalyze", "0", 2,
                     "skip the feedback analysis");
idCVar r_skipDetailTris("r_skipDetailTris", "0", 1,
                        "skip all detail triangles");
idCVar r_sleep("r_sleep", "0", 2,
               "Sleep for this many milliseconds to simulate low frame rates.");
idCVar r_finishEveryDraw("r_finishEveryDraw", "0", 1,
                         "call glFinish() or equivalent after each draw call");
idCVar r_syncAtEndFrame("r_syncAtEndFrame", "0", 1,
                        "sync at GL_EndFrame instead at swap and wait");
idCVar r_printSyncGpuAtEndThreshold(
    "r_printSyncGpuAtEndThreshold", "-1", 4,
    "threshold for print end of frame wait time in ms, -1 == never");
idCVar r_disableGLSyncArb("r_disableGLSyncArb", "0", 1,
                         "use a full glFinish instead of gl sync fence");
idCVar r_gLSyncArbLongTimeOut(
    "r_gLSyncArbLongTimeOut", "0", 1,
    "use a long timeout (1s) for glClientWaitSync  instead of a loop of short (1ms)");
idCVar r_glUseVAO(
    "r_glUseVAO", "1", 1,
    "use Vertex Array Objects to speed up buffer bindings (not supported in tools engine)");
idCVar r_glUseMultiQuery(
    "r_glUseMultiQuery", "1", 1,
    "use the glGetMultiQueryObjectuivAMD extension if available");
idCVar r_glSkipframeSync("r_glSkipframeSync", "0", 1,
                         "skip end frame sync");
idCVar r_enableVideoOverlay("r_enableVideoOverlay", "1", 1,
                            "enable (subtitle) video overlay");
idCVar r_eventLogLight("r_eventLogLight", "0", 2,
                       "Capture frame(s) with event log light");
idCVar r_filterFullscreenGUI(
    "r_filterFullscreenGUI", "0", 2,
    "Filter out the fullscreen only GUI (still renders world in background) 0 = off, 1 = black, 2 = white");

int deltaMicroseconds = 0;
int renderModelGuiFrameCount = 0;
int time_renderFrame = 0;

namespace {

idCodeResource<idDeclRenderParm> rpViewColorMap("viewColorMap");
idCodeResource<idDeclRenderParm> rpLoadingIconParms("loadingIconParms");
idCodeResource<idDeclRenderParm> rpLoadingIcon("loadingIcon");
idCodeResource<idDeclRenderParm> rpSavingIcon("savingIcon");
idCodeResource<idDeclRenderParm> rpTransMap("transMap");
idCodeResource<idDeclRenderProg> progGuiBlend("guiBlend");
idCodeResource<idDeclRenderProg> progGuiLoading("guiLoading");
idCodeResource<idDeclRenderProg> progViewColorIdentity("viewColorIdentity");
idCodeResource<idDeclRenderParm> rpGammaForBrightness("gammaForBrightness");
idCodeResource<idMaterial> mtrBink("_bink");
idCodeResource<idDeclRenderProg> progBink("bink");

template <typename T>
const idTextureSampler* AsSampler(const T* image) {
  return reinterpret_cast<const idTextureSampler*>(image);
}

idRender* Renderer(void* frameState) {
  return static_cast<idRender*>(frameState);
}

void SetNativeDestination() {
  const idODSObject<idRenderDestination> destination{renderDestNative};
  GL_SetRenderDestination(destination, 0, 0);
}

} // namespace

#if defined(WOLF_RENDERER_RENDERTHREAD_TEST_ACCESS)
void SetRenderThreadTestResources(
    const renderThreadTestResources_t& resources) {
  rpViewColorMap.r = reinterpret_cast<idResource*>(
      const_cast<idDeclRenderParm*>(resources.viewColorMap));
  rpLoadingIconParms.r = reinterpret_cast<idResource*>(
      const_cast<idDeclRenderParm*>(resources.loadingIconParms));
  rpLoadingIcon.r = reinterpret_cast<idResource*>(
      const_cast<idDeclRenderParm*>(resources.loadingIcon));
  rpSavingIcon.r = reinterpret_cast<idResource*>(
      const_cast<idDeclRenderParm*>(resources.savingIcon));
  rpTransMap.r = reinterpret_cast<idResource*>(
      const_cast<idDeclRenderParm*>(resources.transMap));
  progGuiBlend.r = reinterpret_cast<idResource*>(
      const_cast<idDeclRenderProg*>(resources.guiBlend));
  progGuiLoading.r = reinterpret_cast<idResource*>(
      const_cast<idDeclRenderProg*>(resources.guiLoading));
  progViewColorIdentity.r = reinterpret_cast<idResource*>(
      const_cast<idDeclRenderProg*>(resources.viewColorIdentity));
  rpGammaForBrightness.r = reinterpret_cast<idResource*>(
      const_cast<idDeclRenderParm*>(resources.gammaForBrightness));
  mtrBink.r = reinterpret_cast<idResource*>(
      const_cast<idMaterial*>(resources.binkMaterial));
  progBink.r = reinterpret_cast<idResource*>(
      const_cast<idDeclRenderProg*>(resources.binkProgram));
}
#endif

void idRenderThread::SetLoadingIconInfo(float posX, float posY, float scale,
                                        float speed) {
  loadingIconPosX = posX;
  loadingIconPosY = posY;
  loadingIconSpeed = speed;
  loadingIconScale = scale;
}

void ResizeImages(const idScreenRect* screenRect) {
  const int width = screenRect->x2 - screenRect->x1 + 1;
  const int height = screenRect->y2 - screenRect->y1 + 1;
  if (renderDestScaledColor->targetWidth == width &&
      renderDestScaledColor->targetHeight == height &&
      (tr.renderDestViewDepth == nullptr ||
       (tr.renderDestViewDepth->targetWidth == width &&
        tr.renderDestViewDepth->targetHeight == height))) {
    return;
  }

  globalImages->UnbindAll();
  auto resize = [](idRenderDestination* destination, int targetWidth,
                   int targetHeight) {
    if (destination != nullptr)
      destination->Resize(targetWidth, targetHeight);
  };
  resize(renderDestScaledColor, width, height);
  resize(renderDestScaledColor2, width, height);
  resize(renderDestScaledFeedback, width, height);
  resize(renderDestScaledColorWithFeedback, width, height);
  resize(renderDestScaledLinear, width, height);
  resize(tr.renderDestViewPostScratch, width, height);
  resize(tr.renderDestViewDepth, width, height);
  resize(tr.renderDestMip1, width / 2, height / 2);
  resize(tr.renderDestMip3, width / 8, height / 8);
  resize(tr.renderDestMip3Alt, width / 8, height / 8);
  resize(tr.renderDestBloom, width / 8, height / 8);
  for (int index = 0; index < 2; ++index) {
    resize(tr.renderDestHazeFlare[index], width / 8, height / 8);
  }
  for (int index = 0; index < 2; ++index) {
    resize(tr.renderDestViewColor[index], width, height);
    resize(tr.renderDestDistortion[index], width / 4, height / 4);
  }
}

void UpdateGraphicsAPIWrapperConfig() {
  wrapperConfig_t config{};
  config.waitForOcclusionQuery = r_waitForQuery.valueInteger != 0;
  config.forceTwoSided = r_forceTwoSided.valueInteger == 1;
  config.disableStateCaching = r_useStateCaching.valueInteger == 0;
  config.lazyBindPrograms = r_lazyBindPrograms.valueInteger != 0;
  config.lazyBindParms = r_lazyBindParms.valueInteger != 0;
  config.lazyBindTextures = r_lazyBindTextures.valueInteger != 0;
  config.stripFragmentBranches = r_stripFragmentBranches.valueInteger != 0;
  config.skipDetailTris = r_skipDetailTris.valueInteger != 0;
  config.singleTriangle = r_singleTriangle.valueInteger != 0;
  config.finishEveryDraw = r_finishEveryDraw.valueInteger != 0;
  config.syncAtEndFrame = r_syncAtEndFrame.valueInteger != 0;
  config.skipGpuSyncAtEndOfFrame = r_glSkipframeSync.valueInteger != 0;
  config.useGLArbSync = r_disableGLSyncArb.valueInteger != 0
                            ? 0
                            : static_cast<unsigned char>(
                                  (r_gLSyncArbLongTimeOut.valueInteger != 0) +
                                  1);
  config.useGLVAO = r_glUseVAO.valueInteger != 0;
  config.printSyncGpuStatsThreshold =
      r_printSyncGpuAtEndThreshold.valueFloat;
  config.defaultPolyOfsUnits = r_defaultPolyOfsUnits.valueFloat;
  config.useMultiQuerry = r_glUseMultiQuery.valueInteger != 0;
  config.defaultPolyOfsFactor = r_defaultPolyOfsFactor.valueFloat;
  GL_SetWrapperConfig(config);
}

void idRenderThread::RenderLoadingIcon(float fracX, float fracY, float size) {
  const idImage* image = renderThreadParmState->GetParmImage(rpLoadingIcon.Get());
  if (image == nullptr)
    return;

  const int width = tr.GetWidth();
  const int height = tr.GetHeight();
  const int minimum = (std::min)(width, height);
  if (minimum <= 0)
    return;

  const float pixelSize = static_cast<float>(minimum) * size;
  idRenderMatrix mvp{};
  mvp.m[0] = pixelSize / static_cast<float>(width);
  mvp.m[3] = fracX;
  mvp.m[5] = pixelSize / static_cast<float>(height);
  mvp.m[7] = fracY;
  mvp.m[10] = 1.0F;
  mvp.m[15] = 1.0F;
  renderThreadParmState->SetMVPMatrix(mvp);
  GL_SetDefaultState();
  GL_Viewport(0, 0, width, height);
  GL_Scissor(0, 0, width, height);
  renderThreadParmState->SetParmSampler(rpTransMap.Get(), AsSampler(image));
  renderThreadParmState->SetParmVector(rpLoadingIconParms.Get(), 11.0F, 8.0F,
                                      1.0F, 0.0F);
  const idDeclRenderProg* blendProgram = progGuiBlend.Get();
  renderThreadParmState->VirtualEvaluateParmBlock(
      blendProgram->data.parmBlock, blendProgram->data.parmBlock);
  const idODSObject<idDeclRenderProg> program{progGuiLoading.Get()};
  const idODSObject<idTriangles> triangles{tr.unitSquareGuiTris};
  GL_DrawElements(program, triangles, 0x2C, false);
  renderThreadParmState->VirtualRollbackParmBlock(
      blendProgram->data.parmBlock, blendProgram->data.parmBlock);
  renderThreadParmState->SetParmSampler(rpTransMap.Get(),
                                        AsSampler(globalImages->blackImage));
}

void idRenderThread::RenderSavingIcon() {
  const idImage* image = renderThreadParmState->GetParmImage(rpSavingIcon.Get());
  if (image == nullptr)
    return;

  const int width = tr.GetWidth();
  const int height = tr.GetHeight();
  if ((std::min)(width, height) <= 0)
    return;

  const int imageWidth = image->opts.width;
  const int imageHeight = image->opts.height;
  idRenderMatrix mvp{};
  mvp.m[0] = static_cast<float>(imageHeight) / static_cast<float>(width);
  mvp.m[3] = -0.7F;
  mvp.m[5] = static_cast<float>(imageHeight) / static_cast<float>(height);
  mvp.m[7] = -0.6F;
  mvp.m[10] = 1.0F;
  mvp.m[15] = 1.0F;
  renderThreadParmState->SetMVPMatrix(mvp);
  GL_SetDefaultState();
  GL_Viewport(0, 0, width, height);
  GL_Scissor(0, 0, width, height);
  renderThreadParmState->SetParmSampler(rpTransMap.Get(), AsSampler(image));
  renderThreadParmState->SetParmVector(
      rpLoadingIconParms.Get(),
      static_cast<float>(imageWidth / imageHeight), 32.0F, 0.0F, 0.0F);
  const idDeclRenderProg* blendProgram = progGuiBlend.Get();
  renderThreadParmState->VirtualEvaluateParmBlock(
      blendProgram->data.parmBlock, blendProgram->data.parmBlock);
  const idODSObject<idDeclRenderProg> program{progGuiLoading.Get()};
  const idODSObject<idTriangles> triangles{tr.unitSquareGuiTris};
  GL_DrawElements(program, triangles, 0x2C, false);
  renderThreadParmState->VirtualRollbackParmBlock(
      blendProgram->data.parmBlock, blendProgram->data.parmBlock);
  renderThreadParmState->SetParmSampler(rpTransMap.Get(),
                                        AsSampler(globalImages->blackImage));
}

void idRenderThread::RenderAutomatic(void* deviceContext) {
  idRender* renderer = Renderer(frameState);
  renderer->WaitAndSwap(deviceContext, false);
  if (renderSystem->GetUsingSMP())
    Sys_PumpRenderThreadEvents();
  GL_StartFrame(tr.FrameNumber());
  SetNativeDestination();
  GL_SetDefaultState();
  const int width = tr.GetWidth();
  const int height = tr.GetHeight();
  GL_Viewport(0, 0, width, height);
  GL_Scissor(0, 0, width, height);
  GL_Clear(true, true, true, 1, 0.0F, 0.0F, 0.0F, 0.0F);

  if (renderOldFrameInAuto) {
    renderThreadParmState->SetFragmentPositionToViewTexture(
        renderer->GetLastUsedRenderWidth(),
        renderer->GetLastUsedRenderHeight(), width, height);
    renderThreadParmState->SetMVPMatrix(renderMatrix_identity);
    renderThreadParmState->SetParmSampler(rpViewColorMap.Get(),
                                          AsSampler(tr.imgViewColor[0]));
    const idDeclRenderProg* identity = progViewColorIdentity.Get();
    renderThreadParmState->VirtualEvaluateParmBlock(identity->data.parmBlock,
                                                     identity->data.parmBlock);
    GL_DrawElements({identity}, {tr.unitSquareTris}, 0, false);
    renderThreadParmState->VirtualRollbackParmBlock(identity->data.parmBlock,
                                                     identity->data.parmBlock);
  }

  renderThreadParmState->SetParmVector(
      rpGammaForBrightness.Get(), tr.gammaForBrighness,
      1.0F / tr.gammaForBrighness, 0.0F, 0.0F);
  if (renderLoadingIcon)
    RenderLoadingIcon(loadingIconPosX, loadingIconPosY, loadingIconScale);
  if (session != nullptr) {
    idSaveGameManager* saveGameManager = session->GetSaveGameManager();
    if (saveGameManager->SaveIconVisible())
      RenderSavingIcon();
  }
  Sys_Sleep(15);
}

void idRenderThread::RenderVideo() {
  if (!tr.BinkVideoIsLoaded() || !tr.videoPlaying)
    return;

  tr.UpdateBinkTextures();
  SetNativeDestination();
  GL_SetDefaultState();
  int width = tr.GetWidth();
  int height = tr.GetHeight();
  GL_Viewport(0, 0, width, height);
  GL_Scissor(0, 0, width, height);
  GL_Clear(true, true, true, 0x80, 0.0F, 0.0F, 0.0F, 0.0F);
  const int binkWidth = static_cast<int>(tr.GetBinkWidth());
  const unsigned binkHeight = tr.GetBinkHeight();
  // Retail performs the aspect-ratio numerator in a 32-bit register
  // (imul ebx, esi) before the signed divide.
  const int unscaledHeight =
      (width * static_cast<int>(binkHeight)) / binkWidth;
  int videoHeight = static_cast<int>(tr.GetPixelAspect() * unscaledHeight);
  videoHeight = (std::min)(videoHeight, height);
  const int videoY = (std::max)((height - videoHeight) / 2, 0);
  GL_Viewport(0, videoY, width, videoHeight);
  GL_Scissor(0, videoY, width, videoHeight);
  renderThreadParmState->SetFragmentPositionToViewTexture(width, height, width,
                                                          height);
  renderThreadParmState->SetMVPMatrixFlipped(renderMatrix_identity);
  const idMaterial* binkMaterial = mtrBink.Get();
  const idDeclRenderProg* binkProgram = progBink.Get();
  renderThreadParmState->VirtualEvaluateParmBlock(
      binkMaterial->renderParmBlock, binkMaterial->renderParmBlock);
  renderThreadParmState->VirtualEvaluateParmBlock(
      binkProgram->data.parmBlock, binkProgram->data.parmBlock);
  GL_DrawElements({binkProgram}, {tr.unitSquareTris}, 0, false);
  renderThreadParmState->VirtualRollbackParmBlock(
      binkProgram->data.parmBlock, binkProgram->data.parmBlock);
  renderThreadParmState->VirtualRollbackParmBlock(
      binkMaterial->renderParmBlock, binkMaterial->renderParmBlock);
  GL_UnbindVAO();

  idRenderVideoOverlay* overlay = common->VideoOverlay();
  if (overlay == nullptr || tr.videoOverlayModel == nullptr ||
      r_enableVideoOverlay.valueInteger == 0) {
    return;
  }

  GL_ViewportAndScissor(0, 0, width, height);
  idRenderModelCommitted* committed = tr.videoOverlayModel->committed;
  if (committed->surfaces.num > 0) {
    renderThreadParmState->SetFragmentPositionToViewTexture(width, height,
                                                            width, height);
    idRenderMatrix mvp{};
    mvp.m[0] = committed->r.scale.x;
    mvp.m[3] = committed->r.origin.x;
    mvp.m[5] = committed->r.scale.y;
    mvp.m[7] = committed->r.origin.y;
    mvp.m[15] = 1.0F;
    renderThreadParmState->SetMVPMatrix(mvp);
    renderThreadParmState->VirtualEvaluateParmBlock(
        committed->renderParmBlock, committed->renderParmBlock);
    for (int index = 0; index < committed->surfaces.num; ++index) {
      const idRenderModelSurface& surface = committed->surfaces.list[index];
      if (surface.geometry == nullptr || surface.material == nullptr)
        continue;
      renderLog.OpenBlock("Entering material %s\n",
                          surface.material->name.str);
      const idDeclRenderProg* stageProgram =
          surface.material->progs[MPR_STAGE];
      const idParmBlock& materialBlock = surface.material->renderParmBlock;
      renderThreadParmState->VirtualEvaluateParmBlock(materialBlock,
                                                       materialBlock);
      renderThreadParmState->VirtualEvaluateParmBlock(
          stageProgram->data.parmBlock, stageProgram->data.parmBlock);
      GL_DrawElements({stageProgram}, {surface.geometry}, surface.extraGLState,
                      false);
      renderThreadParmState->VirtualRollbackParmBlock(
          stageProgram->data.parmBlock, stageProgram->data.parmBlock);
      renderThreadParmState->VirtualRollbackParmBlock(materialBlock,
                                                       materialBlock);
    }
    renderThreadParmState->VirtualRollbackParmBlock(
        committed->renderParmBlock, committed->renderParmBlock);
  }

  tr.videoOverlayModel->Clear(0x28);
  tr.videoOverlayModel->SetViewport(0, 0, width, height);
  tr.videoOverlayModel->SetVirtualSize(
      static_cast<int>(renderSystem->GetPixelAspect() * width), height);
  overlay->Render(tr.videoOverlayModel, tr.GetBinkTime());
  tr.videoOverlayModel->Commit();
}

void idRenderThread::RenderManual(void* deviceContext, bool resolveDepth) {
  idRender* renderer = Renderer(frameState);
  videoManager->Update(committedRenderFrameInfo.renderTime);

  bool hasWorld = false;
  for (int index = 0; index < committedRenderFrameInfo.committedViews.num;
       ++index) {
    if (committedRenderFrameInfo.committedViews.list[index].world != nullptr) {
      hasWorld = true;
      break;
    }
  }
  if (hasWorld && r_skipAnalyze.valueInteger == 0) {
    virtualTextureSystem.PushFeedback(
        virtualTextureSystem.feedbackBuffer.buffer, 160, 120,
        virtualTextureSystem.feedbackBufferPitch, false);
    virtualTextureSystem.VisaulizeFeedback();
  }

  renderThreadParmState->SetParmVector(
      rpGammaForBrightness.Get(), tr.gammaForBrighness,
      1.0F / tr.gammaForBrighness, 0.0F, 0.0F);
  int worldCount = 0;
  for (int index = 0; index < committedRenderFrameInfo.committedViews.num;
       ++index) {
    if (committedRenderFrameInfo.committedViews.list[index].world != nullptr)
      ++worldCount;
  }

  deltaMicroseconds =
      static_cast<int>(Sys_Microseconds()) - lastUsercmdGenMicroSeconds;
  int renderedWorlds = 0;
  for (int index = 0; index < committedRenderFrameInfo.committedViews.num;
       ++index) {
    idScreenViewCommitted& view =
        committedRenderFrameInfo.committedViews.list[index];
    if (view.world == nullptr || view.usePreviousRendering)
      continue;
    resolutionScale.InitForMap(view.world->mapName);
    view.world->PostCommitData(view.renderView, view.renderView->r.renderTime);
    ResizeImages(&view.screenRect);
    renderer->InitSettings(view.world, view.renderView, worldCount);
    renderer->RenderSingleView(deviceContext, view.world, view.renderView,
                               view.screenRect);
    ++renderedWorlds;
    renderer->RenderGuiModels(view.committedGuis.list,
                              view.committedGuis.num, renderDestNative,
                              view.screenRect, view.guiOriginOffset);
    if (renderedWorlds >= 1)
      renderer->CaptureToViewColor(view.screenRect, view.renderView->viewIndex);
  }

  if (renderedWorlds == 0) {
    renderer->WaitAndSwap(deviceContext, false);
    GL_StartFrame(tr.FrameNumber());
    GL_SetDefaultState();
    SetNativeDestination();
    const int height = tr.GetHeight();
    const int width = tr.GetWidth();
    GL_Viewport(0, 0, width, height);
    GL_Scissor(0, 0, width, height);
    GL_Clear(true, false, false, 0, 0.0F, 0.0F, 0.0F, 0.0F);
  }

  if (committedRenderFrameInfo.committedViews.num == 0)
    return;

  SetNativeDestination();
  const int screenHeight = tr.GetHeight();
  const int screenWidth = tr.GetWidth();
  GL_Viewport(0, 0, screenWidth, screenHeight);
  GL_Scissor(0, 0, screenWidth, screenHeight);
  GL_Clear(true, false, false, 0, 0.0F, 0.0F, 0.0F, 0.0F);

  for (int index = 0; index < committedRenderFrameInfo.committedViews.num;
       ++index) {
    idScreenViewCommitted& view =
        committedRenderFrameInfo.committedViews.list[index];
    if (view.world == nullptr && !view.usePreviousRendering)
      continue;

    const int x = view.screenRect.x1;
    const int y = view.screenRect.y1;
    const int width = view.screenRect.x2 - x + 1;
    const int height = view.screenRect.y2 - y + 1;
    GL_Viewport(x, y, width, height);
    GL_Scissor(x, y, width, height);
    renderThreadParmState->SetParmSampler(
        rpViewColorMap.Get(), AsSampler(tr.imgViewColor[view.renderView->viewIndex]));
    const idScreenRect& viewport = view.renderView->viewport;
    const float positionToViewTexture[4] = {
        -static_cast<float>(x) / static_cast<float>(width),
        -static_cast<float>(y) / static_cast<float>(height),
        (static_cast<float>(viewport.x2 - viewport.x1 + 1) /
         static_cast<float>(width)) /
            static_cast<float>(width),
        (static_cast<float>(viewport.y2 - viewport.y1 + 1) /
         static_cast<float>(height)) /
            static_cast<float>(height),
    };
    renderThreadParmState->SetParmVector(rp_positionToViewTexture.Get(),
                                         positionToViewTexture);
    const idDeclRenderProg* identity = progViewColorIdentity.Get();
    renderThreadParmState->VirtualEvaluateParmBlock(identity->data.parmBlock,
                                                     identity->data.parmBlock);
    GL_DrawElements({identity},
                    {tr.unitSquareModel->surfaces.list[0].geometry}, 0, false);
    renderThreadParmState->VirtualRollbackParmBlock(identity->data.parmBlock,
                                                     identity->data.parmBlock);
    if (resolveDepth) {
      _glewBindFramebufferEXT(0x8CA8, tr.renderDestViewDepth->FBOhandle);
      _glewBindFramebufferEXT(0x8CA9, 0);
      _glewBlitFramebufferEXT(x, y, width, height, x, y, width, height, 0x100,
                              0x2600);
      _glewBindFramebufferEXT(0x8CA8, 0);
      _glewBindFramebufferEXT(0x8CA9, 0);
    }
  }

  if (tr.blockColorFrameNum > 0) {
    GL_Clear(true, false, false, 0, tr.blockColor.r, tr.blockColor.g,
             tr.blockColor.b, 0.0F);
    --tr.blockColorFrameNum;
  }
  wrapperStatsExcludingFullScreenGuis = GL_GetCurrentStats();
  if (r_filterFullscreenGUI.valueInteger != 0) {
    const float color =
        r_filterFullscreenGUI.valueInteger == 1 ? 0.0F : 1.0F;
    GL_Clear(true, false, false, 0, color, color, color, 0.0F);
  }

  renderer->InitSettings(nullptr, nullptr, 1);
  for (int index = 0; index < committedRenderFrameInfo.committedViews.num;
       ++index) {
    idScreenViewCommitted& view =
        committedRenderFrameInfo.committedViews.list[index];
    if (view.world != nullptr || view.usePreviousRendering)
      continue;
    const int width = view.screenRect.x2 - view.screenRect.x1 + 1;
    const int height = view.screenRect.y2 - view.screenRect.y1 + 1;
    GL_Viewport(view.screenRect.x1, view.screenRect.y1, width, height);
    GL_Scissor(view.screenRect.x1, view.screenRect.y1, width, height);
    renderer->RenderGuiModels(view.committedGuis.list,
                              view.committedGuis.num, renderDestNative,
                              view.screenRect, view.guiOriginOffset);
  }

  if (committedRenderFrameInfo.captureFrame) {
    idDeclRenderProg::BindTexture(0, tr.imgViewColor[0], nullptr);
    const int width =
        (std::min)(tr.imgViewColor[0]->opts.width, tr.GetWidth());
    const int height =
        (std::min)(tr.imgViewColor[0]->opts.height, tr.GetHeight());
    glCopyTexSubImage2D(0xDE1, 0, 0, 0, 0, 0, width, height);
  }
  if (renderLoadingIcon)
    RenderLoadingIcon(loadingIconPosX, loadingIconPosY, loadingIconScale);
  if (tr.BinkVideoIsLoaded() && tr.videoPlaying)
    RenderVideo();
}

void idRenderThread::RenderBink(void* deviceContext) {
  idRender* renderer = Renderer(frameState);
  renderer->WaitAndSwap(deviceContext, true);
  if (renderSystem->GetUsingSMP())
    Sys_PumpRenderThreadEvents();
  GL_StartFrame(tr.FrameNumber());
  renderer->InitSettings(nullptr, nullptr, 1);
  if (renderModelGuiFrameCount == idRenderModelGui::frameCount)
    idRenderModelGui::StartFrame();
  renderModelGuiFrameCount = idRenderModelGui::frameCount;
  renderThreadParmState->SetParmVector(
      rpGammaForBrightness.Get(), tr.gammaForBrighness,
      1.0F / tr.gammaForBrighness, 0.0F, 0.0F);
  RenderVideo();
  for (int index = 0; index < committedRenderFrameInfo.committedViews.num;
       ++index) {
    idScreenViewCommitted& view =
        committedRenderFrameInfo.committedViews.list[index];
    if (view.world != nullptr || view.usePreviousRendering)
      continue;
    const int width = view.screenRect.x2 - view.screenRect.x1 + 1;
    const int height = view.screenRect.y2 - view.screenRect.y1 + 1;
    GL_Viewport(view.screenRect.x1, view.screenRect.y1, width, height);
    GL_Scissor(view.screenRect.x1, view.screenRect.y1, width, height);
    renderer->RenderGuiModels(view.committedGuis.list,
                              view.committedGuis.num, renderDestNative,
                              view.screenRect, view.guiOriginOffset);
  }
}

void idRenderThread::RenderFrameInternal(renderMode_t mode,
                                         void* deviceContext,
                                         bool skipSwapBuffers,
                                         bool resolveDepth) {
  bool capturedEventLogFrame = false;
  if (r_eventLogLight.valueInteger != 0) {
    capturedEventLogFrame = true;
    r_eventLogLight.SetInteger(r_eventLogLight.valueInteger - 1, true);
    if (!renderEventRecorder.IsEnabled())
      renderEventRecorder.StartCapture();
  }
  RenderEventRecorderLightStartEvent_Impl("Frame - RenderFrameInternal");
  console->BeginTimeRange(RANGE_RENDER, &idColor::colorLtGrey);
  tr.pc.startCPUTimeMicroSec = Sys_Microseconds();
  renderThreadParmState->RollbackOpsToZero();
  UpdateGraphicsAPIWrapperConfig();
  renderThreadParmState->SetParmFloat(
      rp_SysTime.Get(), static_cast<float>(Sys_Milliseconds()) * 0.001F);
  console->BeginTimeRange(RANGE_RENDER, &idColor::colorGreen);
  if (mode == RENDER_MODE_MANUAL)
    RenderManual(deviceContext, resolveDepth);
  else if (mode == RENDER_MODE_AUTOMATIC)
    RenderAutomatic(deviceContext);
  else if (mode == RENDER_MODE_BINK || mode == RENDER_MODE_FORCE_EXIT)
    RenderBink(deviceContext);

  if (tr.updateGammaTable || (r_gamma.flags & 0x20000) != 0 ||
      (r_brightness.flags & 0x20000) != 0) {
    r_gamma.flags &= ~0x20000;
    r_brightness.flags &= ~0x20000;
    tr.SetColorMappings(r_gamma.valueFloat, r_brightness.valueFloat,
                        tr.blackLevel[1]);
    tr.updateGammaTable = false;
  }
  UnbindBufferObjects();
  globalImages->UnbindAll();
  GL_ResetProgramState();
  GL_EndFrame();
  // The retail call at 0x140472180 resolves to the three-byte ICF no-op at
  // RVA 0x1000.  It has no observable state and is intentionally omitted.
  if (!skipSwapBuffers)
    tr.renderingIsReadyForSwapbuffers = true;
  renderThreadParmState->RollbackOpsToZero();
  if (r_sleep.valueInteger > 0)
    Sys_Sleep(static_cast<unsigned long>(r_sleep.valueInteger));
  RenderEventRecorderLightStopEvent_Impl();
  if (capturedEventLogFrame && r_eventLogLight.valueInteger <= 0) {
    renderEventRecorder.StopCapture();
    renderEventRecorder.WriteReport("renderlogs/eventlog_.txt", true,
                                    FSPATH_SAVE);
    renderEventRecorder.Clear();
  }
  wrapperStatsFull = GL_GetCurrentStats();
  tr.pc.endCPUTimeMicroSec = Sys_Microseconds();
  time_renderFrame =
      static_cast<int>(tr.pc.startWaitForEndFrameMicroSec) +
      static_cast<int>(tr.pc.startSwapMicroSec) +
      static_cast<int>(tr.pc.endCPUTimeMicroSec) -
      static_cast<int>(tr.pc.startCPUTimeMicroSec) -
      static_cast<int>(tr.pc.endSwapMicroSec) -
      static_cast<int>(tr.pc.endWaitForEndFrameMicroSec);
  console->BeginTimeRange(RANGE_RENDER, &idColor::colorBlack);
}

int idRenderThread::Run() {
  if (!initialized) {
    tr.CreateRenderThreadContext();
    tr.InitContext();
    tr.ShowGameWindow();
    initialized = true;
    return 0;
  }

  if (deferredVidRestart) {
    tr.Restart();
    deferredVidRestart = false;
  }
  if (Sys_PumpRenderThreadEventsIfRequested())
    return 0;

  for (;;) {
    if (renderMode == RENDER_MODE_FORCE_EXIT) {
      if (!tr.WaitForNextVideoFrame(true)) {
        tr.videoPlaying = false;
        if (renderSystem->HasRenderFlag(16)) {
          renderMode = RENDER_MODE_MANUAL;
          return 0;
        }
        renderMode = RENDER_MODE_AUTOMATIC;
        renderOldFrameInAuto = false;
        renderLoadingIcon = !renderSystem->HasRenderFlag(768);
      }
    } else if (renderMode == RENDER_MODE_AUTOMATIC) {
      Sys_PumpRenderThreadEventsIfRequested();
    }

    if (renderMode == RENDER_MODE_SYNC) {
      GL_Finish();
      idLib::Printf("renderMode == RENDER_MODE_SYNC\n");
    } else {
      RenderFrameInternal(renderMode, hdc, false, false);
    }
    if (isTerminating)
      return 0;
    if (renderMode == RENDER_MODE_MANUAL || renderMode == RENDER_MODE_BINK ||
        renderMode == RENDER_MODE_SYNC) {
      return 0;
    }
  }
}

idRenderFrameInfoCommitted::~idRenderFrameInfoCommitted() = default;

template idList<idScreenViewCommitted, 5>::~idList();

idRenderThread::idRenderThread() {
  renderMode = RENDER_MODE_MANUAL;
  renderModeBeforeSync = RENDER_MODE_MANUAL;
  initialized = false;
  renderLoadingIcon = false;
  renderOldFrameInAuto = false;
  deferredVidRestart = false;
  loadingIconPosX = 0.1F;
  loadingIconPosY = -0.2F;
  loadingIconScale = 0.1F;
  loadingIconSpeed = 0.5F;
  idMem* memory = idMem::Singleton();
  void* allocation = memory->AllocWithLocation(
      "w:\\build\\tech5\\engine\\renderer\\RenderThread.cpp(88) : TAG_RENDERER",
      sizeof(idRender), TAG_RENDERER, false, ALIGN_128, HEAP_DEFAULTHEAP);
  frameState = allocation != nullptr ? ::new (allocation) idRender() : nullptr;
}

idRenderThread::~idRenderThread() = default;
