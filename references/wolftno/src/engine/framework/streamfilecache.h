#pragma once

#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

#ifndef WOLFTNO_PFC_REQUEST_T_DEFINED
#define WOLFTNO_PFC_REQUEST_T_DEFINED
enum pfcRequest_t : int {
  PFC_TEXTURE = 0,
  PFC_DEFAULT = PFC_TEXTURE,
  PFC_SOUND = 1,
};

struct cacheLineRequest_t {
  idFile* file{};
  int fileLine{};
  int priority{};
  int persistence{};
};
#endif

class idStreamFileCache {
public:
  idStreamFileCache();
  ~idStreamFileCache();
  void Init();
  void InitHDC();
  void Shutdown();
  void ClearMem(bool forceClear);
  void NewFrame();
  void ServiceBink(fsDevice_t);
  int ReadCachedData(idFile* file, std::int64_t offset, int length,
                     void* destination, bool blockingRead, int priority,
                     int persistence, pfcRequest_t request);
  bool OutOfMemoryCallback();
  void Flush();
  std::uint64_t UncachedScheduledRead(idFile* file, std::int64_t offset,
                                      void* destination, int length);
  void UncachedBackgroundRead(idFile* file, std::int64_t offset, int length,
                              void** destination, memTag_t tag,
                              volatile bool* completionFlag);
  void BatchUncachedBackgroundRead(idFile*,std::int64_t*,int*,void***,
                                   memTag_t,volatile bool**,int);
  void BatchFreeMemory(void**,int);
  void SignalWork();
  void BatchRequestCacheLines(const cacheLineRequest_t* requests,
                              int numRequests);
  bool PinCachedData(idFile* file, std::int64_t offset, int length,
                     struct pinnedCacheLines_t& lines, bool blockingRead,
                     int priority, int persistence, pfcRequest_t request);
  void UnpinData(struct pinnedCacheLines_t& lines);

  std::int64_t GetCurrentUsageLimit() const;

  idSysMutex interfaceMutex;
  class idStreamControlThread* controlThread;
  bool cancelToTerminate;
  std::byte alignmentTail[7];
};

static_assert(sizeof(cacheLineRequest_t) == 0x18,
              "cache-line request ABI drift");

extern idStreamFileCache* streamFileCache;
extern idStreamFileCache StreamFileCacheLocal;
bool StaticOutOfMemoryCallback();

static_assert(offsetof(idStreamFileCache,controlThread)==8,
              "stream cache control offset drift");
static_assert(sizeof(idStreamFileCache)==24,"stream cache ABI drift");
