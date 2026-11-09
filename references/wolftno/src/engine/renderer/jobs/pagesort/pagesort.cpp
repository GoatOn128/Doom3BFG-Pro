#include "engine/renderer/jobs/pagesort/pagesort.h"

#include "shared/idlib/parallelism/paralleljoblist.h"

#include <utility>

void PageListQuickSort(std::int64_t* values, std::int64_t count) {
  if (count <= 0)
    return;

  std::int64_t lowStack[128];
  std::int64_t highStack[128];
  std::int64_t stackDepth = 0;
  std::int64_t low = 0;
  std::int64_t high = count - 1;

  for (;;) {
    while (high - low >= 4 && stackDepth < 127) {
      std::int64_t left = low;
      std::int64_t right = high;
      const std::int64_t pivot = values[(high + low) / 2];
      do {
        while (values[left] < pivot)
          ++left;
        while (values[right] > pivot)
          --right;
        if (left > right)
          break;
        std::swap(values[left], values[right]);
        --right;
        ++left;
      } while (left < right);

      highStack[stackDepth] = high;
      lowStack[stackDepth] = left;
      ++stackDepth;
      high = right;
    }

    while (low < high) {
      std::int64_t maximum = low;
      for (std::int64_t index = low + 1; index <= high; ++index) {
        if (values[index] > values[maximum])
          maximum = index;
      }
      std::swap(values[maximum], values[high]);
      --high;
    }

    --stackDepth;
    if (stackDepth < 0)
      break;
    low = lowStack[stackDepth];
    high = highStack[stackDepth];
  }
}

void PageSortJob(const pageSortParms_t* parms) {
  const int first = *parms->firstPageIndex;
  const int last = *parms->lastPageIndex;
  const int count = last - first + 1;

  for (int outputIndex = 0; outputIndex < count; ++outputIndex) {
    const int ringIndex = (first + outputIndex) & parms->indexMask;
    const std::int64_t physicalPageNum =
        static_cast<std::int32_t>(parms->physicalPageNums[ringIndex]);
    const int score = parms->pageFrameNum[physicalPageNum] +
                      parms->pagePriority[physicalPageNum] - parms->frameNum;
    parms->sortedPhysicalPageNums[outputIndex] =
        physicalPageNum | (static_cast<std::int64_t>(score) << 32);
  }

  *parms->firstPageIndex = 0;
  *parms->lastPageIndex = last - first;
  PageListQuickSort(parms->sortedPhysicalPageNums, count);
  *parms->done = 1;
}

idParallelJobRegistration register_PageSortJob(
    reinterpret_cast<jobRun_t>(PageSortJob), "PageSortJob");

