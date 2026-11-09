#define WOLFTNO_RETAIL_FACETRACKS_FIND_ALIAS_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/animstack/animator_facetracks.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_FACETRACKS_FIND_ALIAS_ABI

#include "engine/gamelib/animstack/animator_treesway_abi.h"
#include "engine/gamelib/animstack/animstack.h"
#include "shared/idlib/lib.h"

#include <cstdint>

class idAnimation;
extern idAnimation* animation;

namespace {

constexpr char FACE_SETUP_ALIAS[] = "facesetup/facesetup";

// idAnimation's retail alias operations occupy vtable slots 30, 33, and 34.
// This local view leaves the still-unrecovered animation service header alone
// while preserving the exact indirect-call ABI used by this compiland.
class idAnimationFaceTracksABI {
public:
  virtual void Slot00() = 0;
  virtual void Slot01() = 0;
  virtual void Slot02() = 0;
  virtual void Slot03() = 0;
  virtual void Slot04() = 0;
  virtual void Slot05() = 0;
  virtual void Slot06() = 0;
  virtual void Slot07() = 0;
  virtual void Slot08() = 0;
  virtual void Slot09() = 0;
  virtual void Slot10() = 0;
  virtual void Slot11() = 0;
  virtual void Slot12() = 0;
  virtual void Slot13() = 0;
  virtual void Slot14() = 0;
  virtual void Slot15() = 0;
  virtual void Slot16() = 0;
  virtual void Slot17() = 0;
  virtual void Slot18() = 0;
  virtual void Slot19() = 0;
  virtual void Slot20() = 0;
  virtual void Slot21() = 0;
  virtual void Slot22() = 0;
  virtual void Slot23() = 0;
  virtual void Slot24() = 0;
  virtual void Slot25() = 0;
  virtual void Slot26() = 0;
  virtual void Slot27() = 0;
  virtual void Slot28() = 0;
  virtual void Slot29() = 0;
  virtual idAliasHandle_t FindAliasHandle(const idDeclMD6* declaration,
                                          const char* aliasName) = 0;
  virtual void Slot31() = 0;
  virtual void Slot32() = 0;
  virtual idAliasHandle_t AddStrongAliasRef(const char* aliasName) = 0;
  virtual idAliasHandle_t StrongLoadAlias(const idDeclMD6* declaration,
                                          const char* aliasName) = 0;
};

struct idDeclMD6FaceTracksView {
  void* vtable;
  const char* name;
  std::uint8_t prefix[0x50];
  idMD6Model* model;
};

struct idAnimStackFaceTracksView {
  void* treeAnimator;
  idJointAnimator* jointAnimator;
  std::uint8_t allocator[0x138];
  idList<idAnimator_Base*, 25> blendStack;
};

__forceinline idAnimationFaceTracksABI* AnimationABI(idAnimation* service) {
  return reinterpret_cast<idAnimationFaceTracksABI*>(service);
}

__forceinline const idDeclMD6FaceTracksView*
DeclView(const idDeclMD6* declaration) {
  return reinterpret_cast<const idDeclMD6FaceTracksView*>(declaration);
}

__forceinline idAnimStackFaceTracksView* StackView(idAnimStack* stack) {
  return reinterpret_cast<idAnimStackFaceTracksView*>(stack);
}

__forceinline const idAnimStackFaceTracksView*
StackView(const idAnimStack* stack) {
  return reinterpret_cast<const idAnimStackFaceTracksView*>(stack);
}

} // namespace

static_assert(offsetof(idDeclMD6FaceTracksView, model) == 0x60,
              "face-tracks declaration model offset drift");
static_assert(offsetof(idAnimStackFaceTracksView, blendStack) == 0x148,
              "face-tracks blend-stack offset drift");

idAnimator_FaceTracks::idAnimator_FaceTracks()
    : leaves(), branches(), leafAnimModsTime(), mergeBranch(),
      endBranch(nullptr), faceSetupAnimAlias(), animModsTime(-1) {}

idAnimator_FaceTracks::~idAnimator_FaceTracks() = default;

const idMD6Branch*
idAnimator_FaceTracks::InternalGetMergeBranch() const {
  return &mergeBranch;
}

idMD6Branch* idAnimator_FaceTracks::InternalGetMergeBranch() {
  return &mergeBranch;
}

const idMD6Branch* idAnimator_FaceTracks::InternalGetEndBranch() const {
  return endBranch;
}

idMD6Branch* idAnimator_FaceTracks::InternalGetEndBranch() {
  return endBranch;
}

bool idAnimator_FaceTracks::HasFaceTracks(const idDeclMD6* declaration) {
  if (declaration == nullptr) {
    return false;
  }
  return AnimationABI(animation)
             ->FindAliasHandle(declaration, FACE_SETUP_ALIAS)
             .value != 0xFFFFu;
}

void idAnimator_FaceTracks::InternalPreBlendTree(const idAnimStack*,
                                                  int curTime, int) {
  if (endBranch != nullptr) {
    endBranch->left = nullptr;
  }

  if (animModsTime == -1) {
    return;
  }
  if (curTime > animModsTime) {
    animModsTime = -1;
  }
  for (int index = 0; index < leaves.num; ++index) {
    if (curTime > leafAnimModsTime.list[index]) {
      leafAnimModsTime.list[index] = -1;
      leaves.list[index]->SetNumAnimMods(0);
    }
  }
}

void idAnimator_FaceTracks::AddUserChannelMod(idUserChannelIndex userChannel,
                                               float value, int curTime) {
  animModsTime = animModsTime > curTime ? animModsTime : curTime;

  const float channelAsFloat = static_cast<float>(userChannel.value);
  int leafIndex = leaves.num - 1;
  std::int64_t remaining = leafIndex;
  if (leafIndex <= 0) {
    return;
  }

  idMD6LeafPause** const leafList = leaves.list;
  while (!(leafList[leafIndex - 1]->frame > channelAsFloat)) {
    --remaining;
    --leafIndex;
    if (remaining <= 0) {
      return;
    }
  }

  if (leafIndex == -1) {
    return;
  }
  idMD6LeafPause* const leaf = leafList[leafIndex];
  int modIndex = 0;
  if (leafAnimModsTime.list[leafIndex] == curTime) {
    modIndex = leaf->animMods[0].num;
  }
  leafAnimModsTime.list[leafIndex] = curTime;
  leaf->SetNumAnimMods(modIndex + 1);

  idMD6Blend::jointMod_t& mod =
      leaf->animMods[leaf->currentDeferred & 1].list[modIndex];
  *reinterpret_cast<std::uint64_t*>(&mod.mat[0]) = UINT64_C(0x3F800000);
  *reinterpret_cast<std::uint32_t*>(&mod.mat[2]) = 0;
  mod.s[0] = value;
  mod.joint.value = userChannel.value;
  mod.flags = 0x8010u;
}

void idAnimator_FaceTracks::InternalShutdown(idAnimStack* stack) {
  endBranch = nullptr;
  for (int index = 0; index < leaves.num; ++index) {
    auto* const allocator = reinterpret_cast<idMD6Allocator*>(
        reinterpret_cast<std::uint8_t*>(stack) + 0x10);
    idMD6Allocator::FreeTyped(allocator, leaves.list[index]);
  }
  for (int index = 0; index < branches.num; ++index) {
    auto* const allocator = reinterpret_cast<idMD6Allocator*>(
        reinterpret_cast<std::uint8_t*>(stack) + 0x10);
    idMD6Allocator::FreeTyped(allocator, branches.list[index]);
  }
  if (leaves.listStatic == 0 || leaves.listStatic == 2) {
    idMD6LeafPause** const allocation = leaves.list;
    if (allocation != nullptr) {
      idMem::Singleton().Free(allocation, ALIGN_16);
    }
    leaves.list = nullptr;
    leaves.size = 0;
  }
  leaves.num = 0;

  if (branches.listStatic == 0 || branches.listStatic == 2) {
    idMD6Branch** const allocation = branches.list;
    if (allocation != nullptr) {
      idMem::Singleton().Free(allocation, ALIGN_16);
    }
    branches.list = nullptr;
    branches.size = 0;
  }
  branches.num = 0;
}

bool idAnimator_FaceTracks::InternalInit(
    const idAnimatorParms_Base& parms) {
  idAnimStack* stack = parms.animStack;
  const idDeclMD6* const declaration = stack->jointAnimator->decl;
  if (declaration == nullptr || DeclView(declaration)->model == nullptr) {
    return false;
  }

  for (int index = 0; index < StackView(stack)->blendStack.num; ++index) {
    idAnimator_Base* const animator = stack->GetAnimatorByIndex(index);
    auto* const faceTracks = dynamic_cast<idAnimator_FaceTracks*>(animator);
    if (faceTracks != nullptr && faceTracks != this) {
      idLib::Warning(
          "More than one joint was found in the focus group for md6Def '%s'. "
          "The first joint will be used.",
          "<unknown>");
      return false;
    }
    stack = parms.animStack;
  }

  AnimationABI(animation)->AddStrongAliasRef(FACE_SETUP_ALIAS);
  faceSetupAnimAlias = AnimationABI(animation)->StrongLoadAlias(
      declaration, FACE_SETUP_ALIAS);

  using FindAliasFunction = const idMD6Alias* (idDeclMD6::*)(
      const idAliasHandle_t&, bool, bool) const;
  const auto findAlias = static_cast<FindAliasFunction>(&idDeclMD6::FindAlias);
  const idMD6Alias* const alias =
      (declaration->*findAlias)(faceSetupAnimAlias, true, true);
  if (alias == nullptr) {
    return false;
  }
  const idMD6Anim* const anim = alias->mappings.list[0].anim;
  if (anim == nullptr) {
    return false;
  }
  const idMD6AnimData* const animData = anim->animData;
  if (animData == nullptr) {
    return false;
  }

  const int numFrameSets = static_cast<int>(animData->numFrameSets);
  auto& leavesResize =
      *reinterpret_cast<idList<idMD6Anim*, 5>*>(&leaves);
  if (numFrameSets <= leaves.size || leavesResize.Resize(numFrameSets)) {
    leaves.num = numFrameSets < leaves.size ? numFrameSets : leaves.size;
  }
  auto& branchesResize =
      *reinterpret_cast<idList<idMD6Anim*, 5>*>(&branches);
  if (numFrameSets <= branches.size || branchesResize.Resize(numFrameSets)) {
    branches.num =
        numFrameSets < branches.size ? numFrameSets : branches.size;
  }
  if (numFrameSets <= leafAnimModsTime.size ||
      leafAnimModsTime.Resize(numFrameSets)) {
    leafAnimModsTime.num = numFrameSets < leafAnimModsTime.size
                               ? numFrameSets
                               : leafAnimModsTime.size;
  }

  idMD6Branch* previousBranch = nullptr;
  const auto* const animBytes =
      reinterpret_cast<const std::uint8_t*>(animData);
  const auto* const frameSetOffsets = reinterpret_cast<const std::uint32_t*>(
      animBytes + animData->frameSetOffsetTblOffset);
  for (int index = 0; index < numFrameSets; ++index) {
    leaves.list[index] = static_cast<idMD6LeafPause*>(
        reinterpret_cast<idMD6Allocator*>(
            reinterpret_cast<std::uint8_t*>(parms.animStack) + 0x10)
            ->Alloc(idMD6Node::NODE_LEAF_PAUSE));
    branches.list[index] = static_cast<idMD6Branch*>(
        reinterpret_cast<idMD6Allocator*>(
            reinterpret_cast<std::uint8_t*>(parms.animStack) + 0x10)
            ->Alloc(idMD6Node::NODE_BRANCH));
    leafAnimModsTime.list[index] = -1;

    const std::uint32_t scaledOffset =
        frameSetOffsets[numFrameSets - index - 1] << 4;
    const auto signedOffset = static_cast<std::int32_t>(scaledOffset);
    const std::uint16_t frame = *reinterpret_cast<const std::uint16_t*>(
        animBytes + signedOffset + 0x22);

    idMD6LeafPause* const leaf = leaves.list[index];
    leaf->Init(anim, static_cast<float>(frame), idMD6Leaf::WRAP_CLAMP,
               MD6_WEIGHTGROUP_FACE);
    leaf->flags = static_cast<std::int16_t>(leaf->flags | 0x80);

    idMD6Branch* const branch = branches.list[index];
    if (previousBranch != nullptr) {
      previousBranch->left = branch;
    }
    branch->Init(nullptr, leaf, idMD6Blend::BOP_ADD_RIGHT,
                 idMD6Blend::ORIGINBLEND_BRANCH, 1.0f, 1.0f, 0.0f,
                 MD6_WEIGHTGROUP_FACE, idMD6Branch::BLEND_LINEAR);
    previousBranch = branch;
  }

  mergeBranch.targetAlpha = 1.0f;
  mergeBranch.currentAlpha = 1.0f;
  if (numFrameSets <= 1) {
    if (numFrameSets > 0) {
      mergeBranch.right = leaves.list[0];
    }
  } else {
    mergeBranch.right = branches.list[0];
    endBranch = branches.list[numFrameSets - 1];
  }
  return true;
}
