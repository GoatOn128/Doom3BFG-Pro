#pragma once

#include "engine/renderer/bufferobject.h"
#include "engine/renderer/jobs/feedbackanalysis/feedbackanalysis.h"
#include "engine/renderer/jobs/pageminlod/pageminlod.h"
#include "engine/renderer/jobs/pagesort/pagesort.h"
#include "engine/renderer/jobs/transcode/transcode.h"
#include "engine/renderer/virtualtexturepreloader.h"
#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/heaparray.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/parallelism/thread.h"
#include "shared/idlib/metrics/gaugemetric.h"

// PDB-derived renderer boundary used by declmaterial.cpp.  Method bodies and
// the remaining virtual-texture internals belong to the corresponding
// virtualtexture/virtualtexturesystem compilands and are intentionally deferred.

#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/atomicstring.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

class idDeclRenderParm;
template <typename T> class idCodeResource;
class idFile;
class idImage;
struct idImageData;
class idMaterial;
class idVmtrCPU;
class idVMTRData;
class idBitBlockAllocator;
class idBackgroundVmtrUpdater;
struct idVmtrGenAccelerator;
struct pageImagesUpdate_t;
struct pageImagesUpdateParms_t;
class idCmdArgs;
class idParallelJobList;

enum physicalPagesPool_t : int {
  PHYSICAL_PAGES_POOL_VMTR = 0,
  PHYSICAL_PAGES_POOL_UNIQUE = 1,
  PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY = 2,
  PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2 = 3,
  PHYSICAL_PAGES_POOL_SHARED_COUNT = 4,
  PHYSICAL_PAGES_POOL_PRT = 5,
  PHYSICAL_PAGES_POOL_MAX = 6,
};

enum virtualTextureFlavor_t : int {
  ID_VTF_UNKNOWN = 0,
  ID_VTF_BAKE = 1,
  ID_VTF_VMTR = 2,
};

struct pageHeader_t {
  int magic{};
  pageCompression_t pageCompression{COMP_INVALID};
  unsigned int finerDiskOffset[4]{};
  unsigned short finerDiskLength[4]{};
  unsigned short x{};
  unsigned short y{};
  unsigned short level{};
  unsigned short reserved{};

  void Swap();
  void CopySwap(pageHeader_t* destination) const;
};

struct pageFileHeader_t {
  int magic{};
  int pagesWide{};
  int installedMipAndOffset{};
  short numLevels{};
  short layoutVersion{};
  int totalPages{};
  int diskOffsetScale{};
  std::int64_t totalFileSize{};
  pageHeader_t subRoot{};

  void Swap();
};

class idPhysicalPageHash {
public:
  struct pageHashEntry_t {
    std::uint32_t pageID{};
    std::uint16_t mapped{};
    std::uint16_t next{};
  };

  ~idPhysicalPageHash();
  void Init(int hashSize, int indexSize);
  void RemovePage(texturePage_t* page);

  int hashMask{};
  std::uint32_t alignmentBeforePointers{};
  std::int16_t* pageHashTable{};
  pageHashEntry_t* pageHashIndex{};
};

class idPhysicalPageList {
public:
  ~idPhysicalPageList();
  void Init(int size);
  void Remove(int physicalPageNum);

  int firstPageIndex{};
  int lastPageIndex{};
  int indexMask{};
  std::uint32_t alignmentBeforePointer{};
  std::int64_t* physicalPageNums{};
  int sortDone{};
  std::uint32_t alignmentBeforeSortParms{};
  pageSortParms_t* sortParms{};
};

class idVirtualTexture;

class idPhysicalPages {
public:
  idPhysicalPages();
  ~idPhysicalPages();

  void Clear();
  bool AllocWouldThrash(char newPagePriority);
  void CreatePageImagesUpdate(idVirtualTexture* virtualTexture,
                              const texturePage_t* physicalPage,
                              texturePage_t* virtualPage,
                              const void* pageDataPBO,
                              const std::uint8_t* pageData,
                              pageImagesUpdate_t* update);
  void UpdatePageImages(idVirtualTexture* virtualTexture,
                        texturePage_t* physicalPage,
                        texturePage_t* virtualPage,
                        const std::uint8_t* pageData);
  void GetDirectMappedPhysicalPagePointers(transcodeParms_t* parms);
  void UpdateFilterParms(bool forceUpdateFilterParms);
  void ValidateCache();
  bool CreatePhysicalImages(
      physicalPagesPool_t pool, const char* name, int texelsWide,
      int texelsHigh,
      const idCodeResource<idDeclRenderParm>* physicalFilterParms);
  texturePage_t* AllocPage(idVirtualTexture* virtualTexture,
                           texturePage_t* coarser,
                           pageQuadrant_t quadrant,
                           pageImagesUpdateParms_t* deferredUnmap);
  void FreePage(idVirtualTexture* virtualTexture, texturePage_t* page,
                pageImagesUpdateParms_t* deferredUnmap);
  void FreePageTree_r(idVirtualTexture* virtualTexture,
                      texturePage_t* texturePage,
                      pageImagesUpdateParms_t* updateParms);
  void EmptyCache();
  int GetNumResidentPagesForSource(pageSource_t source) const;
  int GetNumLockedPagesForSource(pageSource_t source) const;
  bool ValidatePageTable(const idImageData* pageTable, int currentLevel,
                         int loppedLevels) const;
  const idDeclRenderParm* GetFilterParms() const;

  physicalPagesPool_t physicalPagesPool{PHYSICAL_PAGES_POOL_VMTR};
  std::uint32_t alignmentAfterPool{};
  texturePage_t* pages{};
  int* pagePriority{};
  int* pageFrameNum{};
  std::uint8_t* pageFade{};
  idPhysicalPageHash pageHash;
  idPhysicalPageList freePageList;
  idPhysicalPageList unlockedPageList;
  int numLockedPages{};
  bool pageImageCompressed{};
  bool isDirectMapped{};
  std::uint8_t alignmentBeforeDimensions[2]{};
  int PHYSICAL_TEXELS_WIDE{};
  int PHYSICAL_TEXELS_HIGH{};
  int PHYSICAL_PAGES_WIDE{};
  int PHYSICAL_PAGES_HIGH{};
  int NUM_PHYSICAL_PAGES{};
  std::uint32_t alignmentBeforeImages{};
  idImage* physicalPagesImage0{};
  idImage* physicalPagesImage1{};
  idImage* physicalPagesImage2{};
  idImage* physicalMappingsImage{};
  void* physicalMappingsImageShadow{};
  idPixelUnpackBuffer physicalMappingsImagePBO;
  int feedbackNumPages{};
  float feedbackDynamicLodBias{};
  float oldFeedbackDynamicLodBias{};
  std::uint32_t alignmentBeforeRenderParm{};
  const idCodeResource<idDeclRenderParm>* rpPhysicalFilterParms{};
};

class idVirtualTexture {
public:
  static constexpr int PAGEFILE_MAX_LOD_CLAMP = 12;

  idVirtualTexture();
  ~idVirtualTexture();

  bool SetSource(const char* baseName, const char* pageFilePath,
                 physicalPagesPool_t physicalPagesPool,
                 virtualTextureFlavor_t flavor);
  void ReleaseSource();
  void SetEmpty(const char* pageFile, int pagesWide, int diskOffsetScale);
  idVec4 GetFeedbackVec4() const;
  bool UsesPartiallyResidentTextures() const;
  physicalPagesPool_t GetPhysicalPagesPool() const;
  void FreeData();
  void GetToBeLockedPhysicalPages(int numTexturePages,
                                  pageIndices_t* pageIndices);
  void EmptyCache();
  bool ValidatePageTable();
  void Print();
  void UpdateMinLod(idParallelJobList* jobList);
  pageCompression_t GetNewDiskPageCompression();
  const char* GetBaseName() const;
  void SetMapName(const char* mapName);

  virtualTextureFlavor_t flavor{ID_VTF_UNKNOWN};
  std::uint8_t alignmentAfterFlavor[4]{};
  idVMTRData* vmtrData{};
  pageFileHeader_t header{};
  idStrStatic<256> pageFilePath;
  idStrStatic<256> baseName;
  pageSource_t pageSource{PAGESOURCE_INVALID};
  int layoutVersion{};
  int storedNumLevels{};
  std::uint8_t alignmentBeforeFiles[4]{};
  idFile* pageFile{};
  idFile* pageFileInstalled{};
  int minMipLevelInstalled{};
  int VIRTUAL_PAGES_WIDE_LOG2{};
  int VIRTUAL_TEXELS_WIDE{};
  int VIRTUAL_MAPPING_PAGES_WIDE{};
  unsigned short mipLevelPagesWide[PAGESOURCE_TOTAL]{};
  idStrStatic<128> mapName;
  idPhysicalPages* physicalPages{};
  texturePage_t* rootTexturePage{};
  idImage* pageTableImage{};
  idImage* pageMinLodImage{};
  void* pageTableImageShadow{};
  idPixelUnpackBuffer pageTableImagePBO;
  idPixelUnpackBuffer pageMinLodPBO[2];
  int currentPageMinLodPBO{};
  std::uint32_t alignmentBeforeMinLodParms{};
  pageMinLodUpdate_t* pageMinLodParms{};
  bool enlargedPageTableAllocationFailed{};
  bool isDirectMapped{};
  std::uint8_t alignmentBeforeResidency[2]{};
  int levelResidency[12]{};
  idSysMutex headerMutex;
  bool pageFileIsLocal;
  bool skipFlush{};
  std::uint8_t alignmentBeforeCompression[2]{};
  pageCompression_t forcedPageCompression;
  bool isMapVmtr{};
  std::uint8_t alignmentTail[7]{};
};

class idVirtualMaterial {
public:
  idAtomicString materialName;
  unsigned materialChecksum{};
  unsigned cpuVmtrTimeStamp{};
  int allocX{};
  int allocY{};
  int allocWidth{};
  int allocHeight{};
  int flags{};
  std::uint8_t alignmentBeforePointers[4]{};
  const idMaterial* material{};
  const idVmtrCPU* cpuVmtr{};
  idVec4 scaleAndBias{};
  idVMTRData* vmtrData{};
};

class idVMTRData {
public:
  static constexpr int VMTR_FILE_VERSION = 25;

  explicit idVMTRData(idVirtualTexture* virtualTexture);
  ~idVMTRData();

  void ReadVmtrFile();
  void WriteVmtrFile();
  void SetupVirtualMaterial(idVirtualMaterial* vmtr);
  idVirtualMaterial* VirtualMaterialForMaterial(const idMaterial* material);
  void UpdateVirtualMaterialImages(idVirtualMaterial* vmtr);
  int GeneratePageOrder(idList<int, 5>& pageOrder) const;
  void SetupBitblock(const pageFileHeader_t& fileHeader);

  idStrStatic<256> vmtrHeaderPath;
  idVirtualTexture* vt{};
  idList<idVirtualMaterial*, 5> vmtrs;
  idHashIndex vmtrHash;
  idBitBlockAllocator* bitBlock{};
  idBackgroundVmtrUpdater* backgroundUpdater{};
  idVmtrGenAccelerator* vmtrGenAccelerator{};
};

class idBackgroundVmtrUpdater {
public:
  explicit idBackgroundVmtrUpdater(idVMTRData* vmtrData);
  ~idBackgroundVmtrUpdater() = default;
  static void operator delete(void* pointer) noexcept {
    if (pointer != nullptr) {
      idMem::Singleton()->Free(pointer, ALIGN_16);
    }
  }

  idList<idVirtualMaterial*, 5> updateList;
  idVMTRData* vmtrData{};
  bool isDoingWork{};
  std::uint8_t alignmentTail[7]{};
};

class idPixelPackBuffer;
class idRenderDestination;

struct cachedPageFile_t {
  idFile* file{};
  idFile* fileInstalled{};
};

enum vmtrBatch_t : int {
  BATCH_NONE = 0,
  BATCH_NOMIPS = 1,
  BATCH_NOIMAGES = 2,
};

struct transcodeJobList_t {
  transcodeJobList_t();
  ~transcodeJobList_t();

  idParallelJobList* parallelJobList{};
  int numTranscodeJobs{};
  std::uint32_t alignmentBeforeTranscodeParms{};
  transcodeParms_t* transcodeParms{};
  std::uint8_t* transcodeSourceBuffer{};
  idPixelUnpackBuffer transcodePBO;
  alignas(8) std::uint8_t pageUnmapParmsStorage[24]{};
  alignas(8) std::uint8_t pageImagesUpdateParmsStorage[24]{};
  bool nextFramePageImageUpload{};
  std::uint8_t alignmentTail[7]{};
};

class idVirtualTextureSystem {
public:
  idVirtualTextureSystem();
  ~idVirtualTextureSystem();

  void Init();
  void Shutdown();
  bool Restart();
  void FreeLockedPages();
  void SetContainerPath(const char* path);
  void AddToReferencedVirtualTextures(const char* name, const char* path);
  void WaitForLastAnalyzeFeedback();
  void SetupLoadedVirtualMaterials();
  bool CreatePhysicalImages(bool mapHasSpecularPages,
                            bool mapHasDiffuseOnlyPages,
                            bool mapHasDiffuseOnly2Pages,
                            bool vmtrPhysicalImages);
  void EmptyCache(bool skipVmtrCache);
  idVirtualTexture* VirtualTextureForMaterial(const idMaterial* material);
  idVirtualMaterial* VirtualMaterialForMaterial(const idMaterial* material);
  void UpdateFilterParms(bool forceUpdateFilterParms);
  void LoadMaterialPagesInBackground(const idMaterial* material, char lod);
  void ShowFeedbackAnalysis(std::uint8_t* data, unsigned int width,
                            unsigned int height, int bytePitch);
  void StreamFeedback(const pageIndices_t* transcodePageIndices);
  void AnalyzeFeedback(void* data, int width, int height, int bytePitch,
                       bool forceHighestDetail);
  void GetDiskStatsForFeedback(const pageIndices_t* transcodePageIndices,
                               int* numCacheLines,
                               int* numSeekAndReadMsec);
  void UpdateStats(const pageIndices_t* transcodePageIndices,
                   int numTranscodePages,
                   int maxTranscodePagesPerFrame,
                   int totalStreamedPageData);
  bool TranscodeFeedback(const pageIndices_t* transcodePageIndices,
                         int* transcodePageIndicesOffset, bool blocking,
                         bool lockPages);
  void LockPhysicalPages(idVirtualTexture** virtualTextures,
                         int numVirtualTextures, int maxLockedPages);
  void PushFeedback(void* data, int width, int height, int bytePitch,
                    bool forceHighestDetail);
  void VisaulizeFeedback();
  void FinishFeedback(bool lockPages, int maxWaitTimeInSeconds);
  void ClearFeedback();
  void LogMetrics();
  bool AllowGenMissingPages() const;
  void SyncFeedback();
  void Benchmark(int maxThreads);
  bool RunTranscodeBenchmark(float* megaTexelsPerSecond, float* score);
  void VisualizeFeedbackBuffer(char* data, int width, int height,
                               int bytePitch);
  void ShowFeedbackStreaming(char* data, int width, int height,
                             int bytePitch);
  bool ReloadVirtualTextures();
  idFile* OpenCachedPagesFile(const char* path);
  idFile* OpenInstalledCachedPagesFile(const char* path);
  void CloseCachedPagesFile(idFile* file);

  static void ValidateCache_f(const idCmdArgs& args);
  static void SetSource_f(const idCmdArgs& args);
  static void EmptyCache_f(const idCmdArgs& args);
  static void Restart_f(const idCmdArgs& args);
  static void ListVirtualTextures_f(const idCmdArgs& args);
  static void Benchmark_f(const idCmdArgs& args);
  static void TranscodeBenchmark_f(const idCmdArgs& args);
  static void LockPages_f(const idCmdArgs& args);
  static void Reload_f(const idCmdArgs& args);
  static void PrintVirtualMaterial_f(const idCmdArgs& args);

  idVirtualTexture vts[PAGESOURCE_TOTAL];
  idVirtualTexture* vtPtrs[PAGESOURCE_TOTAL]{};
  idList<cachedPageFile_t, 5> cachedPagesFiles;
  int numReferencedVirtualTextures{};
  char referencedVTPath[100][256]{};
  char referencedVTName[100][32]{};
  idPhysicalPages physicalPagesPool[PHYSICAL_PAGES_POOL_SHARED_COUNT];
  bool mapHasSpecularPages{};
  bool mapHasDiffuseOnlyPages{};
  bool mapHasDiffuseOnly2Pages{};
  char containerPath[256]{};
  std::uint8_t alignmentBeforeFeedbackBuffer[5]{};
  idHeapArray<unsigned char> feedbackBuffer;
  int feedbackBufferPitch{};
  std::uint32_t alignmentBeforeFeedbackObject{};
  idPixelPackBuffer* feedbackBufferObject{};
  idRenderDestination* renderDestFeedback[3]{};
  idImage* feedbackVisualize{};
  idImage* feedbackStreaming{};
  float mipLevelMSE{};
  int feedbackFrameNum{};
  bool flushNextFeedback{};
  std::uint8_t alignmentBeforePageUploadCount[3]{};
  int pageUploadCount{};
  int pageTranscodeCount{};
  int emptyCacheTime{};
  vmtrBatch_t vmtrBatchBuild{BATCH_NONE};
  std::uint32_t alignmentBeforeAnalyzeParms{};
  analyzeParms_t analyzeParms{};
  virtualTextureInfo_t* vtInfo{};
  unsigned int currentPageIndices{};
  std::uint32_t alignmentBeforePageIndices{};
  pageIndices_t* pageIndices[2]{};
  std::uint8_t* explicitPages[2]{};
  int numExplicitPages[2]{};
  idParallelJobList* analyzeJobList{};
  transcodeJobList_t transcodeJobList[2];
  int currentTranscodeJobList{};
  volatile bool cancelToTerminate{};
  std::uint8_t alignmentBeforeMetricMutex[3]{};
  idSysMutex metricMutex;
  idGaugeMetric vmtrTotalPagesMetric;
  idGaugeMetric vmtrBiasMetric;
  idGaugeMetric specTotalPagesMetric;
  idGaugeMetric specBiasMetric;
  idGaugeMetric diffuseTotalPagesMetric;
  idGaugeMetric diffuseBiasMetric;
};

#if defined(WOLF_RENDERER_VIRTUALMATERIALATLAS_TEST_ACCESS)
alignas(idVirtualTextureSystem) extern unsigned char
    virtualMaterialAtlasTestVirtualTextureSystemStorage[
        sizeof(idVirtualTextureSystem)];
#define virtualTextureSystem (*reinterpret_cast<idVirtualTextureSystem*>(    \
    virtualMaterialAtlasTestVirtualTextureSystemStorage))
#elif defined(WOLF_RENDERER_RENDERTHREAD_TEST_ACCESS)
alignas(idVirtualTextureSystem) extern unsigned char
    renderThreadTestVirtualTextureSystemStorage[sizeof(idVirtualTextureSystem)];
#define virtualTextureSystem (*reinterpret_cast<idVirtualTextureSystem*>(    \
    renderThreadTestVirtualTextureSystemStorage))
#elif defined(WOLF_RENDERER_VIRTUALTEXTURE_TEST_ACCESS)
alignas(idVirtualTextureSystem) extern unsigned char
    virtualTextureTestVirtualTextureSystemStorage[sizeof(idVirtualTextureSystem)];
#define virtualTextureSystem (*reinterpret_cast<idVirtualTextureSystem*>(    \
    virtualTextureTestVirtualTextureSystemStorage))
#elif defined(WOLF_RENDERER_VIRTUALTEXTUREPRELOADER_TEST_ACCESS)
alignas(idVirtualTextureSystem) extern unsigned char
    virtualTexturePreloaderTestVirtualTextureSystemStorage[
        sizeof(idVirtualTextureSystem)];
#define virtualTextureSystem (*reinterpret_cast<idVirtualTextureSystem*>(    \
    virtualTexturePreloaderTestVirtualTextureSystemStorage))
#elif defined(WOLF_RENDERER_VIRTUALTEXTURESYSTEM_TEST_ACCESS)
alignas(idVirtualTextureSystem) extern unsigned char
    virtualTextureSystemTestStorage[sizeof(idVirtualTextureSystem)];
#define virtualTextureSystem (*reinterpret_cast<idVirtualTextureSystem*>(    \
    virtualTextureSystemTestStorage))
#else
extern idVirtualTextureSystem virtualTextureSystem;
#endif

static_assert(offsetof(idPhysicalPages, physicalPagesImage0) == 0xB0,
              "idPhysicalPages image offset drift");
static_assert(offsetof(idPhysicalPages, physicalMappingsImage) == 0xC8,
              "idPhysicalPages mapping image offset drift");
static_assert(sizeof(idPhysicalPageHash) == 24,
              "idPhysicalPageHash ABI drift");
static_assert(sizeof(idPhysicalPageHash::pageHashEntry_t) == 8,
              "physical-page hash entry ABI drift");
static_assert(sizeof(idPhysicalPageList) == 40,
              "idPhysicalPageList ABI drift");
static_assert(sizeof(idPhysicalPages) == 0x100,
              "idPhysicalPages ABI drift");
static_assert(sizeof(cachedPageFile_t) == 0x10,
              "cached page-file ABI drift");
static_assert(offsetof(transcodeJobList_t, transcodePBO) == 0x20,
              "transcode job-list PBO offset drift");
static_assert(offsetof(transcodeJobList_t, pageUnmapParmsStorage) == 0x30,
              "transcode job-list unmap offset drift");
static_assert(offsetof(transcodeJobList_t, pageImagesUpdateParmsStorage) == 0x48,
              "transcode job-list update offset drift");
static_assert(sizeof(transcodeJobList_t) == 0x68,
              "transcode job-list ABI drift");
static_assert(offsetof(idVirtualTexture, pageSource) == 0x2A8,
              "idVirtualTexture page-source offset drift");
static_assert(offsetof(idVirtualTexture, pageFile) == 0x2B8,
              "idVirtualTexture page-file offset drift");
static_assert(offsetof(idVirtualTexture, VIRTUAL_MAPPING_PAGES_WIDE) == 0x2D4,
              "idVirtualTexture mapping width offset drift");
static_assert(offsetof(idVirtualTexture, mapName) == 0x2F8,
              "idVirtualTexture map-name offset drift");
static_assert(offsetof(idVirtualTexture, physicalPages) == 0x3A0,
              "idVirtualTexture physical-pages offset drift");
static_assert(offsetof(idVirtualTexture, pageTableImage) == 0x3B0,
              "idVirtualTexture page-table offset drift");
static_assert(offsetof(idVirtualTexture, pageTableImagePBO) == 0x3C8,
              "idVirtualTexture page-table PBO offset drift");
static_assert(offsetof(idVirtualTexture, pageMinLodParms) == 0x400,
              "idVirtualTexture min-LOD parameters offset drift");
static_assert(offsetof(idVirtualTexture, levelResidency) == 0x40C,
              "idVirtualTexture residency offset drift");
static_assert(offsetof(idVirtualTexture, forcedPageCompression) == 0x44C,
              "idVirtualTexture compression offset drift");
static_assert(sizeof(idVirtualTexture) == 0x458,
              "idVirtualTexture ABI drift");
static_assert(sizeof(pageHeader_t) == 40, "page header ABI drift");
static_assert(offsetof(pageHeader_t, finerDiskOffset) == 8,
              "page header disk-offset drift");
static_assert(offsetof(pageHeader_t, x) == 32,
              "page header coordinate drift");
static_assert(sizeof(pageFileHeader_t) == 72,
              "page-file header ABI drift");
static_assert(offsetof(pageFileHeader_t, pagesWide) == 4,
              "page-file width offset drift");
static_assert(offsetof(pageFileHeader_t, subRoot) == 32,
              "page-file sub-root offset drift");
static_assert(offsetof(idVirtualMaterial, scaleAndBias) == 0x38,
              "idVirtualMaterial mapping offset drift");
static_assert(sizeof(idVirtualMaterial) == 80,
              "idVirtualMaterial ABI drift");
static_assert(offsetof(idVMTRData, vt) == 296,
              "VMTR virtual-texture offset drift");
static_assert(offsetof(idVMTRData, vmtrs) == 304,
              "VMTR list offset drift");
static_assert(offsetof(idVMTRData, vmtrHash) == 328,
              "VMTR hash offset drift");
static_assert(offsetof(idVMTRData, bitBlock) == 368,
              "VMTR bit-block offset drift");
static_assert(sizeof(idVMTRData) == 392, "idVMTRData ABI drift");
static_assert(offsetof(idBackgroundVmtrUpdater, vmtrData) == 24,
              "background updater VMTR offset drift");
static_assert(offsetof(idBackgroundVmtrUpdater, isDoingWork) == 32,
              "background updater work-state offset drift");
static_assert(sizeof(idBackgroundVmtrUpdater) == 40,
              "background updater ABI drift");
static_assert(offsetof(idVirtualTextureSystem, physicalPagesPool) == 46752,
              "virtual-texture physical-pool offset drift");
static_assert(offsetof(idVirtualTextureSystem, feedbackBuffer) == 48040,
              "virtual-texture feedback buffer offset drift");
static_assert(offsetof(idVirtualTextureSystem, feedbackBufferObject) == 48064,
              "virtual-texture feedback PBO offset drift");
static_assert(offsetof(idVirtualTextureSystem, renderDestFeedback) == 48072,
              "virtual-texture feedback destination offset drift");
static_assert(offsetof(idVirtualTextureSystem, analyzeParms) == 48144,
              "virtual-texture analysis parameters offset drift");
static_assert(offsetof(idVirtualTextureSystem, transcodeJobList) == 48272,
              "virtual-texture transcode list offset drift");
static_assert(offsetof(idVirtualTextureSystem, vmtrTotalPagesMetric) == 48496,
              "virtual-texture metric offset drift");
static_assert(sizeof(idVirtualTextureSystem) == 49264,
              "virtual-texture system ABI drift");
static_assert(sizeof(idVirtualMaterial) == 0x50,
              "idVirtualMaterial ABI drift");
