#pragma once

#include "engine/framework/resourcemanager.h"
#include "engine/framework/resourcemanager_background.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/dict.h"
#include "shared/idlib/text/atomicstring.h"

#include <cstddef>

class idResource;
enum videoFlags_t : int;

struct idResourceCacheEntry {
  idResourceCacheEntry();
  idAtomicString filename;
  int offset;
  int streamOffset;
  int compressedLength;
  int uncompressedLength;
  unsigned char fileId;
  unsigned char padding[7];
};

struct idStreamedResourceMapABI {
  idStreamedResourceMapABI();
  idHashIndex hash;
  idList<void*,5> usedValues;
  idList<int,5> values;
};

struct idResourceContainer {
  struct resourceTableInfo_t {
    resourceTableInfo_t();
    int tableLength;
    int tableOffset;
    unsigned int resourceMagic;
  };

  idResourceContainer();
  idResourceContainer(const idResourceContainer& other);
  ~idResourceContainer();
  idResourceContainer& operator=(const idResourceContainer& other);

  resourceTableInfo_t resourceTableInfo;
  int padding0;
  idFile* indexFile;
  idFile* resourceFile;
  idStr resourceFileName;
  idFile* streamFile;
  idFile* localizedStreamFile;
  idList<idResourceCacheEntry,99> cacheTable;
  idHashIndex cacheHash;
  int chunkId;
  int dlcId;
};

class idResourceManagerLocal : public idResourceManager {
public:
  idResourceManagerLocal();
  ~idResourceManagerLocal() override;
  void Init() override;
  void Init2(bool fromRestart) override;
  void Shutdown() override;
  void CancelToTerminate() override;
  void CloseOpenFileHandles() override;
  bool ReOpenFileHandles() override;
  void OpenDLCFileHandles() override;
  void StartupComplete() const override;
  void SetFileHook() const override;
  void ReleaseFileHook() const override;
  void EnableInGameResourceLoad() override;
  void DisableInGameResourceLoad() override;
  idFile_Memory* LoadCacheFile(const char* name) override;
  idFile_Stat* StatCacheFile(const char* name) override;
  bool GetCacheFileInfo(const char* name,std::int64_t& offset,
      std::uint64_t& compressedLength,std::uint64_t& length,
      std::uint32_t& timestamp,std::uint8_t& fileId,bool localized) override;
  void GetFilesMatchingPrefix(const char* prefix,idList<idStr,5>& files) override;
  int GetContainerIdForCacheFile(const char* name) override;
  int GetContainerIdForMap(const char* name) override;
  idFile* GetCacheFile(int containerId) override;
  bool IsContainerDLC(int containerId) const override;
  idFile* GetStreamFile(int containerId) override;
  idFile* GetCachedStreamFile() override;
  idFile* GetLocalizedStreamFile(int containerId) override;
  bool IsResourceFile(idFile* file) override;
  unsigned GetBuildPlatforms() override;
  bool GetBinaryModelMask(int index) override;
  const idList<idStr,5>& GetLanguages() override;
  bool ExcludeImage(const char* name) override;
  int GetCurrentDiscNumber() override;
  void SetResourceFilePlatform(unsigned platform) override;
  unsigned GetResourceFilePlatform() override;

  int GetNeededResourcesForMap(
      const char* mapName,
      const idStaticList<idStrStatic<128>,256>* layersActive,
      const idStaticList<idStrStatic<128>,256>* layersDeactive,
      idList<int,5>& neededResources,idList<idStr,5>& neededMegatextures);
private:
  void WaitForVideoOrBackgroundSwaps();
  void DefaultMissing(const char* name);
  void InitStreamContainers(const char* rootPath,idResourceContainer& container,
                            bool setLang);
  void InitResourceContainers();
  void InitResourceContainer(idResourceContainer& container);
  bool LoadNamedResourceBlock(
      const char* resourceFileName,const char* mapName,
      const idStaticList<idStrStatic<128>,256>* layersActive,
      const idStaticList<idStrStatic<128>,256>* layersDeactive,
      const idVideoDesc* videoDesc,videoFlags_t videoFlags,bool initHDC);
public:
  bool AllocGameWithResourceFileRaw(idGame*& game,
      const idGameSpawnInfo& spawnInfo,idFile* saveGameFile,
      const idVideoDesc* videoDesc);
  bool AllocGameWithResourceFile(idGame*& game,
      const idGameSpawnInfo& spawnInfo,idFile* saveGameFile,
      const idVideoDesc* videoDesc) override;

  idFile* resourceMasterIndexFile;
  idStaticList<idResourceContainer,100> resourceContainers;
  idFile* cachedStreamFile;
  idStreamedResourceMapABI streamedResourceMap;
  idList<idDict,5> resourceFilenameAliases;
  idResource* currentSortedResource;
  idResource* currentResource;
  int currentResourceFileId;
  int currentBuildGameChunkPageSource;
  idList<idList<idResourceFileEntry,5>,99> writtenResourceTable;
  idList<idResourceFileEntry,99> fileTable;
  int fileTableResource;
  int padding0;
  idList<bool,5> binaryModelLoadMask;
  unsigned filePlatformBits;
  unsigned buildPlatforms;
  int buildChunk;
  int buildDisc;
  int lastUPacifierMsec;
  int padding1;
  idList<idStr,5> languages;
  bool denyFonts;
  bool inGameLoading;
  unsigned char padding2[2];
  int inGameLoadingPrevProductionMode;
  bool cancelToTerminate;
  unsigned char padding3[7];
};

extern idResourceManagerLocal resourceManagerLocal;

static_assert(sizeof(idResourceCacheEntry)==32,"resource cache entry ABI drift");
static_assert(sizeof(idResourceContainer::resourceTableInfo_t)==12,
              "resource table info ABI drift");
static_assert(sizeof(idResourceContainer)==160,"resource container ABI drift");
static_assert(sizeof(idStreamedResourceMapABI)==88,
              "streamed resource map ABI drift");
static_assert(sizeof(idResourceManagerLocal)==16328,
              "resource manager local ABI drift");
