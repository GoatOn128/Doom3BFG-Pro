#include "engine/renderer/virtualtexture.h"

#include "engine/framework/session.h"
#include "engine/framework/streamfilecache.h"
#include "engine/renderer/image.h"
#include "engine/renderer/imagedata.h"
#include "engine/renderer/imagemanager.h"
#include "shared/idlib/csystems/cmdsystemlocal.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <new>

extern idCVar feature_lowgore;
extern idCVar r_skipVmtrs;
extern idCVar vt_lodBias;
extern idCVar vt_uncompressedVmtr;

const char* compressionStrings[] = {"none", "DXT", "DCT", "HDP", "JXR",
                                    nullptr};
pageCompression_t compressionForString[] = {COMP_NONE, COMP_DXT, COMP_DCT,
                                            COMP_HDP, COMP_JXR};
const char* physicalPoolNames[] = {
    "PHYSICAL_PAGES_POOL_VMTR",
    "PHYSICAL_PAGES_POOL_UNIQUE",
    "PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY",
    "PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2",
    "PHYSICAL_PAGES_POOL_SHARED_COUNT",
    "PHYSICAL_PAGES_POOL_PRT",
};

idCVar vt_useMinLodTexture(
    "vt_useMinLodTexture", "0", 17,
    "Use a min-LOD texture to gradually adjust the LOD when new texture "
    "pages become resident");
idCVar vt_usePageMinLodJobs("vt_usePageMinLodJobs", "1", 18,
                            "0 = off, 1 = threaded", 0.0f, 1.0f);
idCVar vt_pageFadeInSteps("vt_pageFadeInSteps", "3", 18, "");
idCVar vt_usePageFileCache("vt_usePageFileCache", "1", 17,
                           "Use the pageFileCache for all reads");
idCVar vt_minMipLevelInstalled(
    "vt_minMipLevelInstalled", "0", 18,
    "minimum mip level installed on the hard disk (2 = all mips except "
    "finest two installed, 16 = no mips installed)",
    0.0f, 16.0f);
idCVar vt_supportRoundedDownVmtrs(
    "vt_supportRoundedDownVmtrs", "0", 2,
    "quick temporary fix to load in the 4x1 pages properly");
idCVar mt_genPageCompression(
    "mt_genPageCompression", "HDP", 0,
    "Compression method for generated pages.", compressionStrings,
    static_cast<argCompletionRef_t>(
        idCmdSystem::ArgCompletion_String<compressionStrings>));
idCVar mt_genVmtrCompression(
    "mt_genVmtrCompression", "HDP", 0,
    "Compression method for generated Vmtrs.", compressionStrings,
    static_cast<argCompletionRef_t>(
        idCmdSystem::ArgCompletion_String<compressionStrings>));
idCVar vt_logPageWrites(
    "vt_logPageWrites", "0", 17,
    "write out log file entries when virtual texture pages are created and "
    "written");
idCVar vt_stopOnWriteErrors("vt_stopOnWriteErrors", "0", 17,
                            "error out when encountering write errors");
idCVar vt_showLockedPages("vt_showLockedPages", "0", 17,
                          "Colorize locked pages");

namespace {

constexpr int PAGE_FILE_MAGIC = 1999870212;
constexpr int PAGE_SUBROOT_MAGIC = -893391612;

void ClearPageFileHeader(pageFileHeader_t& header) {
  std::memset(&header, 0, sizeof(header));
}

int FloatExponentLevel(int value, int bias) {
  const float asFloat = static_cast<float>(value);
  std::uint32_t bits = 0;
  std::memcpy(&bits, &asFloat, sizeof(bits));
  return static_cast<int>((bits >> 23) & 0xFFu) - bias;
}

void* RetailAlloc(const char* location, std::size_t size, memTag_t tag,
                  bool zero, align_t alignment) {
  return idMem::Singleton()->AllocWithLocation(
      location, size, tag, zero, alignment, HEAP_DEFAULTHEAP);
}

void RetailFree(void* pointer, align_t alignment = ALIGN_16) {
  if (pointer != nullptr) {
    idMem::Singleton()->Free(pointer, alignment);
  }
}

void FreeImageData(idImageData* imageData) {
  if (imageData == nullptr) {
    return;
  }
  RetailFree(imageData->data, ALIGN_16);
  RetailFree(imageData->floatData, ALIGN_16);
  RetailFree(imageData, ALIGN_16);
}

void SetupMipWidths(idVirtualTexture& texture) {
  for (unsigned int level = 0; level < PAGESOURCE_TOTAL; ++level) {
    texture.mipLevelPagesWide[level] =
        static_cast<unsigned short>(texture.header.pagesWide >> level);
  }
}

idImageOpts PageTableImageOpts(const idVirtualTexture& texture) {
  idImageOpts opts{};
  opts.textureType = TT_2D;
  opts.format = FMT_RG8;
  opts.colorFormat = CFM_DEFAULT;
  opts.width = texture.header.pagesWide;
  opts.height = texture.header.pagesWide;
  opts.numLevels = texture.header.numLevels;
  opts.packedTail = true;
  opts.linear = true;
  opts.sampler.filter = TF_NEAREST_MIPMAP_NEAREST;
  opts.sampler.repeat = TR_CLAMP;
  opts.sampler.lodBias = vt_lodBias.valueFloat;
  opts.sampler.lodMaxClamp = 6.0f;
  return opts;
}

idImageOpts PageMinLodImageOpts(const idVirtualTexture& texture) {
  idImageOpts opts{};
  opts.textureType = TT_2D;
  opts.format = FMT_INT8;
  opts.colorFormat = CFM_DEFAULT;
  opts.width = texture.header.pagesWide;
  opts.height = texture.header.pagesWide;
  opts.numLevels = texture.header.numLevels;
  opts.packedTail = true;
  opts.linear = true;
  opts.sampler.filter = TF_NEAREST_MIPMAP_NEAREST;
  opts.sampler.repeat = TR_CLAMP;
  opts.sampler.lodMaxClamp = 13.0f;
  return opts;
}

}  // namespace

pageCompression_t PageCompressionForString(const char* string) {
  for (int index = 0; compressionStrings[index] != nullptr; ++index) {
    if (idStr::Icmp(string, compressionStrings[index]) == 0) {
      return compressionForString[index];
    }
  }
  return COMP_DCT;
}

idStr idStr::GetFileBase() const {
  idStr result;
  ExtractFileBase(result);
  return result;
}

idBackgroundVmtrUpdater::idBackgroundVmtrUpdater(idVMTRData* vmtrData_)
    : vmtrData(vmtrData_), isDoingWork(false) {}

idVMTRData::idVMTRData(idVirtualTexture* virtualTexture)
    : vt(virtualTexture), vmtrHash(1024, 1024) {
  vmtrHash.memTag = TAG_HASHINDEX;
}

idVMTRData::~idVMTRData() = default;

idVirtualTexture::idVirtualTexture()
    : flavor(ID_VTF_UNKNOWN), vmtrData(nullptr), pageSource(PAGESOURCE_INVALID),
      layoutVersion(0), storedNumLevels(0), pageFile(nullptr),
      pageFileInstalled(nullptr), minMipLevelInstalled(16),
      VIRTUAL_PAGES_WIDE_LOG2(0), VIRTUAL_TEXELS_WIDE(0),
      VIRTUAL_MAPPING_PAGES_WIDE(0), physicalPages(nullptr),
      rootTexturePage(nullptr), pageTableImage(nullptr),
      pageMinLodImage(nullptr), pageTableImageShadow(nullptr),
      currentPageMinLodPBO(0), pageMinLodParms(nullptr),
      enlargedPageTableAllocationFailed(false), isDirectMapped(false),
      skipFlush(false), isMapVmtr(false) {
  ClearPageFileHeader(header);
  std::memset(levelResidency, 0, sizeof(levelResidency));
}

idVirtualTexture::~idVirtualTexture() { FreeData(); }

void idVirtualTexture::EmptyCache() {
  physicalPages->FreePageTree_r(this, rootTexturePage, nullptr);
}

bool idVirtualTexture::UsesPartiallyResidentTextures() const {
  return physicalPages->physicalPagesPool == PHYSICAL_PAGES_POOL_PRT;
}

physicalPagesPool_t idVirtualTexture::GetPhysicalPagesPool() const {
  return physicalPages->physicalPagesPool;
}

void idVirtualTexture::SetEmpty(const char*, int pagesWide,
                                int diskOffsetScale) {
  ClearPageFileHeader(header);
  header.pagesWide = pagesWide;
  header.magic = PAGE_FILE_MAGIC;
  header.subRoot.magic = PAGE_SUBROOT_MAGIC;
  header.diskOffsetScale = diskOffsetScale;
  header.numLevels = static_cast<short>(FloatExponentLevel(pagesWide, 126));
  header.layoutVersion = 0;

  for (int level = 0;; ++level) {
    const int levelPagesWide = std::max(1, pagesWide >> level);
    header.totalPages += levelPagesWide * levelPagesWide;
    if (levelPagesWide == 1) {
      break;
    }
  }

  header.totalFileSize = sizeof(pageFileHeader_t);
  VIRTUAL_MAPPING_PAGES_WIDE = pagesWide;
  storedNumLevels = header.numLevels;
  VIRTUAL_PAGES_WIDE_LOG2 = FloatExponentLevel(pagesWide, 127);
  VIRTUAL_TEXELS_WIDE = 120 * pagesWide;
  SetupMipWidths(*this);
  physicalPages = &virtualTextureSystem.physicalPagesPool[
      flavor == ID_VTF_VMTR ? PHYSICAL_PAGES_POOL_VMTR
                            : PHYSICAL_PAGES_POOL_UNIQUE];
}

void idVirtualTexture::GetToBeLockedPhysicalPages(
    int numTexturePages, pageIndices_t* pageIndices) {
  int remaining = numTexturePages;
  unsigned int pagesWide = 1;
  unsigned int level = static_cast<unsigned int>(header.numLevels - 1);

  while (remaining > 0) {
    for (unsigned int y = 0; y < pagesWide && remaining > 0; ++y) {
      for (unsigned int x = 0; x < pagesWide && remaining > 0; ++x) {
        --remaining;
        pageIndices->indices[pageIndices->totalNumPages++] =
            4ull *
            (y | ((static_cast<std::uint64_t>(x) |
                   ((level |
                     (16ull * ((static_cast<std::uint64_t>(level) << 20) |
                               static_cast<unsigned int>(pageSource) |
                               0x10ull)))
                    << 11))
                  << 11));
      }
    }
    if (level == 0) {
      break;
    }
    --level;
    pagesWide *= 2;
  }
}

void idVirtualTexture::Print() {
  if (pageFile == nullptr) {
    idLib::Printf("No source set.\n");
    return;
  }

  if (pageTableImage != nullptr) {
    pageTableImage->Print();
  }

  switch (physicalPages->physicalPagesPool) {
    case PHYSICAL_PAGES_POOL_VMTR:
      idLib::Printf("pool: VMTR\n");
      break;
    case PHYSICAL_PAGES_POOL_UNIQUE:
      idLib::Printf("pool: specular\n");
      break;
    case PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY:
      idLib::Printf("pool: diffuse only 1\n");
      break;
    case PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2:
      idLib::Printf("pool: diffuse only 2\n");
      break;
    case PHYSICAL_PAGES_POOL_PRT:
      idLib::Printf("pool: PRT\n");
      break;
    default:
      break;
  }

  idLib::Printf("Page file: %s\n", pageFilePath.data);
  idLib::Printf("header.magic: 0x%x\n", header.magic);
  idLib::Printf("header.pagesWide: %4i\n", header.pagesWide);
  idLib::Printf("header.numLevels: %4i\n", header.numLevels);
  idLib::Printf("header.layoutVersion: %4i\n", header.layoutVersion);
  idLib::Printf("header.diskOffsetScale: %4i\n", header.diskOffsetScale);
  idLib::Printf("Pages resident per level: ");
  for (int level = 0; level < VIRTUAL_PAGES_WIDE_LOG2 + 1; ++level) {
    idLib::Printf("%3d ", levelResidency[level]);
  }
  idLib::Printf("\n");
  idLib::Printf("Pages resident: %i\n",
                physicalPages->GetNumResidentPagesForSource(pageSource));
  idLib::Printf("Pages locked: %i\n",
                physicalPages->GetNumLockedPagesForSource(pageSource));
}

pageCompression_t idVirtualTexture::GetNewDiskPageCompression() {
  if (forcedPageCompression != COMP_INVALID) {
    return forcedPageCompression;
  }
  if (std::strstr(pageFilePath.data, "_U.") != nullptr) {
    return COMP_NONE;
  }
  if (flavor == ID_VTF_VMTR) {
    if (vt_uncompressedVmtr.valueInteger != 0) {
      return COMP_NONE;
    }
    return PageCompressionForString(mt_genVmtrCompression.valueString.data);
  }
  return PageCompressionForString(mt_genPageCompression.valueString.data);
}

void idVirtualTexture::UpdateMinLod(idParallelJobList* jobList) {
  if (idLib::headless || pageMinLodImage == nullptr ||
      rootTexturePage == nullptr || vt_useMinLodTexture.valueInteger == 0) {
    return;
  }

  idPixelUnpackBuffer& current = pageMinLodPBO[currentPageMinLodPBO];
  if (current.size < 0) {
    current.UnmapBuffer();
  }

  currentPageMinLodPBO ^= 1;
  void* bits =
      pageMinLodPBO[currentPageMinLodPBO].MapBuffer(BM_WRITE_NOSYNC);
  if (pageMinLodParms->pageMinLodPBO != nullptr) {
    UploadPageMinLod(pageMinLodParms);
  }

  pageMinLodParms->pageArray = physicalPages->pages;
  pageMinLodParms->rootPageNum = static_cast<std::int16_t>(
      rootTexturePage - physicalPages->pages);
  pageMinLodParms->pageFade = physicalPages->pageFade;
  pageMinLodParms->fadeInSteps = vt_pageFadeInSteps.valueInteger;
  pageMinLodParms->pageMinLodImage = pageMinLodImage;
  pageMinLodParms->pageMinLodImageSize = header.pagesWide;
  pageMinLodParms->pageMinLodBits = bits;
  pageMinLodParms->pageMinLodBytePitch = header.pagesWide;
  pageMinLodParms->pageMinLodPBO =
      pageMinLodPBO[currentPageMinLodPBO].apiObject;

  if (vt_usePageMinLodJobs.valueInteger == 1) {
    jobList->AddJob(reinterpret_cast<jobRun_t>(UpdatePageMinLodJob),
                    pageMinLodParms);
  } else {
    UpdatePageMinLodJob(pageMinLodParms);
  }
}

idVec4 idVirtualTexture::GetFeedbackVec4() const {
  const int pageTexels = physicalPages->physicalPagesPool ==
                                 PHYSICAL_PAGES_POOL_PRT
                             ? 128
                             : 120;
  return idVec4{static_cast<float>(pageSource),
                static_cast<float>(header.pagesWide),
                static_cast<float>(pageTexels * header.pagesWide), 0.0f};
}

bool idVirtualTexture::ValidatePageTable() {
  const int numLevels = pageTableImage->opts.numLevels;
  const int loppedLevels = header.numLevels - numLevels;
  for (int level = 0; level < numLevels; ++level) {
    idImageData* imageData = pageTableImage->GetImageData(level, 0);
    const bool valid =
        physicalPages->ValidatePageTable(imageData, level, loppedLevels);
    FreeImageData(imageData);
    if (!valid) {
      return false;
    }
  }
  return true;
}

void idVirtualTexture::FreeData() {
  if (flavor != ID_VTF_UNKNOWN) {
    physicalPages->FreePageTree_r(this, rootTexturePage, nullptr);
    flavor = ID_VTF_UNKNOWN;
  }

  if (vmtrData != nullptr) {
    if (vmtrData->backgroundUpdater != nullptr) {
      delete vmtrData->backgroundUpdater;
      vmtrData->backgroundUpdater = nullptr;
    }
    vmtrData->~idVMTRData();
    RetailFree(vmtrData, ALIGN_16);
    vmtrData = nullptr;
  }

  if (pageFile != nullptr) {
    streamFileCache->Flush();
    virtualTextureSystem.CloseCachedPagesFile(pageFile);
    pageFile = nullptr;
  }
  pageFileInstalled = nullptr;

  if (physicalPages != nullptr &&
      physicalPages->physicalPagesPool == PHYSICAL_PAGES_POOL_PRT) {
    physicalPages->~idPhysicalPages();
    RetailFree(physicalPages, ALIGN_16);
  }
  physicalPages = nullptr;
  rootTexturePage = nullptr;

  if (pageTableImage != nullptr) {
    pageTableImage->PurgeImage();
    pageTableImage = nullptr;
  }
  if (pageMinLodImage != nullptr) {
    pageMinLodImage->PurgeImage();
    pageMinLodImage = nullptr;
  }
  if (pageTableImageShadow != nullptr) {
    RetailFree(pageTableImageShadow, ALIGN_16);
    pageTableImageShadow = nullptr;
    pageTableImagePBO.FreeBufferObject();
  }

  for (idPixelUnpackBuffer& pbo : pageMinLodPBO) {
    if (pbo.size < 0) {
      pbo.UnmapBuffer();
    }
    pbo.FreeBufferObject();
  }
  if (pageMinLodParms != nullptr) {
    RetailFree(pageMinLodParms, ALIGN_128);
    pageMinLodParms = nullptr;
  }

  enlargedPageTableAllocationFailed = false;
  isDirectMapped = false;
  pageFileIsLocal = false;
  skipFlush = false;
  layoutVersion = 0;
  storedNumLevels = 0;
  ClearPageFileHeader(header);
  pageFilePath = "";
  baseName = "";
  isMapVmtr = false;
}

bool idVirtualTexture::SetSource(const char* baseName_,
                                 const char* pageFilePath_,
                                 physicalPagesPool_t physicalPagesPool_,
                                 virtualTextureFlavor_t flavor_) {
  virtualTextureSystem.SyncFeedback();
  FreeData();
  baseName = baseName_;
  pageFilePath = pageFilePath_;
  pageFileIsLocal = false;
  skipFlush = false;
  flavor = flavor_;

  if (flavor == ID_VTF_VMTR) {
    void* vmtrMemory = RetailAlloc(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW",
        sizeof(idVMTRData), TAG_NEW, false, ALIGN_16);
    vmtrData = vmtrMemory != nullptr
                   ? new (vmtrMemory) idVMTRData(this)
                   : nullptr;

    void* updaterMemory = RetailAlloc(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW",
        sizeof(idBackgroundVmtrUpdater), TAG_NEW, false, ALIGN_16);
    vmtrData->backgroundUpdater =
        updaterMemory != nullptr
            ? new (updaterMemory) idBackgroundVmtrUpdater(vmtrData)
            : nullptr;
    vmtrData->vmtrHeaderPath = pageFilePath_;
    vmtrData->vmtrHeaderPath.StripFileExtension();
    vmtrData->vmtrHeaderPath += ".vmtr";
  }

  ClearPageFileHeader(header);
  if (baseName == "" || r_skipVmtrs.valueInteger != 0) {
    FreeData();
    return false;
  }

  pageFilePath.SlashesToBackSlashes();
  if (std::strncmp(pageFilePath.data, "\\\\", 2) != 0) {
    pageFileIsLocal = true;
    skipFlush = true;
  }

  pageFile = virtualTextureSystem.OpenCachedPagesFile(pageFilePath.data);
  if (pageFile == nullptr) {
    for (int packageIndex = 0;
         packageIndex < session->GetNumContentPackages(); ++packageIndex) {
      idStr poolName("");
      if (feature_lowgore.valueInteger != 0) {
        poolName += "_lowgore";
      }
      char baseNameExtra[296];
      std::sprintf(baseNameExtra, "%s%s", baseName.data, poolName.data);
      idStr candidate(va("%s/virtualtextures/%s.pages",
                         session->GetContentPackagePath(packageIndex),
                         baseNameExtra));
      pageFile = virtualTextureSystem.OpenCachedPagesFile(candidate.data);
      if (pageFile != nullptr) {
        pageFilePath = candidate;
        break;
      }
    }
  }

  if (pageFile == nullptr) {
    if (pageFilePath != "") {
      idLib::Warning("idVirtualTexture::SetSource: Couldn't open '%s'",
                     pageFilePath.data);
    }
    FreeData();
    return false;
  }

  const int sectorSize = pageFile->GetSectorSize();
  const int scheduledLength =
      (sectorSize + 4095) & ~(sectorSize - 1);
  void* readAllocation = RetailAlloc(
      "w:\\build\\tech5\\engine\\renderer\\VirtualTexture.cpp(426) : "
      "TAG_VIRTUALTEXTURE",
      static_cast<std::size_t>(scheduledLength + sectorSize),
      TAG_VIRTUALTEXTURE, false, ALIGN_16);
  const std::uintptr_t alignedAddress =
      (reinterpret_cast<std::uintptr_t>(readAllocation) + sectorSize - 1) &
      ~static_cast<std::uintptr_t>(sectorSize - 1);
  void* alignedHeader = reinterpret_cast<void*>(alignedAddress);
  if (streamFileCache->UncachedScheduledRead(pageFile, 0, alignedHeader,
                                             scheduledLength) <
      sizeof(pageFileHeader_t)) {
    idLib::Error(
        "idVirtualTexture::SetSource: Failed to read header for '%s'",
        pageFilePath.data);
  }
  std::memcpy(&header, alignedHeader, sizeof(header));

  if (vt_supportRoundedDownVmtrs.valueInteger != 0 &&
      flavor != ID_VTF_VMTR) {
    --header.numLevels;
    header.pagesWide >>= 1;
    header.totalPages >>= 2;
  }
  RetailFree(readAllocation, ALIGN_16);

  if (header.diskOffsetScale == 0) {
    header.diskOffsetScale = 64;
  }
  if (header.magic != PAGE_FILE_MAGIC) {
    idLib::Warning("idVirtualTexture::SetSource: Bad magic %d for '%s'",
                   header.magic, pageFilePath.data);
    ClearPageFileHeader(header);
    streamFileCache->Flush();
    virtualTextureSystem.CloseCachedPagesFile(pageFile);
    pageFile = nullptr;
    return false;
  }

  if (header.layoutVersion > 0 && layoutVersion > 0 &&
      layoutVersion != header.layoutVersion) {
    idLib::Warning(
        "idVirtualTexture::SetSource: Layout version mismatch (material: %d, "
        "vt: %d) for '%s'",
        layoutVersion, header.layoutVersion, baseName_);
    ClearPageFileHeader(header);
    streamFileCache->Flush();
    virtualTextureSystem.CloseCachedPagesFile(pageFile);
    pageFile = nullptr;
    FreeData();
    return false;
  }

  pageFileInstalled =
      virtualTextureSystem.OpenInstalledCachedPagesFile(pageFilePath.data);
  const int installedMip = static_cast<std::int8_t>(
      static_cast<unsigned int>(header.installedMipAndOffset) >> 24);
  minMipLevelInstalled =
      std::max(vt_minMipLevelInstalled.valueInteger, installedMip);
  VIRTUAL_MAPPING_PAGES_WIDE = header.pagesWide;
  storedNumLevels = header.numLevels;

  if (virtualTextureSystem.AllowGenMissingPages()) {
    header.numLevels = static_cast<short>(std::max(
        static_cast<int>(header.numLevels),
        std::min(PAGEFILE_MAX_LOD_CLAMP,
                 static_cast<int>(header.numLevels) + 5)));
  }

  bool directMapped = true;
  physicalPagesPool_t imagePool = physicalPagesPool_;
  if (physicalPagesPool_ == PHYSICAL_PAGES_POOL_PRT) {
    header.numLevels = std::min<short>(header.numLevels, 8);
    header.pagesWide = 1 << (header.numLevels - 1);
    VIRTUAL_PAGES_WIDE_LOG2 = FloatExponentLevel(header.pagesWide, 127);
    VIRTUAL_TEXELS_WIDE = 120 * header.pagesWide;
    SetupMipWidths(*this);

    idStr poolName;
    pageFilePath.ExtractFileBase(poolName);
    void* physicalMemory = RetailAlloc(
        "w:\\build\\tech5\\shared\\idlib\\Heap.h(53) : TAG_NEW",
        sizeof(idPhysicalPages), TAG_NEW, false, ALIGN_16);
    physicalPages = physicalMemory != nullptr
                        ? new (physicalMemory) idPhysicalPages()
                        : nullptr;
    const int physicalTexelsWide = header.pagesWide << 7;
    physicalPages->CreatePhysicalImages(
        PHYSICAL_PAGES_POOL_PRT, poolName.data, physicalTexelsWide,
        physicalTexelsWide, nullptr);
  } else {
    int allocationAttempt = 0;
    for (;;) {
      header.pagesWide = 1 << (header.numLevels - 1);
      VIRTUAL_PAGES_WIDE_LOG2 = FloatExponentLevel(header.pagesWide, 127);
      VIRTUAL_TEXELS_WIDE = 120 * header.pagesWide;
      SetupMipWidths(*this);
      const idImageOpts opts = PageTableImageOpts(*this);

      switch (imagePool) {
        case PHYSICAL_PAGES_POOL_VMTR:
          if (flavor == ID_VTF_VMTR &&
              (pageFilePath.Find("\\_vmtr", true, 0, -1) != -1 ||
               pageFilePath.Find("/_vmtr", true, 0, -1) != -1)) {
            pageTableImage =
                globalImages->ScratchImage("_vmtrPageTable", opts);
          } else {
            const idStr imageName = baseName + "_vmtr";
            pageTableImage = globalImages->ScratchImage(imageName.data, opts);
          }
          physicalPages =
              &virtualTextureSystem
                   .physicalPagesPool[PHYSICAL_PAGES_POOL_VMTR];
          break;
        case PHYSICAL_PAGES_POOL_UNIQUE:
          pageTableImage = globalImages->ScratchImage(pageFilePath.data, opts);
          physicalPages =
              &virtualTextureSystem
                   .physicalPagesPool[PHYSICAL_PAGES_POOL_UNIQUE];
          break;
        case PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY: {
          const idStr imageName = pageFilePath + "_do";
          pageTableImage = globalImages->ScratchImage(imageName.data, opts);
          physicalPages = &virtualTextureSystem.physicalPagesPool[
              PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY];
          break;
        }
        case PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2: {
          const idStr imageName = pageFilePath + "_do2";
          pageTableImage = globalImages->ScratchImage(imageName.data, opts);
          physicalPages = &virtualTextureSystem.physicalPagesPool[
              PHYSICAL_PAGES_POOL_UNIQUE_DIFFUSE_ONLY2];
          break;
        }
        default:
          break;
      }

      isDirectMapped = false;
      if (pageTableImage->allocationFailed) {
        if (header.numLevels == storedNumLevels) {
          idLib::Warning(
              "idVirtualTexture::SetSource: Failed to allocate page table "
              "image for '%s'",
              baseName_);
          FreeData();
          return false;
        }
        enlargedPageTableAllocationFailed = true;
      } else {
        isDirectMapped = pageTableImage->MapTexture();
      }

      int pageTableBytes = 0;
      for (int level = header.numLevels - 1; level >= 0; --level) {
        pageTableBytes += 2 * (1 << level) * (1 << level);
      }
      pageTableImageShadow = RetailAlloc(
          "w:\\build\\tech5\\engine\\renderer\\VirtualTexture.cpp(659) "
          ": TAG_VIRTUALTEXTURE",
          static_cast<std::size_t>(pageTableBytes), TAG_VIRTUALTEXTURE, false,
          ALIGN_16);
      if (!pageTableImagePBO.AllocBufferObject(nullptr, pageTableBytes)) {
        if (header.numLevels == storedNumLevels) {
          idLib::Warning(
              "idVirtualTexture::SetSource: Failed to allocate page table PBO "
              "for '%s'",
              baseName_);
          FreeData();
          return false;
        }
        enlargedPageTableAllocationFailed = true;
      }

      if (!enlargedPageTableAllocationFailed) {
        break;
      }
      if (pageTableImage != nullptr) {
        pageTableImage->PurgeImage();
        pageTableImage = nullptr;
      }
      RetailFree(pageTableImageShadow, ALIGN_16);
      pageTableImageShadow = nullptr;
      pageTableImagePBO.FreeBufferObject();
      header.numLevels = static_cast<short>(storedNumLevels);
      imagePool = physicalPagesPool_;
      if (++allocationAttempt >= 2) {
        break;
      }
    }
  }

  if (vt_useMinLodTexture.valueInteger == 0) {
    return true;
  }

  const idImageOpts minLodOpts = PageMinLodImageOpts(*this);
  idStr minLodName = pageFilePath.GetFileBase();
  idStr poolName(physicalPoolNames[imagePool]);
  poolName.ReplaceFirst("PHYSICAL_PAGES_POOL", "");
  minLodName += poolName;
  minLodName += "_minlod";
  pageMinLodImage = globalImages->ScratchImage(minLodName.data, minLodOpts);
  if (pageMinLodImage->allocationFailed) {
    idLib::Warning(
        "idVirtualTexture::SetSource: Failed to allocate min-LOD image for "
        "'%s'",
        baseName_);
    FreeData();
    return false;
  }

  if (!isDirectMapped || !pageMinLodImage->MapTexture()) {
    directMapped = false;
  }
  isDirectMapped = directMapped;

  const int minLodBytes = header.pagesWide * header.pagesWide;
  for (int index = 0; index < 2; ++index) {
    if (!pageMinLodPBO[index].AllocBufferObject(nullptr, minLodBytes)) {
      idLib::Warning(
          "idVirtualTexture::SetSource: Failed to allocate min-LOD PBO for "
          "'%s'",
          baseName_);
      FreeData();
      return false;
    }
  }
  pageMinLodParms = static_cast<pageMinLodUpdate_t*>(RetailAlloc(
      "w:\\build\\tech5\\engine\\renderer\\VirtualTexture.cpp(730) : "
      "TAG_VIRTUALTEXTURE",
      sizeof(pageMinLodUpdate_t), TAG_VIRTUALTEXTURE, true, ALIGN_128));
  return true;
}

void idVirtualTexture::ReleaseSource() {
  if (pageTableImage == nullptr) {
    return;
  }
  virtualTextureSystem.SyncFeedback();
  FreeData();
  baseName = "";
  pageFilePath = "";
  ClearPageFileHeader(header);
  VIRTUAL_PAGES_WIDE_LOG2 = 0;
  VIRTUAL_TEXELS_WIDE = 0;
  VIRTUAL_MAPPING_PAGES_WIDE = 0;
}
