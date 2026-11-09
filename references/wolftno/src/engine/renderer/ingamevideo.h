#pragma once

#include "engine/decls/decl.h"
#include "engine/renderer/jobs/videotranscode/videotranscode.h"

#include <cstddef>
#include <cstdint>

class idParallelJobList;

class idInGameVideoFile : public idResource {
public:
  idInGameVideoFile() = default;
  ~idInGameVideoFile() override;

  void LoadResource() override;
  idResourceList* GetResourceList() const override;
  void Free();

  std::uint8_t* buffer{};

  static idTypedResourceList<idInGameVideoFile> resourceList;
};

class idInGameVideoManager {
public:
  virtual ~idInGameVideoManager();
  virtual void Init() = 0;
  virtual void Shutdown() = 0;
  virtual void Update(int timeMilliseconds) = 0;
  virtual void UnbindAll() = 0;
  virtual void BindVideo(int index, const idInGameVideoFile* file) = 0;
  virtual void SetVideoTime(int index, int time) = 0;
  virtual void SetLooping(int index, bool loop) = 0;
  virtual void SetPaused(int index, bool paused) = 0;
  virtual int GetWeaponVideoIndex() const = 0;
};

class idInGameVideoManagerLocal final : public idInGameVideoManager {
public:
  static constexpr int MAX_VIDEOS = 6;

  struct video_t {
    int timeOffset;
    int pausedTime;
    bool looping;
    std::uint8_t alignmentBeforeLastFrame[3];
    int lastFrame;
    const idInGameVideoFile* file;
  };

  idInGameVideoManagerLocal();
  ~idInGameVideoManagerLocal() override;

  void Init() override;
  void Shutdown() override;
  void Update(int timeMilliseconds) override;
  void UnbindAll() override;
  void BindVideo(int index, const idInGameVideoFile* file) override;
  void SetVideoTime(int index, int time) override;
  void SetLooping(int index, bool loop) override;
  void SetPaused(int index, bool paused) override;
  int GetWeaponVideoIndex() const override;

  void UnbindVideo(const idInGameVideoFile* file);

  int lastUpdateTimeMilliseconds;
  std::uint32_t alignmentBeforeJobList;
  idParallelJobList* parallelJobList;
  video_t videos[MAX_VIDEOS];
  videoTranscodeParms_t parms[MAX_VIDEOS];
};

template <>
idInGameVideoFile*
idTypedResourceList<idInGameVideoFile>::Alloc(const char* name) const;
template <>
int idTypedResourceList<idInGameVideoFile>::AllocSize() const;

extern idInGameVideoManagerLocal localVideoManager;
extern idInGameVideoManager* videoManager;

static_assert(sizeof(idInGameVideoManager) == 8,
              "idInGameVideoManager ABI drift");
static_assert(sizeof(idInGameVideoManagerLocal::video_t) == 24,
              "in-game video slot ABI drift");
static_assert(offsetof(idInGameVideoManagerLocal, lastUpdateTimeMilliseconds) ==
                  0x8,
              "in-game video update-time offset drift");
static_assert(offsetof(idInGameVideoManagerLocal, parallelJobList) == 0x10,
              "in-game video job-list offset drift");
static_assert(offsetof(idInGameVideoManagerLocal, videos) == 0x18,
              "in-game video slot offset drift");
static_assert(offsetof(idInGameVideoManagerLocal, parms) == 0xA8,
              "in-game video transcode offset drift");
static_assert(sizeof(idInGameVideoManagerLocal) == 0x1C8,
              "idInGameVideoManagerLocal ABI drift");
static_assert(offsetof(idInGameVideoFile, buffer) == 0x30,
              "idInGameVideoFile buffer offset drift");
static_assert(sizeof(idInGameVideoFile) == 0x38,
              "idInGameVideoFile ABI drift");
