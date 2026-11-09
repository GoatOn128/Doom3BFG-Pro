#define WOLFTNO_RETAIL_FIND_ALIAS_REFERENCE_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#include "engine/gamelib/animstack/animator_endadditivechannels.h"
#undef WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_FIND_ALIAS_REFERENCE_ABI

#include "engine/framework/gametimemanager.h"
#include "engine/framework/timemanager.h"
#include "engine/gamelib/animstack/animstack.h"
#include "engine/models/skeletalanimation/jointanimator.h"
#include "engine/models/skeletalanimation/jointconversion.h"

#include <cstdint>

template <>
bool idList<idUserChannelHandle_t, 5>::Resize(int newSize);
template <>
bool idList<idMD6Anim*, 5>::Resize(int newSize);
template <>
bool idList<float, 13>::Resize(int newSize);

idAnimator_EndAdditiveChannels::idAnimator_EndAdditiveChannels()
    : leaves(), branches(), aliasWeights(), mergeBranch(), animAlias(),
      numAddedChannels(0), numCurrentChannels(0),
      wrapMode(idMD6Leaf::WRAP_LOOP),
      firstBlendOp(idMD6Blend::BOP_ADD_RIGHT), currentExternalAlpha(1.0f),
      targetExternalAlpha(1.0f), externalAlphaRate(0.0f) {}

idAnimator_EndAdditiveChannels::~idAnimator_EndAdditiveChannels() = default;

bool idAnimator_EndAdditiveChannels::InternalIsContributing() const {
  return numAddedChannels > 0;
}

void idAnimator_EndAdditiveChannels::SetAnimHandlesAndWeights(
    int, const idAliasHandle_t* handles, int numAnims, const float* weights,
    int) {
  numCurrentChannels = numAnims;
  volatile idAliasHandle_t* const destinationHandles = animAlias.list;
  const volatile idAliasHandle_t* const sourceHandles = handles;
  volatile float* const destinationWeights = aliasWeights.list;
  const volatile float* const sourceWeights = weights;

  std::int64_t index = 0;
  if (numAnims >= 4) {
    std::uint64_t groups =
        (static_cast<std::uint64_t>(numAnims - 4) >> 2) + 1;
    do {
      destinationHandles[index].value = sourceHandles[index].value;
      destinationWeights[index] = sourceWeights[index];
      destinationHandles[index + 1].value = sourceHandles[index + 1].value;
      destinationWeights[index + 1] = sourceWeights[index + 1];
      destinationHandles[index + 2].value = sourceHandles[index + 2].value;
      destinationWeights[index + 2] = sourceWeights[index + 2];
      destinationHandles[index + 3].value = sourceHandles[index + 3].value;
      destinationWeights[index + 3] = sourceWeights[index + 3];
      index += 4;
    } while (--groups != 0);
  }
  while (index < numAnims) {
    destinationHandles[index].value = sourceHandles[index].value;
    destinationWeights[index] = sourceWeights[index];
    ++index;
  }
}

const idMD6Branch*
idAnimator_EndAdditiveChannels::InternalGetMergeBranch() const {
  return numAddedChannels == 1 ? branches.list[0] : &mergeBranch;
}

idMD6Branch* idAnimator_EndAdditiveChannels::InternalGetMergeBranch() {
  return numAddedChannels == 1 ? branches.list[0] : &mergeBranch;
}

const idMD6Branch*
idAnimator_EndAdditiveChannels::InternalGetEndBranch() const {
  return numAddedChannels <= 1 ? nullptr
                               : branches.list[numAddedChannels - 1];
}

idMD6Branch* idAnimator_EndAdditiveChannels::InternalGetEndBranch() {
  return numAddedChannels <= 1 ? nullptr
                               : branches.list[numAddedChannels - 1];
}

void idAnimator_EndAdditiveChannels::SetExternalBlend(float blendTarget,
                                                       int blendMS) {
  targetExternalAlpha = blendTarget;
  externalAlphaRate = static_cast<float>(timeManager.gameTimePerSecond.value) /
                      static_cast<float>(blendMS);
}

void idAnimator_EndAdditiveChannels::InternalShutdown(idAnimStack* stack) {
  for (int index = 0; index < leaves.num; ++index) {
    idMD6Allocator::FreeTyped(stack->GetAllocator(), leaves.list[index]);
  }
  for (int index = 0; index < branches.num; ++index) {
    idMD6Allocator::FreeTyped(stack->GetAllocator(), branches.list[index]);
  }
  leaves.ClearFree();
  branches.ClearFree();
}

void idAnimator_EndAdditiveChannels::InternalPreBlendTree(
    const idAnimStack* stack, int curTime, int) {
  mergeBranch.left = nullptr;
  mergeBranch.right = nullptr;

  const int previousTime =
      gametimeManager.gameTimeManager->GetPreviousGameMs(GAMETIME_SCALED);
  const int elapsed = static_cast<int>(static_cast<std::uint32_t>(curTime) -
                                       static_cast<std::uint32_t>(previousTime));
  if (elapsed > 0) {
    float nextAlpha = targetExternalAlpha;
    const float difference = targetExternalAlpha - currentExternalAlpha;
    const float seconds =
        static_cast<float>(elapsed) /
        static_cast<float>(timeManager.gameTimePerSecond.value);
    if (difference > 0.0f) {
      const float candidate =
          currentExternalAlpha + seconds * externalAlphaRate;
      if (!(candidate > targetExternalAlpha - 0.000001f)) {
        nextAlpha = candidate;
      }
      currentExternalAlpha = nextAlpha;
    } else if (difference < 0.0f) {
      const float candidate =
          currentExternalAlpha - seconds * externalAlphaRate;
      if (!(candidate < targetExternalAlpha + 0.000001f)) {
        nextAlpha = candidate;
      }
      currentExternalAlpha = nextAlpha;
    }
  }

  idMD6Branch* previousBranch = nullptr;
  numAddedChannels = 0;
  for (int index = 0; index < numCurrentChannels; ++index) {
    float alpha = aliasWeights.list[index] * currentExternalAlpha;
    if (!(alpha <= 1.1920929e-7f)) {
      idMD6Blend::blendOp_t operation = idMD6Blend::BOP_ADD_RIGHT;
      idMD6LeafPlay* const leaf = leaves.list[index];
      if (alpha < 0.0f) {
        alpha = -alpha;
        operation = idMD6Blend::BOP_SUB_RIGHT;
      }

      const idMD6Alias* const alias = stack->jointAnimator->decl->FindAlias(
          animAlias.list[index], true, true);
      if (alias == nullptr || alias->mappings.list[0].anim == nullptr) {
        continue;
      }

      const idMD6Anim* const anim = alias->mappings.list[0].anim;
      if (leaf->anim != anim) {
        leaf->Init(anim, curTime, timeManager.gameTimePerSecond.value, 0, 1.0f,
                   wrapMode, weightGroup);
      }

      idMD6Branch* const branch = branches.list[numAddedChannels++];
      if (previousBranch != nullptr) {
        previousBranch->left = branch;
      }
      branch->Init(nullptr, leaf, operation,
                   idMD6Blend::ORIGINBLEND_BRANCH, alpha, alpha, 0.0f,
                   filterGroup, idMD6Branch::BLEND_LINEAR);
      previousBranch = branch;
    }
  }

  if (numAddedChannels <= 1) {
    if (numAddedChannels > 0 && firstBlendOp != idMD6Blend::BOP_ADD_RIGHT) {
      branches.list[0]->op = static_cast<std::uint8_t>(firstBlendOp);
    }
  } else {
    mergeBranch.targetAlpha = 1.0f;
    mergeBranch.currentAlpha = 1.0f;
    mergeBranch.right = branches.list[0];
    if (firstBlendOp != idMD6Blend::BOP_ADD_RIGHT) {
      branches.list[numAddedChannels - 1]->op =
          static_cast<std::uint8_t>(firstBlendOp);
    }
  }
}

bool idAnimator_EndAdditiveChannels::InternalInit(
    const idAnimatorParms_Base& parms) {
  const auto& additiveParms =
      static_cast<const idAnimatorParms_EndAdditiveChannels&>(parms);
  const int maxChannels = additiveParms.maxChannels;

  auto& aliasResize =
      *reinterpret_cast<idList<idUserChannelHandle_t, 5>*>(&animAlias);
  if (maxChannels <= animAlias.size || aliasResize.Resize(maxChannels)) {
    animAlias.num =
        maxChannels < animAlias.size ? maxChannels : animAlias.size;
  }

  auto& leavesResize = *reinterpret_cast<idList<idMD6Anim*, 5>*>(&leaves);
  if (maxChannels <= leaves.size || leavesResize.Resize(maxChannels)) {
    leaves.num = maxChannels < leaves.size ? maxChannels : leaves.size;
  }

  auto& branchesResize =
      *reinterpret_cast<idList<idMD6Anim*, 5>*>(&branches);
  if (maxChannels <= branches.size || branchesResize.Resize(maxChannels)) {
    branches.num = maxChannels < branches.size ? maxChannels : branches.size;
  }

  auto& weightsResize =
      *reinterpret_cast<idList<float, 13>*>(&aliasWeights);
  if (maxChannels <= aliasWeights.size ||
      weightsResize.Resize(maxChannels)) {
    aliasWeights.num = maxChannels < aliasWeights.size ? maxChannels
                                                       : aliasWeights.size;
  }

  firstBlendOp = additiveParms.firstBlendOp;
  for (int index = 0; index < maxChannels; ++index) {
    leaves.list[index] = static_cast<idMD6LeafPlay*>(
        parms.animStack->GetAllocator()->Alloc(idMD6Node::NODE_LEAF_PLAY));
    branches.list[index] = static_cast<idMD6Branch*>(
        parms.animStack->GetAllocator()->Alloc(idMD6Node::NODE_BRANCH));
  }
  return true;
}
