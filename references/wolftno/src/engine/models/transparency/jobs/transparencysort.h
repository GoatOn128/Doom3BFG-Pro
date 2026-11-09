#pragma once

#include "shared/idlib/geometry/drawvert.h"

#include <cstddef>
#include <cstdint>

class idTransparencyVert;

struct transSortIndex_t {
  std::uint32_t value;
};

struct transSortSegmentInfo_t {
  int segmentOffset;
  int quadsAllocated;
  int quadsUsed;
  bool emissivePass;
  bool opaquePass;
  std::uint8_t pad[2];
};

struct transSortResult_t {
  int numSortedQuads;
  int numCollapsedVerts;
  int numEmissiveQuads;
  int numOpaqueQuads;
};

struct transSortParms_t {
  std::int16_t collapse;
  std::int16_t sort;
  int numSegmentAllocations;
  transSortSegmentInfo_t* segmentInfo;
  idTransparencyVert* verts;
  std::uint8_t* emissiveQuads;
  int numQuadsToSort;
  int pad0;
  float* quadDepths;
  transSortResult_t* sortResult;
  int maxIndices;
  int pad1;
  std::uint16_t* indices;
  int maxIndices2;
  int pad2;
  std::uint16_t* indices2;
};

void TransparencyQuickSort(transSortIndex_t* values, std::int64_t num);
std::uint32_t TransparencyTriIndGen(const transSortParms_t* parms,
                                    int quadOffset);
std::uint32_t TransparencyEmissiveTriIndGen(const transSortParms_t* parms,
                                            int quadOffset);
std::uint32_t TransparencySortQuads(const transSortParms_t* parms);

static_assert(sizeof(transSortIndex_t) == 4,
              "transSortIndex_t ABI drift");
static_assert(sizeof(transSortSegmentInfo_t) == 16,
              "transSortSegmentInfo_t ABI drift");
static_assert(offsetof(transSortSegmentInfo_t, emissivePass) == 12,
              "transSortSegmentInfo_t::emissivePass ABI drift");
static_assert(sizeof(transSortResult_t) == 16,
              "transSortResult_t ABI drift");
static_assert(sizeof(transSortParms_t) == 88,
              "transSortParms_t ABI drift");
static_assert(offsetof(transSortParms_t, segmentInfo) == 8,
              "transSortParms_t::segmentInfo ABI drift");
static_assert(offsetof(transSortParms_t, verts) == 16,
              "transSortParms_t::verts ABI drift");
static_assert(offsetof(transSortParms_t, emissiveQuads) == 24,
              "transSortParms_t::emissiveQuads ABI drift");
static_assert(offsetof(transSortParms_t, numQuadsToSort) == 32,
              "transSortParms_t::numQuadsToSort ABI drift");
static_assert(offsetof(transSortParms_t, quadDepths) == 40,
              "transSortParms_t::quadDepths ABI drift");
static_assert(offsetof(transSortParms_t, sortResult) == 48,
              "transSortParms_t::sortResult ABI drift");
static_assert(offsetof(transSortParms_t, maxIndices) == 56,
              "transSortParms_t::maxIndices ABI drift");
static_assert(offsetof(transSortParms_t, indices) == 64,
              "transSortParms_t::indices ABI drift");
static_assert(offsetof(transSortParms_t, maxIndices2) == 72,
              "transSortParms_t::maxIndices2 ABI drift");
static_assert(offsetof(transSortParms_t, indices2) == 80,
              "transSortParms_t::indices2 ABI drift");
