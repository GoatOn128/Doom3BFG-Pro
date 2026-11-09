#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>
#include <zlib.h>

class idResource;
class idResourceList;

struct idPlatformStreamData {
  std::byte lang[16];
  int streamOffset;
  int streamLength;
  int streamAlignment;
  int padding;
  void* streamData;
};

class idResourceFileEntry {
public:
  idResourceFileEntry();
  idResourceFileEntry(const idResourceFileEntry& other);
  ~idResourceFileEntry();
  idResourceFileEntry& operator=(const idResourceFileEntry& other);
  std::size_t Read(idFile* file);
  int resourceIndex;
  int padding;
  idResourceList* resourceList;
  idStrStatic<64> typeName;
  idStrStatic<256> resourceName;
  idStrStatic<256> filename;
  int offset;
  int compressedLength;
  int uncompressedLength;
  int alignment;
  idList<idPlatformStreamData,5> stream;
  unsigned int useBits;
  unsigned char fileId;
  unsigned char tailPadding[3];
};

class idBGLRingBuffer {
public:
  static void* operator new(std::size_t size);
  static void operator delete(void* memory) noexcept;
  explicit idBGLRingBuffer(int numBytes);
  ~idBGLRingBuffer();
  void WriteOfs(std::int64_t dst,const void* src,std::size_t length);
  std::size_t ReadOfs(std::int64_t src,void* dst,std::size_t length);
  std::int64_t CheckSpaceAndWait(std::int64_t offset,int minBytes);
  void Free(std::int64_t base,int numBytes);

  unsigned char* buffer;
  int size;
  int padding0;
  std::int64_t tail;
  std::int64_t commitHead;
  bool cancelToTerminate;
  unsigned char padding1[7];
  idSysSignal freeSignal;
  std::int64_t lastFreeSignalTail;
  idSysSignal dataSignal;
  std::int64_t lastDataSignalHead;
  int freeWaituS;
  int numFreeWaits;
  int dataWaituS;
  int numDataWaits;
};

class idFile_RingBuffer final : public idFile {
public:
  static void* operator new(std::size_t size);
  static void operator delete(void* memory) noexcept;
  idFile_RingBuffer(const char* name,idBGLRingBuffer* ringBuffer,
                    std::int64_t offset,int length);
  ~idFile_RingBuffer() override;
  const char* GetName() const override { return name.data; }
  const char* GetFullPath() const override { return name.data; }
  std::size_t Read(void* buffer,std::size_t length) override;
  std::int64_t Length() const override { return fileSize; }
  std::int64_t Tell() const override { return currOffset; }
  int Seek(std::int64_t offset,fsOrigin_t origin) override;

  idBGLRingBuffer* ringBuffer;
  idStr name;
  std::int64_t fileSize;
  std::int64_t baseOffset;
  std::int64_t currOffset;
};

class idDecompressSpawnThread : public idSysThread {
public:
  static void* operator new(std::size_t size);
  static void operator delete(void* memory) noexcept;
  idDecompressSpawnThread();
  int Run() override;
  void BeginBackgroundLoads();
  std::int64_t compressedOffsetInCurrentEntry;
  std::int64_t uncompressedOffsetInCurrentEntry;
  z_stream z;
};

class idReadSpawnThread : public idSysThread {
public:
  static void* operator new(std::size_t size);
  static void operator delete(void* memory) noexcept;
  idReadSpawnThread() : idSysThread(),decompressThread(nullptr) {}
  int Run() override;
  idDecompressSpawnThread* decompressThread;
};

class idBackgroundLoader {
public:
  struct bgrEntry_t { std::int64_t ringBufferOffset; };

  idBackgroundLoader();
  ~idBackgroundLoader()=default;
  void SpawnThreads();
  void DestroyThreads();
  idFile* GetFile(const char* filename);
  void SkipPastResource(const idResource* resource);
private:
  void DetermineNextReadBlock();
public:
  void BeginBackgroundLoads(idFile& resourceFile,
      const idList<idResourceFileEntry,101>& fileTable);
  void EndBackgroundLoads();

  idFile* resourceFile;
  const idList<idResourceFileEntry,101>* fileTable;
  idList<bgrEntry_t,101> bgrEntries;
  volatile int readIndex;
  volatile int decompressIndex;
  volatile int returnedIndex;
  volatile bool cancelToTerminate;
  unsigned char padding0[3];
  idSysSignal entryStartedSignal;
  idBGLRingBuffer* decompressBuffer;
  unsigned char* readBuffers[2];
  std::int64_t readBufferOfs[2];
  int currentReadBuffer;
  int padding1;
  idReadSpawnThread* readSpawnThread;
  idDecompressSpawnThread* decompressSpawnThread;
  int foregroundSleepMsec;
  int backgroundSeeks;
  int backgroundReadMsec;
  int backgroundSleepMsec;
  int decompressSleepMsec;
  int decompressWorkMsec;
  int startForegroundTime;
  int padding2;
};

extern idBackgroundLoader bgl;

static_assert(sizeof(idPlatformStreamData)==40,"platform stream ABI drift");
static_assert(sizeof(idResourceFileEntry)==760,"resource file entry ABI drift");
static_assert(sizeof(idBGLRingBuffer)==88,"BGL ring buffer ABI drift");
static_assert(sizeof(idFile_RingBuffer)==88,"ring-buffer file ABI drift");
static_assert(sizeof(idReadSpawnThread)==96,"BGL read thread ABI drift");
static_assert(sizeof(idDecompressSpawnThread)==192,"BGL decompress thread ABI drift");
static_assert(sizeof(idBackgroundLoader)==160,"background loader ABI drift");
