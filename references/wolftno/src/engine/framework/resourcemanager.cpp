#include "engine/framework/resourcemanager_local.h"

#include "engine/decls/decl.h"
#include "engine/decls/declmapinfo.h"
#include "engine/framework/common.h"
#include "engine/framework/common_local.h"
#include "engine/framework/coderesource.h"
#include "engine/framework/fileresource.h"
#include "engine/framework/gamespawninfo.h"
#include "engine/framework/gamesystem.h"
#include "engine/framework/session.h"
#include "engine/framework/streamfilecache.h"
#include "engine/framework/usercmdgen.h"
#include "engine/renderer/renderersystem.h"
#include "engine/renderer/rendersystem_bink.h"
#include "engine/renderer/virtualtexture.h"
#include "engine/renderer/virtualtexturepreloader.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/sound/soundsystem.h"
#include "engine/sys/sys_public.h"
#include "engine/sys/sys_streaming_install.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/containers/heaparray.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/sys/sys_time.h"

#include <cstring>
#include <intrin.h>
#include <new>

extern idCVar pc_currentDisc;
extern idCVar joy_pitchSpeed;
extern idCVar joy_yawSpeed;
extern idCVar in_invertLook;
extern idCVar m_sensitivity;
extern idCVar vt_filePathVmtrOverride;
extern idCVar vt_filePath;
extern idCVar feature_lowgore;
extern idCVar com_useMapHeap;
extern idCVar bink_dontfree;
extern void* ZlibAlloc(void* opaque,unsigned int items,unsigned int size);
extern void ZlibFree(void* opaque,void* address);

const char* const RESOURCE_STREAM_NAME="streamed.resources";
const char* const JAPANESE_STREAM="japanese.streamed";
const char* const FRENCH_STREAM="french.streamed";
const char* const GERMAN_STREAM="german.streamed";
const char* const ENGLISH_STREAM="english.streamed";
const char* const RUSSIAN_LANG="russian";
const char* const JAPANESE_LANG="japanese";
const char* const FRENCH_LANG="french";
const char* const ENGLISH_LANG="english";
const char* const POLISH_LANG="polish";
const char* const GERMAN_LANG="german";
const char* const RESOURCE_MASTER_INDEX_FILE_NAME="master.index";
const char* const RESOURCE_INDEX_FILE_NAME="gameresources.index";
const char* const RESOURCE_FILE_NAME="gameresources.resources";
const char* const fontPrefix="generated/textures/borderclamp_alpha_fonts";
const char* const validFontNames[9]={
    "japanese/Futura_Com_Medium_JPN90","arial","courier","arial_black",
    "cyrillic/idakagi_semibold","cyrillic/idginza_narrow_light",
    "cyrillic/idginza_narrow_medium","futura_com","futura_com_book"};

idCVar resource_loadLogging(
    "resource_loadLogging","0",CVAR_INTEGER,
    "Set to 1 to get binary resolve logging",nullptr);
idCVar resource_debugLevel(
    "resource_debugLevel","0",CVAR_INTEGER,
    "Set to 1 for huge console spew",nullptr);
idCVar resource_ignoreLayers(
    "resource_ignoreLayers","0",CVAR_BOOL,
    "ignore layer bits and load all layers during map loads",nullptr);
idCVar resource_virtualTexturePreload(
    "resource_virtualTexturePreload","1",CVAR_BOOL,
    "run the virtual texture preloader in parallel with game init",nullptr);
idCVar r_amdCrashReproDelay(
    "r_amdCrashReproDelay","0",CVAR_FLOAT,
    "Insert a delay before the resource load from resource file-load to trigger amd driver hang/crash",
    nullptr);
idCVar skipLoadingVideoAutomatically(
    "skipLoadingVideoAutomatically","0",CVAR_BOOL,
    "if set, will skip the loading video as soon as the map is loaded",nullptr);
idCVar mem_phyMemBlockSizeM(
    "mem_phyMemBlockSizeM","167",CVAR_INIT|CVAR_INTEGER,
    "Size (in MB) of the physical memory block",nullptr);
idCVar mem_UseTwoDiffusePools(
    "mem_UseTwoDiffusePools","1",CVAR_INIT|CVAR_BOOL,
    "Use two diffuse pools for the wastelands",nullptr);

struct idLayerBits { unsigned char bits[24]; };
static_assert(sizeof(idLayerBits)==24,"map layer bits ABI drift");

static void DevelopmentResourceLoadCallback(idResource*) {}

class ScopedResourceSystemHeap {
public:
  ScopedResourceSystemHeap() { idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP); }
  ~ScopedResourceSystemHeap() { idMem::Singleton()->PopHeap(); }
};

idResourceManagerLocal resourceManagerLocal;
idResourceManager* resourceManager=&resourceManagerLocal;

idResourceManager::~idResourceManager()=default;

idStreamedResourceMapABI::idStreamedResourceMapABI()
    : hash(0x4000,0x8000) {
  hash.memTag=TAG_HASHINDEX;
  usedValues.SetNum(0x8000);
  values.SetNum(0x8000);
  std::memset(usedValues.list,0,
              sizeof(*usedValues.list)*static_cast<std::size_t>(usedValues.size));
}

idResourceManagerLocal::idResourceManagerLocal() {
  cachedStreamFile=nullptr;
  currentSortedResource=nullptr;
  currentResource=nullptr;
  currentResourceFileId=0;
  currentBuildGameChunkPageSource=0;
  fileTableResource=0;
  filePlatformBits=0;
  buildPlatforms=0;
  buildChunk=0;
  buildDisc=0;
  lastUPacifierMsec=0;
  denyFonts=false;
  inGameLoading=false;
  inGameLoadingPrevProductionMode=PROD_DEVELOPMENT;
  cancelToTerminate=false;
}

idResourceManagerLocal::~idResourceManagerLocal()=default;

idFile_Memory* CacheFilePreCallback(const char* const filename) {
  return resourceManager->LoadCacheFile(filename);
}

idFile_Stat* StatCacheFilePreCallback(const char* const filename) {
  return resourceManager->StatCacheFile(filename);
}

void idResourceManagerLocal::Init() {
  buildPlatforms=0;
  bgl.SpawnThreads();
  denyFonts=false;
  idLib::Printf("idLib::SetProduction( PROD_PRODUCTION )\n");
  idLib::production=PROD_PRODUCTION;
  ReOpenFileHandles();
}

void idResourceManagerLocal::Shutdown() {
  bgl.DestroyThreads();
}

void idResourceManagerLocal::SetFileHook() const {
  fileSystem->SetFileCacheCallbackRetail(
      reinterpret_cast<void*>(&CacheFilePreCallback),
      reinterpret_cast<void*>(&StatCacheFilePreCallback));
}

void idResourceManagerLocal::ReleaseFileHook() const {
  fileSystem->SetFileCacheCallbackRetail(nullptr,nullptr);
}

idFile* idResourceManagerLocal::GetCachedStreamFile() {
  return cachedStreamFile;
}

static idFile* ResourceLoadPreCallback(const char* filename) {
  return bgl.GetFile(filename);
}

void idResourceManagerLocal::StartupComplete() const {
  resourceLoadCallback=nullptr;
}

idHashIndex::idHashIndex(const idHashIndex& other)
    : hash(INTERNAL_INVALID_INDEX),indexChain(INTERNAL_INVALID_INDEX),
      hashSize(other.hashSize),indexSize(other.indexSize),
      granularity(other.granularity),hashMask(other.hashMask),
      lookupMask(0),memTag(other.memTag) {
  *this=other;
}

idHashIndex& idHashIndex::operator=(const idHashIndex& other) {
  if(this==&other) return *this;
  granularity=other.granularity;
  hashMask=other.hashMask;
  lookupMask=other.lookupMask;
  if(other.lookupMask==0) {
    hashSize=other.hashSize;
    indexSize=other.indexSize;
    InternalFree();
    return *this;
  }
  if(other.hashSize!=hashSize || hash==INTERNAL_INVALID_INDEX) {
    if(hash!=INTERNAL_INVALID_INDEX)
      idMem::Singleton()->Free(hash,ALIGN_16);
    hashSize=other.hashSize;
    hash=static_cast<int*>(idMem::Singleton()->AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\containers/HashIndex.h(212) : memTag",
        sizeof(int)*static_cast<std::size_t>(hashSize),
        static_cast<memTag_t>(memTag),false,ALIGN_16,HEAP_DEFAULTHEAP));
  }
  if(other.indexSize!=indexSize || indexChain==INTERNAL_INVALID_INDEX) {
    if(indexChain!=INTERNAL_INVALID_INDEX)
      idMem::Singleton()->Free(indexChain,ALIGN_16);
    indexSize=other.indexSize;
    indexChain=static_cast<int*>(idMem::Singleton()->AllocWithLocation(
        "w:\\build\\tech5\\shared\\idlib\\containers/HashIndex.h(219) : memTag",
        sizeof(int)*static_cast<std::size_t>(indexSize),
        static_cast<memTag_t>(memTag),false,ALIGN_16,HEAP_DEFAULTHEAP));
  }
  std::memmove(hash,other.hash,sizeof(int)*static_cast<std::size_t>(hashSize));
  std::memmove(indexChain,other.indexChain,
               sizeof(int)*static_cast<std::size_t>(indexSize));
  return *this;
}

idFile_Stat::idFile_Stat(const char* name,std::int64_t size,
                         std::uint32_t timestamp)
    : idFile(),fileName(name),fileSize(size),fileTimestamp(timestamp) {}

std::int64_t idFile_Stat::Length() const { return fileSize; }

int idResourceManagerLocal::GetCurrentDiscNumber() {
  return pc_currentDisc.valueInteger;
}

void idResourceManagerLocal::CloseOpenFileHandles() {
  for(int index=0;index<resourceContainers.Num();++index) {
    idResourceContainer& container=resourceContainers[index];
    delete container.streamFile;
    container.streamFile=nullptr;
    delete container.resourceFile;
    container.resourceFile=nullptr;
    delete container.indexFile;
    container.indexFile=nullptr;
    delete container.localizedStreamFile;
    container.localizedStreamFile=nullptr;
  }
  delete cachedStreamFile;
  cachedStreamFile=nullptr;
}

idFile_Memory* MiscellaneousFilePreCallback(const char* const filename) {
  idStr canonical(filename);
  canonical.MakeNameCanonical();
  idFileResource* existing=static_cast<idFileResource*>(
      idFileResource::resourceList.FindExisting(canonical.c_str(),false));
  if(existing!=nullptr)
    return existing->GetFileReadOnly();
  idLib::Warning("File '%s' wasn't preloaded, trying to slow load it from disc",
                 canonical.c_str());
  return resourceManager->LoadCacheFile(filename);
}

void idResourceManagerLocal::EnableInGameResourceLoad() {
  inGameLoadingPrevProductionMode=idLib::production;
  if(idLib::production==PROD_LOADED)
    idLib::production=PROD_PRODUCTION;
  SetFileHook();
  idResourceList::inGameLoading=true;
  inGameLoading=true;
}

void idResourceManagerLocal::DisableInGameResourceLoad() {
  if(inGameLoading) {
    idLib::production=inGameLoadingPrevProductionMode;
    ReleaseFileHook();
    idResourceList::inGameLoading=false;
    inGameLoading=false;
  }
}

bool idResourceManagerLocal::IsResourceFile(idFile* file) {
  for(int index=0;index<resourceContainers.Num();++index)
    if(resourceContainers[index].resourceFile==file) return true;
  return false;
}

idFile* idResourceManagerLocal::GetCacheFile(int containerId) {
  if(containerId<0 || containerId>=resourceContainers.Num()) return nullptr;
  return resourceContainers[containerId].resourceFile;
}

idFile* idResourceManagerLocal::GetStreamFile(int containerId) {
  if(containerId<0 || containerId>=resourceContainers.Num() ||
     resourceContainers[containerId].streamFile==nullptr)
    return resourceContainers[0].streamFile;
  return resourceContainers[containerId].streamFile;
}

idFile* idResourceManagerLocal::GetLocalizedStreamFile(int containerId) {
  if(containerId<0 || containerId>=resourceContainers.Num() ||
     resourceContainers[containerId].streamFile==nullptr)
    return resourceContainers[0].localizedStreamFile;
  return resourceContainers[containerId].localizedStreamFile;
}

bool idResourceManagerLocal::IsContainerDLC(int containerId) const {
  return containerId>=0 && containerId<resourceContainers.Num() &&
         resourceContainers[containerId].dlcId>=1;
}

idFile_Stat* idResourceManagerLocal::StatCacheFile(const char* fileName) {
  if(cancelToTerminate) return nullptr;
  std::int64_t offset;
  std::uint64_t compressedSize;
  std::uint64_t uncompressedSize;
  std::uint32_t streamOffset;
  std::uint8_t fileId;
  if(!resourceManager->GetCacheFileInfo(
         fileName,offset,compressedSize,uncompressedSize,streamOffset,fileId,true))
    return nullptr;
  return new idFile_Stat(fileName,static_cast<std::int64_t>(uncompressedSize),0);
}

void idResourceManagerLocal::WaitForVideoOrBackgroundSwaps() {
  if(renderSystem->CanThreadContinue()) {
    renderSystem->FreeBinkVideo();
    return;
  }

  inputSettings_t settings{};
  settings.mouseSensScale=1.0f;
  settings.joySensScale=1.0f;
  settings.bindset=BIND_DEFAULT;
  settings.absoluteAngles=false;
  settings.joyPitchSpeed=joy_pitchSpeed.valueFloat;
  settings.joyYawSpeed=joy_yawSpeed.valueFloat;
  settings.lookInvert=in_invertLook.valueInteger!=0;
  settings.mouseSens=m_sensitivity.valueFloat;
  settings.aimAssistAngles={};
  for(int device=0;device<4;++device)
    (void)usercmdGen->GetUsercmd(device,settings,false,0);
  sys->GenerateEvents();
  sys->ClearEvents();

  bool keepWaiting=true;
  while(!renderSystem->CanThreadContinue()) {
    if(!keepWaiting || cancelToTerminate || common->IsDialogActive() ||
       skipLoadingVideoAutomatically.valueInteger!=0)
      break;
    if(renderSystem->HasRenderFlag(RENDERFLAG_CANEXITCUTSCENE)) {
      for(int device=0;device<4;++device)
        (void)usercmdGen->GetUsercmd(device,settings,false,0);
      sys->GenerateEvents();
      int count=0;
      for(;;) {
        const sysEvent_t event=sys->GetEvent();
        const int masterDevice=common->GetMasterLocalUserInputDevice();
        if(masterDevice==-1) {
          keepWaiting=false;
          break;
        }
        if(masterDevice==event.inputDevice &&
           !commonLocal.HandleDialogEvent(&event)) {
          const bool pressAlready=
              renderSystem->HasRenderFlag(RENDERFLAG_NEXTPRESSEXITS) ||
              !renderSystem->HasRenderFlag(RENDERFLAG_BINKSTARTED);
          if(commonLocal.CutScenePress(event,pressAlready)) {
            keepWaiting=false;
            sys->ClearEvents();
            break;
          }
        }
        if(++count>=20) break;
      }
      sys->ClearEvents();
    }
    session->UpdateSignInManager();
    session->PumpSystemServiceStatus();
    Sys_Sleep(10);
  }
  usercmdGen->ClearAngles();
  if(!renderSystem->HasRenderFlag(RENDERFLAG_BINKSTARTED)) {
    renderSystem->RestartFromLoading();
    commonLocal.ForceLoadingBlack();
  } else {
    renderSystem->FreeBinkVideo();
    renderSystem->EndAutomaticBackgroundSwaps();
  }
}

int idResourceManagerLocal::GetContainerIdForMap(const char* mapName) {
  idStrStatic<256> mapResourceName;
  mapResourceName.Format("generated/buildGame/%s.mapResources",mapName);
  return GetContainerIdForCacheFile(mapResourceName.c_str());
}

void idResourceManagerLocal::DefaultMissing(const char* name) {
  for(idResourceList* list=idResourceList::GetNextResourceList();
      list!=nullptr;list=list->GetNextList()) {
    const int count=list->Num();
    for(int index=0;index<count;++index) {
      idResource* resource=list->Index(index);
      if(resource->GetLoadDeferred()) {
        resource->SetResourceError(
            "LoadNamedResourceBlock %s missing from %s but referenced",
            resource->GetName(),name);
        list->Load(resource->GetName(),true,false);
      }
    }
  }
}

idResourceCacheEntry::idResourceCacheEntry()
    : filename(),offset(0),streamOffset(0),compressedLength(0),
      uncompressedLength(0),fileId(0) {}

idResourceContainer::resourceTableInfo_t::resourceTableInfo_t()
    : tableLength(0),tableOffset(0),resourceMagic(0) {}

idResourceContainer& idResourceContainer::operator=(
    const idResourceContainer& other) {
  if(this==&other) return *this;
  resourceTableInfo=other.resourceTableInfo;
  indexFile=other.indexFile;
  resourceFile=other.resourceFile;
  resourceFileName=other.resourceFileName;
  streamFile=other.streamFile;
  localizedStreamFile=other.localizedStreamFile;
  cacheTable=other.cacheTable;
  cacheHash=other.cacheHash;
  chunkId=other.chunkId;
  dlcId=other.dlcId;
  return *this;
}

idResourceFileEntry& idResourceFileEntry::operator=(
    const idResourceFileEntry& other) {
  if(this==&other) return *this;
  resourceIndex=other.resourceIndex;
  resourceList=other.resourceList;
  typeName=other.typeName;
  resourceName=other.resourceName;
  filename=other.filename;
  offset=other.offset;
  compressedLength=other.compressedLength;
  uncompressedLength=other.uncompressedLength;
  stream=other.stream;
  useBits=other.useBits;
  fileId=other.fileId;
  return *this;
}

idResourceFileEntry::idResourceFileEntry()
    : resourceIndex(-1),resourceList(nullptr),typeName(),resourceName(),
      filename(),offset(0),compressedLength(0),uncompressedLength(0),stream(),
      useBits(0),fileId(0) {}

idResourceFileEntry::idResourceFileEntry(const idResourceFileEntry& other)
    : idResourceFileEntry() { *this=other; }

idResourceFileEntry::~idResourceFileEntry()=default;

idResourceContainer::idResourceContainer()
    : resourceTableInfo(),indexFile(nullptr),resourceFile(nullptr),
      resourceFileName(),streamFile(nullptr),localizedStreamFile(nullptr),
      cacheTable(),cacheHash(1024,1024),chunkId(0),dlcId(-1) {}

idResourceContainer::idResourceContainer(const idResourceContainer& other)
    : idResourceContainer() { *this=other; }

idResourceContainer::~idResourceContainer()=default;

void idResourceManagerLocal::InitStreamContainers(
    const char* rootPath,idResourceContainer& container,bool setLang) {
  delete container.streamFile;
  container.streamFile=nullptr;
  idFile* streamFile=
      fileSystem->OpenOSFileReadRetail(rootPath,RESOURCE_STREAM_NAME);
  if(streamFile==nullptr)
    idLib::Error("Missing %s in production\n",RESOURCE_STREAM_NAME);
  container.streamFile=streamFile;

  delete container.localizedStreamFile;
  container.localizedStreamFile=nullptr;
  idStrStatic<32> localizedName;
  localizedName.Format("%s.streamed",sys_lang.valueString.c_str());
  if(idStr::Icmp(sys_lang.valueString.c_str(),RUSSIAN_LANG)==0 ||
     idStr::Icmp(sys_lang.valueString.c_str(),POLISH_LANG)==0) {
    SetFileHook();
    if(fileSystem->FileExistsRetail(
           va("strings/%s.lang",sys_lang.valueString.c_str()),true))
      localizedName=ENGLISH_STREAM;
    ReleaseFileHook();
  }

  idFile* localized=
      fileSystem->OpenOSFileReadRetail(rootPath,localizedName.c_str());
  if(localized!=nullptr) {
    if(localized->Length()>0) {
      container.localizedStreamFile=localized;
      return;
    }
    delete localized;
  }

  const char* selectedLanguage=nullptr;
  if(fileSystem->OSFileExistsRetail(rootPath,JAPANESE_STREAM,true)) {
    localized=fileSystem->OpenOSFileReadRetail(rootPath,JAPANESE_STREAM);
    selectedLanguage=JAPANESE_LANG;
  } else if(fileSystem->OSFileExistsRetail(rootPath,GERMAN_STREAM,true)) {
    localized=fileSystem->OpenOSFileReadRetail(rootPath,GERMAN_STREAM);
    selectedLanguage=GERMAN_LANG;
  } else if(fileSystem->OSFileExistsRetail(rootPath,FRENCH_STREAM,true) &&
            !fileSystem->OSFileExistsRetail(rootPath,ENGLISH_STREAM,true)) {
    localized=fileSystem->OpenOSFileReadRetail(rootPath,FRENCH_STREAM);
    selectedLanguage=FRENCH_LANG;
  } else {
    localized=fileSystem->OpenOSFileReadRetail(rootPath,ENGLISH_STREAM);
    selectedLanguage=ENGLISH_LANG;
  }
  if(setLang) sys_lang.SetString(selectedLanguage,true);
  if(localized==nullptr) idLib::Warning("localized stream file not found!");
  container.localizedStreamFile=localized;
}

bool idResourceManagerLocal::ExcludeImage(const char* name) {
  if(denyFonts && idLib::production>=PROD_PRODUCTION &&
     idStr::Icmpn(name,fontPrefix,idStr::Length(fontPrefix))==0) {
    const int validCount=
        idStr::Icmp(sys_lang.valueString.c_str(),JAPANESE_LANG)==0 ? 3 : 9;
    idStrStatic<16> languagePrefix("");
    int index=0;
    for(;index<validCount;++index) {
      idStrStatic<128> validFontName(fontPrefix);
      if(index>=3) validFontName.AppendPath(languagePrefix.c_str());
      validFontName.AppendPath(validFontNames[index]);
      if(idStr::Icmpn(validFontName.c_str(),name,validFontName.Length())==0)
        break;
    }
    if(index==validCount) return true;
  }
  return false;
}

int idResourceManagerLocal::GetContainerIdForCacheFile(const char* fileName) {
  for(int containerIndex=0;containerIndex<resourceContainers.Num();
      ++containerIndex) {
    idStrStatic<256> canonical(fileName);
    canonical.MakeNameCanonical();
    idResourceContainer& container=resourceContainers[containerIndex];
    const int hashValue=idStr::Hash(canonical.c_str());
    for(int entryIndex=container.cacheHash.First(hashValue);
        entryIndex!=-1;entryIndex=container.cacheHash.Next(entryIndex)) {
      if(idStr::Icmp(container.cacheTable[entryIndex].filename.str,
                     canonical.c_str())==0)
        return containerIndex;
    }
  }
  return -1;
}

bool idResourceManagerLocal::GetCacheFileInfo(
    const char* fileName,std::int64_t& offset,std::uint64_t& compressedSize,
    std::uint64_t& uncompressedSize,std::uint32_t& streamOffset,
    std::uint8_t& fileId,bool allowMiss) {
  offset=0;
  uncompressedSize=0;
  compressedSize=0;
  fileId=0;
  if(resourceContainers.Num()==0) return false;
  for(int containerIndex=0;containerIndex<resourceContainers.Num();
      ++containerIndex) {
    idStrStatic<256> canonical(fileName);
    canonical.MakeNameCanonical();
    idResourceContainer& container=resourceContainers[containerIndex];
    const int hashValue=idStr::Hash(canonical.c_str());
    for(int entryIndex=container.cacheHash.First(hashValue);
        entryIndex!=-1;entryIndex=container.cacheHash.Next(entryIndex)) {
      const idResourceCacheEntry& entry=container.cacheTable[entryIndex];
      if(idStr::Icmp(entry.filename.str,canonical.c_str())==0) {
        offset=static_cast<unsigned int>(entry.offset);
        uncompressedSize=entry.uncompressedLength;
        compressedSize=entry.compressedLength;
        streamOffset=static_cast<unsigned int>(entry.streamOffset);
        fileId=static_cast<unsigned char>(containerIndex);
        return true;
      }
    }
  }
  if(!allowMiss)
    idLib::FatalError("GetCacheFileInfo: %s not found",fileName);
  idLib::Warning("%s was not in the cache table",fileName);
  return false;
}

idFile_Memory* idResourceManagerLocal::LoadCacheFile(const char* fileName) {
  if(cancelToTerminate) return nullptr;
  std::int64_t offset;
  std::uint64_t compressedSize;
  std::uint64_t uncompressedSize;
  std::uint32_t streamOffset;
  std::uint8_t fileId;
  if(!resourceManager->GetCacheFileInfo(
         fileName,offset,compressedSize,uncompressedSize,streamOffset,fileId,true) ||
     resourceContainers[fileId].resourceFile==nullptr)
    return nullptr;

  char* data=static_cast<char*>(idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\framework\\ResourceManager.cpp(1206) : TAG_RESOURCE",
      static_cast<std::size_t>(uncompressedSize),TAG_RESOURCE,false,ALIGN_16,
      HEAP_DEFAULTHEAP));
  void* fileMemory=idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(95) : tag",
      sizeof(idFile_Memory),TAG_RESOURCE,false,ALIGN_16,HEAP_DEFAULTHEAP);
  idFile_Memory* result=fileMemory!=nullptr
      ? ::new(fileMemory) idFile_Memory(fileName) : nullptr;
  result->SetReadOnlyData(data,uncompressedSize);
  result->ownsData=true;

  if(uncompressedSize==compressedSize) {
    streamFileCache->UncachedScheduledRead(
        resourceContainers[fileId].resourceFile,offset,data,
        static_cast<int>(uncompressedSize));
    return result;
  }

  idHeapArray<unsigned char> compressedData;
  compressedData.Alloc(static_cast<unsigned int>(compressedSize));
  streamFileCache->UncachedScheduledRead(
      resourceContainers[fileId].resourceFile,offset,compressedData.buffer,
      static_cast<int>(compressedSize));
  z_stream z{};
  z.zalloc=ZlibAlloc;
  z.zfree=ZlibFree;
  if(inflateInit2(&z,-15)!=Z_OK) {
    idLib::Warning("zlib inflateInit2 error");
    return nullptr;
  }
  z.avail_in=static_cast<uInt>(compressedSize);
  z.avail_out=static_cast<uInt>(uncompressedSize);
  z.next_in=compressedData.buffer;
  z.next_out=reinterpret_cast<unsigned char*>(data);
  const int inflateResult=inflate(&z,Z_FINISH);
  if(inflateResult==Z_OK ||
     z.total_out==static_cast<unsigned int>(uncompressedSize)) {
    inflateEnd(&z);
    return result;
  }
  idLib::Warning("zlib inflate error");
  return nullptr;
}

void idResourceManagerLocal::GetFilesMatchingPrefix(
    const char* prefix,idList<idStr,5>& fileNames) {
  fileNames.ClearFree();
  idStr canonicalPrefix(prefix);
  canonicalPrefix.MakeNameCanonical();
  const int prefixLength=canonicalPrefix.Length();
  for(int containerIndex=0;containerIndex<resourceContainers.Num();
      ++containerIndex) {
    const idResourceContainer& container=resourceContainers[containerIndex];
    for(int entryIndex=0;entryIndex<container.cacheTable.Num();++entryIndex) {
      const char* filename=container.cacheTable[entryIndex].filename.str;
      if(idStr::Cmpn(canonicalPrefix.c_str(),filename,prefixLength)==0) {
        idStr value(filename);
        fileNames.Append(value);
      }
    }
  }
}

std::size_t idResourceFileEntry::Read(idFile* file) {
  std::size_t bytes=file->Read(&resourceIndex,sizeof(resourceIndex));
  resourceIndex=static_cast<int>(_byteswap_ulong(
      static_cast<unsigned long>(resourceIndex)));
  bytes+=file->ReadString(typeName);
  resourceList=idResourceList::ForTypeName(typeName.c_str());
  bytes+=file->ReadString(resourceName);
  bytes+=file->ReadString(filename);
  bytes+=file->Read(&offset,sizeof(offset));
  offset=static_cast<int>(_byteswap_ulong(static_cast<unsigned long>(offset)));
  bytes+=file->Read(&uncompressedLength,sizeof(uncompressedLength));
  uncompressedLength=static_cast<int>(_byteswap_ulong(
      static_cast<unsigned long>(uncompressedLength)));
  bytes+=file->Read(&compressedLength,sizeof(compressedLength));
  compressedLength=static_cast<int>(_byteswap_ulong(
      static_cast<unsigned long>(compressedLength)));
  int count;
  bytes+=file->Read(&count,sizeof(count));
  count=static_cast<int>(_byteswap_ulong(static_cast<unsigned long>(count)));
  for(int index=0;index<count;++index) {
    idPlatformStreamData data{};
    data.streamData=nullptr;
    bytes+=file->Read(&data,16);
    bytes+=file->Read(&data.streamOffset,sizeof(data.streamOffset));
    data.streamOffset=static_cast<int>(_byteswap_ulong(
        static_cast<unsigned long>(data.streamOffset)));
    bytes+=file->Read(&data.streamLength,sizeof(data.streamLength));
    data.streamLength=static_cast<int>(_byteswap_ulong(
        static_cast<unsigned long>(data.streamLength)));
    data.streamData=nullptr;
    stream.Append(data);
  }
  bytes+=file->Read(&useBits,sizeof(useBits));
  useBits=_byteswap_ulong(useBits);
  bytes+=file->Read(&fileId,sizeof(fileId));
  return bytes;
}

void idResourceManagerLocal::InitResourceContainers() {
  for(int containerIndex=0;containerIndex<resourceContainers.Num();
      ++containerIndex) {
    idResourceContainer& container=resourceContainers[containerIndex];
    unsigned int magic;
    unsigned int tableLength;
    unsigned int unusedHeader[6];
    container.indexFile->Read(&magic,sizeof(magic));
    magic=_byteswap_ulong(magic);
    if(magic!=55788882U)
      idLib::FatalError(
          "idResourceManagerLocal::Init - index header bad magic %d",magic);
    container.indexFile->Read(&tableLength,sizeof(tableLength));
    tableLength=_byteswap_ulong(tableLength);
    container.indexFile->Read(unusedHeader,sizeof(unusedHeader));
    for(unsigned int& value:unusedHeader) value=_byteswap_ulong(value);
    container.resourceTableInfo.tableOffset=32;
    container.resourceTableInfo.tableLength=static_cast<int>(tableLength);
    container.resourceTableInfo.resourceMagic=magic;
  }

  idResourceFileEntry resourceEntry;
  for(int containerIndex=0;containerIndex<resourceContainers.Num();
      ++containerIndex) {
    idResourceContainer& container=resourceContainers[containerIndex];
    idHeapArray<char> buffer;
    buffer.Alloc(static_cast<unsigned int>(
        container.resourceTableInfo.tableLength));
    container.indexFile->ReadOfs(
        container.resourceTableInfo.tableOffset,buffer.buffer,
        static_cast<std::size_t>(container.resourceTableInfo.tableLength));
    idFile_Memory memoryFile("resourceHeader");
    memoryFile.SetReadOnlyData(
        buffer.buffer,
        static_cast<unsigned int>(container.resourceTableInfo.tableLength));
    unsigned int tableMagic;
    int entryCount;
    memoryFile.Read(&tableMagic,sizeof(tableMagic));
    tableMagic=_byteswap_ulong(tableMagic);
    memoryFile.Read(&entryCount,sizeof(entryCount));
    entryCount=static_cast<int>(_byteswap_ulong(
        static_cast<unsigned long>(entryCount)));
    container.cacheHash.Clear();
    container.cacheTable.SetNum(entryCount);
    for(int entryIndex=0;entryIndex<entryCount;++entryIndex) {
      resourceEntry.stream.ClearFree();
      resourceEntry.Read(&memoryFile);
      idResourceCacheEntry& cacheEntry=container.cacheTable[entryIndex];
      cacheEntry.filename.Set(resourceEntry.filename.c_str());
      cacheEntry.offset=resourceEntry.offset;
      cacheEntry.compressedLength=resourceEntry.compressedLength;
      cacheEntry.uncompressedLength=resourceEntry.uncompressedLength;
      cacheEntry.fileId=resourceEntry.fileId;
      if(resourceEntry.stream.Num()>0) {
        cacheEntry.streamOffset=resourceEntry.stream[0].streamOffset;
        if(resourceEntry.stream.Num()>1) {
          for(int streamIndex=0;streamIndex<resourceEntry.stream.Num();
              ++streamIndex) {
            if(idStr::Icmp(sys_lang.valueString.c_str(),
                           reinterpret_cast<const char*>(
                               resourceEntry.stream[streamIndex].lang))==0) {
              cacheEntry.streamOffset=
                  resourceEntry.stream[streamIndex].streamOffset;
              break;
            }
          }
        }
      }
      container.cacheHash.Add(
          idStr::IHash(cacheEntry.filename.str),entryIndex);
    }
  }
}

void idResourceManagerLocal::InitResourceContainer(
    idResourceContainer& container) {
  unsigned int header[8]{};
  container.indexFile->Read(header,sizeof(header));
  for(unsigned int& value:header) value=_byteswap_ulong(value);
  if(header[0]!=55788882U)
    idLib::FatalError(
        "idResourceManagerLocal::Init - index header bad magic %d",header[0]);
  container.resourceTableInfo.tableOffset=32;
  container.resourceTableInfo.tableLength=static_cast<int>(header[1]);
  container.resourceTableInfo.resourceMagic=header[0];

  idResourceFileEntry resourceEntry;
  idHeapArray<char> buffer;
  buffer.Alloc(static_cast<unsigned int>(
      container.resourceTableInfo.tableLength));
  container.indexFile->ReadOfs(
      container.resourceTableInfo.tableOffset,buffer.buffer,
      static_cast<std::size_t>(container.resourceTableInfo.tableLength));
  idFile_Memory memoryFile("resourceHeader");
  memoryFile.SetReadOnlyData(
      buffer.buffer,
      static_cast<unsigned int>(container.resourceTableInfo.tableLength));
  unsigned int tableMagic;
  int entryCount;
  memoryFile.Read(&tableMagic,sizeof(tableMagic));
  tableMagic=_byteswap_ulong(tableMagic);
  memoryFile.Read(&entryCount,sizeof(entryCount));
  entryCount=static_cast<int>(_byteswap_ulong(
      static_cast<unsigned long>(entryCount)));
  container.cacheHash.Clear();
  container.cacheTable.SetNum(entryCount);
  for(int entryIndex=0;entryIndex<entryCount;++entryIndex) {
    resourceEntry.stream.ClearFree();
    resourceEntry.Read(&memoryFile);
    idResourceCacheEntry& cacheEntry=container.cacheTable[entryIndex];
    cacheEntry.filename.Set(resourceEntry.filename.c_str());
    cacheEntry.offset=resourceEntry.offset;
    cacheEntry.compressedLength=resourceEntry.compressedLength;
    cacheEntry.uncompressedLength=resourceEntry.uncompressedLength;
    cacheEntry.fileId=resourceEntry.fileId;
    if(resourceEntry.stream.Num()>0) {
      cacheEntry.streamOffset=resourceEntry.stream[0].streamOffset;
      if(resourceEntry.stream.Num()>1) {
        for(int streamIndex=0;streamIndex<resourceEntry.stream.Num();
            ++streamIndex) {
          if(idStr::Icmp(sys_lang.valueString.c_str(),
                         reinterpret_cast<const char*>(
                             resourceEntry.stream[streamIndex].lang))==0) {
            cacheEntry.streamOffset=
                resourceEntry.stream[streamIndex].streamOffset;
            break;
          }
        }
      }
    }
    container.cacheHash.Add(idStr::IHash(cacheEntry.filename.str),entryIndex);
  }
}

int idResourceManagerLocal::GetNeededResourcesForMap(
    const char* mapName,
    const idStaticList<idStrStatic<128>,256>* layersActive,
    const idStaticList<idStrStatic<128>,256>* layersDeactive,
    idList<int,5>& neededResources,idList<idStr,5>& neededMegatextures) {
  idStrStatic<256> filename;
  filename.Format("generated/buildGame/%s.mapResources",mapName);
  idFile_Memory* file=LoadCacheFile(filename.c_str());
  if(file==nullptr) {
    idLib::Printf("Could not find %s in cache file\n",filename.c_str());
    return -1;
  }

  auto readBigInt=[file](int& value)->std::size_t {
    const std::size_t bytes=file->Read(&value,sizeof(value));
    value=static_cast<int>(_byteswap_ulong(static_cast<unsigned long>(value)));
    return bytes;
  };
  auto readRawString=[file](idStr& value) {
    int length=0;
    file->Read(&length,sizeof(length));
    if(length<=0) {
      value.Empty();
    } else {
      value.Fill(' ',length);
      file->Read(&value[0],static_cast<std::size_t>(length));
    }
  };
  auto intersects=[](const idLayerBits& left,const idLayerBits& right) {
    for(int index=0;index<24;++index)
      if((left.bits[index]&right.bits[index])!=0) return true;
    return false;
  };

  unsigned int magic;
  file->Read(&magic,sizeof(magic));
  magic=_byteswap_ulong(magic);
  if(magic!=22237773U)
    idLib::Error(
        "GetNeededResourcesForMap; file magic missmach, probably version missmach");

  idLayerBits wanted{};
  bool useDefaultLayers=false;
  int numLayers;
  readBigInt(numLayers);
  if(layersActive!=nullptr && layersDeactive!=nullptr) {
    const bool activeEmpty=layersActive->Num()==0 ||
        (layersActive->Num()==1 && (*layersActive)[0].Length()==0);
    const bool deactiveEmpty=layersDeactive->Num()==0 ||
        (layersDeactive->Num()==1 && (*layersDeactive)[0].Length()==0);
    useDefaultLayers=activeEmpty && deactiveEmpty;
  } else {
    std::memset(&wanted,0xFF,sizeof(wanted));
  }

  for(int layerIndex=0;layerIndex<numLayers;++layerIndex) {
    idStr layerName;
    readRawString(layerName);
    if(resource_ignoreLayers.valueInteger==0 && layersActive!=nullptr &&
       layersDeactive!=nullptr) {
      bool requested=false;
      for(int index=0;index<layersActive->Num();++index) {
        if(idStr::Cmp((*layersActive)[index].c_str(),layerName.c_str())==0) {
          requested=true;
          break;
        }
      }
      if(!requested) {
        for(int index=0;index<layersDeactive->Num();++index) {
          if(idStr::Cmp((*layersDeactive)[index].c_str(),layerName.c_str())==0) {
            requested=true;
            break;
          }
        }
      }
      if(requested)
        wanted.bits[layerIndex/8]|=
            static_cast<unsigned char>(1U<<(layerIndex&7));
    }
  }

  int numDefaultLayers;
  readBigInt(numDefaultLayers);
  for(int index=0;index<numDefaultLayers;++index) {
    int layerIndex;
    readBigInt(layerIndex);
    if(useDefaultLayers)
      wanted.bits[layerIndex/8]|=
          static_cast<unsigned char>(1U<<(layerIndex&7));
  }
  wanted.bits[23]|=0x80U;

  int numBinaryModels;
  readBigInt(numBinaryModels);
  binaryModelLoadMask.SetNum(numBinaryModels);
  for(int index=0;index<numBinaryModels;++index) {
    idLayerBits bits{};
    file->Read(&bits,sizeof(bits));
    binaryModelLoadMask[index]=intersects(bits,wanted);
  }

  neededResources.ClearFree();
  int numResources;
  readBigInt(numResources);
  int excludedResources=0;
  for(int index=0;index<numResources;++index) {
    int resourceNumber;
    idLayerBits bits{};
    readBigInt(resourceNumber);
    file->Read(&bits,sizeof(bits));
    if(intersects(bits,wanted))
      neededResources.Append(resourceNumber);
    else
      ++excludedResources;
  }

  int numMegatextures=0;
  if(readBigInt(numMegatextures)==0) numMegatextures=0;
  neededMegatextures.SetNum(numMegatextures);
  for(int index=0;index<numMegatextures;++index)
    readRawString(neededMegatextures[index]);

  delete file;
  return excludedResources;
}

bool idResourceManagerLocal::ReOpenFileHandles() {
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
  try {
    resourceMasterIndexFile=fileSystem->OpenFileReadRetail(
        RESOURCE_MASTER_INDEX_FILE_NAME,true,false);
    if(resourceMasterIndexFile==nullptr)
      resourceMasterIndexFile=fileSystem->OpenFileReadRetail(
          RESOURCE_INDEX_FILE_NAME,true,false);
    if(resourceMasterIndexFile==nullptr) {
      idLib::Printf("idLib::SetProduction( PROD_DEVELOPMENT )\n");
      idLib::production=PROD_DEVELOPMENT;
      resourceLoadCallback=&DevelopmentResourceLoadCallback;
      idMem::Singleton()->PopHeap();
      return false;
    }

    int firstWord;
    resourceMasterIndexFile->Read(&firstWord,sizeof(firstWord));
    firstWord=static_cast<int>(_byteswap_ulong(
        static_cast<unsigned long>(firstWord)));
    if(firstWord==55788882) {
      int numContainers;
      resourceMasterIndexFile->Read(&numContainers,sizeof(numContainers));
      numContainers=static_cast<int>(_byteswap_ulong(
          static_cast<unsigned long>(numContainers)));
      resourceContainers.SetNum(numContainers);
      for(int index=0;index<numContainers;++index) {
        auto readRawString=[this](idStr& value) {
          int length=0;
          resourceMasterIndexFile->Read(&length,sizeof(length));
          if(length<=0) value.Empty();
          else {
            value.Fill(' ',length);
            resourceMasterIndexFile->Read(
                &value[0],static_cast<std::size_t>(length));
          }
        };
        idStr indexFileName;
        idStr resourceFileName;
        readRawString(indexFileName);
        readRawString(resourceFileName);
        idResourceContainer& container=resourceContainers[index];
        container.indexFile=fileSystem->OpenFileReadRetail(
            indexFileName.c_str(),true,false);
        container.resourceFileName=resourceFileName;
        container.resourceFile=fileSystem->OpenFileReadRetail(
            resourceFileName.c_str(),true,false);
        container.chunkId=
            streamingInstallMgr->GetChunkIdForResourceContainerIndex(index);
      }
    } else {
      resourceContainers.SetNum(1);
      resourceContainers[0].indexFile=fileSystem->OpenFileReadRetail(
          RESOURCE_INDEX_FILE_NAME,true,false);
      resourceContainers[0].resourceFile=fileSystem->OpenFileReadRetail(
          RESOURCE_FILE_NAME,true,false);
      if(resourceContainers[0].resourceFile==nullptr)
        idLib::Error("Missing %s in production",RESOURCE_FILE_NAME);
    }

    InitResourceContainers();
    if(resourceContainers.Num()>0)
      InitStreamContainers(nullptr,resourceContainers[0],true);
    OpenDLCFileHandles();
  } catch(...) {
    idMem::Singleton()->PopHeap();
    throw;
  }
  idMem::Singleton()->PopHeap();
  return true;
}

bool idResourceManagerLocal::LoadNamedResourceBlock(
    const char*,const char* mapName,
    const idStaticList<idStrStatic<128>,256>* layersActive,
    const idStaticList<idStrStatic<128>,256>* layersDeactive,
    const idVideoDesc* videoDesc,videoFlags_t videoFlags,bool initHDC) {
  idStrStatic<256> mapNameString(mapName);
  mapNameString.StripFileExtension();
  const int startTime=static_cast<int>(Sys_Milliseconds());
  const bool debug=resource_debugLevel.valueInteger!=0;
  idLib::Printf("Loading named resource block: \"%s\"\n",mapName);
  const bool commonBlock=idStr::Icmp(mapNameString.c_str(),"init")==0 ||
                         idStr::Icmp(mapNameString.c_str(),"common")==0;
  idStrStatic<256> mapResourcesName;
  mapResourcesName.Format("generated/buildGame/%s.mapResources",mapName);
  const int containerIndex=GetContainerIdForCacheFile(mapResourcesName.c_str());
  if(containerIndex<0) {
    idLib::Warning("The map %s isn't present in the cache file",
                   mapResourcesName.c_str());
    return false;
  }
  const int chunkId=
      streamingInstallMgr->GetChunkIdForResourceContainerIndex(containerIndex);
  if(!streamingInstallMgr->IsChunkInstalled(chunkId,false)) {
    idLib::Warning(
        "chunk missing in LoadNamedResourceBlock for map %s, it should never get this far",
        mapNameString.c_str());
    common->AddDialog(GDM_STREAMING_INSTALL_CONTENT_UNAVAILABLE,DIALOG_ACCEPT,
                      nullptr,nullptr,true,nullptr,0,false,false,false,0);
    return false;
  }

  idResourceContainer& container=resourceContainers[containerIndex];
  if(container.resourceFile==nullptr) {
    idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
    try {
      container.resourceFile=fileSystem->OpenFileReadRetail(
          container.resourceFileName.c_str(),true,false);
      if(container.resourceFile==nullptr)
        idLib::Error("could not open resource file %s\n",
                     container.resourceFileName.c_str());
    } catch(...) {
      idMem::Singleton()->PopHeap();
      throw;
    }
    idMem::Singleton()->PopHeap();
  }

  if(videoDesc!=nullptr && renderSystem->GetUsingSMP()) {
    renderSystem->BeginBinkVideo(
        renderSystem->GetGameWindow(),videoDesc,videoFlags,
        soundSystem->GetIXAudio2(),soundSystem->GetSpeakerMask(),
        cvarSystem->GetCVarFloat("s_volume_dB"),common->VideoOverlay(),true);
  }
  if(initHDC) streamFileCache->InitHDC();
  if(!commonBlock) {
    virtualTextureSystem.EmptyCache(true);
    virtualTextureSystem.CreatePhysicalImages(true,true,false,false);
  }

  idMem::Singleton()->PushMapHeap();
  char* tableData=static_cast<char*>(idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\framework\\ResourceManager.cpp(1640) : TAG_RESOURCE",
      static_cast<unsigned int>(container.resourceTableInfo.tableLength),
      TAG_RESOURCE,false,ALIGN_16,HEAP_DEFAULTHEAP));
  idMem::Singleton()->PopMapHeap();
  idFile* tableFile=container.indexFile!=nullptr
      ? container.indexFile : container.resourceFile;
  tableFile->ReadOfs(container.resourceTableInfo.tableOffset,tableData,
                     static_cast<std::size_t>(
                         container.resourceTableInfo.tableLength));
  idLib::Printf("%3.1f seconds to read %uk table header\n",
      static_cast<float>(static_cast<int>(Sys_Milliseconds())-startTime)*0.001f,
      static_cast<unsigned int>(container.resourceTableInfo.tableLength)>>10);

  idFile_Memory memoryFile("resourceHeader");
  memoryFile.SetReadOnlyData(
      tableData,static_cast<unsigned int>(container.resourceTableInfo.tableLength));
  auto readBigInt=[&memoryFile](int& value) {
    memoryFile.Read(&value,sizeof(value));
    value=static_cast<int>(_byteswap_ulong(static_cast<unsigned long>(value)));
  };
  int numResourceIndexes;
  int numFileEntries;
  readBigInt(numResourceIndexes);
  readBigInt(numFileEntries);
  idList<idStr,99> neededMegatextures;
  idHeapArray<bool> resourceIndexIsNeeded;
  resourceIndexIsNeeded.Alloc(static_cast<unsigned int>(numResourceIndexes));

  if(idStr::Cmp(mapNameString.c_str(),"all")==0) {
    std::memset(resourceIndexIsNeeded.buffer,1,
                static_cast<std::size_t>(numResourceIndexes));
  } else if(idStr::Cmp(mapNameString.c_str(),"outro")==0) {
    std::memset(resourceIndexIsNeeded.buffer,0,
                static_cast<std::size_t>(numResourceIndexes));
  } else {
    idList<int,99> neededResourceIndexes;
    const int skipped=GetNeededResourcesForMap(
        mapName,layersActive,layersDeactive,
        reinterpret_cast<idList<int,5>&>(neededResourceIndexes),
        reinterpret_cast<idList<idStr,5>&>(neededMegatextures));
    if(skipped==-1) {
      idLib::Warning("The map isn't present in the resource file");
      return false;
    }
    idLib::Printf("Resources skipped by layer tests: %i\n",skipped);
    std::memset(resourceIndexIsNeeded.buffer,0,
                static_cast<std::size_t>(numResourceIndexes));
    for(int index=0;index<neededResourceIndexes.Num();++index)
      resourceIndexIsNeeded.buffer[neededResourceIndexes[index]]=true;
  }

  int requestedResourceIndexes=0;
  for(int index=0;index<numResourceIndexes;++index)
    if(resourceIndexIsNeeded.buffer[index]) ++requestedResourceIndexes;

  const std::int64_t entriesOffset=memoryFile.Tell();
  int resourcesToLoad=0;
  int streamedResources=0;
  for(int index=0;index<numFileEntries;++index) {
    idResourceFileEntry entry;
    entry.Read(&memoryFile);
    if(!resourceIndexIsNeeded.buffer[entry.resourceIndex]) continue;
    if((entry.useBits&0x20U)!=0 ||
       std::strstr(entry.filename.c_str(),".sbcm")!=nullptr ||
       std::strstr(entry.filename.c_str(),".tbcm")!=nullptr) {
      ++streamedResources;
      continue;
    }
    idResource* resource=entry.resourceList->Lookup(entry.resourceName.c_str());
    if(resource==nullptr || resource->GetLoadDeferred()) ++resourcesToLoad;
  }

  memoryFile.Seek(entriesOffset,FS_SEEK_SET);
  idMem::Singleton()->PushHeap(HEAP_MAPHEAP);
  fileTable.SetNum(resourcesToLoad);
  idMem::Singleton()->PopHeap();
  int loadedTableEntries=0;
  lastUPacifierMsec=0;
  for(int index=0;index<numFileEntries && loadedTableEntries<resourcesToLoad;
      ++index) {
    if(cancelToTerminate || common->IsDialogActive()) return false;
    idResourceFileEntry& entry=fileTable[loadedTableEntries];
    entry.stream.Clear();
    entry.Read(&memoryFile);
    if(!resourceIndexIsNeeded.buffer[entry.resourceIndex]) {
      if(debug)
        idLib::Printf("skipped %s : %s\n",
                      entry.resourceList->GetResourceTypeName(),
                      entry.resourceName.c_str());
      continue;
    }
    if((entry.useBits&0x20U)!=0 ||
       std::strstr(entry.filename.c_str(),".sbcm")!=nullptr ||
       std::strstr(entry.filename.c_str(),".tbcm")!=nullptr)
      continue;
    idResource* resource=entry.resourceList->Lookup(entry.resourceName.c_str());
    if(resource!=nullptr && !resource->GetLoadDeferred()) continue;
    ++loadedTableEntries;
    if(debug)
      idLib::Printf("need to load %s : %s\n",
                    entry.resourceList->GetResourceTypeName(),
                    entry.resourceName.c_str());
    if(resource==nullptr) {
      resource=entry.resourceList->Alloc(entry.resourceName.c_str());
      entry.resourceList->Add(resource);
      resource->SetLoadDeferred(true);
    }
  }

  idMem::Singleton()->PushMapHeap();
  idMem::Singleton()->Free(tableData,ALIGN_16);
  idMem::Singleton()->PopMapHeap();
  idLib::Printf(
      "Load %i resources using %i of %i total resource files, %i streamed\n",
      requestedResourceIndexes,loadedTableEntries,numFileEntries,
      streamedResources);
  if(r_amdCrashReproDelay.valueFloat!=0.0f)
    Sys_Sleep(static_cast<int>(r_amdCrashReproDelay.valueFloat*1000.0f));

  bgl.BeginBackgroundLoads(*container.resourceFile,
      reinterpret_cast<const idList<idResourceFileEntry,101>&>(fileTable));
  fileSystem->SetFilePreCallbackRetail(
      reinterpret_cast<void*>(&ResourceLoadPreCallback));
  idLib::production=PROD_PRODUCTION;
  fileTableResource=0;
  while(fileTableResource<fileTable.Num()) {
    if(cancelToTerminate || common->IsDialogActive()) return false;
    idResourceFileEntry& entry=fileTable[fileTableResource];
    if(!commonBlock)
      common->UpdateLevelLoadPacifier(
          0.0f,static_cast<pacifierState_t>(1),false,false);
    if(debug)
      idLib::Printf("loading %i:%s : %s\n",fileTableResource,
                    entry.resourceList->GetResourceTypeName(),
                    entry.resourceName.c_str());
    idResource* resource=entry.resourceList->Load(
        entry.resourceName.c_str(),true,false);
    if(resource!=nullptr) {
      bgl.SkipPastResource(resource);
      do {
        ++fileTableResource;
        if(fileTableResource==fileTable.Num()) break;
        const idResourceFileEntry& next=fileTable[fileTableResource];
        if(idStr::Cmp(next.resourceName.c_str(),resource->GetName())!=0 ||
           next.resourceList!=resource->GetResourceList())
          break;
      } while(true);
    }
  }
  fileSystem->SetFilePreCallbackRetail(nullptr);
  bgl.EndBackgroundLoads();
  fileTable.ClearFree();
  virtualTextureSystem.SetupLoadedVirtualMaterials();
  binaryModelLoadMask.ClearFree();
  GL_Flush();
  idLib::Printf("%4.1f seconds to preload resources\n",
      static_cast<float>(static_cast<int>(Sys_Milliseconds())-startTime)*0.001f);
  return true;
}

bool idResourceManagerLocal::AllocGameWithResourceFileRaw(
    idGame*& game,const idGameSpawnInfo& spawnInfo,idFile* saveGameFile,
    const idVideoDesc* videoDesc) {
  if(idLib::production==PROD_DEVELOPMENT) {
    if(videoDesc!=nullptr && renderSystem->GetUsingSMP()) {
      renderSystem->BeginBinkVideo(
          renderSystem->GetGameWindow(),videoDesc,static_cast<videoFlags_t>(0),
          soundSystem->GetIXAudio2(),soundSystem->GetSpeakerMask(),
          cvarSystem->GetCVarFloat("s_volume_dB"),common->VideoOverlay(),true);
    }
    return gameSystem->AllocGame(
        game,spawnInfo,common->GameFrameParms(1),saveGameFile,videoDesc);
  }

  const int mapStartTime=static_cast<int>(Sys_Milliseconds());
  idStr canonical(spawnInfo.mapName);
  canonical.MakeNameCanonical();
  if(!LoadNamedResourceBlock(
         RESOURCE_FILE_NAME,canonical.c_str(),&spawnInfo.activeLayers,
         &spawnInfo.deactiveLayers,videoDesc,VIDEO_EXIT,false))
    return false;
  DefaultMissing(canonical.c_str());
  const int gameStartTime=static_cast<int>(Sys_Milliseconds());
  fileSystem->SetFilePreCallbackRetail(
      reinterpret_cast<void*>(&MiscellaneousFilePreCallback));
  idLib::production=PROD_LOADED;
  if(resource_virtualTexturePreload.valueInteger!=0)
    virtualTexturePreloader.BeginPreloading();
  const bool allocated=gameSystem->AllocGame(
      game,spawnInfo,common->GameFrameParms(1),saveGameFile,videoDesc);
  fileSystem->SetFilePreCallbackRetail(nullptr);
  virtualTexturePreloader.EndPreloading();
  for(int index=idFileResource::resourceList.Num()-1;index>=0;--index) {
    idFileResource* resource=static_cast<idFileResource*>(
        idFileResource::resourceList.Index(index));
    if(!resource->GetStaticResource()) resource->FreeData();
  }
  const int finishTime=static_cast<int>(Sys_Milliseconds());
  idLib::Printf("%4.1f seconds for game initialization\n",
      static_cast<float>(finishTime-gameStartTime)*0.001f);
  idLib::Printf("%4.1f total seconds for map load\n",
      static_cast<float>(finishTime-mapStartTime)*0.001f);
  if(!allocated || cancelToTerminate || common->IsDialogActive()) return false;
  const char* spawnSpot=spawnInfo.spawnSpotOverride.IsEmpty()
      ? spawnInfo.spawnSpot.c_str() : spawnInfo.spawnSpotOverride.c_str();
  const int preloadHandle=virtualTexturePreloader.PreloadVMTRsForMap(
      spawnInfo.mapName.c_str(),spawnSpot);
  while(!virtualTexturePreloader.MaterialPreloadIsDone(preloadHandle))
    Sys_Sleep(100);
  return true;
}

bool idResourceManagerLocal::AllocGameWithResourceFile(
    idGame*& game,const idGameSpawnInfo& spawnInfo,idFile* saveGameFile,
    const idVideoDesc* videoDesc) {
  resourceLoadCallback=nullptr;
  commonLocal.ResetMapHeap(spawnInfo.forceHeapReset);
  common->BeginTerminationThread();
  renderSystem->SetRenderFlag(
      idStr::Icmp(spawnInfo.mapName.c_str(),"outro")!=0
          ? RENDERFLAG_CANEXITCUTSCENE : RENDERFLAG_NONE);
  commonLocal.StartLevelLoadGame(spawnInfo,videoDesc!=nullptr);
  if(idLib::production==PROD_PRODUCTION || spawnInfo.forceHeapReset) {
    char remappedName[512];
    const char* name=idDeclMapInfo::resourceList.RemapResourceName(
        remappedName,sizeof(remappedName),spawnInfo.mapName.c_str());
    idDeclMapInfo* mapInfo=static_cast<idDeclMapInfo*>(
        idDeclMapInfo::resourceList.Load(name,false,false));
    if(mapInfo!=nullptr && mapInfo->vmtrChunkNumber>0) {
      const char* path=vt_filePathVmtrOverride.valueString.IsEmpty()
          ? vt_filePath.valueString.c_str()
          : vt_filePathVmtrOverride.valueString.c_str();
      idStr vmtrName;
      vmtrName.Format("chunk%d_vmtr",mapInfo->vmtrChunkNumber);
      const char* suffix=feature_lowgore.valueInteger!=0 ? "_lowgore" : "";
      idStr vmtrPath;
      vmtrPath.Format("%s/%s%s.pages",path,vmtrName.c_str(),suffix);
      if(virtualTextureSystem.vts[3].SetSource(
             vmtrName.c_str(),vmtrPath.c_str(),PHYSICAL_PAGES_POOL_VMTR,
             ID_VTF_VMTR)) {
        virtualTextureSystem.vts[3].isMapVmtr=true;
      } else {
        idLib::Error("colud not open chunk vmtr '%s' from '%s'",
                     vmtrName.c_str(),vmtrPath.c_str());
      }
    }
  }
  const bool result=AllocGameWithResourceFileRaw(
      game,spawnInfo,saveGameFile,videoDesc);
  commonLocal.EndLevelLoad(result);
  WaitForVideoOrBackgroundSwaps();
  common->EndTerminationThread();
  return result;
}

void idResourceManagerLocal::Init2(bool skipIntroVideo) {
  idBaseCodeResource::AllocateCodeResources();
  if(idLib::production==PROD_PRODUCTION) {
    common->BeginTerminationThread();
    if(com_useMapHeap.valueInteger!=0) idMem::Singleton()->InitMapHeap();
    denyFonts=true;
    const char* videoName=skipIntroVideo
        ? nullptr : "bink/loadvideo_pc_xboxone.bik";
    idVideoDesc videoDesc{};
    videoDesc.fadeFromColor={-1.0f,-1.0f,-1.0f,-1.0f};
    if(videoName!=nullptr) videoDesc.videoFile=videoName;
    LoadNamedResourceBlock(RESOURCE_FILE_NAME,"init",nullptr,nullptr,nullptr,
                           VIDEO_EXIT,false);
    renderThreadParmState->Init();
    idLib::Printf("Start time of bink logo video in ms: %d\n",
                  static_cast<int>(Sys_Milliseconds()));
    const videoFlags_t videoFlags=
        skipIntroVideo && !videoDesc.videoFile.IsEmpty()
            ? VIDEO_LOOP : VIDEO_EXIT;
    LoadNamedResourceBlock(
        RESOURCE_FILE_NAME,"common",nullptr,nullptr,
        videoDesc.videoFile.IsEmpty() ? nullptr : &videoDesc,
        videoFlags,true);
    resourceManager->SetFileHook();
    resourceManager->ReleaseFileHook();
    if(skipIntroVideo) {
      renderSystem->FreeBinkVideo();
    } else {
      idScopeCVar resetBinkSystem(&bink_dontfree,false);
      WaitForVideoOrBackgroundSwaps();
    }
    common->EndTerminationThread();
  } else {
    idBaseCodeResource::LoadCodeResources("renderParm");
    idBaseCodeResource::LoadCodeResources(nullptr);
    streamFileCache->InitHDC();
  }
  DefaultMissing("idResourceManagerLocal::Init2");
}

void idResourceManagerLocal::OpenDLCFileHandles() {
  ScopedResourceSystemHeap systemHeap;
  const int packageCount=session->GetNumContentPackages();
  idLib::Printf("OpenDLCFileHandles: %d packages mounted.\n",packageCount);
  for(int packageIndex=0;packageIndex<packageCount;++packageIndex) {
    const int packageId=session->GetContentPackageID(packageIndex);
    idStrStatic<256> packagePath(
        session->GetContentPackagePath(packageIndex));
    idLib::Printf("* Content Package #%d:\n",packageIndex);
    idLib::Printf("  - id: %d\n",packageId);
    idLib::Printf("  - path: %s\n",packagePath.c_str());
    for(int containerIndex=0;
        containerIndex<resourceContainers.Num();++containerIndex) {
      if(resourceContainers[containerIndex].dlcId==packageId)
        idLib::Printf("  - already loaded - skipping...\n");
    }
    idStrStatic<256> dlcBase(packagePath);
    dlcBase.AppendPath("base");
    idFileLocal masterIndexFile(fileSystem->OpenOSFileReadRetail(
        dlcBase.c_str(),RESOURCE_MASTER_INDEX_FILE_NAME));
    if(masterIndexFile.file==nullptr) continue;
    int magic=0;
    masterIndexFile->Read(&magic,sizeof(magic));
    magic=static_cast<int>(_byteswap_ulong(static_cast<unsigned>(magic)));
    if(magic!=55788882) continue;
    int entryCount=0;
    masterIndexFile->Read(&entryCount,sizeof(entryCount));
    entryCount=static_cast<int>(
        _byteswap_ulong(static_cast<unsigned>(entryCount)));
    for(int entryIndex=0;entryIndex<entryCount;++entryIndex) {
      auto readString=[&](idStr& string) {
        int length=0;
        masterIndexFile->Read(&length,sizeof(length));
        if(length<=0) {
          string.Empty();
        } else {
          string.Fill(' ',length);
          masterIndexFile->Read(&string[0],static_cast<std::size_t>(length));
        }
      };
      idStr indexFilename;
      readString(indexFilename);
      idStr resourcesFilename;
      readString(resourcesFilename);
      idFile* indexFile=fileSystem->OpenOSFileReadRetail(
          dlcBase.c_str(),indexFilename.c_str());
      if(indexFile==nullptr) continue;
      idResourceContainer* container=resourceContainers.Alloc();
      container->indexFile=indexFile;
      container->resourceFileName=resourcesFilename;
      container->resourceFile=fileSystem->OpenOSFileReadRetail(
          dlcBase.c_str(),resourcesFilename.c_str());
      container->dlcId=packageId;
      InitResourceContainer(*container);
      InitStreamContainers(dlcBase.c_str(),*container,false);
    }
  }
}

void idResourceManagerLocal::CancelToTerminate() {
  cancelToTerminate=true;
}

void idResourceManagerLocal::SetResourceFilePlatform(unsigned platformFlags) {
  filePlatformBits=platformFlags;
}

unsigned idResourceManagerLocal::GetResourceFilePlatform() {
  return filePlatformBits;
}

unsigned idResourceManagerLocal::GetBuildPlatforms() {
  return buildPlatforms;
}

const idList<idStr,5>& idResourceManagerLocal::GetLanguages() {
  return languages;
}

bool idResourceManagerLocal::GetBinaryModelMask(int id) {
  return id==0 || id>=binaryModelLoadMask.Num() || binaryModelLoadMask[id];
}
