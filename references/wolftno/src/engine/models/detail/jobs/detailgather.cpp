#include "engine/models/detail/jobs/detailgather.h"

#include <algorithm>

namespace {

std::int16_t ToDetailShort(float value) {
  return static_cast<std::int16_t>(
      std::clamp(static_cast<int>(value), -32768, 32767));
}

} // namespace

void SetupSubTreePtrsFromTreeNodes(dm_subTreeTreeNodePtrs_t& subTreePtrs,
                                   const dm_subTreeTreeNodes_t* subTree) {
  auto* base = reinterpret_cast<std::uint8_t*>(
      const_cast<dm_subTreeTreeNodes_t*>(subTree));
  subTreePtrs.treeNodes =
      reinterpret_cast<dm_treeNode_t*>(base + subTree->treeNodeOffset);
}

bool R_CullDetailBoundsToMVP(const dm_subTreeBounds_t& bounds,
                             int maxDetailHeight, const float* mvp) {
  idVec3 floatBounds[2] = {
      {static_cast<float>(bounds.b[0][0]),
       static_cast<float>(bounds.b[0][1]),
       static_cast<float>(bounds.b[0][2])},
      {static_cast<float>(bounds.b[1][0]),
       static_cast<float>(bounds.b[1][1]),
       static_cast<float>(bounds.b[1][2]) +
           static_cast<float>(maxDetailHeight)}};
  int clipMask = 0;
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y) {
      for (int z = 0; z < 2; ++z) {
        const idVec3 point{floatBounds[x].x, floatBounds[y].y,
                           floatBounds[z].z};
        const float clipW = mvp[12] * point.x + mvp[13] * point.y +
                            mvp[14] * point.z + mvp[15];
        const float clipX = mvp[0] * point.x + mvp[1] * point.y +
                            mvp[2] * point.z + mvp[3];
        const float clipY = mvp[4] * point.x + mvp[5] * point.y +
                            mvp[6] * point.z + mvp[7];
        const float clipZ = mvp[8] * point.x + mvp[9] * point.y +
                            mvp[10] * point.z + mvp[11];
        if (clipX > -clipW) clipMask |= 1;
        if (clipW > clipX) clipMask |= 2;
        if (clipY > -clipW) clipMask |= 4;
        if (clipW > clipY) clipMask |= 8;
        if (clipZ > -clipW) clipMask |= 16;
        if (clipW > clipZ) clipMask |= 32;
      }
    }
  }
  return clipMask != 63;
}

void GetDetailRenderListForView_r(dm_subTreeTreeNodePtrs_t& subTreePtrs,
                                  dm_subTreeBounds_t& bounds,
                                  detailGather_t& gather,
                                  std::uint16_t nodeNum,
                                  std::uint16_t numDetailAtLeaf,
                                  std::uint32_t counts) {
  while (static_cast<std::int16_t>(nodeNum) >= 0) {
    const dm_treeNode_t& node =
        subTreePtrs.treeNodes[nodeNum];
    const int axis = node.planeType & 3;
    bool anyFront = false;
    bool anyBack = false;
    for (int corner = 0; corner < 8; ++corner) {
      anyFront |= gather.frustumVerts[axis][corner] >= node.planeDist;
      anyBack |= gather.frustumVerts[axis][corner] <= node.planeDist;
    }

    if (anyFront) {
      if (!anyBack) {
        bounds.b[0][axis] = node.planeDist;
        nodeNum = node.children[0];
        numDetailAtLeaf = (node.planeType >> 2) & 0x7f;
        counts = node.counts[0];
        continue;
      }
      dm_subTreeBounds_t frontBounds = bounds;
      frontBounds.b[0][axis] = node.planeDist;
      GetDetailRenderListForView_r(
          subTreePtrs, frontBounds, gather, node.children[0],
          (node.planeType >> 2) & 0x7f, node.counts[0]);
      bounds.b[1][axis] = node.planeDist;
    } else {
      if (!anyBack)
        return;
      bounds.b[1][axis] = node.planeDist;
    }

    nodeNum = node.children[1];
    numDetailAtLeaf = node.planeType >> 9;
    counts = node.counts[1];
  }

  if (R_CullDetailBoundsToMVP(bounds, gather.maxDetailHeight,
                              gather.mvpMatrix))
    return;

  std::uint16_t detailIndex =
      static_cast<std::uint16_t>(nodeNum) & 0x7fffu;
  const std::uint32_t remaining =
      gather.maxDetailIndices - gather.numDetailIndices;
  if (numDetailAtLeaf > remaining)
    numDetailAtLeaf = static_cast<std::uint16_t>(remaining);
  const int end = detailIndex + numDetailAtLeaf;
  while (detailIndex < end)
    gather.detailIndices[gather.numDetailIndices++] = detailIndex++;
  gather.vertCount += static_cast<std::uint16_t>(counts);
  gather.indexCount += counts >> 16;
}

void GatherDetailIndicesForSubTree(const deferredDetailGatherParms_t* parms,
                                   int& numDetailIndices) {
  numDetailIndices = 0;
  *parms->detailCount = {};
  if (parms->subTreeTreeNodes->numTreeNodes == 0)
    return;

  const detailRenderView_t& view = *parms->viewData;
  detailGather_t gather{};
  gather.maxDetailHeight = parms->maxDetailHeight;
  gather.mvpMatrix = view.mvpMatrix.m;
  const float distances[2] = {view.nearDist, view.farDist};
  int corner = 0;
  for (float distance : distances) {
    const idVec3 center = view.viewOrg + view.viewFwd * distance;
    const idVec3 horizontal = view.viewLeft * (distance * view.fov[0]);
    const idVec3 vertical = view.viewUp * (distance * view.fov[1]);
    const idVec3 height =
        view.viewUp * static_cast<float>(parms->maxDetailHeight);
    const idVec3 corners[4] = {center + horizontal + vertical,
                               center - horizontal + vertical,
                               center - horizontal - vertical - height,
                               center + horizontal - vertical - height};
    for (const idVec3& point : corners) {
      gather.frustumVerts[0][corner] = ToDetailShort(point.x);
      gather.frustumVerts[1][corner] = ToDetailShort(point.y);
      gather.frustumVerts[2][corner] = ToDetailShort(point.z);
      ++corner;
    }
  }

  dm_subTreeTreeNodePtrs_t subTreePtrs{};
  SetupSubTreePtrsFromTreeNodes(subTreePtrs, parms->subTreeTreeNodes);
  gather.detailIndices = parms->detailIndices;
  gather.maxDetailIndices = parms->maxDetailIndices;
  dm_subTreeBounds_t treeBounds = *parms->subTreeBounds;
  GetDetailRenderListForView_r(subTreePtrs, treeBounds, gather, 0, 0, 0);
  parms->detailCount->subTreeDetailCount =
      static_cast<int>(gather.numDetailIndices);
  parms->detailCount->vertCount = static_cast<int>(gather.vertCount);
  parms->detailCount->indexCount = static_cast<int>(gather.indexCount);
  numDetailIndices = static_cast<int>(gather.numDetailIndices);
}
