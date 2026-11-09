#include "engine/renderer/jobs/render/rendersort.h"

#include "engine/renderer/render.h"

#include <cstdint>
#include <cstring>
#include <utility>

namespace {
std::int64_t CompareDrawSurfs(const drawSurf_t* left,
                              const drawSurf_t* right) {
  std::uint32_t leftSort;
  std::uint32_t rightSort;
  std::memcpy(&leftSort, &left->sort, sizeof(leftSort));
  std::memcpy(&rightSort, &right->sort, sizeof(rightSort));
  const std::int32_t sortDifference =
      static_cast<std::int32_t>(leftSort - rightSort);
  if (sortDifference != 0)
    return sortDifference;

  const std::uintptr_t leftProgram =
      reinterpret_cast<std::uintptr_t>(left->prog);
  const std::uintptr_t rightProgram =
      reinterpret_cast<std::uintptr_t>(right->prog);
  if (leftProgram != rightProgram)
    return leftProgram < rightProgram ? -1 : 1;

  const std::uintptr_t leftModel =
      reinterpret_cast<std::uintptr_t>(left->model);
  const std::uintptr_t rightModel =
      reinterpret_cast<std::uintptr_t>(right->model);
  if (leftModel == rightModel)
    return 0;
  return leftModel < rightModel ? -1 : 1;
}

int AdvanceToThreshold(drawSurf_t* const* surfaces, int index, int count,
                       float threshold) {
  while (index < count && surfaces[index]->sort < threshold)
    ++index;
  return index;
}
} // namespace

void DrawSurfQuickSort(drawSurf_t** surfaces, std::int64_t count) {
  if (count <= 0)
    return;

  std::int64_t lowStack[128];
  std::int64_t highStack[128];
  std::int64_t low = 0;
  std::int64_t high = count - 1;
  std::int64_t stackDepth = 0;

  for (;;) {
    std::int64_t left;
    std::int64_t right;
    for (;;) {
      left = low;
      right = high;
      if (high - low < 4 || stackDepth >= 127)
        break;

      drawSurf_t* pivot = surfaces[(low + high) / 2];
      do {
        while (CompareDrawSurfs(surfaces[left], pivot) < 0)
          ++left;
        while (CompareDrawSurfs(surfaces[right], pivot) > 0)
          --right;
        if (left > right)
          break;
        std::swap(surfaces[left], surfaces[right]);
        --right;
        ++left;
      } while (left < right);

      while (CompareDrawSurfs(surfaces[right], pivot) == 0 && low < right)
        --right;
      while (CompareDrawSurfs(surfaces[left], pivot) == 0 && left < high)
        ++left;
      highStack[stackDepth] = high;
      lowStack[stackDepth++] = left;
      high = right;
    }

    if (low < high) {
      while (right > low) {
        std::int64_t maximum = low;
        for (std::int64_t scan = low + 1; scan <= right; ++scan) {
          if (CompareDrawSurfs(surfaces[scan], surfaces[maximum]) > 0)
            maximum = scan;
        }
        std::swap(surfaces[maximum], surfaces[right]);
        --right;
      }
    }

    if (--stackDepth < 0)
      break;
    low = lowStack[stackDepth];
    high = highStack[stackDepth];
  }
}

void DepthDrawSurfQuickSort(drawSurf_t** surfaces, std::int64_t count) {
  if (count <= 0)
    return;

  std::int64_t lowStack[128];
  std::int64_t highStack[128];
  std::int64_t low = 0;
  std::int64_t high = count - 1;
  std::int64_t stackDepth = 0;

  for (;;) {
    std::int64_t left;
    std::int64_t right;
    for (;;) {
      left = low;
      right = high;
      if (high - low < 4 || stackDepth >= 127)
        break;

      drawSurf_t* pivot = surfaces[(low + high) / 2];
      do {
        while (surfaces[left]->depthKey < pivot->depthKey)
          ++left;
        while (surfaces[right]->depthKey > pivot->depthKey)
          --right;
        if (left > right)
          break;
        std::swap(surfaces[left], surfaces[right]);
        --right;
        ++left;
      } while (left < right);

      while (surfaces[right]->depthKey == pivot->depthKey && low < right)
        --right;
      while (surfaces[left]->depthKey == pivot->depthKey && left < high)
        ++left;
      highStack[stackDepth] = high;
      lowStack[stackDepth++] = left;
      high = right;
    }

    if (low < high) {
      while (right > low) {
        std::int64_t maximum = low;
        for (std::int64_t scan = low + 1; scan <= right; ++scan) {
          if (surfaces[scan]->depthKey > surfaces[maximum]->depthKey)
            maximum = scan;
        }
        std::swap(surfaces[maximum], surfaces[right]);
        --right;
      }
    }

    if (--stackDepth < 0)
      break;
    low = lowStack[stackDepth];
    high = highStack[stackDepth];
  }

  std::int64_t first = 0;
  std::int64_t last = count - 1;
  while (first < last) {
    std::swap(surfaces[first], surfaces[last]);
    ++first;
    --last;
  }
}

void RenderSort(const renderSortParms_t* parameters) {
  for (int index = 0; index < parameters->numDrawSurfs; ++index)
    parameters->sortedDrawSurfs[index] = &parameters->drawSurfs[index];
  DrawSurfQuickSort(parameters->sortedDrawSurfs,
                    parameters->numDrawSurfs);

  int index = 0;
  int numDepthSorted = 0;
  parameters->renderPasses[2].firstSurface = 0;
  parameters->renderPasses[3].firstSurface = 0;
  const int afterBackground = AdvanceToThreshold(
      parameters->sortedDrawSurfs, index, parameters->numDrawSurfs,
      parameters->settings->sortBackgroundEmit);
  if (afterBackground < parameters->numDrawSurfs)
    parameters->renderPasses[3].firstSurface = afterBackground;
  index = afterBackground;

  index = AdvanceToThreshold(parameters->sortedDrawSurfs, index,
                             parameters->numDrawSurfs,
                             parameters->settings->sortDecal);
  parameters->renderPasses[3].lastSurface = index - 1;

  index = AdvanceToThreshold(parameters->sortedDrawSurfs, index,
                             parameters->numDrawSurfs,
                             parameters->settings->sortSSS);
  parameters->renderPasses[2].lastSurface = index - 1;
  parameters->renderPasses[4].firstSurface = index;

  index = AdvanceToThreshold(parameters->sortedDrawSurfs, index,
                             parameters->numDrawSurfs,
                             parameters->settings->sortEmit);
  parameters->renderPasses[4].lastSurface = index - 1;
  parameters->renderPasses[5].firstSurface = index;
  while (index < parameters->numDrawSurfs &&
         parameters->sortedDrawSurfs[index]->sort <
             parameters->settings->sortEmitOnly) {
    parameters->depthSortedDrawSurfs[numDepthSorted++] =
        parameters->sortedDrawSurfs[index++];
  }
  parameters->renderPasses[5].lastSurface = index - 1;
  parameters->renderPasses[6].firstSurface = index;

  index = AdvanceToThreshold(parameters->sortedDrawSurfs, index,
                             parameters->numDrawSurfs,
                             parameters->settings->sortLight);
  parameters->renderPasses[6].lastSurface = index - 1;
  parameters->renderPasses[7].firstSurface = index;
  while (index < parameters->numDrawSurfs &&
         parameters->sortedDrawSurfs[index]->sort <
             parameters->settings->sortPerturber) {
    parameters->depthSortedDrawSurfs[numDepthSorted++] =
        parameters->sortedDrawSurfs[index++];
  }
  parameters->renderPasses[7].lastSurface = index - 1;
  parameters->renderPasses[8].firstSurface = index;

  index = AdvanceToThreshold(parameters->sortedDrawSurfs, index,
                             parameters->numDrawSurfs,
                             parameters->settings->sortLast);
  parameters->renderPasses[8].lastSurface = index - 1;
  DepthDrawSurfQuickSort(parameters->depthSortedDrawSurfs, numDepthSorted);
  *parameters->numDepthSortedDrawSurfs = numDepthSorted;
}
