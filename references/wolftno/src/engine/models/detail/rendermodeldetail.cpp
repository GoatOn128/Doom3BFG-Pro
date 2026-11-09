#include "engine/models/detail/rendermodeldetail.h"

#include "engine/models/detail/detailmodeldata.h"
#include "engine/models/renderworld.h"
#include "engine/models/transparency/rendermodeltransparency.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/render.h"
#include "engine/renderer/triangles.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>

extern idCVar r_znear;

idCVar r_useDetailGenJobs("r_useDetailGenJobs", "1", CVAR_INTEGER,
                          "0 = off, 1 = threaded", 0.0f, 1.0f);
idCVar r_detailGenJobsPerSubTree("r_detailGenJobsPerSubTree", "1",
                                 CVAR_INTEGER, "", 1.0f, 32.0f);
idCVar r_showDetailNodeBounds("r_showDetailNodeBounds", "0", CVAR_BOOL,
                              "show the individual detail model bounds");

namespace {

constexpr int kMaxVisibleSubTrees = 128;
constexpr int kMaxGenJobsPerSubTree = 32;
constexpr int kMaximumDetailIndices = 17280;
constexpr int kMaximumDetailVertices = 0x4000;
constexpr int kMaximumDetailIndexes = 6 * kMaximumDetailVertices;
constexpr memTag_t kDetailMemoryTag = static_cast<memTag_t>(0x62);

template <typename T>
T* AllocateDetail(std::size_t count, const char* location) {
  return static_cast<T*>(idMem::Singleton()->AllocWithLocation(
      location, sizeof(T) * count, kDetailMemoryTag, false, ALIGN_16,
      HEAP_DEFAULTHEAP));
}

} // namespace

idRenderModelDetail::~idRenderModelDetail() {
  if (deferredVerts != nullptr)
    vertexBuffers[currentIndex].UnmapBuffer();
  if (deferredIndices != nullptr)
    indexBuffers[currentIndex].UnmapBuffer();
  for (int index = 0; index < 3; ++index) {
    vertexBuffers[index].FreeBufferObject();
    indexBuffers[index].FreeBufferObject();
  }
  idMem* const memory = idMem::Singleton();
  memory->Free(detailGatherParms, ALIGN_16);
  memory->Free(detailGenParms, ALIGN_16);
  memory->Free(viewData, ALIGN_16);
  memory->Free(detailIndices, ALIGN_16);
  memory->Free(subTreeDetailCounts, ALIGN_16);
  memory->Free(totalsUsed, ALIGN_16);
}

static void DetailGatherJob(const deferredDetailGatherParms_t* parms) {
  int numDetailIndices = 0;
  GatherDetailIndicesForSubTree(parms, numDetailIndices);
}

static void DetailGenJob(const deferredDetailGenParms_t* parms) {
  if (parms->totalsUsed != nullptr)
    GetDetailTotalCount(parms->detailCountsList, parms->totalsUsed);

  int vertOffset = 0;
  int indexOffset = 0;
  GetDetailOffsetsForSubTree(parms->detailCountsList,
                             parms->detailCountsIndex, vertOffset,
                             indexOffset);
  const detailSubTreeStats_t& counts =
      parms->detailCountsList[parms->detailCountsIndex];
  if (vertOffset + counts.vertCount <= parms->numVertsAllocated &&
      indexOffset + counts.indexCount <= 6 * parms->numVertsAllocated) {
    detailGenTotals_t stats;
    GenDetailForSubTree(parms, parms->detailIndices,
                        counts.subTreeDetailCount, vertOffset, indexOffset,
                        vertOffset, stats);
  }
}

static idParallelJobRegistration register_DetailGatherJob(
    reinterpret_cast<jobRun_t>(DetailGatherJob), "DetailGatherJob");
static idParallelJobRegistration register_DetailGenJob(
    reinterpret_cast<jobRun_t>(DetailGenJob), "DetailGenJob");

idRenderModelDetail::idRenderModelDetail()
    : detailBSP(nullptr),
      detailGatherParms(nullptr),
      detailGenParms(nullptr),
      viewData(nullptr),
      currentIndex(0),
      detailIndices(nullptr),
      subTreeDetailCounts(nullptr),
      totalsUsed(nullptr),
      vertexBuffers{},
      indexBuffers{},
      deferredVerts(nullptr),
      deferredIndices(nullptr),
      detailMaterial(nullptr) {
  renderModelType = RENDERMODEL_DETAIL;
  g.flags0 |= 0x80u;
  g.flags1 |= 0x21u;
  g.modelDepthSort = MODEL_DEPTH_SORT_LAST;

  for (int index = 0; index < 3; ++index) {
    indexBuffers[index].AllocBufferObject(nullptr, kMaximumDetailIndexes * 2,
                                          BU_DYNAMIC);
    vertexBuffers[index].AllocBufferObject(nullptr, 0x80000, 31, BU_DYNAMIC);
  }

  detailMaterial = static_cast<const idMaterial*>(
      idMaterial::resourceList.FindWithInheritance(
          "genericDetailVmtr", true, true));
  idRenderModelSurface surface{};
  surface.geometry = new idTriangles{};
  surface.geometry->bounds =
      {{{-99999.0f, -99999.0f, -99999.0f},
        {99999.0f, 99999.0f, 99999.0f}}};
  surface.material = detailMaterial;
  surface.geometry->numIndexes = 0;
  surface.geometry->numVerts = 0;
  surface.geometry->vertexBuffer.Reference(vertexBuffers[2]);
  surface.geometry->indexBuffer.Reference(indexBuffers[2]);
  AddSurface(surface);

  detailGatherParms = AllocateDetail<deferredDetailGatherParms_t>(
      kMaxVisibleSubTrees,
      "w:\\build\\tech5\\engine\\models\\detail\\RenderModelDetail.cpp(86) : TAG_DETAIL");
  detailGenParms = AllocateDetail<deferredDetailGenParms_t>(
      kMaxVisibleSubTrees * kMaxGenJobsPerSubTree,
      "w:\\build\\tech5\\engine\\models\\detail\\RenderModelDetail.cpp(87) : TAG_DETAIL");
  viewData = AllocateDetail<detailRenderView_t>(
      1,
      "w:\\build\\tech5\\engine\\models\\detail\\RenderModelDetail.cpp(88) : TAG_DETAIL");
  detailIndices = AllocateDetail<std::uint16_t>(
      kMaximumDetailIndices,
      "w:\\build\\tech5\\engine\\models\\detail\\RenderModelDetail.cpp(89) : TAG_DETAIL");
  subTreeDetailCounts = AllocateDetail<detailSubTreeStats_t>(
      kMaxVisibleSubTrees,
      "w:\\build\\tech5\\engine\\models\\detail\\RenderModelDetail.cpp(90) : TAG_DETAIL");
  totalsUsed = AllocateDetail<detailGenTotals_t>(
      1,
      "w:\\build\\tech5\\engine\\models\\detail\\RenderModelDetail.cpp(91) : TAG_DETAIL");
}

bool idRenderModelDetail::UpdateInView(
    const idRenderView* currentView, const idRenderView* nextView,
    idRenderModelUpdateTools* updateTools) {
  if (r_skipDetail.valueInteger != 0) {
    for (int index = 0; index < surfaces.num; ++index) {
      surfaces[index].geometry->numVerts = 0;
      surfaces[index].geometry->numIndexes = 0;
      surfaces[index].material = nullptr;
    }
    return false;
  }
  if (detailBSP == nullptr)
    return false;

  if (deferredVerts != nullptr && vertexBuffers[currentIndex].size < 0)
    vertexBuffers[currentIndex].UnmapBuffer();
  if (deferredIndices != nullptr && indexBuffers[currentIndex].size < 0)
    indexBuffers[currentIndex].UnmapBuffer();

  idRenderModelSurface& surface = surfaces[0];
  surface.material = detailMaterial;
  surface.geometry->numVerts = totalsUsed->totalVerts;
  surface.geometry->numIndexes = totalsUsed->totalIndices;
  surface.geometry->vertexBuffer.Reference(vertexBuffers[currentIndex]);
  surface.geometry->indexBuffer.Reference(indexBuffers[currentIndex]);

  currentIndex = (currentIndex + 1) % 3;
  deferredVerts = static_cast<idDrawVert*>(
      vertexBuffers[currentIndex].MapBuffer(BM_WRITE_NOSYNC));
  deferredIndices = static_cast<std::uint16_t*>(
      indexBuffers[currentIndex].MapBuffer(BM_WRITE_NOSYNC));

  viewData->time = static_cast<float>(currentView->r.renderTime) * 0.001f;
  viewData->nearDist = r_znear.valueFloat;
  viewData->farDist = r_detailFadeDistMax.valueFloat;
  viewData->fov[0] =
      std::tan(nextView->r.fov_x * 0.017453292f * 0.5f);
  viewData->fov[1] =
      std::tan(nextView->r.fov_y * 0.017453292f * 0.5f);
  viewData->viewOrg = nextView->r.vieworg;
  viewData->viewFwd = nextView->r.viewaxis.mat[0];
  viewData->viewLeft = nextView->r.viewaxis.mat[1];
  viewData->viewUp = nextView->r.viewaxis.mat[2];
  idRenderMatrix::Multiply(nextView->projectionMatrix, nextView->viewMatrix,
                           viewData->mvpMatrix);

  dm_model_t& model = detailBSP->detailModelData;
  int subTreeNums[kMaxVisibleSubTrees];
  const int numVisibleSubTrees = idDetailModel::GetDetailSubTreesForView(
      model, viewData, subTreeNums);
  if (numVisibleSubTrees == 0)
    return false;

  const int useJobs = r_useDetailGenJobs.valueInteger;
  const int roundedSubTreeCount = (numVisibleSubTrees + 3) & ~3;
  const int maxIndicesPerSubTree =
      ((kMaximumDetailVertices / roundedSubTreeCount) + 7) & ~7;
  std::memset(subTreeDetailCounts, 0xff,
              sizeof(detailSubTreeStats_t) * kMaxVisibleSubTrees);

  for (int subTreeIndex = 0; subTreeIndex < numVisibleSubTrees;
       ++subTreeIndex) {
    const dm_subTree_t& subTree = model.subTrees[subTreeNums[subTreeIndex]];
    deferredDetailGatherParms_t& parms = detailGatherParms[subTreeIndex];
    parms.viewData = viewData;
    parms.subTreeBounds = &subTree.bounds;
    parms.subTreeTreeNodes = subTree.treeNodes;
    parms.subTreeTreeNodesSize = subTree.totalTreeNodeSize;
    parms.detailCount = &subTreeDetailCounts[subTreeIndex];
    parms.maxDetailIndices = maxIndicesPerSubTree;
    parms.detailIndices =
        detailIndices + subTreeIndex * maxIndicesPerSubTree;
    parms.maxDetailHeight = subTree.maxDetailHeight;

    if (r_showDetailNodeBounds.valueInteger != 0) {
      int numDetailIndices = 0;
      GatherDetailIndicesForSubTree(&parms, numDetailIndices);
      dm_subTreeDataPtrs_t pointers{};
      SetupSubTreePtrsFromData(pointers, subTree.data);
      for (int detailIndex = 0; detailIndex < numDetailIndices;
           ++detailIndex) {
        const dm_detail_t& detail =
            pointers.detail[parms.detailIndices[detailIndex]];
        idBounds detailBounds;
        CalculateDetailBounds(
            &detail,
            idDetailModelData::palette.list[
                pointers.attributes[detail.attribIdx].paletteIdx],
            detailBounds);
        GetRenderDebug()->DebugBounds(idColor::colorCyan, detailBounds,
                                      vec3_origin, 0, true);
      }
    }

    if (useJobs == 1) {
      updateTools->renderModelJobList->AddJob(
          reinterpret_cast<jobRun_t>(DetailGatherJob), &parms);
    } else {
      int numDetailIndices = 0;
      GatherDetailIndicesForSubTree(&parms, numDetailIndices);
    }
  }
  updateTools->renderModelJobList->InsertSyncPoint(SYNC_SIGNAL);
  updateTools->renderModelJobList->InsertSyncPoint(SYNC_SYNCHRONIZE);

  const int jobsPerSubTree = r_detailGenJobsPerSubTree.valueInteger;
  deferredDetailGenParms_t* genParms = detailGenParms;
  for (int subTreeIndex = 0; subTreeIndex < numVisibleSubTrees;
       ++subTreeIndex) {
    const dm_subTree_t& subTree = model.subTrees[subTreeNums[subTreeIndex]];
    genParms->viewData = viewData;
    genParms->subTreeBounds = &subTree.bounds;
    genParms->subTreeData = subTree.data;
    genParms->subTreeDataSize = subTree.totalDataSize;
    genParms->detailIndices =
        detailIndices + subTreeIndex * maxIndicesPerSubTree;
    genParms->maxDetailIndices = maxIndicesPerSubTree;
    genParms->numJobsForIndices = jobsPerSubTree;
    genParms->jobIndexForIndices = 0;
    genParms->detailCountsList = subTreeDetailCounts;
    genParms->detailCountsIndex = subTreeIndex;
    genParms->numVertsAllocated = kMaximumDetailVertices;
    genParms->verts = deferredVerts;
    genParms->indices = deferredIndices;
    genParms->paletteEntries = idDetailModel::paletteEntries.list;
    genParms->paletteEntriesSize =
        sizeof(detailPaletteEntry_t) * idDetailModel::paletteEntries.size;
    genParms->paletteIndices = idDetailModel::paletteIndices.list;
    genParms->paletteIndicesSize =
        sizeof(std::uint16_t) * idDetailModel::paletteIndices.size;
    genParms->paletteVerts = idDetailModel::paletteVerts.list;
    genParms->paletteVertsSize =
        sizeof(detailPaletteVert_t) * idDetailModel::paletteVerts.size;
    genParms->totalsUsed = subTreeIndex == 0 ? totalsUsed : nullptr;

    int jobNumber = 1;
    for (;;) {
      if (useJobs == 1) {
        updateTools->renderModelJobList->AddJob(
            reinterpret_cast<jobRun_t>(DetailGenJob), genParms);
      } else {
        DetailGenJob(genParms);
      }
      ++genParms;
      if (jobNumber >= jobsPerSubTree)
        break;
      std::memmove(genParms, genParms - 1,
                   sizeof(deferredDetailGenParms_t));
      genParms->jobIndexForIndices = jobNumber;
      genParms->totalsUsed = nullptr;
      ++jobNumber;
    }
  }
  return true;
}
