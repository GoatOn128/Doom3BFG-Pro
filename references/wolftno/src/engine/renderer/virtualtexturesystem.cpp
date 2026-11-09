#include "engine/renderer/virtualtexture.h"

#include "engine/framework/coderesource.h"
#include "engine/framework/console.h"
#include "engine/framework/debuggraph.h"
#include "engine/framework/streamfilecache.h"
#include "engine/decls/decl.h"
#include "engine/renderer/declrenderparm.h"
#include "engine/renderer/declrenderprog.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagedata.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/gl_api.h"
#include "engine/renderer/glconfig.h"
#include "engine/renderer/jobs/pageupdate/pageupdate.h"
#include "engine/renderer/jobs/render/parmstate.h"
#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/jobs/transcode/image/dxt/dxtencoder.h"
#include "engine/renderer/jobs/transcode/pagecompression.h"
#include "engine/renderer/renderdestination.h"
#include "engine/renderer/renderersystem.h"
#include "engine/renderer/renderlocal.h"
#include "engine/renderer/renderlog.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/geometry/rendermatrix.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/sys/sys_alloc.h"
#include "shared/idlib/sys/sys_time.h"
#include "shared/idlib/text/cmdargs.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <cstdio>
#include <cstdlib>

extern idCVar vt_feedbackLODIncrement;
extern idCVar vt_feedbackLowWater;
extern idCVar vt_feedbackHighWater;
extern idCVar vt_showPageUsage;
extern idCVar vt_maxAniso;
extern idCVar vt_filter;
extern idCVar vt_pageBorderAdjust;
extern idCVar vt_feedbackLodBias;
extern idCVar vt_lodBias;
extern idCVar vt_useDirectTextureAccess;
extern idCVar vt_genMissingPages;
extern idCVar vt_maxPhysicalPRTPages;
extern idCVar vt_uncompressedPhysicalImages;
extern idCVar vt_linearPhysicalImages;
extern idCVar vt_touchPhysCacheOnAlloc;
extern idCVar vt_pageFadeInSteps;
extern idCVar vt_useMinLodTexture;
extern idCVar vt_usePageFileCache;
extern idCVar vt_skipStreaming;
extern idCVar vt_pageImageSizeVmtr;
extern idCVar vt_pageImageSizeUnique;
extern idCVar vt_pageImageSizeUniqueDiffuseOnly;
extern idCVar vt_pageImageSizeUniqueDiffuseOnly2;
extern idCVar vt_showFeedbackBuffer;
extern idCVar vt_minLod;
extern idCVar vt_filePath;
extern idCVar vt_showExplicitPages;
extern idCVar vt_useAnalyzeJobs;
extern idCVar vt_usePageSortJobs;
extern idCVar vt_showFeedbackAnalysis;
extern idCVar vt_showFeedbackStreaming;
extern idCVar vt_noSeekCacheLines;
extern idCVar vt_showStats;
extern idCVar vt_recordStats;
extern idCVar forge_logPerformanceMetrics;
extern idCVar com_production;
extern idCVar r_skipVmtrs;
extern idCVar vt_readOnlyNetPages;
extern idCVar vt_useNonBufferedIO;
extern idCVar vt_enablePRT;
extern idCVar vt_packPageFiles;
extern idCVar vt_containers;
extern idCVar vt_uncompressedVmtr;
extern idCVar mt_sizeOverride;
extern idCVar feature_lowgore;
extern idCVar vt_singleStep;
extern idCVar vt_maxPPF;
extern idCVar vt_stripPageBorders;
extern idCVar vt_showPageSources;
extern idCVar vt_showPageMips;
extern idCVar vt_showPageColors;
extern idCVar vt_showPageNumbers;
extern idCVar vt_showPageInfo;
extern idCVar vt_showPageBorders;
extern idCVar vt_showPageCheckered;
extern idCVar vt_showPageSizes;
extern idCVar vt_useTranscodeJobs;
extern idCVar vt_usePageTablePBOs;
extern idCVar vt_useUnsynchronizedPageTablePBOs;
extern idCVar vt_useInvalidateRangeBitForMappingAndPageTablePBOMap;
extern idCVar vt_skipPhysicalTextureUploads;
extern idCVar vt_showPageLoads;
extern idCVar vt_upsampleFilter;
extern idCVar vt_upsampleSharpen;
extern idCVar vt_upsampleNoise;
extern idCVar vt_splitTranscodeJobs;
extern idCVar vt_skipTranscode;
extern idCVar vt_skipUpdatePageImages;
extern idCVar vt_supportRoundedDownVmtrs;
extern idCVar vt_maxLockedPages;
extern idCVar con_noPrint;
extern idCVar jobs_numThreads;
extern idCVar vt_useNetworkPagesWithPreload;
extern idCVar vt_filePathVmtrOverride;
extern idCVar vt_virtualSizeLog2;

extern idCodeResource<idDeclRenderParm> rpPhysicalPRTFilterParms;
extern idCodeResource<idDeclRenderParm> rpLandLayoutVersion;
extern idCodeResource<idDeclRenderParm> rpVirtualTextureOptions;
extern idCodeResource<idDeclRenderParm> rpPhysicalVmtrFilterParms;
extern idCodeResource<idDeclRenderParm> rpPhysicalUniqueFilterParms;
extern idCodeResource<idDeclRenderParm>
    rpPhysicalUniqueDiffuseOnlyFilterParms;
extern idCodeResource<idDeclRenderParm>
    rpPhysicalUniqueDiffuseOnly2FilterParms;
extern const char* physicalPoolNames[];
void Sys_CPUCount(int* numLogicalCPUCores, int* numPhysicalCPUCores,
                  int* numCPUPackages);
const char* Sys_GetProcessorString();
void Sys_SetClipboardData(const char* text);
void VirtualTextureInfo_f(const idCmdArgs& args);

namespace {
idCodeResource<idDeclRenderParm> rpViewColorMap("viewColorMap");
idCodeResource<idDeclRenderProg> progViewColorIdentity("viewColorIdentity");
}  // namespace

namespace {

constexpr std::uint16_t INVALID_PAGE_INDEX = 0xFFFFu;
constexpr int PAGE_SUBROOT_MAGIC = -893391612;

struct defaultDCTPage_t {
  std::uint8_t pad[24]{};
  pageHeader_t header{};
  DCTHeader_t DCTHeader{};
  std::uint8_t pageData[2584]{};
};

defaultDCTPage_t defaultDCTPage = {
    {},
    {PAGE_SUBROOT_MAGIC, COMP_DCT, {}, {}, 0, 0, 0, 0},
    {0x5A, 0x50, 0x3C, 0x50, 0x50, 0x04, 0x0000, 0x070C, 0x0180,
     0x0142, 0x000E},
    {
#include "engine/renderer/virtualtexturesystem_default_dct.inc"
    }};

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value << 8u) | (value >> 8u));
}

void SwapDCTDataLengths(DCTHeader_t& header) {
  header.diffuseSize = ByteSwap16(header.diffuseSize);
  header.normalSize = ByteSwap16(header.normalSize);
  header.specularSize = ByteSwap16(header.specularSize);
  header.powerSize = ByteSwap16(header.powerSize);
  header.alphaSize = ByteSwap16(header.alphaSize);
}

void* AllocVirtualTexture(std::size_t size, bool zero, align_t alignment) {
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\renderer\\VirtualTextureSystem.cpp : "
      "TAG_VIRTUALTEXTURE",
      size, TAG_VIRTUALTEXTURE, zero, alignment, HEAP_DEFAULTHEAP);
}

void FreeVirtualTexture(void* pointer, align_t alignment) {
  if (pointer != nullptr) {
    idMem::Singleton()->Free(pointer, alignment);
  }
}

std::uint32_t PhysicalPageID(const texturePage_t& page) {
  return static_cast<std::uint32_t>(page.pageX) |
         (static_cast<std::uint32_t>(page.pageY) << 11u) |
         (static_cast<std::uint32_t>(page.pageLevel) << 22u) |
         (static_cast<std::uint32_t>(page.pageSource) << 26u);
}

std::uint32_t PhysicalPageHashKey(const texturePage_t& page) {
  return static_cast<std::uint32_t>(page.pageX) |
         (static_cast<std::uint32_t>(page.pageY) << 11u) |
         (static_cast<std::uint32_t>(page.pageLevel) << 22u);
}

std::uint32_t HashPhysicalPage(std::uint32_t key, int mask) {
  const std::uint32_t mixed = 1664525u * key + 1013904223u;
  return (mixed ^ ((mixed ^ (mixed >> 11u)) >> 11u)) &
         static_cast<std::uint32_t>(mask);
}

std::uint8_t EncodePhysicalCoordinate(int coordinate, int pagesWide) {
  if (pagesWide <= 1) {
    return 0;
  }
  const float encoded =
      static_cast<float>(coordinate) /
          static_cast<float>(pagesWide - 1) *
          255.0f +
      0.01f;
  return static_cast<std::uint8_t>(
      std::clamp(static_cast<int>(encoded), 0, 255));
}

int FloorLog2(int value) {
  int result = 0;
  while (value > 1) {
    value >>= 1;
    ++result;
  }
  return result;
}

pageImagesUpdateParms_t& UnmapParms(transcodeJobList_t& jobs) {
  return *reinterpret_cast<pageImagesUpdateParms_t*>(
      jobs.pageUnmapParmsStorage);
}

pageImagesUpdateParms_t& ImageUpdateParms(transcodeJobList_t& jobs) {
  return *reinterpret_cast<pageImagesUpdateParms_t*>(
      jobs.pageImagesUpdateParmsStorage);
}

int FindPhysicalPage(const idPhysicalPages& physicalPages, pageSource_t source,
                     int level, int x, int y) {
  texturePage_t key{};
  key.pageSource = source;
  key.pageLevel = static_cast<std::uint8_t>(level);
  key.pageX = static_cast<std::uint16_t>(x);
  key.pageY = static_cast<std::uint16_t>(y);
  const std::uint32_t bucket = HashPhysicalPage(
      PhysicalPageHashKey(key), physicalPages.pageHash.hashMask);
  for (std::uint16_t index = static_cast<std::uint16_t>(
           physicalPages.pageHash.pageHashTable[bucket]);
       index != INVALID_PAGE_INDEX;
       index = physicalPages.pageHash.pageHashIndex[index].next) {
    if (physicalPages.pageHash.pageHashIndex[index].pageID ==
        PhysicalPageID(key)) {
      return index;
    }
  }
  return -1;
}

void ParsePhysicalImageSize(const idCVar& cvar, bool enabled, int& width,
                            int& height) {
  width = 128;
  height = 128;
  if (!enabled) {
    return;
  }
  const int fields =
      std::sscanf(cvar.valueString.data, "%i %i", &width, &height);
  if (fields == 0) {
    width = 4096;
    height = 4096;
    return;
  }
  if (fields == 1) {
    height = width;
  }
  width = std::max(width, 1024);
  height = std::max(height, 1024);
}

void PurgePhysicalImages(idPhysicalPages& physicalPages) {
  if (physicalPages.physicalPagesImage0 != nullptr) {
    physicalPages.physicalPagesImage0->PurgeImage();
  }
  if (physicalPages.physicalPagesImage1 != nullptr) {
    physicalPages.physicalPagesImage1->PurgeImage();
  }
  if (physicalPages.physicalPagesImage2 != nullptr) {
    physicalPages.physicalPagesImage2->PurgeImage();
  }
  physicalPages.isDirectMapped = false;
}

void SchedulePageSort(idPhysicalPages& physicalPages, int frameNum,
                      idParallelJobList* jobList, bool useJob) {
  idPhysicalPageList& pages = physicalPages.unlockedPageList;
  if (pages.lastPageIndex < pages.firstPageIndex) {
    return;
  }
  pages.sortDone = 0;
  pages.sortParms->frameNum = frameNum;
  pages.sortParms->pagePriority = physicalPages.pagePriority;
  pages.sortParms->pageFrameNum = physicalPages.pageFrameNum;
  pages.sortParms->physicalPageNums = pages.physicalPageNums;
  pages.sortParms->firstPageIndex = &pages.firstPageIndex;
  pages.sortParms->lastPageIndex = &pages.lastPageIndex;
  pages.sortParms->indexMask = pages.indexMask;
  pages.sortParms->sortedPhysicalPageNums = pages.physicalPageNums;
  pages.sortParms->done = &pages.sortDone;
  if (useJob) {
    jobList->AddJob(reinterpret_cast<jobRun_t>(PageSortJob), pages.sortParms);
  } else {
    PageSortJob(pages.sortParms);
  }
}

}  // namespace

static_assert(sizeof(defaultDCTPage_t) == 0xA68,
              "default DCT page ABI drift");
static_assert(offsetof(defaultDCTPage_t, header) == 0x18,
              "default DCT page-header offset drift");
static_assert(offsetof(defaultDCTPage_t, DCTHeader) == 0x40,
              "default DCT codec-header offset drift");
static_assert(offsetof(defaultDCTPage_t, pageData) == 0x50,
              "default DCT payload offset drift");

struct cacheLineStat_t {
  int fileID{};
  int mipLevel{};
  std::uint64_t diskCacheLine{};
};

struct pageFileStats_t {
  pageFileStats_t();
  ~pageFileStats_t();

  idStr path;
  std::uint64_t length{};
  std::uint64_t diskOffset{};
  idList<cacheLineStat_t, 5> cacheLines;
};

pageFileStats_t::pageFileStats_t() = default;
pageFileStats_t::~pageFileStats_t() = default;

template <typename T> class idSort {
public:
  virtual ~idSort() = default;
  virtual int Compare(const T& left, const T& right) const = 0;
};

class idSort_PathStr : public idSort<idStr> {
public:
  ~idSort_PathStr() override;
  int Compare(const idStr& left, const idStr& right) const override {
    return idStr::IcmpPath(left.data, right.data);
  }
};

idSort_PathStr::~idSort_PathStr() = default;

template <typename T, typename Comparator> class idSort_Quick
    : public Comparator {
public:
  __declspec(noinline) void Sort(T* base, unsigned int num) {
    if (num <= 1) {
      return;
    }

    std::ptrdiff_t lo[128];
    std::ptrdiff_t hi[128];
    std::ptrdiff_t low = 0;
    std::ptrdiff_t high = static_cast<std::ptrdiff_t>(num) - 1;
    std::ptrdiff_t stackIndex = 0;

    for (;;) {
      const std::ptrdiff_t currentStackIndex = stackIndex;
      std::ptrdiff_t left = low;

      while (high - low < 4 || stackIndex >= 127) {
        if (low < high) {
          for (T* destination = &base[high]; high > low;
               --high, --destination) {
            std::ptrdiff_t greatest = low;
            for (std::ptrdiff_t index = low + 1; index <= high; ++index) {
              if (this->Compare(base[index], base[greatest]) > 0) {
                greatest = index;
              }
            }
            T temporary(base[greatest]);
            base[greatest] = *destination;
            *destination = temporary;
          }
        }

        stackIndex = currentStackIndex - 1;
        if (stackIndex < 0) {
          return;
        }
        low = lo[stackIndex];
        high = hi[stackIndex];
        left = low;
      }

      {
        T temporary(base[high]);
        base[high] = base[(high + low) / 2];
        base[(high + low) / 2] = temporary;
      }

      std::ptrdiff_t right = high - 1;
      for (;;) {
        while (this->Compare(base[left], base[high]) < 0) {
          ++left;
          if (left >= right) {
            break;
          }
        }
        while (this->Compare(base[right], base[high]) > 0) {
          --right;
          if (right <= left) {
            break;
          }
        }
        if (left >= right) {
          break;
        }

        T temporary(base[left]);
        base[left] = base[right];
        base[right] = temporary;
        ++left;
        --right;
        if (left >= right) {
          break;
        }
      }

      while (right > low && this->Compare(base[right], base[high]) >= 0) {
        --right;
      }
      while (left < high && this->Compare(base[left], base[high]) <= 0) {
        ++left;
      }

      {
        T temporary(base[high]);
        base[high] = base[left];
        base[left] = temporary;
      }

      if (high - left <= right - low) {
        lo[currentStackIndex] = low;
        hi[currentStackIndex] = right;
        low = left;
      } else {
        lo[currentStackIndex] = left;
        hi[currentStackIndex] = high;
        high = right;
      }
      stackIndex = currentStackIndex + 1;
    }
  }
};

template class idSort_Quick<idStr, idSort_PathStr>;

template <typename T>
T* idListArrayNew(int num, unsigned int tag, bool zeroBuffer) {
  if (num <= 0) {
    return nullptr;
  }
  T* result = static_cast<T*>(idMem::Singleton()->AllocWithLocation(
      zeroBuffer
          ? "w:\\build\\tech5\\shared\\idlib\\containers/List.h(27) : tag"
          : "w:\\build\\tech5\\shared\\idlib\\containers/List.h(29) : tag",
      sizeof(T) * static_cast<std::size_t>(num),
      static_cast<memTag_t>(tag), zeroBuffer, ALIGN_16, HEAP_DEFAULTHEAP));
  int constructed = 0;
  try {
    for (; constructed < num; ++constructed) {
      new (&result[constructed]) T();
    }
  } catch (...) {
    while (constructed > 0) {
      result[--constructed].~T();
    }
    idMem::Singleton()->Free(result, ALIGN_16);
    throw;
  }
  return result;
}

template <typename T> void idListArrayDelete(T* pointer, int num) {
  if (pointer != nullptr) {
    for (int index = 0; index < num; ++index) {
      pointer[index].~T();
    }
  }
  idMem::Singleton()->Free(pointer, ALIGN_16);
}

template <typename T>
T* idListArrayResize(T* oldPointer, int oldNum, int newNum,
                     unsigned int tag, bool zeroBuffer) {
  T* replacement =
      newNum > 0 ? idListArrayNew<T>(newNum, tag, zeroBuffer) : nullptr;
  if (newNum > 0 && replacement == nullptr) {
    return nullptr;
  }
  const int copyCount = std::min(oldNum, newNum);
  for (int index = 0; index < copyCount; ++index) {
    replacement[index] = oldPointer[index];
  }
  idListArrayDelete(oldPointer, oldNum);
  return replacement;
}

template pageFileStats_t* idListArrayNew<pageFileStats_t>(
    int, unsigned int, bool);
template void idListArrayDelete<pageFileStats_t>(pageFileStats_t*, int);
template pageFileStats_t* idListArrayResize<pageFileStats_t>(
    pageFileStats_t*, int, int, unsigned int, bool);
template idList<cacheLineStat_t, 5>&
idList<cacheLineStat_t, 5>::operator=(
    const idList<cacheLineStat_t, 5>&);
template idList<pageFileStats_t, 5>::~idList();

int SortCacheLinesOnOffset(const cacheLineStat_t* a,
                           const cacheLineStat_t* b) {
  return a->diskCacheLine > b->diskCacheLine
             ? 1
             : (a->diskCacheLine < b->diskCacheLine ? -1 : 0);
}

static_assert(sizeof(cacheLineStat_t) == 0x10,
              "cache-line statistics ABI drift");
static_assert(sizeof(pageFileStats_t) == 0x50,
              "page-file statistics ABI drift");

namespace {
int transcodeJobHistoryIndex = 0;
int lastTranscodeTime = 0;
int transcodeJobHistoryBytes[16]{};
int transcodeJobHistoryPages[16]{};
int transcodeJobHistoryTime[16]{};
float transcodeJobHistoryGPUTime[16]{};
idDebugGraph* transcodeDebugGraph = nullptr;
idOverlayHandle transcodeOverlayHandle;
idFile* transcodeStatsFile = nullptr;
idDebugGraph* benchmarkGraph = nullptr;
}  // namespace

idPhysicalPageList::~idPhysicalPageList() {
  FreeVirtualTexture(physicalPageNums, ALIGN_16);
  FreeVirtualTexture(sortParms, ALIGN_16);
}

void idPhysicalPageList::Init(int size) {
  if (physicalPageNums != nullptr) {
    FreeVirtualTexture(physicalPageNums, ALIGN_128);
  }
  firstPageIndex = 0;
  lastPageIndex = -1;
  indexMask = size - 1;
  physicalPageNums = static_cast<std::int64_t*>(AllocVirtualTexture(
      sizeof(*physicalPageNums) * static_cast<std::size_t>(size), false,
      ALIGN_128));
  sortDone = 1;
  if (sortParms != nullptr) {
    FreeVirtualTexture(sortParms, ALIGN_16);
  }
  sortParms = static_cast<pageSortParms_t*>(
      AllocVirtualTexture(sizeof(*sortParms), false, ALIGN_16));
}

void idPhysicalPageList::Remove(int physicalPageNum) {
  int found = firstPageIndex;
  while (found <= lastPageIndex &&
         static_cast<int>(physicalPageNums[found & indexMask]) !=
             physicalPageNum) {
    ++found;
  }
  if (found > lastPageIndex) {
    return;
  }
  while (found > firstPageIndex) {
    physicalPageNums[found & indexMask] =
        physicalPageNums[(found - 1) & indexMask];
    --found;
  }
  ++firstPageIndex;
}

idPhysicalPageHash::~idPhysicalPageHash() {
  if (pageHashTable != nullptr) {
    FreeVirtualTexture(pageHashTable, ALIGN_128);
    pageHashTable = nullptr;
  }
  if (pageHashIndex != nullptr) {
    FreeVirtualTexture(pageHashIndex, ALIGN_128);
    pageHashIndex = nullptr;
  }
}

void idPhysicalPageHash::Init(int hashSize, int indexSize) {
  hashMask = hashSize - 1;
  if (pageHashTable != nullptr) {
    FreeVirtualTexture(pageHashTable, ALIGN_128);
  }
  pageHashTable = static_cast<std::int16_t*>(AllocVirtualTexture(
      sizeof(*pageHashTable) * static_cast<std::size_t>(hashSize), false,
      ALIGN_128));
  std::memset(pageHashTable, 0xFF,
              sizeof(*pageHashTable) * static_cast<std::size_t>(hashSize));

  if (pageHashIndex != nullptr) {
    FreeVirtualTexture(pageHashIndex, ALIGN_128);
  }
  pageHashIndex = static_cast<pageHashEntry_t*>(AllocVirtualTexture(
      sizeof(*pageHashIndex) * static_cast<std::size_t>(indexSize), true,
      ALIGN_128));
}

void idPhysicalPageHash::RemovePage(texturePage_t* page) {
  const std::uint32_t pageID = PhysicalPageID(*page);
  const std::uint32_t bucket =
      HashPhysicalPage(PhysicalPageHashKey(*page), hashMask);
  std::uint16_t* link =
      reinterpret_cast<std::uint16_t*>(&pageHashTable[bucket]);
  while (*link != INVALID_PAGE_INDEX) {
    pageHashEntry_t& entry = pageHashIndex[*link];
    if (entry.pageID == pageID) {
      *link = entry.next;
      return;
    }
    link = &entry.next;
  }
}

idPhysicalPages::idPhysicalPages()
    : physicalPagesPool(PHYSICAL_PAGES_POOL_MAX), pages(nullptr),
      pagePriority(nullptr), pageFrameNum(nullptr), pageFade(nullptr),
      numLockedPages(0), pageImageCompressed(false), isDirectMapped(false),
      PHYSICAL_TEXELS_WIDE(1), PHYSICAL_TEXELS_HIGH(1),
      PHYSICAL_PAGES_WIDE(1), PHYSICAL_PAGES_HIGH(1),
      NUM_PHYSICAL_PAGES(1), physicalPagesImage0(nullptr),
      physicalPagesImage1(nullptr), physicalPagesImage2(nullptr),
      physicalMappingsImage(nullptr), physicalMappingsImageShadow(nullptr),
      feedbackNumPages(0), feedbackDynamicLodBias(0.0f),
      oldFeedbackDynamicLodBias(0.0f), rpPhysicalFilterParms(nullptr) {
  pageHash.hashMask = 0;
  freePageList.firstPageIndex = 0;
  freePageList.lastPageIndex = 0;
  freePageList.indexMask = 0;
  freePageList.sortDone = 1;
  unlockedPageList.firstPageIndex = 0;
  unlockedPageList.lastPageIndex = 0;
  unlockedPageList.indexMask = 0;
  unlockedPageList.sortDone = 1;
}

idPhysicalPages::~idPhysicalPages() { Clear(); }

void idPhysicalPages::Clear() {
  if (pages != nullptr) {
    FreeVirtualTexture(pages, ALIGN_128);
    pages = nullptr;
  }
  if (pagePriority != nullptr) {
    FreeVirtualTexture(pagePriority, ALIGN_128);
    pagePriority = nullptr;
  }
  if (pageFrameNum != nullptr) {
    FreeVirtualTexture(pageFrameNum, ALIGN_128);
    pageFrameNum = nullptr;
  }
  if (pageFade != nullptr) {
    FreeVirtualTexture(pageFade, ALIGN_128);
    pageFade = nullptr;
  }
  if (pageHash.pageHashTable != nullptr) {
    FreeVirtualTexture(pageHash.pageHashTable, ALIGN_128);
    pageHash.pageHashTable = nullptr;
  }
  if (pageHash.pageHashIndex != nullptr) {
    FreeVirtualTexture(pageHash.pageHashIndex, ALIGN_128);
    pageHash.pageHashIndex = nullptr;
  }
  physicalMappingsImagePBO.FreeBufferObject();
  if (physicalMappingsImageShadow != nullptr) {
    FreeVirtualTexture(physicalMappingsImageShadow, ALIGN_16);
    physicalMappingsImageShadow = nullptr;
  }
}

int idPhysicalPages::GetNumResidentPagesForSource(pageSource_t source) const {
  int count = 0;
  for (int pageNum = 0; pageNum < NUM_PHYSICAL_PAGES; ++pageNum) {
    count += pages[pageNum].pageSource == source;
  }
  return count;
}

int idPhysicalPages::GetNumLockedPagesForSource(pageSource_t source) const {
  int count = 0;
  for (int pageNum = 0; pageNum < NUM_PHYSICAL_PAGES; ++pageNum) {
    count += pages[pageNum].pageSource == source && pages[pageNum].locked;
  }
  return count;
}

bool idPhysicalPages::AllocWouldThrash(char newPagePriority) {
  if (freePageList.lastPageIndex - freePageList.firstPageIndex != -1) {
    return false;
  }
  const int pageNum = static_cast<int>(
      unlockedPageList.physicalPageNums[unlockedPageList.firstPageIndex &
                                       unlockedPageList.indexMask]);
  return pageFrameNum[pageNum] >= virtualTextureSystem.feedbackFrameNum - 1 &&
         ((newPagePriority & 0x10) == 0 ||
          (pagePriority[pageNum] & 0x10) != 0);
}

void idPhysicalPages::CreatePageImagesUpdate(
    idVirtualTexture* virtualTexture, const texturePage_t* physicalPage,
    texturePage_t* virtualPage, const void* pageDataPBO,
    const std::uint8_t* pageData, pageImagesUpdate_t* update) {
  std::memset(update, 0, sizeof(*update));
  update->physicalPagesPool = virtualTexture->GetPhysicalPagesPool();
  update->physicalPageNum = physicalPage->physicalPageNum;
  update->physicalPagesImage0 = physicalPagesImage0;
  update->physicalPagesImage1 = physicalPagesImage1;
  update->physicalPagesImage2 = physicalPagesImage2;
  update->compressed = pageImageCompressed;
  update->pageDataPBO = pageDataPBO;
  update->pageData = pageData;

  const bool unmap = physicalPage != virtualPage;
  if (physicalPagesPool == PHYSICAL_PAGES_POOL_PRT) {
    update->type = unmap ? PAGE_UPDATE_UNMAP_PRT : PAGE_UPDATE_MAP_PRT;
    update->physX = virtualPage->pageX;
    update->physY = virtualPage->pageY;
    update->mipLevel = virtualPage->pageLevel;
  } else {
    update->type = unmap ? PAGE_UPDATE_UNMAP_PHYSICAL_PAGE
                         : PAGE_UPDATE_MAP_PHYSICAL_PAGE;
    update->physX = physicalPage->physicalPageX;
    update->physY = physicalPage->physicalPageY;
    update->mipLevel = 0;
  }

  if (!unmap && virtualPage->mapped) {
    return;
  }

  if (physicalMappingsImage != nullptr && !unmap) {
    const float levelPagesWide = static_cast<float>(
        virtualTexture->header.pagesWide >> physicalPage->pageLevel);
    const float usablePageWidth =
        128.0f - (vt_pageBorderAdjust.valueFloat + 4.0f) * 2.0f;
    const float scale = usablePageWidth *
                        static_cast<float>(virtualTexture->header.pagesWide) /
                        (static_cast<float>(1 << physicalPage->pageLevel) *
                         static_cast<float>(PHYSICAL_TEXELS_WIDE));
    update->map[0] = scale;
    update->map[1] =
        (0.9375f / static_cast<float>(PHYSICAL_PAGES_WIDE)) *
        levelPagesWide;
    update->map[2] =
        (static_cast<float>((update->physX << 7) + 4) +
         vt_pageBorderAdjust.valueFloat) /
            static_cast<float>(PHYSICAL_TEXELS_WIDE) -
        (static_cast<float>(physicalPage->pageX) / levelPagesWide) * scale;
    update->map[3] =
        (static_cast<float>((update->physY << 7) + 4) +
         vt_pageBorderAdjust.valueFloat) /
            static_cast<float>(PHYSICAL_TEXELS_HIGH) -
        (static_cast<float>(physicalPage->pageY) / levelPagesWide) * scale;

    if (vt_useDirectTextureAccess.valueInteger != 0) {
      update->physicalMappingsImageInfo.bits = physicalMappingsImage->buffers[0];
      update->physicalMappingsImageInfo.bytePitch =
          physicalMappingsImage->pitches[0];
    }
    if (update->physicalMappingsImageInfo.bits == nullptr) {
      update->physicalMappingsImage = physicalMappingsImage;
      update->physicalMappingsImagePBO = physicalMappingsImagePBO.apiObject;
      update->physicalMappingsImagePBOSize =
          physicalMappingsImagePBO.GetAllocedSize();
      update->physicalMappingsImageInfo.bits = physicalMappingsImageShadow;
      update->physicalMappingsImageInfo.bytePitch = 16 * PHYSICAL_PAGES_WIDE;
    }
  }

  if (virtualTexture->pageTableImage != nullptr) {
    const std::uint8_t encodedX =
        EncodePhysicalCoordinate(update->physX, PHYSICAL_PAGES_WIDE);
    const std::uint8_t encodedY =
        EncodePhysicalCoordinate(update->physY, PHYSICAL_PAGES_WIDE);
    update->pageSpot[0] = encodedX;
    update->pageSpot[1] = encodedY;
    update->pageSpot[2] = encodedX;
    update->pageSpot[3] = encodedY;
    update->spotX = virtualPage->pageX;
    update->spotY = virtualPage->pageY;
    update->effectivePageLevel =
        virtualTexture->pageTableImage->opts.numLevels +
        virtualPage->pageLevel - virtualTexture->header.numLevels;

    if (vt_useDirectTextureAccess.valueInteger != 0) {
      for (int level = 0; level < 14; ++level) {
        update->pageTableImageInfo[level].bits =
            virtualTexture->pageTableImage->buffers[level];
        update->pageTableImageInfo[level].bytePitch =
            virtualTexture->pageTableImage->pitches[level];
      }
    }
    if (update->pageTableImageInfo[0].bits == nullptr) {
      update->pageTableImage = virtualTexture->pageTableImage;
      update->pageTableImagePBO = virtualTexture->pageTableImagePBO.apiObject;
      update->pageTableImagePBOSize =
          virtualTexture->pageTableImagePBO.GetAllocedSize();
      std::size_t offset = 0;
      for (int level = 0; level < 14; ++level) {
        const int levelWidth =
            1 << (virtualTexture->header.numLevels - level - 1);
        update->pageTableImageInfo[level].bits =
            static_cast<std::uint8_t*>(virtualTexture->pageTableImageShadow) +
            offset;
        update->pageTableImageInfo[level].bytePitch = 2 * levelWidth;
        offset += static_cast<std::size_t>(2 * levelWidth * levelWidth);
      }
    }
  }

  virtualPage->mapped = physicalPage == virtualPage;
  pageHash.pageHashIndex[virtualPage->physicalPageNum].mapped =
      physicalPage == virtualPage;
}

void idPhysicalPages::UpdatePageImages(idVirtualTexture* virtualTexture,
                                       texturePage_t* physicalPage,
                                       texturePage_t* virtualPage,
                                       const std::uint8_t* pageData) {
  pageImagesUpdate_t update{};
  CreatePageImagesUpdate(virtualTexture, physicalPage, virtualPage, nullptr,
                         pageData, &update);
  pageImagesUpdateParms_t parms{};
  parms.updates = &update;
  parms.numUpdates = 1;
  parms.submittedJob = true;
  UpdatePageImagesJob(&parms);
  UploadPageImages(&parms, nullptr);
}

void idPhysicalPages::GetDirectMappedPhysicalPagePointers(
    transcodeParms_t* parms) {
  parms->directmapped = 1;
  const bool diffuseOnly =
      physicalPagesPool == PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY ||
      physicalPagesPool == PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2;
  if (pageImageCompressed) {
    const int blockX = (parms->physicalPageX << 7) / 4;
    const int blockY = (parms->physicalPageY << 7) / 4;
    if (diffuseOnly) {
      parms->targetImage[1] =
          static_cast<std::uint8_t*>(physicalPagesImage1->buffers[0]) +
          16 * blockX + blockY * physicalPagesImage1->pitches[0];
      parms->targetBytePitch[1] = physicalPagesImage1->pitches[0];
    } else {
      parms->targetImage[0] =
          static_cast<std::uint8_t*>(physicalPagesImage0->buffers[0]) +
          8 * blockX + blockY * physicalPagesImage0->pitches[0];
      parms->targetImage[1] =
          static_cast<std::uint8_t*>(physicalPagesImage1->buffers[0]) +
          16 * blockX + blockY * physicalPagesImage1->pitches[0];
      parms->targetImage[2] =
          static_cast<std::uint8_t*>(physicalPagesImage2->buffers[0]) +
          16 * blockX + blockY * physicalPagesImage2->pitches[0];
      parms->targetBytePitch[0] = physicalPagesImage0->pitches[0];
      parms->targetBytePitch[1] = physicalPagesImage1->pitches[0];
      parms->targetBytePitch[2] = physicalPagesImage2->pitches[0];
    }
    return;
  }

  const int pixelX = parms->physicalPageX << 7;
  const int pixelY = parms->physicalPageY << 7;
  if (diffuseOnly) {
    parms->targetImage[1] =
        static_cast<std::uint8_t*>(physicalPagesImage1->buffers[0]) +
        4 * pixelX + pixelY * physicalPagesImage1->pitches[0];
    parms->targetBytePitch[1] = physicalPagesImage1->pitches[0];
  } else {
    for (int imageIndex = 0; imageIndex < 3; ++imageIndex) {
      idImage* image = imageIndex == 0   ? physicalPagesImage0
                       : imageIndex == 1 ? physicalPagesImage1
                                         : physicalPagesImage2;
      parms->targetImage[imageIndex] =
          static_cast<std::uint8_t*>(image->buffers[0]) + 4 * pixelX +
          pixelY * image->pitches[0];
      parms->targetBytePitch[imageIndex] = image->pitches[0];
    }
  }
}

void UpdateFeedbackDynamicLodBias(float* feedbackDynamicLodBias,
                                  int totalPages, int numFeedbackPages,
                                  bool forceUpdate) {
  const float increment = vt_feedbackLODIncrement.valueFloat;
  if (increment <= 0.0f || vt_showPageUsage.valueInteger > 0 || forceUpdate) {
    *feedbackDynamicLodBias = 0.0f;
    return;
  }

  const float total = static_cast<float>(totalPages);
  const float lowWater =
      std::floor(vt_feedbackLowWater.valueFloat * 0.01f * total);
  const float highWater =
      std::floor(vt_feedbackHighWater.valueFloat * 0.01f * total);
  const float used = static_cast<float>(numFeedbackPages);
  if (used > highWater) {
    *feedbackDynamicLodBias = std::min(
        10.0f, *feedbackDynamicLodBias +
                   increment *
                       std::min(1.0f, (used - highWater) /
                                          (total - highWater)));
  } else if (used < lowWater) {
    *feedbackDynamicLodBias = std::max(
        0.0f, *feedbackDynamicLodBias -
                  increment * std::min(1.0f, (lowWater - used) / lowWater));
  }
}

void UpdatePhysicalFilterParms(const idDeclRenderParm* filterParms,
                               float feedbackDynamicLodBias,
                               float* oldFeedbackDynamicLodBias, bool prt,
                               bool forceUpdate) {
  if (std::fabs(feedbackDynamicLodBias - *oldFeedbackDynamicLodBias) <=
          0.01f &&
      (vt_maxAniso.flags & 0x20000) == 0 &&
      (vt_lodBias.flags & 0x20000) == 0 &&
      (vt_feedbackLodBias.flags & 0x20000) == 0 && !forceUpdate) {
    return;
  }
  const float maxAniso = prt ? 16.0f : 4.0f;
  const float aniso = std::clamp(vt_maxAniso.valueFloat, 1.0f, maxAniso);
  renderThreadParmState->SetParmVector(
      filterParms, vt_lodBias.valueFloat,
      vt_feedbackLodBias.valueFloat + feedbackDynamicLodBias,
      std::log(aniso) * (1.0f / 0.69314718f), 0.0f);
  *oldFeedbackDynamicLodBias = feedbackDynamicLodBias;
}

void idPhysicalPages::UpdateFilterParms(bool forceUpdateFilterParms) {
  const bool prt = physicalPagesPool == PHYSICAL_PAGES_POOL_PRT;
  const bool diffuseOnly =
      physicalPagesPool == PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY ||
      physicalPagesPool == PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2;
  if ((vt_maxAniso.flags & 0x20000) != 0) {
    const float aniso =
        std::clamp(vt_maxAniso.valueFloat, 1.0f, prt ? 16.0f : 4.0f);
    if (physicalPagesImage1 != nullptr) {
      samplerOptions_t sampler = physicalPagesImage1->opts.sampler;
      sampler.aniso = aniso;
      physicalPagesImage1->ModifySampler(sampler);
    }
    if (!diffuseOnly) {
      if (physicalPagesImage0 != nullptr) {
        samplerOptions_t sampler = physicalPagesImage0->opts.sampler;
        sampler.aniso = aniso;
        physicalPagesImage0->ModifySampler(sampler);
      }
      if (physicalPagesImage2 != nullptr) {
        samplerOptions_t sampler = physicalPagesImage2->opts.sampler;
        sampler.aniso = aniso;
        physicalPagesImage2->ModifySampler(sampler);
      }
    }
  }
  if ((vt_filter.flags & 0x20000) != 0) {
    const textureFilter_t filter =
        vt_filter.valueInteger == 0 ? TF_LINEAR : TF_NEAREST;
    if (physicalPagesImage1 != nullptr) {
      samplerOptions_t sampler = physicalPagesImage1->opts.sampler;
      sampler.filter = filter;
      physicalPagesImage1->ModifySampler(sampler);
    }
    if (!diffuseOnly) {
      if (physicalPagesImage0 != nullptr) {
        samplerOptions_t sampler = physicalPagesImage0->opts.sampler;
        sampler.filter = filter;
        physicalPagesImage0->ModifySampler(sampler);
      }
      if (physicalPagesImage2 != nullptr) {
        samplerOptions_t sampler = physicalPagesImage2->opts.sampler;
        sampler.filter = filter;
        physicalPagesImage2->ModifySampler(sampler);
      }
    }
  }
  if (rpPhysicalFilterParms != nullptr) {
    UpdateFeedbackDynamicLodBias(&feedbackDynamicLodBias, NUM_PHYSICAL_PAGES,
                                 feedbackNumPages,
                                 forceUpdateFilterParms);
    UpdatePhysicalFilterParms((*rpPhysicalFilterParms).ConstPtr(),
                              feedbackDynamicLodBias,
                              &oldFeedbackDynamicLodBias, prt,
                              forceUpdateFilterParms);
  }
}

const idDeclRenderParm* idPhysicalPages::GetFilterParms() const {
  return rpPhysicalFilterParms != nullptr
             ? (*rpPhysicalFilterParms).ConstPtr()
             : rpPhysicalPRTFilterParms.ConstPtr();
}

void idPhysicalPages::ValidateCache() {
  while (unlockedPageList.sortDone == 0) {
    Sys_Yield();
  }
  idLib::Printf("%s\n", physicalPoolNames[physicalPagesPool]);
  idLib::Printf("------------------------\n");
  int freePages = 0;
  int currentPages = 0;
  int lockedPages = 0;
  int count[2][16][16]{};
  for (int pageNum = 0; pageNum < NUM_PHYSICAL_PAGES; ++pageNum) {
    const texturePage_t& page = pages[pageNum];
    freePages += !page.mapped;
    lockedPages += page.locked;
    if (page.pageSource >= PAGESOURCE_TOTAL) {
      idLib::Printf("Bad pageSource\n");
      continue;
    }
    if (page.pageLevel >= 16) {
      idLib::Printf("Bad level: %u\n", page.pageLevel);
      continue;
    }
    ++count[0][page.pageSource][page.pageLevel];
    if (virtualTextureSystem.feedbackFrameNum -
            pageFrameNum[page.physicalPageNum] <
        3) {
      ++count[1][page.pageSource][page.pageLevel];
      ++currentPages;
    }
  }

  const int unlockedCount = unlockedPageList.lastPageIndex -
                            unlockedPageList.firstPageIndex + 1;
  const int freeCount =
      freePageList.lastPageIndex - freePageList.firstPageIndex + 1;
  if (unlockedCount + freeCount + lockedPages != NUM_PHYSICAL_PAGES) {
    idLib::Warning(
        "unlockedPageListCount + freePageListCount + lockedPages != "
        "NUM_PHYSICAL_PAGES");
  }
  idLib::Printf("total  : %4i\n", NUM_PHYSICAL_PAGES);
  idLib::Printf("locked : %4i\n", lockedPages);
  idLib::Printf("current: %4i\n", currentPages);
  idLib::Printf("free   : %4i\n", freePages);

  int emptyBuckets = 0;
  int chainAtLeastOne = 0;
  int chainAtLeastTwo = 0;
  int chainAtLeastThree = 0;
  int chainPastThree = 0;
  int totalEntries = 0;
  for (int bucket = 0; bucket <= pageHash.hashMask; ++bucket) {
    int chainLength = 0;
    for (std::uint16_t index =
             static_cast<std::uint16_t>(pageHash.pageHashTable[bucket]);
         index != INVALID_PAGE_INDEX;
         index = pageHash.pageHashIndex[index].next) {
      ++chainLength;
    }
    totalEntries += chainLength;
    emptyBuckets += chainLength == 0;
    chainAtLeastOne += chainLength >= 1;
    chainAtLeastTwo += chainLength >= 2;
    chainAtLeastThree += chainLength >= 3;
    if (chainLength > 3) {
      chainPastThree += chainLength - 3;
    }
  }
  const int denominator = std::max(totalEntries, 1);
  idLib::Printf("%4u %% empty hash table entries\n",
                static_cast<unsigned>((100 * emptyBuckets + 1024) >> 11));
  idLib::Printf("%4i %% pages 0 steps down the hash chain\n",
                (denominator / 2 + 100 * chainAtLeastOne) / denominator);
  idLib::Printf("%4i %% pages 1 step down the hash chain\n",
                (denominator / 2 + 100 * chainAtLeastTwo) / denominator);
  idLib::Printf("%4i %% pages 2 steps down the hash chain\n",
                (denominator / 2 + 100 * chainAtLeastThree) / denominator);
  idLib::Printf("%4i %% pages > 2 steps down the hash chain\n",
                (denominator / 2 + 100 * chainPastThree) / denominator);
  idLib::Printf("------------------------\n");
}

bool idPhysicalPages::ValidatePageTable(const idImageData* pageTable,
                                        int currentLevel,
                                        int loppedLevels) const {
  bool valid = true;
  for (int y = 0; y < pageTable->height; ++y) {
    const std::uint8_t* pixel =
        pageTable->data + 4 * pageTable->width * y;
    for (int x = 0; x < pageTable->width; ++x, pixel += 4) {
      const int physicalX = PHYSICAL_PAGES_WIDE * pixel[0] / 256;
      const int physicalY = PHYSICAL_PAGES_HIGH * pixel[1] / 256;
      const texturePage_t& page =
          pages[PHYSICAL_PAGES_WIDE * physicalY + physicalX];
      const int pageLevel = page.pageLevel - loppedLevels;
      if (pageLevel < currentLevel) {
        valid &= x == page.pageX >> (currentLevel - pageLevel);
        valid &= y == page.pageY >> (currentLevel - pageLevel);
      } else {
        valid &= page.pageX == x >> (pageLevel - currentLevel);
        valid &= page.pageY == y >> (pageLevel - currentLevel);
      }
    }
  }
  return valid;
}

bool idPhysicalPages::CreatePhysicalImages(
    physicalPagesPool_t pool, const char* name, int pageImageWidth,
    int pageImageHeight,
    const idCodeResource<idDeclRenderParm>* physicalFilterParms) {
  idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);

  physicalPagesPool = pool;
  PHYSICAL_TEXELS_WIDE = pageImageWidth;
  PHYSICAL_TEXELS_HIGH = pageImageHeight;
  PHYSICAL_PAGES_WIDE = pageImageWidth / 128;
  PHYSICAL_PAGES_HIGH = pageImageHeight / 128;
  NUM_PHYSICAL_PAGES = PHYSICAL_PAGES_WIDE * PHYSICAL_PAGES_HIGH;
  if (pool == PHYSICAL_PAGES_POOL_PRT) {
    NUM_PHYSICAL_PAGES =
        std::min(NUM_PHYSICAL_PAGES, vt_maxPhysicalPRTPages.valueInteger);
  }

  FreeVirtualTexture(pages, ALIGN_128);
  pages = static_cast<texturePage_t*>(AllocVirtualTexture(
      sizeof(*pages) * static_cast<std::size_t>(NUM_PHYSICAL_PAGES), true,
      ALIGN_128));
  FreeVirtualTexture(pagePriority, ALIGN_128);
  pagePriority = static_cast<int*>(AllocVirtualTexture(
      sizeof(*pagePriority) * static_cast<std::size_t>(NUM_PHYSICAL_PAGES),
      true, ALIGN_128));
  FreeVirtualTexture(pageFrameNum, ALIGN_128);
  pageFrameNum = static_cast<int*>(AllocVirtualTexture(
      sizeof(*pageFrameNum) * static_cast<std::size_t>(NUM_PHYSICAL_PAGES),
      true, ALIGN_128));
  FreeVirtualTexture(pageFade, ALIGN_128);
  pageFade = static_cast<std::uint8_t*>(AllocVirtualTexture(
      static_cast<std::size_t>(NUM_PHYSICAL_PAGES), true, ALIGN_128));

  pageHash.Init(2048, NUM_PHYSICAL_PAGES);
  freePageList.Init(NUM_PHYSICAL_PAGES);
  unlockedPageList.Init(NUM_PHYSICAL_PAGES);
  for (int pageNum = 0; pageNum < NUM_PHYSICAL_PAGES; ++pageNum) {
    texturePage_t& page = pages[pageNum];
    page.physicalPageX = static_cast<std::int16_t>(pageNum % PHYSICAL_PAGES_WIDE);
    page.physicalPageY = static_cast<std::int16_t>(pageNum / PHYSICAL_PAGES_WIDE);
    page.physicalPageNum = static_cast<std::int16_t>(pageNum);
    page.coarserPageNum = -1;
    std::fill(std::begin(page.finerPageNum), std::end(page.finerPageNum),
              static_cast<std::int16_t>(-1));
    pagePriority[pageNum] = 0;
    pageFrameNum[pageNum] = 1;
    pageFade[pageNum] = 0;
    freePageList.physicalPageNums[++freePageList.lastPageIndex &
                                  freePageList.indexMask] = pageNum;
  }
  numLockedPages = 0;
  pageImageCompressed = vt_uncompressedPhysicalImages.valueInteger == 0 &&
                        pool != PHYSICAL_PAGES_POOL_PRT;
  isDirectMapped = true;

  idImageOpts options{};
  options.textureType = TT_2D;
  options.format = pageImageCompressed ? FMT_DXT5 : FMT_RGBA8;
  options.colorFormat = CFM_DEFAULT;
  options.width = PHYSICAL_TEXELS_WIDE;
  options.height = PHYSICAL_TEXELS_WIDE;
  options.numLevels =
      pool == PHYSICAL_PAGES_POOL_PRT ? FloorLog2(PHYSICAL_TEXELS_WIDE) : 1;
  options.packedTail = true;
  options.linear = vt_linearPhysicalImages.valueInteger != 0;
  options.partiallyResident = pool == PHYSICAL_PAGES_POOL_PRT;
  options.sampler.repeat = TR_CLAMP;
  options.sampler.filter =
      pool == PHYSICAL_PAGES_POOL_PRT ? TF_TRILINEAR : TF_LINEAR;
  options.sampler.aniso = std::clamp(
      vt_maxAniso.valueFloat, 1.0f,
      pool == PHYSICAL_PAGES_POOL_PRT ? 16.0f : 4.0f);
  options.sampler.lodMaxClamp = static_cast<float>(
      pool == PHYSICAL_PAGES_POOL_PRT ? FloorLog2(PHYSICAL_PAGES_WIDE) : 1);

  bool allocated = true;
  char imageName[128]{};
  std::snprintf(imageName, sizeof(imageName), "_physical%sPages1", name);
  physicalPagesImage1 = globalImages->ScratchImage(imageName, options);
  if (physicalPagesImage1->allocationFailed) {
    allocated = false;
    isDirectMapped = false;
  } else if (!physicalPagesImage1->MapTexture()) {
    isDirectMapped = false;
  }

  const bool diffuseOnly =
      pool == PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY ||
      pool == PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2;
  if (!diffuseOnly) {
    options.format = pageImageCompressed ? FMT_DXT1 : FMT_RGBA8;
    std::snprintf(imageName, sizeof(imageName), "_physical%sPages0", name);
    physicalPagesImage0 = globalImages->ScratchImage(imageName, options);
    if (physicalPagesImage0->allocationFailed) {
      allocated = false;
      isDirectMapped = false;
    } else if (!physicalPagesImage0->MapTexture()) {
      isDirectMapped = false;
    }

    options.format = pageImageCompressed ? FMT_DXT5 : FMT_RGBA8;
    std::snprintf(imageName, sizeof(imageName), "_physical%sPages2", name);
    physicalPagesImage2 = globalImages->ScratchImage(imageName, options);
    if (physicalPagesImage2->allocationFailed) {
      allocated = false;
      isDirectMapped = false;
    } else if (!physicalPagesImage2->MapTexture()) {
      isDirectMapped = false;
    }
  }

  if (pool == PHYSICAL_PAGES_POOL_PRT) {
    auto* defaultPage = static_cast<std::uint8_t*>(
        AllocVirtualTexture(196608, false, ALIGN_16));
    DefaultUncompressedPageData(defaultPage);
    const int mipLevel = FloorLog2(PHYSICAL_TEXELS_WIDE) - 7;
    if (physicalPagesImage0 != nullptr) {
      physicalPagesImage0->SubImageUpload(mipLevel, 0, 0, 0, 128, 128,
                                          defaultPage + 0x20000, 0);
    }
    if (physicalPagesImage1 != nullptr) {
      physicalPagesImage1->SubImageUpload(mipLevel, 0, 0, 0, 128, 128,
                                          defaultPage, 0);
    }
    if (physicalPagesImage2 != nullptr) {
      physicalPagesImage2->SubImageUpload(mipLevel, 0, 0, 0, 128, 128,
                                          defaultPage + 0x10000, 0);
    }
    FreeVirtualTexture(defaultPage, ALIGN_16);
  } else {
    idImageOpts mappingOptions{};
    mappingOptions.textureType = TT_2D;
    mappingOptions.format = FMT_RGBA32F;
    mappingOptions.colorFormat = CFM_DEFAULT;
    mappingOptions.width = PHYSICAL_PAGES_WIDE;
    mappingOptions.height = PHYSICAL_PAGES_HIGH;
    mappingOptions.numLevels = 1;
    mappingOptions.packedTail = true;
    mappingOptions.linear = true;
    mappingOptions.sampler.filter = TF_NEAREST;
    mappingOptions.sampler.repeat = TR_CLAMP;
    mappingOptions.sampler.lodMaxClamp = 13.0f;
    std::snprintf(imageName, sizeof(imageName), "_physical%sMappings", name);
    physicalMappingsImage =
        globalImages->ScratchImage(imageName, mappingOptions);
    if (physicalMappingsImage->allocationFailed) {
      allocated = false;
      isDirectMapped = false;
    } else if (!physicalMappingsImage->MapTexture()) {
      isDirectMapped = false;
    }

    const int mappingCount = PHYSICAL_PAGES_WIDE * PHYSICAL_PAGES_HIGH;
    auto* mappings = static_cast<float*>(AllocVirtualTexture(
        sizeof(float) * 4 * static_cast<std::size_t>(mappingCount), false,
        ALIGN_16));
    for (int index = 0; index < mappingCount; ++index) {
      mappings[4 * index + 0] = 0.0f;
      mappings[4 * index + 1] = 8.0f;
      mappings[4 * index + 2] = 0.0f;
      mappings[4 * index + 3] = 0.0f;
    }
    physicalMappingsImage->SubImageUpload(
        0, 0, 0, 0, PHYSICAL_PAGES_WIDE, PHYSICAL_PAGES_WIDE, mappings, 0);
    FreeVirtualTexture(physicalMappingsImageShadow, ALIGN_16);
    const int mappingBytes = 16 * mappingCount;
    physicalMappingsImageShadow =
        AllocVirtualTexture(static_cast<std::size_t>(mappingBytes), true,
                            ALIGN_16);
    physicalMappingsImagePBO.FreeBufferObject();
    physicalMappingsImagePBO.AllocBufferObject(physicalMappingsImageShadow,
                                                mappingBytes);

    if (vt_touchPhysCacheOnAlloc.valueInteger != 0) {
      auto* defaultPage = static_cast<std::uint8_t*>(
          AllocVirtualTexture(196608, false, ALIGN_16));
      DefaultUncompressedPageData(defaultPage);
      if (physicalPagesImage0 != nullptr) {
        physicalPagesImage0->SubImageUpload(0, 0, 0, 0, 128, 128,
                                            defaultPage + 0x20000, 0);
      }
      if (physicalPagesImage1 != nullptr) {
        physicalPagesImage1->SubImageUpload(0, 0, 0, 0, 128, 128,
                                            defaultPage, 0);
      }
      if (physicalPagesImage2 != nullptr) {
        physicalPagesImage2->SubImageUpload(0, 0, 0, 0, 128, 128,
                                            defaultPage + 0x10000, 0);
      }
      FreeVirtualTexture(defaultPage, ALIGN_16);
    }
    FreeVirtualTexture(mappings, ALIGN_16);
  }

  rpPhysicalFilterParms = physicalFilterParms;
  idMem::Singleton()->PopHeap();
  return allocated;
}

void idPhysicalPages::FreePage(idVirtualTexture* virtualTexture,
                               texturePage_t* page,
                               pageImagesUpdateParms_t* deferredUnmap) {
  while (unlockedPageList.sortDone == 0) {
    Sys_Yield();
  }
  if (page->locked) {
    page->locked = false;
    unlockedPageList.physicalPageNums[++unlockedPageList.lastPageIndex &
                                      unlockedPageList.indexMask] =
        page->physicalPageNum;
    --numLockedPages;
  }

  if (page->coarserPageNum != -1) {
    texturePage_t* coarser = &pages[page->coarserPageNum];
    if (page->mapped) {
      if (deferredUnmap != nullptr) {
        CreatePageImagesUpdate(
            virtualTexture, coarser, page, nullptr, nullptr,
            &deferredUnmap->updates[deferredUnmap->numUpdates++]);
      } else {
        UpdatePageImages(virtualTexture, coarser, page, nullptr);
      }
    }
    coarser->finerPageNum[page->quadrant] = -1;
  }

  pageHash.RemovePage(page);
  unlockedPageList.Remove(page->physicalPageNum);
  freePageList.physicalPageNums[++freePageList.lastPageIndex &
                                freePageList.indexMask] =
      page->physicalPageNum;

  page->mapped = false;
  page->locked = false;
  page->dirty = false;
  page->maxLevel = 0;
  page->pageLevel = 0;
  page->pageX = 0;
  page->pageY = 0;
  page->pageSource = PAGESOURCE_INVALID;
  page->pageCompression = COMP_NONE;
  page->quadrant = QUADRANT_LOWER_LEFT;
  page->coarserPageNum = -1;
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    page->finerDiskOffset[quadrant] = 0;
    page->finerDiskLength[quadrant] = 0;
    page->finerPageNum[quadrant] = -1;
  }
  if (virtualTexture->rootTexturePage == page) {
    virtualTexture->rootTexturePage = nullptr;
  }
  --virtualTexture->levelResidency[page->pageLevel];
}

void idPhysicalPages::FreePageTree_r(
    idVirtualTexture* virtualTexture, texturePage_t* page,
    pageImagesUpdateParms_t* deferredUnmap) {
  if (page == nullptr) {
    return;
  }
  for (int quadrant = 0; quadrant < 4; ++quadrant) {
    if (page->finerPageNum[quadrant] != -1) {
      FreePageTree_r(virtualTexture, &pages[page->finerPageNum[quadrant]],
                     deferredUnmap);
    }
  }
  FreePage(virtualTexture, page, deferredUnmap);
}

texturePage_t* idPhysicalPages::AllocPage(
    idVirtualTexture* virtualTexture, texturePage_t* coarser,
    pageQuadrant_t quadrant, pageImagesUpdateParms_t* deferredUnmap) {
  while (unlockedPageList.sortDone == 0) {
    Sys_Yield();
  }
  if (freePageList.lastPageIndex - freePageList.firstPageIndex == -1) {
    for (int listIndex = unlockedPageList.firstPageIndex;
         listIndex <= unlockedPageList.lastPageIndex; ++listIndex) {
      texturePage_t* candidate =
          &pages[static_cast<int>(unlockedPageList.physicalPageNums[
              listIndex & unlockedPageList.indexMask])];
      if (candidate == coarser) {
        continue;
      }
      bool isLeaf = true;
      for (std::int16_t child : candidate->finerPageNum) {
        isLeaf &= child == -1;
      }
      if (isLeaf) {
        FreePage(&virtualTextureSystem.vts[candidate->pageSource], candidate,
                 deferredUnmap);
        break;
      }
    }
  }

  const int pageNum = static_cast<int>(freePageList.physicalPageNums[
      freePageList.firstPageIndex++ & freePageList.indexMask]);
  unlockedPageList.physicalPageNums[++unlockedPageList.lastPageIndex &
                                    unlockedPageList.indexMask] = pageNum;
  texturePage_t* page = &pages[pageNum];
  page->mapped = false;
  page->locked = false;
  page->dirty = false;
  page->maxLevel = static_cast<std::uint8_t>(virtualTexture->header.numLevels - 1);
  page->pageLevel = static_cast<std::uint8_t>(
      (coarser != nullptr ? coarser->pageLevel
                          : virtualTexture->header.numLevels) -
      1);
  page->pageX = coarser != nullptr
                    ? static_cast<std::uint16_t>(2 * coarser->pageX +
                                                 (quadrant & 1))
                    : 0;
  page->pageY = coarser != nullptr
                    ? static_cast<std::uint16_t>(2 * coarser->pageY +
                                                 ((quadrant >> 1) & 1))
                    : 0;
  page->pageSource = virtualTexture->pageSource;
  page->pageCompression = virtualTexture->GetNewDiskPageCompression();
  page->quadrant = quadrant;
  page->coarserPageNum =
      coarser != nullptr ? coarser->physicalPageNum : -1;
  for (int child = 0; child < 4; ++child) {
    page->finerDiskOffset[child] = 0;
    page->finerDiskLength[child] = 0;
    page->finerPageNum[child] = -1;
  }
  pagePriority[pageNum] = 0;
  pageFrameNum[pageNum] = 0;
  pageFade[pageNum] = static_cast<std::uint8_t>(vt_pageFadeInSteps.valueInteger);
  if (coarser != nullptr) {
    coarser->finerPageNum[quadrant] = page->physicalPageNum;
  } else {
    virtualTexture->rootTexturePage = page;
  }
  ++virtualTexture->levelResidency[page->pageLevel];

  const std::uint32_t bucket =
      HashPhysicalPage(PhysicalPageHashKey(*page), pageHash.hashMask);
  idPhysicalPageHash::pageHashEntry_t& entry =
      pageHash.pageHashIndex[pageNum];
  entry.pageID = PhysicalPageID(*page);
  entry.mapped = 0;
  entry.next = static_cast<std::uint16_t>(pageHash.pageHashTable[bucket]);
  pageHash.pageHashTable[bucket] = static_cast<std::int16_t>(pageNum);
  return page;
}

void idPhysicalPages::EmptyCache() {
  if (physicalPagesImage1 == nullptr) {
    return;
  }
  if (physicalPagesPool != PHYSICAL_PAGES_POOL_PRT) {
    auto* clearData = static_cast<std::uint8_t*>(
        AllocVirtualTexture(0x10000, false, ALIGN_16));
    if (pageImageCompressed) {
      auto* pattern = static_cast<std::uint8_t*>(
          AllocVirtualTexture(0x10000, false, ALIGN_16));
      for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
          std::uint8_t* pixel = &pattern[4 * (y * 128 + x)];
          pixel[0] = static_cast<std::uint8_t>(x);
          pixel[1] = static_cast<std::uint8_t>(y);
          pixel[2] = 0;
          pixel[3] = 255;
        }
      }
      idDxtEncoder encoder{};
      encoder.CompressImageDXT1Fast_SSE2(pattern, clearData, 128, 128);
      FreeVirtualTexture(pattern, ALIGN_16);
    } else {
      for (int y = 0; y < 128; ++y) {
        for (int x = 0; x < 128; ++x) {
          std::uint8_t* pixel = &clearData[4 * (y * 128 + x)];
          pixel[0] = static_cast<std::uint8_t>(x);
          pixel[1] = static_cast<std::uint8_t>(y);
          pixel[2] = 0;
          pixel[3] = 255;
        }
      }
    }
    for (int y = 0; y < PHYSICAL_PAGES_HIGH; ++y) {
      for (int x = 0; x < PHYSICAL_PAGES_WIDE; ++x) {
        physicalPagesImage1->SubImageUpload(0, x << 7, y << 7, 0, 128, 128,
                                            clearData, 0);
      }
    }
    const bool diffuseOnly =
        physicalPagesPool == PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY ||
        physicalPagesPool == PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2;
    if (!diffuseOnly) {
      for (int y = 0; y < PHYSICAL_PAGES_HIGH; ++y) {
        for (int x = 0; x < PHYSICAL_PAGES_WIDE; ++x) {
          physicalPagesImage0->SubImageUpload(
              0, x << 7, y << 7, 0, 128, 128, clearData, 0);
          physicalPagesImage2->SubImageUpload(
              0, x << 7, y << 7, 0, 128, 128, clearData, 0);
        }
      }
    }
    FreeVirtualTexture(clearData, ALIGN_16);
  }

  if (physicalMappingsImage != nullptr &&
      physicalMappingsImageShadow != nullptr) {
    auto* mappings = static_cast<float*>(physicalMappingsImageShadow);
    const int count = PHYSICAL_PAGES_WIDE * PHYSICAL_PAGES_HIGH;
    for (int index = 0; index < count; ++index) {
      mappings[4 * index + 0] = 0.0f;
      mappings[4 * index + 1] = 8.0f;
      mappings[4 * index + 2] = 0.0f;
      mappings[4 * index + 3] = 0.0f;
    }
    physicalMappingsImage->SubImageUpload(
        0, 0, 0, 0, PHYSICAL_PAGES_WIDE, PHYSICAL_PAGES_HIGH,
        physicalMappingsImageShadow, 0);
  }
}

transcodeJobList_t::transcodeJobList_t() = default;
transcodeJobList_t::~transcodeJobList_t() = default;

idVirtualTextureSystem::idVirtualTextureSystem()
    : vmtrTotalPagesMetric(idStr("vmtrTotalPages"),
                           idStr("total vmtr pages in view")),
      vmtrBiasMetric(idStr("vmtrBias"), idStr("vmtr dynamic LOD bias")),
      specTotalPagesMetric(
          idStr("specTotalPages"),
          idStr("total diffuse+specular pages in view")),
      specBiasMetric(idStr("specBias"),
                     idStr("diffuse+specular LOD bias")),
      diffuseTotalPagesMetric(idStr("diffuseTotalPages"),
                              idStr("total diffuse only pages in view")),
      diffuseBiasMetric(idStr("diffuseBias"),
                        idStr("diffuse only LOD bias")) {
  std::memset(containerPath, 0, sizeof(containerPath));
  std::memset(referencedVTPath, 0, sizeof(referencedVTPath));
  std::memset(referencedVTName, 0, sizeof(referencedVTName));
  for (int source = 0; source < PAGESOURCE_TOTAL; ++source) {
    vtPtrs[source] = &vts[source];
    vts[source].pageSource = static_cast<pageSource_t>(source);
  }
}

idVirtualTextureSystem::~idVirtualTextureSystem() = default;

void idVirtualTextureSystem::Init() {
  cancelToTerminate = false;
  vtInfo = static_cast<virtualTextureInfo_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\VirtualTextureSystem.cpp(2942) : "
          "TAG_VIRTUALTEXTURE",
          256, TAG_VIRTUALTEXTURE, true, ALIGN_128, HEAP_DEFAULTHEAP));
  currentPageIndices = 0;
  pageIndices[0] = static_cast<pageIndices_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\VirtualTextureSystem.cpp(2945) : "
          "TAG_VIRTUALTEXTURE",
          65600, TAG_VIRTUALTEXTURE, true, ALIGN_128, HEAP_DEFAULTHEAP));
  pageIndices[1] = static_cast<pageIndices_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\VirtualTextureSystem.cpp(2946) : "
          "TAG_VIRTUALTEXTURE",
          65600, TAG_VIRTUALTEXTURE, true, ALIGN_128, HEAP_DEFAULTHEAP));
  explicitPages[0] = static_cast<std::uint8_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\VirtualTextureSystem.cpp(2948) : "
          "TAG_VIRTUALTEXTURE",
          0x2000, TAG_VIRTUALTEXTURE, true, ALIGN_128, HEAP_DEFAULTHEAP));
  explicitPages[1] = static_cast<std::uint8_t*>(
      idMem::Singleton()->AllocWithLocation(
          "w:\\build\\tech5\\engine\\renderer\\VirtualTextureSystem.cpp(2949) : "
          "TAG_VIRTUALTEXTURE",
          0x2000, TAG_VIRTUALTEXTURE, true, ALIGN_128, HEAP_DEFAULTHEAP));

  void* destinationMemory = AllocVirtualTexture(
      sizeof(idRenderDestination), false, ALIGN_16);
  renderDestFeedback[0] =
      destinationMemory != nullptr
          ? new (destinationMemory) idRenderDestination()
          : nullptr;
  renderDestFeedback[0]->Create("_feedbackBuffer", 160, 120, TC_RGBA);
  renderDestFeedback[1] = renderDestFeedback[0];
  renderDestFeedback[2] = renderDestFeedback[0];
  feedbackBuffer.Alloc(0x12C00);
  feedbackBufferPitch = 640;
  if (glConfig.pixelBufferObjectAvailable) {
    idHeapArray<unsigned char> clear;
    clear.Alloc(0x12C00);
    std::memset(clear.buffer, 0, clear.num);
    void* pboMemory = AllocVirtualTexture(sizeof(idPixelPackBuffer), false,
                                          ALIGN_16);
    feedbackBufferObject =
        pboMemory != nullptr ? new (pboMemory) idPixelPackBuffer() : nullptr;
    feedbackBufferObject->AllocBufferObject(clear.buffer, clear.num);
  }

  analyzeJobList = parallelJobManager->AllocJobList(
      JOBLIST_ANALYZE, JOBLIST_PRIORITY_MEDIUM, 256, 2,
      &idColor::colorLtGrey);
  const jobListId_t transcodeJobIds[2] = {
      JOBLIST_VIRTUAL_TEXTURE1, JOBLIST_VIRTUAL_TEXTURE2};
  for (int index = 0; index < 2; ++index) {
    transcodeJobList_t& jobs = transcodeJobList[index];
    jobs.parallelJobList = parallelJobManager->AllocJobList(
        transcodeJobIds[index], JOBLIST_PRIORITY_LOW, 258, 2,
        &idColor::colorLtGrey);
    jobs.numTranscodeJobs = 0;
    jobs.transcodeParms = static_cast<transcodeParms_t*>(
        idMem::Singleton()->AllocWithLocation(
            "w:\\build\\tech5\\engine\\renderer\\VirtualTextureSystem.cpp(3093) : "
            "TAG_VIRTUALTEXTURE",
            38912, TAG_VIRTUALTEXTURE, true, ALIGN_128,
            HEAP_DEFAULTHEAP));
    jobs.transcodeSourceBuffer = static_cast<std::uint8_t*>(
        idMem::Singleton()->AllocWithLocation(
            "w:\\build\\tech5\\engine\\renderer\\VirtualTextureSystem.cpp(3094) : "
            "TAG_VIRTUALTEXTURE",
            0x100000, TAG_VIRTUALTEXTURE, false, ALIGN_128,
            HEAP_DEFAULTHEAP));
    jobs.transcodePBO.AllocBufferObject(nullptr, 0xA00000);

    pageImagesUpdateParms_t& unmap = UnmapParms(jobs);
    pageImagesUpdateParms_t& update = ImageUpdateParms(jobs);
    unmap.updates = static_cast<pageImagesUpdate_t*>(
        idMem::Singleton()->AllocWithLocation(
            "w:\\build\\tech5\\engine\\renderer\\VirtualTextureSystem.cpp(2170) : "
            "TAG_VIRTUALTEXTURE",
            108672, TAG_VIRTUALTEXTURE, true, ALIGN_128,
            HEAP_DEFAULTHEAP));
    unmap.numUpdates = 0;
    unmap.numCancelled = 0;
    unmap.submittedJob = false;
    unmap.usePageTablePBOs = vt_usePageTablePBOs.valueInteger != 0;
    unmap.useUnsynchronizedPageTablePBOs =
        vt_useUnsynchronizedPageTablePBOs.valueInteger != 0;
    unmap.useInvalidateRangeBitForPageTablePBOMap =
        vt_useInvalidateRangeBitForMappingAndPageTablePBOMap.valueInteger !=
        0;
    unmap.skipPhysicalTextureUploads =
        vt_skipPhysicalTextureUploads.valueInteger != 0;
    update.updates = static_cast<pageImagesUpdate_t*>(
        idMem::Singleton()->AllocWithLocation(
            "w:\\build\\tech5\\engine\\renderer\\VirtualTextureSystem.cpp(2170) : "
            "TAG_VIRTUALTEXTURE",
            108672, TAG_VIRTUALTEXTURE, true, ALIGN_128,
            HEAP_DEFAULTHEAP));
    update.numUpdates = 0;
    update.numCancelled = 0;
    update.submittedJob = false;
    update.usePageTablePBOs = unmap.usePageTablePBOs;
    update.useUnsynchronizedPageTablePBOs =
        unmap.useUnsynchronizedPageTablePBOs;
    update.useInvalidateRangeBitForPageTablePBOMap =
        unmap.useInvalidateRangeBitForPageTablePBOMap;
    update.skipPhysicalTextureUploads =
        unmap.skipPhysicalTextureUploads;
    jobs.nextFramePageImageUpload = false;
  }

  idStr vmtrBasePath;
  idStr vtFilePathInstall;
  if (idLib::production == PROD_DEVELOPMENT ||
      com_production.valueInteger != 0 ||
      vt_useNetworkPagesWithPreload.valueInteger != 0) {
    const idCVar& selectedPath =
        vt_filePathVmtrOverride.valueString.Length() > 0
            ? vt_filePathVmtrOverride
            : vt_filePath;
    vmtrBasePath = selectedPath.valueString.data;
    vtFilePathInstall = vmtrBasePath;
  } else {
    vtFilePathInstall = fileSystem->GetBasePathStr(FSPATH_INSTALL);
    vtFilePathInstall.AppendPath("virtualtextures");
    vt_filePathVmtrOverride.SetString(vmtrBasePath.data, true);
    idStr testPath(fileSystem->GetBasePathStr(FSPATH_BASE));
    testPath.AppendPath("virtualtextures");
    vt_filePath.SetString(testPath.data, true);
    vmtrBasePath = testPath;
  }

  streamFileCache->Init();
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    vtPtrs[source]->EmptyCache();
  }
  for (idPhysicalPages& pages : physicalPagesPool) {
    pages.EmptyCache();
  }
  emptyCacheTime = Sys_Milliseconds();
  if ((idLib::production != PROD_DEVELOPMENT ||
       com_production.valueInteger != 0) &&
      vt_useNetworkPagesWithPreload.valueInteger == 0) {
    vt_virtualSizeLog2.SetInteger(9, true);
    vt_uncompressedVmtr.SetBool(false, true);
    vt_packPageFiles.SetInteger(2, true);
  }

  const int virtualSizeLog2 =
      std::clamp(vt_virtualSizeLog2.valueInteger, 7, 11);
  const int virtualPagesWide = 1 << virtualSizeLog2;
  const char* specialVersion =
      feature_lowgore.valueInteger != 0 ? "_lowgore" : "";
  idStr vmtrName;
  if (vt_packPageFiles.valueInteger != 0) {
    vmtrName = va("_vmtr%s", specialVersion);
  } else {
    vmtrName = va("_vmtr%s_v%02i_%i_%c", specialVersion,
                  idVMTRData::VMTR_FILE_VERSION, virtualPagesWide / 8,
                  vt_uncompressedVmtr.valueInteger != 0 ? 'U' : 'C');
  }
  idStr vmtrPath(va("%s/%s.vmtr", vmtrBasePath.data, vmtrName.data));
  idStr pageFilePath(vmtrPath);
  pageFilePath.SetFileExtension(".pages");
  if (!vts[1].SetSource(vmtrName.data, pageFilePath.data,
                        PHYSICAL_PAGES_POOL_VMTR, ID_VTF_VMTR)) {
    if (r_skipVmtrs.valueInteger == 0) {
      idLib::FatalError("Couldn't open vmtr page file '%s'",
                        pageFilePath.data);
    }
    vts[1].SetEmpty(pageFilePath.data, virtualPagesWide,
                    vt_uncompressedVmtr.valueInteger != 0 ? 128 : 64);
  }
  AddToReferencedVirtualTextures(vmtrName.data, pageFilePath.data);
  if (idLib::production != PROD_PRODUCTION &&
      r_skipVmtrs.valueInteger == 0) {
    vts[1].vmtrData->ReadVmtrFile();
  }

  idCVar* debugCVars[] = {
      &vt_upsampleFilter,       &vt_upsampleSharpen,
      &vt_upsampleNoise,        &vt_stripPageBorders,
      &vt_showPageSources,      &vt_showPageMips,
      &vt_showPageColors,       &vt_showPageNumbers,
      &vt_showPageInfo,         &vt_showPageBorders,
      &vt_showPageCheckered,    &vt_showPageSizes,
      &vt_showPageUsage};
  for (idCVar* cvar : debugCVars) {
    cvar->flags &= ~0x20000u;
  }
  const bool developmentPhysicalPools =
      idLib::production != PROD_PRODUCTION;
  CreatePhysicalImages(developmentPhysicalPools,
                       developmentPhysicalPools,
                       developmentPhysicalPools, true);
  if (vts[1].header.diskOffsetScale == 2) {
    idVirtualTexture* vmtrTexture = &vts[1];
    LockPhysicalPages(&vmtrTexture, 1, 21);
  }

  idCmdArgs args;
  VirtualTextureInfo_f(args);
  struct commandRegistration_t {
    const char* name;
    commandFunction_t function;
    const char* description;
  };
  const commandRegistration_t commands[] = {
      {"vt_info", reinterpret_cast<commandFunction_t>(VirtualTextureInfo_f),
       "Reports information on the current virtual texture system"},
      {"vt_emptyCache",
       reinterpret_cast<commandFunction_t>(EmptyCache_f),
       "empties and zeros the physical images"},
      {"vt_lockPages", reinterpret_cast<commandFunction_t>(LockPages_f),
       "locks physical pages"},
      {"vt_restart", reinterpret_cast<commandFunction_t>(Restart_f),
       "reallocates the physical images for changes in size or compression"},
      {"vt_reload", reinterpret_cast<commandFunction_t>(Reload_f),
       "reload all the currently loaded virtual textures"},
      {"vt_validateCache",
       reinterpret_cast<commandFunction_t>(ValidateCache_f),
       "consistency checks the page buffers"},
      {"vt_setSource", reinterpret_cast<commandFunction_t>(SetSource_f),
       "Explicitly change the page file for a virtual material to compare"},
      {"vt_benchmark", reinterpret_cast<commandFunction_t>(Benchmark_f),
       "Benchmark the virtual texture transcoding"},
      {"vt_transcodeBenchmark",
       reinterpret_cast<commandFunction_t>(TranscodeBenchmark_f),
       "Benchmark the virtual texture transcoding"},
      {"printVirtualMaterial",
       reinterpret_cast<commandFunction_t>(PrintVirtualMaterial_f),
       "prints data on a virtual material"},
      {"listVirtualTextures",
       reinterpret_cast<commandFunction_t>(ListVirtualTextures_f),
       "lists all the referenced virtual texture pagefile pathnames"}};
  for (const commandRegistration_t& command : commands) {
    cmdSystem->AddCommand(command.name, command.function,
                          command.description, nullptr);
  }
}

void idVirtualTextureSystem::FreeLockedPages() {
  for (int source = 2; source < PAGESOURCE_TOTAL; ++source) {
    idVirtualTexture* virtualTexture = vtPtrs[source];
    if (virtualTexture->flavor != ID_VTF_VMTR || virtualTexture->isMapVmtr) {
      virtualTexture->EmptyCache();
    }
  }
}

void idVirtualTextureSystem::SetupLoadedVirtualMaterials() {
  idList<idResource*, 99> loadedMaterials;
  idMaterial::resourceList.GetLoadedResources(loadedMaterials);
  for (int index = 0; index < loadedMaterials.num; ++index) {
    static_cast<idMaterial*>(loadedMaterials.list[index])->SetupVirtualTexture();
  }
}

bool idVirtualTextureSystem::CreatePhysicalImages(
    bool hasSpecularPages, bool hasDiffuseOnlyPages,
    bool hasDiffuseOnly2Pages, bool vmtrPhysicalImages) {
  mapHasSpecularPages = hasSpecularPages;
  mapHasDiffuseOnlyPages = hasDiffuseOnlyPages;
  mapHasDiffuseOnly2Pages = hasDiffuseOnly2Pages;

  int vmtrWidth = 0;
  int vmtrHeight = 0;
  int uniqueWidth = 0;
  int uniqueHeight = 0;
  int diffuseWidth = 0;
  int diffuseHeight = 0;
  int diffuse2Width = 0;
  int diffuse2Height = 0;
  ParsePhysicalImageSize(vt_pageImageSizeVmtr, true, vmtrWidth, vmtrHeight);
  ParsePhysicalImageSize(vt_pageImageSizeUnique, mapHasSpecularPages,
                         uniqueWidth, uniqueHeight);
  ParsePhysicalImageSize(vt_pageImageSizeUniqueDiffuseOnly,
                         mapHasDiffuseOnlyPages, diffuseWidth, diffuseHeight);
  ParsePhysicalImageSize(vt_pageImageSizeUniqueDiffuseOnly2,
                         mapHasDiffuseOnly2Pages, diffuse2Width,
                         diffuse2Height);

  bool failed = false;
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (attempt != 0) {
      if (mapHasSpecularPages) {
        uniqueWidth = uniqueHeight = 4096;
      }
      if (mapHasDiffuseOnlyPages) {
        diffuseWidth = diffuseHeight = 4096;
      }
      if (mapHasDiffuseOnly2Pages) {
        diffuse2Width = diffuse2Height = 4096;
      }
    }

    PurgePhysicalImages(physicalPagesPool[1]);
    PurgePhysicalImages(physicalPagesPool[2]);
    PurgePhysicalImages(physicalPagesPool[3]);
    failed = false;
    if (vmtrPhysicalImages) {
      PurgePhysicalImages(physicalPagesPool[0]);
      failed |= !physicalPagesPool[0].CreatePhysicalImages(
          PHYSICAL_PAGES_POOL_VMTR, "Vmtr", vmtrWidth, vmtrHeight,
          &rpPhysicalVmtrFilterParms);
    }
    failed |= !physicalPagesPool[1].CreatePhysicalImages(
        PHYSICAL_PAGES_POOL_UNIQUE, "Unique", uniqueWidth, uniqueHeight,
        &rpPhysicalUniqueFilterParms);
    failed |= !physicalPagesPool[2].CreatePhysicalImages(
        PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY, "UniqueDiffuseOnly",
        diffuseWidth, diffuseHeight,
        &rpPhysicalUniqueDiffuseOnlyFilterParms);
    failed |= !physicalPagesPool[3].CreatePhysicalImages(
        PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2, "UniqueDiffuseOnly2",
        diffuse2Width, diffuse2Height,
        &rpPhysicalUniqueDiffuseOnly2FilterParms);
    if (!failed) {
      break;
    }
  }
  feedbackFrameNum += 1024;
  return !failed;
}

void idVirtualTextureSystem::EmptyCache(bool skipVmtrCache) {
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    idVirtualTexture* virtualTexture = vtPtrs[source];
    if (!skipVmtrCache || virtualTexture->flavor != ID_VTF_VMTR ||
        virtualTexture->isMapVmtr) {
      virtualTexture->EmptyCache();
    }
  }
  for (int pool = skipVmtrCache ? 1 : 0;
       pool < PHYSICAL_PAGES_POOL_SHARED_COUNT; ++pool) {
    physicalPagesPool[pool].EmptyCache();
  }
  emptyCacheTime = static_cast<int>(Sys_Milliseconds());
}

void idVirtualTextureSystem::Shutdown() {
  virtualTexturePreloader.EndPreloading();
  if (analyzeJobList != nullptr) {
    parallelJobManager->FreeJobList(analyzeJobList);
    analyzeJobList = nullptr;
  }
  for (transcodeJobList_t& jobs : transcodeJobList) {
    if (jobs.parallelJobList != nullptr) {
      parallelJobManager->FreeJobList(jobs.parallelJobList);
      jobs.parallelJobList = nullptr;
    }
    FreeVirtualTexture(jobs.transcodeParms, ALIGN_16);
    jobs.transcodeParms = nullptr;
    FreeVirtualTexture(jobs.transcodeSourceBuffer, ALIGN_16);
    jobs.transcodeSourceBuffer = nullptr;
    pageImagesUpdateParms_t& unmap = UnmapParms(jobs);
    pageImagesUpdateParms_t& update = ImageUpdateParms(jobs);
    FreeVirtualTexture(unmap.updates, ALIGN_16);
    unmap.updates = nullptr;
    FreeVirtualTexture(update.updates, ALIGN_16);
    update.updates = nullptr;
  }

  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    idVirtualTexture* virtualTexture = vtPtrs[source];
    if (virtualTexture != nullptr && virtualTexture->flavor == ID_VTF_VMTR) {
      virtualTexture->FreeData();
    }
  }
  for (int source = 2; source < PAGESOURCE_TOTAL; ++source) {
    idVirtualTexture* virtualTexture = vtPtrs[source];
    if (virtualTexture->flavor != ID_VTF_VMTR || virtualTexture->isMapVmtr) {
      virtualTexture->FreeData();
    }
  }
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    vtPtrs[source]->EmptyCache();
  }
  for (idPhysicalPages& physicalPages : physicalPagesPool) {
    physicalPages.EmptyCache();
  }
  emptyCacheTime = static_cast<int>(Sys_Milliseconds());

  for (int index = 0; index < 3; ++index) {
    idRenderDestination* destination = renderDestFeedback[index];
    bool alreadyFreed = false;
    for (int earlier = 0; earlier < index; ++earlier) {
      alreadyFreed |= destination == renderDestFeedback[earlier];
    }
    if (destination != nullptr && !alreadyFreed) {
      destination->~idRenderDestination();
      FreeVirtualTexture(destination, ALIGN_16);
    }
  }
  std::fill(std::begin(renderDestFeedback), std::end(renderDestFeedback),
            nullptr);
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    vts[source].FreeData();
  }
  for (idPhysicalPages& physicalPages : physicalPagesPool) {
    physicalPages.Clear();
  }
}

bool idVirtualTextureSystem::Restart() {
  SyncFeedback();
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    vtPtrs[source]->EmptyCache();
  }
  for (idPhysicalPages& physicalPages : physicalPagesPool) {
    physicalPages.EmptyCache();
  }
  emptyCacheTime = static_cast<int>(Sys_Milliseconds());
  return CreatePhysicalImages(mapHasSpecularPages, mapHasDiffuseOnlyPages,
                              mapHasDiffuseOnly2Pages, true);
}

void idVirtualTextureSystem::VisualizeFeedbackBuffer(
    char* data, int width, int height, int bytePitch) {
  if (data == nullptr) {
    return;
  }
  const std::size_t outputBytes =
      static_cast<std::size_t>(4 * width * height);
  auto* output = static_cast<std::uint8_t*>(
      AllocVirtualTexture(outputBytes, false, ALIGN_16));
  for (int y = 0; y < height; ++y) {
    const std::uint8_t* source =
        reinterpret_cast<const std::uint8_t*>(data) + bytePitch * y;
    for (int x = 0; x < width; ++x, source += 4) {
      const int outputOffset = 4 * (y * width + x);
      unsigned int pageX = source[0] + ((source[2] & 0x0F) << 8);
      unsigned int pageY = source[1] + ((source[2] & 0xF0) << 4);
      const int pageLevel = source[3] & 0x0F;
      const int pageSource = source[3] >> 4;
      if (vt_showFeedbackBuffer.valueInteger == 4) {
        output[outputOffset + 0] = (pageLevel & 1) != 0 ? 255 : 0;
        output[outputOffset + 1] = (pageLevel & 2) != 0 ? 255 : 0;
        output[outputOffset + 2] = (pageLevel & 4) != 0 ? 255 : 0;
      } else if (vt_showFeedbackBuffer.valueInteger == 3) {
        output[outputOffset + 0] = (pageSource & 1) != 0 ? 255 : 0;
        output[outputOffset + 1] = (pageSource & 2) != 0 ? 255 : 0;
        output[outputOffset + 2] = (pageSource & 4) != 0 ? 255 : 0;
      } else {
        if (vt_showFeedbackBuffer.valueInteger == 2) {
          pageX >>= pageLevel;
          pageY >>= pageLevel;
        }
        output[outputOffset + 0] = static_cast<std::uint8_t>(32 * pageX);
        output[outputOffset + 1] = static_cast<std::uint8_t>(32 * pageY);
        output[outputOffset + 2] = 0;
      }
      output[outputOffset + 3] = 255;
    }
  }

  if (feedbackVisualize == nullptr) {
    idImageOpts options{};
    options.textureType = TT_2D;
    options.format = FMT_RGBA8;
    options.width = width;
    options.height = height;
    options.numLevels = 1;
    options.packedTail = true;
    options.linear = true;
    options.sampler.filter = TF_NEAREST;
    options.sampler.lodMaxClamp = 13.0f;
    feedbackVisualize = globalImages->ScratchImage("_pageVisualize", options);
  }
  feedbackVisualize->SubImageUpload(0, 0, 0, 0, width, height, output, 0);
  FreeVirtualTexture(output, ALIGN_16);
}

void idVirtualTextureSystem::ShowFeedbackStreaming(
    char* data, int width, int height, int bytePitch) {
  if (data == nullptr) {
    return;
  }
  auto* output = static_cast<std::uint8_t*>(
      AllocVirtualTexture(76800, false, ALIGN_16));
  float squaredError = 0.0f;
  for (int y = 0; y < height; ++y) {
    const std::uint8_t* source =
        reinterpret_cast<const std::uint8_t*>(data) + bytePitch * y;
    for (int x = 0; x < width; ++x, source += 4) {
      std::uint8_t* color = output + 4 * (y * width + x);
      const int requestedLevel = source[3] & 0x0F;
      const int pageSource = source[3] >> 4;
      if (pageSource == 0) {
        color[0] = color[1] = color[2] = 0;
        color[3] = 255;
        continue;
      }
      idVirtualTexture* virtualTexture = vtPtrs[pageSource];
      const int numLevels = virtualTexture->header.numLevels;
      const int clampedLevel = std::clamp(
          requestedLevel, vt_minLod.valueInteger, numLevels - 1);
      if ((virtualTexture->header.pagesWide >> clampedLevel) == 0) {
        color[0] = 255;
        color[1] = 0;
        color[2] = 255;
        color[3] = 255;
        continue;
      }

      const int pageX = source[0] + ((source[2] & 0x0F) << 8);
      const int pageY = source[1] + ((source[2] & 0xF0) << 4);
      unsigned int residentLevel =
          static_cast<unsigned int>(virtualTexture->header.numLevels);
      for (texturePage_t* page = virtualTexture->rootTexturePage;
           page != nullptr;) {
        if (!page->mapped) {
          break;
        }
        residentLevel = page->pageLevel;
        if (page->pageLevel == 0) {
          break;
        }
        const int quadrant =
            2 * ((pageY >> (page->pageLevel - 1)) & 1) |
            ((pageX >> (page->pageLevel - 1)) & 1);
        const std::int16_t child = page->finerPageNum[quadrant];
        if (child == -1) {
          break;
        }
        page = &virtualTexture->physicalPages->pages[child];
      }
      const unsigned int effectiveResidentLevel =
          std::max(residentLevel,
                   static_cast<unsigned int>(clampedLevel));
      const unsigned int difference =
          effectiveResidentLevel - static_cast<unsigned int>(clampedLevel);
      const unsigned int blue = 255 * difference / (numLevels - 1);
      color[0] = static_cast<std::uint8_t>(
          std::clamp(2 * static_cast<int>(blue) - 128, 0, 255));
      color[1] = static_cast<std::uint8_t>(
          std::clamp(255 - 2 * static_cast<int>(blue), 0, 255));
      color[2] = static_cast<std::uint8_t>(blue);
      color[3] = 255;
      squaredError += static_cast<float>(difference * difference);
    }
  }
  mipLevelMSE = squaredError /
                (static_cast<float>(width) * static_cast<float>(height));
  if (feedbackStreaming == nullptr) {
    idImageOpts options{};
    options.textureType = TT_2D;
    options.format = FMT_RGBA8;
    options.width = 160;
    options.height = 120;
    options.numLevels = 1;
    options.packedTail = true;
    options.linear = true;
    options.sampler.filter = TF_LINEAR;
    options.sampler.repeat = TR_CLAMP;
    feedbackStreaming =
        globalImages->ScratchImage("_feedbackStreaming", options);
  }
  feedbackStreaming->SubImageUpload(0, 0, 0, 0, 160, 120, output, 0);
  FreeVirtualTexture(output, ALIGN_16);
}

void idVirtualTextureSystem::SetSource_f(const idCmdArgs& args) {
  if (args.argc != 3) {
    idLib::Printf("usage: vt_setSource <pageSrc_t 1-63> <pageFilePath>\n");
    return;
  }
  const int source = std::atoi(args.Argv(1));
  if (source < 1 || source >= PAGESOURCE_TOTAL) {
    idLib::Printf("'%s' is not a valid pageSource\n", args.Argv(1));
    return;
  }

  idStr fileName(args.Argv(2));
  fileName.StripFileExtension();
  idStr pageFilePath;
  const char* path = fileName.data;
  if (path[0] == '/' || path[0] == '\\' || path[1] == ':' ||
      idStr::Icmpn(path, "nfs:", 4) == 0) {
    pageFilePath.Format("%s.pages", path);
  } else {
    pageFilePath.Format("%s/%s.pages", vt_filePath.valueString.data, path);
  }
  if (fileSystem->GetTimestamp(pageFilePath.data, false) ==
      std::numeric_limits<std::uint32_t>::max()) {
    idLib::Printf("'%s' does not exist!\n", pageFilePath.data);
    return;
  }

  idVirtualTexture* virtualTexture = virtualTextureSystem.vtPtrs[source];
  idStr base(virtualTexture->baseName.data);
  if (base.Length() == 0) {
    base = fileName;
  }
  const virtualTextureFlavor_t flavor = virtualTexture->flavor;
  if (!virtualTexture->SetSource(
          base.data, pageFilePath.data,
          flavor == ID_VTF_VMTR ? PHYSICAL_PAGES_POOL_VMTR
                                : PHYSICAL_PAGES_POOL_UNIQUE,
          flavor)) {
    idLib::FatalError("Error opening vmtr page file '%s'",
                      pageFilePath.data);
  }
  virtualTextureSystem.SetupLoadedVirtualMaterials();
  idLib::Printf("successfully set '%s'\n", pageFilePath.data);
}

void idVirtualTextureSystem::EmptyCache_f(const idCmdArgs&) {
  virtualTextureSystem.EmptyCache(false);
}

void idVirtualTextureSystem::Restart_f(const idCmdArgs&) {
  if (!virtualTextureSystem.Restart()) {
    idLib::Warning("failed to allocate larger physical textures");
  }
}

void idVirtualTextureSystem::ListVirtualTextures_f(const idCmdArgs&) {
  idList<idStr, 5> pageFileList;
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    const idVirtualTexture& virtualTexture = virtualTextureSystem.vts[source];
    if (virtualTexture.flavor == ID_VTF_VMTR) {
      idStr path(virtualTexture.pageFilePath.data);
      path.BackSlashesToSlashes();
      pageFileList.Append(path);
    }
  }
  for (int index = 0;
       index < virtualTextureSystem.numReferencedVirtualTextures; ++index) {
    idStr path(virtualTextureSystem.referencedVTPath[index]);
    path.BackSlashesToSlashes();
    pageFileList.Append(path);
  }
  idSort_Quick<idStr, idSort_PathStr> sorter;
  if (pageFileList.list != nullptr) {
    sorter.Sort(pageFileList.list,
                static_cast<unsigned int>(pageFileList.num));
  }
  idLib::Printf("Referenced virtual textures:\n");
  idLib::Printf("----------------------------\n");
  for (int index = 0; index < pageFileList.num; ++index) {
    idLib::Printf("%s\n", pageFileList[index].data);
  }
  idLib::Printf("----------------------------\n");
  idLib::Printf("Done\n");
}

void idVirtualTextureSystem::AnalyzeFeedback(
    void* data, int width, int height, int bytePitch,
    bool forceHighestDetail) {
  console->AddJobListTimeRange(RANGE_JBL_ANLZ, &idColor::colorLtGrey,
                               analyzeJobList, 0);
  const unsigned int completedPageIndex = currentPageIndices ^ 1u;
  if ((data != nullptr && width > 0 && height > 0) ||
      numExplicitPages[completedPageIndex] > 0) {
    for (int source = 0; source < PAGESOURCE_TOTAL; ++source) {
      idVirtualTexture* virtualTexture = vtPtrs[source];
      virtualTextureInfo_t& info = vtInfo[source];
      if (virtualTexture->pageFilePath.data[0] == '\0') {
        info.numLevels = 0;
        info.baseLevel = vt_minLod.valueInteger;
        info.pagesWide = 0;
        info.physicalPagesPool = 0;
      } else {
        info.numLevels = virtualTexture->header.numLevels;
        info.baseLevel = vt_minLod.valueInteger;
        info.pagesWide = virtualTexture->header.pagesWide;
        info.physicalPagesPool = static_cast<std::uint16_t>(
            virtualTexture->physicalPages->physicalPagesPool);
      }
      info.priorityBias = 0;
    }
    analyzeParms.analyzeFrame = feedbackFrameNum;
    analyzeParms.pageIndices = pageIndices[completedPageIndex];
    analyzeParms.vtInfo = vtInfo;
    analyzeParms.data = data;
    analyzeParms.width = static_cast<unsigned int>(width);
    analyzeParms.height = static_cast<unsigned int>(height);
    analyzeParms.bytePitch = static_cast<unsigned int>(bytePitch);
    analyzeParms.data2 = explicitPages[completedPageIndex];
    analyzeParms.data2Width =
        static_cast<unsigned int>(numExplicitPages[completedPageIndex]);
    analyzeParms.forceHighestDetail = forceHighestDetail;
    if (vt_showExplicitPages.valueInteger != 0) {
      idLib::Printf("Explicit pages: %i\n",
                    numExplicitPages[completedPageIndex]);
    }
    if (vt_useAnalyzeJobs.valueInteger == 1) {
      analyzeJobList->AddJob(
          reinterpret_cast<jobRun_t>(FeedbackAnalysisJob), &analyzeParms);
    } else {
      FeedbackAnalysisJob(&analyzeParms);
    }
  } else {
    currentPageIndices ^= 1u;
  }

  const bool useSortJobs = vt_usePageSortJobs.valueInteger == 1;
  for (idPhysicalPages& physicalPages : physicalPagesPool) {
    SchedulePageSort(physicalPages, feedbackFrameNum, analyzeJobList,
                     useSortJobs);
  }
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    idVirtualTexture* virtualTexture = vtPtrs[source];
    if (virtualTexture->flavor != ID_VTF_UNKNOWN &&
        virtualTexture->GetPhysicalPagesPool() == PHYSICAL_PAGES_POOL_PRT) {
      SchedulePageSort(*virtualTexture->physicalPages, feedbackFrameNum,
                       analyzeJobList, useSortJobs);
    }
  }
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    if (vtPtrs[source]->flavor != ID_VTF_UNKNOWN) {
      vtPtrs[source]->UpdateMinLod(analyzeJobList);
    }
  }
  analyzeJobList->Submit(nullptr, -1);

  const pageIndices_t* completed = pageIndices[currentPageIndices];
  for (int pool = 0; pool < PHYSICAL_PAGES_POOL_SHARED_COUNT; ++pool) {
    physicalPagesPool[pool].feedbackNumPages =
        completed->pageStats[pool].numPages;
  }
  if (vt_showFeedbackAnalysis.valueInteger != 0) {
    vt_showFeedbackAnalysis.SetBool(false, true);
    ShowFeedbackAnalysis(static_cast<std::uint8_t*>(data), width, height,
                         bytePitch);
  }
  if (vt_showFeedbackBuffer.valueInteger != 0) {
    VisualizeFeedbackBuffer(static_cast<char*>(data), width, height,
                            bytePitch);
  } else if (vt_showFeedbackStreaming.valueInteger != 0) {
    ShowFeedbackStreaming(static_cast<char*>(data), width, height, bytePitch);
  } else {
    if (feedbackVisualize != nullptr) {
      delete feedbackVisualize;
    }
    if (feedbackStreaming != nullptr) {
      delete feedbackStreaming;
    }
    feedbackVisualize = nullptr;
    feedbackStreaming = nullptr;
  }
}

void idVirtualTextureSystem::GetDiskStatsForFeedback(
    const pageIndices_t* transcodePageIndices, int* numCacheLines,
    int* numSeekAndReadMsec) {
  *numCacheLines = 0;
  *numSeekAndReadMsec = 0;
  idList<pageFileStats_t, 5> files;
  std::uint64_t cumulativeDiskOffset = 0;
  int maxNumLevels = 0;
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    idVirtualTexture* virtualTexture = vtPtrs[source];
    if (virtualTexture->flavor != ID_VTF_BAKE) {
      continue;
    }
    int fileIndex = 0;
    while (fileIndex < files.num &&
           idStr::Cmp(files[fileIndex].path.data,
                      virtualTexture->pageFilePath.data) != 0) {
      ++fileIndex;
    }
    if (fileIndex == files.num) {
      pageFileStats_t stats;
      stats.path = virtualTexture->pageFilePath;
      stats.length = virtualTexture->header.totalFileSize;
      stats.diskOffset = cumulativeDiskOffset;
      cumulativeDiskOffset += stats.length;
      files.Append(stats);
    }
    maxNumLevels =
        std::max(maxNumLevels, static_cast<int>(virtualTexture->header.numLevels));
  }

  for (std::uint64_t index = 0;
       index < transcodePageIndices->totalNumPages; ++index) {
    const std::uint64_t encoded = transcodePageIndices->indices[index];
    const int source = static_cast<int>((encoded >> 28u) & 0x0Fu);
    const int level = static_cast<int>((encoded >> 24u) & 0x0Fu);
    const int x = static_cast<int>((encoded >> 2u) & 0x7FFu);
    const int y = static_cast<int>((encoded >> 13u) & 0x7FFu);
    idVirtualTexture* virtualTexture = vtPtrs[source];
    if (virtualTexture == nullptr ||
        x >= virtualTexture->mipLevelPagesWide[level] ||
        y >= virtualTexture->mipLevelPagesWide[level]) {
      continue;
    }
    idPhysicalPages* physicalPages = virtualTexture->physicalPages;
    const int physicalPageNum = FindPhysicalPage(
        *physicalPages, static_cast<pageSource_t>(source), level, x, y);
    if (physicalPageNum < 0 ||
        physicalPages->pageHash.pageHashIndex[physicalPageNum].mapped == 0) {
      continue;
    }
    int fileID = 0;
    while (fileID < files.num &&
           idStr::Cmp(files[fileID].path.data,
                      virtualTexture->pageFilePath.data) != 0) {
      ++fileID;
    }
    if (fileID == files.num) {
      continue;
    }
    const texturePage_t& page = physicalPages->pages[physicalPageNum];
    if (page.coarserPageNum == -1) {
      continue;
    }
    const texturePage_t& coarser =
        physicalPages->pages[page.coarserPageNum];
    const int quadrant = (x & 1) | (2 * (y & 1));
    const std::uint64_t diskCacheLine =
        (files[fileID].diskOffset +
         static_cast<std::uint64_t>(virtualTexture->header.diskOffsetScale) *
             coarser.finerDiskOffset[quadrant]) >>
        16u;
    int match = 0;
    while (match < files[fileID].cacheLines.num &&
           (files[fileID].cacheLines[match].fileID != fileID ||
            files[fileID].cacheLines[match].diskCacheLine != diskCacheLine)) {
      ++match;
    }
    if (match == files[fileID].cacheLines.num) {
      cacheLineStat_t cacheLine{};
      cacheLine.fileID = fileID;
      cacheLine.mipLevel = page.pageLevel;
      cacheLine.diskCacheLine = diskCacheLine;
      files[fileID].cacheLines.Append(cacheLine);
    }
  }

  for (int fileIndex = 0; fileIndex < files.num; ++fileIndex) {
    idList<cacheLineStat_t, 5>& lines = files[fileIndex].cacheLines;
    std::sort(lines.list, lines.list + lines.num,
              [](const cacheLineStat_t& a, const cacheLineStat_t& b) {
                return SortCacheLinesOnOffset(&a, &b) < 0;
              });
  }
  idList<cacheLineStat_t, 5> cacheLines;
  for (int mipLevel = maxNumLevels; mipLevel >= 0; --mipLevel) {
    for (int fileIndex = 0; fileIndex < files.num; ++fileIndex) {
      for (int lineIndex = 0;
           lineIndex < files[fileIndex].cacheLines.num; ++lineIndex) {
        const cacheLineStat_t& line = files[fileIndex].cacheLines[lineIndex];
        if (line.mipLevel == mipLevel) {
          cacheLines.Append(line);
        }
      }
    }
  }

  *numCacheLines = cacheLines.num;
  for (int index = 1; index < cacheLines.num; ++index) {
    const std::int64_t skippedLines =
        static_cast<std::int64_t>(cacheLines[index].diskCacheLine) -
        static_cast<std::int64_t>(cacheLines[index - 1].diskCacheLine) - 1;
    if (skippedLines < 0 ||
        skippedLines >= vt_noSeekCacheLines.valueInteger) {
      *numSeekAndReadMsec +=
          static_cast<int>((220 * std::llabs(skippedLines)) >> 16) + 26;
    } else {
      *numSeekAndReadMsec += static_cast<int>(6 * skippedLines + 6);
    }
  }
}

void idVirtualTextureSystem::UpdateStats(
    const pageIndices_t* transcodePageIndices, int numTranscodePages,
    int maxTranscodePagesPerFrame, int totalStreamedPageData) {
  const int now = static_cast<int>(Sys_Milliseconds());
  const int historyIndex = transcodeJobHistoryIndex & 15;
  transcodeJobHistoryBytes[historyIndex] = totalStreamedPageData;
  transcodeJobHistoryPages[historyIndex] = numTranscodePages;
  transcodeJobHistoryTime[historyIndex] = now - lastTranscodeTime;
  lastTranscodeTime = now;
  ++transcodeJobHistoryIndex;

  float pagesPerSecond = 0.0f;
  float bytesPerSecond = 0.0f;
  float gpuMilliseconds = 0.0f;
  for (int index = 0; index < 16; ++index) {
    if (transcodeJobHistoryTime[index] > 0) {
      const float framesPerSecond =
          1000.0f / static_cast<float>(transcodeJobHistoryTime[index]);
      pagesPerSecond += transcodeJobHistoryPages[index] * framesPerSecond;
      bytesPerSecond += transcodeJobHistoryBytes[index] * framesPerSecond;
      gpuMilliseconds += transcodeJobHistoryGPUTime[index];
    }
  }
  pagesPerSecond *= 1.0f / 16.0f;
  bytesPerSecond *= 1.0f / 16.0f;
  gpuMilliseconds *= 1.0f / 16.0f;

  if (vt_showStats.valueInteger != 0) {
    const int width = renderSystem->GetWidth();
    const int height = renderSystem->GetHeight();
    int cacheLines = 0;
    int seekMilliseconds = 0;
    GetDiskStatsForFeedback(transcodePageIndices, &cacheLines,
                            &seekMilliseconds);
    console->PrintOverlay(
        &transcodeOverlayHandle, 1, 1.0f, 1.0f,
        "vmtr / spec / dif1 / dif2       \n"
        "%4d / %4d / %4d / %4d Pg max\n"
        "%4u / %4u / %4u / %4u Pg tot\n"
        "%4u / %4u / %4u / %4u Pg vis\n"
        "%4u / %4u / %4u / %4u Pg top\n"
        "%4.1f / %4.1f / %4.1f / %4.1f X scrn\n"
        "%4.1f / %4.1f / %4.1f / %4.1f f-bias\n"
        "%d cache lines\n%d seek msec\n%1.0f Pg/s\n%1.1f MP/s\n"
        "%1.1f MB/s\n%1.1f GPU ms",
        physicalPagesPool[0].NUM_PHYSICAL_PAGES,
        physicalPagesPool[1].NUM_PHYSICAL_PAGES,
        physicalPagesPool[2].NUM_PHYSICAL_PAGES,
        physicalPagesPool[3].NUM_PHYSICAL_PAGES,
        transcodePageIndices->pageStats[0].numPages,
        transcodePageIndices->pageStats[1].numPages,
        transcodePageIndices->pageStats[2].numPages,
        transcodePageIndices->pageStats[3].numPages,
        transcodePageIndices->pageStats[0].numVisPages,
        transcodePageIndices->pageStats[1].numVisPages,
        transcodePageIndices->pageStats[2].numVisPages,
        transcodePageIndices->pageStats[3].numVisPages,
        transcodePageIndices->pageStats[0].numTopPages,
        transcodePageIndices->pageStats[1].numTopPages,
        transcodePageIndices->pageStats[2].numTopPages,
        transcodePageIndices->pageStats[3].numTopPages,
        transcodePageIndices->pageStats[0].numVisPages * 128.0f * 128.0f /
            (static_cast<float>(width) * height),
        transcodePageIndices->pageStats[1].numVisPages * 128.0f * 128.0f /
            (static_cast<float>(width) * height),
        transcodePageIndices->pageStats[2].numVisPages * 128.0f * 128.0f /
            (static_cast<float>(width) * height),
        transcodePageIndices->pageStats[3].numVisPages * 128.0f * 128.0f /
            (static_cast<float>(width) * height),
        physicalPagesPool[0].feedbackDynamicLodBias,
        physicalPagesPool[1].feedbackDynamicLodBias,
        physicalPagesPool[2].feedbackDynamicLodBias,
        physicalPagesPool[3].feedbackDynamicLodBias, cacheLines,
        seekMilliseconds, pagesPerSecond,
        pagesPerSecond * 128.0f * 128.0f * 0.000001f,
        bytesPerSecond * 0.00000095367432f, gpuMilliseconds);
    if (transcodeDebugGraph == nullptr) {
      transcodeDebugGraph = console->CreateGraph(100);
      transcodeDebugGraph->SetBackgroundColor(idColor::colorBlack);
    }
    transcodeDebugGraph->SetPosition(
        static_cast<float>(renderSystem->GetWidth() - 250), 340.0f, 100.0f,
        100.0f);
    transcodeDebugGraph->Enable(true);
    transcodeDebugGraph->SetValue(
        -1, static_cast<float>(numTranscodePages) /
                static_cast<float>(maxTranscodePagesPerFrame),
        idColor::colorRed);
  } else if (transcodeDebugGraph != nullptr) {
    transcodeDebugGraph->Enable(false);
  }

  if (forge_logPerformanceMetrics.valueInteger != 0) {
    metricMutex.Lock(true);
    vmtrTotalPagesMetric.Log(static_cast<float>(physicalPagesPool[0].NUM_PHYSICAL_PAGES), true);
    vmtrBiasMetric.Log(physicalPagesPool[0].feedbackDynamicLodBias, true);
    specTotalPagesMetric.Log(static_cast<float>(physicalPagesPool[1].NUM_PHYSICAL_PAGES), true);
    specBiasMetric.Log(physicalPagesPool[1].feedbackDynamicLodBias, true);
    diffuseTotalPagesMetric.Log(static_cast<float>(physicalPagesPool[2].NUM_PHYSICAL_PAGES), true);
    diffuseBiasMetric.Log(physicalPagesPool[2].feedbackDynamicLodBias, true);
    metricMutex.Unlock();
  }

  if (vt_recordStats.valueString.Length() != 0) {
    if (transcodeStatsFile != nullptr &&
        idStr::Icmp(transcodeStatsFile->GetName(),
                    vt_recordStats.valueString.data) != 0) {
      delete transcodeStatsFile;
      transcodeStatsFile = nullptr;
    }
    if (transcodeStatsFile == nullptr) {
      transcodeStatsFile = fileSystem->OpenFileWrite(
          vt_recordStats.valueString.data, FSPATH_SAVE);
    }
    if (transcodeStatsFile != nullptr) {
      transcodeStatsFile->WriteFloatString("%f\r\n", pagesPerSecond);
    }
  } else if (transcodeStatsFile != nullptr) {
    transcodeStatsFile->Flush();
    delete transcodeStatsFile;
    transcodeStatsFile = nullptr;
  }
}

void idVirtualTextureSystem::CloseCachedPagesFile(idFile* file) {
  delete file;
}

void idVirtualTextureSystem::SetContainerPath(const char* path) {
  idStr::Copynz(containerPath, path, sizeof(containerPath));
}

void idVirtualTextureSystem::AddToReferencedVirtualTextures(
    const char* name, const char* path) {
  for (int index = 0; index < numReferencedVirtualTextures; ++index) {
    if (std::strcmp(referencedVTName[index], name) == 0) {
      return;
    }
  }
  if (numReferencedVirtualTextures == 100) {
    idLib::Printf("MAX_REFERENCED_VIRTUAL_TEXTURES", name);
    return;
  }
  std::strncpy(referencedVTName[numReferencedVirtualTextures], name, 31);
  std::strncpy(referencedVTPath[numReferencedVirtualTextures], path, 255);
  ++numReferencedVirtualTextures;
}

void idVirtualTextureSystem::WaitForLastAnalyzeFeedback() {
  if (analyzeJobList->IsSubmitted()) {
    analyzeJobList->Wait();
    currentPageIndices ^= 1u;
    numExplicitPages[currentPageIndices] = 0;
  }
}

void idVirtualTextureSystem::ValidateCache_f(const idCmdArgs&) {
  for (idPhysicalPages& physicalPages : virtualTextureSystem.physicalPagesPool) {
    physicalPages.ValidateCache();
  }
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    idVirtualTexture* virtualTexture = virtualTextureSystem.vtPtrs[source];
    if (virtualTexture->flavor == ID_VTF_UNKNOWN) {
      continue;
    }
    idLib::Printf("Validating %s page table... ",
                  virtualTexture->pageFilePath.data);
    idLib::Printf(virtualTexture->ValidatePageTable() ? "successful\n"
                                                      : "failed\n");
  }
}

void idVirtualTextureSystem::UpdateFilterParms(bool forceUpdateFilterParms) {
  if ((vt_lodBias.flags & 0x20000) != 0 ||
      (vt_maxAniso.flags & 0x20000) != 0) {
    forceUpdateFilterParms = true;
    const float bias =
        vt_showPageUsage.valueInteger > 0 ? 0.0f : vt_lodBias.valueFloat;
    for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
      idImage* pageTableImage = vtPtrs[source]->pageTableImage;
      if (pageTableImage != nullptr) {
        samplerOptions_t sampler = pageTableImage->opts.sampler;
        sampler.lodBias = bias;
        pageTableImage->ModifySampler(sampler);
      }
    }
  }

  for (idPhysicalPages& physicalPages : physicalPagesPool) {
    physicalPages.UpdateFilterParms(forceUpdateFilterParms);
  }
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    idVirtualTexture* virtualTexture = vtPtrs[source];
    if (virtualTexture->flavor != ID_VTF_UNKNOWN &&
        virtualTexture->GetPhysicalPagesPool() == PHYSICAL_PAGES_POOL_PRT) {
      virtualTexture->physicalPages->UpdateFilterParms(forceUpdateFilterParms);
    }
  }
  UpdatePhysicalFilterParms(rpPhysicalPRTFilterParms.ConstPtr(), 0.0f,
                            &physicalPagesPool[0].oldFeedbackDynamicLodBias,
                            true, forceUpdateFilterParms);
  renderThreadParmState->SetParmVector(
      rpVirtualTextureOptions.ConstPtr(),
      vt_useMinLodTexture.valueInteger != 0 ? 1.0f : 0.0f,
      vt_filter.valueInteger == 2 ? 1.0f : 0.0f, vt_lodBias.valueFloat,
      0.0f);
  vt_maxAniso.flags &= ~0x20000u;
  vt_filter.flags &= ~0x20000u;
  vt_lodBias.flags &= ~0x20000u;
  vt_feedbackLodBias.flags &= ~0x20000u;
}

void VirtualTextureInfo_f(const idCmdArgs&) {
  for (idPhysicalPages& physicalPages :
       virtualTextureSystem.physicalPagesPool) {
    if (physicalPages.physicalPagesImage1 == nullptr) {
      continue;
    }
    physicalPages.physicalPagesImage1->Print();
    if (physicalPages.physicalPagesPool !=
            PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY &&
        physicalPages.physicalPagesPool !=
            PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2) {
      physicalPages.physicalPagesImage0->Print();
      physicalPages.physicalPagesImage2->Print();
    }
  }

  int totalPageTableBytes = 0;
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    idVirtualTexture* virtualTexture = virtualTextureSystem.vtPtrs[source];
    if (virtualTexture->flavor == ID_VTF_UNKNOWN) {
      continue;
    }
    if (virtualTexture->pageTableImage != nullptr) {
      totalPageTableBytes += virtualTexture->pageTableImage->StorageSize();
    }
    idLib::Printf("\nvirtual texture %i:\n", source);
    virtualTexture->Print();
  }
  idLib::Printf("%4.1f total megs of page table images\n",
                static_cast<float>(totalPageTableBytes) * 0.00000095367432f);
}

void idVirtualTextureSystem::LoadMaterialPagesInBackground(
    const idMaterial* material, char lod) {
  extern idCVar vt_skipExplicitPageFeedback;
  if (vt_skipExplicitPageFeedback.valueInteger != 0) {
    return;
  }
  idVec4 mapping{};
  if (!material->GetVirtualMapping(mapping)) {
    return;
  }

  const float virtualTexels = static_cast<float>(vts[1].VIRTUAL_TEXELS_WIDE);
  const int divisor = 120 << lod;
  const int minX = static_cast<int>(virtualTexels * mapping.z) / divisor;
  const int maxX = static_cast<int>(virtualTexels *
                                    (mapping.z + mapping.x)) /
                   divisor;
  const int minY = static_cast<int>(virtualTexels * mapping.w) / divisor;
  const int maxY = static_cast<int>(virtualTexels *
                                    (mapping.w + mapping.y)) /
                   divisor;
  for (int y = minY; y <= maxY; ++y) {
    for (int x = minX; x <= maxX; ++x) {
      const int byteOffset = 4 * numExplicitPages[currentPageIndices];
      if (byteOffset >= 0x2000) {
        break;
      }
      ++numExplicitPages[currentPageIndices];
      std::uint8_t* encoded = explicitPages[currentPageIndices] + byteOffset;
      encoded[0] = static_cast<std::uint8_t>(x);
      encoded[1] = static_cast<std::uint8_t>(y);
      encoded[2] = static_cast<std::uint8_t>(((y >> 8) << 4) |
                                             ((x >> 8) & 0x0F));
      encoded[3] = static_cast<std::uint8_t>((lod & 0x0F) | 0x10);
    }
  }
}

void idVirtualTextureSystem::ShowFeedbackAnalysis(
    std::uint8_t* data, unsigned int width, unsigned int height,
    int bytePitch) {
  if (data == nullptr) {
    return;
  }
  struct uniqueFeedback_t {
    int x;
    int y;
    int level;
    int source;
    std::uint32_t packed;
  } uniques[2000]{};
  int numUnique = 0;
  for (unsigned int y = 0; y < height; ++y) {
    const std::uint8_t* pixel = data + static_cast<std::size_t>(bytePitch) * y;
    for (unsigned int x = 0; x < width; ++x, pixel += 4) {
      uniqueFeedback_t candidate{};
      candidate.x = pixel[0] + ((pixel[2] & 0x0F) << 8);
      candidate.y = pixel[1] + ((pixel[2] & 0xF0) << 4);
      candidate.level = pixel[3] & 0x0F;
      candidate.source = pixel[3] >> 4;
      std::memcpy(&candidate.packed, pixel, sizeof(candidate.packed));
      int match = 0;
      while (match < numUnique &&
             (uniques[match].x != candidate.x ||
              uniques[match].y != candidate.y ||
              uniques[match].level != candidate.level ||
              uniques[match].source != candidate.source)) {
        ++match;
      }
      if (match == numUnique && numUnique < 2000) {
        uniques[numUnique++] = candidate;
      }
    }
  }

  for (int index = 0; index < numUnique; ++index) {
    const uniqueFeedback_t& page = uniques[index];
    idLib::Printf("0x%8x %4u %4u %4u %4u ", page.packed, page.x,
                  page.y, page.level, page.source);
    if (page.source == PAGESOURCE_ALLOC0 && vts[1].vmtrData != nullptr) {
      idVMTRData* vmtrData = vts[1].vmtrData;
      for (int vmtrIndex = 0; vmtrIndex < vmtrData->vmtrs.num;
           ++vmtrIndex) {
        const idVirtualMaterial* vmtr = vmtrData->vmtrs.list[vmtrIndex];
        const bool insideX = vmtr->allocX / 120 <= page.x &&
                             (vmtr->allocX + vmtr->allocWidth - 1) / 120 >=
                                 page.x;
        const bool insideY = vmtr->allocY / 120 <= page.y &&
                             (vmtr->allocY + vmtr->allocHeight - 1) / 120 >=
                                 page.y;
        if (insideX && insideY) {
          idLib::Printf("%s ", vmtr->materialName.str);
        }
      }
    }
    idLib::Printf("\n");
  }
  idLib::Printf("%i unique base pages\n", numUnique);
}

void idVirtualTextureSystem::LogMetrics() {
  extern idCVar forge_logPerformanceMetrics;
  if (forge_logPerformanceMetrics.valueInteger != 0 &&
      metricMutex.Lock(false)) {
    vmtrTotalPagesMetric.CheckPushState();
    vmtrBiasMetric.CheckPushState();
    specTotalPagesMetric.CheckPushState();
    specBiasMetric.CheckPushState();
    diffuseTotalPagesMetric.CheckPushState();
    diffuseBiasMetric.CheckPushState();
    metricMutex.Unlock();
  }
}

void idVirtualTextureSystem::StreamFeedback(
    const pageIndices_t* transcodePageIndices) {
  cacheLineRequest_t requests[8192]{};
  int numRequests = 0;
  const bool requestCacheLines =
      vt_usePageFileCache.valueInteger != 0 &&
      vt_skipStreaming.valueInteger == 0;

  for (std::uint64_t pageIndex = 0;
       pageIndex < transcodePageIndices->totalNumPages; ++pageIndex) {
    const std::uint64_t encoded = transcodePageIndices->indices[pageIndex];
    const int source = static_cast<int>((encoded >> 28u) & 0x0Fu);
    idVirtualTexture* virtualTexture = vtPtrs[source];
    const int priority = static_cast<int>((encoded >> 48u) & 0x1Fu);
    const int level = static_cast<int>((encoded >> 24u) & 0x0Fu);
    const int x = static_cast<int>((encoded >> 2u) & 0x7FFu);
    const int y = static_cast<int>((encoded >> 13u) & 0x7FFu);
    const int persistenceBase =
        static_cast<int>((encoded >> 32u) & 0xFFFFu);
    if (virtualTexture == nullptr ||
        x >= virtualTexture->mipLevelPagesWide[level] ||
        y >= virtualTexture->mipLevelPagesWide[level]) {
      continue;
    }

    idPhysicalPages* physicalPages = virtualTexture->physicalPages;
    const int physicalPageNum = FindPhysicalPage(
        *physicalPages, static_cast<pageSource_t>(source), level, x, y);
    if (physicalPageNum >= 0 &&
        physicalPages->pageHash.pageHashIndex[physicalPageNum].mapped != 0) {
      physicalPages->pagePriority[physicalPageNum] = priority;
      physicalPages->pageFrameNum[physicalPageNum] = feedbackFrameNum;
      continue;
    }
    if (level >= virtualTexture->header.numLevels - 1 ||
        !requestCacheLines) {
      continue;
    }

    const int coarserPageNum = FindPhysicalPage(
        *physicalPages, static_cast<pageSource_t>(source), level + 1,
        x >> 1, y >> 1);
    if (coarserPageNum < 0 ||
        physicalPages->pageHash.pageHashIndex[coarserPageNum].mapped == 0) {
      continue;
    }
    const int quadrant = (x & 1) | (2 * (y & 1));
    const texturePage_t& coarser = physicalPages->pages[coarserPageNum];
    const int byteLength = virtualTexture->header.diskOffsetScale *
                           coarser.finerDiskLength[quadrant];
    if (byteLength <= 0) {
      continue;
    }
    const std::int64_t diskOffset =
        static_cast<std::int64_t>(virtualTexture->header.diskOffsetScale) *
        coarser.finerDiskOffset[quadrant];
    idFile* pageFile = virtualTexture->pageFileInstalled;
    if (pageFile == nullptr || level < virtualTexture->minMipLevelInstalled) {
      pageFile = virtualTexture->pageFile;
    }
    if (pageFile == nullptr) {
      continue;
    }

    const int firstCacheLine = static_cast<int>(diskOffset >> 16);
    const int lastCacheLine =
        static_cast<int>((diskOffset + byteLength - 1) >> 16);
    const int requestPriority = (priority << 15) + persistenceBase;
    const int requestPersistence = 64 << (level + 4);
    for (int cacheLine = firstCacheLine; cacheLine <= lastCacheLine;
         ++cacheLine) {
      int match = 0;
      while (match < numRequests &&
             (requests[match].file != pageFile ||
              requests[match].fileLine != cacheLine)) {
        ++match;
      }
      if (match == numRequests) {
        if (numRequests >= 8192) {
          break;
        }
        requests[numRequests].file = pageFile;
        requests[numRequests].fileLine = cacheLine;
        requests[numRequests].priority = requestPriority;
        requests[numRequests].persistence = requestPersistence;
        ++numRequests;
      } else {
        requests[match].priority =
            std::max(requests[match].priority, requestPriority);
        requests[match].persistence =
            std::max(requests[match].persistence, requestPersistence);
      }
    }
  }
  if (numRequests != 0) {
    streamFileCache->BatchRequestCacheLines(requests, numRequests);
    streamFileCache->SignalWork();
  }
}

void idVirtualTextureSystem::SyncFeedback() {
  if (feedbackBuffer.buffer == nullptr) {
    return;
  }
  WaitForLastAnalyzeFeedback();
  for (int index = 0; index < 2; ++index) {
    transcodeJobList_t& jobs = transcodeJobList[currentTranscodeJobList];
    jobs.parallelJobList->Wait();
    const int completed = currentTranscodeJobList;
    currentTranscodeJobList = (currentTranscodeJobList + 1) % 2;
    transcodeJobList_t& completedJobs = transcodeJobList[completed];
    if (completedJobs.nextFramePageImageUpload) {
      if (completedJobs.transcodePBO.IsMapped()) {
        completedJobs.transcodePBO.UnmapBuffer();
      }
      UploadPageImages(&UnmapParms(completedJobs),
                       &ImageUpdateParms(completedJobs));
      completedJobs.nextFramePageImageUpload = false;
    }
  }
}

void idVirtualTextureSystem::ClearFeedback() {
  if (feedbackBuffer.buffer == nullptr) {
    return;
  }
  std::memset(feedbackBuffer.buffer, 0, feedbackBuffer.num);
  for (int index = 0; index < 2; ++index) {
    pageIndices[index]->totalNumPages = 0;
    std::memset(pageIndices[index]->pageStats, 0,
                sizeof(pageIndices[index]->pageStats));
    numExplicitPages[index] = 0;
  }
}

bool idVirtualTextureSystem::AllowGenMissingPages() const {
  return vt_genMissingPages.valueInteger == 1;
}

idFile* idVirtualTextureSystem::OpenCachedPagesFile(const char* path) {
  idLib::Printf("OpenCachedPagesFile: %s\n", path);
  const bool writableNetworkPage =
      com_production.valueInteger == 0 &&
      vt_readOnlyNetPages.valueInteger == 0;
  const bool localPath = idStr::Icmpn(path, "\\\\", 2) != 0 &&
                         idStr::Icmpn(path, "nfs:", 4) != 0;
  if ((writableNetworkPage || localPath) &&
      idLib::production < PROD_PRODUCTION &&
      r_skipVmtrs.valueInteger == 0 &&
      vt_useNonBufferedIO.valueInteger == 0) {
    return fileSystem->OpenFileReadWrite(path, false, FSPATH_BASE);
  }
  return fileSystem->OpenFileRead(path, false, true);
}

idVirtualTexture* idVirtualTextureSystem::VirtualTextureForMaterial(
    const idMaterial* material) {
  const char* pageFileName = material->pageFileName;
  if (pageFileName == nullptr || pageFileName[0] == '\0') {
    return nullptr;
  }

  physicalPagesPool_t selectedPool = material->physicalPagesPool;
  if (_glewTexStorageSparseAMD != nullptr &&
      vt_enablePRT.valueInteger != 0 &&
      selectedPool != PHYSICAL_PAGES_POOL_VMTR) {
    selectedPool = PHYSICAL_PAGES_POOL_PRT;
  }

  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    idVirtualTexture* virtualTexture = vtPtrs[source];
    if (idStr::Icmp(virtualTexture->baseName.data, pageFileName) != 0 ||
        virtualTexture->physicalPages == nullptr ||
        virtualTexture->GetPhysicalPagesPool() != selectedPool) {
      continue;
    }
    if (virtualTexture->flavor == ID_VTF_BAKE &&
        virtualTexture->layoutVersion == 0) {
      const int materialLayoutVersion = material->parmBlock.GetInteger(
          rpLandLayoutVersion.ConstPtr(), nullptr);
      virtualTexture->layoutVersion = materialLayoutVersion;
      if (virtualTexture->header.layoutVersion != 0 &&
          materialLayoutVersion != 0 &&
          materialLayoutVersion != virtualTexture->header.layoutVersion) {
        idLib::Warning(
            "Material layoutVersion (%d) differs from Virtual Texture "
            "layoutVersion (%d).",
            materialLayoutVersion, virtualTexture->header.layoutVersion);
      }
    }
    return virtualTexture;
  }

  int source = 1;
  while (source < PAGESOURCE_TOTAL &&
         vts[source].flavor != ID_VTF_UNKNOWN) {
    ++source;
  }
  if (source >= PAGESOURCE_TOTAL) {
    idLib::Error(
        "idVirtualTextureSystem::VirtualTextureForMaterial: "
        "PAGESOURCE_TOTAL exceeded");
  }

  idVirtualTexture* virtualTexture = vtPtrs[source];
  idStr specialVersion;
  if (feature_lowgore.valueInteger != 0) {
    specialVersion += "_lowgore";
  }
  virtualTexture->layoutVersion = material->parmBlock.GetInteger(
      rpLandLayoutVersion.ConstPtr(), nullptr);

  char pageFilePath[256];
  if (vt_packPageFiles.valueInteger != 2 ||
      com_production.valueInteger != 0) {
    std::sprintf(pageFilePath, "%s/%s_v%03d%s.pages",
                 vt_filePath.valueString.data, pageFileName,
                 virtualTexture->layoutVersion, specialVersion.data);
  } else {
    std::sprintf(pageFilePath, "%s/%s%s.pages",
                 vt_filePath.valueString.data, pageFileName,
                 specialVersion.data);
  }

  const bool openedContainer =
      containerPath[0] != '\0' && vt_containers.valueInteger != 0 &&
      virtualTexture->SetSource(pageFileName, containerPath, selectedPool,
                                material->pageFlavor);
  if (!openedContainer &&
      !virtualTexture->SetSource(pageFileName, pageFilePath, selectedPool,
                                 material->pageFlavor)) {
    if (r_skipVmtrs.valueInteger == 0) {
      idLib::Error("Couldn't open page file '%s'", pageFilePath);
    }
    const idDeclRenderParm* landDimensions =
        static_cast<const idDeclRenderParm*>(
            idDeclRenderParm::resourceList.FindWithInheritance(
                "landDimensions", true, true));
    int size = material->parmBlock.GetInteger(landDimensions, nullptr);
    if (mt_sizeOverride.valueInteger > 0) {
      size = mt_sizeOverride.valueInteger;
      idLib::Printf("Using a size override of %i\n",
                    mt_sizeOverride.valueInteger);
    }
    virtualTexture->SetEmpty(
        pageFilePath, 8 * size,
        vt_uncompressedVmtr.valueInteger != 0 ? 128 : 64);
  }
  AddToReferencedVirtualTextures(pageFileName, pageFilePath);
  return virtualTexture;
}

void idVirtualTextureSystem::VisaulizeFeedback() {
  idImage* visualizeImage = feedbackVisualize;
  if (visualizeImage == nullptr) {
    visualizeImage = feedbackStreaming;
  }
  if (visualizeImage == nullptr) {
    return;
  }

  GL_SetDefaultState();
  const int width = tr.GetWidth();
  const int height = tr.GetHeight();
  GL_Viewport(width / 8, height / 8, 6 * width / 8, 6 * height / 8);
  GL_Scissor(0, 0, width, height);
  renderThreadParmState->SetFragmentPositionToViewTexture(
      6 * width / 8, 6 * height / 8, 6 * width / 8, 6 * height / 8);
  renderThreadParmState->SetMVPMatrix(renderMatrix_identity);
  renderThreadParmState->SetParmSampler(
      rpViewColorMap.ConstPtr(),
      reinterpret_cast<const idTextureSampler*>(visualizeImage));
  const idDeclRenderProg* identity = progViewColorIdentity.ConstPtr();
  renderThreadParmState->VirtualEvaluateParmBlock(
      identity->data.parmBlock, identity->data.parmBlock);
  GL_DrawElements({identity}, {tr.unitSquareTris}, 0, false);
  renderThreadParmState->VirtualRollbackParmBlock(
      identity->data.parmBlock, identity->data.parmBlock);
}

bool idVirtualTextureSystem::TranscodeFeedback(
    const pageIndices_t* transcodePageIndices,
    int* transcodePageIndicesOffset, bool blocking, bool lockPages) {
  if (streamFileCache == nullptr) {
    return false;
  }
  streamFileCache->NewFrame();
  if (vt_singleStep.valueInteger >= 0) {
    if (vt_singleStep.valueInteger == 0) {
      return false;
    }
    vt_singleStep.SetInteger(vt_singleStep.valueInteger - 1, true);
  }

  transcodeJobList_t& waitJobs =
      transcodeJobList[currentTranscodeJobList];
  if (blocking) {
    waitJobs.parallelJobList->Wait();
  } else if (vt_recordStats.valueString.Length() == 0) {
    if (!waitJobs.parallelJobList->TryWait()) {
      UpdateStats(transcodePageIndices, 0, 1, 0);
      return false;
    }
    ++feedbackFrameNum;
  } else {
    waitJobs.parallelJobList->Wait();
    ++feedbackFrameNum;
  }

  if (waitJobs.nextFramePageImageUpload &&
      waitJobs.parallelJobList->TryWait()) {
    waitJobs.transcodePBO.UnmapBuffer();
    UploadPageImages(&UnmapParms(waitJobs), &ImageUpdateParms(waitJobs));
    waitJobs.nextFramePageImageUpload = false;
  }

  const int completedList = currentTranscodeJobList;
  transcodeJobList_t& jobs = transcodeJobList[completedList];
  currentTranscodeJobList = (currentTranscodeJobList + 1) % 2;
  transcodeJobList_t& nextJobs = transcodeJobList[currentTranscodeJobList];
  pageUploadCount += ImageUpdateParms(jobs).numUpdates -
                     ImageUpdateParms(jobs).numCancelled;
  console->AddJobListTimeRange(RANGE_JBL_VIRT, &idColor::colorLtGrey,
                               jobs.parallelJobList, 0);

  const int totalNumPages =
      static_cast<int>(transcodePageIndices->totalNumPages);
  if (*transcodePageIndicesOffset >= totalNumPages) {
    UpdateStats(transcodePageIndices, 0, 1, 0);
    return true;
  }

  const int maxTranscodePagesPerFrame = vt_maxPPF.valueInteger;
  const bool debugTranscode =
      vt_stripPageBorders.valueInteger != 0 ||
      vt_showPageSources.valueInteger != 0 ||
      vt_showPageMips.valueInteger != 0 ||
      vt_showPageColors.valueInteger != 0 ||
      vt_showPageNumbers.valueInteger != 0 ||
      vt_showPageInfo.valueInteger != 0 ||
      vt_showPageBorders.valueInteger != 0 ||
      vt_showPageCheckered.valueInteger != 0 ||
      vt_showPageSizes.valueInteger != 0 ||
      vt_showPageUsage.valueInteger != 0;
  int useTranscodeJobs = vt_useTranscodeJobs.valueInteger;
  if (debugTranscode) {
    useTranscodeJobs = std::clamp(useTranscodeJobs, 0, 1);
  }

  pageImagesUpdateParms_t& unmapParms = UnmapParms(jobs);
  pageImagesUpdateParms_t& imageUpdateParms = ImageUpdateParms(jobs);
  unmapParms.numUpdates = 0;
  unmapParms.numCancelled = 0;
  unmapParms.submittedJob = false;
  unmapParms.usePageTablePBOs = vt_usePageTablePBOs.valueInteger != 0;
  unmapParms.useUnsynchronizedPageTablePBOs =
      vt_useUnsynchronizedPageTablePBOs.valueInteger != 0;
  unmapParms.useInvalidateRangeBitForPageTablePBOMap =
      vt_useInvalidateRangeBitForMappingAndPageTablePBOMap.valueInteger != 0;
  unmapParms.skipPhysicalTextureUploads =
      vt_skipPhysicalTextureUploads.valueInteger != 0;
  imageUpdateParms.numUpdates = 0;
  imageUpdateParms.numCancelled = 0;
  imageUpdateParms.submittedJob = false;
  imageUpdateParms.usePageTablePBOs = unmapParms.usePageTablePBOs;
  imageUpdateParms.useUnsynchronizedPageTablePBOs =
      unmapParms.useUnsynchronizedPageTablePBOs;
  imageUpdateParms.useInvalidateRangeBitForPageTablePBOMap =
      unmapParms.useInvalidateRangeBitForPageTablePBOMap;
  imageUpdateParms.skipPhysicalTextureUploads =
      unmapParms.skipPhysicalTextureUploads;
  if (useTranscodeJobs == 1) {
    jobs.parallelJobList->AddJob(
        reinterpret_cast<jobRun_t>(UpdatePageImagesJob), &unmapParms);
    jobs.parallelJobList->InsertSyncPoint(SYNC_SIGNAL);
    jobs.parallelJobList->InsertSyncPoint(SYNC_SYNCHRONIZE);
    unmapParms.submittedJob = true;
  }

  int totalStreamedPageData = 0;
  int numTranscodePages = 0;
  std::uint8_t* sourceCursor = jobs.transcodeSourceBuffer;
  auto* mappedPBO = static_cast<std::uint8_t*>(
      jobs.transcodePBO.MapBuffer(BM_WRITE_NOSYNC));
  jobs.numTranscodeJobs = 0;
  int pboOffset = 0;
  bool allDirectMapped = true;
  bool allDataReady = true;
  bool completedBatch = true;

  while (*transcodePageIndicesOffset < totalNumPages) {
    const std::uint64_t encoded =
        transcodePageIndices->indices[*transcodePageIndicesOffset];
    const int source = static_cast<int>(
        static_cast<std::uint32_t>(encoded) >> 28u);
    const int priority = static_cast<int>((encoded >> 48u) & 0x1Fu);
    int level = static_cast<int>((encoded >> 24u) & 0x0Fu);
    const int x = static_cast<int>((encoded >> 2u) & 0x7FFu);
    const int y = static_cast<int>((encoded >> 13u) & 0x7FFu);
    const int persistence = static_cast<int>((encoded >> 32u) & 0xFFFFu);
    idVirtualTexture* virtualTexture = vtPtrs[source];
    bool advancePageIndex = true;

    if (virtualTexture == nullptr ||
        x >= virtualTexture->mipLevelPagesWide[level] ||
        y >= virtualTexture->mipLevelPagesWide[level] ||
        virtualTexture->physicalPages == nullptr) {
      ++*transcodePageIndicesOffset;
      continue;
    }

    idPhysicalPages* physicalPages = virtualTexture->physicalPages;
    texturePage_t* coarser = nullptr;
    int physicalPageNum = FindPhysicalPage(
        *physicalPages, static_cast<pageSource_t>(source), level, x, y);
    if (physicalPageNum >= 0) {
      idPhysicalPageHash::pageHashEntry_t& hashEntry =
          physicalPages->pageHash.pageHashIndex[physicalPageNum];
      if (hashEntry.mapped == 0) {
        ++*transcodePageIndicesOffset;
        continue;
      }
      texturePage_t* residentPage = &physicalPages->pages[physicalPageNum];
      if (lockPages && !residentPage->locked) {
        residentPage->locked = true;
        physicalPages->unlockedPageList.Remove(
            residentPage->physicalPageNum);
        ++physicalPages->numLockedPages;
      }
      physicalPages->pagePriority[physicalPageNum] = priority;
      physicalPages->pageFrameNum[physicalPageNum] = feedbackFrameNum;
      if (!residentPage->dirty) {
        ++*transcodePageIndicesOffset;
        continue;
      }
      residentPage->dirty = false;
    }

    if (level < virtualTexture->header.numLevels - 1) {
      const int coarserPageNum = FindPhysicalPage(
          *physicalPages, static_cast<pageSource_t>(source), level + 1,
          x >> 1, y >> 1);
      if (coarserPageNum < 0 ||
          physicalPages->pageHash.pageHashIndex[coarserPageNum].mapped == 0) {
        ++*transcodePageIndicesOffset;
        continue;
      }
      coarser = &physicalPages->pages[coarserPageNum];
    }
    if (physicalPages->AllocWouldThrash(static_cast<char>(priority))) {
      ++*transcodePageIndicesOffset;
      continue;
    }

    const pageQuadrant_t quadrant = static_cast<pageQuadrant_t>(
        (x & 1) | (2 * (y & 1)));
    const int diskOffsetScale = virtualTexture->header.diskOffsetScale;
    std::int64_t diskOffset =
        static_cast<std::int64_t>(diskOffsetScale) *
        (coarser != nullptr
             ? coarser->finerDiskOffset[quadrant]
             : virtualTexture->header.subRoot.finerDiskOffset[0]);
    int diskLength =
        diskOffsetScale *
        (coarser != nullptr
             ? coarser->finerDiskLength[quadrant]
             : virtualTexture->header.subRoot.finerDiskLength[0]);
    int upsampleLevel = 0;
    int upsampleX = -1;
    int upsampleY = -1;
    if (vt_genMissingPages.valueInteger == 1 && coarser != nullptr &&
        diskLength == 0) {
      texturePage_t* sourcePage = coarser;
      while (diskLength == 0 && sourcePage->coarserPageNum != -1) {
        const pageQuadrant_t sourceQuadrant =
            static_cast<pageQuadrant_t>((sourcePage->pageX & 1) |
                                        (2 * (sourcePage->pageY & 1)));
        sourcePage =
            &physicalPages->pages[sourcePage->coarserPageNum];
        diskOffset = static_cast<std::int64_t>(diskOffsetScale) *
                     sourcePage->finerDiskOffset[sourceQuadrant];
        diskLength = diskOffsetScale *
                     sourcePage->finerDiskLength[sourceQuadrant];
        ++upsampleLevel;
      }
      const int mask = (1 << upsampleLevel) - 1;
      upsampleX = x & mask;
      upsampleY = y & mask;
    }

    std::uint8_t* nextSource =
        sourceCursor + ((diskLength + 135) & ~127);
    if (nextSource + 16 >= jobs.transcodeSourceBuffer + 0x100000) {
      completedBatch = false;
      break;
    }

    const bool directMapped =
        physicalPages->isDirectMapped && virtualTexture->isDirectMapped &&
        vt_useDirectTextureAccess.valueInteger != 0;
    const int outPageDataLength =
        directMapped ? 0
                     : (physicalPages->pageImageCompressed ? 40960 :
                                                              196608);
    if (!directMapped && pboOffset + outPageDataLength > 0xA00000) {
      completedBatch = false;
      break;
    }

    idFile* pageFile = virtualTexture->pageFileInstalled;
    if (pageFile == nullptr || level < virtualTexture->minMipLevelInstalled) {
      pageFile = virtualTexture->pageFile;
    }
    if (r_skipVmtrs.valueInteger != 0 || pageFile == nullptr) {
      ++*transcodePageIndicesOffset;
      continue;
    }

    pageHeader_t* sourceHeader =
        reinterpret_cast<pageHeader_t*>(sourceCursor + 8);
    bool readSucceeded = true;
    if (vt_skipStreaming.valueInteger != 0) {
      SwapDCTDataLengths(defaultDCTPage.DCTHeader);
      defaultDCTPage.header.magic = PAGE_SUBROOT_MAGIC;
      defaultDCTPage.header.pageCompression = COMP_DCT;
      defaultDCTPage.header.x = static_cast<std::uint16_t>(x);
      defaultDCTPage.header.y = static_cast<std::uint16_t>(y);
      defaultDCTPage.header.level = static_cast<std::uint16_t>(level);
      sourceHeader = &defaultDCTPage.header;
      diskLength = (diskOffsetScale + 2639) & ~(diskOffsetScale - 1);
    } else {
      if (diskLength == 0) {
        ++*transcodePageIndicesOffset;
        continue;
      }
      if (vt_usePageFileCache.valueInteger != 0) {
        readSucceeded = streamFileCache->ReadCachedData(
                            pageFile, diskOffset, diskLength, sourceHeader,
                            blocking, persistence + (priority << 15),
                            64 << (level + 4), PFC_TEXTURE) == diskLength;
        if (readSucceeded) {
          totalStreamedPageData += diskLength;
        } else {
          allDataReady = false;
        }
      } else {
        readSucceeded = pageFile->ReadOfs(diskOffset, sourceHeader,
                                          diskLength) ==
                        static_cast<std::size_t>(diskLength);
      }
      if (vt_showPageLoads.valueInteger != 0) {
        idLib::Printf(
            "Frame %i: page x %u y %u level %u source %u\n",
            feedbackFrameNum, x, y, level, source);
      }
      if (!readSucceeded) {
        ++*transcodePageIndicesOffset;
        continue;
      }
    }

    if (vt_supportRoundedDownVmtrs.valueInteger != 0 &&
        virtualTexture->flavor == ID_VTF_BAKE) {
      ++level;
    }

    pageHeader_t pageHeader{};
    std::memcpy(&pageHeader, sourceHeader, sizeof(pageHeader));
    const unsigned int headerX = pageHeader.x;
    const unsigned int headerY = pageHeader.y;
    const unsigned int headerLevel = pageHeader.level;
    const pageCompression_t pageCompression = pageHeader.pageCompression;
    if (pageHeader.magic != PAGE_SUBROOT_MAGIC ||
        headerX != static_cast<unsigned int>(x >> upsampleLevel) ||
        headerY != static_cast<unsigned int>(y >> upsampleLevel) ||
        (headerLevel != static_cast<unsigned int>(level + upsampleLevel) &&
         vt_genMissingPages.valueInteger != 1 &&
         !virtualTexture->UsesPartiallyResidentTextures()) ||
        pageCompression > COMP_JXR) {
      ++*transcodePageIndicesOffset;
      continue;
    }

    texturePage_t* physicalPage = nullptr;
    if (physicalPageNum < 0) {
      physicalPage = physicalPages->AllocPage(
          virtualTexture, coarser, quadrant, &unmapParms);
    } else {
      physicalPage = &physicalPages->pages[physicalPageNum];
    }
    if (lockPages && !physicalPage->locked) {
      physicalPage->locked = true;
      physicalPages->unlockedPageList.Remove(
          physicalPage->physicalPageNum);
      ++physicalPages->numLockedPages;
    }
    physicalPages->pagePriority[physicalPage->physicalPageNum] = priority;
    physicalPages->pageFrameNum[physicalPage->physicalPageNum] =
        feedbackFrameNum;
    physicalPage->pageCompression = pageCompression;
    if (upsampleLevel != 0) {
      std::memset(physicalPage->finerDiskOffset, 0,
                  sizeof(physicalPage->finerDiskOffset));
      std::memset(physicalPage->finerDiskLength, 0,
                  sizeof(physicalPage->finerDiskLength));
    } else {
      std::memcpy(physicalPage->finerDiskOffset,
                  pageHeader.finerDiskOffset,
                  sizeof(physicalPage->finerDiskOffset));
      std::memcpy(physicalPage->finerDiskLength,
                  pageHeader.finerDiskLength,
                  sizeof(physicalPage->finerDiskLength));
    }

    transcodeParms_t* parms =
        &jobs.transcodeParms[jobs.numTranscodeJobs++];
    std::memset(parms, 0, sizeof(*parms));
    const bool diffuseOnly =
        physicalPages->physicalPagesPool ==
            PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY ||
        physicalPages->physicalPagesPool ==
            PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2;
    parms->imageFlags = diffuseOnly ? 2 : 7;
    parms->layoutVersion = virtualTexture->layoutVersion;
    parms->numLevels = virtualTexture->header.numLevels;
    parms->pageSource = source;
    parms->pageLevel = level;
    parms->pageX = x;
    parms->pageY = y;
    parms->physicalPageX =
        physicalPage->physicalPageNum % physicalPages->PHYSICAL_PAGES_WIDE;
    parms->physicalPageY =
        physicalPage->physicalPageNum / physicalPages->PHYSICAL_PAGES_WIDE;
    parms->inPageCompression = pageCompression;
    parms->pageHeaderSize = sizeof(pageHeader_t);

    std::uint8_t* compressionHeader =
        reinterpret_cast<std::uint8_t*>(sourceHeader) + sizeof(pageHeader_t);
    if (pageCompression == COMP_DCT || pageCompression == COMP_HDP ||
        pageCompression == COMP_JXR) {
      parms->inPageHeader = compressionHeader;
      parms->inPageData = compressionHeader + 16;
      parms->inPageDataLength = diskLength - 56;
      SwapDCTDataLengths(
          *reinterpret_cast<DCTHeader_t*>(compressionHeader));
    } else {
      parms->inPageHeader = nullptr;
      parms->inPageData = compressionHeader;
      parms->inPageDataLength = diskLength - 40;
    }
    parms->outPageCompression =
        physicalPages->pageImageCompressed ? COMP_DXT : COMP_NONE;
    parms->outPageDataLength = outPageDataLength;
    parms->diskOffsetScale = diskOffsetScale;
    parms->targetBytePitch[0] =
        physicalPages->pageImageCompressed ? 256 : 512;
    parms->targetBytePitch[1] = 512;
    parms->targetBytePitch[2] = 512;
    parms->upsampleLevel = upsampleLevel;
    parms->upsampleX = upsampleX;
    parms->upsampleY = upsampleY;
    parms->upsampleFilter = vt_upsampleFilter.valueInteger;
    parms->upsampleSharpen =
        static_cast<std::uint16_t>(vt_upsampleSharpen.valueInteger);
    parms->upsampleNoise =
        static_cast<std::uint16_t>(vt_upsampleNoise.valueInteger);
    parms->stripPageBorders = vt_stripPageBorders.valueInteger != 0;
    parms->dbgShowPageSources = vt_showPageSources.valueInteger != 0;
    parms->dbgShowPageMips =
        static_cast<std::uint8_t>(vt_showPageMips.valueInteger);
    parms->dbgShowPageColors = vt_showPageColors.valueInteger != 0;
    parms->dbgShowPageNumbers = vt_showPageNumbers.valueInteger != 0;
    parms->dbgShowPageInfo = vt_showPageInfo.valueInteger != 0;
    parms->dbgShowPageBorders = vt_showPageBorders.valueInteger != 0;
    parms->dbgShowPageCheckered = vt_showPageCheckered.valueInteger != 0;
    parms->dbgShowPageSizes =
        static_cast<std::uint8_t>(vt_showPageSizes.valueInteger);
    if (vt_showPageUsage.valueInteger != 0) {
      std::uint64_t usage =
          UINT64_C(255) * renderSystem->GetHeight() * persistence *
          vt_showPageUsage.valueInteger * renderSystem->GetWidth();
      usage /= UINT64_C(120) * 120 * 160 * 120;
      parms->dbgShowPageUsage = static_cast<std::uint8_t>(
          std::clamp<std::uint64_t>(usage, 1, 255));
    }

    const void* pageDataPBO = nullptr;
    const std::uint8_t* pageData = nullptr;
    if (directMapped) {
      physicalPages->GetDirectMappedPhysicalPagePointers(parms);
    } else {
      parms->directmapped = 0;
      pageDataPBO = jobs.transcodePBO.apiObject;
      pageData = reinterpret_cast<const std::uint8_t*>(
          static_cast<std::uintptr_t>(pboOffset));
      parms->targetImage[0] =
          mappedPBO + pboOffset +
          (physicalPages->pageImageCompressed ? 0x8000 : 0x20000);
      parms->targetImage[1] = mappedPBO + pboOffset;
      parms->targetImage[2] =
          mappedPBO + pboOffset +
          (physicalPages->pageImageCompressed ? 0x4000 : 0x10000);
    }

    using transcodeRun_t = void (*)(const transcodeParms_t*);
    transcodeRun_t transcodeRun = TranscodePage;
    parms->tempSize = 196608;
    if (!debugTranscode &&
        physicalPages->physicalPagesPool != PHYSICAL_PAGES_POOL_PRT) {
      switch (parms->inPageCompression) {
        case COMP_DXT:
          transcodeRun = upsampleLevel == 0
                             ? TranscodePage_DXT_DXT
                             : TranscodePage_DXT_DXT_upsample;
          parms->tempSize = upsampleLevel == 0 ? 0 : 0x4900;
          parms->inPageDataLength = 40960;
          break;
        case COMP_DCT: {
          if (parms->outPageCompression == COMP_DXT) {
            transcodeRun = upsampleLevel == 0
                               ? TranscodePage_DCT_DXT
                               : TranscodePage_DCT_DXT_upsample;
            parms->tempSize = upsampleLevel == 0 ? 5120 : 21504;
          } else if (parms->outPageCompression == COMP_NONE) {
            transcodeRun = upsampleLevel == 0
                               ? TranscodePage_DCT_RAW
                               : TranscodePage_DCT_RAW_upsample;
            parms->tempSize = upsampleLevel == 0 ? 5120 : 21504;
          }
          const auto* header =
              reinterpret_cast<const DCTHeader_t*>(parms->inPageHeader);
          parms->inPageDataLength =
              header->diffuseSize + header->normalSize +
              header->specularSize + header->powerSize +
              ((header->flags & 8) << 8);
          break;
        }
        case COMP_HDP: {
          if (parms->outPageCompression == COMP_DXT) {
            transcodeRun = upsampleLevel == 0
                               ? TranscodePage_HDP_DXT
                               : TranscodePage_HDP_DXT_upsample;
          } else if (parms->outPageCompression == COMP_NONE) {
            transcodeRun = upsampleLevel == 0
                               ? TranscodePage_HDP_RAW
                               : TranscodePage_HDP_RAW_upsample;
          }
          parms->tempSize = upsampleLevel == 0 ? 54272 : 70656;
          const auto* header =
              reinterpret_cast<const HDPHeader_t*>(parms->inPageHeader);
          parms->inPageDataLength =
              header->diffuseSize + header->normalSize +
              header->specularSize + header->powerSize +
              ((header->flags & 8) << 8);
          break;
        }
        case COMP_JXR: {
          transcodeRun = upsampleLevel == 0
                             ? TranscodePage_JXR_DXT
                             : TranscodePage_JXR_DXT_upsample;
          parms->tempSize = upsampleLevel == 0 ? 136192 : 152576;
          const auto* header =
              reinterpret_cast<const JXRHeader_t*>(parms->inPageHeader);
          parms->inPageDataLength =
              header->diffuseSize + header->normalSize +
              header->specularSize + header->powerSize +
              ((header->flags & 8) << 8);
          break;
        }
        default:
          break;
      }
    }

    const bool splitJob =
        parms->imageFlags == 7 &&
        (transcodeRun == TranscodePage_DCT_DXT ||
         transcodeRun == TranscodePage_DCT_DXT_upsample ||
         transcodeRun == TranscodePage_HDP_DXT ||
         transcodeRun == TranscodePage_HDP_DXT_upsample ||
         transcodeRun == TranscodePage_JXR_DXT ||
         transcodeRun == TranscodePage_JXR_DXT_upsample) &&
        vt_splitTranscodeJobs.valueInteger != 0;
    if (splitJob) {
      transcodeParms_t* secondParms =
          &jobs.transcodeParms[jobs.numTranscodeJobs++];
      std::memcpy(secondParms, parms, sizeof(*parms));
      parms->imageFlags = 5;
      secondParms->imageFlags = 2;
      if (vt_skipTranscode.valueInteger == 0) {
        if (useTranscodeJobs == 1) {
          jobs.parallelJobList->AddJob(
              reinterpret_cast<jobRun_t>(transcodeRun), parms);
          jobs.parallelJobList->AddJob(
              reinterpret_cast<jobRun_t>(transcodeRun), secondParms);
        } else {
          transcodeRun(parms);
          transcodeRun(secondParms);
        }
      }
    } else if (vt_skipTranscode.valueInteger == 0) {
      if (useTranscodeJobs == 1) {
        jobs.parallelJobList->AddJob(
            reinterpret_cast<jobRun_t>(transcodeRun), parms);
      } else {
        transcodeRun(parms);
      }
    }

    sourceCursor = nextSource;
    if (!directMapped) {
      allDirectMapped = false;
      pboOffset += parms->outPageDataLength;
    }
    pageImagesUpdate_t* update =
        &imageUpdateParms.updates[imageUpdateParms.numUpdates++];
    physicalPages->CreatePageImagesUpdate(
        virtualTexture, physicalPage, physicalPage, pageDataPBO, pageData,
        update);
    ++numTranscodePages;

    if (unmapParms.numUpdates >= 256 ||
        imageUpdateParms.numUpdates >= 256 ||
        jobs.numTranscodeJobs >= 255 ||
        (!blocking && !lockPages &&
         numTranscodePages >= maxTranscodePagesPerFrame)) {
      advancePageIndex = false;
      completedBatch = false;
    }
    if (advancePageIndex) {
      ++*transcodePageIndicesOffset;
    } else {
      break;
    }
  }

  pageTranscodeCount += numTranscodePages;
  if (vt_skipUpdatePageImages.valueInteger != 0) {
    unmapParms.numUpdates = 0;
    imageUpdateParms.numUpdates = 0;
  }
  if (allDirectMapped) {
    if (useTranscodeJobs == 1) {
      jobs.parallelJobList->InsertSyncPoint(SYNC_SIGNAL);
      jobs.parallelJobList->InsertSyncPoint(SYNC_SYNCHRONIZE);
      jobs.parallelJobList->AddJob(
          reinterpret_cast<jobRun_t>(UpdatePageImagesJob),
          &imageUpdateParms);
    } else {
      UpdatePageImagesJob(&unmapParms);
      unmapParms.submittedJob = true;
      UpdatePageImagesJob(&imageUpdateParms);
    }
    imageUpdateParms.submittedJob = true;
  }
  jobs.nextFramePageImageUpload = true;
  streamFileCache->SignalWork();
  idParallelJobList* dependency =
      nextJobs.parallelJobList->TryWait() ? nullptr
                                          : nextJobs.parallelJobList;
  jobs.parallelJobList->Submit(dependency, -1);
  UpdateStats(transcodePageIndices, numTranscodePages,
              maxTranscodePagesPerFrame, totalStreamedPageData);
  if (blocking) {
    return false;
  }
  return completedBatch && allDataReady &&
         *transcodePageIndicesOffset >= totalNumPages;
}

void idVirtualTextureSystem::LockPhysicalPages(
    idVirtualTexture** virtualTextures, int numVirtualTextures,
    int maxLockedPages) {
  idVirtualTexture* localVirtualTextures[PAGESOURCE_TOTAL]{};
  if (virtualTextures == nullptr) {
    virtualTextures = localVirtualTextures;
    numVirtualTextures = 0;
    for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
      idVirtualTexture* virtualTexture = vtPtrs[source];
      if (virtualTexture->pageFilePath[0] != '\0' &&
          virtualTexture->physicalPages != nullptr &&
          virtualTexture->physicalPages->NUM_PHYSICAL_PAGES > 1) {
        virtualTextures[numVirtualTextures++] = virtualTexture;
      }
    }
  }

  int totalPagesForPool[PHYSICAL_PAGES_POOL_MAX]{};
  for (int index = 0; index < numVirtualTextures; ++index) {
    idVirtualTexture* virtualTexture = virtualTextures[index];
    if (virtualTexture->physicalPages != nullptr) {
      totalPagesForPool[virtualTexture->GetPhysicalPagesPool()] +=
          virtualTexture->header.totalPages;
    }
  }

  const int startTime = Sys_Milliseconds();
  if (maxLockedPages < 0) {
    maxLockedPages = vt_maxLockedPages.valueInteger;
  }
  if (analyzeJobList->IsSubmitted()) {
    analyzeJobList->Wait();
    currentPageIndices ^= 1u;
    numExplicitPages[currentPageIndices] = 0;
  }

  pageIndices_t* lockedPageIndices = pageIndices[currentPageIndices];
  std::memset(lockedPageIndices->pageStats, 0,
              sizeof(lockedPageIndices->pageStats));
  lockedPageIndices->totalNumPages = 0;
  int totalLockedPagesForPool[PHYSICAL_PAGES_POOL_MAX]{};
  int consideredPagesForPool[PHYSICAL_PAGES_POOL_MAX]{};
  for (int index = 0; index < numVirtualTextures; ++index) {
    idVirtualTexture* virtualTexture = virtualTextures[index];
    if (virtualTexture->physicalPages == nullptr) {
      continue;
    }
    const physicalPagesPool_t pool =
        virtualTexture->GetPhysicalPagesPool();
    if (totalPagesForPool[pool] == 0) {
      continue;
    }
    consideredPagesForPool[pool] += virtualTexture->header.totalPages;
    int numPages =
        maxLockedPages * consideredPagesForPool[pool] /
            totalPagesForPool[pool] -
        totalLockedPagesForPool[pool];
    numPages = std::max(numPages, 1);
    virtualTexture->GetToBeLockedPhysicalPages(numPages,
                                               lockedPageIndices);
    idLib::Printf("%3i pages locked in %s\n", numPages,
                  virtualTexture->pageFilePath.data);
    totalLockedPagesForPool[pool] += numPages;
  }

  FinishFeedback(true, 0);
  const int endTime = Sys_Milliseconds();
  int totalLockedPages = 0;
  for (int pool = 0; pool < PHYSICAL_PAGES_POOL_SHARED_COUNT; ++pool) {
    if (totalPagesForPool[pool] > 0) {
      totalLockedPages += totalLockedPagesForPool[pool];
      physicalPagesPool[pool].ValidateCache();
    }
  }
  idLib::Printf("%4.1f seconds to load and lock %i pages\n",
                static_cast<float>(endTime - startTime) * 0.001f,
                totalLockedPages);
}

void idVirtualTextureSystem::FinishFeedback(bool lockPages,
                                             int maxWaitTimeInSeconds) {
  const int startTime = Sys_Microseconds();
  if (analyzeJobList->IsSubmitted()) {
    analyzeJobList->Wait();
    currentPageIndices ^= 1u;
    numExplicitPages[currentPageIndices] = 0;
  }

  int pageIndicesOffset = 0;
  if (!TranscodeFeedback(pageIndices[currentPageIndices], &pageIndicesOffset,
                         true, lockPages)) {
    for (;;) {
      StreamFeedback(pageIndices[currentPageIndices]);
      if (cancelToTerminate) {
        idLib::Printf("VTS: CANCEL TO TERMINATE!\n");
        break;
      }
      if (maxWaitTimeInSeconds > 0 &&
          Sys_Microseconds() - startTime >
              1000000 * maxWaitTimeInSeconds) {
        idLib::Printf(
            "FinishFeedback: Terminated due to maximum time allowed.\n");
        break;
      }
      if (TranscodeFeedback(pageIndices[currentPageIndices],
                            &pageIndicesOffset, true, lockPages)) {
        break;
      }
    }
  }

  int numPages = static_cast<int>(
      pageIndices[currentPageIndices]->totalNumPages);
  TranscodeFeedback(pageIndices[currentPageIndices], &numPages, true,
                    lockPages);
  for (idPhysicalPages& pages : physicalPagesPool) {
    pages.feedbackDynamicLodBias = 0.0f;
  }
  const int endTime = Sys_Microseconds();
  idLib::Printf("%4.1f seconds for FinishFeedback()\n",
                static_cast<float>(endTime - startTime) * 0.000001f);
}

void idVirtualTextureSystem::PushFeedback(void* data, int width, int height,
                                          int bytePitch,
                                          bool forceHighestDetail) {
  RenderEventRecorderLightStartEvent_Impl("PushFeedback");
  RenderEventRecorderLightStartEvent_Impl("WaitForLastAnalyzeFeedback");
  if (analyzeJobList->IsSubmitted()) {
    analyzeJobList->Wait();
    currentPageIndices ^= 1u;
    numExplicitPages[currentPageIndices] = 0;
  }
  RenderEventRecorderLightStopEvent_Impl();

  RenderEventRecorderLightStartEvent_Impl("TranscodeFeedback");
  int pageIndicesOffset = 0;
  TranscodeFeedback(pageIndices[currentPageIndices], &pageIndicesOffset,
                    false, false);
  RenderEventRecorderLightStopEvent_Impl();

  RenderEventRecorderLightStartEvent_Impl("StreamFeedback");
  StreamFeedback(pageIndices[currentPageIndices]);
  RenderEventRecorderLightStopEvent_Impl();

  RenderEventRecorderLightStartEvent_Impl("AnalyzeFeedback");
  AnalyzeFeedback(data, width, height, bytePitch, forceHighestDetail);
  RenderEventRecorderLightStopEvent_Impl();
  if (flushNextFeedback) {
    flushNextFeedback = false;
    RenderEventRecorderLightStartEvent_Impl("FinishFeedback");
    FinishFeedback(false, 0);
    RenderEventRecorderLightStopEvent_Impl();
  }
  RenderEventRecorderLightStopEvent_Impl();
}

void idVirtualTextureSystem::Benchmark(int maxThreads) {
  if (benchmarkGraph != nullptr) {
    console->DestroyGraph(benchmarkGraph);
    benchmarkGraph = nullptr;
    return;
  }

  con_noPrint.SetBool(false, true);
  vt_skipUpdatePageImages.SetBool(true, true);
  const int oldNumThreads = jobs_numThreads.valueInteger;
  parallelJobManager->WaitForAllJobLists();
  maxThreads = std::clamp(maxThreads, 1, 32);
  float times[33]{};
  float maxTime = 0.0f;
  for (int numThreads = 1; numThreads <= maxThreads; ++numThreads) {
    jobs_numThreads.SetInteger(numThreads, true);
    std::uint64_t bestTime = UINT64_C(0x4000000000000000);
    for (int trial = 0; trial < 4; ++trial) {
      for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
        vtPtrs[source]->EmptyCache();
      }
      for (idPhysicalPages& pages : physicalPagesPool) {
        pages.EmptyCache();
      }
      emptyCacheTime = Sys_Milliseconds();
      const std::uint64_t startTime = Sys_Microseconds();
      int pageIndicesOffset = 0;
      while (!TranscodeFeedback(pageIndices[currentPageIndices],
                                &pageIndicesOffset, true, false)) {
        StreamFeedback(pageIndices[currentPageIndices]);
      }
      bestTime = std::min(bestTime,
                          static_cast<std::uint64_t>(Sys_Microseconds()) -
                              startTime);
    }
    const float seconds = static_cast<float>(bestTime) / 1000000.0f;
    times[numThreads] = seconds;
    maxTime = std::max(maxTime, seconds);
    idLib::Printf(
        "%1.2f seconds to populate virtual texture cache using %d "
        "threads\n",
        seconds, numThreads);
  }

  const int width = renderSystem->GetWidth();
  const int height = renderSystem->GetHeight();
  benchmarkGraph = console->CreateGraph(maxThreads);
  benchmarkGraph->bgColor = idColor::colorBlack;
  benchmarkGraph->sideways = true;
  benchmarkGraph->position = {
      static_cast<float>(width / 2 - width / 4),
      static_cast<float>(height / 2 - height / 8),
      static_cast<float>(width / 2), static_cast<float>(height / 4)};
  benchmarkGraph->enable = true;
  for (int index = 0; index < maxThreads; ++index) {
    const float seconds = times[index + 1];
    const idStr label(
        va("%d threads (%1.2f seconds)", index + 1, seconds));
    benchmarkGraph->SetValue(index, seconds / maxTime,
                             idColor::colorRed);
    benchmarkGraph->SetLabel(index, label.data);
  }

  vt_skipUpdatePageImages.SetBool(false, true);
  jobs_numThreads.SetInteger(oldNumThreads, true);
  for (int source = 1; source < PAGESOURCE_TOTAL; ++source) {
    vtPtrs[source]->EmptyCache();
  }
  for (idPhysicalPages& pages : physicalPagesPool) {
    pages.EmptyCache();
  }
  emptyCacheTime = Sys_Milliseconds();
}

bool idVirtualTextureSystem::RunTranscodeBenchmark(
    float* megaTexelsPerSecond, float* score) {
  *megaTexelsPerSecond = 0.0f;
  *score = 0.0f;
  const idStr fileName("c01p1");
  idStr pageFilePath;
  pageFilePath.Format("%s/%s.pages", vt_filePath.valueString.data,
                      fileName.data);
  if (fileSystem->GetTimestamp(pageFilePath.data, false) ==
      std::numeric_limits<std::uint32_t>::max()) {
    idLib::Printf("'%s' does not exist!\n", pageFilePath.data);
    return false;
  }

  int source = 1;
  while (source < PAGESOURCE_TOTAL &&
         vts[source].flavor != ID_VTF_UNKNOWN) {
    ++source;
  }
  if (source >= PAGESOURCE_TOTAL || vtPtrs[source] == nullptr) {
    return false;
  }
  idVirtualTexture* virtualTexture = vtPtrs[source];
  if (!virtualTexture->SetSource(fileName.data, pageFilePath.data,
                                 PHYSICAL_PAGES_POOL_VMTR, ID_VTF_VMTR)) {
    return false;
  }

  idScopeCVar skipUpdatePageImages(&vt_skipUpdatePageImages, true);
  if (vt_genMissingPages.valueInteger == 1) {
    vt_genMissingPages.SetInteger(2, true);
  }
  parallelJobManager->WaitForAllJobLists();
  WaitForLastAnalyzeFeedback();
  pageIndices_t* benchmarkIndices = pageIndices[currentPageIndices];
  std::memset(benchmarkIndices->pageStats, 0,
              sizeof(benchmarkIndices->pageStats));
  benchmarkIndices->totalNumPages = 0;
  virtualTexture->GetToBeLockedPhysicalPages(1024, benchmarkIndices);

  std::uint64_t bestTime = UINT64_C(0x4000000000000000);
  int bestTranscodeCount = 0;
  for (int trial = 0; trial < 4; ++trial) {
    for (int vtSource = 1; vtSource < PAGESOURCE_TOTAL; ++vtSource) {
      vtPtrs[vtSource]->EmptyCache();
    }
    for (idPhysicalPages& pages : physicalPagesPool) {
      pages.EmptyCache();
    }
    emptyCacheTime = Sys_Milliseconds();
    const std::uint64_t startTime = Sys_Microseconds();
    pageTranscodeCount = 0;
    int pageIndicesOffset = 0;
    while (!TranscodeFeedback(pageIndices[currentPageIndices],
                              &pageIndicesOffset, true, false)) {
      StreamFeedback(pageIndices[currentPageIndices]);
    }
    const std::uint64_t elapsed =
        static_cast<std::uint64_t>(Sys_Microseconds()) - startTime;
    if (elapsed < bestTime) {
      bestTime = elapsed;
      bestTranscodeCount = pageTranscodeCount;
    }
  }

  const float rate = bestTime != 0
                         ? static_cast<float>(bestTranscodeCount << 14) /
                               static_cast<float>(bestTime)
                         : 999.0f;
  *megaTexelsPerSecond = std::clamp(rate, 0.0f, 999.0f);
  *score = std::clamp(*megaTexelsPerSecond * 0.01f, 0.0f, 1.0f);
  const char* rating = "EXCELLENT";
  if (*score <= 0.8f) {
    rating = *score <= 0.6f
                 ? (*score <= 0.4f
                        ? (*score <= 0.2f ? "POOR" : "FAIR")
                        : "GOOD")
                 : "GREAT";
  }

  int numLogicalCPUCores = 0;
  int numPhysicalCPUCores = 0;
  int numCPUPackages = 0;
  Sys_CPUCount(&numLogicalCPUCores, &numPhysicalCPUCores,
               &numCPUPackages);
  idStr text;
  text += va("CPU type = %s\n", Sys_GetProcessorString());
  text += va("CPU frequency = %1.2f MHz\n",
             static_cast<float>(Sys_ClockCyclesPerSecond()) /
                 1000000000.0f);
  text += va("CPU physical cores = %d\n", numPhysicalCPUCores);
  text += va("CPU logical cores = %d\n", numLogicalCPUCores);
  text += va("CPU threads = %d\n", jobs_numThreads.valueInteger);
  text += va("Max PPF = %d\n", vt_maxPPF.valueInteger);
  text += "----------------\n";
  text += va("%1.0f Mega Texels per Second\n", *megaTexelsPerSecond);
  text += va("Benchmark Score: %s\n", rating);
  idLib::Printf("%s", text.data);
  Sys_SetClipboardData(text.data);

  if (vt_genMissingPages.valueInteger == 2) {
    vt_genMissingPages.SetInteger(1, true);
  }
  virtualTexture->ReleaseSource();
  for (int vtSource = 1; vtSource < PAGESOURCE_TOTAL; ++vtSource) {
    vtPtrs[vtSource]->EmptyCache();
  }
  for (idPhysicalPages& pages : physicalPagesPool) {
    pages.EmptyCache();
  }
  emptyCacheTime = Sys_Milliseconds();
  return true;
}

void idVirtualTextureSystem::Benchmark_f(const idCmdArgs& args) {
  virtualTextureSystem.Benchmark(
      args.argc <= 1 ? 6 : static_cast<int>(std::atol(args.argv[1])));
}

void idVirtualTextureSystem::TranscodeBenchmark_f(const idCmdArgs&) {
  float megaTexelsPerSecond = 0.0f;
  float score = 0.0f;
  virtualTextureSystem.RunTranscodeBenchmark(&megaTexelsPerSecond, &score);
}

void idVirtualTextureSystem::LockPages_f(const idCmdArgs&) {
  virtualTextureSystem.LockPhysicalPages(nullptr, 0, -1);
}

bool idVirtualTextureSystem::ReloadVirtualTextures() {
  SyncFeedback();
  idVirtualTexture* virtualTextures[PAGESOURCE_TOTAL]{};
  int numVirtualTextures = 0;
  bool allocationFailed = false;
  for (int index = 1; index < PAGESOURCE_TOTAL; ++index) {
    idVirtualTexture* virtualTexture = vtPtrs[index];
    if (virtualTexture->flavor == ID_VTF_UNKNOWN) {
      continue;
    }
    const idStr baseName(virtualTexture->baseName.data);
    const idStr pageFilePath(virtualTexture->pageFilePath.data);
    const physicalPagesPool_t physicalPagesPool =
        virtualTexture->GetPhysicalPagesPool();
    const bool useSystemHeap =
        virtualTexture->flavor == ID_VTF_VMTR &&
        !virtualTexture->isMapVmtr;
    if (useSystemHeap) {
      idMem::Singleton()->PushHeap(HEAP_SYSTEMHEAP);
    }
    const virtualTextureFlavor_t flavor =
        virtualTexture->flavor == ID_VTF_VMTR ? ID_VTF_VMTR : ID_VTF_BAKE;
    virtualTexture->SetSource(baseName.data, pageFilePath.data,
                              physicalPagesPool, flavor);
    if (useSystemHeap) {
      idMem::Singleton()->PopHeap();
    }
    allocationFailed |= virtualTexture->enlargedPageTableAllocationFailed;
    virtualTextures[numVirtualTextures++] = virtualTexture;
  }
  LockPhysicalPages(virtualTextures, numVirtualTextures, 21);
  SetupLoadedVirtualMaterials();
  return !allocationFailed;
}

void idVirtualTextureSystem::Reload_f(const idCmdArgs&) {
  if (!virtualTextureSystem.ReloadVirtualTextures()) {
    idLib::Warning(
        "failed to allocate larger page tables for more texture detail");
  }
}

idCodeResource<idDeclRenderParm> rpLandLayoutVersion("landLayoutVersion");
idCodeResource<idDeclRenderParm> rpVirtualTextureOptions(
    "virtualTextureOptions");
idCodeResource<idDeclRenderParm> rpPhysicalVmtrFilterParms(
    "physicalVmtrFilterParms");
idCodeResource<idDeclRenderParm> rpPhysicalUniqueFilterParms(
    "physicalUniqueFilterParms");
idCodeResource<idDeclRenderParm> rpPhysicalUniqueDiffuseOnlyFilterParms(
    "physicalUniqueDiffuseOnlyFilterParms");
idCodeResource<idDeclRenderParm> rpPhysicalUniqueDiffuseOnly2FilterParms(
    "physicalUniqueDiffuseOnly2FilterParms");
idCodeResource<idDeclRenderParm> rpPhysicalPRTFilterParms(
    "physicalPRTFilterParms");

idCVar vt_touchPhysCacheOnAlloc(
    "vt_touchPhysCacheOnAlloc", "1", 1,
    "touch the vt physical images right after allocation, we don't really "
    "have the texture otherwise on amd");
idCVar vt_linearPhysicalImages("vt_linearPhysicalImages", "0", 1, "");
idCVar vt_uncompressedPhysicalImages(
    "vt_uncompressedPhysicalImages", "0", 65553,
    "Use uncompressed physical page images. vt_restart after changing this.");
idCVar vt_maxAniso("vt_maxAniso", "4", 20,
                   "Floating point max ansio for physical textures.", 1.0f,
                   4.0f);
idCVar vt_filter(
    "vt_filter", "2", 18,
    "filter physical images, 0 = off, 1 = bilinear, 2 = trilinear", 0.0f,
    2.0f);
idCVar vt_pageBorderAdjust(
    "vt_pageBorderAdjust", "0", 20,
    "Debug adjustment of a page's mapping about it's center");
idCVar vt_feedbackLodBias("vt_feedbackLodBias", "0", 20,
                          "Additional floating point bias for feedback LOD");
idCVar vt_feedbackHighWater(
    "vt_feedbackHighWater", "90", 20,
    "More than this percentage of pages needed this frame and we consider "
    "ourselves oversubscribed.");
idCVar vt_feedbackLowWater(
    "vt_feedbackLowWater", "80", 20,
    "Less than this percentage of pages needed this frame and we consider "
    "ourselves undersubscribed.");
idCVar vt_feedbackLODIncrement(
    "vt_feedbackLODIncrement", "0.125", 20,
    "Base increment value for adjusting dynamic LOD bias.");
idCVar vt_lodBias("vt_lodBias", "0", 20,
                  "Additional floating point bias for virtual texture page "
                  "tables");
idCVar vt_useDirectTextureAccess(
    "vt_useDirectTextureAccess", "1", 17,
    "Use direct texture access extension on the PC if available.");
idCVar vt_maxPhysicalPRTPages("vt_maxPhysicalPRTPages", "4096", 18,
                              "Maximum number of resident PRT pages.");
idCVar vt_enablePRT("vt_enablePRT", "0", 1, "Set to enable PRT mode");
idCVar vt_readOnlyNetPages(
    "vt_readOnlyNetPages", "1", 1,
    "Set to always open network pagefiles in readonly mode (BHVR need this)");

#if !defined(WOLF_RENDERER_VIRTUALTEXTURESYSTEM_TEST_ACCESS)
idVirtualTextureSystem virtualTextureSystem;
#endif

idCVar vt_useNonBufferedIO("vt_useNonBufferedIO", "0", 17,
                           "use non-buffered IO");
idCVar vt_showPageLoads("vt_showPageLoads", "0", 18,
                        "Print each page that is loaded");
idCVar vt_showFeedbackAnalysis("vt_showFeedbackAnalysis", "0", 18,
                               "Report all the unique pages visible");
idCVar vt_showFeedbackBuffer("vt_showFeedbackBuffer", "0", 18,
                             "visualize the feedback buffer");
idCVar vt_showFeedbackStreaming(
    "vt_showFeedbackStreaming", "0", 18,
    "Display the feedback pages as they are streamed in");
idCVar vt_uncompressedVmtr(
    "vt_uncompressedVmtr", "0", 65553,
    "Use uncompressed vmtr data. Only read at startup.");
idCVar vt_maxLockedPages("vt_maxLockedPages", "256", 18,
                         "Don't lock more than pages than this.");
idCVar vt_skipStreaming(
    "vt_skipStreaming", "0", 17,
    "Skip streaming but still transcode and update the page tables");
idCVar vt_skipTranscode("vt_skipTranscode", "0", 17,
                        "Skip transcoding (but don't skip reading)");
idCVar vt_skipUpdatePageImages(
    "vt_skipUpdatePageImages", "0", 17,
    "Skip updating page table and physical images on the PC (but still "
    "read/transcode)");
idCVar vt_nopreload("vt_nopreload", "0", 17, "");
idCVar vt_minLod("vt_minLod", "0", 18,
                 "Set the finest mip that can be accessed.");
idCVar vt_showStats("vt_showStats", "0", 17,
                    "show virtual texture system statistics");
idCVar vt_noSeekCacheLines(
    "vt_noSeekCacheLines", "4", 18,
    "number of cache lines we can skip without seeking");
idCVar vt_recordStats(
    "vt_recordStats", "", 16,
    "record virtual texture system statistics to the given file name");
idCVar vt_singleStep("vt_singleStep", "-1", 18,
                     "single step through the transcode process");
idCVar vt_splitTranscodeJobs(
    "vt_splitTranscodeJobs", "1", 17,
    "split transcode jobs into two jobs per page");
idCVar vt_usePageTablePBOs(
    "vt_usePageTablePBOs", "1", 17,
    "use the PBOs for page table updates on the PC");
idCVar vt_useUnsynchronizedPageTablePBOs(
    "vt_useUnsynchronizedPageTablePBOs", "0", 17,
    "use unsynchronized PBOs for page table updates on the PC");
idCVar vt_useInvalidateRangeBitForMappingAndPageTablePBOMap(
    "vt_useInvalidateRangeBitForMappingAndPageTablePBOMap", "0", 17,
    "use the GL_MAP_INVALIDATE_RANGE_BIT for PBO map for mapping and page "
    "table updates on the PC");
idCVar vt_skipPhysicalTextureUploads("vt_skipPhysicalTextureUploads", "0",
                                     17, "");
idCVar vt_virtualSizeLog2(
    "vt_virtualSizeLog2", "11", 18,
    "Log2 of the virtualTexture dimensions for general texture allocation. "
    "Must be set on the command line.");
idCVar vt_upsampleFilter(
    "vt_upsampleFilter", "3", 18,
    "0 = nearest, 1 = bilinear, 2 = biquadratic, 3 = bicubic, 4 = "
    "sinc+hamming, 5 = sinc+blackman, 6 = sinc+lanczos, 7 = sinc+kaiser");
idCVar vt_upsampleSharpen(
    "vt_upsampleSharpen", "96", 18,
    "amount of sharpening to apply to upsampled texture pages", 0.0f,
    255.0f);
idCVar vt_upsampleNoise(
    "vt_upsampleNoise", "3", 18,
    "amount of noise to add to upsampled texture pages", 0.0f, 255.0f);
idCVar vt_showPageSources("vt_showPageSources", "0", 17,
                          "Shades vmtr and unique pages differently");
idCVar vt_stripPageBorders("vt_stripPageBorders", "0", 17,
                           "Strip the page border by upscaling the center");
idCVar vt_showPageMips(
    "vt_showPageMips", "0", 18,
    "Colorize pages by virtual mip level, if > 1, only color pages beyond "
    "that level-1");
idCVar vt_showPageColors("vt_showPageColors", "0", 17,
                         "use a random color for each page");
idCVar vt_showPageNumbers("vt_showPageNumbers", "0", 17,
                          "Draw page numbers");
idCVar vt_showPageInfo(
    "vt_showPageInfo", "0", 17,
    "Shows page info: source #, layout #, compression, mono/color specular");
idCVar vt_showPageBorders("vt_showPageBorders", "0", 17,
                          "Shows the page borders");
idCVar vt_showPageCheckered(
    "vt_showPageCheckered", "0", 17,
    "Draw a checkered pattern on every page to check how the mip selection "
    "works");
idCVar vt_showPageSizes(
    "vt_showPageSizes", "0", 18,
    "Draw page sizes (1 = show size in bytes, 2 = color green to red from "
    "small to large)");
idCVar vt_showPageUsage(
    "vt_showPageUsage", "0", 18,
    "color each page from red to green based on the percentage of the page "
    "texels that are actually visible");
idCVar vt_pageImageSizeVmtr(
    "vt_pageImageSizeVmtr", "4096", 65554,
    "Dimensions of the physical page images. vt_restart after changing this.");
idCVar vt_pageImageSizeUnique(
    "vt_pageImageSizeUnique", "4096", 65554,
    "Dimensions of the physical page images. vt_restart after changing this.");
idCVar vt_pageImageSizeUniqueDiffuseOnly(
    "vt_pageImageSizeUniqueDiffuseOnly", "4096", 65554,
    "Dimensions of the physical page images.");
idCVar vt_pageImageSizeUniqueDiffuseOnly2(
    "vt_pageImageSizeUniqueDiffuseOnly2", "4096", 65554,
    "Dimensions of the physical page images.");
idCVar vt_useAnalyzeJobs("vt_useAnalyzeJobs", "1", 18,
                         "0 = off, 1 = threaded", 0.0f, 1.0f);
idCVar vt_usePageSortJobs("vt_usePageSortJobs", "1", 18,
                          "0 = off, 1 = threaded", 0.0f, 1.0f);
idCVar vt_useTranscodeJobs("vt_useTranscodeJobs", "1", 18,
                           "0 = off, 1 = threaded", 0.0f, 1.0f);
idCVar vt_maxPPF("vt_maxPPF", "64", 18,
                 "maximum number of transcoded pages per frame", 0.0f,
                 128.0f);
idCVar vt_genMissingPages(
    "vt_genMissingPages", "1", 18,
    "Gen a page in memory for pages that don't exist on disk.", 0.0f, 2.0f);
idCVar vt_extraUpSampleLevels(
    "vt_useUpsample", "1", 18,
    "Number of extra levels in vt to use for upsample", 0.0f, 2.0f);
idCVar vt_useCudaTranscode(
    "vt_useCudaTranscode", "0", 65554,
    "controls Cuda texture transcoding: 0 - off, 1 - smart, 2 - on");
idCVar mt_sizeOverride(
    "mt_sizeOverride", "0", 2,
    "Override the default landscape resolution, specified in kiloTexels");
idCVar vt_skipExplicitPageFeedback(
    "vt_skipExplicitPageFeedback", "0", 17,
    "don't append explicit pages from gui surfaces.");
idCVar vt_showExplicitPages(
    "vt_showExplicitPages", "0", 17,
    "Report the number of explicit pages each frame.");
idCVar vt_containers("vt_containers", "0", 17,
                     "Set to enable loading from containers");
idCVar vt_packPageFiles(
    "vt_packPageFiles", "0", 18,
    "Use the virtual texture file name without any suffixes, 0 = version "
    "vmtr and megas, 1 = version megas, 2 = version none ");
idCVar vt_useNetworkPagesWithPreload(
    "vt_useNetworkPagesWithPreload", "0", 17,
    "Use the network page files and vmtr even when using linear preload "
    "files");
idCVar vt_filePath("vt_filePath", "", 16, "Path for the virtual texture");
idCVar vt_filePathVmtrOverride(
    "vt_filePathVmtrOverride", "generated/pagefiles", 16,
    "Optionally get vmtr files from a different source than vt_filePath");
idCVar vt_createPagesFile(
    "vt_createPagesFile", "1", 0x4000,
    "1 = Create a new pages file if there is none for the specified "
    "material, 2 = allow creation of vmtr pages file but not unique");
