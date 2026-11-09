#pragma once

#include "engine/renderer/virtualtexture.h"

#include <cstddef>
#include <cstdint>

class idImage;

enum pageUpdateType_t : int {
  PAGE_UPDATE_MAP_PHYSICAL_PAGE = 0,
  PAGE_UPDATE_UNMAP_PHYSICAL_PAGE = 1,
  PAGE_UPDATE_MAP_PRT = 2,
  PAGE_UPDATE_UNMAP_PRT = 3,
};

struct pageImageInfo_t {
  void* bits;
  int bytePitch;
  int alignmentPadding;
};

struct pageImagesUpdate_t {
  pageUpdateType_t type;
  physicalPagesPool_t physicalPagesPool;
  int physicalPageNum;
  int physX;
  int physY;
  int mipLevel;
  bool cancelled;
  bool compressed;
  std::uint8_t alignmentBeforePageData[6];
  const void* pageDataPBO;
  const std::uint8_t* pageData;
  idImage* physicalPagesImage0;
  idImage* physicalPagesImage1;
  idImage* physicalPagesImage2;
  float map[4];
  const void* physicalMappingsImagePBO;
  int physicalMappingsImagePBOSize;
  std::uint32_t alignmentBeforeMappingImage;
  idImage* physicalMappingsImage;
  pageImageInfo_t physicalMappingsImageInfo;
  int effectivePageLevel;
  int spotX;
  int spotY;
  std::uint8_t pageSpot[4];
  const void* pageTableImagePBO;
  int pageTableImagePBOSize;
  std::uint32_t alignmentBeforePageTableImage;
  idImage* pageTableImage;
  pageImageInfo_t pageTableImageInfo[14];
  int pad[8];
};

struct pageImagesUpdateParms_t {
  pageImagesUpdate_t* updates;
  int numUpdates;
  int numCancelled;
  bool submittedJob;
  bool usePageTablePBOs;
  bool useUnsynchronizedPageTablePBOs;
  bool useInvalidateRangeBitForPageTablePBOMap;
  bool skipPhysicalTextureUploads;
  std::uint8_t alignmentPadding[3];
};

void CopyImageRect(std::uint8_t* destination, const std::uint8_t* source,
                   unsigned int numScanlines, unsigned int byteWidth,
                   int bytePitch);
void UpdatePageMappings_PC(const pageImagesUpdateParms_t* parms);
void UpdatePageTables_PC(const pageImagesUpdateParms_t* parms);
void UpdatePageImagesJob(const pageImagesUpdateParms_t* parms);
void UploadPhysicalPages(const pageImagesUpdateParms_t** parms,
                         unsigned int numParms);
void UploadPageMappings(const pageImagesUpdateParms_t** parms,
                        unsigned int numParms);
void UploadPageTables(const pageImagesUpdateParms_t** parms,
                      unsigned int numParms);
void UploadPageImages(const pageImagesUpdateParms_t* parms1,
                      const pageImagesUpdateParms_t* parms2);

extern bool useMultiTexSubImage;
extern bool useMappingImageShadow;
extern bool usePageTableImageShadow;

static_assert(sizeof(pageImageInfo_t) == 0x10,
              "page-image info ABI drift");
static_assert(offsetof(pageImagesUpdate_t, pageDataPBO) == 0x20,
              "page update PBO offset drift");
static_assert(offsetof(pageImagesUpdate_t, map) == 0x48,
              "page update mapping offset drift");
static_assert(offsetof(pageImagesUpdate_t, physicalMappingsImageInfo) == 0x70,
              "page update mapping image-info offset drift");
static_assert(offsetof(pageImagesUpdate_t, pageTableImageInfo) == 0xA8,
              "page update table image-info offset drift");
static_assert(sizeof(pageImagesUpdate_t) == 0x1A8,
              "page update ABI drift");
static_assert(sizeof(pageImagesUpdateParms_t) == 0x18,
              "page update parms ABI drift");
