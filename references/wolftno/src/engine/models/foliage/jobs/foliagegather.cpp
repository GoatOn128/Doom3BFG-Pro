#include "engine/models/foliage/jobs/foliagegather.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

std::int16_t ToShort(float value) {
  return static_cast<std::int16_t>(
      std::clamp(static_cast<int>(value), -32768, 32767));
}

unsigned PartitionInfluences(foliageGather_t& gather, unsigned stackOffset,
                             int axis, float plane, bool frontChild) {
  unsigned scan = stackOffset;
  unsigned excluded = stackOffset;
  unsigned end = gather.influenceSphereStackSize;
  while (scan < end) {
    const influenceSphere_t& sphere =
        gather.influenceSpheres[gather.influenceSphereStack[scan]];
    const float coordinate = sphere.center[axis];
    const bool outside = frontChild
                             ? coordinate + sphere.outerRadius <= plane
                             : plane <= coordinate - sphere.outerRadius;
    if (outside) {
      ++excluded;
      ++scan;
    } else {
      --end;
      std::swap(gather.influenceSphereStack[scan],
                gather.influenceSphereStack[end]);
    }
  }
  return excluded;
}

void SetupTreePointers(fm_subTreeTreeNodePtrs_t& pointers,
                       const fm_subTreeTreeNodes_t* data) {
  auto* base = reinterpret_cast<std::uint8_t*>(
      const_cast<fm_subTreeTreeNodes_t*>(data));
  pointers.treeNodes =
      reinterpret_cast<fm_treeNode_t*>(base + data->treeNodeOffset);
  pointers.leafInfo =
      reinterpret_cast<fm_leafInfo_t*>(base + data->leafInfoOffset);
}

void SetupDataPointers(fm_subTreeDataPtrs_t& pointers,
                       const fm_subTreeData_t* data) {
  auto* base = reinterpret_cast<std::uint8_t*>(
      const_cast<fm_subTreeData_t*>(data));
  pointers.foliage = reinterpret_cast<fm_foliage_t*>(base + data->foliageOffset);
  pointers.attributes =
      reinterpret_cast<fm_attribute_t*>(base + data->attributeOffset);
}

} // namespace

bool R_CullFoliageBoundsToMVP(const std::int16_t (*const bounds)[3],
                              const float* mvp, idBounds& floatBounds) {
  floatBounds.b[0].x = static_cast<float>(bounds[0][0]);
  floatBounds.b[1].x = static_cast<float>(bounds[1][0]);
  floatBounds.b[0].y = static_cast<float>(bounds[0][1]);
  floatBounds.b[1].y = static_cast<float>(bounds[1][1]);
  floatBounds.b[0].z = static_cast<float>(bounds[0][2]);
  floatBounds.b[1].z = static_cast<float>(bounds[1][2]);
  int clipMask = 0;
  for (int x = 0; x < 2; ++x) {
    for (int y = 0; y < 2; ++y) {
      for (int z = 0; z < 2; ++z) {
        const idVec3 point{floatBounds.b[x].x, floatBounds.b[y].y,
                           floatBounds.b[z].z};
        const float clipW = mvp[14] * point.z +
                                (mvp[13] * point.y + mvp[12] * point.x) +
                            mvp[15];
        const float clipY = mvp[6] * point.z +
                                (mvp[5] * point.y + mvp[4] * point.x) +
                            mvp[7];
        const float clipX = mvp[2] * point.z +
                                (mvp[1] * point.y + mvp[0] * point.x) +
                            mvp[3];
        const float clipZ = mvp[10] * point.z +
                                (mvp[9] * point.y + mvp[8] * point.x) +
                            mvp[11];
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

void GetFoliageRenderListForView_r(fm_subTreeDataPtrs_t& data,
                                   fm_subTreeTreeNodePtrs_t& tree,
                                   fm_subTreeBounds_t& bounds,
                                   foliageGather_t& gather,
                                   unsigned nodeNum, unsigned, unsigned,
                                   unsigned stackOffset) {
  while ((nodeNum & 0x8000U) == 0) {
    const fm_treeNode_t& node = tree.treeNodes[nodeNum];
    const int axis = node.planeType & 3;
    bool anyFront = false;
    bool anyBack = false;
    for (int corner = 0; corner < 8; ++corner) {
      anyFront |= gather.frustumVerts[axis][corner] >= node.planeDist;
      anyBack |= gather.frustumVerts[axis][corner] <= node.planeDist;
    }
    if (anyFront) {
      if (anyBack) {
        const unsigned frontOffset = PartitionInfluences(
            gather, stackOffset, axis, static_cast<float>(node.planeDist), true);
        fm_subTreeBounds_t frontBounds = bounds;
        frontBounds.b[0][axis] = node.planeDist;
        GetFoliageRenderListForView_r(data, tree, frontBounds, gather,
                                      node.children[0],
                                      (node.planeType >> 2) & 7U,
                                      static_cast<std::uint8_t>(node.planeType) >> 5,
                                      frontOffset);
        stackOffset = PartitionInfluences(
            gather, stackOffset, axis, static_cast<float>(node.planeDist), false);
        bounds.b[1][axis] = node.planeDist;
        nodeNum = node.children[1];
      } else {
        stackOffset = PartitionInfluences(
            gather, stackOffset, axis, static_cast<float>(node.planeDist), true);
        bounds.b[0][axis] = node.planeDist;
        nodeNum = node.children[0];
      }
    } else {
      if (!anyBack)
        return;
      stackOffset = PartitionInfluences(
          gather, stackOffset, axis, static_cast<float>(node.planeDist), false);
      bounds.b[1][axis] = node.planeDist;
      nodeNum = node.children[1];
    }
  }

  const fm_leafInfo_t& leaf = tree.leafInfo[nodeNum & 0x7FFFU];
  idBounds floatBounds;
  if (R_CullFoliageBoundsToMVP(leaf.b, gather.mvpMatrix, floatBounds))
    return;

  const std::uint32_t influenceOffset = gather.numInfluenceSphereIndices;
  std::uint16_t storedInfluenceOffset = 0;
  if (stackOffset < gather.influenceSphereStackSize &&
      gather.numInfluenceSphereIndices + 1 < gather.maxInfluenceSphereIndices) {
    for (unsigned last = gather.influenceSphereStackSize - 1;
         last > stackOffset; --last) {
      for (unsigned index = stackOffset; index < last; ++index) {
        if (gather.influenceSphereStack[index] >
            gather.influenceSphereStack[index + 1]) {
          std::swap(gather.influenceSphereStack[index],
                    gather.influenceSphereStack[index + 1]);
        }
      }
    }
    const unsigned available = gather.maxInfluenceSphereIndices -
                               gather.numInfluenceSphereIndices;
    const unsigned end = std::min(gather.influenceSphereStackSize,
                                  stackOffset + available - 1);
    storedInfluenceOffset = static_cast<std::uint16_t>(influenceOffset);
    for (unsigned index = stackOffset; index < end; ++index) {
      gather.influenceSphereIndices[gather.numInfluenceSphereIndices++] =
          static_cast<std::int16_t>(gather.influenceSphereStack[index]);
    }
    gather.influenceSphereIndices[gather.numInfluenceSphereIndices++] = -1;
  }

  unsigned foliageCount = leaf.numBigFoliage + leaf.numSmallFoliage;
  foliageCount = std::min(foliageCount,
                          gather.maxFoliageIndices - gather.numFoliageIndices);
  const unsigned smallEnd = leaf.foliageOffset + leaf.numSmallFoliage;
  const float centerX = (floatBounds.b[1].x + floatBounds.b[0].x) * 0.5f;
  const float centerY = (floatBounds.b[0].y + floatBounds.b[1].y) * 0.5f;
  const float centerZ = (floatBounds.b[0].z + floatBounds.b[1].z) * 0.5f;
  const float extentY = floatBounds.b[1].y - centerY;
  const float zProjection =
      (floatBounds.b[1].z - centerZ) * gather.viewDir.z;
  const float absX =
      std::fabs((floatBounds.b[1].x - centerX) * gather.viewDir.x);
  const float projectedCenter =
      (centerY - gather.viewOrg.y) * gather.viewDir.y +
          (centerX - gather.viewOrg.x) * gather.viewDir.x +
      (centerZ - gather.viewOrg.z) * gather.viewDir.z;
  float nearestDepth = projectedCenter - absX;
  nearestDepth -= std::fabs(extentY * gather.viewDir.y);
  nearestDepth -= std::fabs(zProjection);
  const float fadeRange =
      gather.foliageSmallFadeDistMax - gather.foliageStartFadeDist;
  const unsigned foliageEnd = leaf.foliageOffset + foliageCount;
  std::uint32_t random = 1664525U * leaf.foliageOffset - 1117345U;
  for (unsigned foliageIndex = leaf.foliageOffset;
       foliageIndex < foliageEnd; ++foliageIndex, random += 1664525U) {
    const std::uint32_t bits = (random & 0x7FFFFFU) | 0x3F800000U;
    float unit;
    std::memcpy(&unit, &bits, sizeof(unit));
    unit -= 1.0f;
    const float fadeDistance = unit * unit * unit * fadeRange +
                               gather.foliageStartFadeDist;
    if (nearestDepth <= gather.foliageStartFadeDist ||
        foliageIndex >= smallEnd || nearestDepth <= fadeDistance) {
      const fm_foliage_t& foliage = data.foliage[foliageIndex];
      if (data.attributes[foliage.attribIdx].opaque != 0)
        ++gather.numOpaqueIndices;
      foliageIndex_t& output =
          gather.foliageIndices[gather.numFoliageIndices++];
      output.foliageIndex = static_cast<std::uint16_t>(foliageIndex);
      output.influenceSphereOffset = storedInfluenceOffset;
    }
  }
}

void GatherFoliageIndicesForSubTree(const deferredFoliageGatherParms_t* parms) {
  *parms->foliageStats = {};
  if (parms->subTreeTreeNodes->numTreeNodes == 0)
    return;

  const foliageRenderView_t& view = *parms->viewData;
  foliageGather_t gather{};
  gather.mvpMatrix = view.mvpMatrix.m;
  gather.viewDir = view.viewFwd;
  gather.viewOrg = view.viewOrg;
  const float distances[2] = {view.nearDist, view.farDist};
  const float maxFoliageHeight =
      static_cast<float>(parms->maxFoliageHeight);
  idVec3 frustumVerts[8];
  int corner = 0;
  for (const float distance : distances) {
    for (int axis = 0; axis < 3; ++axis) {
      const float center =
          view.viewOrg[axis] + view.viewFwd[axis] * distance;
      const float horizontal =
          (view.viewLeft[axis] * distance) * view.fov[0];
      const float vertical =
          (view.viewUp[axis] * distance) * view.fov[1];
      const float height = view.viewUp[axis] * maxFoliageHeight;
      frustumVerts[corner + 0][axis] =
          center + horizontal + vertical + height;
      frustumVerts[corner + 1][axis] =
          center - horizontal + vertical + height;
      frustumVerts[corner + 2][axis] =
          center - horizontal - vertical - height;
      frustumVerts[corner + 3][axis] =
          center + horizontal - vertical - height;
    }
    for (int point = 0; point < 4; ++point) {
      const idVec3& frustumVertex = frustumVerts[corner + point];
      gather.frustumVerts[0][corner + point] = ToShort(frustumVertex.x);
      gather.frustumVerts[1][corner + point] = ToShort(frustumVertex.y);
      gather.frustumVerts[2][corner + point] = ToShort(frustumVertex.z);
    }
    corner += 4;
  }

  fm_subTreeTreeNodePtrs_t treePointers{};
  fm_subTreeDataPtrs_t dataPointers{};
  SetupTreePointers(treePointers, parms->subTreeTreeNodes);
  SetupDataPointers(dataPointers, parms->subTreeData);
  gather.influenceSpheres = parms->visibleInfluenceSpheres->spheres;
  for (int index = 0; index < parms->visibleInfluenceSpheres->numSpheres;
       ++index) {
    const influenceSphere_t& sphere = gather.influenceSpheres[index];
    bool intersects = true;
    for (int axis = 0; axis < 3; ++axis) {
      intersects &= parms->subTreeBounds->b[0][axis] <=
                    sphere.center[axis] + sphere.outerRadius;
      intersects &= sphere.center[axis] - sphere.outerRadius <=
                    parms->subTreeBounds->b[1][axis];
    }
    if (intersects)
      gather.influenceSphereStack[gather.influenceSphereStackSize++] = index;
  }
  gather.foliageIndices = parms->foliageIndices;
  gather.maxFoliageIndices = parms->maxFoliageIndices;
  gather.influenceSphereIndices = parms->influenceSphereIndices;
  gather.maxInfluenceSphereIndices = parms->maxInfluenceSphereIndices;
  gather.influenceSphereIndices[0] = -1;
  gather.numInfluenceSphereIndices = 1;
  gather.foliageStartFadeDist = parms->foliageStartFadeDist;
  gather.foliageSmallFadeDistMax = parms->foliageSmallFadeDistMax;
  fm_subTreeBounds_t bounds = *parms->subTreeBounds;
  GetFoliageRenderListForView_r(dataPointers, treePointers, bounds, gather,
                                0, 0, 0, 0);
  parms->foliageStats->subTreeFoliageCount = gather.numFoliageIndices;
  parms->foliageStats->subTreeOpaqueCount = gather.numOpaqueIndices;
  parms->foliageStats->subTreeInfluenceCount =
      gather.numInfluenceSphereIndices;
}
