#include "engine/models/skeletalanimation/md6blockalloc.h"

#include "shared/idlib/lib.h"

namespace {

void RestoreNodeDefaults(idMD6Node* node, idMD6Node::nodeType_t type) {
  if (node == nullptr)
    return;
  node->type = type;
  switch (type) {
    case idMD6Node::NODE_BRANCH: {
      auto* branch = static_cast<idMD6Branch*>(node);
      branch->left = nullptr;
      branch->right = nullptr;
      branch->leftTimeOverride = -1;
      branch->rightTimeOverride = -1;
      branch->filterGroup = MD6_WEIGHTGROUP_MAX;
      branch->op = 11;
      branch->originBlend = 0;
      branch->currentAlpha = 0.0f;
      branch->targetAlpha = 0.0f;
      branch->alphaRate = 0.0f;
      branch->blendType = idMD6Branch::BLEND_LINEAR;
      break;
    }
    case idMD6Node::NODE_BEST_LEAF:
      static_cast<idMD6BestLeaf*>(node)->tagGroupFilter.type =
          idMD6Node::NODE_TAG_FILTER;
      break;
    default:
      break;
  }
}

} // namespace

idMD6BlockAlloc::idMD6BlockAlloc()
    : playLeaves(), pauseLeaves(), branches(), blendBranches(),
      blendAdditiveBranches(), fusionBranches(), bestLeaves(), tagFilters(),
      nodes(), nodesToDelete() {
  nodes.ClearFree();
  nodesToDelete.ClearFree();
}

idMD6BlockAlloc::~idMD6BlockAlloc() { DeferredFree(); }

std::size_t idMD6BlockAlloc::Size() const {
  return playLeaves.Size() + pauseLeaves.Size() + branches.Size() +
         blendBranches.Size() + blendAdditiveBranches.Size() +
         fusionBranches.Size() + bestLeaves.Size() + tagFilters.Size();
}

int idMD6BlockAlloc::NumNodes() const { return nodes.Num(); }

idMD6Node* idMD6BlockAlloc::NodeForIndex(int index) {
  return nodes[index];
}

const idMD6Node* idMD6BlockAlloc::NodeForIndex(int index) const {
  return nodes[index];
}

idMD6Node* idMD6BlockAlloc::Alloc(idMD6Node::nodeType_t type) {
  idMD6Node* node = nullptr;
  switch (type) {
    case idMD6Node::NODE_BRANCH:
      node = branches.Alloc();
      break;
    case idMD6Node::NODE_LEAF_PAUSE:
      node = pauseLeaves.Alloc();
      break;
    case idMD6Node::NODE_LEAF_PLAY:
      node = playLeaves.Alloc();
      break;
    case idMD6Node::NODE_BLEND_BRANCH:
      node = blendBranches.Alloc();
      break;
    case idMD6Node::NODE_BLENDA_BRANCH:
      node = blendAdditiveBranches.Alloc();
      break;
    case idMD6Node::NODE_FUSION_BRANCH:
      node = fusionBranches.Alloc();
      break;
    case idMD6Node::NODE_BEST_LEAF:
      node = bestLeaves.Alloc();
      break;
    case idMD6Node::NODE_TAG_FILTER:
      node = tagFilters.Alloc();
      break;
    default:
      break;
  }
  RestoreNodeDefaults(node, type);
  nodes.Append(node);
  return node;
}

void idMD6BlockAlloc::FreeMem(idMD6Node* node) {
  int index = -1;
  for (int nodeIndex = 0; nodeIndex < nodes.Num(); ++nodeIndex) {
    if (nodes[nodeIndex] == node) {
      index = nodeIndex;
      break;
    }
  }
  if (index < 0)
    idLib::FatalError("idMD6BlockAlloc::Free: remove failed");
  nodes.RemoveIndex(index);
  nodesToDelete.Append(node);
}

void idMD6BlockAlloc::Condense() { nodes.ResizeForPort(nodes.Num()); }

void idMD6BlockAlloc::DeferredFree() {
  for (int index = 0; index < nodesToDelete.Num(); ++index) {
    idMD6Node* node = nodesToDelete[index];
    switch (node->type) {
      case idMD6Node::NODE_BRANCH:
        branches.Free(static_cast<idMD6Branch*>(node));
        break;
      case idMD6Node::NODE_LEAF_PAUSE:
        pauseLeaves.Free(static_cast<idMD6LeafPause*>(node));
        break;
      case idMD6Node::NODE_LEAF_PLAY:
        playLeaves.Free(static_cast<idMD6LeafPlay*>(node));
        break;
      case idMD6Node::NODE_BLEND_BRANCH:
        blendBranches.Free(static_cast<idMD6BlendBranch*>(node));
        break;
      case idMD6Node::NODE_BLENDA_BRANCH:
        blendAdditiveBranches.Free(
            static_cast<idMD6BlendAdditiveBranch*>(node));
        break;
      case idMD6Node::NODE_FUSION_BRANCH:
        fusionBranches.Free(static_cast<idMD6FusionBranch*>(node));
        break;
      case idMD6Node::NODE_BEST_LEAF:
        bestLeaves.Free(static_cast<idMD6BestLeaf*>(node));
        break;
      case idMD6Node::NODE_TAG_FILTER:
        tagFilters.Free(static_cast<idMD6TagFilter*>(node));
        break;
      default:
        break;
    }
  }
  nodesToDelete.Clear();
}
