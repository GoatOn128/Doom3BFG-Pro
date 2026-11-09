#include "engine/models/transparency/jobs/transparencysort.h"

#include "engine/models/static/jobs/statictransparencygen.h"

#include <algorithm>
#include <cstring>

namespace {

std::uint32_t FloatBits(float value) {
  std::uint32_t bits;
  std::memcpy(&bits, &value, sizeof(bits));
  return bits;
}

std::uint32_t MakeDepthCode(std::uint32_t bits) {
  const std::uint32_t sign = (bits >> 16) & 0x8000u;
  const std::uint32_t exponent = (bits >> 23) & 0xffu;
  if (exponent <= 127u)
    return 0;
  const std::uint32_t exponentDelta = exponent - 127u;
  if (exponentDelta > 15u)
    return sign | 0x7fffu;
  return sign | (exponentDelta << 11) | ((bits >> 12) & 0x7ffu);
}

void WriteQuadIndices(std::uint16_t* indices, std::uint32_t quadIndex) {
  const std::uint16_t base = static_cast<std::uint16_t>(4u * quadIndex);
  indices[0] = base;
  indices[1] = static_cast<std::uint16_t>(base + 2);
  indices[2] = static_cast<std::uint16_t>(base + 3);
  indices[3] = base;
  indices[4] = static_cast<std::uint16_t>(base + 3);
  indices[5] = static_cast<std::uint16_t>(base + 1);
}

} // namespace

void TransparencyQuickSort(transSortIndex_t* values, std::int64_t num) {
  if (num <= 0)
    return;

  std::int64_t lowStack[128];
  std::int64_t highStack[128];
  std::int64_t low = 0;
  std::int64_t high = num - 1;
  std::int64_t stackDepth = 0;
  for (;;) {
    std::int64_t right;
    while (true) {
      std::int64_t left = low;
      right = high;
      if (high - low < 4 || stackDepth >= 127)
        break;
      const std::uint32_t pivot = values[(high + low) / 2].value;
      do {
        while (values[left].value > pivot)
          ++left;
        while (values[right].value < pivot)
          --right;
        if (left > right)
          break;
        std::swap(values[left].value, values[right].value);
        --right;
        ++left;
      } while (left < right);
      highStack[stackDepth] = high;
      lowStack[stackDepth++] = left;
      high = right;
    }

    if (low < high) {
      do {
        std::int64_t minimum = low;
        for (std::int64_t scan = low + 1; scan <= right; ++scan) {
          if (values[scan].value < values[minimum].value)
            minimum = scan;
        }
        std::swap(values[minimum].value, values[right].value);
        --right;
      } while (right > low);
    }
    if (--stackDepth < 0)
      break;
    low = lowStack[stackDepth];
    high = highStack[stackDepth];
  }
}

std::uint32_t TransparencyTriIndGen(const transSortParms_t* parms,
                                    int quadOffset) {
  const auto* sorted = reinterpret_cast<const transSortIndex_t*>(
      parms->quadDepths + quadOffset);
  int numQuads = parms->sortResult->numSortedQuads - quadOffset;
  if (6 * numQuads > parms->maxIndices)
    numQuads = parms->maxIndices / 6;
  for (int index = 0; index < numQuads; ++index)
    WriteQuadIndices(parms->indices + 6 * index,
                     sorted[index].value & 0x7fffu);
  return static_cast<std::uint32_t>(numQuads);
}

std::uint32_t TransparencyEmissiveTriIndGen(const transSortParms_t* parms,
                                            int quadOffset) {
  const auto* sorted = reinterpret_cast<const transSortIndex_t*>(
      parms->quadDepths + quadOffset);
  const std::uint8_t* emissive = parms->emissiveQuads + quadOffset;
  int numQuads = parms->sortResult->numSortedQuads - quadOffset;
  if (6 * numQuads > parms->maxIndices)
    numQuads = parms->maxIndices / 6;

  int emittedIndices = 0;
  for (int index = 0; index < numQuads; ++index) {
    const std::uint32_t quadIndex = sorted[index].value & 0x7fffu;
    if (emissive[quadIndex] != 0) {
      WriteQuadIndices(parms->indices2 + emittedIndices, quadIndex);
      emittedIndices += 6;
    }
    if (emittedIndices > parms->maxIndices2 - 6)
      break;
  }
  const std::uint32_t emittedQuads =
      static_cast<std::uint32_t>(emittedIndices / 6);
  if (parms->sortResult->numEmissiveQuads !=
      static_cast<int>(emittedQuads)) {
    parms->sortResult->numEmissiveQuads = static_cast<int>(emittedQuads);
  }
  return emittedQuads;
}

std::uint32_t TransparencySortQuads(const transSortParms_t* parms) {
  auto* sortIndices =
      reinterpret_cast<transSortIndex_t*>(parms->quadDepths);
  int numSortedQuads = 0;
  int numCollapsedVerts = 0;
  int numEmissiveQuads = 0;
  int numOpaqueQuads = 0;

  if (parms->collapse != 0) {
    for (int segmentIndex = 0;
         segmentIndex < parms->numSegmentAllocations; ++segmentIndex) {
      const transSortSegmentInfo_t& segment = parms->segmentInfo[segmentIndex];
      const int numQuads =
          std::min(segment.quadsAllocated, segment.quadsUsed);
      if (numSortedQuads != segment.segmentOffset) {
        std::memmove(parms->verts + 4 * numSortedQuads,
                     parms->verts + 4 * segment.segmentOffset,
                     sizeof(idTransparencyVert) * 4 *
                         static_cast<std::size_t>(numQuads));
        std::memmove(parms->quadDepths + numSortedQuads,
                     parms->quadDepths + segment.segmentOffset,
                     sizeof(float) * static_cast<std::size_t>(numQuads));
      }
      std::memset(parms->emissiveQuads + numSortedQuads,
                  segment.emissivePass, static_cast<std::size_t>(numQuads));
      for (int quad = 0; quad < numQuads; ++quad) {
        const int outputIndex = numSortedQuads + quad;
        const std::uint32_t bits = FloatBits(parms->quadDepths[outputIndex]);
        sortIndices[outputIndex].value =
            (static_cast<std::uint32_t>(segment.opaquePass != 0) << 31) |
            (MakeDepthCode(bits) << 15) |
            static_cast<std::uint32_t>(outputIndex);
      }
      numOpaqueQuads += numQuads * (segment.opaquePass != 0);
      numEmissiveQuads += numQuads * (segment.emissivePass != 0);
      numCollapsedVerts += 4 * numQuads;
      numSortedQuads += numQuads;
    }
  } else {
    for (int segmentIndex = 0;
         segmentIndex < parms->numSegmentAllocations; ++segmentIndex) {
      const transSortSegmentInfo_t& segment = parms->segmentInfo[segmentIndex];
      const int numQuads =
          std::min(segment.quadsAllocated, segment.quadsUsed);
      for (int quad = 0; quad < numQuads; ++quad) {
        const int sourceIndex = segment.segmentOffset + quad;
        const std::uint32_t bits = FloatBits(parms->quadDepths[sourceIndex]);
        sortIndices[numSortedQuads + quad].value =
            (static_cast<std::uint32_t>(segment.opaquePass != 0) << 31) |
            (MakeDepthCode(bits) << 15) |
            static_cast<std::uint32_t>(sourceIndex);
        parms->emissiveQuads[sourceIndex] = segment.emissivePass;
      }
      numSortedQuads += numQuads;
      numOpaqueQuads += numQuads * (segment.opaquePass != 0);
      numEmissiveQuads += numQuads * (segment.emissivePass != 0);
      numCollapsedVerts += 4 * segment.quadsAllocated;
    }
  }

  if (parms->sort != 0)
    TransparencyQuickSort(sortIndices, numSortedQuads);
  parms->sortResult->numSortedQuads = numSortedQuads;
  parms->sortResult->numCollapsedVerts = numCollapsedVerts;
  parms->sortResult->numEmissiveQuads = numEmissiveQuads;
  parms->sortResult->numOpaqueQuads = numOpaqueQuads;
  return static_cast<std::uint32_t>(numSortedQuads);
}
