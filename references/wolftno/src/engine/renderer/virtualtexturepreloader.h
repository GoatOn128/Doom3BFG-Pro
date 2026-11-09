#pragma once

#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

class idMaterial;
class idVirtualTexture;
struct preloadMaterial_t;

enum preloadType_t : int {
  PRELOAD_INVALID = 0,
  PRELOAD_ALL = 1,
  PRELOAD_MATERIAL_LIST = 2,
};

struct preloadTextureRect_t {
  std::uint16_t x1;
  std::uint16_t y1;
  std::uint16_t x2;
  std::uint16_t y2;
};

#pragma pack(push, 4)
struct preloadCommand_t {
  int handle;
  preloadType_t type;
  idVirtualTexture* texture;
  preloadTextureRect_t textureRects[1024];
  std::uint8_t textureMinLod[1024];
  int numTextureRects;
  int minLod;
  int totalTimeMSec;
  int pagesLoaded[16];
  volatile bool done;
};
#pragma pack(pop)

struct idTmpPreloadMaterial_t {
  const idMaterial* material{};
  int minLod{2};
};

#pragma pack(push, 4)
class idVirtualTexturePreloadThread : public idSysThread {
public:
  idVirtualTexturePreloadThread();
  int Run() override;

  int AddCommand(preloadType_t type,
                 const preloadMaterial_t* const materials,
                 int numMaterials, int minLod);
  void StopPreloading();

  static void operator delete(void* pointer) noexcept {
    if (pointer != nullptr) {
      idMem::Singleton()->Free(pointer, ALIGN_16);
    }
  }

  preloadCommand_t preloadCommands[8];
  preloadCommand_t* preloadCommandQueue[16];
  volatile int preloadCommandQueueStart;
  volatile int preloadCommandQueueEnd;
  int currentHandle;
  volatile bool endCalled;
  volatile bool cancelToTerminate;

private:
  friend class idVirtualTexturePreloader;
  preloadCommand_t* AllocCommand(preloadType_t type, int minLod,
                                 idVirtualTexture* virtualTexture);
  void PrintCommandResult(const preloadCommand_t& command) const;
};
#pragma pack(pop)

class alignas(8) idVirtualTexturePreloader {
public:
  idVirtualTexturePreloader() = default;
  ~idVirtualTexturePreloader() { DestroyThread(); }

  void SpawnThread();
  void DestroyThread();
  void BeginPreloading();
  void EndPreloading();
  bool MaterialPreloadIsDone(int handle);
  int PreloadVMTRsForMap(const char* const mapName, const char* spawnSpot);

  idVirtualTexturePreloadThread* thread{};
  bool running{};
};

extern idVirtualTexturePreloader virtualTexturePreloader;

static_assert(sizeof(preloadTextureRect_t) == 0x8,
              "preload texture rectangle ABI drift");
static_assert(offsetof(preloadCommand_t, textureRects) == 0x10,
              "preload texture-rectangle offset drift");
static_assert(offsetof(preloadCommand_t, textureMinLod) == 0x2010,
              "preload texture min-LOD offset drift");
static_assert(offsetof(preloadCommand_t, numTextureRects) == 0x2410,
              "preload texture-rectangle count offset drift");
static_assert(offsetof(preloadCommand_t, pagesLoaded) == 0x241C,
              "preload page-count offset drift");
static_assert(offsetof(preloadCommand_t, done) == 0x245C,
              "preload completion offset drift");
static_assert(sizeof(preloadCommand_t) == 0x2460,
              "preload command ABI drift");
static_assert(alignof(preloadCommand_t) == 4,
              "preload command alignment drift");
static_assert(sizeof(idTmpPreloadMaterial_t) == 0x10,
              "temporary preload material ABI drift");
static_assert(offsetof(idVirtualTexturePreloadThread, preloadCommands) == 0x58,
              "preload command array offset drift");
static_assert(offsetof(idVirtualTexturePreloadThread, preloadCommandQueue) ==
                  0x12358,
              "preload queue offset drift");
static_assert(offsetof(idVirtualTexturePreloadThread,
                       preloadCommandQueueStart) == 0x123D8,
              "preload queue-start offset drift");
static_assert(offsetof(idVirtualTexturePreloadThread, currentHandle) == 0x123E0,
              "preload handle offset drift");
static_assert(offsetof(idVirtualTexturePreloadThread, endCalled) == 0x123E4,
              "preload end flag offset drift");
static_assert(offsetof(idVirtualTexturePreloadThread, cancelToTerminate) ==
                  0x123E5,
              "preload cancel flag offset drift");
static_assert(sizeof(idVirtualTexturePreloadThread) == 0x123E8,
              "virtual texture preload thread ABI drift");
static_assert(alignof(idVirtualTexturePreloadThread) == 4,
              "virtual texture preload thread alignment drift");
static_assert(offsetof(idVirtualTexturePreloader, running) == 0x8,
              "virtual texture preloader running offset drift");
static_assert(sizeof(idVirtualTexturePreloader) == 0x10,
              "virtual texture preloader ABI drift");
