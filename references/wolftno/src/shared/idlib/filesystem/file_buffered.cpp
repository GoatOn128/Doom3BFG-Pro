#include "file_buffered.h"

#include "filesystemlocal.h"

#include <algorithm>
#include <cstring>

idFile_Buffered::idFile_Buffered(std::uint64_t size)
    : backFile(nullptr), bufferMode(BUFFERMODE_READ), deAllocBuffer(true), deleteFile(false),
      bufferSize(static_cast<std::int64_t>(std::max<std::uint64_t>(1, size))),
      buffer(new std::uint8_t[static_cast<std::size_t>(std::max<std::uint64_t>(1, size))]),
      bufferPos(0), bufferOffSet(0) {}
idFile_Buffered::~idFile_Buffered() {
  Finalize();
  if (deleteFile)
    delete backFile;
  if (deAllocBuffer)
    delete[] buffer;
}
void idFile_Buffered::SetFileWrite(idFile* file) {
  Finalize();
  if (deleteFile)
    delete backFile;
  backFile = file;
  bufferMode = BUFFERMODE_WRITE;
  deleteFile = false;
  bufferPos = 0;
  bufferOffSet = file ? file->Tell() : 0;
}
void idFile_Buffered::SetFileRead(idFile* file) {
  Finalize();
  if (deleteFile)
    delete backFile;
  backFile = file;
  bufferMode = BUFFERMODE_READ;
  deleteFile = false;
  bufferPos = 0;
  bufferOffSet = file ? file->Tell() : 0;
}
bool idFile_Buffered::OpenFileWrite(const char* path, fsPath_t) {
  Finalize();
  if (deleteFile)
    delete backFile;
  backFile = fileSystem != nullptr ? fileSystem->OpenFileWrite(path) : nullptr;
  deleteFile = true;
  bufferMode = BUFFERMODE_WRITE;
  bufferPos = bufferOffSet = 0;
  return backFile != nullptr;
}
bool idFile_Buffered::OpenFileRead(const char* path) {
  Finalize();
  if (deleteFile)
    delete backFile;
  backFile = fileSystem != nullptr ? fileSystem->OpenFileRead(path) : nullptr;
  deleteFile = true;
  bufferMode = BUFFERMODE_READ;
  bufferPos = bufferOffSet = 0;
  return backFile != nullptr;
}
bool idFile_Buffered::WriteOut() {
  if (!backFile || bufferMode != BUFFERMODE_WRITE)
    return false;
  if (bufferPos == 0)
    return true;
  const std::size_t count = backFile->Write(buffer, static_cast<std::size_t>(bufferPos));
  const bool ok = count == static_cast<std::size_t>(bufferPos);
  bufferOffSet += count;
  bufferPos = 0;
  return ok;
}
std::size_t idFile_Buffered::Write(const void* data, std::size_t length) {
  if (!backFile || bufferMode != BUFFERMODE_WRITE || !data)
    return 0;
  const auto* source = static_cast<const std::uint8_t*>(data);
  std::size_t total = 0;
  while (total < length) {
    if (bufferPos == bufferSize && !WriteOut())
      break;
    const std::size_t count =
        std::min<std::size_t>(length - total, static_cast<std::size_t>(bufferSize - bufferPos));
    std::memcpy(buffer + bufferPos, source + total, count);
    bufferPos += count;
    total += count;
  }
  return total;
}
std::size_t idFile_Buffered::Read(void* data, std::size_t length) {
  return backFile && bufferMode == BUFFERMODE_READ ? backFile->Read(data, length) : 0;
}
std::size_t idFile_Buffered::WriteOfs(std::int64_t offset, const void* data, std::size_t length) {
  if (!WriteOut() || !backFile)
    return 0;
  const std::size_t result = backFile->WriteOfs(offset, data, length);
  bufferOffSet = backFile->Tell();
  return result;
}
std::size_t idFile_Buffered::ReadOfs(std::int64_t offset, void* data, std::size_t length) {
  return backFile && bufferMode == BUFFERMODE_READ ? backFile->ReadOfs(offset, data, length) : 0;
}
std::int64_t idFile_Buffered::Length() const {
  return backFile ? (bufferMode == BUFFERMODE_WRITE
                         ? std::max(backFile->Length(), bufferOffSet + bufferPos)
                         : backFile->Length())
                  : 0;
}
void idFile_Buffered::SetLength(std::uint64_t length) {
  if (!backFile)
    return;
  if (bufferMode == BUFFERMODE_WRITE)
    WriteOut();
  backFile->SetLength(length);
  bufferOffSet = backFile->Tell();
}
std::int64_t idFile_Buffered::Tell() const {
  return backFile ? (bufferMode == BUFFERMODE_WRITE ? bufferOffSet + bufferPos : backFile->Tell())
                  : 0;
}
int idFile_Buffered::Seek(std::int64_t offset, fsOrigin_t origin) {
  if (!backFile)
    return -1;
  if (bufferMode == BUFFERMODE_WRITE && !WriteOut())
    return -1;
  const int result = backFile->Seek(offset, origin);
  bufferOffSet = backFile->Tell();
  return result;
}
bool idFile_Buffered::Lock(std::int64_t offset, std::size_t length, fsLock_t mode) {
  return backFile && backFile->Lock(offset, length, mode);
}
bool idFile_Buffered::Unlock(std::int64_t offset, std::size_t length) {
  return backFile && backFile->Unlock(offset, length);
}
void idFile_Buffered::Flush() {
  if (backFile) {
    if (bufferMode == BUFFERMODE_WRITE)
      WriteOut();
    backFile->Flush();
  }
}
void idFile_Buffered::ForceFlush() {
  if (backFile) {
    if (bufferMode == BUFFERMODE_WRITE)
      WriteOut();
    backFile->ForceFlush();
  }
}
bool idFile_Buffered::Finalize() {
  if (!backFile)
    return true;
  return bufferMode != BUFFERMODE_WRITE || WriteOut();
}
