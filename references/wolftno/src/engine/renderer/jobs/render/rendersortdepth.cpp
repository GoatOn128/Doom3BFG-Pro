#include "engine/renderer/jobs/render/rendersortdepth.h"

#include <algorithm>
#include <cstdint>

namespace {

void SortRange(const idRenderModelCommitted** models, std::uint64_t* keys,
               std::int64_t first, std::int64_t last) {
  while (last - first >= 4) {
    std::int64_t left = first;
    std::int64_t right = last;
    const std::uint64_t pivot = keys[(first + last) / 2];
    do {
      while (keys[left] < pivot)
        ++left;
      while (keys[right] > pivot)
        --right;
      if (left <= right) {
        std::swap(models[left], models[right]);
        std::swap(keys[left], keys[right]);
        ++left;
        --right;
      }
    } while (left <= right);
    if (right - first < last - left) {
      if (first < right)
        SortRange(models, keys, first, right);
      first = left;
    } else {
      if (left < last)
        SortRange(models, keys, left, last);
      last = right;
    }
  }
  for (std::int64_t index = first + 1; index <= last; ++index) {
    const std::uint64_t key = keys[index];
    const idRenderModelCommitted* model = models[index];
    std::int64_t insert = index;
    while (insert > first && keys[insert - 1] > key) {
      keys[insert] = keys[insert - 1];
      models[insert] = models[insert - 1];
      --insert;
    }
    keys[insert] = key;
    models[insert] = model;
  }
}

}  // namespace

void DepthModelQuickSort(const idRenderModelCommitted** viewModels, char* keys,
                         std::int64_t count) {
  if (count > 0) {
    SortRange(viewModels, reinterpret_cast<std::uint64_t*>(keys), 0,
              count - 1);
  }
}

void RenderSortDepth(const renderSortDepthParms_t* parameters) {
  DepthModelQuickSort(parameters->viewModels,
                      reinterpret_cast<char*>(parameters->viewModelSortKeys),
                      parameters->numViewModels);
  int modelIndex = 0;
  for (std::uint64_t category = 0; category < 17; ++category) {
    parameters->renderViewModels[category].firstModel = modelIndex;
    while (modelIndex < parameters->numViewModels &&
           (parameters->viewModelSortKeys[modelIndex] >> 56) <= category) {
      ++modelIndex;
    }
    parameters->renderViewModels[category].lastModel = modelIndex - 1;
  }
}
