#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/animstack/animstack.h"

#include "engine/decls/decl.h"
#include "engine/framework/common.h"
#include "engine/framework/gametimemanager.h"
#include "engine/gamelib/animstack/animator_ik.h"
#include "engine/gamelib/animstack/animator_jointmod.h"
#include "engine/gamelib/animstack/animator_pain.h"
#include "engine/gamelib/animstack/animator_torsotracker.h"
#include "engine/gamelib/animstack/animrecorder.h"
#include "engine/gamelib/animstack/animweb/animator_animweb_abi.h"
#include "engine/models/serializer.h"
#include "engine/models/skeletalanimation/jointanimator.h"
#include "engine/models/skeletalanimation/treeanimator.h"
#include "engine/sound/sound_runtime.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/str.h"

#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <xmmintrin.h>

extern bool forceIdListsToReallocateEveryAppend;
bool AssertFailed(const char* file, int line, const char* expression,
                  bool inlineBreak);

idCVar anim_blendSnapshotDebug(
    "anim_blendSnapshotDebug", "0", CVAR_BOOL,
    "Debug events in idAnimStack::BlendSnapshot\n",
    static_cast<argCompletionRef_t>(nullptr));
idCVar anim_blendSnapshotNoLerp(
    "anim_blendSnapshotNoLerp", "0", CVAR_BOOL,
    "Dont lerp between serialized animstacks, for testing",
    static_cast<argCompletionRef_t>(nullptr));

namespace {

bool IsBranchType(std::uint8_t type) {
  return type == idMD6Node::NODE_BRANCH ||
         type == idMD6Node::NODE_BLEND_BRANCH ||
         type == idMD6Node::NODE_BLENDA_BRANCH ||
         type == idMD6Node::NODE_FUSION_BRANCH;
}

idMD6Node* SelectBestLeaf(idMD6Node* node) {
  if (node == nullptr || node->type != idMD6Node::NODE_BEST_LEAF) {
    return node;
  }
  auto* best = static_cast<idMD6BestLeaf*>(node);
  if (best->leafList.num == 0) {
    return nullptr;
  }
  const int index = best->bestLeafIndex < best->leafList.num
                        ? best->bestLeafIndex
                        : 0;
  return best->leafList.list[index];
}

const idMD6Node* SelectBestLeaf(const idMD6Node* node) {
  return SelectBestLeaf(const_cast<idMD6Node*>(node));
}

void SerializeFloatBits(idSerializer& serializer, float& value) {
  int bits = 0;
  if (serializer.writing) {
    std::memcpy(&bits, &value, sizeof(bits));
    serializer.msg->WriteBits(bits, 32);
  } else {
    bits = serializer.msg->ReadBits(32);
    std::memcpy(&value, &bits, sizeof(value));
  }
}

int ClampByteFromFloat(float value) {
  const int converted = static_cast<int>(value);
  if (converted < 0) {
    return 0;
  }
  return converted > 255 ? 255 : converted;
}

float RetailSqrt(float value) {
  const __m128 input = _mm_set_ss(value);
  const __m128 minimum = _mm_set_ss(1.17549435e-38f);
  return _mm_cvtss_f32(_mm_sqrt_ss(_mm_max_ss(input, minimum)));
}

idQuat RetailMat3ToQuat(const idMat3& axis) {
  idQuat result{};
  const float trace = (axis[0].x + axis[1].y) + axis[2].z;
  if (trace > 0.0f) {
    const float rootSquared = trace + 1.0f;
    const float root = RetailSqrt(rootSquared);
    const float inverse = (1.0f / root) * 0.5f;
    result.x = (axis[2].y - axis[1].z) * inverse;
    result.y = (axis[0].z - axis[2].x) * inverse;
    result.z = (axis[1].x - axis[0].y) * inverse;
    result.w = rootSquared * inverse;
    return result;
  }

  int i = axis[1].y > axis[0].x ? 1 : 0;
  if (axis[2].z > axis[i][i]) {
    i = 2;
  }
  static constexpr int next[3] = {1, 2, 0};
  const int j = next[i];
  const int k = next[j];
  const float rootSquared =
      (axis[i][i] - (axis[k][k] + axis[j][j])) + 1.0f;
  const float root = RetailSqrt(rootSquared);
  float inverse = (1.0f / root) * 0.5f;
  const float difference = axis[k][j] - axis[j][k];
  if (difference < 0.0f) {
    inverse = -inverse;
  }
  result[i] = rootSquared * inverse;
  result.w = difference * inverse;
  result[j] = (axis[j][i] + axis[i][j]) * inverse;
  result[k] = (axis[k][i] + axis[i][k]) * inverse;
  return result;
}

void DetachSerializedTreeRoot(idMD6Node* node) {
  if (node != nullptr) {
    *reinterpret_cast<void**>(reinterpret_cast<std::uint8_t*>(node) + 8) =
        nullptr;
  }
}

} // namespace

void idSerializer::SerializeQ(idMat3* axis, int bits) {
  const float scale = static_cast<float>((1 << (bits - 1)) - 1);
  if (writing) {
    const idQuat quaternion = RetailMat3ToQuat(*axis);
    const float values[4] = {quaternion.x, quaternion.y, quaternion.z,
                             quaternion.w};
    int largest = 0;
    for (int index = 1; index < 4; ++index) {
      if (std::fabs(values[index]) > std::fabs(values[largest])) {
        largest = index;
      }
    }
    msg->WriteBits(largest, 2);
    float first = values[(largest + 1) & 3];
    float second = values[(largest - 2) & 3];
    float third = values[(largest - 1) & 3];
    if (values[largest] < 0.0f) {
      first = -first;
      second = -second;
      third = -third;
    }
    msg->WriteBits(static_cast<int>(first * scale), -bits);
    msg->WriteBits(static_cast<int>(second * scale), -bits);
    msg->WriteBits(static_cast<int>(third * scale), -bits);
    return;
  }

  const int largest = msg->ReadBits(2);
  const float first = static_cast<float>(msg->ReadBits(-bits)) / scale;
  const float second = static_cast<float>(msg->ReadBits(-bits)) / scale;
  const float third = static_cast<float>(msg->ReadBits(-bits)) / scale;
  float values[4]{};
  values[(largest + 1) & 3] = first;
  values[(largest - 2) & 3] = second;
  values[(largest - 1) & 3] = third;
  const float remainder =
      ((1.0f - (first * first)) - (second * second)) - (third * third);
  values[largest] = RetailSqrt(std::fabs(remainder));
  *axis = idQuat{values[0], values[1], values[2], values[3]}.ToMat3();
}

class idAnimator_Client final : public idAnimator_Base {
public:
  ~idAnimator_Client() override = default;

protected:
  bool InternalInit(const idAnimatorParms_Base&) override { return true; }
  void InternalShutdown(idAnimStack*) override {}
  void InternalPreBlendTree(const idAnimStack*, int, int) override {}
  void InternalPostBlendTree(const idAnimStack*, int) override {}
  void InternalStart(const idAnimStack*, int,
                     idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalEnd(const idAnimStack*, int,
                   idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalBlend(const idAnimStack*, int, float,
                     idTypesafeNumber<int, gameTimeUnique_t>) override {}
  bool InternalIsContributing() const override { return false; }
  const idMD6Branch* InternalGetMergeBranch() const override {
    return nullptr;
  }
  idMD6Branch* InternalGetMergeBranch() override { return nullptr; }
};

class idAnimator_AF final : public idAnimator_Base {
public:
  idAnimator_AF();
  ~idAnimator_AF() override;
  serializeType_t GetSerializeType() override { return STYPE_AF; }

protected:
  bool InternalInit(const idAnimatorParms_Base&) override;
  void InternalShutdown(idAnimStack*) override;
  void InternalPreBlendTree(const idAnimStack*, int, int) override;
  void InternalPostBlendTree(const idAnimStack*, int) override;
  void InternalStart(const idAnimStack*, int,
                     idTypesafeNumber<int, gameTimeUnique_t>) override;
  void InternalEnd(const idAnimStack*, int,
                   idTypesafeNumber<int, gameTimeUnique_t>) override;
  void InternalBlend(const idAnimStack*, int, float,
                     idTypesafeNumber<int, gameTimeUnique_t>) override;
  bool InternalIsContributing() const override;
  const idMD6Branch* InternalGetMergeBranch() const override;
  idMD6Branch* InternalGetMergeBranch() override;

private:
  std::uint8_t recovered[0x510];
};

static_assert(sizeof(idAnimator_AF) == 0x560,
              "idAnimator_AF ABI drift");

class idAnimator_AnimWebHands final : public idAnimator_AnimWeb {
public:
  idAnimator_AnimWebHands();
  serializeType_t GetSerializeType() override {
    return STYPE_ANIMWEB_HANDS;
  }
  void SerializeSnapshot(idSerializer& serializer) override;
  void InternalInitWeb() override;
  int GetIdleSelector() const { return static_cast<int>(idleSelector); }

  float punchSelector;
  float throwSelector;
  float idleSelector;
  float mhSelector;
  float meleeReturnSelector;
  float zoomSelector;
  float weaponCondition;
  float variationIndex;
  float previousVariationIndex;
  float weaponAnimVarIndex;
  float prevWeaponAnimVarIndex;
  float attackVariation;
  float reloadVariation;
  float animRateVar;
  float altAnimRateVar;
  float alt2AnimRateVar;
};

static_assert(sizeof(idAnimator_AnimWebHands) == 0x3E8,
              "idAnimator_AnimWebHands ABI drift");
static_assert(offsetof(idAnimator_AnimWebHands, punchSelector) == 0x3A8,
              "hands selector ABI drift");
static_assert(offsetof(idAnimator_AnimWebHands, alt2AnimRateVar) == 0x3E4,
              "hands rate ABI drift");

idAnimator_AnimWebHands::idAnimator_AnimWebHands()
    : punchSelector(0.0f), throwSelector(0.0f), idleSelector(0.0f),
      mhSelector(0.0f), meleeReturnSelector(0.0f), zoomSelector(0.0f),
      weaponCondition(0.0f), variationIndex(0.0f),
      previousVariationIndex(0.0f), weaponAnimVarIndex(0.0f),
      prevWeaponAnimVarIndex(0.0f), attackVariation(0.0f),
      reloadVariation(0.0f), animRateVar(1.0f), altAnimRateVar(1.0f),
      alt2AnimRateVar(1.0f) {}

void idAnimator_AnimWebHands::InternalInitWeb() {
  idAnimator_AnimWeb::InternalInitWeb();
  AddBlendVariable("punchSelector", &punchSelector,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("throwSelector", &throwSelector,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("idleSelector", &idleSelector,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("mhSelector", &mhSelector,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("meleeReturnSelector", &meleeReturnSelector,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("zoomSelector", &zoomSelector,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("weaponCondition", &weaponCondition,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("variationIndex", &variationIndex,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("previousVariationIndex", &previousVariationIndex,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("weaponAnimVarIndex", &weaponAnimVarIndex,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("prevWeaponAnimVarIndex", &prevWeaponAnimVarIndex,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("attackVariation", &attackVariation,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("reloadVariation", &reloadVariation,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("animRateVar", &animRateVar,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("altAnimRateVar", &altAnimRateVar,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
  AddBlendVariable("alt2AnimRateVar", &alt2AnimRateVar,
                   idAnimWebScalarPair::FF_NO_OVERRIDE);
}

void idAnimator_AnimWebHands::SerializeSnapshot(idSerializer& serializer) {
  if (GetSerializeType() == STYPE_GENERIC) {
    return;
  }
  float* const values[] = {
      &punchSelector,          &throwSelector,       &idleSelector,
      &mhSelector,             &meleeReturnSelector, &zoomSelector,
      &weaponCondition,        &variationIndex,      &previousVariationIndex,
      &weaponAnimVarIndex,     &prevWeaponAnimVarIndex,
      &attackVariation,        &reloadVariation,     &animRateVar,
  };
  for (float* value : values) {
    SerializeFloatBits(serializer, *value);
  }
}

idAnimStack::syncGroup_t::syncGroup_t()
    : nodes(), fraction(0.0f), syncRate(0.0f), groupNumber(0) {
  nodes.memTag = 5;
}

idAnimStack::syncGroup_t& idAnimStack::syncGroup_t::operator=(
    const syncGroup_t& other) {
  if (this != &other) {
    static_cast<idList<syncNode_t, 5>&>(nodes) =
        static_cast<const idList<syncNode_t, 5>&>(other.nodes);
    for (int index = 0; index < 16; ++index) {
      nodes.staticList[index] = other.nodes.staticList[index];
    }
    fraction = other.fraction;
    syncRate = other.syncRate;
    groupNumber = other.groupNumber;
  }
  return *this;
}

idAnimStack::idAnimStack()
    : treeAnimator(nullptr), jointAnimator(nullptr), md6Allocator(),
      blendStack(), flags(), tree(nullptr), pauseTime(-1),
      skipSerialization(false), typesMemory{}, branchMemory{}, leafMemory{},
      serializedNewTree(false),
      prevSerializedTime(0), curSerializedTime(0),
      createdFromSnapshot(false), blendedFromSnapshot(false),
      disableSerialization(true),
      usedSnapshotNodes(), rememberedNodes() {
  rememberedNodes.granularity = 128;
  usedSnapshotNodes.granularity = 128;
}

idAnimStack::~idAnimStack() {
  idAnimRecorder* recorder = idAnimRecorder::instance;
  if (recorder == nullptr) {
    recorder = idAnimRecorder::allocFunction();
    idAnimRecorder::instance = recorder;
  }
  if (recorder->activeAnimStack == this) {
    recorder->Stop();
  }

  tree = nullptr;
  for (int index = 0; index < blendStack.num; ++index) {
    idAnimator_Base* animator = blendStack.list[index];
    if (animator == nullptr) {
      continue;
    }
    const bool serialized = ShutdownBlendStackIndex(index);
    if (!serialized) {
      blendStack.list[index] = nullptr;
      animator->Shutdown(this);
    }
  }
  blendStack.ClearFree();
  jointAnimator = nullptr;
  tree = nullptr;
}

void idAnimStack::Init(idTreeAnimator* animator) {
  tree = nullptr;
  treeAnimator = animator;
  if (animator != nullptr) {
    jointAnimator = animator->jointAnimator;
  }
}

void idAnimStack::Init(idJointAnimator* animator) {
  jointAnimator = animator;
  tree = nullptr;
  treeAnimator = nullptr;
}

void idAnimStack::Shutdown() { tree = nullptr; }

void idAnimStack::RemoveAnimator(idAnimator_Base* animator) {
  const int index = GetAnimatorIndex(animator);
  if (index < 0) {
    return;
  }
  blendStack.RemoveIndex(index);
  flags.RemoveIndex(index);
  tree = nullptr;
}

idAnimator_Base* idAnimStack::GetAnimator(const char* name) {
  for (int index = 0; index < blendStack.num; ++index) {
    if (idStr::Icmp("<unknown>", name) == 0) {
      return blendStack.list[index];
    }
  }
  return nullptr;
}

int idAnimStack::GetAnimatorIndex(const idAnimator_Base* animator) {
  for (int index = 0; index < blendStack.num; ++index) {
    if (blendStack.list[index] == animator) {
      return index;
    }
  }
  return -1;
}

idAnimator_Base* idAnimStack::GetAnimatorByIndex(int index) {
  return blendStack.list[index];
}

idAnimator_Base*
idAnimStack::GetAnimatorBySerializeType(serializeType_t type) {
  for (int index = 0; index < blendStack.num; ++index) {
    if (blendStack.list[index]->GetSerializeType() == type) {
      return blendStack.list[index];
    }
  }
  return nullptr;
}

void idAnimStack::ClearAnimatorFlag(const idAnimator_Base* animator,
                                    animatorFlag_t flag) {
  const int index = GetAnimatorIndex(animator);
  if (index >= 0) {
    flags.list[index] &= static_cast<std::uint8_t>(~flag);
  }
}

void idAnimStack::SetAnimatorFlag(const idAnimator_Base* animator,
                                  animatorFlag_t flag) {
  const int index = GetAnimatorIndex(animator);
  if (index >= 0) {
    flags.list[index] |= static_cast<std::uint8_t>(flag);
  }
}

bool idAnimStack::IsAnimatorFlagSet(const idAnimator_Base* animator,
                                    animatorFlag_t flag) {
  const int index = GetAnimatorIndex(animator);
  return index >= 0 &&
         (flags.list[index] & static_cast<std::uint8_t>(flag)) != 0;
}

void idAnimStack::Pause(idTypesafeNumber<int, gameTimeUnique_t> curTime) {
  if (pauseTime.value < 0) {
    pauseTime = curTime;
  }
  for (int index = 0; index < blendStack.num; ++index) {
    blendStack.list[index]->Pause(this, curTime);
  }
}

void idAnimStack::Unpause(idTypesafeNumber<int, gameTimeUnique_t> curTime) {
  if (pauseTime.value < 0) {
    return;
  }
  const int timeOffset = curTime.value - pauseTime.value;
  pauseTime.value = -1;

  if (tree != nullptr) {
    idMD6Node* stack[32];
    int top = 0;
    stack[0] = tree;
    while (top >= 0) {
      idMD6Node* node = SelectBestLeaf(stack[top--]);
      if (node == nullptr) {
        continue;
      }
      if (IsBranchType(node->type)) {
        auto* branch = static_cast<idMD6Branch*>(node);
        if (branch->left != nullptr) {
          stack[++top] = branch->left;
        }
        if (branch->right != nullptr) {
          stack[++top] = branch->right;
        }
      } else if (node->type == idMD6Node::NODE_LEAF_PLAY) {
        static_cast<idMD6LeafPlay*>(node)->startTime += timeOffset;
      }
    }
  }

  for (int index = 0; index < blendStack.num; ++index) {
    blendStack.list[index]->Unpause(this, curTime);
  }
}

void idAnimStack::AddAnimator(idAnimator_Base* animator) {
  if (GetAnimatorIndex(animator) >= 0) {
    return;
  }
  int index = 0;
  const int priority = animator->GetStackPriority();
  while (index < blendStack.num &&
         blendStack.list[index]->GetStackPriority() <= priority) {
    ++index;
  }
  if (index >= blendStack.num) {
    blendStack.Append(animator);
    flags.Append(0);
  } else {
    blendStack.Insert(animator, index);
    flags.Insert(0, index);
  }
}

bool idAnimStack::ShutdownBlendStackIndex(int index) {
  idAnimator_Base* animator = blendStack.list[index];
  if (animator == nullptr) {
    return false;
  }
  const bool serialized = animator->serializeProps.createdThroughSerialization;
  if (serialized) {
    animator->Shutdown(this);
    for (idMD6Branch*& serializedTree : animator->serializeProps.serializedTrees) {
      if (serializedTree != nullptr) {
        DetachSerializedTreeRoot(serializedTree);
        idAnimStack_Util::FreeTree_r(&md6Allocator, serializedTree);
        serializedTree = nullptr;
      }
    }
    delete animator;
  }
  blendStack.list[index] = nullptr;
  return serialized;
}

void idAnimStack::FreeSerializedAnimators() {
  if (!createdFromSnapshot) {
    return;
  }
  for (int index = 0; index < blendStack.num; ++index) {
    idAnimator_Base* animator = blendStack.list[index];
    for (idMD6Branch*& serializedTree : animator->serializeProps.serializedTrees) {
      if (serializedTree != nullptr) {
        DetachSerializedTreeRoot(serializedTree);
        idAnimStack_Util::FreeTree_r(&md6Allocator, serializedTree);
        serializedTree = nullptr;
      }
    }
    delete animator;
  }
  blendStack.ClearFree();
  flags.ClearFree();
}

void idAnimStack_Util::UpdateTree_r(const idMD6Skel* skeleton,
                                    int timeMSec, int ticksPerSec,
                                    idMD6Node* node) {
  while (node != nullptr) {
    switch (node->type) {
    case idMD6Node::NODE_FUSION_BRANCH:
      static_cast<idMD6FusionBranch*>(node)->Update(
          false, timeMSec, ticksPerSec, skeleton);
      break;
    case idMD6Node::NODE_BLEND_BRANCH:
      static_cast<idMD6BlendBranch*>(node)->Update(false, true);
      break;
    case idMD6Node::NODE_BLENDA_BRANCH:
      static_cast<idMD6BlendAdditiveBranch*>(node)->Update(false);
      break;
    case idMD6Node::NODE_BEST_LEAF:
      static_cast<idMD6BestLeaf*>(node)->Update();
      break;
    default:
      break;
    }
    if (!IsBranchType(node->type)) {
      return;
    }
    auto* branch = static_cast<idMD6Branch*>(node);
    if (branch->left != nullptr) {
      UpdateTree_r(skeleton, timeMSec, ticksPerSec, branch->left);
    }
    node = branch->right;
  }
}

void idAnimStack_Util::UpdateTree(const idMD6Skel* skeleton, int timeMSec,
                                  int ticksPerSec, idMD6Node* node) {
  UpdateTree_r(skeleton, timeMSec, ticksPerSec, node);
}

void idAnimStack_Util::FreeTree_r(idMD6Allocator* allocator,
                                  idMD6Node* node) {
  if (node == nullptr) {
    return;
  }
  if (IsBranchType(node->type)) {
    auto* branch = static_cast<idMD6Branch*>(node);
    FreeTree_r(allocator, branch->left);
    FreeTree_r(allocator, branch->right);
  }
  if (allocator != nullptr) {
    idMD6Allocator::FreeTyped(allocator, node);
  } else {
    idMem::Singleton()->Free(node, ALIGN_16);
  }
}

void idAnimStack_Util::SerializeJointMods(
    idSerializer& serializer, idMD6Blend::jointMod_t* jointMods,
    int numMods) {
  for (int index = 0; index < numMods; ++index) {
    idMD6Blend::jointMod_t& mod = jointMods[index];
    std::uint16_t joint = static_cast<std::uint16_t>(mod.joint.value);
    if (mod.joint.value == NULL_JOINT_INDEX.value) {
      joint = 0xFFFFu;
    }
    if (serializer.writing) {
      serializer.msg->WriteBits(joint, 16);
    } else {
      joint = static_cast<std::uint16_t>(serializer.msg->ReadBits(16));
    }
    mod.joint.value = joint == 0xFFFFu
                          ? NULL_JOINT_INDEX.value
                          : static_cast<std::int16_t>(joint);

    std::uint16_t flags = mod.flags;
    if (serializer.writing) {
      serializer.msg->WriteBits(flags, 16);
    } else {
      flags = static_cast<std::uint16_t>(serializer.msg->ReadBits(16));
    }
    mod.flags = flags;

    if ((flags & 4u) != 0u) {
      for (float& scale : mod.s) {
        if (serializer.writing) {
          serializer.msg->WriteBits(ClampByteFromFloat(scale), 8);
        } else {
          scale = static_cast<float>(serializer.msg->ReadBits(8));
        }
      }
    }
    if ((flags & 8u) != 0u) {
      SerializeFloatBits(serializer, mod.mat[3]);
      SerializeFloatBits(serializer, mod.mat[7]);
      SerializeFloatBits(serializer, mod.mat[11]);
    }
    if ((flags & 2u) != 0u) {
      idMat3 axis;
      if (serializer.writing) {
        axis[0] = idVec3{mod.mat[0], mod.mat[4], mod.mat[8]};
        axis[1] = idVec3{mod.mat[1], mod.mat[5], mod.mat[9]};
        axis[2] = idVec3{mod.mat[2], mod.mat[6], mod.mat[10]};
      }
      serializer.SerializeQ(&axis, 15);
      if (!serializer.writing) {
        mod.mat[0] = axis[0].x;
        mod.mat[1] = axis[1].x;
        mod.mat[2] = axis[2].x;
        mod.mat[4] = axis[0].y;
        mod.mat[5] = axis[1].y;
        mod.mat[6] = axis[2].y;
        mod.mat[8] = axis[0].z;
        mod.mat[9] = axis[1].z;
        mod.mat[10] = axis[2].z;
      }
    }
  }
}

void idAnimStack_Util::WriteTree_r(
    const idMD6Skel* skeleton, int timeMSec, int ticksPerSec,
    const idAnimStack& stack, idMD6Node* node, idBitMsg& typesMessage,
    idBitMsg& branchMessage, idBitMsg& leafMessage) {
  (void)stack;
  while (node != nullptr) {
    switch (node->type) {
    case idMD6Node::NODE_FUSION_BRANCH:
      static_cast<idMD6FusionBranch*>(node)->Update(
          false, timeMSec, ticksPerSec, skeleton);
      break;
    case idMD6Node::NODE_BLEND_BRANCH:
      static_cast<idMD6BlendBranch*>(node)->Update(false, true);
      break;
    case idMD6Node::NODE_BLENDA_BRANCH:
      static_cast<idMD6BlendAdditiveBranch*>(node)->Update(false);
      break;
    case idMD6Node::NODE_BEST_LEAF:
      static_cast<idMD6BestLeaf*>(node)->Update();
      break;
    default:
      break;
    }

    if (!IsBranchType(node->type)) {
      break;
    }
    auto* branch = static_cast<idMD6Branch*>(node);
    typesMessage.WriteBits(0, 4);
    branchMessage.WriteBits(branch->filterGroup, 3);
    branchMessage.WriteBits(branch->op, 4);
    branchMessage.WriteBits(static_cast<int>(branch->currentAlpha * 255.0f),
                            8);
    branchMessage.WriteBits(static_cast<int>(branch->targetAlpha * 255.0f),
                            8);
    branchMessage.WriteBits(static_cast<int>(branch->alphaRate * 255.0f),
                            16);
    branchMessage.WriteBits(branch->originBlend, 8);
    const bool hasLeft = branch->left != nullptr;
    branchMessage.WriteBits(hasLeft, 1);
    if (hasLeft) {
      WriteTree_r(skeleton, timeMSec, ticksPerSec, stack, branch->left,
                  typesMessage, branchMessage, leafMessage);
    }
    node = branch->right;
  }

  node = SelectBestLeaf(node);
  if (node == nullptr ||
      (node->type != idMD6Node::NODE_LEAF_PAUSE &&
       node->type != idMD6Node::NODE_LEAF_PLAY)) {
    return;
  }
  auto* leaf = static_cast<idMD6Leaf*>(node);
  typesMessage.WriteBits(node->type, 4);
  leafMessage.WriteBits(leaf->weightGroup, 3);
  leafMessage.WriteBits(leaf->wrapMode, 1);
  reinterpret_cast<idResourceList*>(&idMD6Anim::resourceList)
      ->WriteResourceToMsg(reinterpret_cast<const idResource*>(leaf->anim),
                           leafMessage);
  if (node->type == idMD6Node::NODE_LEAF_PLAY) {
    auto* play = static_cast<idMD6LeafPlay*>(node);
    leafMessage.WriteBits(play->startTime, 32);
    leafMessage.WriteBits(
        static_cast<int>(play->GetRateScale() * 255.0f), 16);
  } else {
    auto* pause = static_cast<idMD6LeafPause*>(node);
    int frameBits = 0;
    std::memcpy(&frameBits, &pause->frame, sizeof(frameBits));
    leafMessage.WriteBits(frameBits, 32);
    leafMessage.WriteBits(pause->animMods[0].num, 8);
    leafMessage.WriteBits(pause->flags, 8);
    idSerializer serializer{true, {}, &leafMessage};
    SerializeJointMods(serializer,
                       pause->animMods[pause->currentDeferred & 1].list,
                       pause->animMods[0].num);
  }
}

idMD6Node* idAnimStack_Util::ReadTree_r(
    const idAnimStack& stack, idMD6Allocator* allocator,
    idBitMsg& typesMessage, idBitMsg& branchMessage,
    idBitMsg& leafMessage) {
  (void)stack;
  const int type = typesMessage.ReadBits(4);
  if (type == idMD6Node::NODE_LEAF_PAUSE) {
    auto* pause = allocator != nullptr
                      ? static_cast<idMD6LeafPause*>(
                            allocator->Alloc(idMD6Node::NODE_LEAF_PAUSE))
                      : new idMD6LeafPause();
    pause->weightGroup =
        static_cast<std::uint8_t>(leafMessage.ReadBits(3));
    pause->wrapMode = static_cast<std::uint8_t>(leafMessage.ReadBits(1));
    pause->anim = reinterpret_cast<const idMD6Anim*>(
        reinterpret_cast<idResourceList*>(&idMD6Anim::resourceList)
            ->ReadResourceFromMsg(leafMessage));
    const int frameBits = leafMessage.ReadBits(32);
    std::memcpy(&pause->frame, &frameBits, sizeof(frameBits));
    const int numMods = leafMessage.ReadBits(8);
    pause->SetNumAnimMods(numMods);
    pause->flags = static_cast<std::int16_t>(leafMessage.ReadBits(8));
    idSerializer serializer{false, {}, &leafMessage};
    SerializeJointMods(serializer,
                       pause->animMods[pause->currentDeferred & 1].list,
                       pause->animMods[0].num);
    return pause;
  }
  if (type == idMD6Node::NODE_LEAF_PLAY) {
    auto* play = allocator != nullptr
                     ? static_cast<idMD6LeafPlay*>(
                           allocator->Alloc(idMD6Node::NODE_LEAF_PLAY))
                     : new idMD6LeafPlay();
    play->weightGroup =
        static_cast<std::uint8_t>(leafMessage.ReadBits(3));
    play->wrapMode = static_cast<std::uint8_t>(leafMessage.ReadBits(1));
    play->anim = reinterpret_cast<const idMD6Anim*>(
        reinterpret_cast<idResourceList*>(&idMD6Anim::resourceList)
            ->ReadResourceFromMsg(leafMessage));
    play->startTime = leafMessage.ReadBits(32);
    const std::uint16_t rate =
        static_cast<std::uint16_t>(leafMessage.ReadBits(16));
    play->SetRateScale(play->startTime,
                       static_cast<float>(rate) / 255.0f);
    return play;
  }
  if (type != idMD6Node::NODE_BRANCH &&
      (type <= idMD6Node::NODE_LEAF_PLAY ||
       type > idMD6Node::NODE_FUSION_BRANCH)) {
    return nullptr;
  }

  auto* branch = allocator != nullptr
                     ? static_cast<idMD6Branch*>(
                           allocator->Alloc(idMD6Node::NODE_BRANCH))
                     : new idMD6Branch();
  branch->filterGroup =
      static_cast<std::uint8_t>(branchMessage.ReadBits(3));
  branch->op = static_cast<std::uint8_t>(branchMessage.ReadBits(4));
  branch->currentAlpha =
      static_cast<float>(static_cast<std::uint8_t>(
          branchMessage.ReadBits(8))) /
      255.0f;
  branch->targetAlpha =
      static_cast<float>(static_cast<std::uint8_t>(
          branchMessage.ReadBits(8))) /
      255.0f;
  branch->alphaRate =
      static_cast<float>(static_cast<std::uint16_t>(
          branchMessage.ReadBits(16))) /
      255.0f;
  branch->originBlend =
      static_cast<std::uint8_t>(branchMessage.ReadBits(8));
  branch->left = branchMessage.ReadBits(1) != 0
                     ? ReadTree_r(stack, allocator, typesMessage,
                                  branchMessage, leafMessage)
                     : nullptr;
  branch->right = ReadTree_r(stack, allocator, typesMessage, branchMessage,
                             leafMessage);
  return branch;
}

void idAnimStack::AddNodeToSyncGroups(idList<syncGroup_t, 5>& groups,
                                      idMD6LeafPlay* node, float alpha) {
  syncGroup_t* group = nullptr;
  for (int index = 0; index < groups.num; ++index) {
    if (groups.list[index].groupNumber == node->syncGroup) {
      group = &groups.list[index];
      break;
    }
  }
  if (group == nullptr) {
    if (groups.IsFull()) {
      idLib::Warning("Sync Groups Full(%d)", groups.size);
      return;
    }
    group = groups.Alloc();
    group->groupNumber = node->syncGroup;
  } else if (group->nodes.IsFull()) {
    idLib::Warning("Sync Groups Nodes Full(%d)", group->nodes.size);
    return;
  }
  syncNode_t* syncNode = group->nodes.Alloc();
  syncNode->node = node;
  syncNode->alpha = alpha;
}

void idAnimStack::PreSynchronizeNode_r(idList<syncGroup_t, 5>& groups,
                                       idMD6Node* node,
                                       float branchAlpha) {
  while (node != nullptr) {
    if (!IsBranchType(node->type)) {
      break;
    }
    auto* branch = static_cast<idMD6Branch*>(node);
    float leftAlpha = 0.0f;
    float rightAlpha = 0.0f;
    if (branch->op == idMD6Blend::BOP_REF_LERP) {
      leftAlpha = branchAlpha;
      rightAlpha = branchAlpha;
    } else if (branch->op == idMD6Blend::BOP_LERP ||
               (branch->op > idMD6Blend::BOP_SUB_RIGHT &&
                branch->op <= idMD6Blend::BOP_BLENDA)) {
      rightAlpha = branch->currentAlpha * branchAlpha;
      leftAlpha = (1.0f - branch->currentAlpha) * branchAlpha;
    }

    idMD6Node* right = SelectBestLeaf(branch->right);
    if (right != nullptr) {
      if (right->type == idMD6Node::NODE_LEAF_PLAY) {
        auto* play = static_cast<idMD6LeafPlay*>(right);
        if (play->syncEnabled != 0 && play->syncGroup != 0) {
          AddNodeToSyncGroups(groups, play, rightAlpha);
        }
      } else if (IsBranchType(right->type)) {
        PreSynchronizeNode_r(groups, right, rightAlpha);
      }
    }
    node = branch->left;
    branchAlpha = leftAlpha;
  }

  node = SelectBestLeaf(node);
  if (node != nullptr && node->type == idMD6Node::NODE_LEAF_PLAY) {
    auto* play = static_cast<idMD6LeafPlay*>(node);
    if (play->syncEnabled != 0 && play->syncGroup != 0) {
      AddNodeToSyncGroups(groups, play, branchAlpha);
    }
  } else if (node != nullptr && IsBranchType(node->type)) {
    PreSynchronizeNode_r(groups, node, branchAlpha);
  }
}

void idAnimStack::UpdateSynchronization(int currentTime, int previousTime,
                                        int, int ticksPerSec) {
  idStaticList<syncGroup_t, 4> groups;
  groups.memTag = 5;
  PreSynchronizeNode_r(groups, tree, 1.0f);
  const float elapsedSeconds =
      static_cast<float>(currentTime - previousTime) * 0.001f;
  for (int groupIndex = 0; groupIndex < groups.num; ++groupIndex) {
    syncGroup_t& group = groups.list[groupIndex];
    float weightedRate = 0.0f;
    float totalAlpha = 0.0f;
    syncNode_t* dominant = group.nodes.list;
    for (int nodeIndex = 0; nodeIndex < group.nodes.num; ++nodeIndex) {
      syncNode_t& syncNode = group.nodes.list[nodeIndex];
      if (syncNode.alpha != 0.0f &&
          (syncNode.node->anim->animData->flags & 1) == 0) {
        totalAlpha += syncNode.alpha;
        const int animationLength =
            syncNode.node->GetAnimLength(ticksPerSec, false);
        weightedRate +=
            (syncNode.node->GetRateScale() /
             (static_cast<float>(animationLength) * 0.001f)) *
            syncNode.alpha;
        if (syncNode.alpha > dominant->alpha) {
          dominant = &syncNode;
        }
      }
    }
    group.syncRate = weightedRate / totalAlpha;
    float fraction =
        static_cast<float>(previousTime - dominant->node->startTime) /
        static_cast<float>(dominant->node->GetAnimLength(ticksPerSec,
                                                         false));
    if (fraction < 0.0f) {
      fraction = 0.0f;
    }
    fraction += elapsedSeconds * group.syncRate;
    if (fraction > 1.0f) {
      fraction -= std::floor(fraction);
    }
    for (int nodeIndex = 0; nodeIndex < group.nodes.num; ++nodeIndex) {
      idMD6LeafPlay* play = group.nodes.list[nodeIndex].node;
      const int length = play->GetAnimLength(ticksPerSec, false);
      play->SetFrameTime(currentTime,
                         static_cast<int>(static_cast<float>(length) *
                                          fraction));
    }
  }
}

void idAnimStack::SerializeSnapshot(idSerializer& serializer, idClip* clip,
                                    int curTime,
                                    idGameTimeManager* gameTimeManager,
                                    idTreeAnimator* snapshotTreeAnimator) {
  if (skipSerialization || disableSerialization) {
    return;
  }

  bool noJointAnimator = jointAnimator == nullptr;
  if (serializer.writing) {
    serializer.msg->WriteBits(noJointAnimator, 1);
  } else {
    noJointAnimator = serializer.msg->ReadBits(1) == 1;
  }
  const bool localNoJointAnimator = jointAnimator == nullptr;
  if (localNoJointAnimator != noJointAnimator) {
    idLib::Warning(
        "Client/Server mismatch on noTreeAnimator. Server: %d Client: %d",
        noJointAnimator, localNoJointAnimator);
    return;
  }
  if (noJointAnimator || jointAnimator == nullptr) {
    return;
  }

  if (serializer.writing) {
    serializer.msg->WriteData(typesMemory, sizeof(typesMemory));
    serializer.msg->WriteData(branchMemory, sizeof(branchMemory));
    serializer.msg->WriteData(leafMemory, sizeof(leafMemory));
  } else {
    serializer.msg->ReadData(typesMemory, sizeof(typesMemory));
    serializer.msg->ReadData(branchMemory, sizeof(branchMemory));
    serializer.msg->ReadData(leafMemory, sizeof(leafMemory));
  }

  int animatorCount = blendStack.num;
  if (serializer.writing) {
    serializer.msg->WriteBits(animatorCount, 32);
  } else {
    animatorCount = serializer.msg->ReadBits(32);
  }
  blendStack.SetNum(animatorCount, nullptr);
  flags.SetNum(animatorCount, 0);

  for (int index = 0; index < animatorCount; ++index) {
    int serializeType = STYPE_GENERIC;
    if (serializer.writing) {
      serializeType = blendStack.list[index]->GetSerializeType();
    }
    if (serializer.writing) {
      serializer.msg->WriteBits(serializeType, 3);
    } else {
      serializeType = serializer.msg->ReadBits(3);
      idAnimator_Base* existing = blendStack.list[index];
      if (existing != nullptr &&
          existing->GetSerializeType() != serializeType) {
        ShutdownBlendStackIndex(index);
        blendStack.list[index] = nullptr;
      }
    }

    if (AssertFailed(
            "w:\\build\\tech5\\engine\\gamelib\\animstack\\AnimStack.cpp",
            492, "0", true)) {
      __debugbreak();
    }

    if (blendStack.list[index] == nullptr) {
      createdFromSnapshot = true;
      switch (serializeType) {
      case STYPE_AF:
        blendStack.list[index] = new idAnimator_AF();
        break;
      case STYPE_ANIMWEB_HANDS:
        blendStack.list[index] = new idAnimator_AnimWebHands();
        break;
      case STYPE_WALK_IK:
        blendStack.list[index] = new idAnimator_WalkIK();
        break;
      case STYPE_TORSO_TRACKER:
        blendStack.list[index] = new idAnimator_TorsoTracker();
        break;
      case STYPE_PAIN:
        blendStack.list[index] = new idAnimator_Pain();
        break;
      case STYPE_JOINTMOD:
        blendStack.list[index] = new idAnimator_JointMod();
        break;
      case STYPE_REACH_IK:
        blendStack.list[index] = new idAnimator_ReachIK();
        break;
      default:
        blendStack.list[index] = new idAnimator_Client();
        break;
      }
      idAnimator_Base* created = blendStack.list[index];
      created->serializeProps.animStack = this;
      created->serializeProps.clip = clip;
      created->serializeProps.treeAnimator = snapshotTreeAnimator;
      created->PreSerializeInit(gameTimeManager);
    } else {
      idAnimator_Base* existing = blendStack.list[index];
      existing->serializeProps.animStack = this;
      existing->serializeProps.clip = clip;
      existing->serializeProps.treeAnimator = snapshotTreeAnimator;
    }

    std::uint8_t serializedFlags = flags.list[index];
    if (serializer.writing) {
      serializer.msg->WriteBits(serializedFlags, 8);
    } else {
      serializedFlags =
          static_cast<std::uint8_t>(serializer.msg->ReadBits(8));
    }
    flags.list[index] = static_cast<std::uint8_t>(
        serializedFlags | (flags.list[index] & 0x30u));
  }

  for (int index = 0; index < blendStack.num; ++index) {
    bool enabled = (flags.list[index] & ANIMATORFLAG_ENABLED) != 0;
    if (serializer.writing) {
      serializer.msg->WriteBits(enabled, 1);
    } else {
      enabled = serializer.msg->ReadBits(1) == 1;
    }
    if (enabled) {
      flags.list[index] |= ANIMATORFLAG_ENABLED;
    }
    if ((flags.list[index] & ANIMATORFLAG_ENABLED) != 0 &&
        (flags.list[index] & ANIMATORFLAG_SERIALIZE) != 0 &&
        blendStack.list[index] != nullptr) {
      blendStack.list[index]->SerializeSnapshot(serializer);
    }
  }

  if (serializer.writing) {
    serializer.msg->WriteBits(pauseTime.value, 32);
  } else {
    const int serializedPauseTime = serializer.msg->ReadBits(32);
    if (serializedPauseTime != pauseTime.value) {
      const idTypesafeNumber<int, gameTimeUnique_t> gameTime(
          gameTimeManager->GetGameMs(GAMETIME_SCALED));
      if (serializedPauseTime >= 0) {
        Pause(gameTime);
      } else {
        Unpause(gameTime);
      }
    }
  }
  prevSerializedTime = curSerializedTime;
  curSerializedTime = curTime;
  serializedNewTree = true;
}

void idAnimStack::BlendSnapshot(int curTime, int gameMsPerFrame,
                                int ticksPerSec, float fraction,
                                idGameTimeManager* game,
                                idParallelJobList* parallelJobList) {
  if (disableSerialization) {
    md6Allocator.DeferredFree();
    return;
  }
  if (jointAnimator == nullptr) {
    idLib::Warning("idAnimStack::BlendSnapshot called with NULL tree animator",
                   curTime);
    md6Allocator.DeferredFree();
    return;
  }

  blendedFromSnapshot = true;
  tree = nullptr;
  usedSnapshotNodes.num = 0;
  float blendFraction = 0.0f;
  if (fraction >= 0.0f) {
    blendFraction = fraction <= 1.0f ? fraction : 1.0f;
  }

  int gameTime = curTime;
  idAnimRecorder* recorder = idAnimRecorder::instance;
  if (recorder == nullptr) {
    recorder = idAnimRecorder::allocFunction();
    idAnimRecorder::instance = recorder;
  }
  if (recorder->activeAnimStack == this &&
      recorder->recordingFileMode == idAnimRecorder::RECORDING_FILE_READ) {
    recorder->Serialize(gameTime, *game, false);
  }

  idBitMsg typesMessage;
  typesMessage.writeData = nullptr;
  typesMessage.readData = typesMemory;
  typesMessage.maxSize = sizeof(typesMemory);
  typesMessage.curSize = sizeof(typesMemory);
  typesMessage.writeBit = 0;
  typesMessage.readCount = 0;
  typesMessage.readBit = 0;
  typesMessage.allowOverflow = false;
  typesMessage.overflowed = false;
  typesMessage.tempValue = 0;

  idBitMsg branchMessage;
  branchMessage.writeData = nullptr;
  branchMessage.readData = branchMemory;
  branchMessage.maxSize = sizeof(branchMemory);
  branchMessage.curSize = sizeof(branchMemory);
  branchMessage.writeBit = 0;
  branchMessage.readCount = 0;
  branchMessage.readBit = 0;
  branchMessage.allowOverflow = false;
  branchMessage.overflowed = false;
  branchMessage.tempValue = 0;

  idBitMsg leafMessage;
  leafMessage.writeData = nullptr;
  leafMessage.readData = leafMemory;
  leafMessage.maxSize = sizeof(leafMemory);
  leafMessage.curSize = sizeof(leafMemory);
  leafMessage.writeBit = 0;
  leafMessage.readCount = 0;
  leafMessage.readBit = 0;
  leafMessage.allowOverflow = false;
  leafMessage.overflowed = false;
  leafMessage.tempValue = 0;

  idMD6Branch* previousBranch = nullptr;
  for (int index = 0; index < blendStack.num; ++index) {
    idAnimator_Base* animator = blendStack.list[index];
    const std::uint8_t animatorFlags = flags.list[index];
    if ((animatorFlags & ANIMATORFLAG_CLIENT_AUTHORITATIVE_ENABLED) == 0 &&
        (animatorFlags & ANIMATORFLAG_ENABLED) == 0) {
      continue;
    }

    idMD6Branch* mergeBranch = nullptr;
    if ((animatorFlags & ANIMATORFLAG_SERIALIZE) == 0) {
      mergeBranch = animator->InternalGetMergeBranch();
      if (mergeBranch != nullptr) {
        mergeBranch->left = nullptr;
      }
      animator->InternalPreBlendTree(this, gameTime, ticksPerSec);
      if (!animator->IsContributing()) {
        continue;
      }
      mergeBranch = animator->InternalGetMergeBranch();
      usedSnapshotNodes.Append(mergeBranch->right);
    } else {
      animator->PreBlendSnapshot(this, gameTime, ticksPerSec,
                                 blendFraction);
      const std::uint8_t updatedFlags = flags.list[index];
      if ((updatedFlags & ANIMATORFLAG_CLIENT_AUTHORITATIVE_CONTRIBUTED) ==
              0 &&
          (updatedFlags & ANIMATORFLAG_CONTRIBUTED_LAST_SERIALIZE) == 0) {
        for (idMD6Branch*& serializedTree :
             animator->serializeProps.serializedTrees) {
          if (serializedTree != nullptr) {
            DetachSerializedTreeRoot(serializedTree);
            idAnimStack_Util::FreeTree_r(&md6Allocator, serializedTree);
            serializedTree = nullptr;
          }
        }
        continue;
      }

      mergeBranch = animator->InternalGetMergeBranch();
      if (animator->GetSerializeType() == STYPE_GENERIC) {
        if (serializedNewTree) {
          if (anim_blendSnapshotDebug.valueInteger != 0) {
            idLib::Printf("^6Serialized new tree %s\n",
                          jointAnimator->decl->name.str);
          }
          idMD6Branch*& oldTree = animator->serializeProps.serializedTrees[0];
          if (oldTree != nullptr) {
            DetachSerializedTreeRoot(oldTree);
            idAnimStack_Util::FreeTree_r(&md6Allocator, oldTree);
            oldTree = nullptr;
          }
          oldTree = animator->serializeProps.serializedTrees[1];
          animator->serializeProps.serializedTrees[1] =
              static_cast<idMD6Branch*>(idAnimStack_Util::ReadTree_r(
                  *this, &md6Allocator, typesMessage, branchMessage,
                  leafMessage));
        }
        mergeBranch = animator->serializeProps.serializedTrees[1];
      }

      if (mergeBranch == nullptr || mergeBranch->right == nullptr) {
        continue;
      }
      usedSnapshotNodes.Append(mergeBranch->right);

      idMD6Branch* oldTree = animator->serializeProps.serializedTrees[0];
      if (oldTree != nullptr && oldTree->right != nullptr &&
          anim_blendSnapshotNoLerp.valueInteger == 0) {
        auto* blend = static_cast<idMD6Branch*>(
            md6Allocator.Alloc(idMD6Node::NODE_BRANCH));
        rememberedNodes.Append(blend);
        blend->left = oldTree->right;
        blend->right = mergeBranch->right;
        blend->filterGroup = MD6_WEIGHTGROUP_ALL;
        blend->op = idMD6Blend::BOP_LERP;
        blend->originBlend = idMD6Blend::ORIGINBLEND_BRANCH;
        blend->currentAlpha = blendFraction;
        blend->targetAlpha = 1.0f;
        blend->alphaRate = 0.0f;

        auto* wrapper = static_cast<idMD6Branch*>(
            md6Allocator.Alloc(idMD6Node::NODE_BRANCH));
        rememberedNodes.Append(wrapper);
        wrapper->left = nullptr;
        wrapper->right = blend;
        wrapper->filterGroup = mergeBranch->filterGroup;
        wrapper->op = mergeBranch->op;
        wrapper->originBlend = mergeBranch->originBlend;
        wrapper->currentAlpha =
            ((mergeBranch->currentAlpha - oldTree->currentAlpha) *
             blendFraction) +
            oldTree->currentAlpha;
        wrapper->targetAlpha = mergeBranch->targetAlpha;
        wrapper->alphaRate = mergeBranch->alphaRate;
        mergeBranch = wrapper;
      }
    }

    if (mergeBranch == nullptr) {
      idLib::Warning(
          "curBranch == NULL for animator md6Def '%s' in animstack: %s",
          "<unknown>", jointAnimator->decl->name.str);
      continue;
    }
    if (previousBranch != nullptr) {
      mergeBranch->left = previousBranch->left != nullptr
                              ? static_cast<idMD6Node*>(previousBranch)
                              : previousBranch->right;
      tree = mergeBranch;
    } else {
      tree = mergeBranch->right;
      if (mergeBranch->alphaRate != 0.0f) {
        mergeBranch->UpdateCurrentAlpha(gameTime,
                                        gameTime - gameMsPerFrame,
                                        ticksPerSec);
      }
    }
    previousBranch = mergeBranch;
  }

  if (jointAnimator != nullptr) {
    jointAnimator->BlendTreeInternal(
        gameTime, gameTime - gameMsPerFrame, gameMsPerFrame, ticksPerSec,
        static_cast<idMD6FusionBranch*>(tree), parallelJobList, nullptr,
        nullptr, nullptr, nullptr);
  }
  for (int index = 0; index < blendStack.num; ++index) {
    if ((flags.list[index] & ANIMATORFLAG_CONTRIBUTED_LAST_SERIALIZE) != 0) {
      blendStack.list[index]->InternalPostBlendTree(this, gameTime);
    }
  }
  for (int index = 0; index < rememberedNodes.num; ++index) {
    idMD6Allocator::FreeTyped(&md6Allocator, rememberedNodes.list[index]);
  }
  rememberedNodes.num = 0;
  if (jointAnimator != nullptr) {
    treeAnimator->UpdateTime(gameTime);
  }
  serializedNewTree = false;
  md6Allocator.DeferredFree();
}

void idAnimStack::BlendStackInternal(
    int currentTime, int previousTime, int gameMsPerFrame, int ticksPerSec,
    idGameTimeManager* game, idParallelJobList* parallelJobList,
    float* localR, float* localS, float* localT, float* localU) {
  idAnimRecorder* recorder = idAnimRecorder::instance;
  if (recorder == nullptr) {
    recorder = idAnimRecorder::allocFunction();
    idAnimRecorder::instance = recorder;
  }
  if (recorder->activeAnimStack == this &&
      recorder->recordingFileMode == idAnimRecorder::RECORDING_FILE_READ) {
    BlendSnapshot(currentTime, gameMsPerFrame, ticksPerSec, 0.0f, game,
                  parallelJobList);
    return;
  }

  if (pauseTime.value < 0) {
    const bool recording =
        recorder->activeAnimStack == this &&
        recorder->recordingFileMode == idAnimRecorder::RECORDING_FILE_WRITE;
    if (jointAnimator == nullptr) {
      md6Allocator.DeferredFree();
      return;
    }

    idBitMsg typesMessage;
    typesMessage.InitWrite(typesMemory, sizeof(typesMemory));
    idBitMsg branchMessage;
    branchMessage.InitWrite(branchMemory, sizeof(branchMemory));
    idBitMsg leafMessage;
    leafMessage.InitWrite(leafMemory, sizeof(leafMemory));
    typesMessage.allowOverflow = true;
    branchMessage.allowOverflow = true;
    leafMessage.allowOverflow = true;

    tree = nullptr;
    idMD6Branch* previousBranch = nullptr;
    bool treeOverflowed = false;
    for (int index = 0; index < blendStack.num; ++index) {
      const std::uint8_t initialFlags = flags.list[index];
      idAnimator_Base* animator = blendStack.list[index];
      if ((initialFlags & ANIMATORFLAG_ENABLED) == 0 ||
          (initialFlags & ANIMATORFLAG_INITIALIZED) == 0) {
        flags.list[index] &=
            static_cast<std::uint8_t>(~ANIMATORFLAG_CONTRIBUTED_LAST_SERIALIZE);
        continue;
      }

      animator->PreBlendTree(this, currentTime, ticksPerSec);
      flags.list[index] &=
          static_cast<std::uint8_t>(~ANIMATORFLAG_CONTRIBUTED_LAST_SERIALIZE);
      if (!animator->IsContributing()) {
        continue;
      }

      idMD6Branch* mergeBranch = animator->InternalGetMergeBranch();
      if ((initialFlags & ANIMATORFLAG_SERIALIZE) != 0 &&
          (recording || (common->IsServer() && !disableSerialization)) &&
          !treeOverflowed) {
        flags.list[index] |= ANIMATORFLAG_CONTRIBUTED_LAST_SERIALIZE;
        if (animator->GetSerializeType() == STYPE_GENERIC) {
          mergeBranch->left = nullptr;
          idAnimStack_Util::WriteTree_r(
              jointAnimator->decl->model->skeleton, currentTime,
              ticksPerSec, *this, mergeBranch, typesMessage, branchMessage,
              leafMessage);
          if (typesMessage.overflowed || branchMessage.overflowed ||
              leafMessage.overflowed) {
            idLib::Warning(
                "idAnimStack::BlendStack: Tree too large to serialize.");
            treeOverflowed = true;
          }
        } else {
          idAnimStack_Util::UpdateTree_r(
              jointAnimator->decl->model->skeleton, currentTime,
              ticksPerSec, mergeBranch);
        }
      } else {
        idAnimStack_Util::UpdateTree_r(
            jointAnimator->decl->model->skeleton, currentTime, ticksPerSec,
            mergeBranch);
      }

      idMD6Branch* endBranch = animator->InternalGetEndBranch();
      if (previousBranch != nullptr) {
        if (endBranch != nullptr) {
          endBranch->left = previousBranch->left != nullptr
                                ? static_cast<idMD6Node*>(previousBranch)
                                : previousBranch->right;
          tree = mergeBranch->right;
        } else {
          mergeBranch->left = previousBranch->left != nullptr
                                  ? static_cast<idMD6Node*>(previousBranch)
                                  : previousBranch->right;
          tree = mergeBranch;
        }
        previousBranch = mergeBranch;
      } else if (endBranch != nullptr) {
        tree = nullptr;
      } else {
        tree = mergeBranch->right;
        if (mergeBranch->alphaRate != 0.0f) {
          mergeBranch->UpdateCurrentAlpha(currentTime, previousTime,
                                          ticksPerSec);
        }
        previousBranch = mergeBranch;
      }
    }

    if (treeOverflowed) {
      for (int index = 0; index < blendStack.num; ++index) {
        flags.list[index] &= static_cast<std::uint8_t>(
            ~ANIMATORFLAG_CONTRIBUTED_LAST_SERIALIZE);
      }
      std::memset(typesMemory, 0, sizeof(typesMemory));
      std::memset(branchMemory, 0, sizeof(branchMemory));
      std::memset(leafMemory, 0, sizeof(leafMemory));
    }

    if (tree != nullptr) {
      UpdateSynchronization(currentTime, previousTime, gameMsPerFrame,
                            ticksPerSec);
    }
    if (recording) {
      int gameTime = currentTime;
      recorder->Serialize(gameTime, *game, true);
    }
    if (localR != nullptr) {
      jointAnimator->BlendTreeInternal(
          currentTime, previousTime, gameMsPerFrame, ticksPerSec,
          static_cast<idMD6FusionBranch*>(tree), nullptr, localR, localS,
          localT, localU);
    } else {
      jointAnimator->BlendTreeInternal(
          currentTime, previousTime, gameMsPerFrame, ticksPerSec,
          static_cast<idMD6FusionBranch*>(tree), parallelJobList, nullptr,
          nullptr, nullptr, nullptr);
    }
    for (int index = 0; index < blendStack.num; ++index) {
      const std::uint8_t animatorFlags = flags.list[index];
      if ((animatorFlags & ANIMATORFLAG_ENABLED) != 0 &&
          (animatorFlags & ANIMATORFLAG_INITIALIZED) != 0) {
        blendStack.list[index]->InternalPostBlendTree(this, currentTime);
      }
    }
  }

  if (treeAnimator != nullptr) {
    treeAnimator->UpdateTime(currentTime);
  }
  md6Allocator.DeferredFree();
}
