#pragma once

#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/str.h"

#include <cstdint>

class idFile;
class idFile_Memory;
class idFile_Stat;
class idGame;
class idGameSpawnInfo;
struct idVideoDesc;

class idResourceManager {
public:
  virtual ~idResourceManager();
  virtual void Init() {}
  virtual void Init2(bool) {}
  virtual void Shutdown() {}
  virtual void CancelToTerminate() {}
  virtual void CloseOpenFileHandles() {}
  virtual bool ReOpenFileHandles() { return false; }
  virtual void OpenDLCFileHandles() {}
  virtual void StartupComplete() const {}
  virtual void SetFileHook() const {}
  virtual void ReleaseFileHook() const {}
  virtual void EnableInGameResourceLoad() {}
  virtual void DisableInGameResourceLoad() {}
  virtual bool AllocGameWithResourceFile(idGame*&, const idGameSpawnInfo&,
                                         idFile*, const idVideoDesc*) {
    return false;
  }
  virtual idFile_Memory* LoadCacheFile(const char*) { return nullptr; }
  virtual idFile_Stat* StatCacheFile(const char*) { return nullptr; }
  virtual bool GetCacheFileInfo(const char*, std::int64_t&, std::uint64_t&,
                                std::uint64_t&, std::uint32_t&,
                                std::uint8_t&, bool) {
    return false;
  }
  virtual void GetFilesMatchingPrefix(const char*, idList<idStr, 5>&) {}
  virtual int GetContainerIdForCacheFile(const char*) { return -1; }
  virtual int GetContainerIdForMap(const char*) { return -1; }
  virtual idFile* GetCacheFile(int) { return nullptr; }
  virtual bool IsContainerDLC(int) const { return false; }
  virtual idFile* GetStreamFile(int) { return nullptr; }
  virtual idFile* GetCachedStreamFile() { return nullptr; }
  virtual idFile* GetLocalizedStreamFile(int) { return nullptr; }
  virtual bool IsResourceFile(idFile*) { return false; }
  virtual unsigned GetBuildPlatforms() { return 0; }
  virtual void BeginEntity(const char*, const idList<idStr, 5>&) {}
  virtual void EndEntity() {}
  virtual void RegisterBinaryModelUse(int) {}
  virtual bool GetBinaryModelMask(int) { return false; }
  virtual void RegisterStandAloneFileUse(const char*) {}
  virtual void AddMaterialPairMustBeInTheSameVmtr(const char*, const char*) {}
  virtual const idList<idStr, 5>& GetLanguages() {
    static idList<idStr, 5> empty;
    return empty;
  }
  virtual bool ExcludeImage(const char*) { return false; }
  virtual bool ExcludeFont(const char*) { return false; }
  virtual int GetCurrentDiscNumber() { return 0; }
  virtual void SetResourceFilePlatform(unsigned) {}
  virtual unsigned GetResourceFilePlatform() { return GetResourceFilePlatformCompat(); }

  bool GetCacheFileInfo(const char* name, std::int64_t* offset,
                        std::uint64_t* compressedLength,
                        std::uint64_t* length, std::uint32_t* timestamp,
                        std::uint8_t* fileId, bool localized) {
    if (offset == nullptr || compressedLength == nullptr || length == nullptr ||
        timestamp == nullptr || fileId == nullptr)
      return false;
    return GetCacheFileInfo(name, *offset, *compressedLength, *length,
                            *timestamp, *fileId, localized);
  }
  bool IsResourceFile() const {
    return const_cast<idResourceManager*>(this)->IsResourceFile(nullptr);
  }
  unsigned GetBuildPlatforms() const {
    return const_cast<idResourceManager*>(this)->GetBuildPlatforms();
  }
  virtual unsigned GetResourceFilePlatformCompat() const { return 0; }
};

extern idResourceManager* resourceManager;
