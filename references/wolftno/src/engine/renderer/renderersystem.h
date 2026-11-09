#pragma once

#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

struct idRenderFrameInfo;
class idRenderModel;
class idRenderVideoOverlay;
class idRenderWorld;
struct idThreadGraphicsContext;
struct idVideoDesc;
struct renderView_t;

enum renderFlags_t : int {
  RENDERFLAG_NONE = 0x000,
  RENDERFLAG_WAITFORINPUT = 0x001,
  RENDERFLAG_PRESSEDDURINGCUTSCENE = 0x002,
  RENDERFLAG_NEXTPRESSEXITS = 0x004,
  RENDERFLAG_CANEXITCUTSCENE = 0x008,
  RENDERFLAG_LOADCOMPLETE = 0x010,
  RENDERFLAG_BINKSTARTED = 0x020,
  RENDERFLAG_BINKEXIT = 0x040,
  RENDERFLAG_LOADING = 0x080,
  RENDERFLAG_BLOCKLOADINGICON = 0x100,
  RENDERFLAG_BLOCKLOADINGICONINTRO = 0x200,
  RENDERFLAG_BINKSTARTEDFROMMAIN = 0x400,
  RENDERFLAG_BINKEXITPRESSEDSYNC = 0x800,
  RENDERFLAG_RESET = 0x0FFFFFFF,
};

enum videoFlags_t : int {
  VIDEO_EXIT = 0,
  VIDEO_FREEZE = 1,
  VIDEO_LOOP = 2,
};

enum stereoRenderMode_t : int {
  STEREO_RENDER_OFF = 0,
  STEREO_RENDER_LEFT_AND_RIGHT = 1,
  STEREO_RENDER_TOP_AND_BOTTOM = 2,
};

// Retail idRenderSystem vtable order recovered from the PDB. Implementations
// remain permissive until their owning renderer-system CPP is reconstructed.
class idRenderSystem {
public:
  virtual ~idRenderSystem();
  static __forceinline void operator delete(void* pointer) noexcept {
    idMem::Singleton()->Free(pointer, ALIGN_16);
  }
  static __forceinline void operator delete(void* pointer,
                                             std::size_t) noexcept {
    operator delete(pointer);
  }
  virtual void Init() {}
  virtual void Init2() {}
  virtual void Shutdown() {}
  virtual idThreadGraphicsContext* AllocThreadGraphicsContext() { return nullptr; }
  virtual void DestroyThreadGraphicsContext(idThreadGraphicsContext*) {}
  virtual idRenderWorld* AllocRenderWorld(const char*, bool, bool) { return nullptr; }
  virtual void FreeRenderWorld(idRenderWorld*) {}
  virtual void RenderFrame(void* const, const idRenderFrameInfo&, bool, bool,
                           bool) {}
  virtual void EndFrame(void*, int, int, idRenderWorld*, idRenderModel**, int,
                        bool, bool, bool, int) {}
  virtual void ToolEndFrame(void*, int, int, idRenderWorld*,
                            const renderView_t&, idRenderModel*, int) {}
  virtual int FrameNumber() { return 0; }
  virtual bool SyncRenderThread(bool) { return false; }
  virtual void BeginBinkVideo(void*, const idVideoDesc*, videoFlags_t, void*,
                              int, float, idRenderVideoOverlay*, bool) {}
  virtual bool BinkVideoIsLoaded() { return false; }
  virtual void BinkVideoPause(bool) {}
  virtual bool BinkVideoIsPlaying() { return false; }
  virtual bool BinkVideoPlay() { return false; }
  virtual void FreeBinkVideo() {}
  virtual void SetRenderFlag(renderFlags_t) {}
  virtual void RemoveRenderFlag(int) {}
  virtual bool HasRenderFlag(int) { return false; }
  virtual bool CanThreadContinue() { return false; }
  virtual void RestartFromLoading() {}
  virtual void AllowBackgroundSwaps(bool) {}
  virtual void BeginAutomaticBackgroundSwaps(void*, bool, bool) {}
  virtual void EndAutomaticBackgroundSwaps() {}
  virtual bool IsAutomaticBackgroundSwapsEnabled() { return false; }
  virtual void SetLoadingIconInfo(float, float, float, float, bool) {}
  virtual void RegenerateReferences() {}
  virtual void InhibitEndFrameRendering(int) {}
  virtual bool IsEndFrameRenderingInhibited() { return false; }
  virtual int GetNumRenderWorlds() { return 0; }
  virtual idRenderWorld* RenderWorldForIndex(int) { return nullptr; }
  virtual int GetNumMonitors() { return 0; }
  virtual void GetMonitorSize(int, int*, int*) {}
  virtual int GetCurrentMonitor() { return 0; }
  virtual bool GPUTranscodeAvailable() { return false; }
  virtual void EnableGPUTranscode(bool) {}
  virtual bool GPUTranscodeIsEnabled() { return false; }
  virtual bool SwapTearAvailable() { return false; }
  virtual std::int64_t GetFreeVRamAtInit() { return 0; }
  virtual void ClearOutOfMemory() {}
  virtual bool CheckOutOfMemory() { return false; }
  virtual void* GetGameWindow() { return nullptr; }
  virtual void Restart() {}
  virtual void SwapBuffers(void*, bool) {}
  virtual void SetBlackLevel(float) {}
  virtual void SetGamma(std::uint16_t*, std::uint16_t*, std::uint16_t*) {}
  virtual bool MakeCurrent(void*) { return false; }
  virtual int GetWidth() { return 0; }
  virtual int GetHeight() { return 0; }
  virtual float GetPixelAspect() { return 1.0f; }
  virtual float GetScreenAspect() { return 1.0f; }
  virtual int GetScanoutWidth() { return 0; }
  virtual int GetScanoutHeight() { return 0; }
  virtual stereoRenderMode_t GetStereoRenderMode();
  virtual int GetNumMultisamples() { return 0; }
  virtual int GetMaxSupportedMultisamples() { return 0; }
  virtual bool GetUsingSMP() { return false; }
  virtual bool IgnoreSizeMessages() { return false; }
  virtual void WindowSizeDragged(int, int) {}
  virtual bool WindowChangeMultisamples(int, bool) { return false; }
  virtual bool EnumerateModes(int, int*, int*, int*) { return false; }
  virtual void (*ExtensionPointer(const char*))() { return nullptr; }
  virtual void SetPixelFormat(void*) {}
  virtual void ShowGameWindow() {}
  virtual void HideGameWindow() {}
  virtual bool IsFullscreen() { return false; }
  virtual void GoWindowed() {}
  virtual void GoFullScreen() {}
  virtual void SetStudioMode(bool) {}
  virtual void InitContext() {}
  virtual void InitCoreWindow(void*) {}
  virtual void SetSimpleGamma(float) {}
};

extern idRenderSystem* renderSystem;
