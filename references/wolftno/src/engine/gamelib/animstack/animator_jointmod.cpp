#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_TRIVIAL_IDINDEX_ABI
#include "engine/gamelib/animstack/animator_jointmod.h"

#include "engine/gamelib/animstack/animstack.h"

#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_APPEND_ABI
#undef WOLFTNO_RETAIL_TRIVIAL_IDINDEX_ABI

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <limits>

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

extern bool forceIdListsToReallocateEveryAppend;

namespace {

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

__forceinline void BuildJointMatrix(
    idMat3x4& matrix,
    const idAnimator_JointMod::animatorJointMod_t& jointMod) {
  matrix.mat[0] = jointMod.axis.mat[0].x;
  matrix.mat[1] = jointMod.axis.mat[1].x;
  matrix.mat[2] = jointMod.axis.mat[2].x;
  matrix.mat[3] = jointMod.pos.x;
  matrix.mat[4] = jointMod.axis.mat[0].y;
  matrix.mat[5] = jointMod.axis.mat[1].y;
  matrix.mat[6] = jointMod.axis.mat[2].y;
  matrix.mat[7] = jointMod.pos.y;
  matrix.mat[8] = jointMod.axis.mat[0].z;
  matrix.mat[9] = jointMod.axis.mat[1].z;
  matrix.mat[10] = jointMod.axis.mat[2].z;
  matrix.mat[11] = jointMod.pos.z;
}

__forceinline void ApplyTransformFlags(
    idMD6Blend::jointMod_t& output,
    const idAnimator_JointMod::animatorJointMod_t& jointMod) {
  if (jointMod.type == JOINTMOD_MODEL ||
      jointMod.type == JOINTMOD_MODEL_OVERRIDE) {
    output.flags = static_cast<std::uint16_t>(output.flags | 0x0001u);
  }
  if (jointMod.type == JOINTMOD_MODEL_OVERRIDE ||
      jointMod.type == JOINTMOD_LOCAL_OVERRIDE) {
    output.flags = static_cast<std::uint16_t>(output.flags | 0x0020u);
  }
  if (jointMod.pose == ANIMATION_POSE_CURRENT) {
    output.flags = static_cast<std::uint16_t>(output.flags | 0x4000u);
  }
}

} // namespace

bool idAnimator_JointMod::InternalInit(const idAnimatorParms_Base& parms) {
  return parms.animStack->jointAnimator != nullptr;
}

void idAnimator_JointMod::InternalPreBlendTree(const idAnimStack* stack, int,
                                               int) {
  idJointAnimator* const jointAnimator =
      stack != nullptr ? stack->jointAnimator : nullptr;
  if (!enabled || jointAnimator == nullptr || jointMods.num <= 0) {
    return;
  }

  for (int index = 0; index < jointMods.num; ++index) {
    animatorJointMod_t& jointMod = jointMods.list[index];
    switch (jointMod.driver) {
      case 1:
        jointAnimator->SetJointAxis(jointMod.pose, jointMod.jointIndex,
                                    jointMod.type, jointMod.axis);
        break;
      case 2:
        jointAnimator->SetJointPos(jointMod.pose, jointMod.jointIndex,
                                   jointMod.type, jointMod.pos);
        break;
      case 4:
        jointAnimator->SetJointScale(jointMod.pose, jointMod.jointIndex,
                                     jointMod.type, jointMod.scale);
        break;
      case 3: {
        idMat3x4 matrix;
        BuildJointMatrix(matrix, jointMod);
        jointAnimator->SetJointMat3x4(jointMod.pose, jointMod.jointIndex,
                                      jointMod.type, matrix, true);
        break;
      }
      case 7:
        jointAnimator->SetJointPosAxisScale(
            jointMod.pose, jointMod.jointIndex, jointMod.type, jointMod.pos,
            jointMod.axis, jointMod.scale);
        break;
      default:
        break;
    }
  }
}

void idAnimator_JointMod::SetJointPos(int modIndex, const idVec3& value) {
  jointMods.list[modIndex].pos = value;
}

void idAnimator_JointMod::SetJointAxis(int modIndex, const idMat3& value) {
  jointMods.list[modIndex].axis = value;
}

void idAnimator_JointMod::PreSerializeInit(
    idGameTimeManager* gameTimeManager) {
  gametimeManager.gameTimeManager = gameTimeManager;
  serializeProps.createdThroughSerialization = true;
  const idAnimatorParms_Base parms(
      serializeProps.animStack, "jointmod", idMD6Blend::BOP_LERP,
      idMD6Blend::ORIGINBLEND_BRANCH, MD6_WEIGHTGROUP_ALL,
      MD6_WEIGHTGROUP_ALL, 1.0f);
  Init(gametimeManager.gameTimeManager, parms);
}

int idAnimator_JointMod::GenerateBlendJointMods(
    idMD6Blend::jointMod_t* outMods, int maxOutJointMods) {
  if (outMods == nullptr || maxOutJointMods < jointMods.num) {
    return 0;
  }

  for (int index = 0; index < jointMods.num; ++index) {
    const animatorJointMod_t& jointMod = jointMods.list[index];
    idMD6Blend::jointMod_t& output = outMods[index];
    output.flags = 0;
    output.joint = jointMod.jointIndex;

    switch (jointMod.driver) {
      case 1:
        output.flags = 0x8002u;
        output.mat[0] = jointMod.axis.mat[0].x;
        output.mat[1] = jointMod.axis.mat[1].x;
        output.mat[2] = jointMod.axis.mat[2].x;
        output.mat[4] = jointMod.axis.mat[0].y;
        output.mat[5] = jointMod.axis.mat[1].y;
        output.mat[6] = jointMod.axis.mat[2].y;
        output.mat[8] = jointMod.axis.mat[0].z;
        output.mat[9] = jointMod.axis.mat[1].z;
        output.mat[10] = jointMod.axis.mat[2].z;
        break;
      case 2:
        output.flags = 0x8008u;
        output.mat[3] = jointMod.pos.x;
        output.mat[7] = jointMod.pos.y;
        output.mat[11] = jointMod.pos.z;
        break;
      case 4:
        output.flags = 0x8004u;
        output.s[0] = jointMod.scale.x;
        output.s[1] = jointMod.scale.y;
        output.s[2] = jointMod.scale.z;
        break;
      case 3: {
        idMat3x4 matrix;
        BuildJointMatrix(matrix, jointMod);
        std::memmove(output.mat, matrix.mat, sizeof(matrix.mat));
        break;
      }
      case 7: {
        idMat3x4 matrix;
        BuildJointMatrix(matrix, jointMod);
        std::memmove(output.mat, matrix.mat, sizeof(matrix.mat));
        output.s[0] = jointMod.scale.x;
        output.s[1] = jointMod.scale.y;
        output.s[2] = jointMod.scale.z;
        break;
      }
      default:
        break;
    }
    ApplyTransformFlags(output, jointMod);
  }
  return jointMods.num;
}

void idAnimator_JointMod::PreBlendSnapshot(idAnimStack* stack, int curTime,
                                           int ticksPerSec, float) {
  if (InternalGetMergeBranch() != nullptr) {
    InternalGetMergeBranch()->left = nullptr;
  }
  InternalPreBlendTree(stack, curTime, ticksPerSec);
}

idAnimator_JointMod::idAnimator_JointMod()
    : enabled(true), usePersistentMods(false), jointMods(), maxJointMods(3) {}

idAnimator_JointMod::~idAnimator_JointMod() = default;

template <>
bool idList<idAnimator_JointMod::animatorJointMod_t, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearListExact(*this);
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }
  idAnimator_JointMod::animatorJointMod_t* const replacement =
      static_cast<idAnimator_JointMod::animatorJointMod_t*>(
          idListArrayResize<idAnimator_JointMod::animatorJointMod_t>(
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
int idList<idAnimator_JointMod::animatorJointMod_t, 5>::Append(
    const idAnimator_JointMod::animatorJointMod_t& object) {
  if (list == nullptr) {
    const int initialSize = granularity > 0 ? granularity : 1;
    if (!Resize(initialSize)) {
      return -1;
    }
  }
  if (num == size) {
    int newSize;
    if (forceIdListsToReallocateEveryAppend) {
      newSize = size + 1;
    } else if (granularity != 0) {
      newSize = size + granularity - (size + granularity) % granularity;
    } else {
      newSize = 2 * size;
      if (newSize < size) {
        newSize = (std::numeric_limits<int>::max)();
      } else if (newSize == 0) {
        newSize = 1;
      }
    }
    if (!Resize(newSize)) {
      return -1;
    }
  }
  if (num >= size) {
    return num - 1;
  }
  const int index = num;
  list[index] = object;
  ++num;
  return index;
}

void idAnimator_JointMod::ClearJointMods() {
  if (jointMods.size < 0) {
    ClearListExact(jointMods);
  }
  const int size = jointMods.size;
  jointMods.num = size > 0 ? 0 : size;
}

void idAnimator_JointMod::InternalPostBlendTree(const idAnimStack*, int) {
  if (!usePersistentMods) {
    ClearJointMods();
  }
}

void idAnimator_JointMod::SetJointAxis(animationPose_t pose,
                                       jointModTransform_t type,
                                       idJointIndex_t index,
                                       const idMat3& value) {
  animatorJointMod_t mod;
  mod.pose = pose;
  mod.type = type;
  mod.jointIndex = index;
  mod.axis = value;
  mod.driver = 1;
  mod.pos = vec3_origin;
  mod.scale = vec3_unit;
  jointMods.Append(mod);
}

void idAnimator_JointMod::SetJointPos(animationPose_t pose,
                                      jointModTransform_t type,
                                      idJointIndex_t index,
                                      const idVec3& value) {
  animatorJointMod_t mod;
  mod.pose = pose;
  mod.type = type;
  mod.jointIndex = index;
  mod.axis = mat3_identity;
  mod.pos = value;
  mod.scale = vec3_unit;
  mod.driver = 2;
  jointMods.Append(mod);
}
