#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/animstack/animator_rollbones.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include "engine/gamelib/animstack/animator_treesway_abi.h"
#include "engine/gamelib/animstack/animstack.h"
#include "shared/idlib/lib.h"

#include <cstdint>

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

namespace {

struct idMD6ModelRollBonesView {
  std::uint8_t prefix[0x128];
  idMD6Skel* skeleton;
};

struct idDeclMD6RollBonesView {
  void* vtable;
  const char* name;
  std::uint8_t prefix[0x50];
  idMD6ModelRollBonesView* model;
};

const idDeclMD6RollBonesView* DeclView(const idDeclMD6* declaration) {
  return reinterpret_cast<const idDeclMD6RollBonesView*>(declaration);
}

const idMD6Skel* GetSkeleton(const idDeclMD6* declaration) {
  const idMD6ModelRollBonesView* const model = DeclView(declaration)->model;
  return model == nullptr ? nullptr : model->skeleton;
}

template <typename Type, int Tag>
void ClearListExact(idList<Type, Tag>& list) {
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

static_assert(offsetof(idMD6ModelRollBonesView, skeleton) == 0x128,
              "roll-bones model skeleton offset drift");
static_assert(offsetof(idDeclMD6RollBonesView, name) == 0x08,
              "roll-bones declaration name offset drift");
static_assert(offsetof(idDeclMD6RollBonesView, model) == 0x60,
              "roll-bones declaration model offset drift");

template <>
__declspec(noinline) bool
idList<idAnimator_RollBones::idRollInstance, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    ClearListExact(*this);
    return true;
  }
  if (newSize == size || listStatic != 0) {
    return true;
  }

  auto* const replacement =
      static_cast<idAnimator_RollBones::idRollInstance*>(
          idListArrayResize<idAnimator_RollBones::idRollInstance>(
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
__declspec(noinline) bool
idList<idAnimator_RollBones::idRollInstance, 5>::SetNum(int newNum) {
  if (newNum > size && !Resize(newNum)) {
    return false;
  }
  num = newNum < size ? newNum : size;
  return true;
}

idAnimator_RollBones::idAnimator_RollBones()
    : enabled(false), animator(nullptr), animLeaf(), mergeBranch(),
      rollBones() {}

idAnimator_RollBones::~idAnimator_RollBones() = default;

bool idAnimator_RollBones::HasCompatibleRollBones(
    const idDeclRollBones* rollBoneSetup,
    const idDeclMD6* declaration) {
  int index = 0;
  if (rollBoneSetup->rollBones.num <= 0) {
    return true;
  }

  do {
    const idDeclRollBones::idDeclRollBoneInstance* const source =
        &rollBoneSetup->rollBones.list[index];
    const char* const sourceName = source->rollSourceJoint.c_str();
    const idMD6Skel* const sourceSkeleton = GetSkeleton(declaration);
    const idJointIndex_t sourceJoint =
        sourceSkeleton != nullptr
            ? sourceSkeleton->GetJointIndex(sourceName)
            : idJointIndex_t(-1);

    const char* const destinationName = source->rollDestJoint.c_str();
    const idMD6Skel* const destinationSkeleton = GetSkeleton(declaration);
    const idJointIndex_t destinationJoint =
        destinationSkeleton != nullptr
            ? destinationSkeleton->GetJointIndex(destinationName)
            : idJointIndex_t(-1);

    if (sourceJoint.value < 0 || destinationJoint.value < 0) {
      return false;
    }
    ++index;
  } while (index < rollBoneSetup->rollBones.num);
  return true;
}

void idAnimator_RollBones::InternalPreBlendTree(
    const idAnimStack* stack, int, int) {
  idJointAnimator* const jointAnimator = stack->jointAnimator;
  const idDeclMD6* const declaration = jointAnimator->decl;
  const idMD6ModelRollBonesView* const model = DeclView(declaration)->model;
  const idMD6Skel* const skeleton = model->skeleton;
  if (skeleton == nullptr) {
    return;
  }

  int index = 0;
  while (index < rollBones.num) {
    idRollInstance* const roll = &rollBones.list[index];
    idMat3 sourceRotation;
    jointAnimator->GetModelSpaceJointAxis(
        ANIMATION_POSE_CURRENT, roll->rollSourceJoint, sourceRotation);

    idMat3 parentInverse = mat3_identity;
    idMD6SkelData* const data = skeleton->data;
    const auto* const parents = reinterpret_cast<const std::int16_t*>(
        reinterpret_cast<const std::uint8_t*>(data) +
        data->parentTableOffset);
    const std::int16_t parent = parents[roll->rollSourceJoint.value];
    if (parent >= 0 &&
        jointAnimator->GetModelSpaceJointAxis(
            ANIMATION_POSE_CURRENT, idJointIndex_t(parent),
            parentInverse)) {
      parentInverse.InverseSelf();
    }

    const idMat3 sourceLocal =
        (sourceRotation * parentInverse) * roll->localInv;
    const idRotation sourceLocalRotation = sourceLocal.ToRotation();

    idRotation rollRotation;
    rollRotation.vec.x = roll->rollAxis.x;
    rollRotation.vec.y = roll->rollAxis.y;
    rollRotation.vec.z = roll->rollAxis.z;
    const float projectedXY =
        sourceLocalRotation.vec.x * roll->rollAxis.x +
        sourceLocalRotation.vec.y * roll->rollAxis.y;
    const float projected =
        projectedXY + sourceLocalRotation.vec.z * roll->rollAxis.z;
    const float scaledAngle =
        sourceLocalRotation.angle * roll->rollAmount;
    rollRotation.angle = -(projected * scaledAngle);
    rollRotation.axisValid = false;

    const idMat3 output = rollRotation.ToMat3();
    jointAnimator->SetJointAxis(
        ANIMATION_POSE_CURRENT, roll->rollDestJoint,
        JOINTMOD_LOCAL_OVERRIDE, output);
    ++index;
  }
}

bool idAnimator_RollBones::InternalInit(
    const idAnimatorParms_Base& parms) {
  const idAnimStack* const stack = parms.animStack;
  idJointAnimator* const initialJointAnimator = stack->jointAnimator;
  const idDeclMD6* const declaration = initialJointAnimator->decl;
  if (declaration == nullptr) {
    return false;
  }

  const idMD6ModelRollBonesView* const initialModel =
      DeclView(declaration)->model;
  const idMD6Skel* const skeleton = initialModel->skeleton;
  if (skeleton == nullptr) {
    return false;
  }

  const auto& rollParms =
      static_cast<const idAnimatorParms_RollBones&>(parms);
  const idDeclRollBones* const setup = rollParms.declRollBones;
  if (setup != nullptr) {
    idMD6SkelData* const data = skeleton->data;
    std::uint16_t paddedJointCount =
        static_cast<std::uint16_t>(data->numJoints + 7u);
    paddedJointCount =
        static_cast<std::uint16_t>(paddedJointCount & 0xFFF8u);
    const float* const rollAxes = reinterpret_cast<const float*>(
        reinterpret_cast<const std::uint8_t*>(data) +
        data->basePoseOffset + 28 * paddedJointCount);

    rollBones.SetNum(setup->rollBones.num);
    bool valid = rollBones.num > 0;
    int index = 0;
    while (index < rollBones.num) {
      const idDeclRollBones::idDeclRollBoneInstance* const source =
          &setup->rollBones.list[index];
      idRollInstance* const destination = &rollBones.list[index];

      const char* const sourceName = source->rollSourceJoint.c_str();
      const idMD6Skel* const sourceSkeleton = GetSkeleton(declaration);
      destination->rollSourceJoint =
          sourceSkeleton != nullptr
              ? sourceSkeleton->GetJointIndex(sourceName)
              : idJointIndex_t(-1);

      const char* const destinationName = source->rollDestJoint.c_str();
      const idMD6Skel* const destinationSkeleton = GetSkeleton(declaration);
      destination->rollDestJoint =
          destinationSkeleton != nullptr
              ? destinationSkeleton->GetJointIndex(destinationName)
              : idJointIndex_t(-1);

      destination->localInv = mat3_identity;
      idJointAnimator* const jointAnimator = parms.animStack->jointAnimator;
      if (jointAnimator != nullptr) {
        jointAnimator->GetLocalSpaceJointBindAxis(
            destination->rollSourceJoint, destination->localInv);
        destination->localInv.InverseSelf();
      }

      if (destination->rollSourceJoint.value < 0 ||
          destination->rollDestJoint.value < 0) {
        valid = false;
        if (destination->rollSourceJoint.value < 0) {
          const char* const declarationName = DeclView(declaration)->name;
          const char* const jointName = source->rollSourceJoint.c_str();
          idLib::Warning(
              "idAnimator_RollBones::InternalInit - md6Def '%s' has "
              "rollbone with invalid joint name '%s'",
              declarationName, jointName);
        }
        if (destination->rollDestJoint.value < 0) {
          const char* const declarationName = DeclView(declaration)->name;
          const char* const jointName = source->rollDestJoint.c_str();
          idLib::Warning(
              "idAnimator_RollBones::InternalInit - md6Def '%s' has "
              "rollbone with invalid joint name '%s'",
              declarationName, jointName);
        }
      } else {
        destination->rollAmount = source->rollAmount;
        const std::int16_t destinationIndex =
            destination->rollDestJoint.value;
        destination->rollAxis.x = rollAxes[3 * destinationIndex];
        destination->rollAxis.y = rollAxes[3 * destinationIndex + 1];
        destination->rollAxis.z = rollAxes[3 * destinationIndex + 2];
        destination->rollAxis.Normalize();
      }
      ++index;
    }

    if (!valid) {
      ClearListExact(rollBones);
    }
    enabled = valid;
  }
  return enabled;
}
