#define WOLFTNO_RETAIL_FIND_ALIAS_REFERENCE_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#define WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#define WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#include "engine/gamelib/animstack/animator_synced.h"
#undef WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#undef WOLFTNO_RETAIL_STATICLIST_THROWING_CTOR_ABI
#undef WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_FIND_ALIAS_REFERENCE_ABI

#include "engine/framework/timemanager.h"
#include "engine/models/skeletalanimation/jointconversion.h"
#include "shared/idlib/lib.h"

#include <cstring>

template <>
bool idList<idUserChannelHandle_t, 5>::Resize(int newSize);
template <>
bool idList<idMD6Anim*, 5>::Resize(int newSize);
template <>
bool idList<float, 13>::Resize(int newSize);

idAnimator_Synced::idAnimator_Synced()
    : leaves(), branches(), mergeBranch(), anims(), aliasHandles(),
      aliasWeights(), alphaValues(), wrapMode(idMD6Leaf::WRAP_LOOP),
      rateScale(1.0f) {
  std::memset(anims.list, 0,
              sizeof(*anims.list) * static_cast<std::size_t>(anims.size));
  std::memset(aliasWeights.list, 0,
              sizeof(*aliasWeights.list) *
                  static_cast<std::size_t>(aliasWeights.size));
  std::memset(alphaValues.list, 0,
              sizeof(*alphaValues.list) *
                  static_cast<std::size_t>(alphaValues.size));
}

idAnimator_Synced::~idAnimator_Synced() noexcept(false) = default;

const idMD6Branch* idAnimator_Synced::InternalGetMergeBranch() const {
  return &mergeBranch;
}

idMD6Branch* idAnimator_Synced::InternalGetMergeBranch() {
  return &mergeBranch;
}

bool idAnimator_Synced::InternalIsContributing() const {
  return anims.num > 0;
}

void idAnimator_Synced::InternalPreBlendTree(const idAnimStack*, int curTime,
                                              int) {
  const float targetAlpha = mergeBranch.targetAlpha;
  mergeBranch.left = nullptr;
  mergeBranch.right = nullptr;

  if (targetAlpha == 0.0f && mergeBranch.IsBlendFinished()) {
    aliasHandles.ClearFree();
    anims.ClearFree();
    aliasWeights.ClearFree();
    alphaValues.ClearFree();
    return;
  }

  idMD6Node* tree = nullptr;
  if (anims.num > 0) {
    int animationIndex = 0;
    int branchIndex = -1;
    int alphaIndex = 0;
    idMD6LeafPlay* leaf = leaves;
    do {
      const idMD6Anim* const animation = anims.list[animationIndex];
      if (leaf->anim != animation) {
        leaf->Init(animation, curTime, timeManager.gameTimePerSecond.value, 0,
                   rateScale, wrapMode, weightGroup);
      }

      if (tree != nullptr) {
        if (branchIndex >= 0) {
          float* const alphaList = alphaValues.list;
          const md6WeightGroup_t branchFilterGroup = GetFilterGroup();
          const float alpha = alphaList[alphaIndex];
          branches[branchIndex].Init(
              tree, leaf, idMD6Blend::BOP_LERP,
              idMD6Blend::ORIGINBLEND_BRANCH, alpha, alpha, 0.0f,
              branchFilterGroup, idMD6Branch::BLEND_LINEAR);
          ++alphaIndex;
          tree = &branches[branchIndex];
        }
      } else {
        tree = leaf;
      }

      ++branchIndex;
      ++leaf;
      ++animationIndex;
    } while (branchIndex + 1 < anims.num);
  }

  if (anims.num > 0) {
    mergeBranch.right = tree;
  }
}

void idAnimator_Synced::SetAnimHandle(
    const idDeclMD6* declaration, int curTime, int index,
    const idAliasHandle_t& aliasHandle) {
  aliasHandles.list[index] = aliasHandle;
  const idMD6Alias* const alias =
      declaration->FindAlias(aliasHandle, true, true);
  if (alias == nullptr) {
    idLib::Warning("bad animation alias, %u", aliasHandle.value);
    return;
  }

  anims.list[index] = alias->mappings.list[0].anim;
  if (anims.list[index] != nullptr &&
      anims.list[index] != leaves[index].anim) {
    leaves[index].Init(anims.list[index], curTime,
                       timeManager.gameTimePerSecond.value, 0, rateScale,
                       wrapMode, weightGroup);
  }
}

bool idAnimator_Synced::IsDone(int curTime) {
  int index = 0;
  if (aliasHandles.num <= 0) {
    return true;
  }
  while (!leaves[index].IsPlaying(curTime,
                                  timeManager.gameTimePerSecond.value)) {
    ++index;
    if (index >= aliasHandles.num) {
      return true;
    }
  }
  return false;
}

void idAnimator_Synced::CalcAlphaValuesFromWeights() {
  const int alphaCount = aliasWeights.num - 1;
  auto& alphaResize =
      *reinterpret_cast<idList<float, 13>*>(&alphaValues);
  if (alphaCount <= alphaValues.size || alphaResize.Resize(alphaCount)) {
    alphaValues.num =
        alphaCount < alphaValues.size ? alphaCount : alphaValues.size;
  }

  float accumulatedWeight = 0.0f;
  for (int index = 0; index < alphaValues.num; ++index) {
    const float* const weights = aliasWeights.list;
    accumulatedWeight += weights[index];
    const float denominator = weights[index + 1] + accumulatedWeight;
    if (denominator == 0.0f) {
      alphaValues.list[index] = 0.0f;
    } else {
      float alpha = weights[index + 1] / denominator;
      if (alpha < 0.0f) {
        alpha = 0.0f;
      } else if (alpha > 1.0f) {
        alpha = 1.0f;
      }
      alphaValues.list[index] = alpha;
    }
  }
}

void idAnimator_Synced::InternalEnd(
    const idAnimStack*, int,
    idTypesafeNumber<int, gameTimeUnique_t> blendDurationTicks) {
  if (blendDurationTicks.value <= 0) {
    mergeBranch.left = nullptr;
    mergeBranch.right = nullptr;

    if (aliasHandles.size < 0) {
      aliasHandles.ClearFree();
    }
    int nextNum = aliasHandles.size;
    if (nextNum > 0) {
      nextNum = 0;
    }
    aliasHandles.num = nextNum;

    if (anims.size < 0) {
      anims.ClearFree();
    }
    nextNum = anims.size;
    if (nextNum > 0) {
      nextNum = 0;
    }
    anims.num = nextNum;

    if (aliasWeights.size < 0) {
      aliasWeights.ClearFree();
    }
    nextNum = aliasWeights.size;
    if (nextNum > 0) {
      nextNum = 0;
    }
    aliasWeights.num = nextNum;

    if (alphaValues.size < 0) {
      alphaValues.ClearFree();
    }
    nextNum = alphaValues.size;
    if (nextNum > 0) {
      nextNum = 0;
    }
    alphaValues.num = nextNum;
  }
}

void idAnimator_Synced::SetAnimHandlesAndWeights(
    const idDeclMD6* declaration, int curTime,
    const idAliasHandle_t* handles, int numAnims, const float* weights,
    int numWeights) {
  auto& handleResize =
      *reinterpret_cast<idList<idUserChannelHandle_t, 5>*>(&aliasHandles);
  if (numAnims <= aliasHandles.size || handleResize.Resize(numAnims)) {
    aliasHandles.num =
        numAnims < aliasHandles.size ? numAnims : aliasHandles.size;
  }

  auto& animResize = *reinterpret_cast<idList<idMD6Anim*, 5>*>(&anims);
  if (numAnims <= anims.size || animResize.Resize(numAnims)) {
    anims.num = numAnims < anims.size ? numAnims : anims.size;
  }

  auto& weightResize =
      *reinterpret_cast<idList<float, 13>*>(&aliasWeights);
  if (numWeights <= aliasWeights.size || weightResize.Resize(numWeights)) {
    aliasWeights.num =
        numWeights < aliasWeights.size ? numWeights : aliasWeights.size;
  }

  if (numAnims == 1) {
    aliasHandles.list[0] = handles[0];
    SetAnimHandle(declaration, curTime, 0, handles[0]);
    aliasWeights.list[0] = weights[0];
  } else if (numAnims != 0) {
    if (numAnims > 0) {
      for (int index = 0; index < numAnims; ++index) {
        aliasHandles.list[index] = handles[index];
        SetAnimHandle(declaration, curTime, index, handles[index]);
        aliasWeights.list[index] = weights[index];
      }
    }
    CalcAlphaValuesFromWeights();
  } else {
    if (aliasWeights.size < 0) {
      aliasWeights.ClearFree();
    }
    int nextNum = aliasWeights.size;
    if (nextNum > 0) {
      nextNum = 0;
    }
    aliasWeights.num = nextNum;

    if (alphaValues.size < 0) {
      alphaValues.ClearFree();
    }
    nextNum = alphaValues.size;
    if (nextNum > 0) {
      nextNum = 0;
    }
    alphaValues.num = nextNum;
  }
}
