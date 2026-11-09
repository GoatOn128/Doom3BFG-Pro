#include "engine/renderer/renderlocal.h"

#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/renderthread.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <new>

#if defined(WOLF_RENDERER_RENDERSYSTEM_THREAD_TEST_ACCESS)
extern idRenderSystemLocal* renderThreadTestGlobal;
#endif

__forceinline void* operator new(std::size_t size, memTag_t tag) noexcept {
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag", size, tag,
      false, ALIGN_16, HEAP_DEFAULTHEAP);
}

__forceinline void operator delete(void* pointer, memTag_t) noexcept {
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

void idRenderSystemLocal::RestartRenderThread(void* hdc) {
  if (!renderSynced) {
    idLib::Printf("RestartRenderThread called and renderer is not synced\n");
    return;
  }

  if (hdc != nullptr) {
    renderThread->hdc = hdc;
  }
  renderThread->renderMode = renderThread->renderModeBeforeSync;

  if (renderSystem->GetUsingSMP()) {
    renderSynced = false;
    renderThread->SignalWork();
    return;
  }

  renderSystem->MakeCurrent(hdc);
  switch (renderThread->renderMode) {
    case RENDER_MODE_AUTOMATIC:
      renderThread->RenderFrameInternal(RENDER_MODE_AUTOMATIC, hdc, false,
                                        false);
      break;
    case RENDER_MODE_BINK:
    case RENDER_MODE_FORCE_EXIT:
#if defined(WOLF_RENDERER_RENDERSYSTEM_THREAD_TEST_ACCESS)
      if (renderThreadTestGlobal->WaitForNextVideoFrame(false)) {
#else
      if (tr.WaitForNextVideoFrame(false)) {
#endif
        renderThread->RenderFrameInternal(renderThread->renderMode, hdc,
                                          false, false);
      }
      break;
    case RENDER_MODE_MANUAL:
    case RENDER_MODE_SYNC:
      break;
  }
}

void idRenderSystemLocal::AllowBackgroundSwaps(bool allow) {
  allowBackgroundSwaps = allow;
}

void idRenderSystemLocal::StartRenderThread() {
  renderThread = new (TAG_RENDERER) idRenderThread();
  renderThread->StartWorkerThread("Renderer", CORE_0B, THREAD_HIGHEST,
                                  0x40000);
}

bool idRenderSystemLocal::SyncRenderThread(bool syncGPU) {
  if (renderThread == nullptr) {
    return false;
  }

  if (syncGPU && renderThread->renderMode != RENDER_MODE_FORCE_EXIT &&
      GetUsingSMP()) {
    const renderMode_t previousMode = renderThread->renderMode;
    if (previousMode == RENDER_MODE_AUTOMATIC) {
      renderThread->renderMode = RENDER_MODE_MANUAL;
    }
    if (!renderSynced) {
      renderThread->WaitForThread();
      renderSynced = true;
    }
    renderThread->renderModeBeforeSync = RENDER_MODE_SYNC;
    RestartRenderThread(nullptr);
    renderThread->WaitForThread();
    renderThread->renderModeBeforeSync = previousMode;
    renderThread->renderMode = previousMode;
    GL_Finish();
    renderSynced = true;
    return true;
  }

  renderThread->renderModeBeforeSync = renderThread->renderMode;
  if (renderThread->renderMode == RENDER_MODE_AUTOMATIC) {
    renderThread->renderMode = RENDER_MODE_MANUAL;
  }
  if (renderSynced) {
    if (syncGPU) {
      GL_Finish();
    }
    return false;
  }
  if (renderThread->renderMode != RENDER_MODE_FORCE_EXIT) {
    renderThread->WaitForThread();
  }
  renderSynced = true;
  if (syncGPU) {
    GL_Finish();
  }
  return true;
}

void idRenderSystemLocal::BeginAutomaticBackgroundSwaps(
    void* hdc, bool showLoadingIcon, bool showOldFrame) {
  if (idLib::headless || !allowBackgroundSwaps ||
      renderThread->renderMode == RENDER_MODE_AUTOMATIC) {
    return;
  }

  SyncRenderThread(false);
  automaticBackgroundSwapsEnabled = true;
  renderThread->renderLoadingIcon =
      showLoadingIcon &&
      !HasRenderFlag(RENDERFLAG_LOADCOMPLETE |
                     RENDERFLAG_BLOCKLOADINGICON |
                     RENDERFLAG_BLOCKLOADINGICONINTRO);
  renderThread->renderOldFrameInAuto = showOldFrame;
  renderThread->renderModeBeforeSync = RENDER_MODE_AUTOMATIC;
  RestartRenderThread(hdc);
}

void idRenderSystemLocal::EndAutomaticBackgroundSwaps() {
  if (idLib::headless) {
    return;
  }

  SyncRenderThread(false);
  automaticBackgroundSwapsEnabled = false;
  renderThread->renderLoadingIcon = false;
  renderThread->renderOldFrameInAuto = true;
  renderThread->renderModeBeforeSync = RENDER_MODE_MANUAL;
}

void idRenderSystemLocal::SetLoadingIconInfo(float posX, float posY,
                                             float scale, float speed,
                                             bool render) {
  renderThread->renderLoadingIcon = render;
  renderThread->SetLoadingIconInfo(posX, posY, scale, speed);
}
