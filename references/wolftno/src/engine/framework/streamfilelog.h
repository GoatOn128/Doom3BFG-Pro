#pragma once

#include "shared/idlib/parallelism/thread.h"

#include <cstddef>
#include <cstdint>

class idFile;

enum sflDrive_t : int {
  SFL_OPTICAL_DRIVE = 0,
  SFL_HARD_DRIVE = 1,
  SFL_CONTROL = 2,
  SFL_NUM_DRIVES = 3,
};

enum sflIO_t : int {
  SFL_IDLE = 0,
  SFL_READ = 1,
  SFL_WRITE = 2,
};

struct sflEntry_t {
  std::int64_t microseconds;
  sflDrive_t drive;
  sflIO_t io;
  const idFile* f;
  std::int64_t ofs;
  std::int64_t len;
  int priority;
};

class idStreamFileLog {
public:
  idStreamFileLog();
  ~idStreamFileLog() = default;

  void Clear();
  void NoteFrame();
  void NoteCacheLineRequest(const idFile* const f, int fileLine, int priority);
  void BeginIO(sflDrive_t drive, sflIO_t io, const idFile* const f,
               std::int64_t offset, std::int64_t length, int priority);
  void EndIO(sflDrive_t drive);

  int numEntries;
  int maxEntries;
  sflEntry_t* entries;
  idSysMutex mutex;
  sflIO_t driveState[SFL_NUM_DRIVES];
  sflEntry_t* currentEntry[SFL_NUM_DRIVES];
};

extern idStreamFileLog streamFileLog;

static_assert(sizeof(sflEntry_t) == 0x30, "sflEntry_t ABI drift");
static_assert(offsetof(sflEntry_t, drive) == 0x08,
              "sflEntry_t::drive ABI drift");
static_assert(offsetof(sflEntry_t, priority) == 0x28,
              "sflEntry_t::priority ABI drift");
static_assert(sizeof(idStreamFileLog) == 0x40,
              "idStreamFileLog ABI drift");
static_assert(offsetof(idStreamFileLog, mutex) == 0x10,
              "idStreamFileLog::mutex ABI drift");
static_assert(offsetof(idStreamFileLog, driveState) == 0x18,
              "idStreamFileLog::driveState ABI drift");
static_assert(offsetof(idStreamFileLog, currentEntry) == 0x28,
              "idStreamFileLog::currentEntry ABI drift");
