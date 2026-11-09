#include "engine/models/cuttable/convexhull.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <immintrin.h>
#include <limits>

namespace {

std::int32_t WrapSubtract(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) -
                                   static_cast<std::uint32_t>(right));
}

std::int32_t WrapAdd(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) +
                                   static_cast<std::uint32_t>(right));
}

std::int32_t WrapMultiply(std::int32_t left, std::int32_t right) {
  return static_cast<std::int32_t>(static_cast<std::uint32_t>(left) *
                                   static_cast<std::uint32_t>(right));
}

std::int32_t WrapSubtractProducts(std::int32_t a, std::int32_t b,
                                  std::int32_t c, std::int32_t d) {
  return WrapSubtract(WrapMultiply(a, b), WrapMultiply(c, d));
}

int ComparePoints(const idVec2i& left, const idVec2i& right) {
  const int x = WrapSubtract(left.x, right.x);
  return x != 0 ? x : WrapSubtract(left.y, right.y);
}

template <typename AdvanceLeft, typename StopRight>
void QuickSortIndices(std::int16_t* indices, int low, int high,
                      const AdvanceLeft& advanceLeft,
                      const StopRight& stopRight) {
  int lows[256];
  int highs[256];
  int stackIndex = 0;
  lows[0] = low;
  highs[0] = high;
  do {
    low = lows[stackIndex];
    high = highs[stackIndex];
    if (low < high) {
      std::swap(indices[low], indices[(low + high) >> 1]);
      const std::int16_t pivot = indices[low];
      int left = low + 1;
      int right = high;
      while (left <= right) {
        while (left <= high && advanceLeft(indices[left], pivot))
          ++left;
        while (right >= low && !stopRight(indices[right], pivot))
          --right;
        if (left < right)
          std::swap(indices[left], indices[right]);
      }
      std::swap(indices[low], indices[right]);
      stackIndex += 2;
      lows[stackIndex - 2] = right + 1;
      lows[stackIndex - 1] = low;
      highs[stackIndex - 1] = right - 1;
    }
    --stackIndex;
  } while (stackIndex >= 0);
}

std::int32_t HullTurn(const idList<idVec2i>& points,
                      const idStaticList<std::int16_t, 1024>& sorted,
                      std::int16_t first, std::int16_t second,
                      std::int16_t third) {
  const idVec2i& a = points[sorted[first]];
  const idVec2i& b = points[sorted[second]];
  const idVec2i& c = points[sorted[third]];
  return WrapSubtractProducts(WrapSubtract(a.x, c.x),
                              WrapSubtract(b.y, c.y),
                              WrapSubtract(b.x, c.x),
                              WrapSubtract(a.y, c.y));
}

void AppendHullIndex(const idList<idVec2i>& points,
                     const idStaticList<std::int16_t, 1024>& sorted,
                     idStaticList<std::int16_t, 1024>& hull,
                     std::int16_t index) {
  hull.Append(index);
  while (hull.Num() >= 3 &&
         HullTurn(points, sorted, hull[hull.Num() - 3],
                  hull[hull.Num() - 2], hull[hull.Num() - 1]) >= 0) {
    hull[hull.Num() - 2] = hull[hull.Num() - 1];
    --hull.num;
  }
}

float InverseLength(float lengthSquared) {
  const __m128 clamped =
      _mm_max_ss(_mm_set_ss(lengthSquared),
                 _mm_set_ss(std::numeric_limits<float>::min()));
  return 1.0f / _mm_cvtss_f32(_mm_sqrt_ss(clamped));
}

} // namespace

void idConvexHull::QuickSortWeights(const float* weights,
                                    std::int16_t* indices, int num) {
  QuickSortIndices(
      indices, 0, num - 1,
      [weights](std::int16_t left, std::int16_t right) {
        return !(weights[left] - weights[right] > 0.0f);
      },
      [weights](std::int16_t left, std::int16_t right) {
        return weights[left] - weights[right] <= 0.0f;
      });
}

void idConvexHull::QuickSortPoints(const idVec2i* points,
                                   std::int16_t* indices, int num) {
  const auto lessOrEqual = [points](std::int16_t left, std::int16_t right) {
    return ComparePoints(points[left], points[right]) <= 0;
  };
  QuickSortIndices(indices, 0, num - 1, lessOrEqual, lessOrEqual);
}

void idConvexHull::Create(const idList<idVec2i>& points, float shrink) {
  idStaticList<std::int16_t, 1024> indicesUpper;
  idStaticList<std::int16_t, 1024> indicesLower;
  idStaticList<std::int16_t, 1024> indices;
  idStaticList<std::int16_t, 1024> sorted;
  idStaticList<float, 1024> weights;
  idStaticList<idVec2i, 1024> convex;

  const int numPoints = points.Num();
  for (int index = 0; index < numPoints; ++index)
    sorted.Append(static_cast<std::int16_t>(index));
  if (sorted.Num() > 0)
    QuickSortPoints(points.list, sorted.list, sorted.num);

  const int lastSorted = numPoints - 1;
  const idVec2i& firstPoint = points[sorted[0]];
  const idVec2i& lastPoint = points[sorted[lastSorted]];
  indicesUpper.Append(0);
  indicesLower.Append(0);
  const int baseY = WrapSubtract(firstPoint.y, lastPoint.y);
  const int baseX = WrapSubtract(firstPoint.x, lastPoint.x);
  for (int sortedIndex = 1; sortedIndex < lastSorted; ++sortedIndex) {
    const idVec2i& point = points[sorted[sortedIndex]];
    const int side = WrapSubtractProducts(
        baseX, WrapSubtract(point.y, lastPoint.y),
        WrapSubtract(point.x, lastPoint.x), baseY);
    if (side < 0)
      indicesUpper.Append(static_cast<std::int16_t>(sortedIndex));
    else if (side > 0)
      indicesLower.Append(static_cast<std::int16_t>(sortedIndex));
  }
  indicesUpper.Append(static_cast<std::int16_t>(lastSorted));

  for (int index = 0; index < indicesUpper.Num(); ++index)
    AppendHullIndex(points, sorted, indices, indicesUpper[index]);
  for (int index = indicesLower.Num() - 1; index >= 0; --index)
    AppendHullIndex(points, sorted, indices, indicesLower[index]);
  if (indices.Num() > 0 && indices[0] == indices[indices.Num() - 1])
    --indices.num;
  for (int index = 0; index < indices.Num(); ++index)
    indices[index] = sorted[indices[index]];

  if (indices.Num() < 10) {
    for (int index = 0; index < indices.Num(); ++index) {
      const idVec2i& point = points[indices[index]];
      vertices.Append(
          {static_cast<float>(point.x), static_cast<float>(point.y)});
    }
  } else {
    for (int index = 0; index < indices.Num(); ++index)
      convex.Append(points[indices[index]]);
    sorted.Clear();
    weights.Clear();
    indicesLower.Clear();
    indicesUpper.Clear();

    int previous = indices.Num() - 1;
    int current = 0;
    int next = 1;
    for (int index = 0; index < indices.Num(); ++index) {
      const float edgeY = static_cast<float>(
          WrapSubtract(convex[next].y, convex[previous].y));
      const float edgeX = static_cast<float>(
          WrapSubtract(convex[next].x, convex[previous].x));
      const float offsetY = static_cast<float>(convex[current].y) -
                            static_cast<float>(WrapAdd(convex[previous].y,
                                                       convex[next].y)) *
                                0.5f;
      const float offsetX = static_cast<float>(convex[current].x) -
                            static_cast<float>(WrapAdd(convex[next].x,
                                                       convex[previous].x)) *
                                0.5f;
      const float inverse = InverseLength(edgeY * edgeY + edgeX * edgeX);
      weights.Append(std::fabs((edgeX * inverse) * offsetY +
                               (-(edgeY * inverse)) * offsetX));
      sorted.Append(static_cast<std::int16_t>(current));
      indicesLower.Append(static_cast<std::int16_t>(previous));
      indicesUpper.Append(static_cast<std::int16_t>(next));
      previous = current++;
      next = (next + 1) % indices.Num();
    }

    QuickSortWeights(weights.list, sorted.list, sorted.num);
    for (int index = 0; index < sorted.Num() - 10; ++index) {
      const int removed = sorted[index];
      const int before = indicesLower[removed];
      const int after = indicesUpper[removed];
      indicesLower[after] = static_cast<std::int16_t>(before);
      indicesUpper[before] = static_cast<std::int16_t>(after);
      indicesLower[removed] = -1;
      indicesUpper[removed] = -1;
    }

    indices.Clear();
    int hullIndex = indicesUpper[sorted[sorted.Num() - 1]];
    for (int index = 0; index < 10; ++index) {
      indices.Append(static_cast<std::int16_t>(hullIndex));
      hullIndex = indicesUpper[hullIndex];
    }
    for (int index = 0; index < 10; ++index) {
      const idVec2i& point = convex[indices[index]];
      vertices.Append(
          {static_cast<float>(point.x), static_cast<float>(point.y)});
    }
  }

  if (std::fabs(shrink) > 1.192092896e-7f) {
    int previous = vertices.Num() - 1;
    for (int current = 0; current < vertices.Num(); ++current) {
      const idVec2 delta = vertices[previous] - vertices[current];
      const float lengthSquared = delta.LengthSqr();
      const float inverse = InverseLength(lengthSquared);
      if (shrink * 2.0f <= inverse * lengthSquared) {
        const idVec2 offset{(delta.x * inverse) * shrink,
                            (inverse * delta.y) * shrink};
        vertices[previous] = vertices[previous] - offset;
        vertices[current] = offset + vertices[current];
      }
      previous = current;
    }
  }
}
