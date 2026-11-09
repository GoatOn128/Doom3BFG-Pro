#pragma once

#include <cstddef>
#include <cstdint>

struct pageSortParms_t {
  int frameNum;
  const int* pagePriority;
  const int* pageFrameNum;
  const std::int64_t* physicalPageNums;
  int* firstPageIndex;
  int* lastPageIndex;
  int indexMask;
  std::int64_t* sortedPhysicalPageNums;
  volatile int* done;
};

void PageListQuickSort(std::int64_t* values, std::int64_t count);
void PageSortJob(const pageSortParms_t* parms);

static_assert(offsetof(pageSortParms_t, pagePriority) == 0x8,
              "pageSortParms_t priority offset drift");
static_assert(offsetof(pageSortParms_t, physicalPageNums) == 0x18,
              "pageSortParms_t physical page offset drift");
static_assert(offsetof(pageSortParms_t, sortedPhysicalPageNums) == 0x38,
              "pageSortParms_t sorted page offset drift");
static_assert(sizeof(pageSortParms_t) == 0x48, "pageSortParms_t ABI drift");

