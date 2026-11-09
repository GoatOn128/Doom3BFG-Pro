#define WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI
#define WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#define WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#include "shared/idlib/csystems/cvarsystemlocal.h"

#define WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#define WOLFTNO_RETAIL_TRACKER_LEAF_DTOR_UNWIND_ABI
#include "engine/gamelib/animstack/animator_ik.h"

#include "engine/framework/gametimemanager.h"
#include "engine/gamelib/animstack/animstack.h"
#include "engine/gamelib/physics/physics.h"
#include "engine/models/rendermodel.h"
#include "engine/models/serializer.h"
#include "engine/models/skeletalanimation/md6skel.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "engine/models/skeletalanimation/treeanimator.h"
#include "shared/idlib/geometry/tracemodel.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/math/mathlib.h"

#undef WOLFTNO_RETAIL_TRACKER_LEAF_DTOR_UNWIND_ABI
#undef WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI
#undef WOLFTNO_RETAIL_IDSTR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#undef WOLFTNO_RETAIL_IDMATH_CONSTANT_ABI
#undef WOLFTNO_RETAIL_CVAR_INLINE_FREEDATA_ABI
#undef WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <xmmintrin.h>

class idCollisionModelManager {
public:
  bool GetStepMoveResult(trace_t* result, idCollisionQuery& query,
                         bool peek);
};
extern idCollisionModelManager* collisionModelManager;
extern const idVec3 vec3_up;

idCVar ik_enable("ik_enable", "1", CVAR_BOOL, "enable IK",
                 static_cast<argCompletionRef_t>(nullptr));
idCVar ik_enableSmoothing("ik_enableSmoothing", "1", CVAR_BOOL,
                          "enable IK smoothing",
                          static_cast<argCompletionRef_t>(nullptr));
idCVar ik_debug("ik_debug", "0", CVAR_BOOL, "show IK debug lines",
                static_cast<argCompletionRef_t>(nullptr));

bool idClip::QueryIsValid(const idClipQuery& clipQuery) const {
  if (clipQuery.index < collisionQueryFirstSubmittedIndex ||
      clipQuery.index > collisionQueryLastSubmittedIndex) {
    return false;
  }
  const std::uint64_t resultOffset =
      collisionQueries[clipQuery.index & 0xfff].query.offset &
      0x3fffffffffffffULL;
  return resultOffset >= idCollisionQuery::finalResultsFirstSubmittedOffset &&
         resultOffset < idCollisionQuery::finalResultsLastSubmittedOffset;
}

namespace {

constexpr const char* kModelTraceName =
    "w:\\build\\tech5\\engine\\gamelib\\animstack\\Animator_IK.cpp(606) : Translation";
constexpr const char* kLegTraceName =
    "w:\\build\\tech5\\engine\\gamelib\\animstack\\Animator_IK.cpp(693) : Translation";
constexpr const char* kWaistTraceName =
    "w:\\build\\tech5\\engine\\gamelib\\animstack\\Animator_IK.cpp(775) : Translation";
constexpr const char* kFootTraceName =
    "w:\\build\\tech5\\engine\\gamelib\\animstack\\Animator_IK.cpp(861) : Translation";

__forceinline float RetailInverseLength(float lengthSquared) {
  return 1.0f /
         _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
             _mm_set_ss(lengthSquared), idMath::vector_float_smallestNonDenorm)));
}

__forceinline float RetailLengthSquared(float lengthSquared) {
  return _mm_cvtss_f32(_mm_sqrt_ps(_mm_max_ps(
      _mm_set_ss(lengthSquared), idMath::vector_float_smallestNonDenorm)));
}

__forceinline float RetailLength(const idVec3& value) {
  return RetailLengthSquared(((value.x * value.x) + (value.y * value.y)) +
                             (value.z * value.z));
}

__forceinline idVec3 TransformVector(const idMat3& axis,
                                     const idVec3& vector) {
  return {
      ((axis.mat[0].x * vector.x) + (axis.mat[1].x * vector.y)) +
          (axis.mat[2].x * vector.z),
      ((axis.mat[0].y * vector.x) + (axis.mat[1].y * vector.y)) +
          (axis.mat[2].y * vector.z),
      ((axis.mat[0].z * vector.x) + (axis.mat[1].z * vector.y)) +
          (axis.mat[2].z * vector.z),
  };
}

__forceinline idVec3 InverseTransformVector(const idMat3& axis,
                                            const idVec3& vector) {
  return {
      ((vector.y * axis.mat[0].y) + (vector.x * axis.mat[0].x)) +
          (vector.z * axis.mat[0].z),
      ((vector.y * axis.mat[1].y) + (vector.x * axis.mat[1].x)) +
          (vector.z * axis.mat[1].z),
      ((vector.y * axis.mat[2].y) + (vector.x * axis.mat[2].x)) +
          (vector.z * axis.mat[2].z),
  };
}

__forceinline idVec3 TransformPoint(const idVec3& origin,
                                    const idMat3& axis,
                                    const idVec3& point) {
  const idVec3 rotated = TransformVector(axis, point);
  return {rotated.x + origin.x, rotated.y + origin.y,
          rotated.z + origin.z};
}

__forceinline idVec3 InverseTransformPoint(const idVec3& origin,
                                           const idMat3& axis,
                                           const idVec3& point) {
  return InverseTransformVector(
      axis, {point.x - origin.x, point.y - origin.y, point.z - origin.z});
}

__forceinline idMat3 Transpose(const idMat3& value) {
  idMat3 result;
  result.mat[0] = {value.mat[0].x, value.mat[1].x, value.mat[2].x};
  result.mat[1] = {value.mat[0].y, value.mat[1].y, value.mat[2].y};
  result.mat[2] = {value.mat[0].z, value.mat[1].z, value.mat[2].z};
  return result;
}

__forceinline idVec3 JointOrigin(const idJointMat& joint) {
  return {joint.mat[3], joint.mat[7], joint.mat[11]};
}

__forceinline idMat3 JointAxis(const idJointMat& joint) {
  idMat3 result;
  result.mat[0] = {joint.mat[0], joint.mat[4], joint.mat[8]};
  result.mat[1] = {joint.mat[1], joint.mat[5], joint.mat[9]};
  result.mat[2] = {joint.mat[2], joint.mat[6], joint.mat[10]};
  return result;
}

__forceinline idVec3 NormalizeRetail(const idVec3& value) {
  const float squared =
      ((value.x * value.x) + (value.y * value.y)) + (value.z * value.z);
  const float inverse = RetailInverseLength(squared);
  return {value.x * inverse, value.y * inverse, value.z * inverse};
}

__forceinline int ParentJoint(const idMD6Skel* skeleton,
                              idJointIndex_t joint) {
  const idMD6SkelData* data = skeleton->data;
  const auto* parents = reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<const std::uint8_t*>(data) + data->parentTblOffset);
  return parents[joint.value];
}

__forceinline idJointIndex_t FindJoint(const idDeclMD6* declaration,
                                      const char* name) {
  if (declaration == nullptr || declaration->model == nullptr ||
      declaration->model->skeleton == nullptr) {
    return idJointIndex_t(static_cast<std::int16_t>(-1));
  }
  return declaration->model->skeleton->GetJointIndex(name);
}

__forceinline const idMD6Anim* PoseAnimation(const idDeclMD6* declaration) {
  const idMD6Alias* alias =
      declaration->FindAlias("misc/af_pose", true, true);
  if (alias != nullptr) {
    return alias->mappings.list[0].anim;
  }
  const idMD6Skel* skeleton = declaration->model->skeleton;
  return skeleton != nullptr ? skeleton->defaultAnim : nullptr;
}

__forceinline int FootstepIndex(const idDeclWalkIK* declaration,
                                footStepType_t footstep) {
  if (declaration == nullptr) {
    return -1;
  }
  for (int index = 0; index < declaration->legJoints.num; ++index) {
    if (declaration->legJoints.list[index].footstep == footstep) {
      return index;
    }
  }
  return -1;
}

void SetJointModFromOriginAxis(idMD6Blend::jointMod_t& mod,
                               idJointIndex_t joint, std::uint16_t flags,
                               const idVec3& origin, const idMat3& axis) {
  mod.joint = joint;
  mod.flags = flags;
  mod.mat[0] = axis.mat[0].x;
  mod.mat[1] = axis.mat[1].x;
  mod.mat[2] = axis.mat[2].x;
  mod.mat[3] = origin.x;
  mod.mat[4] = axis.mat[0].y;
  mod.mat[5] = axis.mat[1].y;
  mod.mat[6] = axis.mat[2].y;
  mod.mat[7] = origin.y;
  mod.mat[8] = axis.mat[0].z;
  mod.mat[9] = axis.mat[1].z;
  mod.mat[10] = axis.mat[2].z;
  mod.mat[11] = origin.z;
  mod.s[0] = 0.0f;
  mod.s[1] = 0.0f;
  mod.s[2] = 0.0f;
}

} // namespace

idAnimator_IK::idAnimator_IK()
    : enabled(true), ikInitialized(false), entityNum(0x1fff),
      animator(nullptr), clip(nullptr) {}

idAnimator_IK::~idAnimator_IK() {
  animator = nullptr;
  clip = nullptr;
}

void idAnimator_IK::PreBlendSnapshot(idAnimStack*, int, int, float) {
  Evaluate();
}

bool idAnimator_IK::SolveTwoBones(const idVec3& startPos, idVec3& endPos,
                                  const idVec3& direction, float len0,
                                  float len1, idVec3& jointPos) {
  const float deltaX = endPos.x - startPos.x;
  const float deltaY = endPos.y - startPos.y;
  const float deltaZ = endPos.z - startPos.z;
  const float distanceSquared =
      ((deltaY * deltaY) + (deltaX * deltaX)) + (deltaZ * deltaZ);
  const float inverseDistance = RetailInverseLength(distanceSquared);
  const float distance = inverseDistance * distanceSquared;

  if (distance >= len0 + len1) {
    const float unitX = deltaX * inverseDistance;
    const float unitY = deltaY * inverseDistance;
    const float unitZ = deltaZ * inverseDistance;
    jointPos.x = startPos.x + unitX * len0;
    jointPos.y = startPos.y + unitY * len0;
    jointPos.z = startPos.z + unitZ * len0;
    endPos.x = jointPos.x + unitX * len1;
    endPos.y = jointPos.y + unitY * len1;
    endPos.z = jointPos.z + unitZ * len1;
    return false;
  }

  if (std::fabs(len0 - len1) >= distance) {
    jointPos.x = startPos.x + deltaX * 0.5f;
    jointPos.y = startPos.y + deltaY * 0.5f;
    jointPos.z = startPos.z + deltaZ * 0.5f;
    return false;
  }

  const float unitX = deltaX * inverseDistance;
  const float unitY = deltaY * inverseDistance;
  const float unitZ = deltaZ * inverseDistance;
  const float projection =
      ((direction.y * unitY) + (direction.x * unitX)) +
      (direction.z * unitZ);
  const float planeX = direction.x - unitX * projection;
  const float planeY = direction.y - unitY * projection;
  const float planeZ = direction.z - unitZ * projection;
  const float planeSquared =
      ((planeY * planeY) + (planeX * planeX)) + (planeZ * planeZ);
  const float inversePlane = RetailInverseLength(planeSquared);
  const float along =
      (((distance * distance + len0 * len0) - len1 * len1) *
       (inverseDistance * 0.5f));
  const float across = RetailLengthSquared(
      std::fabs((len0 * len0) - (along * along)));
  jointPos.x =
      (startPos.x + unitX * along) + across * (planeX * inversePlane);
  jointPos.y = (startPos.y + unitY * along) + planeY * inversePlane * across;
  jointPos.z = (startPos.z + unitZ * along) + planeZ * inversePlane * across;
  return true;
}

float idAnimator_IK::GetBoneAxis(const idVec3& startPos,
                                 const idVec3& endPos,
                                 const idVec3& direction, idMat3& axis) {
  const float deltaX = endPos.x - startPos.x;
  const float deltaY = endPos.y - startPos.y;
  const float deltaZ = endPos.z - startPos.z;
  const float squared =
      ((deltaX * deltaX) + (deltaY * deltaY)) + (deltaZ * deltaZ);
  const float inverse = RetailInverseLength(squared);
  axis.mat[0] = {deltaX * inverse, deltaY * inverse, deltaZ * inverse};

  const float projection =
      ((axis.mat[0].x * direction.x) +
       (axis.mat[0].y * direction.y)) +
      (axis.mat[0].z * direction.z);
  const idVec3 second = {
      direction.x - axis.mat[0].x * projection,
      direction.y - axis.mat[0].y * projection,
      direction.z - axis.mat[0].z * projection,
  };
  const float secondSquared =
      ((second.x * second.x) + (second.y * second.y)) +
      (second.z * second.z);
  const float secondInverse = RetailInverseLength(secondSquared);
  axis.mat[1] = {second.x * secondInverse, second.y * secondInverse,
                 second.z * secondInverse};
  axis.mat[2].x = axis.mat[1].y * axis.mat[0].z -
                  axis.mat[1].z * axis.mat[0].y;
  axis.mat[2].y = axis.mat[1].z * axis.mat[0].x -
                  axis.mat[0].z * axis.mat[1].x;
  axis.mat[2].z = axis.mat[0].y * axis.mat[1].x -
                  axis.mat[1].y * axis.mat[0].x;
  return inverse * squared;
}

idAnimator_WalkIK::idAnimator_WalkIK()
    : decl(nullptr), clipModel(nullptr), clipMask(1), upNormal{0.0f, 0.0f, 1.0f},
      footModel(nullptr), createdClipModel(false), numLegs(0),
      enabledLegs(0), footJoints{}, ankleJoints{}, kneeJoints{}, hipJoints{},
      dirJoints{}, waistJointHandle(static_cast<std::int16_t>(-1)),
      hipForward{}, kneeForward{}, upperLegLength{},
      lowerLegLength{}, upperLegToHipJoint{}, lowerLegToKneeJoint{},
      kneeJointTransform{}, pivotFoot(-1), pivotYaw(0.0f), pivotPos{},
      onGround(false), onStairs(false), isDead(false), oldHeightsValid(false),
      deadKeepLastMods(false), oldWaistHeight(0.0f),
      oldAnkleHeights{}, oldGroundPhysicsId(-1), oldGroundPhysicsOrigin{},
      surfaceTypes{}, modelQuery{}, waistQuery{}, legQueries{}, footQueries{},
      lastModelSpaceFootOrigin{}, groundTraceDist(1.0f), internalRadiusXY(0.0f),
      animLeaf(), mergeBranch() {
  for (int index = 0; index < MAX_LEGS; ++index) {
    footJoints[index].value = -1;
    ankleJoints[index].value = -1;
    kneeJoints[index].value = -1;
    hipJoints[index].value = -1;
    dirJoints[index].value = -1;
    upperLegToHipJoint[index] = mat3_identity;
    lowerLegToKneeJoint[index] = mat3_identity;
    kneeJointTransform[index] = mat3_identity;
  }
}

idAnimator_WalkIK::~idAnimator_WalkIK() {
  if (footModel != nullptr) {
    footModel->Delete();
  }
  if (createdClipModel && clipModel != nullptr) {
    clipModel->Delete();
  }
}

void idAnimator_WalkIK::EnableLeg(int leg) { enabledLegs |= 1 << leg; }

void idAnimator_WalkIK::DisableLeg(int leg) { enabledLegs &= ~(1 << leg); }

void idAnimator_WalkIK::EnableAll() {
  oldHeightsValid = false;
  enabledLegs = (1 << numLegs) - 1;
  modelQuery.index = 0;
  waistQuery.index = 0;
  for (idClipQuery& query : legQueries) {
    query.index = 0;
  }
  enabled = true;
}

void idAnimator_WalkIK::DisableAll() {
  enabledLegs = 0;
  oldHeightsValid = false;
  modelQuery.index = 0;
  waistQuery.index = 0;
  for (idClipQuery& query : legQueries) {
    query.index = 0;
  }
}

bool idAnimator_WalkIK::IsActive() const {
  return ikInitialized && ik_enable.valueInteger != 0 && decl != nullptr &&
         animator != nullptr && enabled && enabledLegs != 0;
}

bool idAnimator_WalkIK::InternalIsContributing() const {
  return IsActive() || deadKeepLastMods;
}

idJointIndex_t idAnimator_WalkIK::GetFootJoint(
    footStepType_t footstep) const {
  const int index = FootstepIndex(decl, footstep);
  if (index >= 0) {
    return footJoints[index];
  }
  idLib::Warning("idAnimator_WalkIK::GetFootJoint: no leg found for %d",
                 footstep);
  return footJoints[0];
}

int idAnimator_WalkIK::GetFootSurfaceType(footStepType_t footstep) const {
  const int index = FootstepIndex(decl, footstep);
  if (index >= 0) {
    return surfaceTypes[index];
  }
  if (decl != nullptr) {
    idLib::Warning(
        "idAnimator_WalkIK::GetFootSurfaceType: no leg found for %d",
        footstep);
    return surfaceTypes[0];
  }
  return 0;
}

const idVec3& idAnimator_WalkIK::GetFootPosition(
    footStepType_t footstep) const {
  const int index = FootstepIndex(decl, footstep);
  if (index >= 0) {
    return lastModelSpaceFootOrigin[index];
  }
  idLib::Warning("idAnimator_WalkIK::GetFootPosition: no leg found for %d",
                 footstep);
  return lastModelSpaceFootOrigin[0];
}

bool idAnimator_WalkIK::InternalInit(const idAnimatorParms_Base& baseParms) {
  const auto& parms =
      static_cast<const idAnimatorParms_WalkIK&>(baseParms);
  decl = parms.declWalkIK;
  animator = parms.treeAnimator;
  entityNum = parms.entityNum;
  clip = parms.clip;
  clipModel = parms.clipModel;
  clipMask = parms.clipMask;
  upNormal = parms.upNormal;
  if (clipModel != nullptr) {
    internalRadiusXY = clipModel->bounds.GetInternalRadiusXY();
  }

  const idDeclMD6* modelDecl =
      parms.animStack->jointAnimator != nullptr
          ? parms.animStack->jointAnimator->decl
          : nullptr;
  if (modelDecl == nullptr || decl == nullptr || animator == nullptr) {
    return false;
  }

  numLegs = std::min(decl->legJoints.num, MAX_LEGS);
  if (numLegs == 0) {
    return true;
  }

  const int jointCount = modelDecl->NumJoints();
  auto* joints = static_cast<idJointMat*>(
      _alloca(static_cast<std::size_t>(jointCount) * sizeof(idJointMat)));
  const idMD6Anim* pose = PoseAnimation(modelDecl);
  const bool removeOrigin = animator->GetRemoveOriginTranslation();
  const idVec3 visualOffset = animator->GetVisualOffset();
  if (!modelDecl->GetJointsForAnimTime(joints, pose, 0, visualOffset,
                                       removeOrigin)) {
    idLib::Error("IK animator '%s' is missing pose anim '%s'",
                 baseParms.name.c_str(), pose->name.str);
  }

  enabledLegs = 0;
  for (int leg = 0; leg < numLegs; ++leg) {
    const idIKLeg& legInfo = decl->legJoints.list[leg];
    footJoints[leg] = FindJoint(modelDecl, legInfo.foot.str);
    if (footJoints[leg].value == -1) {
      idLib::Warning("idAnimator_WalkIK::Init: invalid foot joint '%s'",
                     legInfo.foot.str);
      return false;
    }
    ankleJoints[leg] = FindJoint(modelDecl, legInfo.ankle.str);
    if (ankleJoints[leg].value == -1) {
      idLib::Warning("idAnimator_WalkIK::Init: invalid ankle joint '%s'",
                     legInfo.ankle.str);
      return false;
    }
    kneeJoints[leg] = FindJoint(modelDecl, legInfo.knee.str);
    if (kneeJoints[leg].value == -1) {
      idLib::Warning("idAnimator_WalkIK::Init: invalid knee joint '%s'",
                     legInfo.knee.str);
      return false;
    }
    hipJoints[leg] = FindJoint(modelDecl, legInfo.hip.str);
    if (hipJoints[leg].value == -1) {
      idLib::Warning("idAnimator_WalkIK::Init: invalid hip joint '%s'",
                     legInfo.hip.str);
      return false;
    }
    dirJoints[leg] = FindJoint(modelDecl, legInfo.forward.str);
    enabledLegs |= 1 << leg;
  }

  waistJointHandle = FindJoint(modelDecl, decl->waistJoint.str);
  if (waistJointHandle.value == -1) {
    idLib::Warning("idAnimator_WalkIK::Init: invalid waist joint '%s'",
                   decl->waistJoint.str);
    return false;
  }

  const idMD6Skel* skeleton = modelDecl->model->skeleton;
  for (int leg = 0; leg < numLegs; ++leg) {
    oldAnkleHeights[leg] = 0.0f;
    const idVec3 ankleOrigin = JointOrigin(joints[ankleJoints[leg].value]);
    const idVec3 kneeOrigin = JointOrigin(joints[kneeJoints[leg].value]);
    const idVec3 hipOrigin = JointOrigin(joints[hipJoints[leg].value]);
    const idMat3 kneeAxis = JointAxis(joints[kneeJoints[leg].value]);
    const idMat3 hipAxis = JointAxis(joints[hipJoints[leg].value]);
    lastModelSpaceFootOrigin[leg] = JointOrigin(joints[footJoints[leg].value]);

    idVec3 hipDirection;
    idVec3 kneeDirection;
    if (dirJoints[leg].value != -1) {
      kneeDirection =
          JointOrigin(joints[dirJoints[leg].value]) - kneeOrigin;
      hipDirection = kneeDirection;
    } else if (decl->dirsFromBasePose) {
      const int hipParent = ParentJoint(skeleton, hipJoints[leg]);
      hipDirection = hipOrigin - JointOrigin(joints[hipParent]);
      const float hipSquared = hipDirection.LengthSqr();
      if (hipSquared < 0.0001f) {
        idLib::Error(
            "idAnimator_WalkIK: hip joint (%d) and hip parent joint (%d) "
            "are in the same position",
            hipJoints[leg].value, static_cast<std::int16_t>(hipParent));
      }
      hipDirection = NormalizeRetail(hipDirection);
      const idVec3 hipToKnee = kneeOrigin - hipOrigin;
      const float vertical = hipToKnee * vec3_up;
      kneeDirection = hipToKnee - vec3_up * vertical;
      if (kneeDirection.LengthSqr() < 0.0001f) {
        idLib::Error(
            "idAnimator_WalkIK: knee joint (%d) and hip joint (%d) are in "
            "the same position",
            kneeJoints[leg].value, hipJoints[leg].value);
      }
      kneeDirection = NormalizeRetail(kneeDirection);
    } else {
      const idVec3& legDirection = decl->legJoints.list[leg].kneeDir;
      kneeDirection = legDirection.LengthSqr() <= 0.0f ? decl->kneeDir
                                                       : legDirection;
      hipDirection = kneeDirection;
    }

    hipForward[leg] = InverseTransformVector(hipAxis, hipDirection);
    kneeForward[leg] = InverseTransformVector(kneeAxis, kneeDirection);

    idMat3 boneAxis;
    upperLegLength[leg] =
        GetBoneAxis(hipOrigin, kneeOrigin, hipDirection, boneAxis);
    upperLegToHipJoint[leg] = hipAxis * Transpose(boneAxis);
    lowerLegLength[leg] =
        GetBoneAxis(kneeOrigin, ankleOrigin, kneeDirection, boneAxis);
    lowerLegToKneeJoint[leg] = kneeAxis * Transpose(boneAxis);
  }

  if (decl->footSize > 0.0f) {
    const float size = decl->footSize;
    const idVec3 vertices[4] = {{size, size, 0.0f},
                                {-size, size, 0.0f},
                                {-size, -size, 0.0f},
                                {size, -size, 0.0f}};
    idTraceModel traceModel;
    traceModel.SetupPolygon(vertices, 4);
    footModel = new idClipModel(clip, &traceModel, 1, nullptr);
  }
  ikInitialized = true;
  return true;
}

void idAnimator_WalkIK::SerializeSnapshot(idSerializer& serializer) {
  const idDeclWalkIK* oldDecl = decl;
  if (serializer.writing) {
    idDeclWalkIK::resourceList.WriteResourceToMsg(decl, *serializer.msg);
  } else {
    decl = static_cast<const idDeclWalkIK*>(
        idDeclWalkIK::resourceList.ReadResourceFromMsg(*serializer.msg));
  }

  if (serializer.writing) {
    serializer.msg->WriteBits(entityNum, 32);
  } else {
    entityNum = serializer.msg->ReadBits(32);
  }

  if (!serializer.writing && oldDecl != decl) {
    const idBounds bounds{{{-16.0f, -16.0f, 0.0f},
                           {16.0f, 16.0f, 90.0f}}};
    idTraceModel traceModel;
    traceModel.SetupBox(bounds);
    if (clipModel != nullptr) {
      clipModel->Delete();
    }
    clipModel = new idClipModel(serializeProps.clip, &traceModel, 1, nullptr);
    createdClipModel = true;
    idAnimatorParms_WalkIK parms(
        decl, -1, serializeProps.clip, clipModel, 257, vec3_up,
        serializeProps.animStack, "walkIK", 1.0f,
        serializeProps.treeAnimator);
    idAnimator_Base::Init(gametimeManager.gameTimeManager, parms);
  }

  int serializedLegs = 0;
  int mask = 1;
  for (int leg = 0; leg < numLegs; ++leg, mask *= 2) {
    bool legEnabled = (enabledLegs & mask) != 0;
    if (serializer.writing) {
      serializer.msg->WriteBits(legEnabled, 1);
    } else {
      legEnabled = serializer.msg->ReadBits(1) == 1;
    }
    if (legEnabled) {
      serializedLegs |= mask;
    }
  }
  if (!serializer.writing && enabledLegs != serializedLegs) {
    oldHeightsValid = false;
    modelQuery.index = 0;
    waistQuery.index = 0;
    for (idClipQuery& query : legQueries) {
      query.index = 0;
    }
  }
  enabledLegs = serializedLegs;
}

idAnimator_ReachIK::idAnimator_ReachIK()
    : decl(nullptr), numArms(0), enabledArms(0), handJoints{}, elbowJoints{},
      shoulderJoints{}, targetTransforms{}, shoulderForward{}, elbowForward{},
      upperArmLength{}, lowerArmLength{}, upperArmToShoulderJoint{},
      lowerArmToElbowJoint{} {
  for (int arm = 0; arm < MAX_ARMS; ++arm) {
    handJoints[arm].value = -1;
    elbowJoints[arm].value = -1;
    shoulderJoints[arm].value = -1;
    upperArmToShoulderJoint[arm] = mat3_identity;
    lowerArmToElbowJoint[arm] = mat3_identity;
  }
}

idAnimator_ReachIK::~idAnimator_ReachIK() { decl = nullptr; }

void idAnimator_ReachIK::SetTargetOrigin(int arm, const idVec3& targetPos) {
  targetTransforms[arm].origin = targetPos;
  targetTransforms[arm].validOrigin = true;
}

void idAnimator_ReachIK::SetTargetAxis(int arm, const idMat3& targetAxis) {
  targetTransforms[arm].axis = targetAxis;
  targetTransforms[arm].validAxis = true;
}

bool idAnimator_ReachIK::InternalInit(
    const idAnimatorParms_Base& baseParms) {
  const auto& parms =
      static_cast<const idAnimatorParms_ReachIK&>(baseParms);
  decl = parms.declReachIK;
  animator = parms.treeAnimator;
  entityNum = parms.entityNum;
  clip = parms.clip;
  const idDeclMD6* modelDecl =
      parms.animStack->jointAnimator != nullptr
          ? parms.animStack->jointAnimator->decl
          : nullptr;
  if (modelDecl == nullptr || animator == nullptr || decl == nullptr) {
    return false;
  }
  numArms = std::min(decl->armInfo.num, MAX_ARMS);
  if (numArms == 0) {
    return true;
  }

  const int jointCount = modelDecl->NumJoints();
  auto* joints = static_cast<idJointMat*>(
      _alloca(static_cast<std::size_t>(jointCount) * sizeof(idJointMat)));
  const idMD6Anim* pose = PoseAnimation(modelDecl);
  const bool removeOrigin = animator->GetRemoveOriginTranslation();
  const idVec3 visualOffset = animator->GetVisualOffset();
  if (!modelDecl->GetJointsForAnimTime(joints, pose, 0, visualOffset,
                                       removeOrigin)) {
    return false;
  }

  enabledArms = 0;
  for (int arm = 0; arm < numArms; ++arm) {
    const idIKArm& info = decl->armInfo.list[arm];
    handJoints[arm] = FindJoint(modelDecl, info.hand.c_str());
    if (handJoints[arm].value == -1) {
      idLib::Warning("idAnimator_ReachIK::Init: invalid hand joint '%s'",
                     info.hand.c_str());
      return false;
    }
    elbowJoints[arm] = FindJoint(modelDecl, info.elbow.c_str());
    if (elbowJoints[arm].value == -1) {
      idLib::Warning("idAnimator_ReachIK::Init: invalid elbow joint '%s'",
                     info.elbow.c_str());
      return false;
    }
    shoulderJoints[arm] = FindJoint(modelDecl, info.shoulder.c_str());
    if (shoulderJoints[arm].value == -1) {
      idLib::Warning(
          "idAnimator_ReachIK::Init: invalid shoulder joint '%s'",
          info.shoulder.c_str());
      return false;
    }
    enabledArms |= 1 << arm;
  }

  const idVec3 direction{-1.0f, 0.0f, 0.0f};
  for (int arm = 0; arm < numArms; ++arm) {
    const idVec3 handOrigin = JointOrigin(joints[handJoints[arm].value]);
    const idVec3 elbowOrigin = JointOrigin(joints[elbowJoints[arm].value]);
    const idVec3 shoulderOrigin =
        JointOrigin(joints[shoulderJoints[arm].value]);
    const idMat3 elbowAxis = JointAxis(joints[elbowJoints[arm].value]);
    const idMat3 shoulderAxis = JointAxis(joints[shoulderJoints[arm].value]);
    shoulderForward[arm] = InverseTransformVector(shoulderAxis, direction);
    elbowForward[arm] = InverseTransformVector(elbowAxis, direction);
    idMat3 boneAxis;
    upperArmLength[arm] =
        GetBoneAxis(shoulderOrigin, elbowOrigin, direction, boneAxis);
    upperArmToShoulderJoint[arm] = shoulderAxis * Transpose(boneAxis);
    lowerArmLength[arm] =
        GetBoneAxis(elbowOrigin, handOrigin, direction, boneAxis);
    lowerArmToElbowJoint[arm] = elbowAxis * Transpose(boneAxis);
  }
  ikInitialized = true;
  return true;
}

idMat3 idAnimator_ReachIK::ModelToJointAxis(animationPose_t pose,
                                            idJointIndex_t joint,
                                            const idMat3& modelSpaceAxis) {
  if (animator == nullptr) {
    return mat3_identity;
  }
  const int parent = ParentJoint(animator->decl->model->skeleton, joint);
  idMat3 parentAxis = mat3_identity;
  if (parent >= 0) {
    animator->GetModelSpaceJointAxis(
        pose, idJointIndex_t(static_cast<std::int16_t>(parent)), parentAxis);
  }
  parentAxis.InverseSelf();
  idMat3 result = modelSpaceAxis * parentAxis;
  result.OrthoNormalizeSelf();
  return result;
}

idMat3 idAnimator_ReachIK::ModelToJointAxis(
    animationPose_t pose, idJointIndex_t joint,
    const idMat3& modelSpaceAxis, idJointIndex_t parentJoint,
    const idMat3& parentModelAxis) {
  if (animator == nullptr) {
    return mat3_identity;
  }
  const int jointParent = ParentJoint(animator->decl->model->skeleton, joint);
  idMat3 oldParentAxis;
  animator->GetModelSpaceJointAxis(pose, parentJoint, oldParentAxis);
  oldParentAxis.InverseSelf();
  const idMat3 delta = oldParentAxis * parentModelAxis;
  idMat3 parentAxis;
  animator->GetModelSpaceJointAxis(
      pose, idJointIndex_t(static_cast<std::int16_t>(jointParent)),
      parentAxis);
  parentAxis *= delta;
  parentAxis.OrthoNormalizeSelf();
  parentAxis.InverseSelf();
  idMat3 result = modelSpaceAxis * parentAxis;
  result.OrthoNormalizeSelf();
  return result;
}

void idAnimator_ReachIK::SerializeSnapshot(idSerializer& serializer) {
  const idDeclReachIK* oldDecl = decl;
  if (serializer.writing) {
    idDeclReachIK::resourceList.WriteResourceToMsg(decl, *serializer.msg);
  } else {
    decl = static_cast<const idDeclReachIK*>(
        idDeclReachIK::resourceList.ReadResourceFromMsg(*serializer.msg));
  }
  if (oldDecl != decl) {
    idAnimatorParms_ReachIK parms(
        decl, -1, serializeProps.clip, serializeProps.animStack, "reachIK",
        1.0f, serializeProps.treeAnimator);
    idAnimator_Base::Init(gametimeManager.gameTimeManager, parms);
  }

  for (targetTransform_t& target : targetTransforms) {
    bool validTag = target.validTag;
    if (serializer.writing) {
      serializer.msg->WriteBits(validTag, 1);
    } else {
      validTag = serializer.msg->ReadBits(1) == 1;
    }
    target.validTag = validTag;
    if (!validTag) {
      continue;
    }
    serializer.Serialize(target.selfTag.trans);
    float* quaternion[4] = {&target.selfTag.rot.w, &target.selfTag.rot.x,
                            &target.selfTag.rot.y, &target.selfTag.rot.z};
    for (float* component : quaternion) {
      int bits;
      if (serializer.writing) {
        std::memcpy(&bits, component, sizeof(bits));
        serializer.msg->WriteBits(bits, 32);
      } else {
        bits = serializer.msg->ReadBits(32);
        std::memcpy(component, &bits, sizeof(bits));
      }
    }

    std::uint16_t jointIndex = 0xffff;
    if (serializer.writing) {
      if (animator != nullptr) {
        jointIndex = static_cast<std::uint16_t>(
            animator->GetJointIndex(
                idJointHandle_t(target.selfTag.parentJoint.value))
                .value);
      } else {
        jointIndex = 0;
      }
    }
    if (jointIndex == static_cast<std::uint16_t>(NULL_JOINT_INDEX.value)) {
      jointIndex = 0xffff;
    }
    if (serializer.writing) {
      serializer.msg->WriteBits(jointIndex, 16);
    } else {
      jointIndex = static_cast<std::uint16_t>(serializer.msg->ReadBits(16));
    }
    if (!serializer.writing) {
      if (animator != nullptr) {
        const idJointIndex_t index(
            static_cast<std::int16_t>(jointIndex == 0xffff ? -1
                                                           : jointIndex));
        target.selfTag.parentJoint.value = animator->GetJointHandle(index).value;
      } else {
        target.selfTag.parentJoint.value = 0;
      }
    }
  }
}

idAnimator_HandIK::idAnimator_HandIK() : decl(nullptr), evalParams{} {}

void idAnimator_HandIK::SetTarget(int arm, const idVec3& targetPos,
                                  const idMat3& targetRot, bool useAsOffset,
                                  bool slave, float slaveBlend) {
  handsIKEvalParams_t& params = evalParams[arm];
  params.isInitialized = true;
  params.targetOrigin = targetPos;
  params.targetAxis = targetRot;
  params.targetIsOffset = useAsOffset;
  params.targetIsSlave = slave;
  params.targetSlaveBlend = slaveBlend;
}

bool idAnimator_HandIK::InternalInit(
    const idAnimatorParms_Base& baseParms) {
  const auto& parms = static_cast<const idAnimatorParms_HandIK&>(baseParms);
  decl = parms.declHandIK;
  animator = parms.treeAnimator;
  entityNum = parms.entityNum;
  clip = parms.clip;
  if (animator == nullptr) {
    return false;
  }
  const idDeclMD6* modelDecl = animator->jointAnimator->decl;
  if (modelDecl == nullptr || decl == nullptr || decl->handInfo.num != 2) {
    return false;
  }
  const int jointCount = modelDecl->NumJoints();
  auto* joints = static_cast<idJointMat*>(
      _alloca(static_cast<std::size_t>(jointCount) * sizeof(idJointMat)));
  const idMD6Skel* skeleton = modelDecl->model->skeleton;
  const idMD6Anim* defaultAnim =
      skeleton != nullptr ? skeleton->defaultAnim : nullptr;
  const bool removeOrigin = animator->GetRemoveOriginTranslation();
  const idVec3 visualOffset = animator->GetVisualOffset();
  if (!modelDecl->GetJointsForAnimTime(joints, defaultAnim, 0, visualOffset,
                                       removeOrigin)) {
    return false;
  }
  for (handsIKEvalParams_t& params : evalParams) {
    params.Clear();
  }

  for (int arm = 0; arm < NUM_ARMS; ++arm) {
    const idIKHand& info = decl->handInfo.list[arm];
    handsIKEvalParams_t& params = evalParams[arm];
    params.attachJoint = FindJoint(modelDecl, info.attach.c_str());
    if (params.attachJoint.value == -1) {
      idLib::Warning("idAnimator_HandIK::Init: invalid hand joint '%s'",
                     info.attach.c_str());
      return false;
    }
    params.handJoint = FindJoint(modelDecl, info.hand.c_str());
    if (params.handJoint.value == -1) {
      idLib::Warning("idAnimator_HandIK::Init: invalid hand joint '%s'",
                     info.hand.c_str());
      return false;
    }
    params.forearmJoint = FindJoint(modelDecl, info.foreArm.c_str());
    if (params.forearmJoint.value == -1) {
      idLib::Warning("idAnimator_HandIK::Init: invalid forearm joint '%s'",
                     info.foreArm.c_str());
      return false;
    }
    params.elbowJoint = FindJoint(modelDecl, info.elbow.c_str());
    if (params.elbowJoint.value == -1) {
      idLib::Warning("idAnimator_HandIK::Init: invalid elbow joint '%s'",
                     info.elbow.c_str());
      return false;
    }
    params.shoulderJoint = FindJoint(modelDecl, info.shoulder.c_str());
    if (params.shoulderJoint.value == -1) {
      idLib::Warning("idAnimator_HandIK::Init: invalid shoulder joint '%s'",
                     info.shoulder.c_str());
      return false;
    }
  }

  for (handsIKEvalParams_t& params : evalParams) {
    const idVec3 attach = JointOrigin(joints[params.attachJoint.value]);
    const idVec3 hand = JointOrigin(joints[params.handJoint.value]);
    const idVec3 forearm = JointOrigin(joints[params.forearmJoint.value]);
    const idVec3 elbow = JointOrigin(joints[params.elbowJoint.value]);
    const idVec3 shoulder = JointOrigin(joints[params.shoulderJoint.value]);
    params.handToForearmLength = RetailLength(hand - forearm);
    params.lowerArmLength = RetailLength(hand - elbow);
    params.upperArmLength = RetailLength(elbow - shoulder);
    static_cast<void>(attach);
  }
  ikInitialized = true;
  return true;
}

void idAnimator_HandIK::InternalPreBlendTree(const idAnimStack*, int, int) {
  if (animator == nullptr || !enabled || !evalParams[0].isInitialized ||
      !evalParams[1].isInitialized) {
    return;
  }

  idMD6Blend::jointMod_t* control = animator->jointAnimator->AllocJointMod();
  control->flags = static_cast<std::uint16_t>(-32671);
  const auto packJoints = [](const handsIKEvalParams_t& params) {
    const auto extend = [](idJointIndex_t joint) {
      return static_cast<std::uint32_t>(
          static_cast<std::int32_t>(joint.value));
    };
    return extend(params.handJoint) |
           ((extend(params.forearmJoint) |
             ((extend(params.elbowJoint) |
               (extend(params.shoulderJoint) << 8))
              << 8))
            << 8);
  };
  const std::uint32_t packedRight = packJoints(evalParams[0]);
  const std::uint32_t packedLeft = packJoints(evalParams[1]);
  std::memcpy(&control->s[0], &packedRight, sizeof(packedRight));
  std::memcpy(&control->s[1], &packedLeft, sizeof(packedLeft));
  std::uint32_t packedFlags = evalParams[0].targetIsSlave ? 1u : 0u;
  packedFlags |= evalParams[0].targetIsOffset ? 0x100u : 0u;
  packedFlags |= evalParams[1].targetIsSlave ? 0x10000u : 0u;
  packedFlags |= evalParams[1].targetIsOffset ? 0x1000000u : 0u;
  std::memcpy(&control->s[2], &packedFlags, sizeof(packedFlags));
  const std::uint32_t packedAttach =
      (static_cast<std::uint32_t>(
           static_cast<std::int32_t>(evalParams[0].attachJoint.value)) |
       (static_cast<std::uint32_t>(
            static_cast<std::int32_t>(evalParams[1].attachJoint.value))
        << 8))
      << 16;
  std::memcpy(&control->mat[0], &packedAttach, sizeof(packedAttach));
  control->mat[1] = evalParams[0].handToForearmLength;
  control->mat[2] = evalParams[0].lowerArmLength;
  control->mat[3] = evalParams[0].upperArmLength;
  control->mat[4] = evalParams[1].handToForearmLength;
  control->mat[5] = evalParams[1].lowerArmLength;
  control->mat[6] = evalParams[1].upperArmLength;
  control->mat[7] = evalParams[0].targetIsSlave
                        ? evalParams[0].targetSlaveBlend
                        : evalParams[1].targetSlaveBlend;
  control->joint = evalParams[0].attachJoint;

  int parameterIndex = 0;
  for (int arm = 0; arm < NUM_ARMS; ++arm) {
    const handsIKEvalParams_t& params = evalParams[arm];
    idMD6Blend::jointMod_t* target = animator->jointAnimator->AllocJointMod();
    target->flags = static_cast<std::uint16_t>(-32607);
    target->joint = params.attachJoint;
    target->s[0] = static_cast<float>(parameterIndex++);
    const idQuat rotation = params.targetAxis.ToQuat();
    target->mat[0] = rotation.x;
    target->mat[1] = rotation.y;
    target->mat[2] = rotation.z;
    target->mat[3] = rotation.w;
    target->mat[4] = params.targetOrigin.x;
    target->mat[5] = params.targetOrigin.y;
    target->mat[6] = params.targetOrigin.z;

    idMD6Blend::jointMod_t* forearm =
        animator->jointAnimator->AllocJointMod();
    forearm->flags = static_cast<std::uint16_t>(-32607);
    forearm->joint = params.forearmJoint;
    forearm->s[0] = static_cast<float>(parameterIndex++);

    idMD6Blend::jointMod_t* shoulder =
        animator->jointAnimator->AllocJointMod();
    shoulder->flags = static_cast<std::uint16_t>(-32607);
    shoulder->joint = params.shoulderJoint;
    shoulder->s[0] = static_cast<float>(parameterIndex++);
  }
}

void idAnimator_HandIK::SerializeSnapshot(idSerializer& serializer) {
  const idDeclHandIK* oldDecl = decl;
  if (serializer.writing) {
    idDeclHandIK::resourceList.WriteResourceToMsg(decl, *serializer.msg);
  } else {
    decl = static_cast<const idDeclHandIK*>(
        idDeclHandIK::resourceList.ReadResourceFromMsg(*serializer.msg));
  }
  if (oldDecl != decl) {
    idAnimatorParms_HandIK parms(
        decl, -1, serializeProps.clip, serializeProps.animStack, "handIK",
        1.0f, serializeProps.treeAnimator);
    idAnimator_Base::Init(gametimeManager.gameTimeManager, parms);
  }
}

void idAnimator_ReachIK::Evaluate() {
  if (animator == nullptr || !enabled) {
    return;
  }

  const idVec3 modelOrigin = animator->useDeferredPosition
                                 ? animator->deferredOrigin
                                 : animator->g.origin;
  const idMat3 modelAxis = animator->useDeferredPosition
                               ? animator->deferredAxis
                               : animator->g.axis;
  bool resolvedSelfTarget = false;

  for (int arm = 0; arm < numArms; ++arm) {
    targetTransform_t& target = targetTransforms[arm];
    if (!target.validTag || !target.selfTag.IsValid()) {
      continue;
    }
    const idJointIndex_t parentIndex =
        animator->GetJointIndex(idJointHandle_t(target.selfTag.parentJoint.value));
    if (parentIndex.value < 0) {
      continue;
    }
    idVec3 parentOrigin;
    idMat3 parentAxis;
    if (!animator->GetModelSpaceJointTransform(
            ANIMATION_POSE_CURRENT, parentIndex, parentOrigin, parentAxis)) {
      continue;
    }

    const idVec3 scaledTag = {
        target.selfTag.trans.x * animator->g.scale.x,
        target.selfTag.trans.y * animator->g.scale.y,
        target.selfTag.trans.z * animator->g.scale.z,
    };
    const idVec3 targetModelOrigin =
        TransformPoint(parentOrigin, parentAxis, scaledTag);
    const idMat3 targetModelAxis = target.selfTag.rot.ToMat3() * parentAxis;
    target.origin = TransformPoint(modelOrigin, modelAxis, targetModelOrigin);
    target.axis = targetModelAxis * modelAxis;
    target.validOrigin = true;
    target.validAxis = true;
    resolvedSelfTarget = true;
  }

  idMat3 shoulderModelAxes[MAX_ARMS];
  idMat3 elbowModelAxes[MAX_ARMS];
  for (int arm = 0; arm < numArms; ++arm) {
    idVec3 shoulderModelOrigin;
    idMat3 shoulderReferenceAxis;
    animator->GetModelSpaceJointTransform(
        ANIMATION_POSE_CURRENT, shoulderJoints[arm], shoulderModelOrigin,
        shoulderReferenceAxis);
    const idVec3 shoulderWorldOrigin =
        TransformPoint(modelOrigin, modelAxis, shoulderModelOrigin);
    const idVec3 shoulderModelDirection =
        TransformVector(shoulderReferenceAxis, shoulderForward[arm]);
    const idVec3 shoulderWorldDirection =
        TransformVector(modelAxis, shoulderModelDirection);

    idVec3 handModelOrigin;
    idMat3 handReferenceAxis;
    animator->GetModelSpaceJointTransform(ANIMATION_POSE_CURRENT,
                                          handJoints[arm], handModelOrigin,
                                          handReferenceAxis);
    idVec3 handWorldOrigin =
        TransformPoint(modelOrigin, modelAxis, handModelOrigin);
    if (targetTransforms[arm].validOrigin) {
      handWorldOrigin = targetTransforms[arm].origin;
    } else if (clip != nullptr) {
      trace_t trace;
      clip->Translation(&trace, shoulderWorldOrigin, handWorldOrigin, nullptr,
                        mat3_identity, 1, entityNum, false, nullptr);
      handWorldOrigin = trace.endpos;
    }

    idVec3 elbowModelOrigin;
    idMat3 elbowReferenceAxis;
    animator->GetModelSpaceJointTransform(
        ANIMATION_POSE_CURRENT, elbowJoints[arm], elbowModelOrigin,
        elbowReferenceAxis);
    const idVec3 elbowModelDirection =
        TransformVector(elbowReferenceAxis, elbowForward[arm]);
    const idVec3 elbowWorldDirection =
        TransformVector(modelAxis, elbowModelDirection);

    idVec3 elbowWorldOrigin =
        TransformPoint(modelOrigin, modelAxis, elbowModelOrigin);
    SolveTwoBones(shoulderWorldOrigin, handWorldOrigin, elbowWorldDirection,
                  upperArmLength[arm], lowerArmLength[arm],
                  elbowWorldOrigin);

    idMat3 boneWorldAxis;
    GetBoneAxis(shoulderWorldOrigin, elbowWorldOrigin,
                shoulderWorldDirection, boneWorldAxis);
    shoulderModelAxes[arm] =
        upperArmToShoulderJoint[arm] * (boneWorldAxis * Transpose(modelAxis));
    GetBoneAxis(elbowWorldOrigin, handWorldOrigin, elbowWorldDirection,
                boneWorldAxis);
    elbowModelAxes[arm] =
        lowerArmToElbowJoint[arm] * (boneWorldAxis * Transpose(modelAxis));
  }

  if (resolvedSelfTarget) {
    for (int arm = 0; arm < numArms; ++arm) {
      const idMat3 shoulderLocal = ModelToJointAxis(
          ANIMATION_POSE_CURRENT, shoulderJoints[arm],
          shoulderModelAxes[arm]);
      animator->jointAnimator->SetJointAxis(
          DRIVER_MODEL, shoulderJoints[arm], JOINTMOD_LOCAL_OVERRIDE,
          shoulderLocal);
      const idMat3 elbowLocal = ModelToJointAxis(
          ANIMATION_POSE_CURRENT, elbowJoints[arm], elbowModelAxes[arm],
          shoulderJoints[arm], shoulderModelAxes[arm]);
      animator->jointAnimator->SetJointAxis(
          DRIVER_MODEL, elbowJoints[arm], JOINTMOD_LOCAL_OVERRIDE,
          elbowLocal);
      if (targetTransforms[arm].validAxis) {
        const idMat3 targetModelAxis =
            targetTransforms[arm].axis * Transpose(modelAxis);
        const idMat3 handLocal = ModelToJointAxis(
            ANIMATION_POSE_CURRENT, handJoints[arm], targetModelAxis,
            elbowJoints[arm], elbowModelAxes[arm]);
        animator->jointAnimator->SetJointAxis(
            DRIVER_MODEL, handJoints[arm], JOINTMOD_LOCAL_OVERRIDE,
            handLocal);
      }
    }
  } else {
    for (int arm = 0; arm < numArms; ++arm) {
      if (targetTransforms[arm].validAxis) {
        animator->jointAnimator->SetJointAxis(
            DRIVER_MODEL, handJoints[arm], JOINTMOD_MODEL_OVERRIDE,
            targetTransforms[arm].axis);
      }
      animator->jointAnimator->SetJointAxis(
          DRIVER_MODEL, shoulderJoints[arm], JOINTMOD_MODEL_OVERRIDE,
          shoulderModelAxes[arm]);
      animator->jointAnimator->SetJointAxis(
          DRIVER_MODEL, elbowJoints[arm], JOINTMOD_MODEL_OVERRIDE,
          elbowModelAxes[arm]);
    }
  }
}

void idAnimator_WalkIK::Evaluate() {
  if (!ikInitialized || ik_enable.valueInteger == 0 || decl == nullptr ||
      animator == nullptr || !enabled || enabledLegs == 0 || clip == nullptr ||
      clipModel == nullptr) {
    return;
  }

  const idVec3 modelOrigin = animator->useDeferredPosition
                                 ? animator->deferredOrigin
                                 : animator->g.origin;
  const idMat3 modelAxis = animator->useDeferredPosition
                               ? animator->deferredAxis
                               : animator->g.axis;
  onGround = false;
  onStairs = false;

  if (clip->QueryIsValid(modelQuery)) {
    idCollisionQuery& query =
        clip->collisionQueries[modelQuery.index & 0xfff].query;
    modelQuery.index = 0;
    trace_t trace;
    collisionModelManager->GetStepMoveResult(&trace, query, false);
    if (trace.fraction < 1.0f) {
      onGround = true;
      if ((trace.c.surfaceFlags & 0x400) != 0) {
        onStairs = true;
      }
    }
    idPhysics* groundPhysics = idPhysics::GetPhysicsForId(trace.c.physicsId);
    if (trace.fraction < 1.0f && groundPhysics != nullptr) {
      const idVec3& groundOrigin = groundPhysics->GetOrigin(0);
      if (trace.c.physicsId == oldGroundPhysicsId) {
        const idVec3 movement = groundOrigin - oldGroundPhysicsOrigin;
        const float heightChange = movement * upNormal;
        oldWaistHeight = heightChange + oldWaistHeight;
        for (int leg = 0; leg < numLegs; ++leg) {
          oldAnkleHeights[leg] = heightChange + oldAnkleHeights[leg];
        }
      }
      oldGroundPhysicsId = trace.c.physicsId;
      oldGroundPhysicsOrigin = groundOrigin;
    } else {
      oldGroundPhysicsId = -1;
      oldGroundPhysicsOrigin = vec3_origin;
    }
  }

  const idVec3 modelTraceEnd = modelOrigin - upNormal * groundTraceDist;
  modelQuery = clip->Translation(nullptr, modelOrigin, modelTraceEnd,
                                 clipModel, mat3_identity, clipMask,
                                 entityNum, false, kModelTraceName);

  idVec3 modelSpaceFootJointOrigins[MAX_LEGS]{};
  idVec3 worldSpaceFootJointOrigins[MAX_LEGS]{};
  int lowestFoot = -1;
  float lowestHeight = 1.0e30f;
  for (int leg = 0; leg < numLegs; ++leg) {
    animator->GetModelSpaceJointPosition(ANIMATION_POSE_CURRENT,
                                         footJoints[leg],
                                         modelSpaceFootJointOrigins[leg]);
    worldSpaceFootJointOrigins[leg] =
        TransformPoint(modelOrigin, modelAxis,
                       modelSpaceFootJointOrigins[leg]);
    const float height = worldSpaceFootJointOrigins[leg] * upNormal;
    if (lowestHeight > height) {
      lowestHeight = height;
      lowestFoot = leg;
    }
  }

  if (decl->usePivot) {
    const float yaw = modelAxis.mat[0].ToYaw();
    if (lowestFoot != pivotFoot ||
        std::fabs(idMath::AngleNormalize180(yaw - pivotYaw)) > 30.0f) {
      pivotFoot = lowestFoot;
      pivotYaw = yaw;
      animator->GetModelSpaceJointPosition(DRIVER_MODEL,
                                           footJoints[pivotFoot], pivotPos);
    }
    worldSpaceFootJointOrigins[pivotFoot] =
        TransformPoint(modelOrigin, modelAxis, pivotPos);
  }

  bool allLegQueriesValid = true;
  float groundHeights[MAX_LEGS]{};
  idVec3 groundModelOrigins[MAX_LEGS]{};
  int bit = 1;
  for (int leg = 0; leg < numLegs; ++leg, bit = _rotl(bit, 1)) {
    if ((enabledLegs & bit) == 0) {
      continue;
    }
    if (!clip->QueryIsValid(legQueries[leg])) {
      allLegQueriesValid = false;
      surfaceTypes[leg] = 0;
    } else {
      idCollisionQuery& query =
          clip->collisionQueries[legQueries[leg].index & 0xfff].query;
      legQueries[leg].index = 0;
      trace_t trace;
      collisionModelManager->GetStepMoveResult(&trace, query, false);
      surfaceTypes[leg] = trace.fraction < 1.0f ? trace.c.surfaceType : 0;
      groundHeights[leg] = trace.endpos * upNormal;
      groundModelOrigins[leg] =
          InverseTransformPoint(modelOrigin, modelAxis, trace.endpos);
    }

    const float footDown = isDead ? decl->footDownTraceDead
                                  : decl->footDownTrace;
    idVec3 traceCenter = worldSpaceFootJointOrigins[leg];
    if (decl->restrictToBounds || decl->checkFeetForSolids) {
      const idVec3 towardModel = modelOrigin - traceCenter;
      const idVec3 horizontal =
          towardModel - upNormal * (towardModel * upNormal);
      const float horizontalSquared = horizontal.LengthSqr();
      const float inverseHorizontal = RetailInverseLength(horizontalSquared);
      const float horizontalLength = inverseHorizontal * horizontalSquared;
      const float maximumDistance =
          (internalRadiusXY - decl->footSize) +
          decl->restrictToBoundsTolerance;
      const float inward =
          horizontalLength - std::fmin(horizontalLength, maximumDistance);
      traceCenter += horizontal * (inverseHorizontal * inward);
    }
    const idVec3 traceStart = traceCenter + upNormal * decl->footUpTrace;
    const idVec3 traceEnd = traceCenter - upNormal * footDown;
    legQueries[leg] =
        clip->Translation(nullptr, traceStart, traceEnd, footModel,
                          mat3_identity, 257, entityNum, false, kLegTraceName);
  }

  if (!allLegQueriesValid) {
    return;
  }

  float ankleSmoothing = onStairs ? decl->stairSmoothing : decl->smoothing;
  float waistSmoothing =
      onStairs ? decl->stairWaistSmoothing : decl->waistSmoothing;
  if (ik_enableSmoothing.valueInteger == 0) {
    ankleSmoothing = 0.0f;
    waistSmoothing = 0.0f;
  }

  const float baseHeight = animator->g.origin * upNormal;
  float minimumFootShift = 1.0e30f;
  float maximumAnkleHeight = -1.0e30f;
  idVec3 modelSpaceAnkleOrigins[MAX_LEGS]{};
  idMat3 oldAnkleAxes[MAX_LEGS];
  bit = 1;
  for (int leg = 0; leg < numLegs; ++leg, bit = _rotl(bit, 1)) {
    float shift =
        onGround && (enabledLegs & bit) != 0
            ? (groundHeights[leg] - baseHeight) + decl->footShift
            : 0.0f;
    if (minimumFootShift > shift) {
      minimumFootShift = shift;
    }
    animator->GetModelSpaceJointTransform(
        ANIMATION_POSE_CURRENT, ankleJoints[leg],
        modelSpaceAnkleOrigins[leg], oldAnkleAxes[leg]);
    const idVec3 worldAnkle =
        TransformPoint(modelOrigin, modelAxis, modelSpaceAnkleOrigins[leg]);
    const float worldHeight = worldAnkle * upNormal;
    if (oldHeightsValid) {
      shift = shift - ((worldHeight + shift) - oldAnkleHeights[leg]) *
                          ankleSmoothing;
    }
    const float newHeight = worldHeight + shift;
    if (newHeight > maximumAnkleHeight) {
      maximumAnkleHeight = newHeight;
    }
    oldAnkleHeights[leg] = newHeight;
    const idVec3 modelUp = InverseTransformVector(modelAxis, upNormal);
    modelSpaceAnkleOrigins[leg] += modelUp * shift;
  }

  idVec3 waistModelOrigin;
  idMat3 waistAxis;
  animator->GetModelSpaceJointTransform(ANIMATION_POSE_CURRENT,
                                        waistJointHandle, waistModelOrigin,
                                        waistAxis);
  const idVec3 waistWorldOrigin =
      TransformPoint(modelOrigin, modelAxis, waistModelOrigin);
  idVec3 waistWorldShift =
      upNormal * (minimumFootShift + decl->waistShift);

  if (decl->minWaistFloorDist > 0.0f && waistWorldShift * upNormal < 0.0f) {
    if (clip->QueryIsValid(waistQuery)) {
      idCollisionQuery& query =
          clip->collisionQueries[waistQuery.index & 0xfff].query;
      waistQuery.index = 0;
      trace_t trace;
      collisionModelManager->GetStepMoveResult(&trace, query, false);
      const float distance =
          ((waistWorldOrigin + waistWorldShift) - trace.endpos) * upNormal;
      if (decl->minWaistFloorDist > distance) {
        waistWorldShift +=
            upNormal * (decl->minWaistFloorDist - distance);
      }
    }
    const float traceDistance = decl->footDownTrace + decl->footUpTrace;
    const idVec3 waistTraceEnd =
        waistWorldOrigin + waistWorldShift - upNormal * traceDistance;
    waistQuery = clip->Translation(nullptr, waistWorldOrigin, waistTraceEnd,
                                   footModel, modelAxis, 257, entityNum,
                                   false, kWaistTraceName);
  }

  if (decl->minWaistAnkleDist > 0.0f) {
    const float waistHeight =
        (waistWorldOrigin + waistWorldShift) * upNormal;
    const float distance = waistHeight - maximumAnkleHeight;
    if (decl->minWaistAnkleDist > distance) {
      waistWorldShift +=
          upNormal * (decl->minWaistAnkleDist - distance);
    }
  }
  const bool hadOldHeights = oldHeightsValid;
  if (hadOldHeights) {
    const float smoothed =
        (((waistWorldOrigin + waistWorldShift) * upNormal) -
         oldWaistHeight) *
        waistSmoothing;
    waistWorldShift -= upNormal * smoothed;
  }
  oldWaistHeight = (waistWorldOrigin + waistWorldShift) * upNormal;

  if (!hadOldHeights) {
    oldHeightsValid = true;
    return;
  }

  const idVec3 waistModelShift =
      InverseTransformVector(modelAxis, waistWorldShift);
  const float clipTopDistance = clipModel->bounds.b[1] * upNormal;
  const idVec3 boxTop = modelOrigin + upNormal * clipTopDistance;
  idMat3 newHipAxes[MAX_LEGS];
  idMat3 newKneeAxes[MAX_LEGS];

  for (int leg = 0; leg < numLegs; ++leg) {
    idVec3 hipOrigin;
    idMat3 hipAxis;
    idVec3 kneeOrigin;
    idMat3 kneeAxis;
    animator->GetModelSpaceJointTransform(ANIMATION_POSE_CURRENT,
                                          hipJoints[leg], hipOrigin,
                                          hipAxis);
    animator->GetModelSpaceJointTransform(ANIMATION_POSE_CURRENT,
                                          kneeJoints[leg], kneeOrigin,
                                          kneeAxis);
    hipOrigin += waistModelShift;
    const idVec3 hipDirection = TransformVector(hipAxis, hipForward[leg]);
    const idVec3 kneeDirection = TransformVector(kneeAxis, kneeForward[leg]);

    bool footBlocked = false;
    if (decl->checkFeetForSolids && clip->QueryIsValid(footQueries[leg])) {
      idCollisionQuery& query =
          clip->collisionQueries[footQueries[leg].index & 0xfff].query;
      footQueries[leg].index = 0;
      trace_t trace;
      collisionModelManager->GetStepMoveResult(&trace, query, false);
      const float resultHeight = trace.endpos * upNormal;
      const float originalHeight =
          worldSpaceFootJointOrigins[leg] * upNormal;
      if (trace.fraction < 1.0f && resultHeight - originalHeight > 2.0f) {
        const idBounds& bounds = clipModel->absBounds;
        footBlocked = trace.endpos.x < bounds.b[0].x ||
                      trace.endpos.y < bounds.b[0].y ||
                      trace.endpos.z < bounds.b[0].z ||
                      trace.endpos.x > bounds.b[1].x ||
                      trace.endpos.y > bounds.b[1].y ||
                      trace.endpos.z > bounds.b[1].z;
      }
    }
    const idVec3& baseFootOrigin =
        footBlocked ? groundModelOrigins[leg]
                    : modelSpaceAnkleOrigins[leg];
    const idVec3 delta =
        lastModelSpaceFootOrigin[leg] - baseFootOrigin;
    const float verticalDelta = delta * upNormal;
    const float smooth = decl->footSmoothing;
    idVec3 footOrigin =
        baseFootOrigin +
        (delta - upNormal * verticalDelta) * smooth;
    SolveTwoBones(hipOrigin, footOrigin, kneeDirection,
                  upperLegLength[leg], lowerLegLength[leg], kneeOrigin);
    GetBoneAxis(hipOrigin, kneeOrigin, hipDirection, hipAxis);
    GetBoneAxis(kneeOrigin, footOrigin, kneeDirection, kneeAxis);
    lastModelSpaceFootOrigin[leg] = footOrigin;
    newHipAxes[leg] = upperLegToHipJoint[leg] * hipAxis;
    newKneeAxes[leg] = lowerLegToKneeJoint[leg] * kneeAxis;

    if (decl->checkFeetForSolids) {
      footQueries[leg] = clip->Translation(
          nullptr, boxTop, worldSpaceFootJointOrigins[leg], nullptr,
          mat3_identity, 257, entityNum, false, kFootTraceName);
    }
  }

  animLeaf.Init(nullptr, 0.0f, idMD6Leaf::WRAP_CLAMP,
                MD6_WEIGHTGROUP_ALL);
  animLeaf.SetNumAnimMods(3 * numLegs + 1);
  ++animLeaf.currentDeferred;
  animLeaf.SetUseStackTop(true);
  animLeaf.SetClearNonModWeights(true);
  idMD6Blend::jointMod_t* mods =
      animLeaf.animMods[animLeaf.currentDeferred & 1].list;
  SetJointModFromOriginAxis(mods[0], waistJointHandle, 8,
                            waistModelShift, mat3_identity);
  for (int leg = 0; leg < numLegs; ++leg) {
    SetJointModFromOriginAxis(mods[leg * 3 + 1], hipJoints[leg], 35,
                              vec3_origin, newHipAxes[leg]);
    SetJointModFromOriginAxis(mods[leg * 3 + 2], kneeJoints[leg], 35,
                              vec3_origin, newKneeAxes[leg]);
    SetJointModFromOriginAxis(mods[leg * 3 + 3], ankleJoints[leg], 35,
                              vec3_origin, oldAnkleAxes[leg]);
  }
  mergeBranch.left = nullptr;
  mergeBranch.right = &animLeaf;
  mergeBranch.filterGroup = MD6_WEIGHTGROUP_LEGS;
}
