#pragma once

#include "shared/idlib/filesystem/file.h"

#include <cstdint>

#ifndef WOLFTNO_PFC_REQUEST_T_DEFINED
#define WOLFTNO_PFC_REQUEST_T_DEFINED
enum pfcRequest_t : int {
  PFC_TEXTURE = 0,
  PFC_DEFAULT = PFC_TEXTURE,
  PFC_SOUND = 1,
};
#endif

// NEEDS_WORK(shared/idlib/filesystem): declaration boundary required by Bink
// network I/O. Its queueing/cache policy stays deferred to streamfilecache.cpp.
class idStreamFileCache {
public:
  virtual ~idStreamFileCache() = default;
  virtual int ReadCachedData(idFile* file, std::int64_t offset, int length,
                             void* destination, bool blockingRead,
                             int priority, int persistence,
                             pfcRequest_t request) = 0;
  virtual void ServiceBink(fsDevice_t device) = 0;
};

extern idStreamFileCache* streamFileCache;
