#pragma once

// NEEDS_WORK(engine/renderer/renderthread): PDB-exact layout boundary used by
// RenderSystem_bink. The render-thread behavior remains owned by
// renderer/renderthread.cpp and will be reconstructed with that file.

#include "shared/idlib/parallelism/thread.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/rendersystem_runtime.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

enum renderMode_t : int {
  RENDER_MODE_MANUAL = 0,
  RENDER_MODE_AUTOMATIC = 1,
  RENDER_MODE_SYNC = 2,
  RENDER_MODE_FORCE_EXIT = 3,
  RENDER_MODE_BINK = 4,
};

class idRenderThread : public idSysThread {
public:
  idRenderThread();
  ~idRenderThread() override;
  int Run() override;
  void RenderFrameInternal(renderMode_t mode, void* deviceContext,
                           bool swapBuffers, bool forceVSync);
  void SetLoadingIconInfo(float posX, float posY, float scale, float speed);

  static void operator delete(void* pointer) noexcept {
    if (pointer != nullptr)
      idMem::Singleton()->Free(pointer, ALIGN_16);
  }

#if defined(WOLF_RENDERER_RENDERTHREAD_TEST_ACCESS)
public:
#else
private:
#endif
  void RenderLoadingIcon(float fracX, float fracY, float size);
  void RenderSavingIcon();
  void RenderAutomatic(void* deviceContext);
  void RenderVideo();
  void RenderManual(void* deviceContext, bool resolveDepth);
  void RenderBink(void* deviceContext);

public:

  void* hdc;
  renderMode_t renderMode;
  renderMode_t renderModeBeforeSync;
  idRenderFrameInfoCommitted committedRenderFrameInfo;
  void* frameState;
  wrapperStats_t wrapperStatsFull;
  wrapperStats_t wrapperStatsExcludingFullScreenGuis;
  bool initialized;
  bool renderLoadingIcon;
  bool renderOldFrameInAuto;
  bool deferredVidRestart;
  float loadingIconPosX;
  float loadingIconPosY;
  float loadingIconScale;
  float loadingIconSpeed;
  std::uint32_t recoveredTailPadding;
};

static_assert(sizeof(wrapperStats_t) == 36, "wrapper stats ABI drift");
static_assert(offsetof(idRenderThread, hdc) == 0x58,
              "render thread HDC offset drift");
static_assert(offsetof(idRenderThread, renderMode) == 0x60,
              "render thread mode offset drift");
static_assert(offsetof(idRenderThread, renderModeBeforeSync) == 0x64,
              "render thread sync-mode offset drift");
static_assert(offsetof(idRenderThread, frameState) == 0x2E0,
              "render thread frame-state offset drift");
static_assert(offsetof(idRenderThread, renderLoadingIcon) == 0x331,
              "render thread loading-icon offset drift");
static_assert(sizeof(idRenderThread) == 0x348,
              "render thread ABI drift");

void ResizeImages(const idScreenRect* screenRect);
void UpdateGraphicsAPIWrapperConfig();

#if defined(WOLF_RENDERER_RENDERTHREAD_TEST_ACCESS)
class idDeclRenderParm;
class idDeclRenderProg;
class idMaterial;

struct renderThreadTestResources_t {
  const idDeclRenderParm* viewColorMap;
  const idDeclRenderParm* loadingIconParms;
  const idDeclRenderParm* loadingIcon;
  const idDeclRenderParm* savingIcon;
  const idDeclRenderParm* transMap;
  const idDeclRenderProg* guiBlend;
  const idDeclRenderProg* guiLoading;
  const idDeclRenderProg* viewColorIdentity;
  const idDeclRenderParm* gammaForBrightness;
  const idMaterial* binkMaterial;
  const idDeclRenderProg* binkProgram;
};

void SetRenderThreadTestResources(const renderThreadTestResources_t& resources);
#endif
