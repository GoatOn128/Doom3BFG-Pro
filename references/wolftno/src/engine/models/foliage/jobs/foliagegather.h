#pragma once

#include "engine/models/transparency/jobs/influencespherecull.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/geometry/rendermatrix.h"

#include <cstddef>
#include <cstdint>

struct fm_treeNode_t {
  std::uint16_t planeType;
  std::int16_t planeDist;
  std::uint16_t children[2];
};

struct fm_subTreeBounds_t { std::int16_t b[2][4]; };
struct fm_subTreeData_t {
  int numFoliage;
  int foliageOffset;
  int numAttributes;
  int attributeOffset;
};
struct fm_subTreeTreeNodes_t {
  int numTreeNodes;
  int treeNodeOffset;
  int numLeafInfo;
  int leafInfoOffset;
};
struct fm_foliage_t {
  std::int16_t xyz[3];
  std::uint8_t attribIdx;
  std::uint8_t uniformSizeScale;
  std::uint8_t color[3];
  std::uint8_t shapeType;
  std::uint8_t upVec[3];
  std::uint8_t pad;
};
struct fm_attribute_t {
  float sortScaleBias[4];
  std::uint16_t declIdx;
  std::uint16_t quadWidth;
  std::uint16_t quadHeight;
  std::uint8_t widthVariance;
  std::uint8_t heightVariance;
  std::uint8_t flipHoriz;
  std::uint8_t shape;
  std::uint8_t swayMagnitude;
  std::uint8_t opaque;
  int pad;
};
struct fm_subTreeDataPtrs_t { fm_foliage_t* foliage; fm_attribute_t* attributes; };
struct fm_leafInfo_t {
  std::int16_t b[2][3];
  std::uint16_t foliageOffset;
  std::uint8_t numBigFoliage;
  std::uint8_t numSmallFoliage;
};
struct fm_subTreeTreeNodePtrs_t { fm_treeNode_t* treeNodes; fm_leafInfo_t* leafInfo; };
struct foliageIndex_t { std::uint16_t foliageIndex; std::uint16_t influenceSphereOffset; };
struct foliageSubTreeStats_t {
  int subTreeFoliageCount;
  int subTreeOpaqueCount;
  int subTreeInfluenceCount;
  int pad;
};
struct foliageRenderView_t {
  float time;
  float nearDist;
  float farDist;
  float fov[2];
  idVec3 viewOrg;
  idVec3 viewFwd;
  idVec3 viewLeft;
  idVec3 viewUp;
  idRenderMatrix mvpMatrix;
  std::uint16_t atlasWidth;
  std::uint16_t atlasHeight;
  int pad[2];
};
struct foliageGather_t {
  std::int16_t frustumVerts[3][8];
  const float* mvpMatrix;
  idVec3 viewDir;
  idVec3 viewOrg;
  foliageIndex_t* foliageIndices;
  std::uint32_t numFoliageIndices;
  std::uint32_t numOpaqueIndices;
  std::uint32_t maxFoliageIndices;
  std::int16_t* influenceSphereIndices;
  std::uint32_t numInfluenceSphereIndices;
  std::uint32_t maxInfluenceSphereIndices;
  const influenceSphere_t* influenceSpheres;
  std::uint32_t influenceSphereStackSize;
  int influenceSphereStack[256];
  float foliageStartFadeDist;
  float foliageSmallFadeDistMax;
};
struct deferredFoliageGatherParms_t {
  const foliageRenderView_t* viewData;
  const visibleInfluenceSpheres_t* visibleInfluenceSpheres;
  const fm_subTreeData_t* subTreeData;
  const fm_subTreeBounds_t* subTreeBounds;
  const fm_subTreeTreeNodes_t* subTreeTreeNodes;
  int subTreeTreeNodesSize;
  int maxFoliageHeight;
  float foliageStartFadeDist;
  float foliageSmallFadeDistMax;
  std::uint32_t maxFoliageIndices;
  std::uint32_t maxInfluenceSphereIndices;
  foliageSubTreeStats_t* foliageStats;
  foliageIndex_t* foliageIndices;
  std::int16_t* influenceSphereIndices;
  int pad[2];
};

bool R_CullFoliageBoundsToMVP(const std::int16_t (*const bounds)[3],
                              const float* mvp, idBounds& floatBounds);
void GetFoliageRenderListForView_r(fm_subTreeDataPtrs_t& subTreeDataPtrs,
                                   fm_subTreeTreeNodePtrs_t& subTreePtrs,
                                   fm_subTreeBounds_t& bounds,
                                   foliageGather_t& gather,
                                   unsigned nodeNum,
                                   unsigned numFoliageAtLeaf,
                                   unsigned numSmallFoliageAtLeaf,
                                   unsigned stackOffset);
void GatherFoliageIndicesForSubTree(const deferredFoliageGatherParms_t* parms);

static_assert(sizeof(fm_treeNode_t) == 8);
static_assert(sizeof(fm_subTreeBounds_t) == 16);
static_assert(sizeof(fm_subTreeData_t) == 16);
static_assert(sizeof(fm_subTreeTreeNodes_t) == 16);
static_assert(sizeof(fm_foliage_t) == 16);
static_assert(sizeof(fm_attribute_t) == 32);
static_assert(sizeof(fm_leafInfo_t) == 16);
static_assert(sizeof(foliageIndex_t) == 4);
static_assert(sizeof(foliageSubTreeStats_t) == 16);
static_assert(sizeof(foliageRenderView_t) == 144);
static_assert(offsetof(foliageRenderView_t, viewOrg) == 20);
static_assert(offsetof(foliageRenderView_t, viewFwd) == 32);
static_assert(offsetof(foliageRenderView_t, viewLeft) == 44);
static_assert(offsetof(foliageRenderView_t, viewUp) == 56);
static_assert(offsetof(foliageRenderView_t, mvpMatrix) == 68);
static_assert(sizeof(foliageGather_t) == 1168);
static_assert(offsetof(foliageGather_t, mvpMatrix) == 48);
static_assert(offsetof(foliageGather_t, viewDir) == 56);
static_assert(offsetof(foliageGather_t, viewOrg) == 68);
static_assert(offsetof(foliageGather_t, foliageIndices) == 80);
static_assert(offsetof(foliageGather_t, influenceSphereIndices) == 104);
static_assert(offsetof(foliageGather_t, influenceSpheres) == 120);
static_assert(offsetof(foliageGather_t, influenceSphereStack) == 132);
static_assert(offsetof(foliageGather_t, foliageStartFadeDist) == 1156);
static_assert(sizeof(deferredFoliageGatherParms_t) == 96);
static_assert(offsetof(deferredFoliageGatherParms_t, foliageStats) == 64);
static_assert(offsetof(deferredFoliageGatherParms_t, foliageIndices) == 72);
static_assert(offsetof(deferredFoliageGatherParms_t, influenceSphereIndices) == 80);
