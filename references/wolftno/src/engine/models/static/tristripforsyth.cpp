#include "engine/models/static/tristripforsyth.h"

#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace {

std::uint16_t cachePositionScore[32];
std::uint16_t valenceScore[32];

std::uint16_t VertexScore(std::uint8_t liveTriangles,
                          std::int8_t cachePosition) {
  if (liveTriangles == 0)
    return 0;
  std::uint16_t score = cachePosition >= 0
                            ? cachePositionScore[cachePosition]
                            : 0;
  if (liveTriangles < 32)
    score = static_cast<std::uint16_t>(score + valenceScore[liveTriangles]);
  return score;
}

bool TriangleWasAdded(const std::uint8_t* added, int triangle) {
  return (added[static_cast<std::size_t>(triangle) >> 3] &
          (1U << (triangle & 7))) != 0;
}

void* AllocateForsyth(std::size_t size) {
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\shared\\idlib\\Heap.h(76) : TAG_NEW", size,
      TAG_NEW, false, ALIGN_16, HEAP_DEFAULTHEAP);
}

} // namespace

void initForsyth() {
  for (int cachePosition = 0; cachePosition < 32; ++cachePosition) {
    const float score = cachePosition < 3
                            ? 0.75f
                            : std::pow(1.0f -
                                           static_cast<float>(cachePosition - 3) *
                                               0.034482758f,
                                       1.5f);
    cachePositionScore[cachePosition] =
        static_cast<std::uint16_t>(static_cast<int>(score * 7281.0f));
  }
  for (int valence = 1; valence < 32; ++valence) {
    const float score = std::pow(static_cast<float>(valence), -0.5f) * 2.0f;
    valenceScore[valence] =
        static_cast<std::uint16_t>(static_cast<int>(score * 7281.0f));
  }
}

int* reorderForsyth(const int* indices, int nTriangles, int nVertices) {
  initForsyth();

  auto* liveTriangleCount = static_cast<std::uint8_t*>(
      AllocateForsyth(static_cast<std::size_t>(nVertices)));
  std::memset(liveTriangleCount, 0, static_cast<std::size_t>(nVertices));
  const int indexCount = nTriangles * 3;
  for (int index = 0; index < indexCount; ++index) {
    std::uint8_t& count = liveTriangleCount[indices[index]];
    if (count == 255) {
      idMem::Singleton()->Free(liveTriangleCount, ALIGN_16);
      return nullptr;
    }
    ++count;
  }

  auto* activeTriangleOffset = static_cast<int*>(AllocateForsyth(
      sizeof(int) * static_cast<std::size_t>(nVertices)));
  auto* vertexScore = static_cast<std::uint16_t*>(AllocateForsyth(
      sizeof(std::uint16_t) * static_cast<std::size_t>(nVertices)));
  auto* cachePosition = static_cast<std::int8_t*>(
      AllocateForsyth(static_cast<std::size_t>(nVertices)));
  const std::size_t addedSize =
      static_cast<std::size_t>((nTriangles + 7) / 8);
  auto* triangleAdded =
      static_cast<std::uint8_t*>(AllocateForsyth(addedSize));
  auto* triangleScore = static_cast<std::uint16_t*>(AllocateForsyth(
      sizeof(std::uint16_t) * static_cast<std::size_t>(nTriangles)));
  auto* activeTriangles = static_cast<int*>(AllocateForsyth(
      sizeof(int) * static_cast<std::size_t>(indexCount)));
  std::memset(cachePosition, 0xff, static_cast<std::size_t>(nVertices));
  std::memset(triangleAdded, 0, addedSize);
  std::memset(triangleScore, 0,
              sizeof(std::uint16_t) * static_cast<std::size_t>(nTriangles));
  std::memset(activeTriangles, 0,
              sizeof(int) * static_cast<std::size_t>(indexCount));

  int activeOffset = 0;
  for (int vertex = 0; vertex < nVertices; ++vertex) {
    activeTriangleOffset[vertex] = activeOffset;
    activeOffset += liveTriangleCount[vertex];
    liveTriangleCount[vertex] = 0;
  }
  for (int triangle = 0; triangle < nTriangles; ++triangle) {
    for (int corner = 0; corner < 3; ++corner) {
      const int vertex = indices[triangle * 3 + corner];
      activeTriangles[activeTriangleOffset[vertex] +
                      liveTriangleCount[vertex]++] = triangle;
    }
  }

  for (int vertex = 0; vertex < nVertices; ++vertex) {
    vertexScore[vertex] =
        VertexScore(liveTriangleCount[vertex], cachePosition[vertex]);
    for (int active = 0; active < liveTriangleCount[vertex]; ++active) {
      const int triangle =
          activeTriangles[activeTriangleOffset[vertex] + active];
      triangleScore[triangle] = static_cast<std::uint16_t>(
          triangleScore[triangle] + vertexScore[vertex]);
    }
  }

  int bestTriangle = -1;
  int bestScore = -1;
  for (int triangle = 0; triangle < nTriangles; ++triangle) {
    if (triangleScore[triangle] > bestScore) {
      bestScore = triangleScore[triangle];
      bestTriangle = triangle;
    }
  }

  auto* triangleOrder = static_cast<int*>(AllocateForsyth(
      sizeof(int) * static_cast<std::size_t>(nTriangles)));
  std::array<int, 11> cache;
  std::memset(cache.data(), 0xff, sizeof(cache));
  int outputPosition = 0;
  int scanTriangle = 0;
  while (bestTriangle >= 0) {
    triangleAdded[static_cast<std::size_t>(bestTriangle) >> 3] |=
        static_cast<std::uint8_t>(1U << (bestTriangle & 7));
    triangleOrder[outputPosition++] = bestTriangle;

    for (int corner = 0; corner < 3; ++corner) {
      const int vertex = indices[bestTriangle * 3 + corner];
      int oldPosition = cachePosition[vertex];
      if (oldPosition < 0)
        oldPosition = corner + 8;
      if (oldPosition > corner) {
        for (int position = oldPosition; position > corner; --position) {
          cache[position] = cache[position - 1];
          if (cache[position] >= 0)
            ++cachePosition[cache[position]];
        }
      }
      cache[corner] = vertex;
      cachePosition[vertex] = static_cast<std::int8_t>(corner);

      const int count = liveTriangleCount[vertex];
      int active = 0;
      while (active < count &&
             activeTriangles[activeTriangleOffset[vertex] + active] !=
                 bestTriangle) {
        ++active;
      }
      if (active < count) {
        activeTriangles[activeTriangleOffset[vertex] + active] =
            activeTriangles[activeTriangleOffset[vertex] + count - 1];
      }
      --liveTriangleCount[vertex];
    }

    for (int position = 0; position < 11; ++position) {
      const int vertex = cache[position];
      if (vertex < 0)
        break;
      if (position >= 8) {
        cachePosition[vertex] = -1;
        cache[position] = -1;
      }
      const std::uint16_t newScore =
          VertexScore(liveTriangleCount[vertex], cachePosition[vertex]);
      const std::int16_t scoreDifference = static_cast<std::int16_t>(
          newScore - vertexScore[vertex]);
      for (int active = 0; active < liveTriangleCount[vertex]; ++active) {
        const int triangle =
            activeTriangles[activeTriangleOffset[vertex] + active];
        triangleScore[triangle] = static_cast<std::uint16_t>(
            triangleScore[triangle] + scoreDifference);
      }
      vertexScore[vertex] = newScore;
    }

    bestTriangle = -1;
    bestScore = -1;
    for (int position = 0; position < 8 && cache[position] >= 0; ++position) {
      const int vertex = cache[position];
      for (int active = 0; active < liveTriangleCount[vertex]; ++active) {
        const int triangle =
            activeTriangles[activeTriangleOffset[vertex] + active];
        if (triangleScore[triangle] > bestScore) {
          bestScore = triangleScore[triangle];
          bestTriangle = triangle;
        }
      }
    }
    if (bestTriangle < 0) {
      while (scanTriangle < nTriangles &&
             TriangleWasAdded(triangleAdded, scanTriangle)) {
        ++scanTriangle;
      }
      if (scanTriangle < nTriangles)
        bestTriangle = scanTriangle;
    }
  }

  idMem::Singleton()->Free(activeTriangles, ALIGN_16);
  idMem::Singleton()->Free(activeTriangleOffset, ALIGN_16);
  idMem::Singleton()->Free(vertexScore, ALIGN_16);
  idMem::Singleton()->Free(liveTriangleCount, ALIGN_16);
  idMem::Singleton()->Free(cachePosition, ALIGN_16);
  idMem::Singleton()->Free(triangleAdded, ALIGN_16);
  idMem::Singleton()->Free(triangleScore, ALIGN_16);

  auto* reordered = static_cast<int*>(AllocateForsyth(
      sizeof(int) * static_cast<std::size_t>(indexCount)));
  for (int triangle = 0; triangle < nTriangles; ++triangle) {
    const int sourceTriangle = triangleOrder[triangle];
    reordered[triangle * 3] = indices[sourceTriangle * 3];
    reordered[triangle * 3 + 1] = indices[sourceTriangle * 3 + 1];
    reordered[triangle * 3 + 2] = indices[sourceTriangle * 3 + 2];
  }
  idMem::Singleton()->Free(triangleOrder, ALIGN_16);
  return reordered;
}
