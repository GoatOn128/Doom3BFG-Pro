#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/animstack/animator_torsotracker.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <cstdint>
#include <xmmintrin.h>

namespace {

struct headTrackGroupABI_t {
  std::uint8_t prefix[0x20];
  idJointIndex_t focusJointIdx;
  idJointIndex_t orientationJointIdx;
  float blendTime;
};

struct headTrackGroupListABI_t {
  headTrackGroupABI_t* list;
  int num;
  int size;
  std::uint16_t granularity;
  std::uint8_t memTag;
  std::uint8_t listStatic;
};

static_assert(sizeof(headTrackGroupABI_t) == 0x28,
              "head-track group ABI drift");
static_assert(sizeof(headTrackGroupListABI_t) == 0x18,
              "head-track group list ABI drift");

const idMD6Model* DeclModel(const idDeclMD6* declaration) {
  return *reinterpret_cast<idMD6Model* const*>(
      reinterpret_cast<const std::uint8_t*>(declaration) + 0x60);
}

const headTrackGroupListABI_t& HeadTrackGroups(
    const idDeclMD6* declaration) {
  return *reinterpret_cast<const headTrackGroupListABI_t*>(
      reinterpret_cast<const std::uint8_t*>(declaration) + 0x320);
}

const headTrackGroupABI_t* HeadTrackGroup(const idDeclMD6* declaration,
                                          int index) {
  const headTrackGroupListABI_t& groups = HeadTrackGroups(declaration);
  if (index < 0 || index >= groups.num) {
    return nullptr;
  }
  return &groups.list[index];
}

float InverseSqrtClamped(float value) {
  const __m128 minimum = _mm_set_ss(1.175494351e-38f);
  const __m128 root = _mm_sqrt_ps(_mm_max_ps(_mm_set_ss(value), minimum));
  return 1.0f / _mm_cvtss_f32(root);
}

idVec3 TransformPosition(const idVec3& position, const idMat3& axis,
                         const idVec3& origin) {
  idVec3 result;
  result.x = ((position.y * axis.mat[1].x) +
              (position.x * axis.mat[0].x)) +
             (position.z * axis.mat[2].x);
  result.y = ((position.x * axis.mat[0].y) +
              (position.y * axis.mat[1].y)) +
             (position.z * axis.mat[2].y);
  result.z = ((position.x * axis.mat[0].z) +
              (position.y * axis.mat[1].z)) +
             (position.z * axis.mat[2].z);
  result.x += origin.x;
  result.y += origin.y;
  result.z += origin.z;
  return result;
}

idJointIndex_t JointIndexForTag(const idTreeAnimator* treeAnimator,
                                const tagData_t& tag) {
  const idMD6Model* model = DeclModel(treeAnimator->decl);
  if (model == nullptr || model->skeleton == nullptr) {
    return idJointIndex_t(static_cast<std::int16_t>(-1));
  }
  const auto& handle = *reinterpret_cast<const idJointHandle_t*>(
      &tag.parentJoint);
  return model->skeleton->GetJointIndex(handle);
}

} // namespace

idAnimator_TorsoTracker::~idAnimator_TorsoTracker() = default;

void idAnimator_TorsoTracker::SetReferenceJointOverride(
    const idJointIndex_t jointIndex) {
  referenceJointOverrideIdx = jointIndex;
}

const idJointIndex_t idAnimator_TorsoTracker::GetReferenceJointOverride() const {
  return referenceJointOverrideIdx;
}

void idAnimator_TorsoTracker::GetModelSpaceFrameOfReferenceForChild(
    const idTreeAnimator* childTreeAnimator, idVec3& modelPosition,
    idMat3& modelAxis) const {
  (void)childTreeAnimator;
  modelAxis.mat[0] = msDirToFocus;

  const idVec3& up = mat3_identity.mat[2];
  const float sideX = (modelAxis.mat[0].z * up.y) -
                      (modelAxis.mat[0].y * up.z);
  const float sideY = (modelAxis.mat[0].x * up.z) -
                      (modelAxis.mat[0].z * up.x);
  const float sideZ = (modelAxis.mat[0].y * up.x) -
                      (modelAxis.mat[0].x * up.y);
  const float sideLengthSquared =
      ((sideX * sideX) + (sideY * sideY)) + (sideZ * sideZ);
  const float inverseSideLength = InverseSqrtClamped(sideLengthSquared);
  modelAxis.mat[1].x = sideX * inverseSideLength;
  modelAxis.mat[1].y = sideY * inverseSideLength;
  modelAxis.mat[1].z = sideZ * inverseSideLength;

  const float finalX =
      (modelAxis.mat[0].y * modelAxis.mat[1].z) -
      (modelAxis.mat[0].z * modelAxis.mat[1].y);
  const float finalY =
      (modelAxis.mat[1].x * modelAxis.mat[0].z) -
      (modelAxis.mat[1].z * modelAxis.mat[0].x);
  const float finalZ =
      (modelAxis.mat[1].y * modelAxis.mat[0].x) -
      (modelAxis.mat[1].x * modelAxis.mat[0].y);
  const float finalLengthSquared =
      ((finalX * finalX) + (finalY * finalY)) + (finalZ * finalZ);
  const float inverseFinalLength = InverseSqrtClamped(finalLengthSquared);
  modelAxis.mat[2].x = finalX * inverseFinalLength;
  modelAxis.mat[2].y = finalY * inverseFinalLength;
  modelAxis.mat[2].z = finalZ * inverseFinalLength;
  modelPosition = msReferenceJointPos;
}

void idAnimator_TorsoTracker::GetWorldSpaceFrameOfReferenceForChild(
    const idTreeAnimator* childTreeAnimator, idVec3& worldPosition,
    idMat3& worldAxis) const {
  idVec3 modelPosition;
  idMat3 modelAxis;
  GetModelSpaceFrameOfReferenceForChild(childTreeAnimator, modelPosition,
                                        modelAxis);
  worldAxis = modelAxis * childTreeAnimator->g.axis;
  worldPosition = wsFocusJointPos;
}

idAnimator_TorsoTracker::idAnimator_TorsoTracker()
    : referenceJointOverrideIdx(static_cast<std::int16_t>(-1)),
      useOriginXYForWorldSpace(false) {}

void idAnimator_TorsoTracker::SerializeSnapshot(idSerializer& serializer) {
  idAnimator_TrackerBase::SerializeSnapshot(serializer);

  bool useOrigin = useOriginXYForWorldSpace;
  if (serializer.writing) {
    serializer.msg->WriteBits(useOrigin, 1);
  } else {
    useOrigin = serializer.msg->ReadBits(1) == 1;
  }
  useOriginXYForWorldSpace = useOrigin;

  std::uint16_t value =
      static_cast<std::uint16_t>(referenceJointOverrideIdx.value);
  if (referenceJointOverrideIdx.value == NULL_JOINT_INDEX.value) {
    value = 0xFFFFu;
  }
  if (serializer.writing) {
    serializer.msg->WriteBits(value, 16);
  } else {
    value = static_cast<std::uint16_t>(serializer.msg->ReadBits(16));
  }
  if (value == 0xFFFFu) {
    referenceJointOverrideIdx.value = NULL_JOINT_INDEX.value;
  } else {
    referenceJointOverrideIdx.value = static_cast<std::int16_t>(value);
  }
}

void idAnimator_TorsoTracker::GetWorldSpaceFrameOfReference(
    const idAnimator_TrackerBase* frameParentTracker,
    const idTreeAnimator* frameTreeAnimator, idVec3& worldPosition,
    idMat3& worldAxis) const {
  if (referenceTagOverride.parentJoint.value == 0xFFFFu) {
    if (frameParentTracker != nullptr && frameParentTracker->IsContributing()) {
      frameParentTracker->GetWorldSpaceFrameOfReferenceForChild(
          frameTreeAnimator, worldPosition, worldAxis);
    } else {
      const headTrackGroupABI_t* group =
          HeadTrackGroup(frameTreeAnimator->decl, trackGroupIndex);
      const idJointIndex_t jointIndex =
          group != nullptr ? group->focusJointIdx : idJointIndex_t(0);
      frameTreeAnimator->GetWorldSpaceJointTransform(
          ANIMATION_POSE_CURRENT, jointIndex, worldPosition, worldAxis);
    }
  } else {
    const idJointIndex_t jointIndex =
        JointIndexForTag(frameTreeAnimator, referenceTagOverride);
    idVec3 parentOrigin;
    idMat3 parentAxis;
    if (frameTreeAnimator->GetModelSpaceJointTransform(
            ANIMATION_POSE_CURRENT, jointIndex, parentOrigin, parentAxis)) {
      worldPosition = TransformPosition(referenceTagOverride.trans, parentAxis,
                                        parentOrigin);
      worldAxis = referenceTagOverride.rot.ToMat3() * parentAxis;
    }

    const bool deferred = (frameTreeAnimator->useDeferredPosition & 1u) != 0;
    const idMat3& renderAxis =
        deferred ? frameTreeAnimator->deferredAxis : frameTreeAnimator->g.axis;
    const idVec3& renderOrigin =
        deferred ? frameTreeAnimator->deferredOrigin
                 : frameTreeAnimator->g.origin;
    worldPosition = TransformPosition(worldPosition, renderAxis, renderOrigin);
    const idMat3& finalRenderAxis =
        (frameTreeAnimator->useDeferredPosition & 1u) != 0
            ? frameTreeAnimator->deferredAxis
            : frameTreeAnimator->g.axis;
    worldAxis *= finalRenderAxis;
  }

  if (useOriginXYForWorldSpace) {
    worldPosition.x = frameTreeAnimator->g.origin.x;
    worldPosition.y = frameTreeAnimator->g.origin.y;
  }
}

void idAnimator_TorsoTracker::GetModelSpaceFrameOfReference(
    const idAnimator_TrackerBase* frameParentTracker,
    const idTreeAnimator* frameTreeAnimator, idVec3& modelPosition,
    idMat3& modelAxis) const {
  if (referenceTagOverride.parentJoint.value == 0xFFFFu) {
    if (frameParentTracker != nullptr && frameParentTracker->IsContributing()) {
      frameParentTracker->GetModelSpaceFrameOfReferenceForChild(
          frameTreeAnimator, modelPosition, modelAxis);
    } else {
      const headTrackGroupABI_t* group =
          HeadTrackGroup(frameTreeAnimator->decl, trackGroupIndex);
      const idJointIndex_t jointIndex =
          referenceJointOverrideIdx.value < 0
              ? *reinterpret_cast<const idJointIndex_t*>(
                    reinterpret_cast<const std::uint8_t*>(group) + 0x22)
              : referenceJointOverrideIdx;
      frameTreeAnimator->GetModelSpaceJointTransform(
          ANIMATION_POSE_CURRENT, jointIndex, modelPosition, modelAxis);
    }
    return;
  }

  const idJointIndex_t jointIndex =
      JointIndexForTag(frameTreeAnimator, referenceTagOverride);
  idVec3 parentOrigin;
  idMat3 parentAxis;
  if (frameTreeAnimator->GetModelSpaceJointTransform(
          ANIMATION_POSE_CURRENT, jointIndex, parentOrigin, parentAxis)) {
    modelPosition = TransformPosition(referenceTagOverride.trans, parentAxis,
                                      parentOrigin);
    modelAxis = referenceTagOverride.rot.ToMat3() * parentAxis;
  }
}
