#include "engine/framework/streamfilelog.h"

#include "shared/idlib/sys/sys_time.h"

idStreamFileLog::idStreamFileLog() : mutex() {
  driveState[0] = SFL_IDLE;
  driveState[1] = SFL_IDLE;
  driveState[2] = SFL_IDLE;
  currentEntry[0] = nullptr;
  currentEntry[1] = nullptr;
  currentEntry[2] = nullptr;
}

void idStreamFileLog::Clear() {
  numEntries = 0;
}

void idStreamFileLog::NoteFrame() {
  if (entries == nullptr) return;

  idScopedCriticalSection lock(mutex);
  sflEntry_t& entry = entries[numEntries++ & (maxEntries - 1)];
  entry.drive = SFL_CONTROL;
  entry.io = SFL_IDLE;
  entry.f = nullptr;
  entry.ofs = 0;
  entry.len = 0;
  entry.priority = 0;
  entry.microseconds = static_cast<std::int64_t>(Sys_Microseconds());
}

void idStreamFileLog::NoteCacheLineRequest(const idFile* const f, int fileLine,
                                           int priority) {
  if (entries == nullptr) return;

  idScopedCriticalSection lock(mutex);
  const std::int64_t offset = static_cast<std::int64_t>(fileLine) << 16;
  const int mask = maxEntries - 1;
  const sflEntry_t& previous = entries[(numEntries - 1) & mask];
  if (previous.drive == SFL_CONTROL && previous.f == f &&
      previous.ofs == offset && previous.priority == priority) {
    return;
  }

  sflEntry_t& entry = entries[numEntries++ & mask];
  entry.drive = SFL_CONTROL;
  entry.io = SFL_IDLE;
  entry.f = f;
  entry.ofs = offset;
  entry.len = 0;
  entry.priority = priority;
  entry.microseconds = static_cast<std::int64_t>(Sys_Microseconds());
}

void idStreamFileLog::BeginIO(sflDrive_t drive, sflIO_t io,
                              const idFile* const f,
                              std::int64_t offset, std::int64_t length,
                              int priority) {
  driveState[drive] = io;
  if (entries == nullptr) return;

  idScopedCriticalSection lock(mutex);
  currentEntry[drive] = &entries[numEntries++ & (maxEntries - 1)];
  sflEntry_t& entry = *currentEntry[drive];
  entry.drive = drive;
  entry.io = io;
  entry.f = f;
  entry.ofs = offset;
  entry.len = length;
  entry.priority = priority;
  entry.microseconds = static_cast<std::int64_t>(Sys_Microseconds());
}

void idStreamFileLog::EndIO(sflDrive_t drive) {
  driveState[drive] = SFL_IDLE;
  if (currentEntry[drive] == nullptr) return;

  idScopedCriticalSection lock(mutex);
  sflEntry_t& entry = entries[numEntries++ & (maxEntries - 1)];
  entry = *currentEntry[drive];
  entry.io = SFL_IDLE;
  entry.microseconds = static_cast<std::int64_t>(Sys_Microseconds());
  currentEntry[drive] = nullptr;
}

idStreamFileLog streamFileLog;
