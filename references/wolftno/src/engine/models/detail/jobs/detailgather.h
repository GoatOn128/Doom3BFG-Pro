#pragma once

#include "shared/idlib/geometry/rendermatrix.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

struct dm_treeNode_t {
  std::uint16_t planeType;
  std::int16_t planeDist;
  std::uint16_t children[2];
  std::uint32_t counts[2];
};

struct dm_subTreeBounds_t {
  std::int16_t b[2][4];
};

struct dm_subTreeTreeNodes_t {
  int numTreeNodes;
  int treeNodeOffset;
  int pad[2];
};

struct dm_subTreeTreeNodePtrs_t {
  dm_treeNode_t* treeNodes;
};

struct detailGather_t {
  std::int16_t frustumVerts[3][8];
  int maxDetailHeight;
  const float* mvpMatrix;
  std::uint16_t* detailIndices;
  std::uint32_t maxDetailIndices;
  std::uint32_t numDetailIndices;
  std::uint32_t vertCount;
  std::uint32_t indexCount;
};

struct detailRenderView_t {
  float time;
  float nearDist;
  float farDist;
  float fov[2];
  idVec3 viewOrg;
  idVec3 viewFwd;
  idVec3 viewLeft;
  idVec3 viewUp;
  int pad[3];
  idRenderMatrix mvpMatrix;
};

struct detailSubTreeStats_t {
  int subTreeDetailCount;
  int vertCount;
  int indexCount;
  int pad;
};

struct deferredDetailGatherParms_t {
  const detailRenderView_t* viewData;
  const dm_subTreeBounds_t* subTreeBounds;
  const dm_subTreeTreeNodes_t* subTreeTreeNodes;
  int subTreeTreeNodesSize;
  int maxDetailHeight;
  detailSubTreeStats_t* detailCount;
  std::uint16_t* detailIndices;
  std::uint32_t maxDetailIndices;
};

void SetupSubTreePtrsFromTreeNodes(dm_subTreeTreeNodePtrs_t& subTreePtrs,
                                   const dm_subTreeTreeNodes_t* subTree);
bool R_CullDetailBoundsToMVP(const dm_subTreeBounds_t& bounds,
                             int maxDetailHeight, const float* mvp);
void GetDetailRenderListForView_r(dm_subTreeTreeNodePtrs_t& subTreePtrs,
                                  dm_subTreeBounds_t& bounds,
                                  detailGather_t& gather,
                                  std::uint16_t nodeNum,
                                  std::uint16_t numDetailAtLeaf,
                                  std::uint32_t counts);
void GatherDetailIndicesForSubTree(const deferredDetailGatherParms_t* parms,
                                   int& numDetailIndices);

static_assert(sizeof(dm_treeNode_t) == 16, "dm_treeNode_t ABI drift");
static_assert(offsetof(dm_treeNode_t, planeDist) == 2,
              "dm_treeNode_t::planeDist ABI drift");
static_assert(offsetof(dm_treeNode_t, children) == 4,
              "dm_treeNode_t::children ABI drift");
static_assert(offsetof(dm_treeNode_t, counts) == 8,
              "dm_treeNode_t::counts ABI drift");
static_assert(sizeof(dm_subTreeBounds_t) == 16,
              "dm_subTreeBounds_t ABI drift");
static_assert(sizeof(dm_subTreeTreeNodes_t) == 16,
              "dm_subTreeTreeNodes_t ABI drift");
static_assert(sizeof(dm_subTreeTreeNodePtrs_t) == 8,
              "dm_subTreeTreeNodePtrs_t ABI drift");
static_assert(sizeof(detailGather_t) == 88, "detailGather_t ABI drift");
static_assert(offsetof(detailGather_t, maxDetailHeight) == 48,
              "detailGather_t::maxDetailHeight ABI drift");
static_assert(offsetof(detailGather_t, mvpMatrix) == 56,
              "detailGather_t::mvpMatrix ABI drift");
static_assert(offsetof(detailGather_t, detailIndices) == 64,
              "detailGather_t::detailIndices ABI drift");
static_assert(offsetof(detailGather_t, maxDetailIndices) == 72,
              "detailGather_t::maxDetailIndices ABI drift");
static_assert(offsetof(detailGather_t, numDetailIndices) == 76,
              "detailGather_t::numDetailIndices ABI drift");
static_assert(offsetof(detailGather_t, vertCount) == 80,
              "detailGather_t::vertCount ABI drift");
static_assert(offsetof(detailGather_t, indexCount) == 84,
              "detailGather_t::indexCount ABI drift");
static_assert(sizeof(detailRenderView_t) == 144,
              "detailRenderView_t ABI drift");
static_assert(offsetof(detailRenderView_t, mvpMatrix) == 80,
              "detailRenderView_t::mvpMatrix ABI drift");
static_assert(sizeof(detailSubTreeStats_t) == 16,
              "detailSubTreeStats_t ABI drift");
static_assert(sizeof(deferredDetailGatherParms_t) == 56,
              "deferredDetailGatherParms_t ABI drift");
static_assert(offsetof(deferredDetailGatherParms_t, detailCount) == 32,
              "deferredDetailGatherParms_t::detailCount ABI drift");
static_assert(offsetof(deferredDetailGatherParms_t, maxDetailIndices) == 48,
              "deferredDetailGatherParms_t::maxDetailIndices ABI drift");
