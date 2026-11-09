#include "engine/renderer/jobs/feedbackanalysis/feedbackanalysis.h"

#include "shared/idlib/parallelism/paralleljoblist.h"

#include <cstdint>
#include <cstring>
#include <utility>

namespace {

constexpr std::uint64_t kPageFlagsMask = 0x3ull;
constexpr std::uint64_t kPageKeyMask = 0xFFFFFFFCull;
constexpr std::uint64_t kPageCountUnit = 0x100000000ull;
constexpr std::uint64_t kPagePriorityUnit = 0x1000000000000ull;
constexpr std::uint64_t kPagePriorityMask = 0x1F000000000000ull;
constexpr std::uint64_t kPagePriorityClearMask = 0xFFE0FFFFFFFFFFFFull;
constexpr std::uint32_t kHashBuckets = 1024;
constexpr std::uint32_t kHashNextOffset = 1024;
constexpr std::uint32_t kParentOffset = 9216;
constexpr std::uint32_t kMaximumPages = 8192;

std::uint32_t PageHash(std::uint32_t x, std::uint32_t y,
                       std::uint32_t level) {
  const std::uint32_t packed = x | ((y | (level << 11u)) << 11u);
  const std::uint32_t hash = packed * 1664525u + 1013904223u;
  return (hash ^ ((hash ^ (hash >> 11u)) >> 11u)) &
         (kHashBuckets - 1u);
}

std::uint64_t PageKey(std::uint32_t virtualTexture,
                      std::uint32_t level, std::uint32_t x,
                      std::uint32_t y) {
  const std::uint64_t textureAndLevel =
      ((static_cast<std::uint64_t>(virtualTexture & 0xFu) << 4u) |
       (level & 0xFu))
      << 11u;
  return 4ull *
         ((x & 0x7FFu) |
          (((y & 0x7FFu) | textureAndLevel) << 11u));
}

void SortPageIndicesDescending(std::uint64_t* values, std::uint16_t count) {
  if (count <= 1) {
    return;
  }

  std::int64_t lowStack[128];
  std::int64_t highStack[128];
  std::int64_t stackDepth = 0;
  std::int64_t low = 0;
  std::int64_t high = static_cast<std::int64_t>(count) - 1;

  for (;;) {
    while (high - low >= 4 && stackDepth < 127) {
      std::int64_t left = low;
      std::int64_t right = high;
      const std::uint64_t pivot = values[(high + low) / 2];
      do {
        while (values[left] > pivot) {
          ++left;
        }
        while (values[right] < pivot) {
          --right;
        }
        if (left > right) {
          break;
        }
        std::swap(values[left], values[right]);
        --right;
        ++left;
      } while (left <= right);

      highStack[stackDepth] = high;
      lowStack[stackDepth] = left;
      ++stackDepth;
      high = right;
    }

    while (low < high) {
      std::int64_t minimum = low;
      for (std::int64_t index = low + 1; index <= high; ++index) {
        if (values[index] < values[minimum]) {
          minimum = index;
        }
      }
      std::swap(values[minimum], values[high]);
      --high;
    }

    --stackDepth;
    if (stackDepth < 0) {
      break;
    }
    low = lowStack[stackDepth];
    high = highStack[stackDepth];
  }
}

} // namespace

void GetUniqueVirtualPages(pageIndices_t* pageIndices, std::uint16_t* temp,
                           const virtualTextureInfo_t* vtInfo,
                           const void* data, std::uint32_t width,
                           std::uint32_t height, std::uint32_t bytePitch,
                           const void* data2, std::uint32_t data2Width,
                           bool forceHighestDetail) {
  std::memset(temp, 0, kHashBuckets * sizeof(temp[0]));

  auto* const hashHeads = temp;
  auto* const hashNext = temp + kHashNextOffset;
  auto* const parents = temp + kParentOffset;
  auto* const indices = pageIndices->indices;
  const auto* const feedback = static_cast<const std::uint8_t*>(data);
  const auto* const explicitFeedback = static_cast<const std::uint8_t*>(data2);
  const std::uint32_t detailMask = forceHighestDetail ? 0u : 31u;

  std::uint16_t totalPages = 0;
  std::uint16_t currentPage = 0;
  std::uint16_t currentFinestPage = 0;
  int lastX = -1;
  int lastY = -1;
  int lastLevel = -1;
  int lastVirtualTexture = -1;

  const int effectiveHeight =
      bytePitch == 0 ? 0 : static_cast<int>(height);
  for (int row = -1; row < effectiveHeight; ++row) {
    const std::uint8_t* rowData = nullptr;
    std::uint32_t rowWidth = 0;
    if (row == -1) {
      rowData = explicitFeedback;
      rowWidth = data2Width;
    } else {
      rowData = feedback + static_cast<std::size_t>(bytePitch) * row;
      rowWidth = width;
    }

    bool capacityReached = false;
    for (std::uint32_t pixelIndex = 0; pixelIndex < rowWidth; ++pixelIndex) {
      const std::uint8_t* const pixel = rowData + pixelIndex * 4u;
      const std::uint32_t pageX =
          pixel[0] | (static_cast<std::uint32_t>(pixel[2] & 0x0Fu) << 8u);
      const std::uint32_t pageY =
          pixel[1] | (static_cast<std::uint32_t>(pixel[2] & 0xF0u) << 4u);
      const std::uint32_t virtualTexture = pixel[3] >> 4u;
      const std::uint32_t feedbackLevel = pixel[3] & 0x0Fu;

      if (virtualTexture == 0 || virtualTexture >= 16) {
        continue;
      }

      if (lastX == static_cast<int>(pageX) &&
          lastY == static_cast<int>(pageY) &&
          lastLevel == static_cast<int>(feedbackLevel) &&
          lastVirtualTexture == static_cast<int>(virtualTexture)) {
        for (std::uint16_t page = currentPage; page != 0;
             page = parents[page]) {
          indices[page - 1] += kPageCountUnit;
        }
        continue;
      }

      const virtualTextureInfo_t& info = vtInfo[virtualTexture];
      std::uint32_t numLevels = info.numLevels;
      if (numLevels == 0) {
        continue;
      }

      std::uint32_t baseLevel = info.baseLevel;
      const std::uint32_t requestedLevel = detailMask & feedbackLevel;
      if (requestedLevel >= baseLevel) {
        baseLevel = requestedLevel;
        if (requestedLevel > numLevels - 1u) {
          baseLevel = numLevels - 1u;
        }
      }
      if (pageX >= info.pagesWide || pageY >= info.pagesWide) {
        continue;
      }

      lastX = static_cast<int>(pageX);
      lastY = static_cast<int>(pageY);
      lastLevel = static_cast<int>(feedbackLevel);
      lastVirtualTexture = static_cast<int>(virtualTexture);

      if ((info.pagesWide >> baseLevel) == 0) {
        continue;
      }

      std::uint32_t levelX = pageX >> baseLevel;
      std::uint32_t levelY = pageY >> baseLevel;
      std::uint64_t levelKeys[16]{};
      std::uint16_t levelHashes[16]{};
      std::uint16_t foundPage = 0;
      std::uint32_t foundLevel = baseLevel;

      for (; foundLevel < numLevels; ++foundLevel) {
        const std::uint32_t hash = PageHash(levelX, levelY, foundLevel);
        const std::uint64_t key =
            PageKey(virtualTexture, foundLevel, levelX, levelY);
        levelHashes[foundLevel] = static_cast<std::uint16_t>(hash);
        levelKeys[foundLevel] = key;

        std::uint16_t page = hashHeads[hash];
        while (page != 0 &&
               key != (indices[page - 1] & kPageKeyMask)) {
          page = hashNext[page];
        }
        if (page != 0) {
          foundPage = page;
          currentFinestPage = page;
          break;
        }
        levelX >>= 1u;
        levelY >>= 1u;
      }

      if (baseLevel < foundLevel &&
          static_cast<std::uint32_t>(totalPages) + 16u >= kMaximumPages) {
        currentPage = currentFinestPage;
        capacityReached = true;
        break;
      }

      std::uint64_t priority =
          (row == -1 ? 16ull : 0ull) |
          (static_cast<std::uint64_t>(info.priorityBias) - baseLevel +
           foundLevel + 1ull);
      std::uint16_t parentPage = foundPage;
      if (baseLevel < foundLevel) {
        std::uint64_t newPriority = priority - 1ull;
        for (std::uint32_t level = foundLevel; level-- > baseLevel;) {
          ++totalPages;
          indices[totalPages - 1] =
              levelKeys[level] | (newPriority << 48u) | kPageCountUnit;
          const std::uint16_t hash = levelHashes[level];
          hashNext[totalPages] = hashHeads[hash];
          hashHeads[hash] = totalPages;
          parents[totalPages] = parentPage;
          parentPage = totalPages;
          --newPriority;
        }
        currentFinestPage = totalPages;
      }
      currentPage = currentFinestPage;

      for (std::uint16_t page = foundPage; page != 0;
           page = parents[page], ++priority) {
        const std::uint64_t oldPriority =
            (indices[page - 1] >> 48u) & 0x1Fu;
        if (priority > oldPriority) {
          indices[page - 1] =
              (priority << 48u) |
              (indices[page - 1] & kPagePriorityClearMask);
        }
      }

      if (currentPage != 0) {
        indices[currentPage - 1] |= 1ull;
      }
      for (std::uint16_t page = parents[currentPage]; page != 0;
           page = parents[page]) {
        indices[page - 1] |= 2ull;
      }
      for (std::uint16_t page = foundPage; page != 0;
           page = parents[page]) {
        indices[page - 1] += kPageCountUnit;
      }
    }

    if (capacityReached) {
      continue;
    }
  }

  pageIndices->totalNumPages = totalPages;
  for (pageStats_t& stats : pageIndices->pageStats) {
    stats.numPages = 0;
    stats.numVisPages = 0;
    stats.numTopPages = 0;
  }

  for (std::uint16_t index = 0; index < totalPages; ++index) {
    const std::uint64_t page = indices[index];
    const std::uint32_t virtualTexture =
        static_cast<std::uint32_t>(page >> 28u) & 0xFu;
    pageStats_t& stats =
        pageIndices->pageStats[vtInfo[virtualTexture].physicalPagesPool];
    ++stats.numPages;
    stats.numVisPages = static_cast<std::uint16_t>(
        stats.numVisPages + (page & 1ull));
    stats.numTopPages = static_cast<std::uint16_t>(
        stats.numTopPages + ((page & 2ull) == 0));
  }

  for (std::uint16_t index = 0; index < totalPages; ++index) {
    const std::uint64_t page = indices[index];
    const std::uint32_t priority =
        static_cast<std::uint32_t>((page & kPagePriorityMask) >> 48u);
    const std::uint32_t count =
        static_cast<std::uint32_t>((page >> 32u) & 0xFFFFu);
    if (priority != 0 && (priority & 16u) == 0 &&
        count <= (1u << priority)) {
      indices[index] -= kPagePriorityUnit;
    }
  }

  SortPageIndicesDescending(indices, totalPages);
}

void FeedbackAnalysisJob(const analyzeParms_t* parms) {
  std::uint16_t temp[17408];
  GetUniqueVirtualPages(
      parms->pageIndices, temp, parms->vtInfo, parms->data, parms->width,
      parms->height, parms->bytePitch, parms->data2, parms->data2Width,
      parms->forceHighestDetail);
}

idParallelJobRegistration register_FeedbackAnalysisJob(
    reinterpret_cast<jobRun_t>(FeedbackAnalysisJob), "FeedbackAnalysisJob");
