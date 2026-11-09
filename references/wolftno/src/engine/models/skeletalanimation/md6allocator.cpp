#include "engine/animation/md6.h"

namespace {

template <typename T, int Tag>
void SetMD6ListDefaults(idList<T, Tag>& list) {
  list.granularity = 30;
  list.memTag = 0;
  list.listStatic = 0;
}

} // namespace

idMD6BestLeaf::~idMD6BestLeaf() = default;

idMD6BlendBranch_Base::idMD6BlendBranch_Base(
    idMD6Node::nodeType_t nodeType) {
  type = nodeType;
  left = nullptr;
  right = nullptr;
  leftTimeOverride = -1;
  rightTimeOverride = -1;
  filterGroup = MD6_WEIGHTGROUP_MAX;
  op = 11;
  originBlend = 0;
  currentAlpha = 0.0f;
  targetAlpha = 0.0f;
  alphaRate = 0.0f;
  blendType = BLEND_LINEAR;
  SetMD6ListDefaults(coordinateList);
  SetMD6ListDefaults(animationList);
}

idMD6BlendBranch_Base::~idMD6BlendBranch_Base() = default;

idMD6BlendBranch::~idMD6BlendBranch() = default;

idMD6FusionBranch::~idMD6FusionBranch() = default;

idMD6BlendBranch::idMD6BlendBranch()
    : idMD6BlendBranch_Base(idMD6Node::NODE_BLEND_BRANCH) {
  SetMD6ListDefaults(currentCoordinate);
  SetMD6ListDefaults(userChannelIndices);
  SetMD6ListDefaults(activeCoordinate);
  SetMD6ListDefaults(activeIndices);
  SetMD6ListDefaults(branchList);
  numDimensions = 1;
  isAngleWrapped = false;
}

idMD6BlendBranch::idMD6BlendBranch(idMD6Node::nodeType_t nodeType)
    : idMD6BlendBranch_Base(nodeType) {
  SetMD6ListDefaults(currentCoordinate);
  SetMD6ListDefaults(userChannelIndices);
  SetMD6ListDefaults(activeCoordinate);
  SetMD6ListDefaults(activeIndices);
  SetMD6ListDefaults(branchList);
  numDimensions = 1;
  isAngleWrapped = false;
}

idMD6FusionBranch::idMD6FusionBranch()
    : idMD6BlendBranch(idMD6Node::NODE_FUSION_BRANCH) {
  SetMD6ListDefaults(phaseToFrameLookupData);
}

void idMD6Allocator::FreeTyped(idMD6Allocator* allocator,
                               const idMD6Node* node) {
  if (node == nullptr)
    return;
  idMD6Node* mutableNode = const_cast<idMD6Node*>(node);
  switch (node->type) {
    case idMD6Node::NODE_BRANCH:
    case idMD6Node::NODE_LEAF_PLAY:
    case idMD6Node::NODE_TAG_FILTER:
      allocator->FreeMem(mutableNode);
      break;
    case idMD6Node::NODE_LEAF_PAUSE:
      allocator->Free(static_cast<idMD6LeafPause*>(mutableNode));
      break;
    case idMD6Node::NODE_BLEND_BRANCH:
      allocator->Free(static_cast<idMD6BlendBranch*>(mutableNode));
      break;
    case idMD6Node::NODE_BLENDA_BRANCH:
      allocator->Free(static_cast<idMD6BlendAdditiveBranch*>(mutableNode));
      break;
    case idMD6Node::NODE_FUSION_BRANCH:
      allocator->Free(static_cast<idMD6FusionBranch*>(mutableNode));
      break;
    case idMD6Node::NODE_BEST_LEAF:
      allocator->Free(static_cast<idMD6BestLeaf*>(mutableNode));
      break;
    default:
      break;
  }
}

template void idMD6Allocator::Free<idMD6LeafPause>(idMD6LeafPause*);
template void idMD6Allocator::Free<idMD6BlendBranch>(idMD6BlendBranch*);
template void idMD6Allocator::Free<idMD6BestLeaf>(idMD6BestLeaf*);
template void idMD6Allocator::Free<idMD6BlendAdditiveBranch>(
    idMD6BlendAdditiveBranch*);
template void idMD6Allocator::Free<idMD6FusionBranch>(idMD6FusionBranch*);
