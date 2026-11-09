#define WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI
#define WOLFTNO_RETAIL_DRAGJOINTS_ANIMWEB_INDEX_ABI
#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/animstack/animator_dragjoints.h"

#include "engine/decls/decljointname.h"
#include "engine/framework/common.h"
#include "engine/gamelib/animstack/animstack.h"
#include "engine/gamelib/animstack/coneconstraint.h"
#pragma warning(push)
#pragma warning(disable : 4099 4324)
#include "engine/models/renderworld.h"
#include "engine/models/skeletalanimation/treeanimator.h"
#pragma warning(pop)
#include "engine/models/skeletalanimation/md6skel.h"
#include "shared/idlib/color.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/math/mathlib.h"

#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_DRAGJOINTS_ANIMWEB_INDEX_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <xmmintrin.h>

class idEntity;
class idScenePoint {
public:
  virtual void ActivateTargets(idEntity* target);
};

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

template <>
bool idList<dragSubWebIndex_t, 5>::Resize(int newSize);
template <>
bool idList<dragStateIndex_t, 5>::Resize(int newSize);

namespace {

struct idDeclDragJointsView {
  std::uint8_t recoveredBase[0x60];
  float ghostJointDistance;
  float maxBend;
  float downHillPitchAdjust;
  std::uint8_t recoveredPadding[4];
  idJointName baseJoint;
  idList<idJointName, 5> joints;
  idList<idStr, 5> subWebs;
  idList<idStr, 5> excludeStates;
};

}  // namespace

static_assert(offsetof(idDeclDragJointsView, ghostJointDistance) == 0x60,
              "drag-joints ghost distance offset drift");
static_assert(offsetof(idDeclDragJointsView, maxBend) == 0x64,
              "drag-joints maximum bend offset drift");
static_assert(offsetof(idDeclDragJointsView, downHillPitchAdjust) == 0x68,
              "drag-joints downhill adjustment offset drift");
static_assert(offsetof(idDeclDragJointsView, baseJoint) == 0x70,
              "drag-joints base name offset drift");
static_assert(offsetof(idDeclDragJointsView, joints) == 0x98,
              "drag-joints declaration joint-list offset drift");
static_assert(offsetof(idDeclDragJointsView, subWebs) == 0xB0,
              "drag-joints declaration subweb-list offset drift");
static_assert(offsetof(idDeclDragJointsView, excludeStates) == 0xC8,
              "drag-joints declaration state-list offset drift");
static_assert(sizeof(idDeclDragJointsView) == 0xE0,
              "drag-joints declaration ABI drift");

idCVar anim_enableDragJoints(
    "anim_enableDragJoints", "1", CVAR_BOOL, "use dragjoints",
    static_cast<argCompletionRef_t>(nullptr));
idCVar anim_enableDragJointsConstraints(
    "anim_enableDragJointsConstraints", "1", CVAR_BOOL,
    "use dragjoints constraints",
    static_cast<argCompletionRef_t>(nullptr));
idCVar anim_debugDragJoints(
    "anim_debugDragJoints", "0", CVAR_BOOL,
    "display debugging information for drag joints",
    static_cast<argCompletionRef_t>(nullptr));

float idMath::ASin(float value) {
  if (value <= -1.0f) {
    return -1.5707964f;
  }
  if (value < 1.0f) {
    return ::asinf(value);
  }
  return 1.5707964f;
}

float idMath::AngleNormalize360(float angle) {
  if (angle >= 360.0f || angle < 0.0f) {
    return angle - (::floorf(angle * 0.0027777778f) * 360.0f);
  }
  return angle;
}

float idMath::AngleNormalize180(float angle) {
  float normalized = angle;
  if (angle >= 360.0f || angle < 0.0f) {
    normalized = angle - (::floorf(angle * 0.0027777778f) * 360.0f);
  }
  if (normalized > 180.0f) {
    return normalized - 360.0f;
  }
  return normalized;
}

float idMath::AngleDelta(float angle1, float angle2) {
  float delta = angle1 - angle2;
  if (delta >= 360.0f || delta < 0.0f) {
    delta = delta - (::floorf(delta * 0.0027777778f) * 360.0f);
  }
  if (delta > 180.0f) {
    return delta - 360.0f;
  }
  return delta;
}

template <>
__declspec(noinline) bool
idList<idAnimator_DragJoints::idJoint, 5>::Resize(int newSize) {
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

  if (size > 0 || newSize == granularity) {
    reinterpret_cast<idScenePoint*>(this)->idScenePoint::ActivateTargets(
        reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }

  list = static_cast<idAnimator_DragJoints::idJoint*>(
      idListArrayResize<idAnimator_DragJoints::idJoint>(
          list, size, newSize, static_cast<memTag_t>(memTag), false));
  if (list == nullptr) {
    return false;
  }
  size = newSize;
  if (num > newSize) {
    num = newSize;
  }
  return true;
}

idAnimator_DragJoints::idAnimator_DragJoints()
    : enabled(false), blendFrame(0), numBlendFrames(20), animweb(nullptr),
      subwebs(), excludeStates(), baseJoint(static_cast<std::int16_t>(-1)),
      joints(), maxBend(90.0f), downHillPitchAdjust(1.0f), animator(nullptr),
      animLeaf(), mergeBranch(nullptr) {}

idAnimator_DragJoints::~idAnimator_DragJoints() = default;

void idAnimator_DragJoints::ReinitJoints() {
  idMat3 rotation;
  animator->GetWorldSpaceJointTransform(DRIVER_MODEL, baseJoint,
                                        joints.list[0].position, rotation);
  for (int index = 1; index < joints.num; ++index) {
    const float length = joints.list[index - 1].length;
    joints.list[index].position.x =
        joints.list[index - 1].position.x - (rotation.mat[0].x * length);
    joints.list[index].position.y =
        joints.list[index - 1].position.y - (rotation.mat[0].y * length);
    joints.list[index].position.z =
        joints.list[index - 1].position.z - (rotation.mat[0].z * length);
  }
}

void idAnimator_DragJoints::InternalShutdown(idAnimStack* stack) {
  if (mergeBranch != nullptr) {
    idMD6Allocator::FreeTyped(stack->GetAllocator(), mergeBranch);
    mergeBranch = nullptr;
  }
}

void idAnimator_DragJoints::InternalPreBlendTree(
    const idAnimStack*, int, int) {
  if (anim_enableDragJoints.valueInteger == 0) {
    return;
  }

  if (anim_debugDragJoints.valueInteger != 0) {
    for (int index = 0; index < joints.num; ++index) {
      idVec3 position;
      animator->GetWorldSpaceJointPosition(DRIVER_MODEL,
                                           joints.list[index].index,
                                           position);
      common->RW()->GetRenderDebug()->DebugPoint(
          idColor::colorRed, position, 0, false);
    }
  }

  if (animweb != nullptr) {
    const dragSubWebIndex_t currentSubWeb = animweb->GetCurSubWebIndex();
    bool active = false;
    for (int index = 0; index < subwebs.num; ++index) {
      if (subwebs.list[index].value == currentSubWeb.value) {
        active = true;
        break;
      }
    }

    if (active) {
      const dragStateIndex_t currentState = animweb->GetCurStateIndex();
      for (int index = 0; index < excludeStates.num; ++index) {
        if (excludeStates.list[index].value == currentState.value) {
          active = false;
          break;
        }
      }
    }

    if (active) {
      if (blendFrame == 0) {
        ReinitJoints();
      }
      const int nextFrame = blendFrame + 1;
      blendFrame = nextFrame < numBlendFrames ? nextFrame : numBlendFrames;
    } else {
      blendFrame = blendFrame > 0 ? blendFrame - 1 : 0;
    }
  }

  if (blendFrame == 0) {
    return;
  }

  idMat3 baseRotation;
  animator->GetWorldSpaceJointTransform(DRIVER_MODEL, baseJoint,
                                        joints.list[0].position,
                                        baseRotation);

  idVec3 lastDirection{-baseRotation.mat[0].x,
                       -baseRotation.mat[0].y,
                       -baseRotation.mat[0].z};
  for (int index = 1; index < joints.num; ++index) {
    idVec3 boneDirection{
        joints.list[index].position.x - joints.list[index - 1].position.x,
        joints.list[index].position.y - joints.list[index - 1].position.y,
        joints.list[index].position.z - joints.list[index - 1].position.z};
    const float boneLengthSquared =
        ((boneDirection.x * boneDirection.x) +
         (boneDirection.y * boneDirection.y)) +
        (boneDirection.z * boneDirection.z);
    const __m128 clampedBoneLength = _mm_max_ss(
        _mm_set_ss(boneLengthSquared),
        idMath::vector_float_smallestNonDenorm);
    const float inverseLength =
        1.0f / _mm_sqrt_ss(clampedBoneLength).m128_f32[0];
    boneDirection.x *= inverseLength;
    boneDirection.y *= inverseLength;
    boneDirection.z *= inverseLength;

    idVec3 outputDirection = boneDirection;
    if (anim_enableDragJointsConstraints.valueInteger != 0) {
      idConeConstraint_Vec3 cone(lastDirection,
                                 idRadians(maxBend * 0.017453292f));
      cone.RotateTo(boneDirection, 1.0f);
      outputDirection = cone.current;
    }

    const float length = joints.list[index - 1].length;
    joints.list[index].position.x =
        joints.list[index - 1].position.x + (outputDirection.x * length);
    joints.list[index].position.y =
        joints.list[index - 1].position.y + (outputDirection.y * length);
    joints.list[index].position.z =
        joints.list[index - 1].position.z + (outputDirection.z * length);
    lastDirection = boneDirection;
  }

  const bool finishedBlend = blendFrame == numBlendFrames;
  const float blendFraction =
      static_cast<float>(blendFrame) / static_cast<float>(numBlendFrames);

  for (int index = 0; index < joints.num - 1; ++index) {
    idVec3 newDirection{
        joints.list[index + 1].position.x - joints.list[index].position.x,
        joints.list[index + 1].position.y - joints.list[index].position.y,
        joints.list[index + 1].position.z - joints.list[index].position.z};
    const float newLengthSquared =
        ((newDirection.x * newDirection.x) +
         (newDirection.y * newDirection.y)) +
        (newDirection.z * newDirection.z);
    const __m128 clampedNewLength = _mm_max_ss(
        _mm_set_ss(newLengthSquared),
        idMath::vector_float_smallestNonDenorm);
    float inverseLength =
        1.0f / _mm_sqrt_ss(clampedNewLength).m128_f32[0];
    newDirection.x *= inverseLength;
    newDirection.y *= inverseLength;
    newDirection.z *= inverseLength;

    idVec3 previousDirection;
    if (index != 0) {
      previousDirection = {
          joints.list[index].position.x - joints.list[index - 1].position.x,
          joints.list[index].position.y - joints.list[index - 1].position.y,
          joints.list[index].position.z - joints.list[index - 1].position.z};
      const float previousLengthSquared =
          ((previousDirection.x * previousDirection.x) +
           (previousDirection.y * previousDirection.y)) +
          (previousDirection.z * previousDirection.z);
      const __m128 clampedPreviousLength = _mm_max_ss(
          _mm_set_ss(previousLengthSquared),
          idMath::vector_float_smallestNonDenorm);
      inverseLength =
          1.0f / _mm_sqrt_ss(clampedPreviousLength).m128_f32[0];
      previousDirection.x *= inverseLength;
      previousDirection.y *= inverseLength;
      previousDirection.z *= inverseLength;
    } else {
      previousDirection = {-animator->g.axis.mat[0].x,
                           -animator->g.axis.mat[0].y,
                           -animator->g.axis.mat[0].z};
    }

    if (anim_debugDragJoints.valueInteger != 0) {
      common->RW()->GetRenderDebug()->DebugPoint(
          idColor::colorGreen, joints.list[index].position, 0, false);
      common->RW()->GetRenderDebug()->DebugLine(
          idColor::colorWhite, joints.list[index].position,
          joints.list[index + 1].position, 0, false);
      const idVec3 previousEnd{
          joints.list[index].position.x + (previousDirection.x * 5.0f),
          joints.list[index].position.y + (previousDirection.y * 5.0f),
          joints.list[index].position.z + (previousDirection.z * 5.0f)};
      common->RW()->GetRenderDebug()->DebugLine(
          idColor::colorRed, joints.list[index].position, previousEnd, 0,
          false);
      const idVec3 newEnd{
          joints.list[index].position.x + (newDirection.x * 5.0f),
          joints.list[index].position.y + (newDirection.y * 5.0f),
          joints.list[index].position.z + (newDirection.z * 5.0f)};
      common->RW()->GetRenderDebug()->DebugLine(
          idColor::colorYellow, joints.list[index].position, newEnd, 0,
          false);
    }

    const float previousHorizontalSquared =
        (previousDirection.x * previousDirection.x) +
        (previousDirection.y * previousDirection.y);
    const __m128 clampedPreviousHorizontal = _mm_max_ss(
        _mm_set_ss(previousHorizontalSquared),
        idMath::vector_float_smallestNonDenorm);
    const float previousHorizontalInverse =
        1.0f / _mm_sqrt_ss(clampedPreviousHorizontal).m128_f32[0];
    const float newHorizontalSquared =
        (newDirection.x * newDirection.x) +
        (newDirection.y * newDirection.y);
    const __m128 clampedNewHorizontal = _mm_max_ss(
        _mm_set_ss(newHorizontalSquared),
        idMath::vector_float_smallestNonDenorm);
    const float newHorizontalInverse =
        1.0f / _mm_sqrt_ss(clampedNewHorizontal).m128_f32[0];
    const float previousHeading =
        ::atan2f(previousHorizontalInverse * previousDirection.x,
                 previousHorizontalInverse * previousDirection.y);
    const float newHeading =
        ::atan2f(newHorizontalInverse * newDirection.x,
                 newHorizontalInverse * newDirection.y);
    const float yaw = idMath::AngleNormalize180(
        (previousHeading * 57.295776f) - (newHeading * 57.295776f));

    const float previousPitch = idMath::ASin(previousDirection.z);
    const float newPitch = idMath::ASin(newDirection.z);
    float pitch = (newPitch * 57.295776f) -
                  (previousPitch * 57.295776f);
    pitch = idMath::AngleNormalize180(pitch);
    if (pitch < 0.0f) {
      pitch *= downHillPitchAdjust;
    }

    const idAngles angles{pitch, yaw, 0.0f};
    idMat3 axis;
    if (finishedBlend) {
      axis = angles.ToMat3();
    } else {
      const idQuat target = angles.ToQuat();
      idQuat blended;
      blended.Slerp(quat_identity, target, blendFraction);
      axis = blended.ToMat3();
    }
    animator->jointAnimator->SetJointAxis(
        DRIVER_MODEL, joints.list[index].index, JOINTMOD_MODEL, axis);
  }
}

bool idAnimator_DragJoints::InternalInit(
    const idAnimatorParms_Base& parms) {
  mergeBranch = static_cast<idMD6Branch*>(
      parms.animStack->GetAllocator()->Alloc(idMD6Node::NODE_BRANCH));
  if (mergeBranch == nullptr) {
    return false;
  }

  const auto& dragParms =
      static_cast<const idAnimatorParms_DragJoints&>(parms);
  animator = dragParms.treeAnimator;
  const auto* const declaration =
      reinterpret_cast<const idDeclDragJointsView*>(
          dragParms.declDragJoints);
  maxBend = declaration->maxBend;
  downHillPitchAdjust = declaration->downHillPitchAdjust;

  const idDeclMD6* const modelDeclaration =
      parms.animStack->jointAnimator->decl;
  if (declaration != nullptr) {
    if (animweb != nullptr) {
      subwebs.SetNum(declaration->subWebs.num);
      for (int index = 0; index < declaration->subWebs.num; ++index) {
        subwebs.list[index] =
            animweb->decl->GetSubWebIndexForName(
                declaration->subWebs.list[index].c_str());
      }

      excludeStates.SetNum(declaration->excludeStates.num);
      for (int index = 0; index < declaration->excludeStates.num; ++index) {
        excludeStates.list[index] =
            animweb->decl->GetStateIndexForName(
                declaration->excludeStates.list[index].c_str());
      }
    }

    const int declaredJointCount = declaration->joints.num;
    if (declaredJointCount > 0) {
      joints.SetNum(declaredJointCount + 1);

      if (modelDeclaration->model != nullptr &&
          modelDeclaration->model->skeleton != nullptr) {
        baseJoint = modelDeclaration->model->skeleton->GetJointIndex(
            declaration->baseJoint.c_str());
      } else {
        baseJoint = idJointIndex_t(static_cast<std::int16_t>(-1));
      }

      enabled = true;
      for (int index = 0; index < declaredJointCount; ++index) {
        if (modelDeclaration->model != nullptr &&
            modelDeclaration->model->skeleton != nullptr) {
          joints.list[index].index =
              modelDeclaration->model->skeleton->GetJointIndex(
                  declaration->joints.list[index].c_str());
        } else {
          joints.list[index].index =
              idJointIndex_t(static_cast<std::int16_t>(-1));
        }
        if (joints.list[index].index.value < 0) {
          enabled = false;
        }
      }

      if (enabled) {
        joints.list[joints.num - 1].index =
            joints.list[joints.num - 2].index;
        for (int index = 0; index < joints.num; ++index) {
          animator->GetWorldSpaceJointTransform(
              DRIVER_MODEL, joints.list[index].index,
              joints.list[index].position,
              joints.list[index].originalRotation);
          if (index == joints.num - 1) {
            const idVec3 delta{
                joints.list[index].position.x - joints.list[0].position.x,
                joints.list[index].position.y - joints.list[0].position.y,
                joints.list[index].position.z - joints.list[0].position.z};
            const float deltaLengthSquared =
                ((delta.x * delta.x) + (delta.y * delta.y)) +
                (delta.z * delta.z);
            const __m128 clampedDeltaLength = _mm_max_ss(
                _mm_set_ss(deltaLengthSquared),
                idMath::vector_float_smallestNonDenorm);
            const float inverseLength =
                1.0f / _mm_sqrt_ss(clampedDeltaLength).m128_f32[0];
            joints.list[index].position.x +=
                (delta.x * inverseLength) * declaration->ghostJointDistance;
            joints.list[index].position.y +=
                (delta.y * inverseLength) * declaration->ghostJointDistance;
            joints.list[index].position.z +=
                (delta.z * inverseLength) * declaration->ghostJointDistance;
          }
        }

        for (int index = 0; index < joints.num - 1; ++index) {
          const float deltaX = joints.list[index].position.x -
                               joints.list[index + 1].position.x;
          const float deltaY = joints.list[index].position.y -
                               joints.list[index + 1].position.y;
          const float horizontalSquared =
              (deltaY * deltaY) + (deltaX * deltaX);
          const __m128 clamped = _mm_max_ss(
              _mm_set_ss(horizontalSquared),
              idMath::vector_float_smallestNonDenorm);
          joints.list[index].length =
              _mm_sqrt_ss(clamped).m128_f32[0];
        }
        ReinitJoints();
      }
    }
  }

  return enabled;
}
