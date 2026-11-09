#pragma once

#include "shared/idlib/text/str.h"

#include <cstdarg>
#include <cstddef>
#include <cstdint>

enum fsDevice_t {
  FS_DEVICE_HARD_DISK_DRIVE,
  FS_DEVICE_OPTICAL_DISK_DRIVE,
  FS_DEVICE_SOLID_STATE_DRIVE,
  FS_DEVICE_NETWORK,
  FS_DEVICE_MEMORY
};
enum fsPath_t {
  FSPATH_BASE,
  FSPATH_CACHE,
  FSPATH_SAVE,
  FSPATH_INSTALL,
  FSPATH_SOURCE,
  FSPATH_OVERLAY,
  FSPATH_BASE_AND_OVERLAY
};
enum fsOrigin_t { FS_SEEK_CUR, FS_SEEK_END, FS_SEEK_SET };
enum fsMode_t { FS_READ, FS_WRITE, FS_READ_WRITE, FS_READ_NO_BUFFERING, FS_APPEND };
enum fsLock_t { FS_LOCK_SHARED, FS_LOCK_EXCLUSIVE };
class idBitMsg;

struct alignas(4) idFileProps {
  std::uint32_t timestamp;
  bool writable;
};

static_assert(sizeof(idFileProps) == 8, "idFileProps ABI drift");

class idFile {
public:
  idFile();
  virtual ~idFile() = default;
  virtual const char* GetName() const { return ""; }
  virtual const char* GetFullPath() const { return GetName(); }
  virtual std::size_t Read(void*, std::size_t) { return 0; }
  virtual std::size_t Write(const void*, std::size_t) { return 0; }
  virtual std::size_t ReadOfs(std::int64_t offset, void* buffer, std::size_t length);
  virtual std::size_t WriteOfs(std::int64_t offset, const void* buffer, std::size_t length);
  virtual bool Lock(std::int64_t, std::size_t, fsLock_t) { return false; }
  virtual bool Unlock(std::int64_t, std::size_t) { return false; }
  virtual std::int64_t Length() const { return 0; }
  virtual void SetLength(std::uint64_t) {}
  virtual std::int64_t Tell() const { return 0; }
  virtual int Seek(std::int64_t, fsOrigin_t) { return -1; }
  virtual std::size_t Printf(const char* format, ...);
  virtual std::size_t VPrintf(const char* format, std::va_list arguments);
  virtual std::size_t WriteFloatString(const char* format, ...);
  virtual std::uint32_t Timestamp() const { return 0; }
  virtual bool IsWritable() const { return false; }
  virtual void Flush() {}
  virtual void ForceFlush() { Flush(); }
  virtual int GetSectorSize() const { return 1; }
  virtual fsDevice_t GetDevice() const { return FS_DEVICE_MEMORY; }
  virtual bool IsOSNative() const { return false; }
  template <typename T> std::size_t ReadBig(T& value);
  template <typename T> std::size_t WriteBig(const T& value);
  template <typename T> std::size_t ReadBigArray(T* values, int count);
  template <typename T> std::size_t WriteBigArray(const T* values, int count);
  std::size_t ReadString(idStr& string);
  std::size_t WriteString(const char* string);
  std::uint32_t uniqID;
};

class idFile_Memory : public idFile {
public:
  idFile_Memory();
  explicit idFile_Memory(const char* fileName);
  ~idFile_Memory() override;
  const char* GetName() const override { return name.data; }
  const char* GetFullPath() const override { return name.data; }
  std::size_t Read(void* buffer, std::size_t length) override;
  std::size_t Write(const void* buffer, std::size_t length) override;
  std::size_t ReadOfs(std::int64_t offset, void* buffer, std::size_t length) override;
  std::size_t WriteOfs(std::int64_t offset, const void* buffer, std::size_t length) override;
  std::int64_t Length() const override { return static_cast<std::int64_t>(fileSize); }
  void SetLength(std::uint64_t length) override;
  std::int64_t Tell() const override { return curPtr && filePtr ? curPtr - filePtr : 0; }
  int Seek(std::int64_t offset, fsOrigin_t origin) override;
  std::uint32_t Timestamp() const override { return timestamp; }
  bool IsWritable() const override { return mode != FS_READ; }
  fsDevice_t GetDevice() const override { return FS_DEVICE_MEMORY; }
#if defined(WOLFTNO_RETAIL_PVS_CONST_FILE_DATA_ABI)
  void SetReadOnlyData(const char* data, std::uint64_t length);
#else
  void SetReadOnlyData(char* data, std::uint64_t length);
#endif
  void SetWritableData(char* data, std::uint64_t length);
  void SetMaxLength(std::uint64_t length);
  void MakeReadOnly();
  void MakeWritable();
  void Clear(bool freeMemory = true);
  bool AtomicWrite(const char* relativePath, unsigned int basePath) const;
  char* GetDataPtr() { return filePtr; }
  const char* GetDataPtr() const { return filePtr; }
  idStrStatic<260> name;
  int mode;
  std::uint64_t maxSize;
  std::uint64_t fileSize;
  std::uint64_t allocated;
  std::uint32_t timestamp;
  char* filePtr;
  char* curPtr;
  bool ownsData;
};

class idFile_Stat : public idFile {
public:
  idFile_Stat(const char* name,std::int64_t size,std::uint32_t timestamp);
  const char* GetName() const override { return fileName.data; }
  const char* GetFullPath() const override { return fileName.data; }
  std::int64_t Length() const override;
  std::uint32_t Timestamp() const override { return fileTimestamp; }
  idStr fileName;
  std::int64_t fileSize;
  std::uint32_t fileTimestamp;
};

static_assert(sizeof(idFile_Stat)==72,"stat file ABI drift");

class idFile_String : public idFile_Memory {
public:
  explicit idFile_String(const char* fileName = "string") : idFile_Memory(fileName) {}
  idStr ToString() const { return idStr(filePtr ? filePtr : "", 0, static_cast<int>(fileSize)); }
};

class idFile_BitMsg : public idFile {
public:
  explicit idFile_BitMsg(idBitMsg* message);
  const char* GetName() const override { return name.data; }
  const char* GetFullPath() const override { return name.data; }
  std::size_t Read(void* buffer, std::size_t length) override;
  std::size_t Write(const void* buffer, std::size_t length) override;
  std::int64_t Length() const override;
  std::int64_t Tell() const override;
  bool IsWritable() const override { return mode != FS_READ; }
  idStr name;
  int mode;
  idBitMsg* msg;
};

class idFile_Permanent : public idFile {
public:
  idFile_Permanent(const char* relativePath, const char* osPath, fsMode_t mode, bool create);
  ~idFile_Permanent() override;
  const char* GetName() const override { return name.data; }
  const char* GetFullPath() const override { return fullPath.data; }
  std::size_t Read(void* buffer, std::size_t length) override;
  std::size_t Write(const void* buffer, std::size_t length) override;
  std::size_t ReadOfs(std::int64_t offset, void* buffer, std::size_t length) override;
  std::size_t WriteOfs(std::int64_t offset, const void* buffer, std::size_t length) override;
  bool Lock(std::int64_t offset, std::size_t length, fsLock_t type) override;
  bool Unlock(std::int64_t offset, std::size_t length) override;
  std::int64_t Length() const override { return fileSize; }
  void SetLength(std::uint64_t length) override;
  std::int64_t Tell() const override;
  int Seek(std::int64_t offset, fsOrigin_t origin) override;
  std::uint32_t Timestamp() const override;
  bool IsWritable() const override { return mode != FS_READ && mode != FS_READ_NO_BUFFERING; }
  void Flush() override;
  void ForceFlush() override;
  int GetSectorSize() const override { return sectorSize; }
  fsDevice_t GetDevice() const override { return device; }
  bool IsOSNative() const override { return true; }
  bool IsOpen() const { return handle != nullptr; }
  static bool GetFilestat(const char* path, std::uint32_t* timestamp, std::uint64_t* size);
  idStr name;
  idStr fullPath;
  fsMode_t mode;
  std::int64_t fileSize;
  int sectorSize;
  fsDevice_t device;
  void* handle;
};

class idFileLocal {
public:
  __forceinline explicit idFileLocal(idFile* value = nullptr) : file(value) {}
  ~idFileLocal() { delete file; }
  idFile* operator->() const { return file; }
  idFile* Release() {
    idFile* result = file;
    file = nullptr;
    return result;
  }
  idFile* file;
};

class idFile_AtomicWrite : public idFile_Memory {
public:
  idFile_AtomicWrite(const char* relativePath, fsPath_t basePath);
  ~idFile_AtomicWrite() override;
  const char* GetFullPath() const override { return osPath.data; }
  void Clear(bool freeMemory = true);
  idStr writeName;
  fsPath_t basePath;
  idStr osPath;
};

std::uint32_t WintimeToTimestamp(std::uint64_t windowsTime);
std::size_t FS_WriteFloatString(char* buffer, std::size_t bufferSize, const char* format,
                                std::va_list arguments);
bool AtomicFileWrite(const char* relativePath, unsigned int basePath, const void* data,
                     std::int64_t length);

static_assert(sizeof(idFile) == 16, "idFile ABI drift");
static_assert(sizeof(idFile_Memory) == 384, "idFile_Memory ABI drift");
static_assert(sizeof(idFile_BitMsg) == 72, "idFile_BitMsg ABI drift");
