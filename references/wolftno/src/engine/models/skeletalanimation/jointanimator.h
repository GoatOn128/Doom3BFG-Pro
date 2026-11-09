#pragma once

#include "engine/models/skeletalanimation/jobs/md6blend.h"
#include "engine/models/skeletalanimation/md6util.h"
#include "engine/renderer/bufferobject.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/containers/list.h"

#include <cstddef>
#include <cstdint>

class idColor;
class idFile;
class idParallelJobList;
class idRenderDebug;
struct tagData_t;

enum animationPose_t : int {
  ANIMATION_POSE_CURRENT = 0,
  DRIVER_MODEL = 1,
};

enum jointModTransform_t : int {
  JOINTMOD_NONE = 0,
  JOINTMOD_LOCAL = 1,
  JOINTMOD_LOCAL_OVERRIDE = 2,
  JOINTMOD_MODEL = 3,
  JOINTMOD_MODEL_OVERRIDE = 4,
};

class idMat3x4 {
public:
  float mat[12]{};
};

struct idJointAnimatorCallbacks {
  void (*drawJoints)(const class idJointAnimator& animator,
                     idRenderDebug* renderDebug, animationPose_t pose,
                     const idVec3& origin, const idMat3& axis,
                     const idVec3& scale,
                     const idList<idJointIndex_t, 5>* selectedJoints,
                     bool showBones, bool showAxis, bool showBounds,
                     bool showJointNames, const idColor* lineColor){};
  void (*save)(const class idJointAnimator& animator, idFile& file){};
  bool (*load)(class idJointAnimator& animator, idFile& file){};
};

void SetJointAnimatorCallbacks(const idJointAnimatorCallbacks& callbacks);
void AnimationBlendJob(idMD6Blend::blendParms_t* blendParms);
void AnimationBlendJobToRSTU(idMD6Blend::blendParms_t* blendParms,
                             float* localR, float* localS, float* localT,
                             float* localU);

class idJointAnimator {
public:
  explicit idJointAnimator(const idDeclMD6* declaration);
  ~idJointAnimator();

  void CalcFrameBoundsFromJoints();
  void CalcFrameBoundsFromBlendOutput();
  bool SyncJoints();
  void Reference(idJointBuffer* jointBuffer, int jointRefOffset,
                 int numRefJoints);
  void SetDualQuaternion(bool enable);
  float GetBlendedUserChannel(idUserChannelIndex index) const;

  bool GetModelSpaceJointBindTransform(idJointIndex_t jointIndex,
                                       idVec3& origin, idMat3& axis) const;
  bool GetModelSpaceJointPosition(animationPose_t pose,
                                  idJointIndex_t jointIndex,
                                  idVec3& origin) const;
  bool GetModelSpaceJointAxis(animationPose_t pose, idJointIndex_t jointIndex,
                              idMat3& axis) const;
  bool GetModelSpaceJointTransform(animationPose_t pose,
                                   idJointIndex_t jointIndex,
                                   idVec3& origin, idMat3& axis) const;
  bool GetModelSpaceAttachmentTransform(idJointIndex_t jointIndex,
                                        const idVec3& translation,
                                        const idQuat& rotation,
                                        idVec3& origin, idMat3& axis) const;
  bool GetModelSpaceAttachmentPosition(idJointIndex_t jointIndex,
                                       const idVec3& translation,
                                       const idQuat& rotation,
                                       idVec3& origin) const;
  bool GetModelSpaceAttachmentAxis(idJointIndex_t jointIndex,
                                   const idVec3& translation,
                                   const idQuat& rotation,
                                   idMat3& axis) const;
  bool GetModelSpaceJointFrameDeltas(const idJointIndex_t* indices,
                                     int numIndices, idVec3* translations,
                                     idMat3* rotations) const;
  bool GetModelSpaceReferenceJointFrameDeltas(
      const idJointIndex_t* indices, int numIndices, idVec3* translations,
      idMat3* rotations) const;
  bool GetModelSpaceAttachmentFrameDeltas(
      idJointIndex_t jointIndex, const idVec3& translation,
      const idQuat& rotation, idVec3& translationDelta,
      idMat3& rotationDelta) const;
  void DrawJoints(idRenderDebug* renderDebug, animationPose_t pose,
                  const idVec3& origin, const idMat3& axis,
                  const idVec3& scale,
                  const idList<idJointIndex_t, 5>* selectedJoints,
                  bool showBones, bool showAxis, bool showBounds,
                  bool showJointNames, const idColor* lineColor) const;
  void SetFromAnim(const idMD6Anim* anim, int time, bool updateRenderModel,
                   bool checkForOriginTranslation,
                   const idVec3* translationScale,
                   const idMD6Blend::jointMod_t* mods, int numMods);
  bool GetLocalSpaceJointBindAxis(idJointIndex_t jointIndex,
                                  idMat3& axis) const;
  bool GetModelSpaceTagTransform(const tagData_t& tag, idVec3& origin,
                                 idMat3& axis) const;
  bool GetModelSpaceTagPosition(const tagData_t& tag, idVec3& origin) const;
  void ApplyOriginDeltas(idVec3& outOrigin, idMat3& outAxis,
                         const idVec3& origin, const idMat3& axis,
                         const idVec3& scale) const;
  void ApplyOriginDeltas(idVec3& outOrigin, idQuat& outAxis,
                         const idVec3& origin, const idQuat& axis,
                         const idVec3& scale) const;
  void Save(idFile& file) const;
  bool Load(idFile& file);
  void ClearJointMods();
  void BlendTreeInternal(int currentTime, int previousTime,
                         int gameMsPerFrame, int ticksPerSec,
                         idMD6FusionBranch* tree,
                         idParallelJobList* parallelJobList, float* localR,
                         float* localS, float* localT, float* localU);

  void SetJointPosAxisScale(animationPose_t pose, idJointIndex_t jointIndex,
                            jointModTransform_t type, const idVec3& pos,
                            const idMat3& axis, const idVec3& scale);
  void SetJointMat3x4(animationPose_t pose, idJointIndex_t jointIndex,
                      jointModTransform_t type, const idMat3x4& matrix,
                      bool setPosition);
  void SetJointPos(animationPose_t pose, idJointIndex_t jointIndex,
                   jointModTransform_t type, const idVec3& pos);
  void SetJointAxis(animationPose_t pose, idJointIndex_t jointIndex,
                    jointModTransform_t type, const idMat3& axis);
  void SetJointScale(animationPose_t pose, idJointIndex_t jointIndex,
                     jointModTransform_t type, const idVec3& scale);
  idMD6Blend::jointMod_t* AllocJointMod();

  const idDeclMD6* decl;
  idJointBuffer jointBuffers[10];
  md6AnimCommand_t* commands;
  idMD6Blend::blendParms_t* blendParms;
  idList<idMD6Blend::jointMod_t, 17> jointMods[2];
  idList<idMD6Blend::blendShapeEval_t, 17> blendShapesEval;
  idJointMat* joints[4];
  float* userChannels[2];
  md6BlendOutputs_t* blendOutputs[2];
  idBounds frameBounds;
  idBounds normalizedBounds;
  idBounds translatedBounds;
  idJointIndex_t skipJointForBounds;
  std::uint16_t recoveredSkipPadding{};
  int currentDeferred;
  int deferredJobJointBuffer;
  int nextRenderThreadJointBuffer;
  int renderThreadJointBuffer;
  int lastBlendTime;
  std::uint8_t animatorFlags;
  std::uint8_t recoveredTail[7]{};

private:
  bool IsValidJoint(idJointIndex_t jointIndex) const;
  static std::uint16_t JointModFlags(animationPose_t pose,
                                     jointModTransform_t type,
                                     std::uint16_t baseFlags);
};

static_assert(sizeof(idMat3x4) == 0x30);
static_assert(sizeof(idJointBuffer) == 0x18);
static_assert(offsetof(idJointAnimator, commands) == 0xF8);
static_assert(offsetof(idJointAnimator, blendShapesEval) == 0x138);
static_assert(offsetof(idJointAnimator, joints) == 0x150);
static_assert(offsetof(idJointAnimator, userChannels) == 0x170);
static_assert(offsetof(idJointAnimator, blendOutputs) == 0x180);
static_assert(offsetof(idJointAnimator, frameBounds) == 0x190);
static_assert(offsetof(idJointAnimator, normalizedBounds) == 0x1A8);
static_assert(offsetof(idJointAnimator, translatedBounds) == 0x1C0);
static_assert(offsetof(idJointAnimator, skipJointForBounds) == 0x1D8);
static_assert(offsetof(idJointAnimator, animatorFlags) == 0x1F0);
static_assert(sizeof(idJointAnimator) == 0x1F8);
