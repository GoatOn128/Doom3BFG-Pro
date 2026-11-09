#pragma once

#include "engine/gamelib/animstack/animator_trackerbase.h"
#include "engine/models/serializer.h"
#include "engine/models/skeletalanimation/taginfo.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>

class idAnimator_TorsoTracker : public idAnimator_TrackerBase {
public:
  idAnimator_TorsoTracker();
  ~idAnimator_TorsoTracker() override;

  serializeType_t GetSerializeType() override { return STYPE_TORSO_TRACKER; }
  void SerializeSnapshot(idSerializer& serializer) override;
  void SetReferenceJointOverride(idJointIndex_t jointIndex) override;
  const idJointIndex_t GetReferenceJointOverride() const override;
  void GetModelSpaceFrameOfReferenceForChild(
      const idTreeAnimator* treeAnimator, idVec3& modelPosition,
      idMat3& modelAxis) const override;
  void GetWorldSpaceFrameOfReferenceForChild(
      const idTreeAnimator* treeAnimator, idVec3& worldPosition,
      idMat3& worldAxis) const override;
  void GetWorldSpaceFrameOfReference(
      const idAnimator_TrackerBase* parentTracker,
      const idTreeAnimator* treeAnimator, idVec3& worldPosition,
      idMat3& worldAxis) const override;
  void GetModelSpaceFrameOfReference(
      const idAnimator_TrackerBase* parentTracker,
      const idTreeAnimator* treeAnimator, idVec3& modelPosition,
      idMat3& modelAxis) const override;

  void SetUseOriginXYForFocusPoint(bool enabled) {
    useOriginXYForWorldSpace = enabled;
  }
  void SetReferenceTagOverride(const tagData_t& tag) {
    referenceTagOverride = tag;
  }

  static void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }

private:
  tagData_t referenceTagOverride;
  idJointIndex_t referenceJointOverrideIdx;
  bool useOriginXYForWorldSpace;
  std::uint8_t torsoTrackerTailPadding[5];

  friend class AnimatorTorsoTrackerValidationAccess;
};

static_assert(sizeof(idAnimator_TorsoTracker) == 0x1B8,
              "idAnimator_TorsoTracker ABI drift");
