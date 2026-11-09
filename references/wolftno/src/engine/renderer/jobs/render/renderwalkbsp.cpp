#include "engine/renderer/jobs/render/renderwalkbsp.h"

#include "engine/renderer/jobs/render/graphicsapiwrapper.h"
#include "engine/renderer/render.h"

#include <algorithm>

namespace {
bool BoundsOverlap(const idBounds& left, const idBounds& right) {
  return left.b[0].x <= right.b[1].x && left.b[0].y <= right.b[1].y &&
         left.b[0].z <= right.b[1].z && right.b[0].x <= left.b[1].x &&
         right.b[0].y <= left.b[1].y && right.b[0].z <= left.b[1].z;
}
} // namespace

void RenderWalkBSP(const renderWalkBSPParms_t* parameters) {
  int singleWorldArea = parameters->settings->singleWorldArea;
  if (singleWorldArea >= 0) {
    if (singleWorldArea > parameters->numWorldAreas - 1)
      singleWorldArea = parameters->numWorldAreas - 1;
    parameters->nodeWorldAreas[0].areaNum =
        static_cast<std::int16_t>(singleWorldArea);
    parameters->nodeWorldAreas[0].nodeOcclusionTestIndex = -1;
    *parameters->numNodeWorldAreas = 1;
    return;
  }

  if (!parameters->settings->IsComboMap()) {
    for (int area = 0; area < parameters->numWorldAreas; ++area) {
      parameters->nodeWorldAreas[area].areaNum =
          static_cast<std::int16_t>(area);
      parameters->nodeWorldAreas[area].nodeOcclusionTestIndex = -1;
    }
    *parameters->numNodeWorldAreas = parameters->numWorldAreas;
    return;
  }

  int stackNodes[256];
  int stackOcclusionTestIndices[256];
  int stackDepth = 0;
  int nodeIndex = 0;
  int occlusionTestIndex = -1;
  bool popStack = false;

  for (;;) {
    if (popStack) {
      if (stackDepth == 0)
        return;
      --stackDepth;
      nodeIndex = stackNodes[stackDepth];
      occlusionTestIndex = stackOcclusionTestIndices[stackDepth];
      popStack = false;
    }

    if (parameters->areaNodeBoundsCulled[nodeIndex] != 0 &&
        !parameters->settings->skipNodeCPUCulling) {
      popStack = true;
      continue;
    }

    const int queryIndex = parameters->areaNodeOcclusionIndex[nodeIndex];
    const bool queryDisabled =
        queryIndex < 0 || queryIndex >= 64 ||
        parameters->settings->skipNodeGPUCulling ||
        BoundsOverlap(parameters->areaNodeBounds[nodeIndex],
                      parameters->settings->viewBounds);
    if (!queryDisabled) {
      occlusionTestIndex = *parameters->numOcclusionTestNodes;
      parameters->occlusionTestNodes[occlusionTestIndex] = nodeIndex;
      ++*parameters->numOcclusionTestNodes;
      const std::int64_t queryNumber =
          parameters->areaNodeOcclusionQuery[queryIndex];
      if (queryNumber >=
              parameters->renderView->firstOcclusionQueryInPreviousFrame &&
          GL_GetDeferredQueryResult(queryNumber) == 0) {
        popStack = true;
        continue;
      }
    }

    const areaNode_t& node = parameters->areaNodes[nodeIndex];
    const float distance = parameters->settings->viewOrigin.x * node.plane.a +
                           parameters->settings->viewOrigin.y * node.plane.b +
                           parameters->settings->viewOrigin.z * node.plane.c +
                           node.plane.d;
    const int nearSide = distance >= 0.0f ? 0 : 1;
    int nearChild = node.children[nearSide];
    int farChild = node.children[nearSide == 0 ? 1 : 0];

    if (nearChild > 0 && farChild > 0) {
      stackNodes[stackDepth] = farChild;
      stackOcclusionTestIndices[stackDepth] = occlusionTestIndex;
      ++stackDepth;
      nodeIndex = nearChild;
      continue;
    }

    int outputCount = *parameters->numNodeWorldAreas;
    if (nearChild < 0 && outputCount < 8192) {
      parameters->nodeWorldAreas[outputCount].areaNum =
          static_cast<std::int16_t>(-1 - nearChild);
      parameters->nodeWorldAreas[outputCount].nodeOcclusionTestIndex =
          static_cast<std::int16_t>(occlusionTestIndex);
      ++outputCount;
    }
    if (farChild < 0 && outputCount < 8192) {
      parameters->nodeWorldAreas[outputCount].areaNum =
          static_cast<std::int16_t>(-1 - farChild);
      parameters->nodeWorldAreas[outputCount].nodeOcclusionTestIndex =
          static_cast<std::int16_t>(occlusionTestIndex);
      ++outputCount;
    }
    *parameters->numNodeWorldAreas = outputCount;
    nodeIndex = std::max(nearChild, farChild);
    popStack = nodeIndex <= 0;
  }
}
