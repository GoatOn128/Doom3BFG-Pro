#define WOLFTNO_RETAIL_CHANNELWEIGHT_ANIMFORALIAS_ABI
#define WOLFTNO_RETAIL_CHANNELWEIGHT_TICKS_ABI
#define WOLFTNO_RETAIL_CHANNELWEIGHT_USERCHANNEL_RESTRICT_ABI
#define WOLFTNO_RETAIL_CHANNELWEIGHT_USHORT_ALLOC_REF_ABI
#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDLIST_NOINLINE_ALLOC_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#include "engine/gamelib/animstack/animator_channelweight.h"

#include "engine/framework/gametimemanager.h"
#include "engine/framework/timemanager.h"
#include "engine/gamelib/animstack/animstack.h"
#include "engine/models/skeletalanimation/jointanimator.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"

#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_NOINLINE_ALLOC_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_CHANNELWEIGHT_USHORT_ALLOC_REF_ABI
#undef WOLFTNO_RETAIL_CHANNELWEIGHT_USERCHANNEL_RESTRICT_ABI
#undef WOLFTNO_RETAIL_CHANNELWEIGHT_TICKS_ABI
#undef WOLFTNO_RETAIL_CHANNELWEIGHT_ANIMFORALIAS_ABI

#include <algorithm>
#include <cstdint>
#include <malloc.h>

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

template <>
bool idList<std::uint16_t, 5>::Resize(int newSize);

extern bool forceIdListsToReallocateEveryAppend;

idCVar debug_channelWeight(
    "debug_channelWeight", "-1", CVAR_INTEGER,
    "enable Animator_ChannelWeight debug info for the specified entity number",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

template <typename Type>
__forceinline const Type& RetailAt(const void* object, std::size_t offset) {
  return *reinterpret_cast<const Type*>(
      static_cast<const std::uint8_t*>(object) + offset);
}

template <typename Type>
__forceinline Type& RetailAt(void* object, std::size_t offset) {
  return *reinterpret_cast<Type*>(static_cast<std::uint8_t*>(object) +
                                  offset);
}

__forceinline const idMD6Model* DeclModel(const idDeclMD6* declaration) {
  return RetailAt<const idMD6Model*>(declaration, 0x60);
}

__forceinline const idAliasHandle_t&
DeclBaseUserChannelAlias(const idDeclMD6* declaration) {
  return RetailAt<idAliasHandle_t>(declaration, 0x514);
}

__forceinline const idList<idAliasHandle_t, 19>&
DeclUserChannelAliasMap(const idDeclMD6* declaration) {
  return RetailAt<idList<idAliasHandle_t, 19>>(declaration, 0x518);
}

__forceinline std::uint16_t ModelNumUserChannels(const idMD6Model* model) {
  const void* const skeleton = RetailAt<const void*>(model, 0x128);
  const idMD6SkelData* const data =
      RetailAt<const idMD6SkelData*>(skeleton, 0x38);
  return data->numUserChannels;
}

template <typename Type, int Tag>
__forceinline void ClearListExact(idList<Type, Tag>& list) {
  if (list.listStatic == 0 || list.listStatic == 2) {
    if (list.list != nullptr) {
      idMem::Singleton().Free(list.list, ALIGN_16);
    }
    list.list = nullptr;
    list.size = 0;
  }
  list.num = 0;
}

} // namespace

const idMD6Branch*
idAnimator_ChannelWeight::InternalGetMergeBranch() const {
  return mergeBranch;
}

void idAnimator_ChannelWeight::InternalShutdown(idAnimStack* stack) {
  idMD6Allocator* const allocator = stack->GetAllocator();
  if (mergeBranch != nullptr) {
    idMD6Allocator::FreeTyped(allocator, mergeBranch);
    mergeBranch = nullptr;
  }
  if (baseAnimation.leaf != nullptr) {
    idMD6Allocator::FreeTyped(allocator, baseAnimation.leaf);
    baseAnimation.leaf = nullptr;
  }
  for (int index = 0; index < leaves.num; ++index) {
    leafAnimation_t& animation = leaves.list[index];
    if (animation.leaf != nullptr) {
      idMD6Allocator::FreeTyped(allocator, animation.leaf);
      animation.leaf = nullptr;
    }
    if (animation.branch != nullptr) {
      idMD6Allocator::FreeTyped(allocator, animation.branch);
      animation.branch = nullptr;
    }
  }
  ClearListExact(userChannelToLeafMap);
  ClearListExact(leaves);
}

void idAnimator_ChannelWeight::InternalPreBlendTree(
    const idAnimStack* stack, int curTime, int) {
  if (!IsContributing()) {
    return;
  }

  const idDeclMD6* const declaration = stack->jointAnimator->decl;
  const idMD6Model* const model = DeclModel(declaration);
  const std::uint16_t paddedUserChannels = static_cast<std::uint16_t>(
      ModelNumUserChannels(model) + static_cast<std::uint16_t>(7));
  const unsigned int userChannelBytes =
      4u * static_cast<unsigned int>(paddedUserChannels & 0xFFF8u);
  float* const userChannels =
      static_cast<float*>(_alloca(userChannelBytes));
  const int previousTime =
      curTime - gametimeManager.gameTimeManager->GetGameMsPerFrame(
                    GAMETIME_SCALED);

  const idMD6Leaf::wrapMode_t wrapMode =
      static_cast<idMD6Leaf::wrapMode_t>(controlAnimation->wrapMode);
  const idTypesafeNumber<int, gameTimeUnique_t> animDurationTicks =
      controlAnimation->GetAnimTicks(timeManager.gameTimePerSecond.value);
  const idTypesafeNumber<int, gameTimeUnique_t> totalAnimTicks =
      controlAnimation->GetFrameTicks(previousTime);
  float normalizedTime = 1.0f;
  if (wrapMode != idMD6Leaf::WRAP_CLAMP ||
      totalAnimTicks.value < animDurationTicks.value) {
    normalizedTime = static_cast<float>(totalAnimTicks.value %
                                        animDurationTicks.value) /
                     static_cast<float>(animDurationTicks.value);
  }

  const idMD6Anim* const animation = controlAnimation->anim;
  const int numFrames =
      animation->animData != nullptr ? animation->animData->numFrames : 0;
  const float frame = static_cast<float>(numFrames - 1) * normalizedTime;
  model->GetUserChannelsForAnimFrame(userChannels, userChannelBytes,
                                     animation, frame);

  idStr userChannelOutput;
  const bool debug = debug_channelWeight.valueInteger != -1 &&
                     debug_channelWeight.valueInteger == entityNumber;
  if (debug) {
    userChannelOutput =
        va("time = %.3f frame = %.3f ( ", normalizedTime, frame);
  }

  for (int channel = 0; channel < userChannelToLeafMap.num; ++channel) {
    const std::uint16_t leafIndex = userChannelToLeafMap.list[channel];
    if (leafIndex == InvalidLeafIndex) {
      continue;
    }
    const float alpha = userChannels[channel];
    idMD6Branch* const branch = leaves.list[leafIndex].branch;
    branch->currentAlpha = alpha;
    branch->targetAlpha = alpha;
    if (debug) {
      userChannelOutput += va("%.3f ", alpha);
    }
  }
  if (debug) {
    userChannelOutput += ")\n";
    idLib::Printf(userChannelOutput.c_str());
  }
}

bool idAnimator_ChannelWeight::InternalIsContributing() const {
  return controlAnimation != nullptr && baseAnimation.animation != nullptr &&
         leaves.num > 0 && userChannelToLeafMap.num > 0;
}

int idMD6Anim::NumCompressedUserChannels() const {
  const idMD6AnimData* const data = animData;
  if (data == nullptr) {
    return 0;
  }

  int firstCount = 0;
  int secondCount = 0;
  if ((data->flags & 0x800u) != 0) {
    const std::uint8_t* const bytes =
        reinterpret_cast<const std::uint8_t*>(data);
    const std::uint16_t firstOffset =
        *reinterpret_cast<const std::uint16_t*>(
            bytes + sizeof(idMD6AnimData) + offsetof(idMD6AnimData, flags) +
            sizeof(std::uint16_t) * data->numAnimMaps);
    const std::uint16_t secondOffset =
        *reinterpret_cast<const std::uint16_t*>(
            bytes + sizeof(idMD6AnimData) +
            offsetof(idMD6AnimData, frameSetTblOffset) +
            sizeof(std::uint16_t) * data->numAnimMaps);
    firstCount = bytes[firstOffset];
    secondCount = bytes[secondOffset];
  }
  return firstCount + secondCount;
}

idAnimator_ChannelWeight::idAnimator_ChannelWeight()
    : userChannelToLeafMap(), leaves(), baseAnimation(), mergeBranch(nullptr),
      controlAnimation(nullptr), entityNumber(8191), lockOwner(nullptr) {}

idAnimator_ChannelWeight::~idAnimator_ChannelWeight() = default;

void idAnimator_ChannelWeight::SetControlAnimation(
    const idMD6LeafPlay* animation, void* lockOwner_) {
  if (lockOwner != nullptr && lockOwner != lockOwner_) {
    return;
  }

  const idMD6Anim* const anim = animation != nullptr ? animation->anim : nullptr;
  const int numCompressedUserChannels =
      anim != nullptr ? anim->NumCompressedUserChannels() : 0;
  if (numCompressedUserChannels > 0 &&
      numCompressedUserChannels <= leaves.num) {
    mergeBranch->currentAlpha = 1.0f;
    mergeBranch->targetAlpha = 1.0f;
    controlAnimation = animation;
    lockOwner = animation != nullptr ? lockOwner_ : nullptr;
  } else {
    controlAnimation = nullptr;
    lockOwner = nullptr;
  }
}

template <>
__declspec(noinline) bool
idList<idAnimator_ChannelWeight::leafAnimation_t, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearListExact(*this);
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }

  idAnimator_ChannelWeight::leafAnimation_t* const replacement =
      static_cast<idAnimator_ChannelWeight::leafAnimation_t*>(
          idListArrayResize<idAnimator_ChannelWeight::leafAnimation_t>(
          list, size, newSize, static_cast<memTag_t>(memTag), false));
  list = replacement;
  if (replacement == nullptr) {
    return false;
  }
  size = newSize;
  if (num > newSize) {
    num = newSize;
  }
  return true;
}

template <>
__declspec(noinline)
idAnimator_ChannelWeight::leafAnimation_t&
idList<idAnimator_ChannelWeight::leafAnimation_t, 5>::Alloc() {
  const int required = num + 1;
  if (required > size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = required;
    } else if (granularity != 0) {
      const int rounded = required + granularity - 1;
      newSize = rounded - rounded % granularity;
    } else {
      for (newSize = 1; newSize < required; newSize *= 2) {
      }
    }
    Resize(newSize);
  }
  const int index = num;
  if (index >= size) {
    return list[size - 1];
  }
  ++num;
  return list[index];
}

bool idAnimator_ChannelWeight::InternalInit(
    const idAnimatorParms_Base& parms) {
  idAnimStack* const stack = parms.animStack;
  idMD6Allocator* const allocator = stack->GetAllocator();
  mergeBranch = static_cast<idMD6Branch*>(
      allocator->Alloc(idMD6Node::NODE_BRANCH));
  if (mergeBranch == nullptr) {
    return false;
  }
  baseAnimation.leaf = static_cast<idMD6LeafPause*>(
      allocator->Alloc(idMD6Node::NODE_LEAF_PAUSE));
  if (baseAnimation.leaf == nullptr) {
    return false;
  }

  const idDeclMD6* const declaration = stack->jointAnimator->decl;
  filterGroup = parms.filterGroup;
  const idList<idAliasHandle_t, 19>& aliasMap =
      DeclUserChannelAliasMap(declaration);
  const int numUserChannels = ModelNumUserChannels(DeclModel(declaration));
  if (numUserChannels > userChannelToLeafMap.size) {
    userChannelToLeafMap.Resize(numUserChannels);
  }
  if (aliasMap.num > leaves.size) {
    leaves.Resize(aliasMap.num);
  }

  int numLeaves = 0;
  for (int channel = 0; channel < numUserChannels; ++channel) {
    const idAliasHandle_t alias = aliasMap.list[static_cast<std::int16_t>(channel)];
    const idMD6Anim* const animation =
        declaration->AnimForAlias(alias, true);
    std::uint16_t& leafIndex = userChannelToLeafMap.Alloc();
    if (animation != nullptr) {
      leafIndex = static_cast<std::uint16_t>(numLeaves++);
      leaves.Alloc().animation = animation;
    } else {
      leafIndex = InvalidLeafIndex;
    }
  }

  mergeBranch->op = idMD6Blend::BOP_ADD_RIGHT;
  mergeBranch->filterGroup = static_cast<std::uint8_t>(filterGroup);
  mergeBranch->currentAlpha = 0.0f;
  mergeBranch->targetAlpha = 0.0f;
  baseAnimation.animation =
      declaration->AnimForAlias(DeclBaseUserChannelAlias(declaration), true);
  baseAnimation.leaf->Init(baseAnimation.animation, 0.0f,
                           idMD6Leaf::WRAP_CLAMP, filterGroup);

  idMD6Node* left = baseAnimation.leaf;
  bool success = true;
  for (int index = 0; index < leaves.num; ++index) {
    idMD6LeafPause* const leaf = static_cast<idMD6LeafPause*>(
        allocator->Alloc(idMD6Node::NODE_LEAF_PAUSE));
    if (leaf == nullptr) {
      success = false;
      break;
    }
    idMD6Branch* const branch = static_cast<idMD6Branch*>(
        allocator->Alloc(idMD6Node::NODE_BRANCH));
    if (branch == nullptr) {
      success = false;
      break;
    }

    const va leafName("%s_b%d", "<unknown>", index);
    static_cast<void>(leafName);
    leafAnimation_t& leafAnimation = leaves.list[index];
    leaf->Init(leafAnimation.animation, 0.0f, idMD6Leaf::WRAP_CLAMP,
               filterGroup);
    branch->Init(left, leaf, idMD6Blend::BOP_ADD_RIGHT,
                 idMD6Blend::ORIGINBLEND_BRANCH, 0.0f, 0.0f, 0.0f,
                 filterGroup, idMD6Branch::BLEND_LINEAR);
    leafAnimation.branch = branch;
    leafAnimation.leaf = leaf;
    left = branch;
  }

  mergeBranch->right = left;
  if (!success) {
    InternalShutdown(stack);
  }
  return success;
}
