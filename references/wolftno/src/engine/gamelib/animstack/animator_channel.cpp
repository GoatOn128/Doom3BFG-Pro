#define WOLFTNO_RETAIL_CHANNELWEIGHT_TICKS_ABI
#define WOLFTNO_RETAIL_CHANNEL_BLENDPARMS_ABI
#define WOLFTNO_RETAIL_FIND_ALIAS_REFERENCE_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#define WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI

#include "engine/gamelib/animstack/animator_channel.h"

#include "engine/framework/timemanager.h"
#include "engine/gamelib/animstack/animstack.h"
#include "engine/models/skeletalanimation/jointanimator.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/str.h"

#include <cmath>

namespace {

bool IsBranchNode(const idMD6Node* node) {
  const std::uint8_t type = node->type;
  return type == idMD6Node::NODE_BRANCH ||
         type == idMD6Node::NODE_BLEND_BRANCH ||
         type == idMD6Node::NODE_BLENDA_BRANCH ||
         type == idMD6Node::NODE_FUSION_BRANCH;
}

} // namespace

idAnimator_Channel::~idAnimator_Channel() noexcept(false) {
  mergeBranch = nullptr;
}

idAnimator_Channel::idAnimator_Channel()
    : mergeBranch(nullptr), random(0) {
  forceFree = false;
  omitFreeLeafWarning = false;
  numFreeLeaves = 0;
  numFreeBranches = 0;
  blendOutDurationMS = 0;
  leaves[0] = nullptr;
  leaves[1] = nullptr;
  leaves[2] = nullptr;
  branches[0] = nullptr;
  branches[1] = nullptr;
  freeLeaves[0] = nullptr;
  freeLeaves[1] = nullptr;
  freeLeaves[2] = nullptr;
  freeBranches[0] = nullptr;
  freeBranches[1] = nullptr;
  random.SetSeed(0);
}

bool idAnimator_Channel::InternalInit(const idAnimatorParms_Base& parms) {
  idMD6Allocator* const allocator = parms.animStack->GetAllocator();
  mergeBranch =
      static_cast<idMD6Branch*>(allocator->Alloc(idMD6Node::NODE_BRANCH));

  for (int index = 0; index < 3; ++index) {
    idMD6LeafPlay* const leaf = static_cast<idMD6LeafPlay*>(
        allocator->Alloc(idMD6Node::NODE_LEAF_PLAY));
    leaves[index] = leaf;
    freeLeaves[numFreeLeaves++] = leaf;
  }

  for (int index = 0; index < 2; ++index) {
    idMD6Branch* const branch =
        static_cast<idMD6Branch*>(allocator->Alloc(idMD6Node::NODE_BRANCH));
    branches[index] = branch;
    freeBranches[numFreeBranches++] = branch;
  }

  mergeBranch->left = nullptr;
  mergeBranch->right = nullptr;
  return true;
}

void idAnimator_Channel::InternalShutdown(idAnimStack* animStack) {
  idMD6Allocator* const allocator = animStack->GetAllocator();
  if (mergeBranch != nullptr) {
    idMD6Allocator::FreeTyped(allocator, mergeBranch);
    mergeBranch = nullptr;
  }

  for (idMD6LeafPlay*& leaf : leaves) {
    if (leaf != nullptr) {
      idMD6Allocator::FreeTyped(allocator, leaf);
      leaf = nullptr;
    }
  }
  for (idMD6Branch*& branch : branches) {
    if (branch != nullptr) {
      idMD6Allocator::FreeTyped(allocator, branch);
      branch = nullptr;
    }
  }
  numFreeLeaves = 0;
  numFreeBranches = 0;
}

void idAnimator_Channel::ClearAnimator() {
  InternalEnd(nullptr, 0, idTypesafeNumber<int, gameTimeUnique_t>(0));
}

bool idAnimator_Channel::IsDone(int curTime) const {
  if (mergeBranch == nullptr || mergeBranch->right == nullptr ||
      !mergeBranch->right->IsValid()) {
    return true;
  }
  const idMD6Node* const node = mergeBranch->right;
  return node->type == idMD6Node::NODE_LEAF_PLAY &&
         !static_cast<const idMD6LeafPlay*>(node)->IsPlaying(
             curTime, timeManager.gameTimePerSecond.value);
}

bool idAnimator_Channel::IsDone(int curTime,
                                bool clampIgnoreExtraFrame) const {
  if (mergeBranch == nullptr || mergeBranch->right == nullptr ||
      !mergeBranch->right->IsValid()) {
    return true;
  }
  const idMD6Node* const node = mergeBranch->right;
  return node->type == idMD6Node::NODE_LEAF_PLAY &&
         !static_cast<const idMD6LeafPlay*>(node)->IsPlaying(
             curTime, timeManager.gameTimePerSecond.value,
             clampIgnoreExtraFrame);
}

void idAnimator_Channel::ForceFreeLeaf() {
  if (numFreeLeaves > 0) {
    idLib::Warning("%s: No need to free a leaf.",
                   "idAnimator_Channel::ForceFreeLeaf");
    return;
  }

  idMD6Branch* branch = mergeBranch;
  idMD6Branch* priorBranch = nullptr;
  while (branch != nullptr) {
    idMD6Node* const right = branch->right;
    if (right == nullptr) {
      return;
    }
    if (right->type == idMD6Node::NODE_LEAF_PAUSE ||
        right->type == idMD6Node::NODE_LEAF_PLAY) {
      if (priorBranch != nullptr) {
        priorBranch->right = branch->left;
      }
      freeLeaves[numFreeLeaves] = static_cast<idMD6LeafPlay*>(right);
      ++numFreeLeaves;
      freeBranches[numFreeBranches] = branch;
      ++numFreeBranches;
      return;
    }
    if (IsBranchNode(right)) {
      priorBranch = branch;
      branch = static_cast<idMD6Branch*>(right);
    }
  }
}

void idAnimator_Channel::FreeTree(idMD6Node* node) {
  if (node == nullptr) {
    return;
  }
  if (IsBranchNode(node)) {
    idMD6Branch* const branch = static_cast<idMD6Branch*>(node);
    FreeTree(branch->left);
    FreeTree(branch->right);
    freeBranches[numFreeBranches++] = branch;
  } else {
    freeLeaves[numFreeLeaves++] = static_cast<idMD6LeafPlay*>(node);
  }
}

idMD6LeafPlay* idAnimator_Channel::GetLastPlayedLeaf() {
  if (mergeBranch == nullptr || mergeBranch->right == nullptr) {
    return nullptr;
  }
  idMD6Node* node = mergeBranch->right;
  if (node->type == idMD6Node::NODE_LEAF_PLAY) {
    return static_cast<idMD6LeafPlay*>(node);
  }
  if (!IsBranchNode(node)) {
    return nullptr;
  }
  node = static_cast<idMD6Branch*>(node)->left;
  return node != nullptr && node->type == idMD6Node::NODE_LEAF_PLAY
             ? static_cast<idMD6LeafPlay*>(node)
             : nullptr;
}

void idAnimator_Channel::InternalPreBlendTree(const idAnimStack*, int curTime,
                                              int) {
  mergeBranch->left = nullptr;
  idMD6Branch* parent = mergeBranch;
  if (parent == nullptr) {
    return;
  }

  idMD6Node* node = parent->right;
  while (node != nullptr) {
    if (IsBranchNode(node)) {
      idMD6Branch* const branch = static_cast<idMD6Branch*>(node);
      if (branch->IsBlendFinished() || !IsContributing()) {
        parent->right = branch->left;
        FreeTree(branch->right);
        freeBranches[numFreeBranches++] = branch;
        break;
      }
      parent = branch;
      node = branch->right;
      continue;
    }
    if (node->type == idMD6Node::NODE_LEAF_PLAY) {
      break;
    }
  }

  node = mergeBranch->right;
  if (node == nullptr) {
    return;
  }
  idMD6LeafPlay* animation = IsBranchNode(node)
                                 ? static_cast<idMD6LeafPlay*>(
                                       static_cast<idMD6Branch*>(node)->left)
                                 : static_cast<idMD6LeafPlay*>(node);
  if (animation->wrapMode == idMD6Leaf::WRAP_CLAMP) {
    const idTypesafeNumber<int, gameTimeUnique_t> animationLength =
        animation->GetAnimTicks(timeManager.gameTimePerSecond.value);
    const int remaining = animationLength.value + animation->startTime - curTime;
    if (remaining > 0) {
      if (remaining <= blendOutDurationMS) {
        mergeBranch->targetAlpha = 0.0f;
        const float difference =
            mergeBranch->targetAlpha - mergeBranch->currentAlpha;
        mergeBranch->alphaRate =
            static_cast<float>(std::fabs(static_cast<double>(difference))) *
            static_cast<float>(timeManager.gameTimePerSecond.value) /
            static_cast<float>(remaining);
      }
    } else {
      mergeBranch->alphaRate = 0.0f;
      mergeBranch->currentAlpha = mergeBranch->targetAlpha;
    }
  }

  idMD6Node* const rootRight = mergeBranch->right;
  if (rootRight->type == idMD6Node::NODE_LEAF_PLAY &&
      ((mergeBranch->targetAlpha == 0.0f &&
        mergeBranch->currentAlpha == 0.0f) ||
       !animation->IsPlaying(curTime,
                             timeManager.gameTimePerSecond.value))) {
    mergeBranch->right = nullptr;
    freeLeaves[numFreeLeaves++] = static_cast<idMD6LeafPlay*>(rootRight);
  }
}

void idAnimator_Channel::InternalEnd(
    const idAnimStack*, int,
    idTypesafeNumber<int, gameTimeUnique_t> blendDurationTicks) {
  if (mergeBranch != nullptr && blendDurationTicks.value <= 0) {
    if (mergeBranch->right != nullptr) {
      FreeTree(mergeBranch->right);
    }
    mergeBranch->left = nullptr;
    mergeBranch->right = nullptr;
  }
}

bool idAnimator_Channel::IsAnimPlaying(const idAnimStack* stack,
                                       const idAliasHandle_t& aliasHandle,
                                       int curTime) const {
  const idMD6Alias* const alias =
      stack->jointAnimator->decl->FindAlias(aliasHandle, true, true);
  if (alias == nullptr || mergeBranch == nullptr ||
      mergeBranch->right == nullptr) {
    return false;
  }

  idMD6Node* pending[32];
  int top = 0;
  pending[0] = mergeBranch->right;
  while (top >= 0) {
    idMD6Node* const current = pending[top--];
    if (IsBranchNode(current)) {
      const idMD6Branch* const branch =
          static_cast<const idMD6Branch*>(current);
      if (branch->left != nullptr &&
          !branch->IsChildBlendedOut(branch->left)) {
        pending[++top] = branch->left;
      }
      if (branch->right != nullptr &&
          !branch->IsChildBlendedOut(branch->right)) {
        pending[++top] = branch->right;
      }
      continue;
    }
    if (current->type != idMD6Node::NODE_LEAF_PLAY) {
      continue;
    }

    const idMD6LeafPlay* const leaf =
        static_cast<const idMD6LeafPlay*>(current);
    if (!leaf->IsPlaying(curTime, timeManager.gameTimePerSecond.value)) {
      continue;
    }
    for (int index = 0; index < alias->mappings.num; ++index) {
      const idMD6Anim* const mappedAnimation =
          alias->mappings.list[index].anim;
      if (mappedAnimation != nullptr && leaf->anim == mappedAnimation) {
        return true;
      }
    }
  }
  return false;
}

bool idAnimator_Channel::StartNode(idJointAnimator* animator, idMD6Node* node,
                                   int, idMD6Blend::blendOp_t blendOp,
                                   idMD6Leaf::wrapMode_t wrap,
                                   const blendParms_t& blendParms,
                                   int blendOutDurationMS_,
                                   float targetAlpha) {
  static_cast<void>(animator);
  if (mergeBranch == nullptr) {
    return false;
  }

  const std::int16_t durationMS = blendParms.GetDurationMS();
  idMD6Node* const oldRight = mergeBranch->right;
  const bool hadAnimation = oldRight != nullptr;
  if (oldRight != nullptr) {
    if (durationMS != 0) {
      const int branchNumber = 2 - numFreeBranches;
      if (numFreeBranches == 0) {
        return false;
      }
      --numFreeBranches;
      idMD6Branch* const branch = freeBranches[numFreeBranches];
      if (branch == nullptr) {
        return false;
      }

      va branchName("%s%d", "<unknown>", branchNumber);
      branch->filterGroup = static_cast<std::uint8_t>(GetFilterGroup());
      branch->op = static_cast<std::uint8_t>(blendOp);
      branch->originBlend = blendParms.parms.originBlend;
      branch->left = node;
      branch->currentAlpha = 1.0f;
      branch->targetAlpha = 0.0f;
      branch->right = oldRight;
      branch->alphaRate =
          static_cast<float>(timeManager.gameTimePerSecond.value) /
          static_cast<float>(durationMS);
      mergeBranch->right = branch;
    } else {
      FreeTree(oldRight);
      mergeBranch->right = node;
      mergeBranch->currentAlpha = 1.0f;
      mergeBranch->targetAlpha = 1.0f;
      mergeBranch->alphaRate = 0.0f;
    }
  } else {
    mergeBranch->right = node;
  }

  mergeBranch->targetAlpha = targetAlpha;
  if (!hadAnimation) {
    mergeBranch->currentAlpha = 0.0f;
  }
  if (durationMS <= 0) {
    mergeBranch->alphaRate = 0.0f;
    mergeBranch->currentAlpha = targetAlpha;
  } else {
    const float difference =
        mergeBranch->targetAlpha - mergeBranch->currentAlpha;
    mergeBranch->alphaRate =
        static_cast<float>(std::fabs(static_cast<double>(difference))) *
        static_cast<float>(timeManager.gameTimePerSecond.value) /
        static_cast<float>(durationMS);
  }

  if (wrap != idMD6Leaf::WRAP_CLAMP) {
    blendOutDurationMS = 0;
  } else {
    blendOutDurationMS = static_cast<std::int16_t>(
        blendOutDurationMS_ < 0 ? durationMS : blendOutDurationMS_);
  }
  return true;
}

bool idAnimator_Channel::StartLeaf(
    idJointAnimator* animator, const idMD6Anim* animation, int curTime,
    float rateScale, idMD6Blend::blendOp_t blendOp,
    idMD6Leaf::wrapMode_t wrap, const blendParms_t& blendParms,
    int blendOutDurationMS_, idMD6LeafPlay** leafStarted, float targetAlpha) {
  if (animation == nullptr) {
    return false;
  }
  if (numFreeLeaves == 0 && forceFree) {
    ForceFreeLeaf();
  }
  if (numFreeLeaves == 0) {
    return false;
  }

  --numFreeLeaves;
  idMD6LeafPlay* const leaf = freeLeaves[numFreeLeaves];
  if (leaf == nullptr) {
    return false;
  }

  std::int16_t destinationStartFrame;
  std::int16_t destinationDuration;
  blendParms.GetDestFrames(animation, destinationStartFrame,
                           destinationDuration);
  leaf->Init(animation, curTime, timeManager.gameTimePerSecond.value,
             destinationStartFrame, rateScale, wrap, weightGroup);
  if (!StartNode(animator, leaf, curTime, blendOp, wrap, blendParms,
                 blendOutDurationMS_, targetAlpha)) {
    freeLeaves[numFreeLeaves++] = leaf;
    return false;
  }
  if (leafStarted != nullptr) {
    *leafStarted = leaf;
  }
  return true;
}

bool idAnimator_Channel::PlayAnim(
    const idAnimStack* stack, const idMD6Anim* animation, int curTime,
    float rateScale, const blendParms_t& blendParms,
    int blendOutDurationMS_, idMD6LeafPlay** leafStarted, float targetAlpha) {
  return StartLeaf(stack->jointAnimator, animation, curTime, rateScale,
                   idMD6Blend::BOP_LERP, idMD6Leaf::WRAP_CLAMP, blendParms,
                   blendOutDurationMS_, leafStarted, targetAlpha);
}

bool idAnimator_Channel::CycleAnim(
    const idAnimStack* stack, const idMD6Anim* animation, int curTime,
    float rateScale, const blendParms_t& blendParms,
    idMD6LeafPlay** leafStarted, float targetAlpha) {
  idJointAnimator* const animator = stack->jointAnimator;
  const std::int16_t durationMS = blendParms.GetDurationMS();
  return StartLeaf(animator, animation, curTime, rateScale,
                   idMD6Blend::BOP_LERP, idMD6Leaf::WRAP_REPEAT, blendParms,
                   durationMS, leafStarted, targetAlpha);
}

bool idAnimator_Channel::StartLeaf(
    idJointAnimator* animator, const idAliasHandle_t& aliasHandle, int curTime,
    float rateScale, idMD6Blend::blendOp_t blendOp,
    idMD6Leaf::wrapMode_t wrap, const blendParms_t& blendParms,
    int blendOutDurationMS_, idMD6LeafPlay** leafStarted, float targetAlpha) {
  if (animator == nullptr) {
    return false;
  }
  const idMD6Alias* const alias =
      animator->decl->FindAlias(aliasHandle, true, true);
  if (alias == nullptr) {
    return false;
  }
  const int index = alias->mappings.num != 0
                        ? random.RandomInt(alias->mappings.num)
                        : 0;
  return StartLeaf(animator, alias->mappings.list[index].anim, curTime,
                   rateScale, blendOp, wrap, blendParms, blendOutDurationMS_,
                   leafStarted, targetAlpha);
}

bool idAnimator_Channel::PlayAnim(
    const idAnimStack* stack, const idAliasHandle_t& aliasHandle, int curTime,
    float rateScale, const blendParms_t& blendParms,
    int blendOutDurationMS_, idMD6LeafPlay** leafStarted, float targetAlpha) {
  return StartLeaf(stack->jointAnimator, aliasHandle, curTime, rateScale,
                   idMD6Blend::BOP_LERP, idMD6Leaf::WRAP_CLAMP, blendParms,
                   blendOutDurationMS_, leafStarted, targetAlpha);
}

bool idAnimator_Channel::CycleAnim(
    const idAnimStack* stack, const idAliasHandle_t& aliasHandle, int curTime,
    float rateScale, const blendParms_t& blendParms,
    idMD6LeafPlay** leafStarted, float targetAlpha) {
  idJointAnimator* const animator = stack->jointAnimator;
  const std::int16_t durationMS = blendParms.GetDurationMS();
  return StartLeaf(animator, aliasHandle, curTime, rateScale,
                   idMD6Blend::BOP_LERP, idMD6Leaf::WRAP_REPEAT, blendParms,
                   durationMS, leafStarted, targetAlpha);
}
