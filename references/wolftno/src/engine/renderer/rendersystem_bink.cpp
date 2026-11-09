#include "engine/renderer/rendersystem_bink.h"

#include "engine/framework/coderesource.h"
#include "engine/framework/versionselector.h"
#include "engine/models/rendermodelgui.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/renderlocal.h"
#include "engine/renderer/renderthread.h"
#include "engine/renderer/rendervideooverlay.h"
#include "engine/renderer/texturesampler.h"
#include "engine/sys/sys_public.h"
#include "engine/sys/win32/win_main.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/filesystem/streamfilecache.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <new>

idCVar bink_skip("bink_skip", "0", CVAR_BOOL,
                 "do not load and play bink");
idCVar bink_dontfree(
    "bink_dontfree", "0", CVAR_BOOL | CVAR_INIT,
    "keep bink textures around since we need to be able to play videos whenever");
idCVar bink_debug("bink_debug", "0", CVAR_INTEGER, "dump information");
idCVar bink_ioMegs("bink_ioMegs", "32", CVAR_INTEGER,
                   "larger buffers for less seeks during load");
idCVar bink_gamePumpIO(
    "bink_gamePumpIO", "1", CVAR_BOOL,
    "if game should bump bink io (to make sure it runs in our slightly inflated prio-mess");
idCVar bink_videoBasePath(
    "bink_videoBasePath", "", CVAR_ALL,
    "use separate base path for videos (to share video data between builds)");
idCVar bink_threaded("bink_threaded", "1", CVAR_BOOL,
                     "use two threads for bink decode");
idCVar bink_debugPreLoadCompleteFile(
    "bink_debugPreLoadCompleteFile", "0", CVAR_BOOL,
    "read complete file to a memory before playback start (using bink option BINKPRELOADALL)");
idCVar bink_useSFCForNetwork(
    "bink_useSFCForNetwork", "1", CVAR_BOOL,
    "use the stream file cache when reading videos over network (to reduce the number of reads)");

namespace {
constexpr std::uint32_t BINK_OPEN_BASE_FLAGS = 0x01004000u;
constexpr std::uint32_t BINK_OPEN_DIRECT_TEXTURES = 0x00000400u;
constexpr std::uint32_t BINK_OPEN_PRELOAD_ALL = 0x00002000u;
constexpr std::uint32_t BINK_OPEN_USER_IO = 0x02000000u;
constexpr std::uint32_t BINK_BACKGROUND_SUSPEND = 0x80000001u;
constexpr std::uint32_t BINK_BACKGROUND_RESUME = 0x80000002u;
constexpr std::uint32_t BINK_BACKGROUND_ENABLE = 2u;

BINK* bink = nullptr;
videoFlags_t videoFlags = VIDEO_EXIT;
BINKREALTIME binkRealTime{};
BINKSUMMARY binkSummary{};
BINKFRAMEBUFFERS bfb{};
idImage* binkY[3]{};
idImage* binkCr[3]{};
idImage* binkCb[3]{};
bool binkSetupOnce = false;
bool binkDirectlyIntoTextures = false;
bool binkUsingSFCForNetworkIO = false;
fsDevice_t binkLocation = FS_DEVICE_HARD_DISK_DRIVE;
int currentBufferIndex = 0;
int binkThreadId0 = 0;
int binkThreadId1 = 1;
int slowFrames = 0;
int stalledTime = 0;
int totalFrames = 0;
int skipped = 0;
std::uint64_t lastFrameTime = 0;
idBinkTimingWatcher* watcherTread = nullptr;
idSysMutex binkMutex;
idColor binkVideoFadeFromColor{-1.0F, -1.0F, -1.0F, -1.0F};

idCodeResource<idMaterial> mtrBink("_bink");
idCodeResource<idDeclRenderParm> rpBinkY("binkY");
idCodeResource<idDeclRenderParm> rpBinkCr("binkCr");
idCodeResource<idDeclRenderParm> rpBinkCb("binkCb");

#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
RenderSystemBinkTestAccess::Hooks testHooks{};
#endif

std::uint32_t NullGetBufferSize(BINKIO*, std::uint32_t) { return 0; }
std::uint32_t NullIdle(BINKIO*) { return 0; }
int NullBackgroundControl(BINKIO*, std::uint32_t) { return 0; }

void StopTimingWatcher() {
  if (watcherTread == nullptr)
    return;
  watcherTread->StopThread(false);
  watcherTread->curFrame = -1;
  watcherTread->frameEv.Raise();
  watcherTread->StopThread(true);
#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
  if (testHooks.destroyWatcher != nullptr)
    testHooks.destroyWatcher(watcherTread);
#else
    watcherTread->~idBinkTimingWatcher();
    idMem::Singleton()->Free(watcherTread, ALIGN_16);
#endif
  watcherTread = nullptr;
}

void DestroyGuiModel(idRenderModelGui*& model) {
  if (model == nullptr)
    return;
#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
  if (testHooks.destroyModel != nullptr)
    testHooks.destroyModel(model);
#else
    model->~idRenderModelGui();
    idMem::Singleton()->Free(model, ALIGN_16);
#endif
  model = nullptr;
}

void DestroyImage(idImage*& image) {
  if (image == nullptr)
    return;
#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
  if (testHooks.destroyImage != nullptr)
    testHooks.destroyImage(image);
#else
    image->~idImage();
    idMem::Singleton()->Free(image, ALIGN_16);
#endif
  image = nullptr;
}

void ResetBinkSamplersToDefault() {
  const auto* defaultSampler = reinterpret_cast<const idTextureSampler*>(
      globalImages != nullptr ? globalImages->defaultImage : nullptr);
  renderThreadParmState->SetParmSampler(rpBinkY.Get(), defaultSampler);
  renderThreadParmState->SetParmSampler(rpBinkCr.Get(), defaultSampler);
  renderThreadParmState->SetParmSampler(rpBinkCb.Get(), defaultSampler);
}

int LanguageTrackOffset() {
  const char* const language = sys_lang.GetString();
  if (idStr::Icmp(language, "english") == 0)
    return 0;
  if (idStr::Icmp(language, "french") == 0)
    return 1;
  if (idStr::Icmp(language, "german") == 0)
    return 2;
  if (idStr::Icmp(language, "italian") == 0)
    return 3;
  if (idStr::Icmp(language, "japanese") == 0)
    return 4;
  if (idStr::Icmp(language, "spanish") == 0)
    return 7;
  idLib::Warning("Unknown language");
  return 0;
}

int BinkVolumeFromDecibels(float decibels) {
  const int volume = static_cast<int>(
      std::exp(decibels * 0.11512925F) * 32768.0F);
  return std::clamp(volume, 0, 0x8000);
}

void ConfigureBinkAudio(BINK* movie, int languageOffset, int speakerMask,
                        int volume) {
  std::uint32_t primaryTrack = static_cast<std::uint32_t>(languageOffset);
  std::uint32_t surroundTrack = static_cast<std::uint32_t>(languageOffset + 1);
  std::uint32_t lfeTrack = static_cast<std::uint32_t>(languageOffset + 2);
  std::uint32_t centerTrack = static_cast<std::uint32_t>(languageOffset + 3);
  if (languageOffset == 0) {
    primaryTrack = 11;
    surroundTrack = 12;
    lfeTrack = 13;
    centerTrack = 14;
  }

  if (movie->NumTracks == 1) {
    BinkSetVolume(movie, primaryTrack, static_cast<std::uint32_t>(volume));
    return;
  }

  std::uint32_t bins[4]{0, 1, 0, 0};
  int volumes[4]{volume, volume, 0, 0};
  BinkSetSpeakerVolumes(movie, primaryTrack, bins, volumes, 2);

  int nextBin = 2;
  if ((speakerMask & 0x4) != 0) {
    bins[0] = 2;
    volumes[0] = volume;
    nextBin = 3;
    BinkSetSpeakerVolumes(movie, centerTrack, bins, volumes, 1);
  } else {
    bins[0] = 0;
    bins[1] = 1;
    volumes[0] = 7071 * volume / 10000;
    volumes[1] = volumes[0];
    BinkSetSpeakerVolumes(movie, centerTrack, bins, volumes, 2);
  }

  if ((speakerMask & 0x8) != 0) {
    bins[0] = static_cast<std::uint32_t>(nextBin++);
    volumes[0] = volume;
  } else {
    bins[0] = 0;
    volumes[0] = 0;
  }
  BinkSetSpeakerVolumes(movie, lfeTrack, bins, volumes, 1);

  if ((speakerMask & 0x30) == 0 && (speakerMask & 0x600) == 0) {
    bins[0] = 0;
    bins[1] = 1;
    bins[2] = 1;
    bins[3] = 0;
    volumes[0] = 8717 * volume / 10000;
    volumes[1] = volumes[0];
    volumes[2] = -4899 * volume / 10000;
    volumes[3] = volumes[2];
    BinkSetSpeakerVolumes(movie, surroundTrack, bins, volumes, 4);
    return;
  }

  if ((speakerMask & 0x30) == 0) {
    if ((speakerMask & 0x40) != 0)
      ++nextBin;
    if ((speakerMask & 0x80) != 0)
      ++nextBin;
    if ((speakerMask & 0x100) != 0)
      ++nextBin;
  }
  bins[0] = static_cast<std::uint32_t>(nextBin);
  bins[1] = static_cast<std::uint32_t>(nextBin + 1);
  volumes[0] = volume;
  volumes[1] = volume;
  BinkSetSpeakerVolumes(movie, surroundTrack, bins, volumes, 2);
}

idImageOpts MakeBinkImageOptions(std::uint32_t width, std::uint32_t height) {
  idImageOpts options{};
  options.textureType = TT_2D;
  options.format = FMT_LUM8;
  options.colorFormat = CFM_DEFAULT;
  options.width = static_cast<int>(width);
  options.height = static_cast<int>(height);
  options.depth = 1;
  options.numLevels = 0;
  options.numSamples = 0;
  options.packedTail = true;
  options.readback = true;
  options.linear = true;
  options.textureBindType = TB_SHADER;
  options.textureUsage = TU_DEFAULT;
  options.textureOptions = TO_NONE;
  options.sampler.filter = TF_DEFAULT;
  options.sampler.repeat = TR_CLAMP;
  options.sampler.aniso = 13.0F;
  return options;
}

void AllocateBinkImages() {
  for (int index = 0; index < 3; ++index) {
    idStr name;
    idImageOpts options =
        MakeBinkImageOptions(bfb.YABufferWidth, bfb.YABufferHeight);
    name.Format("_binkY%i", index);
#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
    if (testHooks.scratchImage != nullptr)
      binkY[index] = testHooks.scratchImage(name.data, options);
#else
      idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
      binkY[index] = globalImages->ScratchImage(name.data, options);
#endif
    options.width = static_cast<int>(bfb.cRcBBufferWidth);
    options.height = static_cast<int>(bfb.cRcBBufferHeight);
    name.Format("_binkCr%i", index);
#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
    if (testHooks.scratchImage != nullptr)
      binkCr[index] = testHooks.scratchImage(name.data, options);
#else
    binkCr[index] = globalImages->ScratchImage(name.data, options);
#endif
    name.Format("_binkCb%i", index);
#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
    if (testHooks.scratchImage != nullptr)
      binkCb[index] = testHooks.scratchImage(name.data, options);
#else
    binkCb[index] = globalImages->ScratchImage(name.data, options);
    idMem::Singleton()->PopHeap();
#endif
  }
}

void RegisterDirectFrameBuffers() {
  for (int frameIndex = 0; frameIndex < 2; ++frameIndex) {
    BINKFRAMEPLANESET& frame = bfb.Frames[frameIndex];
    frame.YPlane.Buffer = binkY[frameIndex]->buffers[0];
    frame.YPlane.BufferPitch =
        static_cast<std::uint32_t>(binkY[frameIndex]->pitches[0]);
    frame.cRPlane.Buffer = binkCr[frameIndex]->buffers[0];
    frame.cRPlane.BufferPitch =
        static_cast<std::uint32_t>(binkCr[frameIndex]->pitches[0]);
    frame.cBPlane.Buffer = binkCb[frameIndex]->buffers[0];
    frame.cBPlane.BufferPitch =
        static_cast<std::uint32_t>(binkCb[frameIndex]->pitches[0]);

    for (std::uint32_t y = 0; y < bfb.YABufferHeight; ++y) {
      auto* row = static_cast<std::uint8_t*>(frame.YPlane.Buffer) +
                  y * frame.YPlane.BufferPitch;
      for (std::uint32_t x = 0; x < bfb.YABufferWidth; ++x)
        row[x] = static_cast<std::uint8_t>(x);
    }
    for (std::uint32_t y = 0; y < bfb.cRcBBufferHeight; ++y) {
      auto* crRow = static_cast<std::uint8_t*>(frame.cRPlane.Buffer) +
                    y * frame.cRPlane.BufferPitch;
      auto* cbRow = static_cast<std::uint8_t*>(frame.cBPlane.Buffer) +
                    y * frame.cBPlane.BufferPitch;
      for (std::uint32_t x = 0; x < bfb.cRcBBufferWidth; ++x) {
        crRow[x] = static_cast<std::uint8_t>(x);
        cbRow[x] = static_cast<std::uint8_t>(x);
      }
    }
  }
  BinkRegisterFrameBuffers(bink, &bfb);
}

idRenderModelGui* AllocateVideoOverlayModel() {
#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
  if (testHooks.allocateModel != nullptr)
    return testHooks.allocateModel();
  return nullptr;
#else
  idMem* const memory = idMem::Singleton();
  void* const storage = memory->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idRenderModelGui), TAG_GUI_MODEL, false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  return storage != nullptr ? new (storage) idRenderModelGui : nullptr;
#endif
}

idBinkTimingWatcher* AllocateTimingWatcher() {
#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
  if (testHooks.allocateWatcher != nullptr)
    return testHooks.allocateWatcher();
  return nullptr;
#else
  idMem* const memory = idMem::Singleton();
  void* const storage = memory->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW",
      sizeof(idBinkTimingWatcher), TAG_NEW, false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  return storage != nullptr ? new (storage) idBinkTimingWatcher : nullptr;
#endif
}
} // namespace

#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
#define WOLF_BINK_LOCAL
#else
#define WOLF_BINK_LOCAL static
#endif

WOLF_BINK_LOCAL void* binkMemAlloc(std::uint32_t bytes) {
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\renderer\\RenderSystem_bink.cpp(118) : TAG_BINK",
      bytes, TAG_BINK, false, ALIGN_16, HEAP_DEFAULTHEAP);
}

WOLF_BINK_LOCAL void binkMemFree(void* pointer) {
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

#undef WOLF_BINK_LOCAL

void idBinkIOWrapper::SetInfo(BINKIO* binkIO, void*, std::uint32_t,
                              std::uint32_t fileSize, std::uint32_t) {
  Context& context = *reinterpret_cast<Context*>(binkIO->iodata);
  if (context.size > fileSize)
    context.size = fileSize;
}

std::uint32_t idBinkIOWrapper::ReadHeader(BINKIO* binkIO, int offset,
                                          void* destination,
                                          std::uint32_t size) {
  Context& context = *reinterpret_cast<Context*>(binkIO->iodata);
  const std::int64_t readOffset =
      offset == -1 ? static_cast<std::int64_t>(context.offset) : offset;
  std::size_t bytesRead = 0;
  if (bink_useSFCForNetwork.GetInteger() != 0) {
    if (readOffset == 0 && context.file->Length() == 0)
      return 0;
    bytesRead = static_cast<std::size_t>(streamFileCache->ReadCachedData(
        context.file, readOffset, static_cast<int>(size), destination, true,
        0x70000000, 0, PFC_SOUND));
  } else {
    bytesRead = context.file->ReadOfs(readOffset, destination, size);
  }
  if (binkIO->flipendian_callback != nullptr)
    binkIO->flipendian_callback(destination, size);
  context.offset = static_cast<std::uint64_t>(readOffset + bytesRead);
  return static_cast<std::uint32_t>(bytesRead);
}

std::uint32_t idBinkIOWrapper::ReadFrame(BINKIO* binkIO, std::uint32_t,
                                         int originalOffset,
                                         void* destination,
                                         std::uint32_t size) {
  Context& context = *reinterpret_cast<Context*>(binkIO->iodata);
  const std::int64_t readOffset = originalOffset == -1
                                      ? static_cast<std::int64_t>(context.offset)
                                      : originalOffset;
  std::size_t bytesRead = 0;
  if (bink_useSFCForNetwork.GetInteger() != 0) {
    bytesRead = static_cast<std::size_t>(streamFileCache->ReadCachedData(
        context.file, readOffset, static_cast<int>(size), destination, true,
        0x7FFFFFFF, 1, PFC_SOUND));
    const std::uint64_t nextOffset =
        static_cast<std::uint64_t>(readOffset + bytesRead);
    const std::uint64_t remaining = context.size - nextOffset;
    const std::uint32_t prefetch = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(PREFETCH_SIZE, remaining));
    if (prefetch != 0) {
      streamFileCache->ReadCachedData(
          context.file, static_cast<std::int64_t>(nextOffset),
          static_cast<int>(prefetch), nullptr, false, 0x7FFFFFF0, 1,
          PFC_SOUND);
    }
  } else {
    bytesRead = context.file->ReadOfs(readOffset, destination, size);
  }
  if (binkIO->flipendian_callback != nullptr)
    binkIO->flipendian_callback(destination, size);
  context.offset = static_cast<std::uint64_t>(readOffset + bytesRead);
  return static_cast<std::uint32_t>(bytesRead);
}

void idBinkIOWrapper::Close(BINKIO* binkIO) {
  Context& context = *reinterpret_cast<Context*>(binkIO->iodata);
  if (bink_useSFCForNetwork.GetInteger() != 0) {
    const std::uint64_t remaining = context.size - context.offset;
    const std::uint32_t prefetch = static_cast<std::uint32_t>(
        std::min<std::uint64_t>(PREFETCH_SIZE, remaining));
    if (prefetch != 0) {
      streamFileCache->ReadCachedData(
          context.file, static_cast<std::int64_t>(context.offset),
          static_cast<int>(prefetch), nullptr, true, 0x70000000, 1,
          PFC_SOUND);
    }
  }
  delete context.file;
}

int idBinkIOWrapper::Open(BINKIO* binkIO, const char* name,
                          std::uint32_t) {
  idLib::Printf("idBinkIOWrapper::Open: %s\n", name);
  binkIO->ReadFrame = &ReadFrame;
  binkIO->ReadHeader = &ReadHeader;
  binkIO->GetBufferSize = &NullGetBufferSize;
  binkIO->SetInfo = &SetInfo;
  binkIO->Idle = &NullIdle;
  binkIO->Close = &Close;
  binkIO->BGControl = &NullBackgroundControl;

  idFile* const file = fileSystem->OpenFileRead(name, true, false);
  if (file == nullptr) {
    idLib::Warning("idBinkIOWrapper::Open could not open %s", name);
    return -1;
  }
  Context& context = *reinterpret_cast<Context*>(binkIO->iodata);
  context.file = file;
  context.offset = 0;
  context.size = static_cast<std::uint64_t>(file->Length());
  return 1;
}

bool idRenderSystemLocal::BinkVideoIsLoaded() { return bink != nullptr; }

void idRenderSystemLocal::BinkVideoPause(bool pause) {
  if (bink == nullptr) {
    renderThread->renderMode = RENDER_MODE_MANUAL;
    videoPlaying = false;
    return;
  }
  if (pause) {
    renderThread->renderMode = RENDER_MODE_MANUAL;
    videoPlaying = false;
    BinkPause(bink, 1);
  } else {
    renderThread->renderMode = RENDER_MODE_BINK;
    videoPlaying = true;
    BinkPause(bink, 0);
  }
}

bool idRenderSystemLocal::BinkVideoIsPlaying() {
  return renderThread->renderMode == RENDER_MODE_BINK ||
         renderThread->renderMode == RENDER_MODE_FORCE_EXIT;
}

bool idRenderSystemLocal::CanThreadContinue() {
  return renderThread->renderMode != RENDER_MODE_BINK &&
         renderThread->renderMode != RENDER_MODE_FORCE_EXIT &&
         !HasRenderFlag(RENDERFLAG_WAITFORINPUT);
}

bool idRenderSystemLocal::WaitForNextVideoFrame(bool isRenderThread) {
  if (bink == nullptr ||
      (bink->FrameNum == bink->Frames && videoFlags == VIDEO_EXIT))
    return false;
  do {
    if (isRenderThread)
      Sys_PumpRenderThreadEventsIfRequested();
    else
      Sys_PumpEvents();
    Sys_Sleep(1);
  } while (BinkVideoIsPlaying() && BinkWait(bink) != 0);
  return true;
}

std::uint32_t bink_percentage_full(BINK* movie) {
  BINKREALTIME realtime{};
  BinkGetRealtime(movie, &realtime, 0);
  return realtime.ReadBufferSize != 0
             ? 100u * realtime.ReadBufferUsed / realtime.ReadBufferSize
             : 100u;
}

unsigned idRenderSystemLocal::GetBinkWidth() const {
  return bink != nullptr ? bink->Width : 0u;
}

unsigned idRenderSystemLocal::GetBinkHeight() const {
  return bink != nullptr ? bink->Height : 0u;
}

float idRenderSystemLocal::GetBinkTime() const {
  if (bink == nullptr || binkRealTime.FrameNum == 0xFFFFFFFFu)
    return 0.0F;
  return static_cast<float>(static_cast<int>(binkRealTime.FrameNum)) /
         (static_cast<float>(static_cast<int>(binkRealTime.FrameRate)) /
          static_cast<float>(static_cast<int>(binkRealTime.FrameRateDiv)));
}

idScopeThreadAffinity::~idScopeThreadAffinity() noexcept(false) {
  Sys_SetCurrentThreadPlaformAffinity(oldAffinity);
}

idScopeThreadPriorityAndAffinity::~idScopeThreadPriorityAndAffinity()
    noexcept(false) = default;

bool idRenderSystemLocal::BinkVideoPlay() {
  if (bink == nullptr ||
      (bink->FrameNum == bink->Frames && videoFlags == VIDEO_EXIT))
    return false;
  do {
    Sys_PumpEvents();
    Sys_Sleep(1);
  } while (tr.BinkVideoIsPlaying() && BinkWait(bink) != 0);
  return true;
}

void idRenderSystemLocal::FreeBinkVideo() {
  videoPlaying = false;
  if (!idLib::IsMainThread()) {
    renderThread->renderMode = RENDER_MODE_MANUAL;
    return;
  }

  if (bink_debug.GetInteger() != 0) {
    const float slowPercent =
        static_cast<float>(slowFrames) / static_cast<float>(totalFrames) *
        100.0F;
    idLib::Printf(
        "FreeBinkVideo, slow: %d (%.2f%%), t: %d, sum wait %dms\n",
        slowFrames, slowPercent, totalFrames, stalledTime / 1000);
  }
  StopTimingWatcher();

  if (binkVideoFadeFromColor.r < 0.0F ||
      binkVideoFadeFromColor.g < 0.0F ||
      binkVideoFadeFromColor.b < 0.0F ||
      binkVideoFadeFromColor.a < 0.0F) {
    blockColor = {-1.0F, -1.0F, -1.0F, -1.0F};
  } else {
    blockColor = binkVideoFadeFromColor;
  }
  blockColorFrameNum = 3;
  renderSystem->SetRenderFlag(RENDERFLAG_BINKEXIT);
  renderSystem->RemoveRenderFlag(0x920);
  renderThread->renderMode = RENDER_MODE_MANUAL;
  SyncRenderThread(false);

  idScopedCriticalSection lock(binkMutex);
  if (videoOverlay != nullptr)
    videoOverlay->End();
  videoOverlay = nullptr;
  DestroyGuiModel(videoOverlayModel);
  if (bink != nullptr) {
    BinkClose(bink);
    bink = nullptr;
  }
  ResetBinkSamplersToDefault();
  if (bink_dontfree.GetInteger() == 0) {
    for (int index = 0; index < 3; ++index) {
      DestroyImage(binkY[index]);
      DestroyImage(binkCr[index]);
      DestroyImage(binkCb[index]);
    }
    binkSetupOnce = false;
  }
  if (bink_threaded.GetInteger() != 0 &&
      (BinkRequestStopAsyncThread(
           static_cast<std::uint32_t>(binkThreadId0)) == 0 ||
       BinkRequestStopAsyncThread(
           static_cast<std::uint32_t>(binkThreadId1)) == 0 ||
       BinkWaitStopAsyncThread(static_cast<std::uint32_t>(binkThreadId0)) ==
           0 ||
       BinkWaitStopAsyncThread(static_cast<std::uint32_t>(binkThreadId1)) ==
           0)) {
    idLib::Warning(
        "idRenderSystemLocal::FreeBinkVideo: BinkRequestStopAsyncThread/BinkWaitStopAsyncThread failed");
  }
}

void idRenderSystemLocal::RestartFromLoading() {
  renderThread->renderLoadingIcon = false;
  StopTimingWatcher();
  blockColor = idColor::colorBlack;
  blockColorFrameNum = 10;
  renderThread->renderMode = RENDER_MODE_MANUAL;
  SyncRenderThread(false);
  idScopedCriticalSection lock(binkMutex);
  if (videoOverlay != nullptr)
    videoOverlay->End();
  videoOverlay = nullptr;
  DestroyGuiModel(videoOverlayModel);
}

void idRenderSystemLocal::UpdateBinkTextures() {
  if (bink == nullptr)
    return;
  if (binkUsingSFCForNetworkIO || bink_gamePumpIO.GetInteger() != 0)
    streamFileCache->ServiceBink(binkLocation);
  if (BinkWait(bink) != 0)
    return;

  BinkGetSummary(bink, &binkSummary);
  BinkGetRealtime(bink, &binkRealTime, 0);
  const std::uint64_t doFrameStart = Sys_Microseconds();
  int decodedFrames = 0;
  do {
    if (bink_threaded.GetInteger() != 0) {
      if (BinkDoFrameAsync(bink, static_cast<std::uint32_t>(binkThreadId0),
                           static_cast<std::uint32_t>(binkThreadId1)) != 0) {
        while (BinkDoFrameAsyncWait(bink, 0xFFFFFFFFu) == 0)
          Sys_Sleep(1);
      } else {
        idLib::Warning("BinkDoFrameAsync failed");
      }
    } else {
      skipped = static_cast<int>(BinkDoFrame(bink));
    }
    ++decodedFrames;
    if (BinkShouldSkip(bink) == 0)
      break;
    BinkNextFrame(bink);
  } while (true);

  if (skipped != 0)
    idLib::Printf("BinkDoFrame() skipped on %i\n", bink->FrameNum);
  const std::uint64_t doFrameEnd = Sys_Microseconds();
  if (bink_gamePumpIO.GetInteger() != 0 && binkMutex.Lock(false)) {
    BinkService(bink);
    binkMutex.Unlock();
  }
  const std::uint64_t serviceEnd = Sys_Microseconds();

  currentBufferIndex = (currentBufferIndex + 1) % 3;
  if (!binkDirectlyIntoTextures) {
    BINKFRAMEBUFFERS* const frameBuffers = bink->allocatedframebuffers;
    const std::uint32_t frameIndex = bink->FrameNum & 1u;
    binkY[currentBufferIndex]->SubImageUpload(
        0, 0, 0, 0, static_cast<int>(frameBuffers->YABufferWidth),
        static_cast<int>(frameBuffers->YABufferHeight),
        frameBuffers->Frames[frameIndex].YPlane.Buffer, 0);
    binkCr[currentBufferIndex]->SubImageUpload(
        0, 0, 0, 0, static_cast<int>(frameBuffers->cRcBBufferWidth),
        static_cast<int>(frameBuffers->cRcBBufferHeight),
        frameBuffers->Frames[frameIndex].cRPlane.Buffer, 0);
    binkCb[currentBufferIndex]->SubImageUpload(
        0, 0, 0, 0, static_cast<int>(frameBuffers->cRcBBufferWidth),
        static_cast<int>(frameBuffers->cRcBBufferHeight),
        frameBuffers->Frames[frameIndex].cBPlane.Buffer, 0);
  }
  const std::uint64_t uploadEnd = Sys_Microseconds();

  renderThreadParmState->SetParmSampler(
      rpBinkY.Get(), reinterpret_cast<const idTextureSampler*>(
                         binkY[currentBufferIndex]));
  renderThreadParmState->SetParmSampler(
      rpBinkCr.Get(), reinterpret_cast<const idTextureSampler*>(
                          binkCr[currentBufferIndex]));
  renderThreadParmState->SetParmSampler(
      rpBinkCb.Get(), reinterpret_cast<const idTextureSampler*>(
                          binkCb[currentBufferIndex]));

  if (bink_debug.GetInteger() != 0) {
    ++totalFrames;
    const std::uint64_t doFrameTime = doFrameEnd - doFrameStart;
    const std::uint64_t uploadTime = uploadEnd - serviceEnd;
    const std::uint64_t now = Sys_Microseconds();
    std::uint64_t frameTime = lastFrameTime != 0 ? now - lastFrameTime : 0;
    lastFrameTime = now;
    if (frameTime > 47000)
      ++slowFrames;
    if (frameTime > 34000)
      stalledTime += static_cast<int>(frameTime - 34000);
    if (bink_debug.GetInteger() == 1 || doFrameTime > 10000 ||
        uploadTime > 30000 || frameTime > 100000) {
      const bool hasFrameSkip = skipped != 0 || decodedFrames > 1;
      va frameSkip(" frame_skip: %d", decodedFrames - 1);
      const char* const slow = frameTime > 48000 ? " bink_slow" : "";
      idLib::Printf(
          "bink frame %5i: %5.2f ms doFrame, %5.3f ms service sound, %5.2f ms upload, %5.2f ms frametime%s%s\n",
          bink->FrameNum,
          static_cast<double>(static_cast<int>(doFrameTime)) * 0.001,
          static_cast<double>(
              static_cast<int>(serviceEnd - doFrameEnd)) * 0.001,
          static_cast<double>(static_cast<int>(uploadTime)) * 0.001,
          static_cast<double>(static_cast<int>(frameTime)) * 0.001, slow,
          hasFrameSkip ? frameSkip.buffer : "");
      lastFrameTime = Sys_Microseconds();
    }
    if (watcherTread != nullptr)
      watcherTread->StepFrame(static_cast<int>(bink->FrameNum));
  }
  if (bink->FrameNum < bink->Frames || videoFlags == VIDEO_LOOP)
    BinkNextFrame(bink);
}

void FillBinkBuffer(volatile bool& cancelToTerminate) {
  if (!renderSystem->BinkVideoIsPlaying() || binkUsingSFCForNetworkIO)
    return;
  idScopedCriticalSection lock(binkMutex);
  if (bink == nullptr)
    return;

  BinkControlBackgroundIO(bink, BINK_BACKGROUND_SUSPEND);
  const std::uint32_t initialPercent = bink_percentage_full(bink);
  if (initialPercent == 0)
    idLib::Printf("Bink underflow at %i\n", static_cast<int>(Sys_Milliseconds()));
  if (bink_debug.GetInteger() != 0) {
    idLib::Printf("%6i: FillBinkBuffer: %i\n",
                  static_cast<int>(Sys_Milliseconds()), initialPercent);
  }
  if (initialPercent < 40) {
    if (bink_gamePumpIO.GetInteger() != 0) {
      BinkControlBackgroundIO(bink, BINK_BACKGROUND_ENABLE);
      while (renderSystem->BinkVideoIsPlaying() &&
             bink_percentage_full(bink) < 90) {
        BinkControlBackgroundIO(bink, BINK_BACKGROUND_RESUME);
        BinkService(bink);
      }
    } else {
      BinkControlBackgroundIO(bink, BINK_BACKGROUND_RESUME);
      while (renderSystem->BinkVideoIsPlaying() &&
             bink_percentage_full(bink) < 90 && !cancelToTerminate) {
        Sys_Sleep(1);
      }
    }
  }
  BinkControlBackgroundIO(bink, BINK_BACKGROUND_SUSPEND);
}

idBinkTimingWatcher::idBinkTimingWatcher() : frameEv(false) {}

void idBinkTimingWatcher::operator delete(void* pointer) noexcept {
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

void idBinkTimingWatcher::operator delete(void* pointer,
                                          std::size_t) noexcept {
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

int idBinkTimingWatcher::Run() {
  int lastFrameTimeMS = static_cast<int>(Sys_Milliseconds());
  while (!isTerminating) {
    if (frameEv.Wait(100)) {
      lastFrameTimeMS = static_cast<int>(Sys_Milliseconds());
    } else if (bink != nullptr && bink->FrameNum < bink->Frames - 2) {
      idLib::Printf("bink frame %5i missed deadline: %dms\n", curFrame,
                    static_cast<int>(Sys_Milliseconds()) - lastFrameTimeMS);
    }
  }
  return 0;
}

void idRenderSystemLocal::BeginBinkVideo(
    void* hdc, const idVideoDesc* videoDesc, videoFlags_t requestedVideoFlags,
    void* xAudioHardware, int soundSystemSpeakerMask,
    float binkVolumeDBFloat, idRenderVideoOverlay* overlay, bool force) {
  slowFrames = 0;
  stalledTime = 0;
  totalFrames = 0;
  SyncRenderThread(false);
  idScopedCriticalSection lock(binkMutex);
  videoFlags = requestedVideoFlags;
  FreeBinkVideo();
  renderSystem->RemoveRenderFlag(0x442);
  renderSystem->SetRenderFlag(RENDERFLAG_BINKSTARTED);

  char remappedBuffer[520]{};
  const char* selectedName = idVersionSelector::RemapVideoName(
      remappedBuffer, 512, videoDesc->videoFile.data);
  idStrStatic<512> resourceFileName;
  if (bink_videoBasePath.GetString()[0] != '\0') {
    if (idStr::IcmpnPath(selectedName, "base/", 5) == 0) {
      idLib::Warning(
          "Filename '%s' begins with 'base/' which is incorrect. All filenames should be relative to 'base/'",
          selectedName);
      selectedName += 5;
    }
    resourceFileName.Format("%s/%s", bink_videoBasePath.GetString(),
                            selectedName);
    selectedName = resourceFileName.data;
  }
  if (bink_skip.GetInteger() != 0) {
    idLib::Warning("skipping following bink file %s", selectedName);
    return;
  }

  char fixedPath[256]{};
  if (!fileSystem->FileExists(selectedName, true)) {
    fileSystem->FixLongFilename("", "bik", selectedName, fixedPath,
                                sizeof(fixedPath));
    selectedName = fixedPath;
  }
  idFile* sourceFile = fileSystem->OpenFileRead(selectedName, true, false);
  if (sourceFile == nullptr) {
    idLib::Warning("couldn't find bink file %s", selectedName);
    return;
  }
  if (sourceFile->Length() == 0) {
    idLib::Warning("bink file %s zero sized", selectedName);
    return;
  }
  idStr realPath(sourceFile->GetFullPath());
  binkLocation = sourceFile->GetDevice();
  const bool osNative = sourceFile->IsOSNative();
  binkUsingSFCForNetworkIO =
      !osNative && bink_useSFCForNetwork.GetInteger() != 0;
  delete sourceFile;

  BinkSetMemory(&binkMemAlloc, &binkMemFree);
  if (xAudioHardware != nullptr) {
    BinkSetSoundSystem(reinterpret_cast<void*>(&BinkOpenXAudio2),
                       xAudioHardware);
  }
  if (bink_threaded.GetInteger() != 0) {
    idScopeThreadPriorityAndAffinity threadScope(CORE_ANY, THREAD_HIGHEST);
    if (BinkStartAsyncThread(static_cast<std::uint32_t>(binkThreadId0), 0) ==
            0 ||
        BinkStartAsyncThread(static_cast<std::uint32_t>(binkThreadId1), 0) ==
            0) {
      idLib::Warning("failed to start bink threads for %s",
                     videoDesc->videoFile.data);
      return;
    }
  }

  const int languageOffset = LanguageTrackOffset();
  int tracks[4]{static_cast<int>(languageOffset), languageOffset + 1,
                languageOffset + 2, languageOffset + 3};
  if (languageOffset == 0) {
    tracks[0] = 11;
    tracks[1] = 12;
    tracks[2] = 13;
    tracks[3] = 14;
  }
  BINK_OPEN_OPTIONS options{};
  options.IOBufferSize =
      static_cast<std::uint32_t>(bink_ioMegs.GetInteger() << 20);
  options.TotTracks = 4;
  options.TrackNums = tracks;

  std::uint32_t openFlags = BINK_OPEN_BASE_FLAGS;
  if (binkDirectlyIntoTextures)
    openFlags |= BINK_OPEN_DIRECT_TEXTURES;
  char nativePath[256]{};
  fileSystem->RelativePathToOSPath(realPath.data, nativePath,
                                   sizeof(nativePath),
                                   FSPATH_BASE_AND_OVERLAY);
  if (!osNative) {
    options.UserOpen = &idBinkIOWrapper::Open;
    openFlags |= BINK_OPEN_USER_IO;
  }
  if (bink_debugPreLoadCompleteFile.GetInteger() != 0)
    openFlags |= BINK_OPEN_PRELOAD_ALL;

  {
    idScopeThreadPriorityAndAffinity threadScope(CORE_ANY, THREAD_HIGHEST);
    idMem::Singleton()->PushMapHeap();
    bink = BinkOpenWithOptions(nativePath, &options, openFlags);
    idMem::Singleton()->PopMapHeap();
  }
  if (bink == nullptr) {
    idLib::Warning("BinkOpen( %s ) failed: %s", nativePath, BinkGetError());
    return;
  }

  ConfigureBinkAudio(bink, languageOffset, soundSystemSpeakerMask,
                     BinkVolumeFromDecibels(binkVolumeDBFloat));
  BinkGetFrameBuffersInfo(bink, &bfb);
  if (binkSetupOnce) {
    if (static_cast<std::uint32_t>(binkY[0]->opts.width) !=
            bfb.YABufferWidth ||
        static_cast<std::uint32_t>(binkY[0]->opts.height) !=
            bfb.YABufferHeight ||
        static_cast<std::uint32_t>(binkCr[0]->opts.width) !=
            bfb.cRcBBufferWidth ||
        static_cast<std::uint32_t>(binkCr[0]->opts.height) !=
            bfb.cRcBBufferHeight) {
      idLib::Warning(
          "Bink video '%s' with dimensions %dx%d does not match dimensions of previously played video %dx%d, aborting video",
          videoDesc->videoFile.data, bfb.YABufferWidth, bfb.YABufferHeight,
          binkY[0]->opts.width, binkY[0]->opts.height);
      FreeBinkVideo();
      return;
    }
  } else {
    AllocateBinkImages();
  }

  if (binkDirectlyIntoTextures)
    RegisterDirectFrameBuffers();
  if (!binkUsingSFCForNetworkIO)
    BinkControlBackgroundIO(bink, BINK_BACKGROUND_SUSPEND);
  BinkGetSummary(bink, &binkSummary);
  BinkGetRealtime(bink, &binkRealTime, 0);

  videoOverlayModel = AllocateVideoOverlayModel();
#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
  if (testHooks.setModelName != nullptr)
    testHooks.setModelName(videoOverlayModel, "videoOverlayModel");
#else
    videoOverlayModel->SetName("videoOverlayModel");
#endif
  videoOverlay = overlay;
  if (videoOverlay != nullptr) {
    const float frameRate =
        static_cast<float>(binkSummary.FileFrameRate) /
        static_cast<float>(binkSummary.FileFrameRateDiv);
    const float duration =
        static_cast<float>(binkSummary.TotalFrames) / frameRate;
    videoOverlay->Start(videoDesc->videoFile.data, duration);
  }
  renderThread->renderModeBeforeSync =
      force ? RENDER_MODE_FORCE_EXIT : RENDER_MODE_BINK;
  RestartRenderThread(hdc);
  binkSetupOnce = true;
  if (bink_debug.GetInteger() != 0) {
    watcherTread = AllocateTimingWatcher();
    watcherTread->StartThread("bink timing watcher", CORE_ANY, THREAD_HIGHEST,
                              0x40000);
  }
  videoPlaying = true;
  binkVideoFadeFromColor = videoDesc->fadeFromColor;
  lastFrameTime = Sys_Microseconds();
}

#if defined(WOLF_RENDERER_RENDERSYSTEM_BINK_TEST_ACCESS)
namespace RenderSystemBinkTestAccess {
void SetHooks(const Hooks& hooks) { testHooks = hooks; }
void ResetState() {
  bink = nullptr;
  videoFlags = VIDEO_EXIT;
  binkRealTime = {};
  binkSummary = {};
  bfb = {};
  std::fill(std::begin(binkY), std::end(binkY), nullptr);
  std::fill(std::begin(binkCr), std::end(binkCr), nullptr);
  std::fill(std::begin(binkCb), std::end(binkCb), nullptr);
  binkSetupOnce = false;
  binkDirectlyIntoTextures = false;
  binkUsingSFCForNetworkIO = false;
  binkLocation = FS_DEVICE_HARD_DISK_DRIVE;
  currentBufferIndex = 0;
  slowFrames = 0;
  stalledTime = 0;
  totalFrames = 0;
  skipped = 0;
  lastFrameTime = 0;
  watcherTread = nullptr;
  binkVideoFadeFromColor = {-1.0F, -1.0F, -1.0F, -1.0F};
}
void SetMovie(BINK* movie) { bink = movie; }
BINK* GetMovie() { return bink; }
void SetDirectlyIntoTextures(bool direct) {
  binkDirectlyIntoTextures = direct;
}
void SetUsingStreamCache(bool useStreamCache) {
  binkUsingSFCForNetworkIO = useStreamCache;
}
void SetLocation(int device) { binkLocation = static_cast<fsDevice_t>(device); }
void SetImages(idImage* const y[3], idImage* const cr[3],
               idImage* const cb[3]) {
  std::copy(y, y + 3, binkY);
  std::copy(cr, cr + 3, binkCr);
  std::copy(cb, cb + 3, binkCb);
}
idImage* GetYImage(int index) { return binkY[index]; }
int GetCurrentBufferIndex() { return currentBufferIndex; }
const BINKREALTIME& GetRealtime() { return binkRealTime; }
const BINKSUMMARY& GetSummary() { return binkSummary; }
const BINKFRAMEBUFFERS& GetFrameBuffers() { return bfb; }
bool GetSetupOnce() { return binkSetupOnce; }
int GetSlowFrames() { return slowFrames; }
int GetStalledTime() { return stalledTime; }
int GetTotalFrames() { return totalFrames; }
std::uint64_t GetLastFrameTime() { return lastFrameTime; }
idBinkTimingWatcher* GetWatcher() { return watcherTread; }
} // namespace RenderSystemBinkTestAccess
#endif
