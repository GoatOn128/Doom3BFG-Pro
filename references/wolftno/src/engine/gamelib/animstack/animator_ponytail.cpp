#define WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI
#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#define WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#define WOLFTNO_RETAIL_IDLIST_OUTOFLINE_RESIZE_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_PONYTAIL_QUATERNION_MULTIPLY_ABI
#define WOLFTNO_RETAIL_STATICLIST_FORCEINLINE_CTOR_ABI
#include "shared/idlib/csystems/cvarsystemlocal.h"

#define WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#define WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#define WOLFTNO_RETAIL_TRACKER_LEAF_DTOR_UNWIND_ABI
#include "engine/gamelib/animstack/animator_ponytail.h"

#undef WOLFTNO_RETAIL_TRACKER_LEAF_DTOR_UNWIND_ABI
#undef WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI

#include "engine/framework/gametimemanager.h"
#include "engine/gamelib/animstack/animstack.h"
#include "engine/models/skeletalanimation/treeanimator.h"
#include "engine/models/skeletalanimation/md6skel.h"
#include "shared/idlib/color.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"

#undef WOLFTNO_RETAIL_TRACKER_LEAF_DTOR_UNWIND_ABI
#undef WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI
#undef WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#undef WOLFTNO_RETAIL_STATICLIST_FORCEINLINE_CTOR_ABI
#undef WOLFTNO_RETAIL_PONYTAIL_QUATERNION_MULTIPLY_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_IDLIST_OUTOFLINE_RESIZE_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CTOR_DTOR_ABI
#undef WOLFTNO_RETAIL_IDLIST_FORCEINLINE_CLEARFREE_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <xmmintrin.h>

class idRenderDebug {
public:
  virtual ~idRenderDebug();
  virtual void DebugClear(int time) = 0;
  virtual void DebugLine(const idVec4& color, const idVec3& start,
                         const idVec3& end, int lifetime,
                         bool depthTest) = 0;
  virtual void DebugArrow(const idVec4& color, const idVec3& start,
                          const idVec3& end, float size, int lifetime,
                          bool depthTest) = 0;
};

class idEntity;
class idScenePoint {
public:
  virtual void ActivateTargets(idEntity* target);
};

idRenderDebug* GetGameLibRenderDebug();

void SetJointModFromOriginAxis(idMD6Blend::jointMod_t& jointMod,
                               idJointIndex_t joint,
                               unsigned short flags,
                               const idVec3& origin,
                               const idMat3& axis);

template <typename Type>
void* idListArrayResize(void* oldPointer, int oldCount, int newCount,
                        memTag_t tag, bool zeroBuffer);

template <>
void* idListArrayResize<idPonytailSegment>(
    void* oldPointer, int oldCount, int newCount, memTag_t tag,
    bool zeroBuffer);

static idCVar pt_debug(
    "pt_debug", "0", CVAR_INTEGER, "1 = show ponytail debug info",
    static_cast<argCompletionRef_t>(nullptr));
static idCVar pt_modifyJoints(
    "pt_modifyJoints", "1", CVAR_BOOL,
    "1 = allow ponytails to modify joints using anim mods",
    static_cast<argCompletionRef_t>(nullptr));

static __forceinline float InverseLength(const float lengthSquared) {
  const __m128 squared = _mm_set_ss(lengthSquared);
  const __m128 root = _mm_sqrt_ps(
      _mm_max_ps(squared, idMath::vector_float_smallestNonDenorm));
  return 1.0f / _mm_cvtss_f32(root);
}

static __forceinline float LengthClamped(const idVec3& value) {
  const float squared =
      ((value.x * value.x) + (value.y * value.y)) + (value.z * value.z);
  return _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
      _mm_set_ss(squared), idMath::vector_float_smallestNonDenorm)));
}

static __forceinline float RetailMin(const float first,
                                     const float second) {
  return _mm_cvtss_f32(
      _mm_min_ss(_mm_set_ss(first), _mm_set_ss(second)));
}

static __forceinline float RetailMax(const float first,
                                     const float second) {
  return _mm_cvtss_f32(
      _mm_max_ss(_mm_set_ss(first), _mm_set_ss(second)));
}

static __forceinline idQuat InverseMultiply(const idQuat& left,
                                            const idQuat& right) {
  idQuat result;
  result.w =
      ((right.w * left.w - right.x * -left.x) - right.y * -left.y) -
      right.z * -left.z;
  result.z =
      ((right.w * -left.z + right.z * left.w) + right.y * -left.x) -
      right.x * -left.y;
  result.y =
      ((right.w * -left.y + right.y * left.w) + right.x * -left.z) -
      right.z * -left.x;
  result.x =
      ((right.w * -left.x + right.x * left.w) + right.z * -left.y) -
      right.y * -left.z;
  return result;
}

static __forceinline idQuat Multiply(const idQuat& left,
                                     const idQuat& right) {
  idQuat result;
  result.w =
      ((left.w * right.w - left.x * right.x) - left.y * right.y) -
      left.z * right.z;
  result.z =
      ((left.w * right.z + left.z * right.w) + left.x * right.y) -
      left.y * right.x;
  result.y =
      ((left.w * right.y + left.y * right.w) + left.z * right.x) -
      left.x * right.z;
  result.x =
      ((left.w * right.x + left.x * right.w) + left.y * right.z) -
      left.z * right.y;
  return result;
}

static __forceinline idVec3 ModelToWorld(const idTreeAnimator& animator,
                                         const idVec3& local) {
  idVec3 result;
  result.x =
      (((local.y * animator.g.axis.mat[1].x) +
        (local.x * animator.g.axis.mat[0].x)) +
       (local.z * animator.g.axis.mat[2].x)) +
      animator.g.origin.x;
  result.y =
      (((local.x * animator.g.axis.mat[0].y) +
        (local.y * animator.g.axis.mat[1].y)) +
       (local.z * animator.g.axis.mat[2].y)) +
      animator.g.origin.y;
  result.z =
      (((local.x * animator.g.axis.mat[0].z) +
        (local.y * animator.g.axis.mat[1].z)) +
       (local.z * animator.g.axis.mat[2].z)) +
      animator.g.origin.z;
  return result;
}

static __forceinline idVec3 WorldToModel(const idTreeAnimator& animator,
                                         const idVec3& world) {
  const float deltaX = world.x - animator.g.origin.x;
  const float deltaY = world.y - animator.g.origin.y;
  const float deltaZ = world.z - animator.g.origin.z;
  return {
      ((deltaX * animator.g.axis.mat[0].x) +
       (deltaY * animator.g.axis.mat[0].y)) +
          (deltaZ * animator.g.axis.mat[0].z),
      ((deltaX * animator.g.axis.mat[1].x) +
       (deltaY * animator.g.axis.mat[1].y)) +
          (deltaZ * animator.g.axis.mat[1].z),
      ((deltaX * animator.g.axis.mat[2].x) +
       (deltaY * animator.g.axis.mat[2].y)) +
          (deltaZ * animator.g.axis.mat[2].z),
  };
}

static void DebugPoint(const idColor& color, const idVec3& point,
                       const float scale, const int time,
                       const bool depthTest) {
  const float zero = 0.0f;
  idVec3 start{point.x - scale, point.y - zero, point.z - zero};
  idVec3 end{point.x + scale, point.y + zero, point.z + zero};
  GetGameLibRenderDebug()->DebugLine(color, start, end, time, depthTest);

  start = {point.x - zero, point.y - scale, point.z - zero};
  end = {point.x + zero, point.y + scale, point.z + zero};
  GetGameLibRenderDebug()->DebugLine(color, start, end, time, depthTest);

  start = {point.x - zero, point.y - zero, point.z - scale};
  end = {point.x + zero, point.y + zero, point.z + scale};
  GetGameLibRenderDebug()->DebugLine(color, start, end, time, depthTest);
}

static void DebugAxis(const idVec3& origin, const idMat3& axis,
                      const float scale, const int time,
                      const bool depthTest) {
  const float axis0X = scale * axis.mat[0].x;
  const float axis0Y = scale * axis.mat[0].y;
  const float axis0Z = scale * axis.mat[0].z;
  const float axis1X = scale * axis.mat[1].x;
  const float axis1Y = scale * axis.mat[1].y;
  const float axis1Z = scale * axis.mat[1].z;
  const float axis2X = scale * axis.mat[2].x;
  const float axis2Y = scale * axis.mat[2].y;
  const float axis2Z = scale * axis.mat[2].z;

  idVec3 end{axis0X + origin.x, axis0Y + origin.y,
             axis0Z + origin.z};
  GetGameLibRenderDebug()->DebugLine(idColor::colorGreen, origin, end,
                                     time, depthTest);
  end = {axis1X + origin.x, axis1Y + origin.y, axis1Z + origin.z};
  GetGameLibRenderDebug()->DebugLine(idColor::colorRed, origin, end,
                                     time, depthTest);
  end = {axis2X + origin.x, axis2Y + origin.y, axis2Z + origin.z};
  GetGameLibRenderDebug()->DebugLine(idColor::colorBlue, origin, end,
                                     time, depthTest);
}

template <>
const float idSpring<idVec3>::K_MAX = 10000.0f;

float idMath::Cos(float a) {
  float value = a;
  if (a < 0.0f || a >= 6.2831855f) {
    value = a - (::floorf(a * 0.15915494f) * 6.2831855f);
  }
  return ::cosf(value);
}

idQuat& idQuat::operator*=(const idQuat& value) {
  *this = *this * value;
  return *this;
}

template <>
void idSpring<idVec3>::Update(float dt) {
  float remainingTime = dt;
  while (remainingTime > 0.0f) {
    float step;
    if (remainingTime <= 0.0085000005f) {
      step = remainingTime;
      remainingTime = 0.0f;
    } else {
      remainingTime = remainingTime - 0.0085000005f;
      step = 0.0085000005f;
    }

    const float deltaY = p1.y - p0.y;
    const float deltaZ = p1.z - p0.z;
    const float deltaX = p1.x - p0.x;
    const float lengthSquared =
        ((deltaY * deltaY) + (deltaX * deltaX)) +
        (deltaZ * deltaZ);
    const float inverseLength = InverseLength(lengthSquared);
    const float length = inverseLength * lengthSquared;
    const float springForce = -((length - restLength) * k);
    const float forceZ = (deltaZ * inverseLength) * springForce;
    const float forceY = (deltaY * inverseLength) * springForce;
    const float inverseMass = 1.0f / m;

    const float velocityX =
        ((((-(length - restLength) * k) * (deltaX * inverseLength) -
           (c * vel.x)) *
          inverseMass) *
         step) +
        vel.x;
    const float velocityY =
        (((forceY - (c * vel.y)) * inverseMass) * step) + vel.y;
    const float velocityZ =
        (((forceZ - (c * vel.z)) * inverseMass) * step) + vel.z;
    vel.x = velocityX;
    vel.y = velocityY;
    vel.z = velocityZ;

    const float speedSquared =
        ((velocityX * velocityX) + (velocityY * velocityY)) +
        (velocityZ * velocityZ);
    const float inverseSpeed = InverseLength(speedSquared);
    const float normalizedX = velocityX * inverseSpeed;
    vel.x = normalizedX;
    vel.y = velocityY * inverseSpeed;
    vel.z = velocityZ * inverseSpeed;

    float speed = inverseSpeed * speedSquared;
    if (maxSpeed > 0.0f) {
      speed = RetailMin(maxSpeed, inverseSpeed * speedSquared);
    }
    const float scaledY = speed * vel.y;
    const float scaledZ = speed * vel.z;
    vel.x = speed * normalizedX;
    vel.y = scaledY;
    vel.z = scaledZ;

    p1.y = (step * scaledY) + p1.y;
    p1.z = (step * scaledZ) + p1.z;
    p1.x = (step * (speed * normalizedX)) + p1.x;

    if (speed < 0.0000099999997f) {
      vel.y = 0.0f;
      vel.z = 0.0f;
      vel.x = 0.0f;
    }
    if (length < 0.0000099999997f) {
      p1 = p0;
    }
  }

  if (hasPMin) {
    const float oldY = p1.y;
    p1.x = RetailMax(pMin.x, p1.x);
    const float newY = RetailMax(pMin.y, oldY);
    const float oldZ = p1.z;
    p1.y = newY;
    p1.z = RetailMax(pMin.z, oldZ);
  }
  if (hasPMax) {
    const float oldY = p1.y;
    p1.x = RetailMin(pMax.x, p1.x);
    const float newY = RetailMin(pMax.y, oldY);
    const float oldZ = p1.z;
    p1.y = newY;
    p1.z = RetailMin(pMax.z, oldZ);
  }
  if (fixDenormals) {
    p1.FixDenormals();
  }
}

void idConeConstraint_Quat::SetLimitAngle(idRadians angle) {
  limitAngleDot = idMath::Cos(static_cast<float>(angle));
  const float halfAngle = static_cast<float>(angle) * 0.5f;
  halfLimitAngleCosine = idMath::Cos(halfAngle);
  float wrappedHalfAngle = halfAngle;
  if (halfAngle < 0.0f || halfAngle >= 6.2831855f) {
    wrappedHalfAngle =
        halfAngle - (::floorf(halfAngle * 0.15915494f) * 6.2831855f);
  }
  halfLimitAngleSine = ::sinf(wrappedHalfAngle);
}

template <>
void idSpring<idVec3>::SetConstants(float springConstant, float damping) {
  if (springConstant > K_MAX) {
    idLib::Warning("%s - k=%0.2f exceeds K_MAX=%0.2f",
                   "idSpring<class idVec3>::SetConstants", springConstant,
                   K_MAX);
    springConstant = K_MAX;
  }
  k = springConstant;
  if (damping < 0.0f) {
    const float criticalSquared = springConstant * m;
    c = _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
            _mm_set_ss(criticalSquared),
            idMath::vector_float_smallestNonDenorm))) *
        2.0f;
  } else {
    c = damping;
  }
}

template <>
void* idListArrayResize<jointTransform_t>(
    void* oldPointer, int oldCount, int newCount, memTag_t tag,
    bool zeroBuffer) {
  jointTransform_t* replacement = nullptr;
  if (newCount <= 0) {
    idMem::Singleton().Free(oldPointer, ALIGN_16);
    return replacement;
  }

  replacement = static_cast<jointTransform_t*>(
      idMem::Singleton().AllocWithLocation(
          zeroBuffer
              ? "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(27) : tag"
              : "w:\\build\\tech5\\shared\\idlib\\containers\\List.h(29) : tag",
          sizeof(jointTransform_t) * static_cast<std::size_t>(newCount), tag,
          zeroBuffer, ALIGN_16, HEAP_DEFAULTHEAP));
  if (replacement == nullptr) {
    return nullptr;
  }

  const int copyCount = oldCount < newCount ? oldCount : newCount;
  auto* const oldValues = static_cast<jointTransform_t*>(oldPointer);
  for (int index = 0; index < copyCount; ++index) {
    replacement[index] = oldValues[index];
  }
  idMem::Singleton().Free(oldPointer, ALIGN_16);
  return replacement;
}

template <typename Type, int Tag>
static __forceinline bool ResizePonytailList(
    idList<Type, Tag>& values, int newSize) {
  if (newSize <= 0) {
    if (values.listStatic == 0 || values.listStatic == 2) {
      if (values.list != nullptr) {
        idMem::Singleton().Free(values.list, ALIGN_16);
      }
      values.list = nullptr;
      values.size = 0;
    }
    values.num = 0;
    return true;
  }

  if (newSize == values.size || values.listStatic != 0) {
    return true;
  }

  if (values.size > 0 || newSize == values.granularity) {
    reinterpret_cast<idScenePoint*>(&values)
        ->idScenePoint::ActivateTargets(
            reinterpret_cast<idEntity*>(static_cast<std::intptr_t>(newSize)));
  }

  values.list = static_cast<Type*>(idListArrayResize<Type>(
      values.list, values.size, newSize,
      static_cast<memTag_t>(values.memTag), false));
  if (values.list == nullptr) {
    return false;
  }
  values.size = newSize;
  if (values.num > newSize) {
    values.num = newSize;
  }
  return true;
}

template <>
__declspec(noinline) bool
idList<idPonytailSegment, 30>::Resize(int newSize) {
  return ResizePonytailList(*this, newSize);
}

template <>
__declspec(noinline) bool idList<jointTransform_t, 5>::Resize(int newSize) {
  return ResizePonytailList(*this, newSize);
}

bool idAnimator_Ponytail::InternalIsContributing() const {
  return isInitialized;
}

const idMD6Branch* idAnimator_Ponytail::InternalGetMergeBranch() const {
  return &mergeBranch;
}

idMD6Branch* idAnimator_Ponytail::InternalGetMergeBranch() {
  return &mergeBranch;
}

idAnimator_Ponytail::idAnimator_Ponytail()
    : declPonytail(nullptr), parentAnimator(nullptr), joints(), mergeBranch(),
      pauseLeaf(), playLeaf(), lerpBranch(), segments(), ptEndSpring(),
      isInitialized(false) {}

idAnimator_Ponytail::~idAnimator_Ponytail() noexcept(false) {
  joints.ClearFree();
  segments.ClearFree();
}

bool idAnimator_Ponytail::InternalInit(const idAnimatorParms_Base& parms) {
  const idDeclMD6* const declaration = parms.animStack->jointAnimator->decl;
  pauseLeaf.Init(nullptr, 0.0f, idMD6Leaf::WRAP_CLAMP,
                 MD6_WEIGHTGROUP_ALL);

  const idMD6Skel* const skeleton = declaration->model->skeleton;
  const idMD6Anim* const defaultAnimation =
      skeleton != nullptr ? skeleton->defaultAnim : nullptr;
  playLeaf.Init(defaultAnimation, 0, 0, 0, 1.0f,
                idMD6Leaf::WRAP_REPEAT, MD6_WEIGHTGROUP_ALL);
  lerpBranch.Init(&playLeaf, &pauseLeaf, idMD6Blend::BOP_REF_LERP,
                  idMD6Blend::ORIGINBLEND_BRANCH, 1.0f, 1.0f, 0.0f,
                  MD6_WEIGHTGROUP_ALL, idMD6Branch::BLEND_LINEAR);
  mergeBranch.right = &lerpBranch;
  isInitialized = false;

  const auto& ponytailParms =
      static_cast<const idAnimatorParms_Ponytail&>(parms);
  declPonytail = ponytailParms.declPonytail;
  parentAnimator = ponytailParms.parentAnimator;

  const int jointCount = declPonytail->jointInfo.num;
  segments.SetNum(jointCount);
  joints.SetNum(jointCount);

  for (int index = 0; index < jointCount; ++index) {
    const idDeclPonytail::idPonytailJointInfo& jointInfo =
        declPonytail->jointInfo.list[index];
    const idRadians maxAngle(idMath::M_DEG2RAD * jointInfo.limitAngle);
    const idConeConstraint_Quat constraint(quat_identity, maxAngle);
    segments.list[index] = idPonytailSegment(vec3_origin, constraint);

    const idMD6Model* const model = declaration->model;
    if (model != nullptr && model->skeleton != nullptr) {
      joints.list[index] =
          model->skeleton->GetJointIndex(jointInfo.jointName.c_str());
    } else {
      joints.list[index] = idJointIndex_t(static_cast<std::int16_t>(-1));
    }
  }
  return true;
}

void idAnimator_Ponytail::InternalPreBlendTree(
    const idAnimStack*, const int curTime, const int) {
  idTreeAnimator* const animator = parentAnimator;
  if (animator == nullptr) {
    return;
  }

  const int jointCount = declPonytail->jointInfo.num;
  idStaticList<jointTransform_t, 16> jointTrans;
  jointTrans.SetNum(jointCount);
  for (int index = 0; index < jointCount; ++index) {
    idMat3 modelAxis;
    animator->GetModelSpaceJointTransform(
        ANIMATION_POSE_CURRENT, joints.list[index],
        jointTrans.list[index].msOrigin, modelAxis);
    jointTrans.list[index].msQuat = modelAxis.ToQuat();
  }

  const idDeclPonytail::idPonytailSpringInfo& springInfo =
      declPonytail->springInfo.list[0];

  if (isInitialized) {
    const float dt =
        gametimeManager.gameTimeManager->GetRealMsPerFrame(GAMETIME_SCALED) *
        0.001f;
    ptEndSpring.m = springInfo.mass;
    ptEndSpring.SetConstants(springInfo.tightness, springInfo.damping);
    ptEndSpring.maxSpeed = 2500.0f;
    ptEndSpring.p0 =
        ModelToWorld(*animator, jointTrans.list[springInfo.ptJointIndex].msOrigin);
    ptEndSpring.vel.x = (dt * 0.0f) + ptEndSpring.vel.x;
    ptEndSpring.vel.y = (dt * 0.0f) + ptEndSpring.vel.y;
    ptEndSpring.vel.z = ((springInfo.gravityMag * -1.0f) * dt) +
                        ptEndSpring.vel.z;
    ptEndSpring.Update(dt);

    const float deltaZ = ptEndSpring.p1.z - ptEndSpring.p0.z;
    const float deltaY = ptEndSpring.p1.y - ptEndSpring.p0.y;
    const float deltaX = ptEndSpring.p1.x - ptEndSpring.p0.x;
    const float distanceSquared =
        ((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ);
    if (distanceSquared > springInfo.maxLength * springInfo.maxLength) {
      const float inverseDistance = InverseLength(distanceSquared);
      const float maxLength = springInfo.maxLength;
      ptEndSpring.p1.x =
          (maxLength * (deltaX * inverseDistance)) + ptEndSpring.p0.x;
      ptEndSpring.p1.y =
          ((inverseDistance * deltaY) * maxLength) + ptEndSpring.p0.y;
      ptEndSpring.p1.z =
          ((inverseDistance * deltaZ) * maxLength) + ptEndSpring.p0.z;
    }

    if (pt_debug.valueInteger != 0) {
      DebugPoint(idColor::colorBlue, ptEndSpring.p0, 4.0f, 0, false);
      DebugPoint(idColor::colorCyan, ptEndSpring.p1, 4.0f, 0, false);
    }

    const idVec3 target = WorldToModel(*animator, ptEndSpring.p1);
    for (int index = 0; index < jointCount; ++index) {
      idPonytailSegment& segment = segments.list[index];
      const idPonytailSegment* previous = nullptr;
      if (index > 0) {
        previous = &segments.list[index - 1];
        const idVec3 forward =
            previous->coneConstraintQ.current.ToForward();
        segment.origin.x =
            (previous->len * forward.x) + previous->origin.x;
        segment.origin.y =
            (previous->len * forward.y) + previous->origin.y;
        segment.origin.z =
            (previous->len * forward.z) + previous->origin.z;
        segment.coneConstraintQ.center =
            previous->coneConstraintQ.current;
      }

      const float directionZ = target.z - segment.origin.z;
      const float directionY = target.y - segment.origin.y;
      const float directionX = target.x - segment.origin.x;
      const float directionSquared =
          ((directionX * directionX) + (directionY * directionY)) +
          (directionZ * directionZ);
      const float inverseDirection = InverseLength(directionSquared);
      const idVec3 direction{directionX * inverseDirection,
                             inverseDirection * directionY,
                             inverseDirection * directionZ};
      segment.coneConstraintQ.RotateTo(
          direction, declPonytail->jointInfo.list[index].lerpRate);

      if (pt_debug.valueInteger != 0) {
        const idVec3 worldOrigin = ModelToWorld(*animator, segment.origin);
        const idMat3 worldAxis =
            segment.coneConstraintQ.current.ToMat3() * animator->g.axis;
        DebugAxis(worldOrigin, worldAxis, 0.5f, 0, false);
        if (previous != nullptr) {
          const idVec3 previousWorld =
              ModelToWorld(*animator, previous->origin);
          GetGameLibRenderDebug()->DebugArrow(
              idColor::colorRed, previousWorld, worldOrigin, 0.5f, 0,
              false);
        }
      }
    }
  } else {
    for (int index = 0; index < jointCount; ++index) {
      idPonytailSegment& segment = segments.list[index];
      const idDeclPonytail::idPonytailJointInfo& jointInfo =
          declPonytail->jointInfo.list[index];
      idQuat center;
      if (index == jointCount - 1) {
        center = index != 0
                     ? segments.list[index - 1].coneConstraintQ.center
                     : jointTrans.list[index].msQuat;
        segment.len = 0.0f;
      } else {
        const idVec3 delta{
            jointTrans.list[index + 1].msOrigin.x -
                jointTrans.list[index].msOrigin.x,
            jointTrans.list[index + 1].msOrigin.y -
                jointTrans.list[index].msOrigin.y,
            jointTrans.list[index + 1].msOrigin.z -
                jointTrans.list[index].msOrigin.z,
        };
        center = delta.ToMat3().ToQuat();
        segment.len = LengthClamped(delta);
      }

      segment.localAxis =
          InverseMultiply(center, jointTrans.list[index].msQuat);
      segment.origin = jointTrans.list[index].msOrigin;
      segment.coneConstraintQ.center = center;
      segment.coneConstraintQ.SetLimitAngle(
          idRadians(idMath::M_DEG2RAD * jointInfo.limitAngle));
    }

    ptEndSpring.k = 1.0f;
    ptEndSpring.m = 1.0f;
    ptEndSpring.c = 2.0f;
    ptEndSpring.p0 = {0.0f, 0.0f, 0.0f};
    ptEndSpring.p1 = {0.0f, 0.0f, 0.0f};
    ptEndSpring.vel = {0.0f, 0.0f, 0.0f};
    ptEndSpring.maxSpeed = 0.0f;
    ptEndSpring.pMin = {0.0f, 0.0f, 0.0f};
    ptEndSpring.hasPMin = false;
    ptEndSpring.pMax = {0.0f, 0.0f, 0.0f};
    ptEndSpring.hasPMax = false;
    ptEndSpring.m = springInfo.mass;
    ptEndSpring.SetConstants(springInfo.tightness, springInfo.damping);
    ptEndSpring.maxSpeed = 2500.0f;
    ptEndSpring.p0 =
        ModelToWorld(*animator, jointTrans.list[springInfo.ptJointIndex].msOrigin);
    ptEndSpring.p1 = ptEndSpring.p0;
    isInitialized = true;
  }

  if (pt_modifyJoints.valueInteger != 0) {
    pauseLeaf.SetNumAnimMods(joints.num);
    idMD6Blend::jointMod_t* const mods =
        pauseLeaf.animMods[pauseLeaf.currentDeferred & 1].list;
    for (int index = 0; index < joints.num; ++index) {
      const idPonytailSegment& segment = segments.list[index];
      const idQuat rotation =
          Multiply(segment.coneConstraintQ.current, segment.localAxis);
      const idMat3 axis = rotation.ToMat3();
      SetJointModFromOriginAxis(mods[index], joints.list[index], 0x2bu,
                                segment.origin, axis);
    }
    lerpBranch.currentAlpha = 0.99989998f;
    mergeBranch.left = nullptr;
    mergeBranch.right = &lerpBranch;
  } else {
    pauseLeaf.SetNumAnimMods(0);
    mergeBranch.right = &playLeaf;
  }
  playLeaf.startTime = curTime;
}
