#include "engine/models/foliage/rendermodelfoliage.h"

#include "engine/models/renderworld.h"
#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/renderer/imagemanager.h"
#include "engine/renderer/render.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstring>

extern idCVar r_znear;

idCVar r_useFoliageGenJobs("r_useFoliageGenJobs", "1", CVAR_INTEGER,
                           "0 = off, 1 = threaded", 0.0f, 1.0f);
idCVar r_showFoliageNodeBounds(
    "r_showFoliageNodeBounds", "0", CVAR_INTEGER,
    "show the individual foliage bounds - 0 = off, 1 = just the bounds, 2 = bounds + position and size");
idCVar r_showFoliageSubTreeNodes(
    "r_showFoliageSubTreeNodes", "-1", CVAR_INTEGER,
    "show the nodes of the specified subtree");
idCVar r_showFoliageSubTreeBounds(
    "r_showFoliageSubTreeBounds", "-1", CVAR_INTEGER,
    "show the bounds of the specified subtree");

namespace {

constexpr int kMaxVisibleSubTrees = 128;
constexpr int kJobsPerSubTree = 16;
constexpr int kMaximumFoliageIndices = 33664;
constexpr int kMaximumFoliageQuads = 8192;
constexpr memTag_t kFoliageMemoryTag = static_cast<memTag_t>(0x35);

constexpr idColor kSubTreeColors[16] = {
    {1.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.0f, 0.0f, 1.0f},
    {0.5f, 0.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 0.0f, 1.0f},
    {0.0f, 0.5f, 0.0f, 1.0f}, {0.0f, 0.0f, 1.0f, 1.0f},
    {1.0f, 1.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f, 1.0f},
    {0.0f, 1.0f, 1.0f, 1.0f}, {1.0f, 0.5f, 0.0f, 1.0f},
    {0.5f, 0.0f, 0.5f, 1.0f}, {1.0f, 0.5f, 0.5f, 1.0f},
    {0.5f, 0.25f, 0.0f, 1.0f}, {0.75f, 0.75f, 0.75f, 1.0f},
    {0.5f, 0.5f, 0.5f, 1.0f}, {0.25f, 0.25f, 0.25f, 1.0f}};

void SetupDataPointers(fm_subTreeDataPtrs_t& pointers,
                       const fm_subTreeData_t* data) {
  auto* base = reinterpret_cast<std::uint8_t*>(
      const_cast<fm_subTreeData_t*>(data));
  pointers.foliage =
      reinterpret_cast<fm_foliage_t*>(base + data->foliageOffset);
  pointers.attributes =
      reinterpret_cast<fm_attribute_t*>(base + data->attributeOffset);
}

idBounds BoundsFromSubTree(const fm_subTreeBounds_t& source) {
  return {{{static_cast<float>(source.b[0][0]),
            static_cast<float>(source.b[0][1]),
            static_cast<float>(source.b[0][2])},
           {static_cast<float>(source.b[1][0]),
            static_cast<float>(source.b[1][1]),
            static_cast<float>(source.b[1][2])}}};
}

void DebugSubTreeFoliage(idRenderDebug* renderDebug,
                         const fm_subTree_t& subTree, int subTreeIndex) {
  fm_subTreeDataPtrs_t pointers{};
  SetupDataPointers(pointers, subTree.data);
  idBounds bounds;
  bounds.Clear();
  const idColor& color = kSubTreeColors[subTreeIndex & 15];
  for (int index = 0; index < subTree.data->numFoliage; ++index) {
    const fm_foliage_t& foliage = pointers.foliage[index];
    const idVec3 origin{static_cast<float>(foliage.xyz[0]),
                        static_cast<float>(foliage.xyz[1]),
                        static_cast<float>(foliage.xyz[2])};
    bounds.AddPoint(origin);
    renderDebug->DebugPoint(color, origin, 0, true);
  }
  renderDebug->DebugBounds(color, bounds, idVec3{}, 0, false);
}

void DebugGatheredFoliage(idRenderDebug* renderDebug,
                          const deferredFoliageGatherParms_t& parms,
                          const idMat3& viewAxis, int mode) {
  if (renderDebug == nullptr)
    return;
  fm_subTreeDataPtrs_t pointers{};
  SetupDataPointers(pointers, parms.subTreeData);
  for (int index = 0; index < parms.foliageStats->subTreeFoliageCount;
       ++index) {
    const fm_foliage_t& foliage =
        pointers.foliage[parms.foliageIndices[index].foliageIndex];
    idBounds bounds;
    CalculateFoliageBounds(&foliage, &pointers.attributes[foliage.attribIdx],
                           bounds);
    const float radius = (bounds.b[1].x - bounds.b[0].x) * 0.5f;
    const idColor& color = radius < 100.0f ? idColor::colorGreen
                                           : idColor::colorYellow;
    renderDebug->DebugBounds(color, bounds, idVec3{}, 0, true);
    if (mode == 2) {
      const idVec3 center{(bounds.b[0].x + bounds.b[1].x) * 0.5f,
                          (bounds.b[0].y + bounds.b[1].y) * 0.5f,
                          (bounds.b[0].z + bounds.b[1].z) * 0.5f};
      char text[96];
      std::snprintf(text, sizeof(text), "%d, %d, %d - %.2f",
                    foliage.xyz[0], foliage.xyz[1], foliage.xyz[2], radius);
      renderDebug->DebugText(text, center, 1.0f, idColor::colorRed,
                             viewAxis, 0, 0, true, true);
    }
  }
}

template <typename T>
T* AllocateFoliage(std::size_t count, const char* location) {
  return static_cast<T*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(T) * count, kFoliageMemoryTag, false, ALIGN_16,
      HEAP_DEFAULTHEAP));
}

} // namespace

idParallelJobRegistration register_FoliageGatherJob(
    reinterpret_cast<jobRun_t>(FoliageGatherJob), "FoliageGatherJob");
idParallelJobRegistration register_FoliageGenJob(
    reinterpret_cast<jobRun_t>(FoliageGenJob), "FoliageGenJob");

idRenderModelFoliage::~idRenderModelFoliage() {
  idMem* const memory = idMem::Singleton();
  memory->Free(foliageGatherParms, ALIGN_16);
  memory->Free(foliageGenParms, ALIGN_16);
  memory->Free(viewData, ALIGN_16);
  memory->Free(foliageIndices, ALIGN_16);
  memory->Free(influenceSphereIndices, ALIGN_16);
  memory->Free(subTreeFoliageStats, ALIGN_16);
}

void FoliageGatherJob(const deferredFoliageGatherParms_t* parms) {
  GatherFoliageIndicesForSubTree(parms);
}

idRenderModelFoliage::idRenderModelFoliage()
    : foliageBSP(nullptr),
      foliageGatherParms(AllocateFoliage<deferredFoliageGatherParms_t>(
          kMaxVisibleSubTrees,
          "w:\\build\\tech5\\engine\\models\\foliage\\RenderModelFoliage.cpp(62) : TAG_FOLIAGE")),
      foliageGenParms(AllocateFoliage<deferredFoliageGenParms_t>(
          kMaxVisibleSubTrees * kJobsPerSubTree,
          "w:\\build\\tech5\\engine\\models\\foliage\\RenderModelFoliage.cpp(63) : TAG_FOLIAGE")),
      viewData(AllocateFoliage<foliageRenderView_t>(
          1,
          "w:\\build\\tech5\\engine\\models\\foliage\\RenderModelFoliage.cpp(64) : TAG_FOLIAGE")),
      foliageIndices(AllocateFoliage<foliageIndex_t>(
          kMaximumFoliageIndices,
          "w:\\build\\tech5\\engine\\models\\foliage\\RenderModelFoliage.cpp(65) : TAG_FOLIAGE")),
      influenceSphereIndices(AllocateFoliage<std::int16_t>(
          kMaximumFoliageIndices,
          "w:\\build\\tech5\\engine\\models\\foliage\\RenderModelFoliage.cpp(66) : TAG_FOLIAGE")),
      subTreeFoliageStats(AllocateFoliage<foliageSubTreeStats_t>(
          kMaxVisibleSubTrees,
          "w:\\build\\tech5\\engine\\models\\foliage\\RenderModelFoliage.cpp(67) : TAG_FOLIAGE")) {
  renderModelType = RENDERMODEL_FOLIAGE;
  g.flags0 |= 0x80u;
  g.flags1 |= 0x21u;
  g.modelDepthSort = MODEL_DEPTH_SORT_LAST;
}

void FoliageGenJob(const deferredFoliageGenParms_t* parms) {
  int totalFoliage = 0;
  int totalOpaque = 0;
  GetFoliageTotalStats(parms->foliageStatsList, totalFoliage, totalOpaque);
  if (parms->opaqueQuadsNumAllocated != nullptr)
    *parms->opaqueQuadsNumAllocated = totalOpaque;
  if (parms->opaqueQuadsNumUsed != nullptr)
    *parms->opaqueQuadsNumUsed = totalOpaque;
  if (parms->transQuadsNumAllocated != nullptr)
    *parms->transQuadsNumAllocated = kMaximumFoliageQuads - totalOpaque;
  if (parms->transQuadsNumUsed != nullptr)
    *parms->transQuadsNumUsed = totalFoliage - totalOpaque;
  if (parms->transQuadsOffset != nullptr)
    *parms->transQuadsOffset = totalOpaque + parms->quadsOffset;

  const int totalTransVertices = 4 * (totalFoliage - totalOpaque);
  const int totalOpaqueVertices = 4 * totalOpaque;
  int foliageOffset = 0;
  int opaqueOffset = 0;
  GetFoliageOffsetForSubTree(parms->foliageStatsList,
                             parms->foliageStatsIndex, foliageOffset,
                             opaqueOffset);

  int jobOffset = 0;
  int jobCount = 0;
  const foliageSubTreeStats_t& stats =
      parms->foliageStatsList[parms->foliageStatsIndex];
  GetFoliageOffsetAndCountForJob(
      parms->numJobsForIndices, parms->jobIndexForIndices,
      stats.subTreeFoliageCount, jobOffset, jobCount);

  int jobOpaqueOffset = 0;
  int jobOpaqueCount = 0;
  if (stats.subTreeOpaqueCount > 0) {
    fm_subTreeDataPtrs_t pointers{};
    SetupDataPointers(pointers, parms->subTreeData);
    GetOpaqueOffsetAndCountForJob(jobOffset, jobCount, pointers,
                                  parms->foliageIndices, jobOpaqueOffset,
                                  jobOpaqueCount);
  }

  int opaqueVertexOffset = 4 * opaqueOffset + 4 * jobOpaqueOffset;
  int transVertexOffset =
      4 * (foliageOffset - opaqueOffset) +
      4 * (jobOffset - jobOpaqueOffset);
  opaqueVertexOffset = std::min(totalOpaqueVertices, opaqueVertexOffset);
  transVertexOffset = std::min(totalTransVertices, transVertexOffset);
  int numOpaque = jobOpaqueCount;
  int numTrans = jobCount - jobOpaqueCount;
  if (4 * numOpaque + opaqueVertexOffset > totalOpaqueVertices) {
    numOpaque -=
        (opaqueVertexOffset + 4 * numOpaque - totalOpaqueVertices) / 4;
    numOpaque = std::max(0, numOpaque);
  }
  if (4 * numTrans + transVertexOffset > totalTransVertices) {
    numTrans -=
        (transVertexOffset + 4 * numTrans - totalTransVertices) / 4;
    numTrans = std::max(0, numTrans);
  }
  GenFoliageForSubTree(
      parms, parms->foliageIndices + jobOffset, jobCount,
      opaqueVertexOffset, transVertexOffset + totalOpaqueVertices,
      numOpaque, numTrans);
}

bool idRenderModelFoliage::UpdateInView(
    const idRenderView* currentView, const idRenderView* nextView,
    idRenderModelUpdateTools* updateTools) {
  if (r_skipFoliage.valueInteger != 0 || foliageBSP == nullptr)
    return false;

  viewData->time = static_cast<float>(currentView->r.renderTime) * 0.001f;
  viewData->atlasWidth = static_cast<std::uint16_t>(
      globalImages->transSortImageAtlas->ATLAS_WIDTH);
  viewData->atlasHeight = static_cast<std::uint16_t>(
      globalImages->transSortImageAtlas->ATLAS_HEIGHT);
  viewData->nearDist = r_znear.valueFloat;
  viewData->farDist = r_foliageBigFadeDistMax.valueFloat;
  viewData->fov[0] =
      std::tan(nextView->r.fov_x * 0.017453292f * 0.5f);
  viewData->fov[1] =
      std::tan(nextView->r.fov_y * 0.017453292f * 0.5f);
  viewData->viewFwd = nextView->r.viewaxis.mat[0];
  viewData->viewLeft = nextView->r.viewaxis.mat[1];
  viewData->viewUp = nextView->r.viewaxis.mat[2];
  viewData->viewOrg = nextView->r.vieworg;

  idRenderMatrix projectionMatrix = nextView->projectionMatrix;
  const float zRange =
      r_foliageBigFadeDistMax.valueFloat - r_znear.valueFloat;
  projectionMatrix.m[11] =
      -(r_foliageBigFadeDistMax.valueFloat * 2.0f *
        r_znear.valueFloat) /
      zRange;
  projectionMatrix.m[10] =
      -(r_foliageBigFadeDistMax.valueFloat + r_znear.valueFloat) /
      zRange;
  idRenderMatrix::Multiply(projectionMatrix, nextView->viewMatrix,
                           viewData->mvpMatrix);

  int subTreeNums[kMaxVisibleSubTrees];
  fm_model_t& model = foliageBSP->foliageModelData;
  const int debugSubTreeNodes = r_showFoliageSubTreeNodes.valueInteger;
  idRenderDebug* const renderDebug = GetRenderDebug();
  if (debugSubTreeNodes >= 0 && debugSubTreeNodes < model.numSubTrees &&
      renderDebug != nullptr) {
    DebugSubTreeFoliage(renderDebug, model.subTrees[debugSubTreeNodes],
                        debugSubTreeNodes);
  }
  const int debugSubTreeBounds = r_showFoliageSubTreeBounds.valueInteger;
  if (debugSubTreeBounds >= 0 && debugSubTreeBounds < model.numSubTrees &&
      renderDebug != nullptr) {
    renderDebug->DebugBounds(idColor::colorGreen,
                             BoundsFromSubTree(
                                 model.subTrees[debugSubTreeBounds].bounds),
                             idVec3{}, 0, false);
  }
  const int numVisibleSubTrees = idFoliageModel::GetFoliageSubTreesForView(
      model, viewData, subTreeNums);
  if (numVisibleSubTrees == 0)
    return false;

  foliageSegmentStats_t segmentStats[2]{};
  for (int segment = 0; segment < 2; ++segment) {
    if (!idRenderModelTransparency::AllocateSegment(
            false, segment == 0, segmentStats[segment].quadsOffset,
            segmentStats[segment].quadsAllocated,
            segmentStats[segment].quadsUsed))
      return false;
  }

  int quadsOffset = 0;
  idTransparencyVert* quadVertices = nullptr;
  float* quadDepths = nullptr;
  if (!idRenderModelTransparency::AllocateQuads(
          kMaximumFoliageQuads, quadsOffset, quadVertices, quadDepths))
    return false;
  *segmentStats[0].quadsOffset = quadsOffset;
  *segmentStats[1].quadsOffset = quadsOffset;

  const int roundedSubTreeCount = (numVisibleSubTrees + 3) & ~3;
  const int roundedCapacity =
      ((kMaximumFoliageIndices / roundedSubTreeCount) + 7) & ~7;
  const int maxIndicesPerSubTree = std::min(4096, roundedCapacity);
  std::memset(subTreeFoliageStats, 0xff,
              sizeof(foliageSubTreeStats_t) * kMaxVisibleSubTrees);

  const visibleInfluenceSpheres_t* visibleInfluenceSpheres =
      updateTools->transparencyModel->visibleInfluenceSpheres;
  for (int index = 0; index < numVisibleSubTrees; ++index) {
    const fm_subTree_t& subTree = model.subTrees[subTreeNums[index]];
    deferredFoliageGatherParms_t& parms = foliageGatherParms[index];
    parms.viewData = viewData;
    parms.visibleInfluenceSpheres = visibleInfluenceSpheres;
    parms.subTreeData = subTree.data;
    parms.subTreeBounds = &subTree.bounds;
    parms.subTreeTreeNodes = subTree.treeNodes;
    parms.subTreeTreeNodesSize = subTree.totalTreeNodeSize;
    parms.maxFoliageHeight = subTree.maxFoliageHeight;
    parms.foliageStartFadeDist = r_foliageStartFadeDist.valueFloat;
    parms.foliageSmallFadeDistMax = r_foliageSmallFadeDistMax.valueFloat;
    parms.maxFoliageIndices = maxIndicesPerSubTree;
    parms.maxInfluenceSphereIndices = maxIndicesPerSubTree;
    parms.foliageStats = &subTreeFoliageStats[index];
    parms.foliageIndices = foliageIndices + index * maxIndicesPerSubTree;
    parms.influenceSphereIndices =
        influenceSphereIndices + index * maxIndicesPerSubTree;
    if (r_showFoliageNodeBounds.valueInteger != 0) {
      GatherFoliageIndicesForSubTree(&parms);
      DebugGatheredFoliage(renderDebug, parms, nextView->r.viewaxis,
                           r_showFoliageNodeBounds.valueInteger);
    }
    if (r_useFoliageGenJobs.valueInteger == 1) {
      updateTools->renderModelJobList->AddJob(
          reinterpret_cast<jobRun_t>(FoliageGatherJob), &parms);
    } else {
      GatherFoliageIndicesForSubTree(&parms);
    }
  }
  updateTools->renderModelJobList->InsertSyncPoint(SYNC_SIGNAL);
  updateTools->renderModelJobList->InsertSyncPoint(SYNC_SYNCHRONIZE);

  deferredFoliageGenParms_t* genParms = foliageGenParms;
  for (int subTreeIndex = 0; subTreeIndex < numVisibleSubTrees;
       ++subTreeIndex) {
    const fm_subTree_t& subTree = model.subTrees[subTreeNums[subTreeIndex]];
    deferredFoliageGenParms_t base{};
    base.viewData = viewData;
    base.visibleInfluenceSpheres = visibleInfluenceSpheres;
    base.influenceSphereIndices =
        influenceSphereIndices + subTreeIndex * maxIndicesPerSubTree;
    base.maxInfluenceSphereIndices = maxIndicesPerSubTree;
    base.subTreeBounds = &subTree.bounds;
    base.subTreeData = subTree.data;
    base.subTreeDataSize = subTree.totalDataSize;
    base.foliageIndices =
        foliageIndices + subTreeIndex * maxIndicesPerSubTree;
    base.maxFoliageIndices = maxIndicesPerSubTree;
    base.numJobsForIndices = kJobsPerSubTree;
    base.jobIndexForIndices = 0;
    base.foliageStatsList = subTreeFoliageStats;
    base.foliageStatsIndex = subTreeIndex;
    base.foliageStartFadeDist = r_foliageStartFadeDist.valueFloat;
    base.foliageSmallFadeDistMax = r_foliageSmallFadeDistMax.valueFloat;
    base.foliageBigFadeDistMax = r_foliageBigFadeDistMax.valueFloat;
    base.quadsOffset = quadsOffset;
    base.quadsVertices = quadVertices;
    base.quadsDepths = quadDepths;
    base.opaqueQuadsNumAllocated =
        subTreeIndex == 0 ? segmentStats[0].quadsAllocated : nullptr;
    base.opaqueQuadsNumUsed =
        subTreeIndex == 0 ? segmentStats[0].quadsUsed : nullptr;
    base.transQuadsOffset =
        subTreeIndex == 0 ? segmentStats[1].quadsOffset : nullptr;
    base.transQuadsNumAllocated =
        subTreeIndex == 0 ? segmentStats[1].quadsAllocated : nullptr;
    base.transQuadsNumUsed =
        subTreeIndex == 0 ? segmentStats[1].quadsUsed : nullptr;

    for (int jobIndex = 0; jobIndex < kJobsPerSubTree; ++jobIndex) {
      *genParms = base;
      genParms->jobIndexForIndices = jobIndex;
      if (jobIndex != 0) {
        genParms->opaqueQuadsNumAllocated = nullptr;
        genParms->opaqueQuadsNumUsed = nullptr;
        genParms->transQuadsOffset = nullptr;
        genParms->transQuadsNumAllocated = nullptr;
        genParms->transQuadsNumUsed = nullptr;
      }
      if (r_useFoliageGenJobs.valueInteger == 1) {
        updateTools->renderModelJobList->AddJob(
            reinterpret_cast<jobRun_t>(FoliageGenJob), genParms);
      } else {
        FoliageGenJob(genParms);
      }
      ++genParms;
    }
  }
  return true;
}
