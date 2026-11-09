#pragma once

#include "shared/idlib/filesystem/file.h"

#include <cstdint>

struct pinnedCacheLines_t {
  pinnedCacheLines_t();

  idFile* cacheFile;
  std::int64_t ofs;
  int length;
  int numLines;
  int extra;
  const void* lineData[24];
  int lineLength[24];
};

int SoundReadCachedData(idFile* file, std::int64_t offset, int length, void* buffer,
                        bool blocking, int priority, int readAhead);
bool SoundPinCachedData(idFile* file, std::int64_t offset, int length,
                        pinnedCacheLines_t* lines, bool persistence, int priority,
                        int granularity);
void SoundUnpinCachedData(pinnedCacheLines_t* lines);
void SoundAdoptPinnedFile(pinnedCacheLines_t* lines, idFile* file);

static_assert(sizeof(pinnedCacheLines_t) == 320, "pinned cache-line ABI drift");
