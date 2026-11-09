#include "engine/compilers/meshoptimizer/meshoptimizer.h"

#include "shared/idlib/lib.h"

#include <algorithm>
#include <cfloat>
#include <climits>
#include <cstring>
#include <limits>
#include <vector>

namespace {
constexpr unsigned int L1_CACHE_SIZE = 4;
constexpr unsigned int L2_CACHE_SIZE = 23;
constexpr std::uint16_t CACHE_EMPTY = 0xFFFF;

struct triangleChoice_t {
  unsigned int cost;
  std::uint16_t indexes[3];
};

triangleChoice_t BestTriangleRotation(std::uint16_t* l1Cache, std::uint16_t* l2Cache,
                                      const std::uint16_t* indexes) {
  std::uint16_t rotations[5] = {indexes[0], indexes[1], indexes[2], indexes[0], indexes[1]};
  const unsigned int costs[3] = {
      GetTriangleCost(l1Cache, l2Cache, &rotations[0]),
      GetTriangleCost(l1Cache, l2Cache, &rotations[1]),
      GetTriangleCost(l1Cache, l2Cache, &rotations[2]),
  };

  unsigned int rotation = 0;
  if (costs[0] > costs[1] || costs[0] > costs[2])
    rotation = costs[1] <= costs[2] ? 1u : 2u;

  triangleChoice_t result{};
  result.cost = costs[rotation];
  result.indexes[0] = rotations[rotation + 0];
  result.indexes[1] = rotations[rotation + 1];
  result.indexes[2] = rotations[rotation + 2];
  return result;
}

void GatherTrianglesForVertex(std::uint16_t vertex, const std::vector<unsigned int>& offsets,
                              const std::vector<std::uint16_t>& counts,
                              const std::vector<unsigned int>& trianglesByVertex,
                              const std::vector<std::uint8_t>& rendered,
                              std::vector<unsigned int>& renderList) {
  renderList.clear();
  if (vertex == CACHE_EMPTY || vertex >= counts.size())
    return;
  const unsigned int begin = offsets[vertex];
  const unsigned int end = begin + counts[vertex];
  for (unsigned int i = begin; i < end; ++i) {
    const unsigned int triangleOffset = trianglesByVertex[i];
    if (rendered[triangleOffset / 3] == 0)
      renderList.push_back(triangleOffset);
  }
}

unsigned int CachePosition(const std::uint16_t* cache, unsigned int count,
                           std::uint16_t vertex) {
  for (unsigned int i = 0; i < count; ++i) {
    if (cache[i] == vertex)
      return i;
  }
  return 0;
}
} // namespace

unsigned int GetTriangleCost(std::uint16_t* l1Cache, std::uint16_t* l2Cache,
                             std::uint16_t* indexes) {
  unsigned int cost = 0;
  for (unsigned int vertexIndex = 0; vertexIndex < 3; ++vertexIndex) {
    const std::uint16_t vertex = indexes[vertexIndex];
    bool found = false;
    std::uint16_t* destination = l1Cache + L1_CACHE_SIZE;
    for (unsigned int i = 0; i < L1_CACHE_SIZE; ++i) {
      if (l1Cache[i] == vertex) {
        l1Cache[7] = vertex;
        found = true;
      } else {
        *destination++ = l1Cache[i];
      }
    }

    if (found) {
      l1Cache += L1_CACHE_SIZE;
      continue;
    }

    ++l1Cache;
    cost += 6;
    l1Cache[3] = vertex;
    unsigned int i = 0;
    for (; i < L2_CACHE_SIZE; ++i) {
      if (l2Cache[i] == vertex)
        break;
    }
    if (i == L2_CACHE_SIZE) {
      cost += 36;
      ++l2Cache;
      l2Cache[22] = vertex;
    }
  }
  return cost;
}

void RenderTriangle(std::uint16_t*& l1Cache, std::uint16_t*& l2Cache,
                    std::uint16_t* indexes) {
  for (unsigned int vertexIndex = 0; vertexIndex < 3; ++vertexIndex) {
    const std::uint16_t vertex = indexes[vertexIndex];
    bool found = false;
    std::uint16_t* destination = l1Cache + L1_CACHE_SIZE;
    for (unsigned int i = 0; i < L1_CACHE_SIZE; ++i) {
      if (l1Cache[i] == vertex) {
        l1Cache[7] = vertex;
        found = true;
      } else {
        *destination++ = l1Cache[i];
      }
    }

    if (found) {
      l1Cache += L1_CACHE_SIZE;
      continue;
    }

    ++l1Cache;
    l1Cache[3] = vertex;
    unsigned int i = 0;
    for (; i < L2_CACHE_SIZE; ++i) {
      if (l2Cache[i] == vertex)
        break;
    }
    if (i == L2_CACHE_SIZE) {
      ++l2Cache;
      l2Cache[22] = vertex;
    }
  }
}

unsigned int GetBestRenderOrder_r(unsigned int inCost, unsigned int inBestCost,
                                  unsigned int* renderList, unsigned int renderListSize,
                                  unsigned int* triOrder, std::uint16_t* renderOrder,
                                  std::uint16_t* l1Cache, std::uint16_t* l2Cache,
                                  std::uint16_t* indexes) {
  while (renderListSize > 4) {
    unsigned int bestPosition = UINT_MAX;
    unsigned int bestCost = inBestCost;
    triangleChoice_t bestChoice{};

    for (unsigned int i = 0; i < renderListSize; ++i) {
      const triangleChoice_t choice =
          BestTriangleRotation(l1Cache, l2Cache, indexes + renderList[i]);
      const unsigned int cost = inCost + choice.cost;
      if (cost < bestCost) {
        bestPosition = i;
        bestCost = cost;
        bestChoice = choice;
      }
    }

    if (bestPosition == UINT_MAX)
      return inBestCost;

    renderOrder[0] = bestChoice.indexes[0];
    renderOrder[1] = bestChoice.indexes[1];
    renderOrder[2] = bestChoice.indexes[2];
    triOrder[0] = renderList[bestPosition];
    RenderTriangle(l1Cache, l2Cache, renderOrder);

    renderList[bestPosition] = renderList[renderListSize - 1];
    --renderListSize;
    ++triOrder;
    renderOrder += 3;
    inCost = bestCost;
  }

  unsigned int bestCost = inBestCost;
  for (unsigned int i = 0; i < renderListSize; ++i) {
    const unsigned int triangleOffset = renderList[i];
    const triangleChoice_t choice =
        BestTriangleRotation(l1Cache, l2Cache, indexes + triangleOffset);
    unsigned int cost = inCost + choice.cost;
    if (cost >= bestCost)
      continue;

    std::vector<unsigned int> remainingTriOrder(renderListSize > 0 ? renderListSize - 1 : 0);
    std::vector<std::uint16_t> remainingRenderOrder(
        renderListSize > 0 ? 3 * (renderListSize - 1) : 0);

    if (renderListSize > 1) {
      std::uint16_t* nextL1 = l1Cache;
      std::uint16_t* nextL2 = l2Cache;
      std::uint16_t rotated[3] = {choice.indexes[0], choice.indexes[1], choice.indexes[2]};
      RenderTriangle(nextL1, nextL2, rotated);

      const unsigned int displaced = renderList[i];
      renderList[i] = renderList[renderListSize - 1];
      cost = GetBestRenderOrder_r(cost, bestCost, renderList, renderListSize - 1,
                                  remainingTriOrder.data(), remainingRenderOrder.data(), nextL1,
                                  nextL2, indexes);
      renderList[renderListSize - 1] = renderList[i];
      renderList[i] = displaced;
    }

    if (cost < bestCost) {
      bestCost = cost;
      renderOrder[0] = choice.indexes[0];
      renderOrder[1] = choice.indexes[1];
      renderOrder[2] = choice.indexes[2];
      triOrder[0] = triangleOffset;
      if (renderListSize > 1) {
        std::memmove(renderOrder + 3, remainingRenderOrder.data(),
                     6ull * (renderListSize - 1));
        std::memmove(triOrder + 1, remainingTriOrder.data(),
                     4ull * (renderListSize - 1));
      }
    }
  }
  return bestCost;
}

void MeshOptimizer(std::uint16_t* indexes, unsigned int numIndexes,
                   std::uint16_t* outputIndexes, float k1, float k2, float k3) {
  const unsigned int numTriangles = numIndexes / 3;
  for (unsigned int triangle = 0; triangle < numTriangles; ++triangle) {
    const std::uint16_t* tri = indexes + triangle * 3;
    if (tri[0] == tri[1] || tri[0] == tri[2] || tri[1] == tri[2])
      idLib::Error("Degenerate triangle in mesh!");
  }

  unsigned int maxVertex = 0;
  for (unsigned int i = 0; i < numIndexes; ++i)
    maxVertex = std::max(maxVertex, static_cast<unsigned int>(indexes[i]));
  const unsigned int numVertices = maxVertex + 1;

  std::vector<unsigned int> remainingUses(numVertices, 0);
  for (unsigned int i = 0; i < numIndexes; ++i)
    ++remainingUses[indexes[i]];

  unsigned int maxUses = 0;
  unsigned int totalUses = 0;
  std::vector<unsigned int> offsets(numVertices, 0);
  for (unsigned int vertex = 0; vertex < numVertices; ++vertex) {
    offsets[vertex] = totalUses;
    totalUses += remainingUses[vertex];
    maxUses = std::max(maxUses, remainingUses[vertex]);
  }

  std::vector<unsigned int> trianglesByVertex(totalUses);
  std::vector<std::uint16_t> fill(numVertices, 0);
  for (unsigned int triangle = 0; triangle < numTriangles; ++triangle) {
    const unsigned int triangleOffset = triangle * 3;
    for (unsigned int corner = 0; corner < 3; ++corner) {
      const std::uint16_t vertex = indexes[triangleOffset + corner];
      trianglesByVertex[offsets[vertex] + fill[vertex]++] = triangleOffset;
    }
  }

  const unsigned int cacheBatchCapacity = maxUses + 10;
  std::vector<std::uint16_t> l1Storage(12u * cacheBatchCapacity, CACHE_EMPTY);
  std::vector<std::uint16_t> l2Storage(23u * cacheBatchCapacity, CACHE_EMPTY);
  std::vector<std::uint8_t> rendered(numTriangles, 0);
  std::vector<std::uint16_t> selectedRenderOrder(3u * std::max(1u, maxUses));
  std::vector<unsigned int> selectedTriOrder(std::max(1u, maxUses));
  std::vector<std::uint16_t> trialRenderOrder(3u * std::max(1u, maxUses));
  std::vector<unsigned int> trialTriOrder(std::max(1u, maxUses));
  std::vector<unsigned int> renderList;
  renderList.reserve(std::max(1u, maxUses));

  std::uint16_t* l1Cache = l1Storage.data();
  std::uint16_t* l2Cache = l2Storage.data();
  unsigned int trianglesLeft = numTriangles;

  while (trianglesLeft != 0) {
    int selectedCost = -1;
    unsigned int selectedCount = 0;
    float selectedScore = FLT_MAX;

    auto evaluateVertex = [&](std::uint16_t vertex) {
      if (vertex == CACHE_EMPTY || vertex >= remainingUses.size() || remainingUses[vertex] == 0)
        return;
      GatherTrianglesForVertex(vertex, offsets, fill, trianglesByVertex, rendered, renderList);
      const unsigned int count = static_cast<unsigned int>(renderList.size());
      const unsigned int cost = GetBestRenderOrder_r(
          0, UINT_MAX, renderList.data(), count, trialTriOrder.data(), trialRenderOrder.data(),
          l1Cache, l2Cache, indexes);

      std::uint16_t* replayL1 = l1Cache;
      std::uint16_t* replayL2 = l2Cache;
      for (unsigned int i = 0; i < selectedCount; ++i)
        RenderTriangle(replayL1, replayL2, selectedRenderOrder.data() + i * 3);
      const unsigned int position = CachePosition(replayL2, L2_CACHE_SIZE, vertex);
      const float score = (static_cast<float>(cost) * k1 / 42.0f) +
                          (static_cast<float>(count) * k2) +
                          (static_cast<float>(position) * k3 / 23.0f);
      if (selectedScore > score) {
        selectedScore = score;
        selectedCost = static_cast<int>(cost);
        selectedCount = count;
        std::memmove(selectedRenderOrder.data(), trialRenderOrder.data(), 6ull * count);
        std::memmove(selectedTriOrder.data(), trialTriOrder.data(), 4ull * count);
      }
    };

    for (unsigned int i = 0; i < L1_CACHE_SIZE; ++i)
      evaluateVertex(l1Cache[i]);
    if (selectedCost == -1) {
      for (unsigned int i = 0; i < L2_CACHE_SIZE; ++i)
        evaluateVertex(l2Cache[i]);
    }

    if (selectedCost == -1) {
      unsigned int seedVertex = UINT_MAX;
      int leastUses = INT_MAX;
      for (unsigned int vertex = 0; vertex < maxVertex; ++vertex) {
        const int uses = static_cast<int>(remainingUses[vertex]);
        if (uses != 0 && uses < leastUses) {
          leastUses = uses;
          seedVertex = vertex;
        }
      }
      if (seedVertex == UINT_MAX) {
        for (unsigned int vertex = 0; vertex < numVertices; ++vertex) {
          if (remainingUses[vertex] != 0) {
            seedVertex = vertex;
            break;
          }
        }
      }
      GatherTrianglesForVertex(static_cast<std::uint16_t>(seedVertex), offsets, fill,
                               trianglesByVertex, rendered, renderList);
      selectedCount = static_cast<unsigned int>(renderList.size());
      GetBestRenderOrder_r(0, UINT_MAX, renderList.data(), selectedCount,
                           selectedTriOrder.data(), selectedRenderOrder.data(), l1Cache, l2Cache,
                           indexes);
    }

    std::uint16_t* committedL1 = l1Cache;
    std::uint16_t* committedL2 = l2Cache;
    for (unsigned int i = 0; i < selectedCount; ++i) {
      const unsigned int triangleOffset = selectedTriOrder[i];
      --remainingUses[indexes[triangleOffset + 0]];
      --remainingUses[indexes[triangleOffset + 1]];
      --remainingUses[indexes[triangleOffset + 2]];
      rendered[triangleOffset / 3] = 1;

      std::uint16_t* rotated = selectedRenderOrder.data() + i * 3;
      RenderTriangle(committedL1, committedL2, rotated);
      outputIndexes[0] = rotated[0];
      outputIndexes[1] = rotated[1];
      outputIndexes[2] = rotated[2];
      outputIndexes += 3;
    }

    trianglesLeft -= selectedCount;
    std::memmove(l1Cache, committedL1, 2ull * L1_CACHE_SIZE);
    std::memmove(l2Cache, committedL2, 2ull * L2_CACHE_SIZE);
  }
}
