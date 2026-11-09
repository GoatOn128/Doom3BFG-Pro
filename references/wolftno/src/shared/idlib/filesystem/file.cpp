#include "file.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/networking/bitmsg.h"

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <sys/stat.h>
#ifdef _WIN32
#include <io.h>
#endif

namespace {
std::atomic<std::uint32_t> atomicUniqID{0};
int CSeekOrigin(fsOrigin_t origin) {
  return origin == FS_SEEK_CUR ? SEEK_CUR : origin == FS_SEEK_END ? SEEK_END : SEEK_SET;
}
std::int64_t FileTell(std::FILE* file) {
#ifdef _WIN32
  return _ftelli64(file);
#else
  return static_cast<std::int64_t>(ftello(file));
#endif
}
int FileSeek(std::FILE* file, std::int64_t offset, int origin) {
#ifdef _WIN32
  return _fseeki64(file, offset, origin);
#else
  return fseeko(file, static_cast<off_t>(offset), origin);
#endif
}
std::filesystem::path ResolveBasePath(const char* path, unsigned int) {
  return std::filesystem::path(path ? path : "");
}
} // namespace

idFile::idFile() : uniqID(++atomicUniqID) {}

std::size_t idFile::ReadString(idStr& string) {
  int length = 0;
  std::size_t bytes = Read(&length, sizeof(length));
  if (length <= 0) {
    string.Empty();
    return bytes;
  }
  string.Fill(' ', length);
  return bytes + Read(&string[0], static_cast<std::size_t>(length));
}

std::size_t idFile::WriteString(const char* string) {
  const int length = string != nullptr ? static_cast<int>(std::strlen(string)) : 0;
  const std::size_t bytes = Write(&length, sizeof(length));
  return bytes + (length > 0 ? Write(string, static_cast<std::size_t>(length)) : 0);
}

std::size_t idFile::ReadOfs(std::int64_t offset, void* buffer, std::size_t length) {
  const std::int64_t old = Tell();
  if (Seek(offset, FS_SEEK_SET) != 0)
    return 0;
  const std::size_t result = Read(buffer, length);
  Seek(old, FS_SEEK_SET);
  return result;
}

std::size_t idFile::WriteOfs(std::int64_t offset, const void* buffer, std::size_t length) {
  const std::int64_t old = Tell();
  if (Seek(offset, FS_SEEK_SET) != 0)
    return 0;
  const std::size_t result = Write(buffer, length);
  Seek(old, FS_SEEK_SET);
  return result;
}

std::size_t idFile::Printf(const char* format, ...) {
  std::va_list arguments;
  va_start(arguments, format);
  const std::size_t result = VPrintf(format, arguments);
  va_end(arguments);
  return result;
}

std::size_t idFile::VPrintf(const char* format, std::va_list arguments) {
  char buffer[16384];
  const int count = idStr::vsnPrintf(buffer, sizeof(buffer), format, arguments);
  return count > 0 ? Write(buffer, static_cast<std::size_t>(count)) : 0;
}

std::size_t FS_WriteFloatString(char* buffer, std::size_t bufferSize, const char* format,
                                std::va_list arguments) {
  if (!buffer || bufferSize == 0)
    return 0;
  const int count = idStr::vsnPrintf(buffer, static_cast<int>(bufferSize), format, arguments);
  return count > 0 ? static_cast<std::size_t>(count) : 0;
}

std::size_t idFile::WriteFloatString(const char* format, ...) {
  char buffer[16384];
  std::va_list arguments;
  va_start(arguments, format);
  const std::size_t count = FS_WriteFloatString(buffer, sizeof(buffer), format, arguments);
  va_end(arguments);
  return Write(buffer, count);
}

idFile_Memory::idFile_Memory() : idFile_Memory("memory") {}
idFile_Memory::idFile_Memory(const char* fileName)
    : name(fileName ? fileName : "memory"), mode(FS_WRITE), maxSize(0), fileSize(0), allocated(0),
      timestamp(0), filePtr(nullptr), curPtr(nullptr), ownsData(true) {}
idFile_Memory::~idFile_Memory() {
  if (ownsData)
    delete[] filePtr;
}

idFile_BitMsg::idFile_BitMsg(idBitMsg* message)
    : name("*bitmsg*"), mode(FS_READ_WRITE), msg(message) {}
std::size_t idFile_BitMsg::Read(void* buffer, std::size_t length) {
  return static_cast<std::size_t>(
      msg->ReadData(buffer, static_cast<int>(length)));
}
std::size_t idFile_BitMsg::Write(const void* buffer, std::size_t length) {
  if (mode == FS_READ)
    idLib::FatalError("idFile_Memory::Write: %s not opened in write mode",
                      name.data);
  msg->WriteData(buffer, static_cast<int>(length));
  return length;
}
std::int64_t idFile_BitMsg::Length() const {
  return msg ? msg->GetSize() : 0;
}
std::int64_t idFile_BitMsg::Tell() const {
  return mode != FS_READ ? msg->GetSize() : msg->GetReadCount();
}

void idFile_Memory::SetReadOnlyData(char* value, std::uint64_t length) {
  if (ownsData)
    delete[] filePtr;
  maxSize = fileSize = allocated = length;
  mode = FS_READ;
  filePtr = curPtr = value;
  ownsData = false;
}
void idFile_Memory::SetWritableData(char* value, std::uint64_t length) {
  if (ownsData)
    delete[] filePtr;
  maxSize = allocated = length;
  fileSize = 0;
  mode = FS_WRITE;
  filePtr = curPtr = value;
  ownsData = false;
}

std::size_t idFile_Memory::ReadOfs(std::int64_t offset, void* buffer, std::size_t length) {
  if ((mode != FS_READ && mode != FS_READ_WRITE) || offset < 0 ||
      static_cast<std::uint64_t>(offset) >= fileSize)
    return 0;
  length = std::min<std::size_t>(length, static_cast<std::size_t>(fileSize - offset));
  std::memcpy(buffer, filePtr + offset, length);
  return length;
}
std::size_t idFile_Memory::WriteOfs(std::int64_t offset, const void* buffer, std::size_t length) {
  const std::int64_t old = Tell();
  if (Seek(offset, FS_SEEK_SET) != 0)
    return 0;
  const std::size_t result = Write(buffer, length);
  Seek(old, FS_SEEK_SET);
  return result;
}
std::size_t idFile_Memory::Read(void* buffer, std::size_t length) {
  const std::size_t result = ReadOfs(Tell(), buffer, length);
  if (curPtr)
    curPtr += result;
  return result;
}
std::size_t idFile_Memory::Write(const void* buffer, std::size_t length) {
  if (mode == FS_READ) {
    idLib::FatalError("idFile_Memory::Write: %s not opened in write mode",
                      name.data);
  }
  const std::uint64_t position = static_cast<std::uint64_t>(Tell());
  const std::uint64_t required = position + length + 1;
  if (required > allocated) {
    if (maxSize != 0) {
      idLib::FatalError("idFile_Memory::Write: exceeded maximum size %llu",
                        static_cast<unsigned long long>(maxSize));
    }
    std::uint64_t spread = required - 1;
    spread |= spread >> 1;
    spread |= spread >> 2;
    spread |= spread >> 4;
    spread |= spread >> 8;
    spread |= spread >> 16;
    const std::uint64_t capacity =
        std::max(2 * allocated, spread + allocated + 1);
    char* replacement = new char[static_cast<std::size_t>(capacity)];
    if (allocated != 0) {
      std::memcpy(replacement, filePtr, static_cast<std::size_t>(allocated));
    }
    delete[] filePtr;
    filePtr = replacement;
    curPtr = filePtr + position;
    allocated = capacity;
  }
  std::memcpy(curPtr, buffer, length);
  curPtr += length;
  fileSize = std::max(fileSize, position + length);
  filePtr[fileSize] = '\0';
  return length;
}

int idFile_Memory::Seek(std::int64_t offset, fsOrigin_t origin) {
  const std::int64_t base = origin == FS_SEEK_CUR ? Tell() : origin == FS_SEEK_END ? Length() : 0;
  const std::int64_t position = base + offset;
  if (position < 0 || static_cast<std::uint64_t>(position) > fileSize)
    return -1;
  curPtr = filePtr ? filePtr + position : nullptr;
  return 0;
}
void idFile_Memory::SetLength(std::uint64_t length) {
  if (length > allocated) {
    if (!ownsData)
      return;
    const std::uint64_t oldPosition = static_cast<std::uint64_t>(Tell());
    char* replacement = new char[static_cast<std::size_t>(length)];
    if (filePtr && fileSize)
      std::memcpy(replacement, filePtr, static_cast<std::size_t>(std::min(fileSize, length)));
    delete[] filePtr;
    filePtr = replacement;
    allocated = length;
    curPtr = filePtr + std::min(oldPosition, length);
  } else if (Tell() > static_cast<std::int64_t>(length))
    curPtr = filePtr + length;
  fileSize = length;
}
void idFile_Memory::SetMaxLength(std::uint64_t length) {
  const std::uint64_t oldSize = fileSize;
  SetLength(length);
  fileSize = std::min(oldSize, length);
  maxSize = length;
}
void idFile_Memory::MakeReadOnly() {
  mode = FS_READ;
  Seek(0, FS_SEEK_SET);
}
void idFile_Memory::MakeWritable() {
  mode = FS_WRITE;
  Seek(0, FS_SEEK_SET);
}
void idFile_Memory::Clear(bool freeMemory) {
  if (freeMemory && ownsData) {
    delete[] filePtr;
    filePtr = nullptr;
    allocated = 0;
  }
  fileSize = 0;
  curPtr = filePtr;
}
bool idFile_Memory::AtomicWrite(const char* relativePath, unsigned int basePath) const {
  return AtomicFileWrite(relativePath, basePath, filePtr, static_cast<std::int64_t>(fileSize));
}

idFile_Permanent::idFile_Permanent(const char* relativePath, const char* osPath, fsMode_t fileMode,
                                   bool create)
    : name(relativePath ? relativePath : ""), fullPath(osPath ? osPath : relativePath),
      mode(fileMode), fileSize(0), sectorSize(4096), device(FS_DEVICE_HARD_DISK_DRIVE),
      handle(nullptr) {
  if (create && IsWritable()) {
    std::error_code error;
    const auto parent = std::filesystem::path(fullPath.data).parent_path();
    if (!parent.empty())
      std::filesystem::create_directories(parent, error);
  }
  const char* openMode = fileMode == FS_WRITE        ? "wb"
                         : fileMode == FS_READ_WRITE ? (create ? "w+b" : "r+b")
                         : fileMode == FS_APPEND     ? "ab+"
                                                     : "rb";
  std::FILE* file = nullptr;
  if (fopen_s(&file, fullPath.data, openMode) == 0)
    handle = file;
  if (file) {
    FileSeek(file, 0, SEEK_END);
    fileSize = FileTell(file);
    FileSeek(file, 0, fileMode == FS_APPEND ? SEEK_END : SEEK_SET);
  }
}
idFile_Permanent::~idFile_Permanent() {
  if (handle)
    std::fclose(static_cast<std::FILE*>(handle));
}
std::size_t idFile_Permanent::Read(void* buffer, std::size_t length) {
  return handle ? std::fread(buffer, 1, length, static_cast<std::FILE*>(handle)) : 0;
}
std::size_t idFile_Permanent::Write(const void* buffer, std::size_t length) {
  if (!handle || !IsWritable())
    return 0;
  const std::size_t result = std::fwrite(buffer, 1, length, static_cast<std::FILE*>(handle));
  fileSize = std::max(fileSize, Tell());
  return result;
}
std::size_t idFile_Permanent::ReadOfs(std::int64_t offset, void* buffer, std::size_t length) {
  return idFile::ReadOfs(offset, buffer, length);
}
std::size_t idFile_Permanent::WriteOfs(std::int64_t offset, const void* buffer,
                                       std::size_t length) {
  return idFile::WriteOfs(offset, buffer, length);
}
bool idFile_Permanent::Lock(std::int64_t, std::size_t, fsLock_t) {
  return handle != nullptr;
}
bool idFile_Permanent::Unlock(std::int64_t, std::size_t) {
  return handle != nullptr;
}
void idFile_Permanent::SetLength(std::uint64_t length) {
#ifdef _WIN32
  if (handle && IsWritable() && _chsize_s(_fileno(static_cast<std::FILE*>(handle)), length) == 0)
    fileSize = static_cast<std::int64_t>(length);
#else
  (void)length;
#endif
}
std::int64_t idFile_Permanent::Tell() const {
  return handle ? FileTell(static_cast<std::FILE*>(handle)) : -1;
}
int idFile_Permanent::Seek(std::int64_t offset, fsOrigin_t origin) {
  return handle ? FileSeek(static_cast<std::FILE*>(handle), offset, CSeekOrigin(origin)) : -1;
}
void idFile_Permanent::Flush() {
  if (handle)
    std::fflush(static_cast<std::FILE*>(handle));
}
void idFile_Permanent::ForceFlush() {
  Flush();
}

std::uint32_t WintimeToTimestamp(std::uint64_t windowsTime) {
  constexpr std::uint64_t epochDifference = 116444736000000000ull;
  return windowsTime > epochDifference
             ? static_cast<std::uint32_t>((windowsTime - epochDifference) / 10000000ull)
             : 0;
}
bool idFile_Permanent::GetFilestat(const char* path, std::uint32_t* outTimestamp,
                                   std::uint64_t* outSize) {
  if (!path || !outTimestamp || !outSize)
    return false;
  struct _stat64 status{};
  if (_stat64(path, &status) != 0)
    return false;
  *outTimestamp = static_cast<std::uint32_t>(status.st_mtime);
  *outSize = static_cast<std::uint64_t>(status.st_size);
  return true;
}
std::uint32_t idFile_Permanent::Timestamp() const {
  std::uint32_t result = 0;
  std::uint64_t size = 0;
  GetFilestat(fullPath.data, &result, &size);
  return result;
}

bool AtomicFileWrite(const char* relativePath, unsigned int basePath, const void* data,
                     std::int64_t length) {
  if (!relativePath || (!data && length != 0) || length < 0)
    return false;
  const std::filesystem::path destination = ResolveBasePath(relativePath, basePath);
  std::error_code error;
  if (!destination.parent_path().empty())
    std::filesystem::create_directories(destination.parent_path(), error);
  const std::filesystem::path temporary = destination.string() + ".tmp";
  std::FILE* file = nullptr;
  if (fopen_s(&file, temporary.string().c_str(), "wb") != 0 || !file)
    return false;
  const std::size_t written = std::fwrite(data, 1, static_cast<std::size_t>(length), file);
  std::fflush(file);
  std::fclose(file);
  if (written != static_cast<std::size_t>(length)) {
    std::filesystem::remove(temporary, error);
    return false;
  }
  std::filesystem::remove(destination, error);
  error.clear();
  std::filesystem::rename(temporary, destination, error);
  return !error;
}

idFile_AtomicWrite::idFile_AtomicWrite(const char* relativePath, fsPath_t path)
    : idFile_Memory(relativePath), writeName(relativePath), basePath(path),
      osPath(ResolveBasePath(relativePath, path).string().c_str()) {}
idFile_AtomicWrite::~idFile_AtomicWrite() {
  if (filePtr || fileSize == 0)
    AtomicWrite(writeName.data, basePath);
}
void idFile_AtomicWrite::Clear(bool freeMemory) {
  idFile_Memory::Clear(freeMemory);
  writeName.Clear();
  osPath.Clear();
}
