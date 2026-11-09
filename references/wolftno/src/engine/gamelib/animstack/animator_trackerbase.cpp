#define WOLFTNO_RETAIL_TRACKER_LEAF_DTOR_UNWIND_ABI
#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#include "shared/idlib/csystems/cvarsystemlocal.h"
#pragma warning(push)
#pragma warning(disable : 4099 4324)
#include "engine/models/renderworld.h"
#define WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#include "engine/gamelib/animstack/animator_trackerbase.h"

#include "engine/gamelib/animstack/animstack.h"
#include "engine/gamelib/animstack/animweb/animwebutils.h"
#include "engine/models/serializer.h"
#include "engine/models/skeletalanimation/jointanimator.h"
#include "shared/idlib/color.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"
#pragma warning(pop)

#undef WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#undef WOLFTNO_RETAIL_TRACKER_LEAF_DTOR_UNWIND_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI

#include <cmath>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <xmmintrin.h>

#define TRACKER_HAS_FLAG(flag) ((trackerFlags & (flag)) != 0)
#define TRACKER_SET_FLAG(flag, enabled)                                      \
  trackerFlags = static_cast<std::uint8_t>(                                 \
      (trackerFlags & ~static_cast<std::uint8_t>(flag)) |                   \
      ((enabled) ? static_cast<std::uint8_t>(flag) : 0u))

bool AssertFailed(const char* file, int line, const char* expression,
                  bool inlineBreak);
idRenderDebug* GetGameLibRenderDebug();

namespace {

struct headTrackJointABI_t {
  idJointIndex_t jointIdx;
  std::uint16_t padding;
  float pitchScale;
  float yawScale;
};

struct headTrackGroupABI_t {
  std::uint8_t name[8];
  idList<headTrackJointABI_t, 5> trackJoints;
  idJointIndex_t focusJointIdx;
  idJointIndex_t orientationJointIdx;
  int blendTime;
};

static_assert(sizeof(headTrackJointABI_t) == 0x0C,
              "head-track joint ABI drift");
static_assert(offsetof(headTrackJointABI_t, pitchScale) == 0x04,
              "head-track pitch scale ABI drift");
static_assert(offsetof(headTrackJointABI_t, yawScale) == 0x08,
              "head-track yaw scale ABI drift");
static_assert(sizeof(headTrackGroupABI_t) == 0x28,
              "head-track group ABI drift");
static_assert(offsetof(headTrackGroupABI_t, trackJoints) == 0x08,
              "head-track joint-list ABI drift");
static_assert(offsetof(headTrackGroupABI_t, focusJointIdx) == 0x20,
              "head-track focus-joint ABI drift");

idCVar anim_tracker_disable(
    "anim_tracker_disable", "0", CVAR_BOOL, "1 = show 3D debug info",
    static_cast<argCompletionRef_t>(nullptr));
idCVar anim_debugTorsoTracker(
    "anim_debugTorsoTracker", "0", CVAR_INTEGER,
    "1 = show 3D debug info", static_cast<argCompletionRef_t>(nullptr));
idCVar anim_torsoTrackerZeroPitch(
    "anim_torsoTrackerZeroPitch", "0", CVAR_BOOL,
    "1 = clear torso tracker pitch each frame",
    static_cast<argCompletionRef_t>(nullptr));
idCVar anim_torsoTrackerZeroYaw(
    "anim_torsoTrackerZeroYaw", "0", CVAR_BOOL,
    "1 = clear torso tracker yaw each frame",
    static_cast<argCompletionRef_t>(nullptr));
idCVar anim_torsoApplyOffsetToEndJointOnly(
    "anim_torsoApplyOffsetToEndJointOnly", "0", CVAR_BOOL,
    "1 = apply extra offset in torso trackers only to the end joint in the head tracking group",
    static_cast<argCompletionRef_t>(nullptr));

const idList<headTrackGroupABI_t, 5>& HeadTrackGroups(
    const idDeclMD6* declaration) {
  return *reinterpret_cast<const idList<headTrackGroupABI_t, 5>*>(
      reinterpret_cast<const std::uint8_t*>(declaration) + 0x320);
}

const headTrackGroupABI_t* HeadTrackGroup(const idDeclMD6* declaration,
                                          int index) {
  const idList<headTrackGroupABI_t, 5>& groups =
      HeadTrackGroups(declaration);
  if (index < 0 || index >= groups.num) {
    return nullptr;
  }
  return &groups.list[index];
}

float InverseLength(float lengthSquared) {
  const __m128 squared = _mm_set_ss(lengthSquared);
  const __m128 root = _mm_sqrt_ps(
      _mm_max_ps(squared, idMath::vector_float_smallestNonDenorm));
  return 1.0f / _mm_cvtss_f32(root);
}

void NormalizeRetail(idVec3& vector) {
  const float lengthSquared =
      ((vector.x * vector.x) + (vector.y * vector.y)) +
      (vector.z * vector.z);
  const float inverseLength = InverseLength(lengthSquared);
  vector.x *= inverseLength;
  vector.y *= inverseLength;
  vector.z *= inverseLength;
}

float DotRetail(const idVec3& left, const idVec3& right) {
  return ((left.x * right.x) + (left.y * right.y)) +
         (left.z * right.z);
}

float ACosClamped(float value) {
  if (value <= -1.0f) {
    return idMath::PI;
  }
  if (value >= 1.0f) {
    return 0.0f;
  }
  return ::acosf(value);
}

idMat3 Transpose(const idMat3& matrix) {
  idMat3 result;
  result.mat[0] = {matrix.mat[0].x, matrix.mat[1].x,
                   matrix.mat[2].x};
  result.mat[1] = {matrix.mat[0].y, matrix.mat[1].y,
                   matrix.mat[2].y};
  result.mat[2] = {matrix.mat[0].z, matrix.mat[1].z,
                   matrix.mat[2].z};
  return result;
}

idMat3 AxisForDirection(const idVec3& direction,
                        const idMat3& referenceAxis) {
  idMat3 axis;
  axis.mat[0] = direction;
  axis.mat[1] = referenceAxis.mat[2].Cross(direction);
  NormalizeRetail(axis.mat[1]);
  axis.mat[2] = direction.Cross(axis.mat[1]);
  NormalizeRetail(axis.mat[2]);
  return axis;
}

void SerializeFlag(idSerializer& serializer, std::uint8_t& flags,
                   std::uint8_t mask) {
  bool value = (flags & mask) != 0;
  if (serializer.writing) {
    serializer.msg->WriteBits(value, 1);
  } else {
    value = serializer.msg->ReadBits(1) == 1;
  }
  flags = static_cast<std::uint8_t>(
      (flags & ~mask) | (value ? mask : 0u));
}

void SerializeInt(idSerializer& serializer, int& value) {
  if (serializer.writing) {
    serializer.msg->WriteBits(value, 32);
  } else {
    value = serializer.msg->ReadBits(32);
  }
}

void SerializeFloat(idSerializer& serializer, float& value) {
  int bits;
  std::memcpy(&bits, &value, sizeof(bits));
  if (serializer.writing) {
    serializer.msg->WriteBits(bits, 32);
  } else {
    bits = serializer.msg->ReadBits(32);
    std::memcpy(&value, &bits, sizeof(value));
  }
}

} // namespace

idAnimatorParms_TrackerBase::idAnimatorParms_TrackerBase(
    idAnimStack* const animStack_, const char* const name_,
    const idMD6Blend::originBlend_t originBlend_,
    const md6WeightGroup_t weightGroup_,
    const md6WeightGroup_t filterGroup_, const float alpha_,
    const bool projectReferenceOrientation_,
    idTreeAnimator* const treeAnimator_)
    : idAnimatorParms_Base(animStack_, name_, idMD6Blend::BOP_REF_LERP,
                           originBlend_, weightGroup_, filterGroup_, alpha_),
      treeAnimator(treeAnimator_),
      projectReferenceOrientation(projectReferenceOrientation_) {}

idAnimator_TrackerBase::idAnimator_TrackerBase()
    : parentTracker(nullptr), treeAnimator(nullptr), focusPoint(vec3_origin),
      focusPoints{vec3_origin, vec3_origin}, yawRight(-90.0f),
      yawLeft(90.0f), pitchUp(-90.0f), pitchDown(90.0f),
      trackGroupIndex(-1), mergeBranch(), leaf(),
      msDirToFocus{1.0f, 0.0f, 0.0f}, wsFocusJointPos(vec3_origin),
      msReferenceJointPos(vec3_origin), idealOffsetAngles(ang_zero),
      curOffsetAngles(ang_zero), lastAngles(ang_zero), lastValidTime(-1),
      trackerFlags(0) {}

idAnimator_TrackerBase::~idAnimator_TrackerBase() {
  parentTracker = nullptr;
}

bool idAnimator_TrackerBase::InternalInit(
    const idAnimatorParms_Base& parms) {
  leaf.Init(nullptr, 0.0f, idMD6Leaf::WRAP_CLAMP,
            MD6_WEIGHTGROUP_ALL);
  const auto& trackerParms =
      static_cast<const idAnimatorParms_TrackerBase&>(parms);
  TRACKER_SET_FLAG(PROJECT_REFERENCE_ORIENTATION,
                   trackerParms.projectReferenceOrientation);
  TRACKER_SET_FLAG(CLIENT_SNAP_FOCUS_POINT, true);
  treeAnimator = trackerParms.treeAnimator;
  return true;
}

void idAnimator_TrackerBase::SetPitchConstraints(
    const idTypesafeNumber<float, DegreesUnique_t> up,
    const idTypesafeNumber<float, DegreesUnique_t> down) {
  pitchUp = up;
  pitchDown = down;
}

void idAnimator_TrackerBase::SetYawConstraints(
    const idTypesafeNumber<float, DegreesUnique_t> right,
    const idTypesafeNumber<float, DegreesUnique_t> left) {
  yawRight = right;
  yawLeft = left;
}

void idAnimator_TrackerBase::GetUnconstrainedAngleDeltas(
    const idAnimator_TrackerBase* const frameParentTracker,
    idTreeAnimator* const frameTreeAnimator, idAngles& angleDeltas,
    idAngles& angles, idVec3& cross) {
  (void)cross;

  idMat3 worldReferenceAxis;
  GetWorldSpaceFrameOfReference(frameParentTracker, frameTreeAnimator,
                                wsFocusJointPos, worldReferenceAxis);

  const idVec3 worldEndPosition = focusPoint;
  const idMat3& directionReferenceAxis =
      TRACKER_HAS_FLAG(USE_TRACK_GROUP_AS_REFERENCE)
          ? worldReferenceAxis
          : frameTreeAnimator->g.axis;
  const idVec3 worldDirection = worldEndPosition - wsFocusJointPos;
  msDirToFocus.x = DotRetail(worldDirection,
                             directionReferenceAxis.mat[0]);
  msDirToFocus.y = DotRetail(worldDirection,
                             directionReferenceAxis.mat[1]);
  msDirToFocus.z = DotRetail(worldDirection,
                             directionReferenceAxis.mat[2]);
  NormalizeRetail(msDirToFocus);

  idMat3 modelReferenceAxis;
  GetModelSpaceFrameOfReference(frameParentTracker, frameTreeAnimator,
                                msReferenceJointPos,
                                modelReferenceAxis);

  if (TRACKER_HAS_FLAG(USE_EXPERIMENTAL_MODE)) {
    const idMat3 currentAxis =
        AxisForDirection(msDirToFocus, modelReferenceAxis);
    angles = (Transpose(modelReferenceAxis) * currentAxis).ToAngles();
  } else if (!TRACKER_HAS_FLAG(ENABLE_CONSTRAINTS)) {
    idVec3 forward = modelReferenceAxis.mat[0];
    if (TRACKER_HAS_FLAG(PROJECT_REFERENCE_ORIENTATION)) {
      const idVec3 up{0.0f, 0.0f, 1.0f};
      forward -= up * DotRetail(forward, up);
      NormalizeRetail(forward);
    }

    if (DotRetail(forward, msDirToFocus) < 0.99999f) {
      const idMat3 currentAxis =
          AxisForDirection(msDirToFocus, modelReferenceAxis);
      const idAngles targetAngles = currentAxis.ToAngles();
      const idAngles currentAngles = modelReferenceAxis.ToAngles();
      angles.pitch = targetAngles.pitch - currentAngles.pitch;
      angles.yaw = targetAngles.yaw - currentAngles.yaw;
      angles.roll = targetAngles.roll - currentAngles.roll;
    }
  } else {
    idVec3 forward{msDirToFocus.x, msDirToFocus.y, 0.0f};
    NormalizeRetail(forward);
    const idVec3 side{-forward.y, forward.x, 0.0f};

    idVec3 referenceForward{modelReferenceAxis.mat[0].x,
                            modelReferenceAxis.mat[0].y, 0.0f};
    NormalizeRetail(referenceForward);

    const float forwardDot = DotRetail(forward, referenceForward);
    const float sideDot = DotRetail(side, referenceForward);
    float yawRadians;
    if (forwardDot < 0.0f) {
      if (sideDot > 0.0f) {
        yawRadians = (-0.5f * idMath::PI) - ACosClamped(sideDot);
      } else {
        yawRadians = (1.5f * idMath::PI) - ACosClamped(sideDot);
      }
    } else {
      yawRadians = ACosClamped(sideDot) - (0.5f * idMath::PI);
    }

    idVec3 pitchDirection = msDirToFocus;
    pitchDirection -= side * DotRetail(pitchDirection, side);
    NormalizeRetail(pitchDirection);
    const float pitchDot =
        DotRetail(side.Cross(pitchDirection), forward);

    angleDeltas.pitch =
        (ACosClamped(pitchDot) - (0.5f * idMath::PI)) *
        idMath::M_RAD2DEG;
    angleDeltas.yaw = yawRadians * idMath::M_RAD2DEG;
    angleDeltas.roll = 0.0f;
  }

  if (anim_debugTorsoTracker.valueInteger == 1) {
    GetGameLibRenderDebug()->DebugPoint(
        reinterpret_cast<const idVec4&>(idColor::colorCyan), focusPoint,
        0, true);
    GetGameLibRenderDebug()->DebugArrow(
        reinterpret_cast<const idVec4&>(idColor::colorYellow),
        wsFocusJointPos, worldEndPosition, 4.0f, 0, true);
  }
}

const idJointIndex_t
idAnimator_TrackerBase::GetReferenceJointOverride() const {
  return idJointIndex_t(static_cast<std::int16_t>(-1));
}

bool idAnimator_TrackerBase::InternalIsContributing() const {
  return leaf.animMods[0].num > 0;
}

void idAnimator_TrackerBase::Update(
    const idAnimator_TrackerBase* const parentTracker_,
    const idVec3& focusPoint_, const idAngles& offsetAngles_,
    const idTypesafeNumber<float, DegreesUnique_t> pitchUp_,
    const idTypesafeNumber<float, DegreesUnique_t> pitchDown_,
    const idTypesafeNumber<float, DegreesUnique_t> yawRight_,
    const idTypesafeNumber<float, DegreesUnique_t> yawLeft_) {
  focusPoint = focusPoint_;
  idealOffsetAngles = offsetAngles_;
  parentTracker = parentTracker_;
  pitchDown = pitchDown_;
  pitchUp = pitchUp_;
  yawLeft = yawLeft_;
  yawRight = yawRight_;
  InternalUpdate(parentTracker_);
}

void idAnimator_TrackerBase::SerializeSnapshot(idSerializer& serializer) {
  SerializeFlag(serializer, trackerFlags,
                PROJECT_REFERENCE_ORIENTATION);
  SerializeFlag(serializer, trackerFlags, ENABLE_CONSTRAINTS);

  focusPoints[0] = focusPoints[1];
  focusPoints[1] = focusPoint;
  serializer.Serialize(&focusPoints[1]);
  SerializeInt(serializer, trackGroupIndex);
  SerializeFloat(serializer, idealOffsetAngles.pitch);
  SerializeFloat(serializer, idealOffsetAngles.yaw);
  SerializeFloat(serializer, idealOffsetAngles.roll);
  SerializeFloat(serializer, pitchUp.value);
  SerializeFloat(serializer, pitchDown.value);
  SerializeFloat(serializer, yawLeft.value);
  SerializeFloat(serializer, yawRight.value);

  if (AssertFailed(
          "w:\\build\\tech5\\engine\\gamelib\\animstack\\Animator_TrackerBase.cpp",
          423, "0", true)) {
    __debugbreak();
  }

  idAnimatorParms_Base torsoChannelParms(
      serializeProps.animStack, "tracker", idMD6Blend::BOP_REF_LERP,
      idMD6Blend::ORIGINBLEND_BRANCH, MD6_WEIGHTGROUP_TORSO,
      MD6_WEIGHTGROUP_TORSO, 1.0f);
  if (!idAnimator_Base::Init(gametimeManager.gameTimeManager,
                             torsoChannelParms)) {
    idLib::Warning("Failed to initialize idAnimator_TrackerBase.");
  }
  focusPoints[0] = focusPoints[1];
  focusPoint = focusPoints[1];
}

void idAnimator_TrackerBase::InternalDoTracking(
    const idAnimStack* const stack, const float deltaLerpScale) {
  if (anim_tracker_disable.valueInteger != 0) {
    mergeBranch.right = nullptr;
    leaf.SetNumAnimMods(0);
    return;
  }
  const idDeclMD6* const declaration = stack->jointAnimator->decl;
  const headTrackGroupABI_t* const group =
      HeadTrackGroup(declaration, trackGroupIndex);
  if (group == nullptr || group->focusJointIdx.value < 0) {
    mergeBranch.right = nullptr;
    leaf.SetNumAnimMods(0);
    return;
  }

  idAngles angles = ang_zero;
  idAngles angleDeltas = ang_zero;
  float finalPitch;
  float finalYaw;
  float finalRoll;
  float constrainedPitch = angleDeltas.pitch;
  float constrainedYaw = angleDeltas.yaw;

  if (!TRACKER_HAS_FLAG(LOCK_TRACKING)) {
    const idAngles targetAngles = idealOffsetAngles;
    curOffsetAngles.pitch =
        LerpToWithScale(curOffsetAngles.pitch, targetAngles.pitch, 0.1f);
    curOffsetAngles.yaw =
        LerpToWithScale(curOffsetAngles.yaw, targetAngles.yaw, 0.1f);
    curOffsetAngles.roll =
        LerpToWithScale(curOffsetAngles.roll, targetAngles.roll, 0.1f);

    idVec3 cross;
    GetUnconstrainedAngleDeltas(parentTracker, treeAnimator, angleDeltas,
                                angles, cross);
    if (TRACKER_HAS_FLAG(ENABLE_CONSTRAINTS)) {
      constrainedPitch = angleDeltas.pitch;
      if (constrainedPitch < pitchUp.value) {
        constrainedPitch = pitchUp.value;
        TRACKER_SET_FLAG(PITCH_CLAMPED, true);
      } else if (constrainedPitch > pitchDown.value) {
        constrainedPitch = pitchDown.value;
        TRACKER_SET_FLAG(PITCH_CLAMPED, true);
      } else {
        TRACKER_SET_FLAG(PITCH_CLAMPED, false);
      }

      constrainedYaw = angleDeltas.yaw;
      if (constrainedYaw < yawRight.value) {
        constrainedYaw = yawRight.value;
        TRACKER_SET_FLAG(YAW_CLAMPED, true);
      } else if (constrainedYaw > yawLeft.value) {
        constrainedYaw = yawLeft.value;
        TRACKER_SET_FLAG(YAW_CLAMPED, true);
      } else {
        TRACKER_SET_FLAG(YAW_CLAMPED, false);
      }

      if (anim_torsoTrackerZeroPitch.valueInteger != 0) {
        constrainedPitch = 0.0f;
      }
      if (anim_torsoTrackerZeroYaw.valueInteger != 0) {
        constrainedYaw = 0.0f;
      }

      finalPitch = angles.pitch;
      finalYaw = angles.yaw;
      finalRoll = angles.roll;
      lastAngles.pitch = constrainedPitch;
      lastAngles.yaw = constrainedYaw;
      lastAngles.roll = angleDeltas.roll;
    } else {
      finalPitch = LerpToWithScale(lastAngles.pitch, angles.pitch,
                                   deltaLerpScale);
      finalYaw = LerpToWithScale(lastAngles.yaw, angles.yaw,
                                 deltaLerpScale);
      finalRoll = angles.roll;
      lastAngles.pitch = finalPitch;
      lastAngles.yaw = finalYaw;
      lastAngles.roll = finalRoll;
      constrainedPitch = angleDeltas.pitch;
      constrainedYaw = angleDeltas.yaw;
    }
  } else {
    finalPitch = lastAngles.pitch;
    finalYaw = lastAngles.yaw;
    finalRoll = lastAngles.roll;
  }

  leaf.SetNumAnimMods(group->trackJoints.num);
  ++leaf.currentDeferred;
  leaf.SetUseStackTop(true);
  leaf.SetClearNonModWeights(true);
  idList<idMD6Blend::jointMod_t, 30>& mods =
      leaf.animMods[leaf.currentDeferred & 1];

  for (int index = 0; index < group->trackJoints.num; ++index) {
    const headTrackJointABI_t& trackJoint =
        group->trackJoints.list[index];
    idAngles jointAngles;
    if (TRACKER_HAS_FLAG(ENABLE_CONSTRAINTS)) {
      jointAngles.pitch = constrainedPitch * trackJoint.pitchScale;
      jointAngles.yaw = constrainedYaw * trackJoint.yawScale;
      jointAngles.roll = 0.0f;
    } else {
      jointAngles.pitch = finalPitch * trackJoint.yawScale;
      jointAngles.yaw = finalYaw * trackJoint.yawScale;
      jointAngles.roll = finalRoll * trackJoint.yawScale;
    }

    idMat3 jointAxis = jointAngles.ToMat3();
    if ((curOffsetAngles.pitch != ang_zero.pitch ||
         curOffsetAngles.yaw != ang_zero.yaw ||
         curOffsetAngles.roll != ang_zero.roll) &&
        (anim_torsoApplyOffsetToEndJointOnly.valueInteger == 0 ||
         trackJoint.jointIdx.value == group->focusJointIdx.value)) {
      idAngles offsetAngles;
      offsetAngles.pitch = curOffsetAngles.pitch * trackJoint.pitchScale;
      offsetAngles.yaw = curOffsetAngles.yaw * trackJoint.yawScale;
      offsetAngles.roll = curOffsetAngles.roll * trackJoint.yawScale;
      jointAxis *= offsetAngles.ToMat3();
    }

    idMD6Blend::jointMod_t& mod = mods.list[index];
    mod.mat[0] = jointAxis.mat[0].x;
    mod.mat[1] = jointAxis.mat[1].x;
    mod.mat[2] = jointAxis.mat[2].x;
    mod.mat[3] = vec3_origin.x;
    mod.mat[4] = jointAxis.mat[0].y;
    mod.mat[5] = jointAxis.mat[1].y;
    mod.mat[6] = jointAxis.mat[2].y;
    mod.mat[7] = vec3_origin.y;
    mod.mat[8] = jointAxis.mat[0].z;
    mod.mat[9] = jointAxis.mat[1].z;
    mod.mat[10] = jointAxis.mat[2].z;
    mod.mat[11] = vec3_origin.z;
    mod.s[0] = 0.0f;
    mod.s[1] = 0.0f;
    mod.s[2] = 0.0f;
    mod.joint = trackJoint.jointIdx;
    mod.flags = 0x8003u;
  }

  mergeBranch.right = &leaf;
  mergeBranch.left = nullptr;
  mergeBranch.filterGroup = MD6_WEIGHTGROUP_TORSO;
  parentTracker = nullptr;
}

void idAnimator_TrackerBase::InternalPreBlendTree(
    const idAnimStack* const stack, const int curTime, int) {
  if (idAnimator_Base::IsContributing()) {
    lastValidTime = curTime;
  }
  InternalDoTracking(stack, 1.0f);
}

void idAnimator_TrackerBase::PreBlendSnapshot(
    idAnimStack* const stack, int, int, const float fraction) {
  bool snapFocusPoint = false;
  if (!idAnimator_Base::IsContributing() ||
      !stack->IsAnimatorFlagSet(
          this, idAnimStack::ANIMATORFLAG_CONTRIBUTED_LAST_SERIALIZE)) {
    snapFocusPoint = true;
  }

  if (snapFocusPoint || TRACKER_HAS_FLAG(CLIENT_SNAP_FOCUS_POINT)) {
    focusPoints[0] = focusPoints[1];
    focusPoint = focusPoints[1];
    TRACKER_SET_FLAG(CLIENT_SNAP_FOCUS_POINT, snapFocusPoint);
  } else {
    focusPoint.x =
        ((focusPoints[1].x - focusPoints[0].x) * fraction) +
        focusPoints[0].x;
    focusPoint.y =
        ((focusPoints[1].y - focusPoints[0].y) * fraction) +
        focusPoints[0].y;
    focusPoint.z =
        ((focusPoints[1].z - focusPoints[0].z) * fraction) +
        focusPoints[0].z;
  }
  InternalDoTracking(serializeProps.animStack, 1.0f);
}

#undef TRACKER_SET_FLAG
#undef TRACKER_HAS_FLAG
