#include "rectallocator.h"

#include <algorithm>
#include <limits>
#include <numeric>
#include <vector>

namespace {
struct idSortrects {
  const idList<idVec2i>* inputSizes = nullptr;
  bool operator()(int left, int right) const {
    const idVec2i& a = (*inputSizes)[left];
    const idVec2i& b = (*inputSizes)[right];
    return a.x * a.x + a.y * a.y > b.x * b.x + b.y * b.y;
  }
};

template <typename T, typename Comparator> class idSort_Quick {
public:
  explicit idSort_Quick(Comparator comparator) : compare(comparator) {}
  void Sort(T* base, unsigned count) { std::sort(base, base + count, compare); }

private:
  Comparator compare;
};

bool Overlaps(idVec2i position, idVec2i size, idVec2i otherPosition, idVec2i otherSize) {
  return position.x < otherPosition.x + otherSize.x && position.x + size.x > otherPosition.x &&
         position.y < otherPosition.y + otherSize.y && position.y + size.y > otherPosition.y;
}
} // namespace

float RectPackingFraction(const idList<idVec2i>& inputSizes, idVec2i totalSize) {
  const int totalArea = totalSize.x * totalSize.y;
  if (totalArea == 0)
    return 0.0f;
  long long usedArea = 0;
  for (int index = 0; index < inputSizes.Num(); ++index)
    usedArea += static_cast<long long>(inputSizes[index].x) * inputSizes[index].y;
  return static_cast<float>(usedArea) / static_cast<float>(totalArea);
}

bool RectAllocator(const idList<idVec2i>& inputSizes, idList<idVec2i>& outputPositions,
                   idVec2i& totalSize) {
  outputPositions.SetNum(inputSizes.Num());
  totalSize = {0, 0};
  if (inputSizes.Num() == 0)
    return true;
  std::vector<int> order(static_cast<std::size_t>(inputSizes.Num()));
  std::iota(order.begin(), order.end(), 0);
  idSort_Quick<int, idSortrects> sorter(idSortrects{&inputSizes});
  sorter.Sort(order.data(), static_cast<unsigned>(order.size()));

  for (int orderIndex = 0; orderIndex < inputSizes.Num(); ++orderIndex) {
    const int rectangle = order[orderIndex];
    const idVec2i size = inputSizes[rectangle];
    bool found = false;
    idVec2i best{};
    idVec2i bestExtent{};
    long long bestScore = std::numeric_limits<long long>::max();
    std::vector<idVec2i> candidates{{0, 0}};
    for (int previousOrder = 0; previousOrder < orderIndex; ++previousOrder) {
      const int previous = order[previousOrder];
      candidates.push_back(
          {outputPositions[previous].x + inputSizes[previous].x, outputPositions[previous].y});
      candidates.push_back(
          {outputPositions[previous].x, outputPositions[previous].y + inputSizes[previous].y});
    }
    for (const idVec2i candidate : candidates) {
      bool collision = false;
      for (int previousOrder = 0; previousOrder < orderIndex; ++previousOrder) {
        const int previous = order[previousOrder];
        if (Overlaps(candidate, size, outputPositions[previous], inputSizes[previous])) {
          collision = true;
          break;
        }
      }
      if (collision)
        continue;
      idVec2i extent{std::max(totalSize.x, candidate.x + size.x),
                     std::max(totalSize.y, candidate.y + size.y)};
      extent.x = (extent.x + 31) & ~31;
      if (extent.x > 1024 || extent.y > 1024)
        continue;
      const long long score =
          static_cast<long long>(extent.x) * extent.x + static_cast<long long>(extent.y) * extent.y;
      if (score < bestScore ||
          (score == bestScore && candidate.x + candidate.y > best.x + best.y)) {
        found = true;
        best = candidate;
        bestExtent = extent;
        bestScore = score;
      }
    }
    if (!found)
      return false;
    outputPositions[rectangle] = best;
    totalSize = bestExtent;
  }
  return true;
}
