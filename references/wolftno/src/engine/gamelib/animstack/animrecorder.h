#pragma once

#include "shared/idlib/containers/array.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

class idAnimStack;
class idCompressor;
class idFile;
class idGameTimeManager;
class idSerializer;

class idAnimRecorder {
public:
  enum blockType_t : std::uint8_t {
    BLOCK_HEADER = 0,
    BLOCK_FRAME = 1,
    BLOCK_INVALID = 2,
  };

  enum recordingFileMode_t {
    RECORDING_FILE_CLOSED = 0,
    RECORDING_FILE_READ = 1,
    RECORDING_FILE_WRITE = 2,
  };

  virtual void Serialize(int& gameTime, idGameTimeManager& gameTimeManager,
                         bool writing);
  virtual void Stop();
  virtual void ReadFrames(int numFrames, bool validateFile);

  static void KillInstance();

  static void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }

protected:
  friend class idAnimStack;

  idAnimRecorder();
  virtual ~idAnimRecorder() { CloseFile(); }
  virtual void WriteHeaderBlock();
  virtual bool ReadHeaderBlock();

  void OpenFile(bool writing);
  void CloseFile();
  void ValidateFile(bool writing);
  void ValidateBlock(std::uint8_t expectedBlockType);
  static idAnimRecorder* AllocInstance();

  static idAnimRecorder* instance;
  static const char* inputFileName;
  static const char* outputDataFileName;
  static constexpr int bufferSize = 1425;
  static constexpr int version = 2;
  static idAnimRecorder* (*allocFunction)();

public:
  idArray<std::uint8_t, bufferSize> buffer;
  int cachedGameTime;
  int cachedMessageSize;
  int dataStart;
  idCompressor* compressor;
  idFile* file;
  idSerializer* activeSerializer;
  idAnimStack* activeAnimStack;
  recordingFileMode_t recordingFileMode;
  bool isPaused;

};

static_assert(offsetof(idAnimRecorder, buffer) == 0x08,
              "anim recorder buffer offset drift");
static_assert(offsetof(idAnimRecorder, cachedGameTime) == 0x59C,
              "anim recorder cached time offset drift");
static_assert(offsetof(idAnimRecorder, cachedMessageSize) == 0x5A0,
              "anim recorder cached message size offset drift");
static_assert(offsetof(idAnimRecorder, dataStart) == 0x5A4,
              "anim recorder data start offset drift");
static_assert(offsetof(idAnimRecorder, compressor) == 0x5A8,
              "anim recorder compressor offset drift");
static_assert(offsetof(idAnimRecorder, file) == 0x5B0,
              "anim recorder file offset drift");
static_assert(offsetof(idAnimRecorder, activeSerializer) == 0x5B8,
              "anim recorder active serializer offset drift");
static_assert(offsetof(idAnimRecorder, activeAnimStack) == 0x5C0,
              "anim recorder active stack offset drift");
static_assert(offsetof(idAnimRecorder, recordingFileMode) == 0x5C8,
              "anim recorder file mode offset drift");
static_assert(offsetof(idAnimRecorder, isPaused) == 0x5CC,
              "anim recorder pause flag offset drift");
static_assert(sizeof(idAnimRecorder) == 0x5D0,
              "anim recorder ABI drift");
