#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "engine/gamelib/animstack/treeanimator_abi.h"

#include <cstddef>
#include <cstdint>

enum DegreesUnique_t : int;
enum RadiansUnique_t : int;

class idAnimatorParms_TrackerBase : public idAnimatorParms_Base {
public:
  idAnimatorParms_TrackerBase(
      idAnimStack* animStack, const char* name,
      idMD6Blend::originBlend_t originBlend,
      md6WeightGroup_t weightGroup, md6WeightGroup_t filterGroup,
      float alpha, bool projectReferenceOrientation,
      idTreeAnimator* treeAnimator);

  idTreeAnimator* treeAnimator;
  bool projectReferenceOrientation;
  std::uint8_t trackerParmsPadding[7];
};

class idAnimator_TrackerBase : public idAnimator_Base {
public:
  idAnimator_TrackerBase();
  ~idAnimator_TrackerBase() override;

  void SerializeSnapshot(idSerializer& serializer) override;
  void PreBlendSnapshot(idAnimStack* stack, int curTime, int ticksPerSec,
                        float blendWeight) override;

  void Update(
      const idAnimator_TrackerBase* parentTracker,
      const idVec3& focusPoint, const idAngles& offsetAngles,
      idTypesafeNumber<float, DegreesUnique_t> pitchUp,
      idTypesafeNumber<float, DegreesUnique_t> pitchDown,
      idTypesafeNumber<float, DegreesUnique_t> yawRight,
      idTypesafeNumber<float, DegreesUnique_t> yawLeft);

  void SetFocusPoint(const idVec3& point) { focusPoint = point; }
  const idVec3& GetFocusPoint() const { return focusPoint; }
  void SetHeadTrackGroupIndex(int index) { trackGroupIndex = index; }
  int GetHeadTrackGroupIndex() const { return trackGroupIndex; }
  void SetEnableConstraints(bool enabled) {
    trackerFlags = static_cast<std::uint8_t>(
        (trackerFlags & ~ENABLE_CONSTRAINTS) |
        (enabled ? ENABLE_CONSTRAINTS : 0u));
  }
  bool GetEnableConstraints() const {
    return (trackerFlags & ENABLE_CONSTRAINTS) != 0;
  }
  void SetIdealOffsetAngles(const idAngles& angles) {
    idealOffsetAngles = angles;
  }
  const idAngles& GetIdealOffsetAngles() const {
    return idealOffsetAngles;
  }
  void LockTracking(bool locked) {
    trackerFlags = static_cast<std::uint8_t>(
        (trackerFlags & ~LOCK_TRACKING) |
        (locked ? LOCK_TRACKING : 0u));
  }
  void UseTrackGroupAsReference(bool use) {
    trackerFlags = static_cast<std::uint8_t>(
        (trackerFlags & ~USE_TRACK_GROUP_AS_REFERENCE) |
        (use ? USE_TRACK_GROUP_AS_REFERENCE : 0u));
  }
  void UseExperimentalMode(bool use) {
    trackerFlags = static_cast<std::uint8_t>(
        (trackerFlags & ~USE_EXPERIMENTAL_MODE) |
        (use ? USE_EXPERIMENTAL_MODE : 0u));
  }
  bool ValidAsParent(int curTime, int msPerFrame) const {
    return IsContributing() && curTime - lastValidTime < 2 * msPerFrame;
  }

  void SetPitchConstraints(
      idTypesafeNumber<float, DegreesUnique_t> up,
      idTypesafeNumber<float, DegreesUnique_t> down);
  void SetYawConstraints(
      idTypesafeNumber<float, DegreesUnique_t> right,
      idTypesafeNumber<float, DegreesUnique_t> left);

  idTypesafeNumber<float, DegreesUnique_t> GetPitchUpDegrees() const {
    return pitchUp;
  }
  idTypesafeNumber<float, DegreesUnique_t> GetPitchDownDegrees() const {
    return pitchDown;
  }
  idTypesafeNumber<float, DegreesUnique_t> GetYawRightDegrees() const {
    return yawRight;
  }
  idTypesafeNumber<float, DegreesUnique_t> GetYawLeftDegrees() const {
    return yawLeft;
  }
  bool IsYawClamped() const { return (trackerFlags & YAW_CLAMPED) != 0; }
  bool IsPitchClamped() const {
    return (trackerFlags & PITCH_CLAMPED) != 0;
  }
  void SetProjectReferenceOrientation(bool project) {
    trackerFlags = static_cast<std::uint8_t>(
        (trackerFlags & ~PROJECT_REFERENCE_ORIENTATION) |
        (project ? PROJECT_REFERENCE_ORIENTATION : 0u));
  }
  bool GetProjectReferenceOrientation() const {
    return (trackerFlags & PROJECT_REFERENCE_ORIENTATION) != 0;
  }

  virtual void SetReferenceJointOverride(idJointIndex_t) {}
  virtual const idJointIndex_t GetReferenceJointOverride() const;
  virtual void GetModelSpaceFrameOfReferenceForChild(
      const idTreeAnimator*, idVec3&, idMat3&) const {}
  virtual void GetWorldSpaceFrameOfReferenceForChild(
      const idTreeAnimator*, idVec3&, idMat3&) const {}
  virtual void GetWorldSpaceFrameOfReference(
      const idAnimator_TrackerBase*, const idTreeAnimator*, idVec3&,
      idMat3&) const {}
  virtual void GetModelSpaceFrameOfReference(
      const idAnimator_TrackerBase*, const idTreeAnimator*, idVec3&,
      idMat3&) const {}

  const idVec3& GetLastModelSpaceDirToFocus() const {
    return msDirToFocus;
  }
  const idVec3& GetLastModelSpaceReferenceJointPos() const {
    return msReferenceJointPos;
  }
  const idVec3& GetLastWorldSpaceFocusJointPos() const {
    return wsFocusJointPos;
  }

protected:
  bool InternalInit(const idAnimatorParms_Base& parms) override;
  void InternalShutdown(idAnimStack*) override {}
  void InternalPreBlendTree(const idAnimStack* stack, int curTime,
                            int ticksPerSec) override;
  void InternalPostBlendTree(const idAnimStack*, int) override {}
  void InternalStart(
      const idAnimStack*, int,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalEnd(
      const idAnimStack*, int,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalBlend(
      const idAnimStack*, int, float,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  bool InternalIsContributing() const override;
  const idMD6Branch* InternalGetMergeBranch() const override {
    return &mergeBranch;
  }
  idMD6Branch* InternalGetMergeBranch() override { return &mergeBranch; }
  virtual void InternalUpdate(const idAnimator_TrackerBase*) {}

private:
  enum trackerFlag_t : std::uint8_t {
    YAW_CLAMPED = 1u << 0,
    PITCH_CLAMPED = 1u << 1,
    ENABLE_CONSTRAINTS = 1u << 2,
    PROJECT_REFERENCE_ORIENTATION = 1u << 3,
    CLIENT_SNAP_FOCUS_POINT = 1u << 4,
    LOCK_TRACKING = 1u << 5,
    USE_TRACK_GROUP_AS_REFERENCE = 1u << 6,
    USE_EXPERIMENTAL_MODE = 1u << 7,
  };

  void GetUnconstrainedAngleDeltas(
      const idAnimator_TrackerBase* parentTracker,
      idTreeAnimator* treeAnimator, idAngles& angleDeltas,
      idAngles& angles, idVec3& cross);
  void InternalDoTracking(const idAnimStack* stack, float deltaLerpScale);

protected:

  const idAnimator_TrackerBase* parentTracker;
  idTreeAnimator* treeAnimator;
  idVec3 focusPoint;
  idVec3 focusPoints[2];
  idTypesafeNumber<float, DegreesUnique_t> yawRight;
  idTypesafeNumber<float, DegreesUnique_t> yawLeft;
  idTypesafeNumber<float, DegreesUnique_t> pitchUp;
  idTypesafeNumber<float, DegreesUnique_t> pitchDown;
  int trackGroupIndex;
  idMD6Branch mergeBranch;
  idMD6LeafPause leaf;
  idVec3 msDirToFocus;
  idVec3 wsFocusJointPos;
  idVec3 msReferenceJointPos;
  idAngles idealOffsetAngles;
  idAngles curOffsetAngles;
  idAngles lastAngles;
  int lastValidTime;
  std::uint8_t trackerFlags;
  std::uint8_t trackerTailPadding[3];

  friend class AnimatorTorsoTrackerValidationAccess;
};

static_assert(sizeof(idMD6Branch) == 0x38, "idMD6Branch ABI drift");
static_assert(sizeof(idMD6LeafPause) == 0x70,
              "idMD6LeafPause ABI drift");
static_assert(sizeof(idAnimatorParms_TrackerBase) == 0x58,
              "idAnimatorParms_TrackerBase ABI drift");
static_assert(offsetof(idAnimatorParms_TrackerBase, treeAnimator) == 0x48,
              "tracker parameter tree-animator ABI drift");
static_assert(offsetof(idAnimatorParms_TrackerBase,
                       projectReferenceOrientation) == 0x50,
              "tracker parameter projection ABI drift");
static_assert(sizeof(idAnimator_TrackerBase) == 0x190,
              "idAnimator_TrackerBase ABI drift");
