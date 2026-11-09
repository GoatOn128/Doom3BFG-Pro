#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/animstack/animator_freezejoints.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include "engine/decls/decljointname.h"
#include "engine/gamelib/animstack/animstack.h"
#include "engine/gamelib/animstack/treeanimator_abi.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"

#include <cstdint>

template <typename T>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

namespace {

struct idDeclFreezeJointsView {
  std::uint8_t declPrefix[0x60];
  idList<idJointName, 5> joints;
};

} // namespace

static_assert(offsetof(idDeclFreezeJointsView, joints) == 0x60,
              "freeze-joints declaration list offset drift");
static_assert(sizeof(idDeclFreezeJointsView) == 0x78,
              "freeze-joints declaration ABI drift");

idCVar anim_enableFreezeJoints(
    "anim_enableFreezeJoints", "1", CVAR_BOOL, "use freezejoints",
    static_cast<argCompletionRef_t>(nullptr));

template <>
__declspec(noinline) bool
idList<idAnimator_FreezeJoints::idJoint, 5>::Resize(int newSize) {
  if (newSize <= 0) {
    if (listStatic == 0 || listStatic == 2) {
      if (list != nullptr) {
        idMem::Singleton().Free(list, ALIGN_16);
      }
      list = nullptr;
      size = 0;
    }
    num = 0;
    return true;
  }

  if (newSize == size || listStatic != 0) {
    return true;
  }

  auto* replacement =
      static_cast<idAnimator_FreezeJoints::idJoint*>(
          idListArrayResize<idAnimator_FreezeJoints::idJoint>(
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

idAnimator_FreezeJoints::idAnimator_FreezeJoints()
    : enabled(false), joints(), animator(nullptr), animLeaf(),
      mergeBranch(nullptr) {}

idAnimator_FreezeJoints::~idAnimator_FreezeJoints() = default;

void idAnimator_FreezeJoints::InternalShutdown(idAnimStack* stack) {
  if (mergeBranch != nullptr) {
    idMD6Allocator::FreeTyped(stack->GetAllocator(), mergeBranch);
    mergeBranch = nullptr;
  }
}

void idAnimator_FreezeJoints::InternalPreBlendTree(
    const idAnimStack*, int, int) {
  if (anim_enableFreezeJoints.valueInteger != 0) {
    for (int index = 0; index < joints.num; ++index) {
      animator->jointAnimator->SetJointAxis(
          DRIVER_MODEL, joints.list[index].index,
          JOINTMOD_MODEL_OVERRIDE, joints.list[index].originalRotation);
    }
  }
}

bool idAnimator_FreezeJoints::InternalInit(
    const idAnimatorParms_Base& parms) {
  const auto& freezeParms =
      static_cast<const idAnimatorParms_FreezeJoints&>(parms);

  mergeBranch = static_cast<idMD6Branch*>(
      parms.animStack->GetAllocator()->Alloc(idMD6Node::NODE_BRANCH));
  if (mergeBranch == nullptr) {
    return false;
  }

  animator = freezeParms.treeAnimator;
  const volatile idJointAnimator* const jointAnimator =
      parms.animStack->jointAnimator;
  const idDeclMD6* const modelDeclaration = jointAnimator->decl;

  if (freezeParms.decl != nullptr) {
    enabled = true;
    const auto* const declaration =
        reinterpret_cast<const idDeclFreezeJointsView*>(freezeParms.decl);
    const int count = declaration->joints.num;
    joints.SetNum(count);

    int index = 0;
    if (declaration->joints.num > 0) {
      do {
        const auto& sourceJoints =
            reinterpret_cast<const idDeclFreezeJointsView*>(
                freezeParms.decl)->joints;
        const char* const jointName = sourceJoints.list[index].c_str();

        idJointIndex_t jointIndex;
        const idMD6Model* const model =
            *reinterpret_cast<idMD6Model* const*>(
                reinterpret_cast<const std::uint8_t*>(modelDeclaration) +
                0x60);
        if (model != nullptr && model->skeleton != nullptr) {
          jointIndex = model->skeleton->GetJointIndex(jointName);
        } else {
          jointIndex = idJointIndex_t(-1);
        }

        joints.list[index].index = jointIndex;
        animator->GetModelSpaceJointAxis(
            ANIMATION_POSE_CURRENT, joints.list[index].index,
            joints.list[index].originalRotation);
        ++index;
      } while (
          index < reinterpret_cast<const idDeclFreezeJointsView*>(
                      freezeParms.decl)->joints.num);
    }
  }

  return enabled;
}
