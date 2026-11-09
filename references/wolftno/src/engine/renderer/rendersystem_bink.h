#pragma once

#include "engine/renderer/renderersystem.h"
#include "external/bink/bink_sdk.h"
#include "shared/idlib/color.h"
#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

class idCVar;
class idImage;
class idImageOpts;
class idRenderModelGui;

struct idVideoDesc {
  idStrStatic<512> videoFile;
  idColor fadeFromColor;
};

class idBinkIOWrapper {
public:
  static int Open(BINKIO* binkIO, const char* name, std::uint32_t flags);

#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
public:
#else
private:
#endif
  struct Context {
    std::uint64_t size;
    std::uint64_t offset;
    class idFile* file;
  };
  static_assert(sizeof(Context) == 0x18, "Bink I/O context ABI drift");

  static std::uint32_t ReadHeader(BINKIO* binkIO, int offset,
                                  void* destination, std::uint32_t size);
  static std::uint32_t ReadFrame(BINKIO* binkIO, std::uint32_t frameNumber,
                                 int originalOffset, void* destination,
                                 std::uint32_t size);
  static void SetInfo(BINKIO* binkIO, void* buffer, std::uint32_t size,
                      std::uint32_t fileSize, std::uint32_t simulate);
  static void Close(BINKIO* binkIO);

  static constexpr std::uint32_t PREFETCH_SIZE = 0x20000;
};

class idBinkTimingWatcher : public idSysThread {
public:
  idBinkTimingWatcher();
  static void operator delete(void* pointer) noexcept;
  static void operator delete(void* pointer, std::size_t) noexcept;
  int Run() override;
  void StepFrame(int frame) {
    curFrame = frame;
    frameEv.Raise();
  }

  int curFrame;
  std::uint32_t recoveredAlignment;
  idSysSignal frameEv;
};

#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
void* binkMemAlloc(std::uint32_t bytes);
void binkMemFree(void* pointer);
#endif
std::uint32_t bink_percentage_full(BINK* movie);
void FillBinkBuffer(volatile bool& cancelToTerminate);

extern idCVar bink_skip;
extern idCVar bink_dontfree;
extern idCVar bink_debug;
extern idCVar bink_ioMegs;
extern idCVar bink_gamePumpIO;
extern idCVar bink_videoBasePath;
extern idCVar bink_threaded;
extern idCVar bink_debugPreLoadCompleteFile;
extern idCVar bink_useSFCForNetwork;

#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
namespace RenderSystemBinkTestAccess {
struct Hooks {
  idImage* (*scratchImage)(const char* name, const idImageOpts& options){};
  void (*destroyImage)(idImage* image){};
  idRenderModelGui* (*allocateModel)(){};
  void (*setModelName)(idRenderModelGui* model, const char* name){};
  void (*destroyModel)(idRenderModelGui* model){};
  idBinkTimingWatcher* (*allocateWatcher)(){};
  void (*destroyWatcher)(idBinkTimingWatcher* watcher){};
};

void SetHooks(const Hooks& hooks);
void ResetState();
void SetMovie(BINK* movie);
BINK* GetMovie();
void SetDirectlyIntoTextures(bool direct);
void SetUsingStreamCache(bool useStreamCache);
void SetLocation(int device);
void SetImages(idImage* const y[3], idImage* const cr[3],
               idImage* const cb[3]);
idImage* GetYImage(int index);
int GetCurrentBufferIndex();
const BINKREALTIME& GetRealtime();
const BINKSUMMARY& GetSummary();
const BINKFRAMEBUFFERS& GetFrameBuffers();
bool GetSetupOnce();
int GetSlowFrames();
int GetStalledTime();
int GetTotalFrames();
std::uint64_t GetLastFrameTime();
idBinkTimingWatcher* GetWatcher();
} // namespace RenderSystemBinkTestAccess
#endif

static_assert(sizeof(idVideoDesc) == 0x238, "video description ABI drift");
static_assert(offsetof(idVideoDesc, fadeFromColor) == 0x228,
              "video description fade offset drift");
static_assert(offsetof(idBinkTimingWatcher, curFrame) == 0x58,
              "Bink timing-watcher frame offset drift");
static_assert(offsetof(idBinkTimingWatcher, frameEv) == 0x60,
              "Bink timing-watcher signal offset drift");
static_assert(sizeof(idBinkTimingWatcher) == 0x68,
              "Bink timing-watcher ABI drift");
