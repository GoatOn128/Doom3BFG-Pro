#define WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI
#define WOLFTNO_RETAIL_ANIMWEB_GETNAME_POINTER_ABI
#define WOLFTNO_RETAIL_ANIMWEB_GETSCALAR_REFERENCE_ABI
#define WOLFTNO_RETAIL_ANIMWEB_RESTART_FRAME_INT_ABI
#define WOLFTNO_RETAIL_FIND_ALIAS_REFERENCE_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#include "engine/gamelib/animstack/animweb/animwebblendtree.h"

#include "engine/models/skeletalanimation/animation.h"
#include "engine/models/skeletalanimation/md6skel.h"
#include "shared/idlib/lib.h"
#undef WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_FIND_ALIAS_REFERENCE_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_RESTART_FRAME_INT_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_GETSCALAR_REFERENCE_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_GETNAME_POINTER_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI

#include <cmath>

namespace {

constexpr float SMALLEST_NORMAL_FLOAT = 1.1754944e-38f;

int AnimationFrameCount(const idMD6Anim* animationValue) {
  return animationValue->animData != nullptr
             ? animationValue->animData->numFrames
             : 0;
}

}  // namespace

void idAnimWebBlendTree::StartTreePlaying_r(
    idAnimator_AnimWeb* animator, const idDeclMD6* md6Decl,
    const idMD6AnimProps* animProps, int numAnimProps, int curTime,
    int ticksPerSec, idMD6Node* node) {
  for (;;) {
    if (node->type == idMD6Node::NODE_BRANCH) {
      idMD6Branch* const branch = static_cast<idMD6Branch*>(node);
      StartTreePlaying_r(animator, md6Decl, animProps, numAnimProps,
                         curTime, ticksPerSec, branch->left);
      node = branch->right;
      continue;
    }

    if (node->type == idMD6Node::NODE_BLEND_BRANCH ||
        node->type == idMD6Node::NODE_BLENDA_BRANCH ||
        node->type == idMD6Node::NODE_FUSION_BRANCH) {
      if (node->type == idMD6Node::NODE_BLENDA_BRANCH) {
        idMD6BlendAdditiveBranch* const additive =
            static_cast<idMD6BlendAdditiveBranch*>(node);
        if (additive->baseAnimation != nullptr) {
          StartTreePlaying_r(animator, md6Decl, animProps, numAnimProps,
                             curTime, ticksPerSec,
                             additive->baseAnimation);
        }
      }

      idMD6BlendBranch_Base* const blend =
          static_cast<idMD6BlendBranch_Base*>(node);
      for (int index = 0; index < blend->animationList.num; ++index) {
        if (blend->animationList.list[index] != nullptr) {
          StartTreePlaying_r(animator, md6Decl, animProps, numAnimProps,
                             curTime, ticksPerSec,
                             blend->animationList.list[index]);
        }
      }
      if (node->type == idMD6Node::NODE_FUSION_BRANCH) {
        idMD6BlendBranch* const fusion =
            static_cast<idMD6BlendBranch*>(node);
        fusion->Update(true, fusion->userChannelIndices.num <= 0);
      }
      return;
    }

    if (node->type == idMD6Node::NODE_BEST_LEAF) {
      idMD6BestLeaf* const bestLeaf = static_cast<idMD6BestLeaf*>(node);
      for (int index = 0; index < bestLeaf->leafList.num; ++index) {
        if (bestLeaf->leafList.list[index] != nullptr) {
          StartTreePlaying_r(animator, md6Decl, animProps, numAnimProps,
                             curTime, ticksPerSec,
                             bestLeaf->leafList.list[index]);
        }
      }
      return;
    }

    if (node->type == idMD6Node::NODE_LEAF_PLAY) {
      idMD6LeafPlay* const leaf = static_cast<idMD6LeafPlay*>(node);
      for (int index = 0; index < numAnimProps; ++index) {
        if (animProps[index].SelectAnim(md6Decl, 0) != leaf->anim) {
          continue;
        }
        leaf->Restart(
            curTime, ticksPerSec, 0,
            static_cast<idMD6Leaf::wrapMode_t>(animProps[index].wrap));
        leaf->initCounter = static_cast<std::uint8_t>((curTime >> 4) % 255);
        if (animator->channelWeightAnimator.baseAnimation.animation != nullptr &&
            animator->channelWeightAnimator.leaves.num > 0 &&
            animator->channelWeightAnimator.userChannelToLeafMap.num > 0) {
          animator->channelWeightAnimator.SetControlAnimation(leaf, nullptr);
        }
        return;
      }
    }
    return;
  }
}

void idAnimWebBlendTree::CopyScalars(const idAnimator_AnimWeb* animator,
                                     int curTime, int ticksPerSec,
                                     std::uint8_t copyFlags) const {
  int leafNumber = 0;
  for (int pairIndex = 0; pairIndex < scalarPairs.num; ++pairIndex) {
    idAnimWebScalarPair& pair = scalarPairs.list[pairIndex];
    if ((pair.fieldFlags & copyFlags) == 0) {
      continue;
    }

    idMD6Node* const node = pair.node;

    if (node->type == idMD6Node::NODE_BLEND_BRANCH ||
        node->type == idMD6Node::NODE_FUSION_BRANCH) {
      if (pair.fieldType == 4) {
        const float scalar =
            animator->scalars.list[pair.scalarIndex].GetScalar(
                animator->scalarMem);
        static_cast<idMD6BlendBranch*>(node)
            ->currentCoordinate.list[pair.coordinateIndex] = scalar;
      }
      continue;
    }

    if (node->type == idMD6Node::NODE_BLENDA_BRANCH) {
      if (pair.fieldType == 4) {
        const float scalar =
            animator->scalars.list[pair.scalarIndex].GetScalar(
                animator->scalarMem);
        static_cast<idMD6BlendAdditiveBranch*>(node)->currentCoordinate =
            scalar;
      }
      continue;
    }

    if (node->type == idMD6Node::NODE_BRANCH) {
      if (pair.fieldType == 0) {
        idMD6Branch* const branch = static_cast<idMD6Branch*>(node);
        branch->currentAlpha =
            animator->scalars.list[pair.scalarIndex].GetScalar(
                animator->scalarMem);
        branch->targetAlpha =
            animator->scalars.list[pair.scalarIndex].GetScalar(
                animator->scalarMem);
        leafNumber = 0;
        branch->alphaRate = 0.0f;
      }
      continue;
    }

    if (node->type == idMD6Node::NODE_BEST_LEAF) {
      if (pair.fieldType == 0 && (pair.fieldFlags & 0x10u) != 0u) {
        const float scalar =
            animator->scalars.list[pair.scalarIndex].GetScalar(
                animator->scalarMem);
        if (std::fabs(scalar) >= SMALLEST_NORMAL_FLOAT) {
          static_cast<idMD6BestLeaf*>(node)->desiredTag |= 1u << pair.data1;
        }
      }
      continue;
    }

    if (node->type == idMD6Node::NODE_LEAF_PLAY) {
      idMD6LeafPlay* const playLeaf = static_cast<idMD6LeafPlay*>(node);
      if (pair.fieldType == 1) {
        const float scalar =
            animator->scalars.list[pair.scalarIndex].GetScalar(
                animator->scalarMem);
        playLeaf->SetRateScale(curTime, scalar);
        continue;
      }
      if (pair.fieldType != 2) {
        continue;
      }

      int animationIndex = static_cast<int>(
          animator->scalars.list[pair.scalarIndex].GetScalar(
              animator->scalarMem));
      if ((pair.fieldFlags & 4u) != 0u) {
        animationIndex %= static_cast<int>(pair.data1);
      }
      if ((pair.fieldFlags & 8u) != 0u) {
        animationIndex += pair.data2;
      }
      if (animationIndex >= 0 && animationIndex < leaves.num) {
        const idMD6LeafPlay* const indexedLeaf = leaves.list[animationIndex];
        if (indexedLeaf != nullptr) {
          playLeaf->anim = indexedLeaf->anim;
          if ((pair.fieldFlags & 0x20u) == 0u) {
            playLeaf->SetRateScale(curTime, indexedLeaf->GetRateScale());
          }
          const int numFrames = AnimationFrameCount(indexedLeaf->anim);
          if (playLeaf->GetFrame(curTime, ticksPerSec) >= numFrames) {
            const char* const name = playLeaf->anim->name.str;
            idLib::Warning(
                "Leaf '%s' current frame is out of range for indexed anim "
                "'%s'! Cur = %u, range = [0, %u)",
                name, name, playLeaf->GetFrame(curTime, ticksPerSec),
                AnimationFrameCount(indexedLeaf->anim));
          }
        }
        leafNumber = 0;
        continue;
      }

      idStr leafAnims;
      for (; leafNumber < leaves.num; ++leafNumber) {
        const idMD6Anim* const animationValue =
            leaves.list[leafNumber]->anim;
        if (animationValue != nullptr) {
          leafAnims += va("\tanim %d: %s\n", leafNumber,
                          animationValue->name.str);
        }
      }
      idLib::Warning(
          "Anim array index out of range! Index = %d, range = [0,%d)%s",
          animationIndex, leaves.num, leafAnims.c_str());
      leafNumber = 0;
      continue;
    }

    if (node->type == idMD6Node::NODE_LEAF_PAUSE) {
      idMD6LeafPause* const pauseLeaf = static_cast<idMD6LeafPause*>(node);
      if (pair.fieldType == 3) {
        pauseLeaf->frame =
            animator->scalars.list[pair.scalarIndex].GetScalar(
                animator->scalarMem);
        continue;
      }
      if (pair.fieldType != 2) {
        continue;
      }

      int animationIndex = static_cast<int>(
          animator->scalars.list[pair.scalarIndex].GetScalar(
              animator->scalarMem));
      if ((pair.fieldFlags & 4u) != 0u) {
        animationIndex %= static_cast<int>(pair.data1);
      }
      if ((pair.fieldFlags & 8u) != 0u) {
        animationIndex += pair.data2;
      }
      if (animationIndex >= 0 && animationIndex < leaves.num) {
        const idMD6LeafPlay* const indexedLeaf = leaves.list[animationIndex];
        if (indexedLeaf != nullptr) {
          pauseLeaf->anim = indexedLeaf->anim;
          const int numFrames = AnimationFrameCount(indexedLeaf->anim);
          if (pauseLeaf->frame >= static_cast<float>(numFrames)) {
            const char* const name = pauseLeaf->anim->name.str;
            idLib::Warning(
                "Leaf '%s' current frame is out of range for indexed anim "
                "'%s'! Cur = %f, range = [0, %u)",
                name, name, pauseLeaf->frame,
                AnimationFrameCount(pauseLeaf->anim));
          }
        }
        continue;
      }

      idStr leafAnims;
      for (; leafNumber < leaves.num; ++leafNumber) {
        const idMD6Anim* const animationValue =
            leaves.list[leafNumber]->anim;
        if (animationValue != nullptr) {
          leafAnims += va("\tanim %d: %s\n", leafNumber,
                          animationValue->name.str);
        }
      }
      idLib::Warning(
          "Anim array index out of range! Index = %d, range = [0,%d)%s",
          animationIndex, leaves.num, leafAnims.c_str());
      leafNumber = 0;
    }
  }
}

bool idAnimWebBlendTree::GetAnimLength(
    int curTime, int ticksPerSec, idMD6Leaf::wrapMode_t wrapMode,
    bool ignoreAdditive, int& curFrame, int& numFrames,
    int& frameRate) const {
  idMD6Node* stack[32];
  stack[0] = tree;
  numFrames = 0;
  int stackIndex = 0;
  do {
    idMD6Node* const node = stack[stackIndex];
    if (node->type == idMD6Node::NODE_BRANCH ||
        node->type == idMD6Node::NODE_BLEND_BRANCH ||
        node->type == idMD6Node::NODE_BLENDA_BRANCH ||
        node->type == idMD6Node::NODE_FUSION_BRANCH) {
      const idMD6Branch* const branch = static_cast<idMD6Branch*>(node);
      --stackIndex;
      if (branch->left != nullptr &&
          !branch->IsChildBlendedOut(branch->left)) {
        stack[++stackIndex] = branch->left;
      }
      if (branch->right != nullptr &&
          !branch->IsChildBlendedOut(branch->right)) {
        stack[++stackIndex] = branch->right;
      }
    } else if (node->type == idMD6Node::NODE_BEST_LEAF) {
      idMD6BestLeaf* const bestLeaf = static_cast<idMD6BestLeaf*>(node);
      --stackIndex;
      if (bestLeaf->leafList.num != 0) {
        const unsigned short index =
            bestLeaf->bestLeafIndex >= bestLeaf->leafList.num
                ? 0
                : bestLeaf->bestLeafIndex;
        if (bestLeaf->leafList.list[index] != nullptr) {
          stack[++stackIndex] = bestLeaf->leafList.list[
              bestLeaf->bestLeafIndex >= bestLeaf->leafList.num
                  ? 0
                  : bestLeaf->bestLeafIndex];
        }
      }
    } else if (node->type == idMD6Node::NODE_LEAF_PAUSE ||
               node->type == idMD6Node::NODE_LEAF_PLAY) {
      const idMD6Leaf* const leaf = static_cast<idMD6Leaf*>(node);
      const int leafFrames = AnimationFrameCount(leaf->anim);
      if (leafFrames > numFrames &&
          (wrapMode == idMD6Leaf::WRAP_INVALID ||
           leaf->wrapMode == wrapMode) &&
          (!ignoreAdditive || (leaf->anim->animData->flags & 1u) == 0u)) {
        if (node->type == idMD6Node::NODE_LEAF_PLAY) {
          curFrame = static_cast<idMD6LeafPlay*>(node)->GetFrame(
              curTime, ticksPerSec);
        } else {
          curFrame = static_cast<int>(
              static_cast<idMD6LeafPause*>(node)->frame);
        }
        numFrames = AnimationFrameCount(leaf->anim);
        frameRate = leaf->anim->animData != nullptr
                        ? leaf->anim->animData->frameRate
                        : 30;
      }
      --stackIndex;
    }
  } while (stackIndex >= 0);
  return numFrames > 0;
}

idAnimWebBlendTree::idAnimWebBlendTree(
    idAnimWebBlendTreeNodeIndex initialNodeIndex)
    : tree(nullptr),
      nodeIndex(initialNodeIndex),
      modelIndex(0),
      web(nullptr),
      scalarPairs(),
      leaves() {}

void idAnimWebBlendTree::BeginPlaying(idAnimator_AnimWeb* animator,
                                      const idMD6AnimProps* animProps,
                                      int numAnimProps, int curTime,
                                      int ticksPerSec) {
  CopyScalars(animator, curTime, ticksPerSec, 2u);
  StartTreePlaying_r(animator, animator->modelDecls.list[modelIndex],
                     animProps, numAnimProps, curTime, ticksPerSec, tree);
}

void idAnimWebBlendTree::PreBlendTree(const idAnimator_AnimWeb* animator,
                                     int curTime, int ticksPerSec) {
  CopyScalars(animator, curTime, ticksPerSec, 1u);
}

void idAnimWebBlendTree::GetUsedScalarIndices(
    const idAnimator_AnimWeb*, idList<int, 5>& usedScalars,
    int minIndex, int maxIndex) const {
  for (int pairIndex = 0; pairIndex < scalarPairs.num; ++pairIndex) {
    const int scalarIndex = scalarPairs.list[pairIndex].scalarIndex;
    if ((minIndex < 0 || scalarIndex >= minIndex) &&
        (maxIndex <= 0 || scalarIndex <= maxIndex)) {
      bool found = false;
      for (int usedIndex = 0; usedIndex < usedScalars.num; ++usedIndex) {
        if (usedScalars.list[usedIndex] == scalarIndex) {
          found = true;
          break;
        }
      }
      if (!found) {
        usedScalars.Append(scalarIndex);
      }
    }
  }
}

void idAnimWebBlendTree::EnumerateTree_r(
    idMD6Node* initialNode,
    idStaticList<const idMD6Node*, 32>& outputNodes) {
  if (initialNode == nullptr) {
    return;
  }
  const idMD6Node* stack[64];
  stack[0] = initialNode;
  int stackIndex = 0;
  do {
    const idMD6Node* const node = stack[stackIndex];
    bool found = false;
    for (int outputIndex = 0; outputIndex < outputNodes.num;
         ++outputIndex) {
      if (outputNodes.list[outputIndex] == node) {
        found = true;
        break;
      }
    }
    if (!found) {
      outputNodes.Append(node);
    }
    const std::uint8_t type = node->type;
    if (type == idMD6Node::NODE_BRANCH ||
        type == idMD6Node::NODE_BLEND_BRANCH ||
        type == idMD6Node::NODE_BLENDA_BRANCH ||
        type == idMD6Node::NODE_FUSION_BRANCH) {
      const idMD6Branch* const branch = static_cast<const idMD6Branch*>(node);
      --stackIndex;
      if (branch->left != nullptr) {
        stack[++stackIndex] = branch->left;
      }
      if (branch->right != nullptr) {
        stack[++stackIndex] = branch->right;
      }
      if (type == idMD6Node::NODE_BLEND_BRANCH ||
          type == idMD6Node::NODE_BLENDA_BRANCH ||
          type == idMD6Node::NODE_FUSION_BRANCH) {
        const idMD6BlendBranch_Base* const blend =
            static_cast<const idMD6BlendBranch_Base*>(node);
        for (int index = 0; index < blend->animationList.num; ++index) {
          stack[++stackIndex] = blend->animationList.list[index];
        }
      }
    } else {
      --stackIndex;
      if (type == idMD6Node::NODE_BEST_LEAF) {
        const idMD6BestLeaf* const bestLeaf =
            static_cast<const idMD6BestLeaf*>(node);
        for (int index = 0; index < bestLeaf->leafList.num; ++index) {
          stack[++stackIndex] = bestLeaf->leafList.list[index];
        }
      }
    }
  } while (stackIndex >= 0);
}

void idAnimWebBlendTree::Free() {
  idStaticList<const idMD6Node*, 32> nodes;
  const int numLeaves = leaves.num;
  for (int index = 0; index < leaves.num; ++index) {
    nodes.Append(leaves.list[index]);
    idMD6Allocator::FreeTyped(web->md6Allocator, leaves.list[index]);
  }
  leaves.ClearFree();
  if (tree != nullptr) {
    EnumerateTree_r(tree, nodes);
    for (int index = numLeaves; index < nodes.num; ++index) {
      idMD6Allocator::FreeTyped(web->md6Allocator, nodes.list[index]);
      nodes.list[index] = nullptr;
    }
    tree = nullptr;
  }
}

idAnimWebBlendTree::~idAnimWebBlendTree() {
  Free();
  web = nullptr;
  tree = nullptr;
}

bool idAnimWebBlendTree::Init(
    idAnimator_AnimWeb* animator, const idDeclAnimWebNode* declarationNode,
    idAnimWebBlendTreeModelIndex initialModelIndex,
    const char* blendEquation,
    const idDeclAnimWebNode::idModelInfo& modelInfo, int ticksPerSec) {
  Free();
  web = animator;
  idMD6Allocator* const allocator = animator->md6Allocator;
  idStaticList<idAnimWebBlendEquation::existingNode_t, 64> existingNodes;
  const idDeclMD6* const md6Decl =
      animator->modelDecls.list[initialModelIndex.value];

  if (modelInfo.animProps.num > leaves.size) {
    leaves.Resize(modelInfo.animProps.num);
  }
  for (int index = 0; index < modelInfo.animProps.num; ++index) {
    idMD6LeafPlay* const leaf = static_cast<idMD6LeafPlay*>(
        allocator->Alloc(idMD6Node::NODE_LEAF_PLAY));
    if ((declarationNode->customFlags & 0x100u) != 0u) {
      leaf->syncGroup = 1;
      leaf->syncEnabled = 1;
    }

    idAnimWebBlendEquation::existingNode_t* const existing =
        existingNodes.Alloc();
    existing->node = leaf;
    idStr::snPrintf(existing->name, 8, "anim%d", index);

    const idMD6AnimProps& properties = modelInfo.animProps.list[index];
    if (properties.aliasHandle != 0xFFFFu) {
      const idAliasHandle_t aliasHandle(properties.aliasHandle);
      const idMD6Alias* const alias =
          md6Decl->FindAlias(aliasHandle, true, true);
      if (alias != nullptr && (alias->flags & 2u) == 0u) {
        idLib::Warning("%s md6def '%s' has not preloaded alias '%s'",
                       "idAnimWebBlendTree::Init", md6Decl->name.str,
                       alias->name.str);
        const idAliasHandle_t loadHandle(properties.aliasHandle);
        using StrongLoadAliasFn = const idMD6Alias*(__fastcall*)(
            idAnimation*, const idDeclMD6*, const idAliasHandle_t&);
        StrongLoadAliasFn* const animationVtable =
            *reinterpret_cast<StrongLoadAliasFn**>(animation);
        animationVtable[35](animation, md6Decl, loadHandle);
      }
    }

    const idMD6Anim* animationValue = properties.SelectAnim(md6Decl, 0);
    if (animationValue == nullptr) {
      animationValue = md6Decl->model->skeleton != nullptr
                           ? md6Decl->model->skeleton->defaultAnim
                           : nullptr;
      if (properties.aliasHandle == 0xFFFFu) {
        idStr nodeName;
        declarationNode->GetName(animator->decl, nodeName);
        idLib::Warning(
            "idAnimWebBlendTree::Init - invalid or missing animation "
            "specified in blend equation '%s' in animWeb '%s', node '%s'",
            blendEquation, animator->decl->name.str, nodeName.c_str());
      }
      if (animationValue == nullptr) {
        idStr nodeName;
        declarationNode->GetName(animator->decl, nodeName);
        idLib::Warning(
            "idAnimWebBlendTree::Init - failed to load default anim in "
            "blend equation '%s' for md6Def '%s' in animWeb '%s', node "
            "'%s'",
            blendEquation, md6Decl->name.str, animator->decl->name.str,
            nodeName.c_str());
        return false;
      }
    }

    leaf->Init(animationValue, 0, 0, 0, properties.rate,
               static_cast<idMD6Leaf::wrapMode_t>(properties.wrap),
               MD6_WEIGHTGROUP_ALL);
    leaves.Append(leaf);
  }

  modelIndex = initialModelIndex.value;
  const idMD6Model* const model =
      animator->decl->models.list[initialModelIndex.value]->model;
  tree = nullptr;
  idAnimWebBlendEquation equation;
  if (!equation.Parse(modelInfo, model, allocator, 0, ticksPerSec,
                      blendEquation, animator->scalars, existingNodes,
                      tree, scalarPairs)) {
    Free();
    idStr nodeName;
    declarationNode->GetName(animator->decl, nodeName);
    idLib::Warning(
        "idAnimWebBlendTree::Init - failed parsing blend equation '%s' in "
        "animWeb '%s', node '%s'",
        blendEquation, animator->decl->name.str, nodeName.c_str());
    return false;
  }
  nodeIndex.value = declarationNode->nodeIndex.value;
  return true;
}
