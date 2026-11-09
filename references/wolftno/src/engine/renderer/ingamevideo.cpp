#include "engine/renderer/ingamevideo.h"

#include "engine/framework/console.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagemanager.h"
#include "shared/idlib/color.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <new>

namespace {

constexpr std::uint32_t VIDEO_MAGIC = 0x00354456U;

void SwapBigIntArray(std::int32_t* values, int count) {
  for (int index = 0; index < count; ++index) {
    const std::uint32_t value = static_cast<std::uint32_t>(values[index]);
    values[index] = static_cast<std::int32_t>(
        ((value & 0x000000FFU) << 24U) |
        ((value & 0x0000FF00U) << 8U) |
        ((value & 0x00FF0000U) >> 8U) |
        ((value & 0xFF000000U) >> 24U));
  }
}

} // namespace

idTypedResourceList<idInGameVideoFile> idInGameVideoFile::resourceList(
    "video");
idInGameVideoManagerLocal localVideoManager;
idInGameVideoManager* videoManager = &localVideoManager;

idInGameVideoManager::~idInGameVideoManager() = default;

idInGameVideoManagerLocal::idInGameVideoManagerLocal()
    : lastUpdateTimeMilliseconds(0), alignmentBeforeJobList(0),
      parallelJobList(nullptr), videos{}, parms{} {
  for (int index = 0; index < MAX_VIDEOS; ++index) {
    videos[index].timeOffset = 0;
    videos[index].pausedTime = -1;
    videos[index].looping = true;
    videos[index].lastFrame = -1;
    videos[index].file = nullptr;
  }
}

idInGameVideoManagerLocal::~idInGameVideoManagerLocal() {
  Shutdown();
  for (videoTranscodeParms_t& parameters : parms) {
    idMem::Singleton()->Free(parameters.dxtBuffer, ALIGN_128);
  }
}

int idInGameVideoManagerLocal::GetWeaponVideoIndex() const { return 4; }

void idInGameVideoManagerLocal::Shutdown() {
  if (parallelJobList != nullptr) {
    parallelJobManager->FreeJobList(parallelJobList);
    parallelJobList = nullptr;
  }
}

void idInGameVideoManagerLocal::BindVideo(
    int index, const idInGameVideoFile* file) {
  if (static_cast<unsigned int>(index) >= MAX_VIDEOS) {
    idLib::Warning("Video index %d out of range.  Valid index 0-%d", index,
                   MAX_VIDEOS - 1);
    return;
  }
  if (videos[index].file != file) {
    videos[index].file = file;
    videos[index].lastFrame = -1;
  }
}

void idInGameVideoManagerLocal::SetVideoTime(int index, int time) {
  if (static_cast<unsigned int>(index) >= MAX_VIDEOS) {
    idLib::Warning("Video index %d out of range.  Valid index 0-%d", index,
                   MAX_VIDEOS - 1);
    return;
  }
  videos[index].timeOffset = lastUpdateTimeMilliseconds - time;
}

void idInGameVideoManagerLocal::SetLooping(int index, bool loop) {
  if (static_cast<unsigned int>(index) >= MAX_VIDEOS) {
    idLib::Warning("Video index %d out of range.  Valid index 0-%d", index,
                   MAX_VIDEOS - 1);
    return;
  }
  videos[index].looping = loop;
}

void idInGameVideoManagerLocal::SetPaused(int index, bool paused) {
  if (static_cast<unsigned int>(index) >= MAX_VIDEOS) {
    idLib::Warning("Video index %d out of range.  Valid index 0-%d", index,
                   MAX_VIDEOS - 1);
    return;
  }
  videos[index].pausedTime = paused ? lastUpdateTimeMilliseconds : -1;
}

void idInGameVideoManagerLocal::UnbindAll() {
  if (parallelJobList != nullptr) {
    parallelJobList->Wait();
  }
  for (video_t& video : videos) {
    video.timeOffset = 0;
    video.pausedTime = -1;
    video.looping = true;
    video.lastFrame = -1;
    video.file = nullptr;
  }
}

void idInGameVideoManagerLocal::Init() {
  lastUpdateTimeMilliseconds = 0;
  parallelJobList = parallelJobManager->AllocJobList(
      JOBLIST_VIDEO, JOBLIST_PRIORITY_MEDIUM, MAX_VIDEOS, 0,
      &idColor::colorCyan);

  idImageOpts imageOptions{};
  imageOptions.textureType = TT_2D;
  imageOptions.format = FMT_DXT5;
  imageOptions.colorFormat = CFM_DEFAULT;
  imageOptions.width = 128;
  imageOptions.height = 128;
  imageOptions.packedTail = true;
  imageOptions.sampler.repeat = TR_REPEAT;
  imageOptions.sampler.lodMaxClamp = 13.0F;

  for (int index = 0; index < MAX_VIDEOS; ++index) {
    idStr imageName;
    imageName.Format("_video%d", index);
    parms[index].image = globalImages->ScratchImage(imageName.data,
                                                     imageOptions);
    parms[index].dxtBuffer = static_cast<std::uint8_t*>(
        idMem::Singleton()->AllocWithLocation(
            "w:\\build\\tech5\\engine\\renderer\\InGameVideo.cpp(111) : TAG_RENDERWORLD",
            0x4000U, TAG_RENDERWORLD, false, ALIGN_128, HEAP_DEFAULTHEAP));
    parms[index].needSubImageUpload = false;
  }
}

void idInGameVideoManagerLocal::UnbindVideo(
    const idInGameVideoFile* file) {
  if (parallelJobList != nullptr) {
    parallelJobList->Wait();
  }
  int nonMatching = 0;
  for (video_t& video : videos) {
    if (video.file != nullptr && video.file == file) {
      video.timeOffset = 0;
      video.pausedTime = -1;
      video.looping = true;
      video.lastFrame = -1;
      video.file = nullptr;
    } else {
      ++nonMatching;
    }
  }
  if (nonMatching > 0 && parallelJobList != nullptr) {
    parallelJobList->Submit(nullptr, -1);
  }
}

void idInGameVideoManagerLocal::Update(int timeMilliseconds) {
  lastUpdateTimeMilliseconds = timeMilliseconds;
  if (parallelJobList == nullptr) {
    return;
  }

  parallelJobList->Wait();
  console->AddJobListTimeRange(RANGE_JBL_VDEO, &idColor::colorYellow,
                               parallelJobList, 0);
  for (int index = 0; index < MAX_VIDEOS; ++index) {
    videoTranscodeParms_t& parameters = parms[index];
    video_t& video = videos[index];
    if (parameters.needSubImageUpload) {
      if (parameters.image != nullptr) {
        parameters.image->SubImageUpload(0, 0, 0, 0, 128, 128,
                                         parameters.dxtBuffer, 0);
        parameters.needSubImageUpload = false;
      }
    }

    if (video.file == nullptr || video.file->buffer == nullptr) {
      continue;
    }
    const auto* header =
        reinterpret_cast<const std::int32_t*>(video.file->buffer);
    const int frameRate = header[1];
    const int frameCount = header[2];
    int updateTime = timeMilliseconds;
    if (video.pausedTime >= 0) {
      updateTime = video.pausedTime;
    }
    const int elapsed = updateTime - video.timeOffset;
    int frame = 0;
    if (elapsed >= 0) {
      frame = static_cast<int>((static_cast<std::int64_t>(elapsed) *
                                static_cast<std::int64_t>(frameRate)) /
                               1000);
      if (video.looping) {
        frame %= frameCount;
      } else if (frame > frameCount - 1) {
        frame = frameCount - 1;
      }
    }

    if (frame == video.lastFrame) {
      continue;
    }
    video.lastFrame = frame;
    parameters.dctQualityLuma = 90;
    parameters.dctQualityChroma = 80;
    const int frameStart = header[3 + frame];
    const int frameEnd = header[4 + frame];
    parameters.dctBufferSize = frameEnd - frameStart;
    parameters.dctBuffer = video.file->buffer + frameStart;
    parameters.targetBytePitch = 512;
    parameters.needSubImageUpload = true;
    parallelJobList->AddJob(reinterpret_cast<jobRun_t>(VideoTranscodeJob),
                            &parameters);
  }
  parallelJobList->Submit(nullptr, -1);
}

void idInGameVideoFile::Free() {
  if (buffer != nullptr) {
    idMem::Singleton()->Free(buffer, ALIGN_16);
    buffer = nullptr;
  }
}

idResourceList* idInGameVideoFile::GetResourceList() const {
  return &resourceList;
}

idInGameVideoFile::~idInGameVideoFile() {
  if (videoManager != nullptr) {
    localVideoManager.UnbindVideo(this);
  }
  Free();
}

void idInGameVideoFile::LoadResource() {
  Free();
  const char* fileName = name.str;
  if (fileName[0] == '\0') {
    SetResourceError("Empty name on ingamevideo");
    return;
  }

  idFile* file = fileSystem->OpenFileRead(fileName, true, false);
  if (file == nullptr) {
    return;
  }
  const int length = static_cast<int>(file->Length());
  if (length < 0) {
    SetResourceError("Could not open video '%s'", fileName);
  } else if (static_cast<unsigned int>(length) < 12U) {
    SetResourceError("Video '%s' too small", fileName);
    Free();
  } else {
    buffer = static_cast<std::uint8_t*>(
        idMem::Singleton()->AllocWithLocation(
            "w:\\build\\tech5\\engine\\renderer\\InGameVideo.cpp(332) : TAG_VIDEO",
            static_cast<std::size_t>(length), TAG_VIDEO, false, ALIGN_16,
            HEAP_DEFAULTHEAP));
    file->Read(buffer, static_cast<std::size_t>(length));
    SwapBigIntArray(reinterpret_cast<std::int32_t*>(buffer), 3);
    auto* header = reinterpret_cast<std::int32_t*>(buffer);
    if (static_cast<std::uint32_t>(header[0]) == VIDEO_MAGIC) {
      SwapBigIntArray(header + 3, header[2] + 1);
    } else {
      SetResourceError("Bad magic in video '%s'", fileName);
      Free();
    }
  }
  delete file;
}

template <>
idInGameVideoFile*
idTypedResourceList<idInGameVideoFile>::Alloc(const char* name) const {
  void* storage = idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idInGameVideoFile), TAG_RESOURCE, false, ALIGN_16,
      HEAP_DEFAULTHEAP);
  idInGameVideoFile* video =
      storage != nullptr ? new (storage) idInGameVideoFile : nullptr;
  video->SetName(name);
  return video;
}

template <>
int idTypedResourceList<idInGameVideoFile>::AllocSize() const {
  return sizeof(idInGameVideoFile);
}
