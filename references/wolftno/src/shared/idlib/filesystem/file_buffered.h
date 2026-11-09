#pragma once

#include "file.h"

class idFile_Buffered : public idFile {
public:
  enum BufferMode_t { BUFFERMODE_READ, BUFFERMODE_WRITE };
  explicit idFile_Buffered(std::uint64_t bufferSize = 65536);
  ~idFile_Buffered() override;
  void SetFileWrite(idFile* file);
  void SetFileRead(idFile* file);
  bool OpenFileWrite(const char* relativePath, fsPath_t basePath = FSPATH_BASE);
  bool OpenFileRead(const char* relativePath);
  const char* GetName() const override { return backFile ? backFile->GetName() : ""; }
  const char* GetFullPath() const override { return backFile ? backFile->GetFullPath() : ""; }
  std::size_t Read(void* data, std::size_t length) override;
  std::size_t Write(const void* data, std::size_t length) override;
  std::size_t ReadOfs(std::int64_t offset, void* data, std::size_t length) override;
  std::size_t WriteOfs(std::int64_t offset, const void* data, std::size_t length) override;
  std::int64_t Length() const override;
  void SetLength(std::uint64_t length) override;
  std::int64_t Tell() const override;
  int Seek(std::int64_t offset, fsOrigin_t origin) override;
  std::uint32_t Timestamp() const override { return backFile ? backFile->Timestamp() : 0; }
  bool IsWritable() const override { return bufferMode == BUFFERMODE_WRITE; }
  bool Lock(std::int64_t offset, std::size_t length, fsLock_t mode) override;
  bool Unlock(std::int64_t offset, std::size_t length) override;
  void Flush() override;
  void ForceFlush() override;
  fsDevice_t GetDevice() const override {
    return backFile ? backFile->GetDevice() : FS_DEVICE_MEMORY;
  }
  bool IsOSNative() const override { return backFile && backFile->IsOSNative(); }
  bool WriteOut();
  bool Finalize();

  idFile* backFile;
  BufferMode_t bufferMode;
  bool deAllocBuffer;
  bool deleteFile;
  std::int64_t bufferSize;
  std::uint8_t* buffer;
  std::int64_t bufferPos;
  std::int64_t bufferOffSet;
};
static_assert(sizeof(idFile_Buffered) == 64, "buffered file ABI drift");
