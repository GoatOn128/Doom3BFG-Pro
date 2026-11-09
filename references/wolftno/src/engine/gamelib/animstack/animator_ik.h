#pragma once

#include "engine/decls/declreachik.h"
#include "engine/decls/declwalkik.h"
#include "engine/gamelib/animstack/animator_base.h"
#include "engine/gamelib/physics/clip.h"
#include "engine/models/skeletalanimation/jointanimator.h"
#include "engine/models/skeletalanimation/taginfo.h"

#include <cstddef>
#include <cstdint>

class idTreeAnimator;

class idAnimatorParms_IK : public idAnimatorParms_Base {
public:
  idAnimatorParms_IK(idAnimStack* stack, const char* animatorName,
                     float animatorAlpha, idTreeAnimator* sourceAnimator)
      : idAnimatorParms_Base(stack, animatorName, idMD6Blend::BOP_REF_LERP,
                             idMD6Blend::ORIGINBLEND_BRANCH,
                             MD6_WEIGHTGROUP_ALL, MD6_WEIGHTGROUP_ALL,
                             animatorAlpha),
        treeAnimator(sourceAnimator) {}

  idTreeAnimator* treeAnimator;
};

class idAnimatorParms_WalkIK : public idAnimatorParms_IK {
public:
  idAnimatorParms_WalkIK(const idDeclWalkIK* declaration, int entityNumber,
                         idClip* ownerClip, idClipModel* model, int mask,
                         const idVec3& up, idAnimStack* stack,
                         const char* animatorName, float animatorAlpha,
                         idTreeAnimator* sourceAnimator)
      : idAnimatorParms_IK(stack, animatorName, animatorAlpha,
                           sourceAnimator),
        declWalkIK(declaration), entityNum(entityNumber), clipMask(mask),
        clip(ownerClip), clipModel(model), upNormal(up) {}

  const idDeclWalkIK* declWalkIK;
  int entityNum;
  int clipMask;
  idClip* clip;
  idClipModel* clipModel;
  idVec3 upNormal;
  std::uint8_t tailPadding[4];
};

class idAnimatorParms_ReachIK : public idAnimatorParms_IK {
public:
  idAnimatorParms_ReachIK(const idDeclReachIK* declaration, int entityNumber,
                          idClip* ownerClip, idAnimStack* stack,
                          const char* animatorName, float animatorAlpha,
                          idTreeAnimator* sourceAnimator)
      : idAnimatorParms_IK(stack, animatorName, animatorAlpha,
                           sourceAnimator),
        declReachIK(declaration), entityNum(entityNumber), clip(ownerClip) {}

  const idDeclReachIK* declReachIK;
  int entityNum;
  std::uint8_t entityPadding[4];
  idClip* clip;
};

class idAnimatorParms_HandIK : public idAnimatorParms_IK {
public:
  idAnimatorParms_HandIK(const idDeclHandIK* declaration, int entityNumber,
                         idClip* ownerClip, idAnimStack* stack,
                         const char* animatorName, float animatorAlpha,
                         idTreeAnimator* sourceAnimator)
      : idAnimatorParms_IK(stack, animatorName, animatorAlpha,
                           sourceAnimator),
        declHandIK(declaration), entityNum(entityNumber), clip(ownerClip) {}

  const idDeclHandIK* declHandIK;
  int entityNum;
  std::uint8_t entityPadding[4];
  idClip* clip;
};

class idAnimator_IK : public idAnimator_Base {
public:
  idAnimator_IK();
  ~idAnimator_IK() override;

  bool IsInitialized() const { return ikInitialized; }
  int GetEntityNum() const { return entityNum; }
  void SetEntityNum(int value) { entityNum = value; }

  priority_t GetStackPriority() const override { return PRIORITY_IK; }
  void PreBlendSnapshot(idAnimStack* stack, int curTime, int ticksPerSec,
                        float fraction) override;

protected:
  virtual void Evaluate() = 0;

  bool SolveTwoBones(const idVec3& startPos, idVec3& endPos,
                     const idVec3& direction, float firstLength,
                     float secondLength, idVec3& jointPos);
  float GetBoneAxis(const idVec3& startPos, const idVec3& endPos,
                    const idVec3& direction, idMat3& axis);

  bool InternalInit(const idAnimatorParms_Base&) override { return true; }
  void InternalShutdown(idAnimStack*) override {}
  void InternalPreBlendTree(const idAnimStack*, int, int) override {
    Evaluate();
  }
  void InternalPostBlendTree(const idAnimStack*, int) override {}
  void InternalStart(const idAnimStack*, int,
                     idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalEnd(const idAnimStack*, int,
                   idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalBlend(const idAnimStack*, int, float,
                     idTypesafeNumber<int, gameTimeUnique_t>) override {}
  bool InternalIsContributing() const override { return false; }
  const idMD6Branch* InternalGetMergeBranch() const override {
    return nullptr;
  }
  idMD6Branch* InternalGetMergeBranch() override { return nullptr; }

public:
  bool enabled;
  bool ikInitialized;
  std::uint8_t ikPadding[2];
  int entityNum;
  idTreeAnimator* animator;
  idClip* clip;

};

class idAnimator_WalkIK : public idAnimator_IK {
public:
  static constexpr int MAX_LEGS = 8;

  idAnimator_WalkIK();
  ~idAnimator_WalkIK() override;

  serializeType_t GetSerializeType() override { return STYPE_WALK_IK; }
  void SerializeSnapshot(idSerializer& serializer) override;

  void SetClipModel(idClipModel* value) { clipModel = value; }
  void EnableAll();
  void DisableAll();
  void EnableLeg(int leg);
  void DisableLeg(int leg);
  void SetDeadKeepLastMods(bool value) { deadKeepLastMods = value; }
  bool IsActive() const;
  bool IsEnabled() const { return enabled; }
  bool IsOnGround() const { return onGround; }
  bool IsOnStairs() const { return onStairs; }
  bool IsDead() const { return isDead; }
  void SetIsDead(bool value) { isDead = value; }
  idJointIndex_t GetFootJoint(footStepType_t footstep) const;
  int GetFootSurfaceType(footStepType_t footstep) const;
  const idVec3& GetFootPosition(footStepType_t footstep) const;
  void SetEnabled(bool value) { enabled = value; }

protected:
  bool InternalInit(const idAnimatorParms_Base& parms) override;
  void InternalShutdown(idAnimStack*) override {}
  bool InternalIsContributing() const override;
  const idMD6Branch* InternalGetMergeBranch() const override {
    return &mergeBranch;
  }
  idMD6Branch* InternalGetMergeBranch() override { return &mergeBranch; }
  void Evaluate() override;

public:
  const idDeclWalkIK* decl;
  idClipModel* clipModel;
  int clipMask;
  idVec3 upNormal;
  idClipModel* footModel;
  bool createdClipModel;
  std::uint8_t createdPadding[3];
  int numLegs;
  int enabledLegs;
  idJointIndex_t footJoints[MAX_LEGS];
  idJointIndex_t ankleJoints[MAX_LEGS];
  idJointIndex_t kneeJoints[MAX_LEGS];
  idJointIndex_t hipJoints[MAX_LEGS];
  idJointIndex_t dirJoints[MAX_LEGS];
  idJointIndex_t waistJointHandle;
  std::uint8_t waistPadding[2];
  idVec3 hipForward[MAX_LEGS];
  idVec3 kneeForward[MAX_LEGS];
  float upperLegLength[MAX_LEGS];
  float lowerLegLength[MAX_LEGS];
  idMat3 upperLegToHipJoint[MAX_LEGS];
  idMat3 lowerLegToKneeJoint[MAX_LEGS];
  idMat3 kneeJointTransform[MAX_LEGS];
  int pivotFoot;
  float pivotYaw;
  idVec3 pivotPos;
  bool onGround;
  bool onStairs;
  bool isDead;
  bool oldHeightsValid;
  bool deadKeepLastMods;
  std::uint8_t statePadding[3];
  float oldWaistHeight;
  float oldAnkleHeights[MAX_LEGS];
  int oldGroundPhysicsId;
  idVec3 oldGroundPhysicsOrigin;
  int surfaceTypes[MAX_LEGS];
  idClipQuery modelQuery;
  idClipQuery waistQuery;
  idClipQuery legQueries[MAX_LEGS];
  idClipQuery footQueries[MAX_LEGS];
  idVec3 lastModelSpaceFootOrigin[MAX_LEGS];
  float groundTraceDist;
  float internalRadiusXY;
  idMD6LeafPause animLeaf;
  idMD6Branch mergeBranch;

};

class idAnimator_ReachIK : public idAnimator_IK {
public:
  static constexpr int MAX_ARMS = 2;

  struct targetTransform_t {
    targetTransform_t() {
      origin = vec3_origin;
      axis = mat3_identity;
      selfTag.trans = vec3_origin;
      selfTag.rot.x = 0.0f;
      selfTag.rot.y = 0.0f;
      selfTag.rot.z = 0.0f;
      selfTag.rot.w = 0.0f;
      selfTag.parentJoint.value = static_cast<std::uint16_t>(-1);
      validOrigin = false;
      validAxis = false;
      validTag = false;
    }

    idVec3 origin;
    idMat3 axis;
    tagData_t selfTag;
    bool validOrigin;
    bool validAxis;
    bool validTag;
    std::uint8_t padding;
  };

  idAnimator_ReachIK();
  ~idAnimator_ReachIK() override;

  serializeType_t GetSerializeType() override { return STYPE_REACH_IK; }
  void SerializeSnapshot(idSerializer& serializer) override;

  void SetTargetOrigin(int arm, const idVec3& targetPos);
  const idVec3& GetTargetOrigin(int arm) const {
    return targetTransforms[arm].origin;
  }
  void SetTargetAxis(int arm, const idMat3& targetAxis);
  const idMat3& GetTargetAxis(int arm) const {
    return targetTransforms[arm].axis;
  }
  void SetTargetTransform(int arm, const idVec3& targetPos,
                          const idMat3& targetAxis) {
    SetTargetOrigin(arm, targetPos);
    SetTargetAxis(arm, targetAxis);
  }
  const targetTransform_t& GetTargetTransform(int arm) const {
    return targetTransforms[arm];
  }
  void ClearTargetTransform(int arm) {
    targetTransforms[arm].validOrigin = false;
    targetTransforms[arm].validAxis = false;
    targetTransforms[arm].validTag = false;
  }
  void SetSelfTargetTag(int arm, tagData_t tag) {
    targetTransforms[arm].selfTag = tag;
    targetTransforms[arm].validTag = true;
  }
  tagData_t GetSelfTargetTag(int arm) const {
    return targetTransforms[arm].selfTag;
  }

protected:
  bool InternalInit(const idAnimatorParms_Base& parms) override;
  void Evaluate() override;

public:
  idMat3 ModelToJointAxis(animationPose_t pose, idJointIndex_t joint,
                          const idMat3& modelSpaceAxis);
  idMat3 ModelToJointAxis(animationPose_t pose, idJointIndex_t joint,
                          const idMat3& modelSpaceAxis,
                          idJointIndex_t parentJoint,
                          const idMat3& parentModelAxis);

  const idDeclReachIK* decl;
  int numArms;
  int enabledArms;
  idJointIndex_t handJoints[MAX_ARMS];
  idJointIndex_t elbowJoints[MAX_ARMS];
  idJointIndex_t shoulderJoints[MAX_ARMS];
  targetTransform_t targetTransforms[MAX_ARMS];
  idVec3 shoulderForward[MAX_ARMS];
  idVec3 elbowForward[MAX_ARMS];
  float upperArmLength[MAX_ARMS];
  float lowerArmLength[MAX_ARMS];
  idMat3 upperArmToShoulderJoint[MAX_ARMS];
  idMat3 lowerArmToElbowJoint[MAX_ARMS];
  std::uint8_t tailPadding[4];

};

class idAnimator_HandIK : public idAnimator_IK {
public:
  static constexpr int NUM_ARMS = 2;

  struct handsIKEvalParams_t {
    handsIKEvalParams_t() { Clear(); }
    void Clear() {
      isInitialized = false;
      targetOrigin = vec3_origin;
      targetAxis.mat[0] = vec3_origin;
      targetAxis.mat[1] = vec3_origin;
      targetAxis.mat[2] = vec3_origin;
      targetIsOffset = true;
      targetIsSlave = false;
      targetSlaveBlend = 1.0f;
      attachJoint.value = -1;
      handJoint.value = -1;
      forearmJoint.value = -1;
      elbowJoint.value = -1;
      shoulderJoint.value = -1;
      handToForearmLength = 0.0f;
      lowerArmLength = 0.0f;
      upperArmLength = 0.0f;
    }

    bool isInitialized;
    std::uint8_t initializedPadding[3];
    idVec3 targetOrigin;
    idMat3 targetAxis;
    bool targetIsOffset;
    bool targetIsSlave;
    std::uint8_t targetPadding[2];
    float targetSlaveBlend;
    idJointIndex_t attachJoint;
    idJointIndex_t handJoint;
    idJointIndex_t forearmJoint;
    idJointIndex_t elbowJoint;
    idJointIndex_t shoulderJoint;
    std::uint8_t jointPadding[2];
    float handToForearmLength;
    float lowerArmLength;
    float upperArmLength;
  };

  idAnimator_HandIK();
  ~idAnimator_HandIK() override = default;

  serializeType_t GetSerializeType() override { return STYPE_HAND_IK; }
  void SerializeSnapshot(idSerializer& serializer) override;
  void SetTarget(int arm, const idVec3& targetPos, const idMat3& targetRot,
                 bool useAsOffset, bool slave, float slaveBlend);

protected:
  bool InternalInit(const idAnimatorParms_Base& parms) override;
  void InternalPreBlendTree(const idAnimStack* stack, int curTime,
                            int ticksPerSec) override;
  void Evaluate() override {}

public:
  const idDeclHandIK* decl;
  handsIKEvalParams_t evalParams[NUM_ARMS];

};

static_assert(sizeof(idAnimatorParms_IK) == 0x50);
static_assert(sizeof(idAnimatorParms_WalkIK) == 0x80);
static_assert(offsetof(idAnimatorParms_WalkIK, declWalkIK) == 0x50);
static_assert(offsetof(idAnimatorParms_WalkIK, upNormal) == 0x70);
static_assert(sizeof(idAnimatorParms_ReachIK) == 0x68);
static_assert(sizeof(idAnimatorParms_HandIK) == 0x68);
static_assert(sizeof(idAnimator_IK) == 0x68);
static_assert(offsetof(idAnimator_IK, enabled) == 0x50);
static_assert(offsetof(idAnimator_IK, animator) == 0x58);
static_assert(sizeof(idAnimator_WalkIK) == 0x760);
static_assert(offsetof(idAnimator_WalkIK, footJoints) == 0x9c);
static_assert(offsetof(idAnimator_WalkIK, hipForward) == 0xf0);
static_assert(offsetof(idAnimator_WalkIK, animLeaf) == 0x6b8);
static_assert(offsetof(idAnimator_WalkIK, mergeBranch) == 0x728);
static_assert(sizeof(idAnimator_ReachIK::targetTransform_t) == 0x54);
static_assert(sizeof(idAnimator_ReachIK) == 0x200);
static_assert(offsetof(idAnimator_ReachIK, targetTransforms) == 0x84);
static_assert(offsetof(idAnimator_ReachIK, upperArmToShoulderJoint) == 0x16c);
static_assert(sizeof(idAnimator_HandIK::handsIKEvalParams_t) == 0x54);
static_assert(sizeof(idAnimator_HandIK) == 0x118);
static_assert(offsetof(idAnimator_HandIK, evalParams) == 0x70);
