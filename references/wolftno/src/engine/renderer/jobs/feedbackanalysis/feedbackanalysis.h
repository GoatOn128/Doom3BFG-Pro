#pragma once

#include <cstddef>
#include <cstdint>

struct pageStats_t {
  std::uint16_t numPages;
  std::uint16_t numVisPages;
  std::uint16_t numTopPages;
  std::uint16_t pad;
};

struct pageIndices_t {
  std::uint64_t totalNumPages;
  pageStats_t pageStats[6];
  std::uint64_t indices[1];
};

struct virtualTextureInfo_t {
  std::uint32_t numLevels;
  std::uint32_t baseLevel;
  std::uint32_t pagesWide;
  std::uint16_t physicalPagesPool;
  std::uint16_t priorityBias;
};

struct analyzeParms_t {
  int analyzeFrame;
  std::uint32_t alignmentBeforePointers;
  pageIndices_t* pageIndices;
  const virtualTextureInfo_t* vtInfo;
  const void* data;
  std::uint32_t width;
  std::uint32_t height;
  std::uint32_t bytePitch;
  std::uint32_t alignmentBeforeData2;
  const void* data2;
  std::uint32_t data2Width;
  bool forceHighestDetail;
  std::uint8_t alignmentTail[3];
};

void GetUniqueVirtualPages(pageIndices_t* pageIndices, std::uint16_t* temp,
                           const virtualTextureInfo_t* vtInfo,
                           const void* data, std::uint32_t width,
                           std::uint32_t height, std::uint32_t bytePitch,
                           const void* data2, std::uint32_t data2Width,
                           bool forceHighestDetail);
void FeedbackAnalysisJob(const analyzeParms_t* parms);

static_assert(sizeof(pageStats_t) == 8, "page statistics ABI drift");
static_assert(offsetof(pageIndices_t, indices) == 0x38,
              "page-index payload offset drift");
static_assert(sizeof(pageIndices_t) == 0x40, "page-index ABI drift");
static_assert(sizeof(virtualTextureInfo_t) == 0x10,
              "virtual-texture info ABI drift");
static_assert(offsetof(analyzeParms_t, pageIndices) == 0x08,
              "analysis page-index offset drift");
static_assert(offsetof(analyzeParms_t, data) == 0x18,
              "analysis feedback-data offset drift");
static_assert(offsetof(analyzeParms_t, data2) == 0x30,
              "analysis explicit-data offset drift");
static_assert(offsetof(analyzeParms_t, forceHighestDetail) == 0x3C,
              "analysis detail flag offset drift");
static_assert(sizeof(analyzeParms_t) == 0x40,
              "feedback-analysis parameters ABI drift");
