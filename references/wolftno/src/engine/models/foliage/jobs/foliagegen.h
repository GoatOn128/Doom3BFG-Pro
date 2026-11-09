#pragma once

#include "engine/models/foliage/jobs/foliagegather.h"
#include "engine/models/static/jobs/statictransparencygen.h"

#include <cstddef>
#include <cstdint>

struct deferredFoliageGenParms_t {
  const foliageRenderView_t* viewData;
  const visibleInfluenceSpheres_t* visibleInfluenceSpheres;
  const std::int16_t* influenceSphereIndices;
  std::int32_t maxInfluenceSphereIndices;
  const fm_subTreeBounds_t* subTreeBounds;
  const fm_subTreeData_t* subTreeData;
  std::int32_t subTreeDataSize;
  const foliageIndex_t* foliageIndices;
  std::int32_t maxFoliageIndices;
  std::int32_t numJobsForIndices;
  std::int32_t jobIndexForIndices;
  const foliageSubTreeStats_t* foliageStatsList;
  std::int32_t foliageStatsIndex;
  float foliageStartFadeDist;
  float foliageSmallFadeDistMax;
  float foliageBigFadeDistMax;
  std::int32_t quadsOffset;
  idTransparencyVert* quadsVertices;
  float* quadsDepths;
  int* opaqueQuadsNumAllocated;
  int* opaqueQuadsNumUsed;
  int* transQuadsOffset;
  int* transQuadsNumAllocated;
  int* transQuadsNumUsed;
};

void CalculateFoliageBounds(const std::int16_t* const xyz, float uniformSizeScale,
                            float quadWidth, float quadHeight,
                            float widthVariance, float heightVariance,
                            const idVec3& upVec, idBounds& bounds);
void CalculateFoliageBounds(const fm_foliage_t* foliage,
                            const fm_attribute_t* attribute,
                            idBounds& bounds);
void GetFoliageOffsetForSubTree(
    const foliageSubTreeStats_t* foliageStatsList, int subTreeIndex,
    int& totalFoliage, int& totalOpaque);
void GetFoliageTotalStats(const foliageSubTreeStats_t* foliageStatsList,
                          int& totalFoliage, int& totalOpaque);
void GetFoliageOffsetAndCountForJob(int numJobs, int jobIndex,
                                    int totalCount, int& jobOffset,
                                    int& jobCount);
void GetOpaqueOffsetAndCountForJob(
    int jobOffset, int jobCount, const fm_subTreeDataPtrs_t& subTreePtrs,
    const foliageIndex_t* foliageIndices, int& opaqueOffset,
    int& opaqueCount);
int BuildFoliageQuad(idTransparencyVert* verts, float* quadDepth,
                     unsigned int foliageIndex, const fm_foliage_t* foliage,
                     const fm_attribute_t* attribute,
                     const foliageRenderView_t* viewData,
                     const influenceSphere_t* influenceSpheres,
                     const std::int16_t* influenceSphereIndices,
                     float foliageStartFadeDist,
                     float foliageSmallFadeDistMax,
                     float foliageBigFadeDistMax);
int GenFoliageForSubTree(const deferredFoliageGenParms_t* parms,
                         const foliageIndex_t* foliageIndices, int num,
                         int opaqueVertexOffset, int transVertexOffset,
                         int numOpaque, int numTrans);

static_assert(sizeof(deferredFoliageGenParms_t) == 168);
static_assert(offsetof(deferredFoliageGenParms_t, subTreeBounds) == 32);
static_assert(offsetof(deferredFoliageGenParms_t, foliageIndices) == 56);
static_assert(offsetof(deferredFoliageGenParms_t, foliageStatsList) == 80);
static_assert(offsetof(deferredFoliageGenParms_t, quadsVertices) == 112);
static_assert(offsetof(deferredFoliageGenParms_t, transQuadsNumUsed) == 160);
