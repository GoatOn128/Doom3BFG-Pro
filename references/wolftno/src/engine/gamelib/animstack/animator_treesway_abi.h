#pragma once

#include "engine/animation/md6.h"
#include "engine/gamelib/animstack/animstack.h"

#include <cstddef>
#include <cstdint>

enum invalidJointHandle_t { INVALID_JOINT_HANDLE = -1 };

template <typename T, typename InvalidTag, int InvalidValue>
class idHandle {
public:
  T value;
};

using idJointHandle_t =
    idHandle<unsigned short, invalidJointHandle_t, 65535>;

struct idMD6SkelData {
  std::uint16_t size;
  std::uint16_t numJoints;
  std::uint16_t numUserChannels;
  std::uint16_t parentCrc;
  std::uint16_t basePoseOffset;
  std::uint16_t inverseBasePoseOffset;
  std::uint16_t inverseBasePoseQuatOffset;
  std::uint16_t parentTableOffset;
  std::uint16_t jointHandleTblOffset;
  std::uint16_t userChannelHandleTblOffset;
  std::uint16_t jointWeightOffsets[8];
  std::uint16_t userWeightOffsets[8];
  std::uint8_t padding[12];
};

class idMD6Skel {
public:
  idJointIndex_t GetJointIndex(const char* name) const;
  idJointIndex_t GetJointIndex(const idJointHandle_t& handle) const;

private:
  std::uint8_t prefix[0x38];

public:
  idMD6SkelData* data;

private:
  std::uint8_t suffix[0x18];
};

enum animationPose_t {
  ANIMATION_POSE_CURRENT = 0,
  DRIVER_MODEL = 1
};

enum jointModTransform_t {
  JOINTMOD_NONE = 0,
  JOINTMOD_LOCAL_OVERRIDE = 2,
  JOINTMOD_MODEL_OVERRIDE = 4
};

class idMat3x4 {
public:
  float mat[12];
};

class idJointAnimator {
public:
  const idDeclMD6* decl;

  bool GetModelSpaceJointTransform(animationPose_t pose,
                                   idJointIndex_t jointIndex,
                                   idVec3& origin, idMat3& axis) const;
  bool GetModelSpaceJointAxis(animationPose_t pose,
                              idJointIndex_t jointIndex,
                              idMat3& axis) const;
  bool GetLocalSpaceJointBindAxis(idJointIndex_t jointIndex,
                                  idMat3& axis) const;
  void SetJointMat3x4(animationPose_t pose, idJointIndex_t jointIndex,
                      jointModTransform_t transform,
                      const idMat3x4& matrix, bool markDirty);
  void SetJointAxis(animationPose_t pose, idJointIndex_t jointIndex,
                    jointModTransform_t transform, const idMat3& axis);
};

static_assert(sizeof(idJointHandle_t) == 0x02,
              "idJointHandle_t ABI drift");
static_assert(sizeof(idMD6SkelData) == 0x40,
              "idMD6SkelData ABI drift");
static_assert(offsetof(idMD6SkelData, numJoints) == 0x02,
              "idMD6SkelData::numJoints ABI drift");
static_assert(offsetof(idMD6SkelData, jointHandleTblOffset) == 0x10,
              "idMD6SkelData::jointHandleTblOffset ABI drift");
static_assert(sizeof(idMD6Skel) == 0x58, "idMD6Skel ABI drift");
static_assert(offsetof(idMD6Skel, data) == 0x38,
              "idMD6Skel::data ABI drift");
static_assert(sizeof(idMat3x4) == 0x30, "idMat3x4 ABI drift");
