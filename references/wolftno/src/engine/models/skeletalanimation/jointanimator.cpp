#include "engine/models/skeletalanimation/jointanimator.h"

#include "engine/models/skeletalanimation/taginfo.h"
#include "engine/models/renderworld.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/parallelism/paralleljoblist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <new>
#include <cstdio>

extern idCVar r_disableBlendShapes;
extern int g_md6UtilGameTimePerSecond;

idCVar r_jointNameScale("r_jointNameScale", "0.02", CVAR_FLOAT,
                        "scale used for skeleton joint names");
idCVar r_jointNameOffset("r_jointNameOffset", "0.5", CVAR_FLOAT,
                         "offset used for skeleton joint names");
idCVar r_jointAxisScale("r_jointAxisScale", "1", CVAR_FLOAT,
                        "scale used for skeleton joint axes");
idCVar r_jointName("r_jointName", "", CVAR_RENDERER,
                   "draw only the named skeleton joint");
idCVar r_jointSubString("r_jointSubString", "", CVAR_RENDERER,
                        "draw skeleton joints containing this substring");
idCVar r_forceNonDeferredAnimBlend(
    "r_forceNonDeferredAnimBlend", "0", CVAR_BOOL,
    "force animation blending to finish on the calling thread");
idCVar r_forceDualQuatSkinning("r_forceDualQuatSkinning", "0", CVAR_BOOL,
                               "force dual quaternion skinning");
idCVar r_useAnimBlendJobs("r_useAnimBlendJobs", "1", CVAR_BOOL,
                          "run animation blending as parallel jobs");

namespace {

constexpr std::uint8_t AF_DEFERRED_READY = 1u << 0;
constexpr std::uint8_t AF_DUAL_QUAT = 1u << 1;
constexpr std::uint8_t AF_CALC_BOUNDS = 1u << 2;
constexpr std::uint8_t AF_CLEAR_ORIGIN = 1u << 3;
constexpr std::uint8_t AF_NORMALIZED_BOUNDS = 1u << 4;
constexpr std::uint8_t AF_TRANSLATED_BOUNDS = 1u << 5;
constexpr std::uint8_t AF_BLEND_SHAPES = 1u << 6;
constexpr int NUM_JOINT_BUFFERS = 10;
constexpr int MAX_ANIM_COMMANDS = 128;
constexpr memTag_t TAG_MD6_JOINTBUFFERS = static_cast<memTag_t>(0x14);
constexpr memTag_t TAG_MD6_MISC = static_cast<memTag_t>(0x15);

idJointAnimatorCallbacks jointAnimatorCallbacks{};

const idMD6SkelData* SkeletonData(const idJointAnimator& animator) {
  return animator.decl != nullptr && animator.decl->model != nullptr &&
                 animator.decl->model->skeleton != nullptr
             ? animator.decl->model->skeleton->data
             : nullptr;
}

int PaddedJoints(const idMD6SkelData* data) {
  return data != nullptr ? (data->numJoints + 7) & ~7 : 0;
}

int PaddedUserChannels(const idMD6SkelData* data) {
  return data != nullptr ? (data->numUserChannels + 7) & ~7 : 0;
}

const float* SkeletonFloats(const idMD6SkelData* data,
                            std::uint16_t offset) {
  return data != nullptr
             ? reinterpret_cast<const float*>(
                   reinterpret_cast<const std::uint8_t*>(data) + offset)
             : nullptr;
}

idMat3 JointAxis(const idJointMat& joint) {
  idMat3 result;
  result[0] = {joint.mat[0], joint.mat[4], joint.mat[8]};
  result[1] = {joint.mat[1], joint.mat[5], joint.mat[9]};
  result[2] = {joint.mat[2], joint.mat[6], joint.mat[10]};
  return result;
}

idVec3 JointOrigin(const idJointMat& joint) {
  return {joint.mat[3], joint.mat[7], joint.mat[11]};
}

idMat3 Transpose(const idMat3& matrix) {
  idMat3 result;
  result[0] = {matrix[0].x, matrix[1].x, matrix[2].x};
  result[1] = {matrix[0].y, matrix[1].y, matrix[2].y};
  result[2] = {matrix[0].z, matrix[1].z, matrix[2].z};
  return result;
}

void OrthoNormalize(idMat3& matrix) {
  matrix[0].Normalize();
  matrix[1] -= matrix[0] * (matrix[0] * matrix[1]);
  matrix[1].Normalize();
  matrix[2] = matrix[0].Cross(matrix[1]);
  matrix[2].Normalize();
}

idQuat QuatMultiply(const idQuat& lhs, const idQuat& rhs) {
  return {lhs.w * rhs.x + lhs.x * rhs.w + lhs.y * rhs.z - lhs.z * rhs.y,
          lhs.w * rhs.y - lhs.x * rhs.z + lhs.y * rhs.w + lhs.z * rhs.x,
          lhs.w * rhs.z + lhs.x * rhs.y - lhs.y * rhs.x + lhs.z * rhs.w,
          lhs.w * rhs.w - lhs.x * rhs.x - lhs.y * rhs.y - lhs.z * rhs.z};
}

idQuat InverseUnitQuat(const float* values) {
  const float lengthSquared = values[0] * values[0] + values[1] * values[1] +
                              values[2] * values[2] + values[3] * values[3];
  const float inverseLength =
      1.0f / std::sqrt(std::max(lengthSquared, 1.17549435e-38f));
  return {-values[0] * inverseLength, -values[1] * inverseLength,
          -values[2] * inverseLength, values[3]};
}

void ClearBounds(idBounds& bounds) {
  bounds[0] = {1.0e30f, 1.0e30f, 1.0e30f};
  bounds[1] = {-1.0e30f, -1.0e30f, -1.0e30f};
}

bool ValidBounds(const idBounds& bounds) {
  return bounds[0].x <= bounds[1].x && bounds[0].y <= bounds[1].y &&
         bounds[0].z <= bounds[1].z;
}

float BoundsVolume(const idBounds& bounds) {
  if (!ValidBounds(bounds))
    return 0.0f;
  return (bounds[1].x - bounds[0].x) *
         (bounds[1].y - bounds[0].y) *
         (bounds[1].z - bounds[0].z);
}

void ExpandModelBounds(idBounds& bounds, const idMD6Model* model) {
  if (!ValidBounds(bounds) || model == nullptr)
    return;
  bounds[0] += model->minBoundsExpansion;
  bounds[1] += model->maxBoundsExpansion;
}

void* Alloc(std::size_t size, memTag_t tag, bool zero, align_t alignment) {
  return idMem::Singleton()->AllocWithLocation(
      "w:\\build\\tech5\\engine\\models\\skeletalanimation\\JointAnimator.cpp",
      size, tag, zero, alignment, HEAP_DEFAULTHEAP);
}

void Free(void*& pointer, align_t alignment) {
  if (pointer == nullptr)
    return;
  idMem::Singleton()->Free(pointer, alignment);
  pointer = nullptr;
}

std::uint16_t ByteSwap16(std::uint16_t value) {
  return static_cast<std::uint16_t>((value >> 8) | (value << 8));
}

std::uint32_t ByteSwap32(std::uint32_t value) {
  return (value >> 24) | ((value >> 8) & 0x0000ff00u) |
         ((value << 8) & 0x00ff0000u) | (value << 24);
}

bool WriteBE32(idFile& file, std::uint32_t value) {
  value = ByteSwap32(value);
  return file.Write(&value, sizeof(value)) == sizeof(value);
}

bool ReadBE32(idFile& file, std::uint32_t& value) {
  if (file.Read(&value, sizeof(value)) != sizeof(value))
    return false;
  value = ByteSwap32(value);
  return true;
}

bool WriteBE16(idFile& file, std::uint16_t value) {
  value = ByteSwap16(value);
  return file.Write(&value, sizeof(value)) == sizeof(value);
}

bool ReadBE16(idFile& file, std::uint16_t& value) {
  if (file.Read(&value, sizeof(value)) != sizeof(value))
    return false;
  value = ByteSwap16(value);
  return true;
}

bool WriteBEFloats(idFile& file, const float* values, int count) {
  for (int index = 0; index < count; ++index) {
    std::uint32_t bits;
    std::memcpy(&bits, &values[index], sizeof(bits));
    if (!WriteBE32(file, bits))
      return false;
  }
  return true;
}

bool ReadBEFloats(idFile& file, float* values, int count) {
  for (int index = 0; index < count; ++index) {
    std::uint32_t bits;
    if (!ReadBE32(file, bits))
      return false;
    std::memcpy(&values[index], &bits, sizeof(bits));
  }
  return true;
}

bool WriteBounds(idFile& file, const idBounds& bounds) {
  return WriteBEFloats(file, &bounds[0].x, 6);
}

bool ReadBounds(idFile& file, idBounds& bounds) {
  return ReadBEFloats(file, &bounds[0].x, 6);
}

idJointIndex_t JointIndexForHandle(const idMD6SkelData* data,
                                   std::uint16_t handle) {
  if (data == nullptr || handle == 0xffffu)
    return idJointIndex_t(-1);
  const auto* handles = reinterpret_cast<const std::uint16_t*>(
      reinterpret_cast<const std::uint8_t*>(data) + data->jointHandleTblOffset);
  for (int index = 0; index < data->numJoints; ++index) {
    if (handles[index] == handle)
      return idJointIndex_t(static_cast<std::int16_t>(index));
  }
  return idJointIndex_t(-1);
}

void StoreAxis(idMD6Blend::jointMod_t& mod, const idMat3& axis) {
  mod.mat[0] = axis[0].x;
  mod.mat[1] = axis[1].x;
  mod.mat[2] = axis[2].x;
  mod.mat[4] = axis[0].y;
  mod.mat[5] = axis[1].y;
  mod.mat[6] = axis[2].y;
  mod.mat[8] = axis[0].z;
  mod.mat[9] = axis[1].z;
  mod.mat[10] = axis[2].z;
}

void AnimationBlendJobAdapter(void* data) {
  AnimationBlendJob(static_cast<idMD6Blend::blendParms_t*>(data));
}

idParallelJobRegistration register_AnimationBlendJob(
    AnimationBlendJobAdapter, "AnimationBlendJob");

} // namespace

void SetJointAnimatorCallbacks(const idJointAnimatorCallbacks& callbacks) {
  jointAnimatorCallbacks = callbacks;
}

void AnimationBlendJob(idMD6Blend::blendParms_t* blendParms) {
  if (blendParms == nullptr || blendParms->blendOutputs == nullptr)
    return;
  alignas(128) char temporary[0x10000];
  idMD6Blend::ExecuteCommands(
      blendParms->skeleton, temporary, sizeof(temporary), blendParms->cmds,
      blendParms->numCmds, blendParms->clearOriginTransform != 0,
      blendParms->boundsSkipJoint, blendParms->flags != 0,
      blendParms->config->visualOffset, true, blendParms->mods,
      blendParms->numMods, blendParms->invertedBasePose,
      blendParms->invertedBasePoseQuat, blendParms->jointRemap,
      reinterpret_cast<__m128*>(blendParms->referencePose),
      reinterpret_cast<__m128*>(blendParms->finalPose), blendParms->renderPose,
      blendParms->blendOutputs, blendParms->userChannels,
      blendParms->blendShapeEvals, blendParms->numBlendShapesEval);
  std::atomic_thread_fence(std::memory_order_release);
  blendParms->blendOutputs->done = true;
}

void AnimationBlendJobToRSTU(idMD6Blend::blendParms_t* blendParms,
                             float* localR, float* localS, float* localT,
                             float* localU) {
  if (blendParms == nullptr || blendParms->blendOutputs == nullptr)
    return;
  alignas(128) char temporary[0x10000];
  idMD6Blend::ExecuteCommandsToRSTU(
      blendParms->skeleton, temporary, sizeof(temporary), blendParms->cmds,
      blendParms->numCmds, blendParms->clearOriginTransform != 0,
      blendParms->boundsSkipJoint, blendParms->config->visualOffset, true,
      blendParms->mods, blendParms->numMods, blendParms->jointRemap,
      reinterpret_cast<__m128*>(blendParms->finalPose), localR, localS, localT,
      blendParms->blendOutputs, localU, blendParms->blendShapeEvals,
      blendParms->numBlendShapesEval);
  std::atomic_thread_fence(std::memory_order_release);
  blendParms->blendOutputs->done = true;
}

idJointAnimator::idJointAnimator(const idDeclMD6* declaration)
    : decl(declaration), commands(nullptr), blendParms(nullptr),
      skipJointForBounds(-1), currentDeferred(0), deferredJobJointBuffer(-1),
      nextRenderThreadJointBuffer(0), renderThreadJointBuffer(0),
      lastBlendTime(-1), joints{}, userChannels{}, blendOutputs{},
      animatorFlags(AF_CLEAR_ORIGIN) {
  const idMD6SkelData* data = SkeletonData(*this);
  if (data == nullptr || PaddedJoints(data) == 0)
    return;

  const std::size_t jointBytes =
      sizeof(idJointMat) * static_cast<std::size_t>(PaddedJoints(data));
  for (idJointMat*& joint : joints) {
    joint = static_cast<idJointMat*>(
        Alloc(jointBytes, TAG_MD6_JOINTBUFFERS, false, ALIGN_128));
  }
  const std::size_t channelBytes =
      sizeof(float) * static_cast<std::size_t>(PaddedUserChannels(data));
  for (float*& channel : userChannels) {
    channel = channelBytes == 0
                  ? nullptr
                  : static_cast<float*>(
                        Alloc(channelBytes, TAG_MD6_MISC, true, ALIGN_16));
  }
  for (md6BlendOutputs_t*& output : blendOutputs) {
    output = static_cast<md6BlendOutputs_t*>(
        Alloc(sizeof(md6BlendOutputs_t), TAG_MD6_MISC, false, ALIGN_16));
    if (output != nullptr)
      ::new (output) md6BlendOutputs_t;
  }
  blendParms = static_cast<idMD6Blend::blendParms_t*>(
      Alloc(sizeof(idMD6Blend::blendParms_t), TAG_MD6_MISC, true, ALIGN_16));
  commands = static_cast<md6AnimCommand_t*>(
      Alloc(sizeof(md6AnimCommand_t) * MAX_ANIM_COMMANDS, TAG_MD6_MISC, false,
            ALIGN_16));
  if (commands != nullptr) {
    for (int index = 0; index < MAX_ANIM_COMMANDS; ++index)
      commands[index].op = md6AnimCommand_t::OP_END;
  }
  if (blendParms != nullptr)
    blendParms->blendOutputs = blendOutputs[0];

  if (joints[0] != nullptr) {
    alignas(128) char temporary[0x4000];
    static const float zeroOffset[4]{};
    const float* visualOffset = decl->config != nullptr
                                    ? decl->config->visualOffset
                                    : zeroOffset;
    idMD6Blend::DecodeBasePose(data, reinterpret_cast<float*>(temporary),
                               sizeof(temporary), visualOffset,
                               true, reinterpret_cast<__m128*>(joints[0]));
    idMD6Blend::TransformModelMatrices(
        data, decl->model->jointRemap.list, joints[0]->mat,
        SkeletonFloats(data, data->inverseBasePoseOffset),
        SkeletonFloats(data, data->inverseBasePoseQuatOffset), joints[2]->mat,
        false, nullptr);
    for (idJointBuffer& buffer : jointBuffers)
      buffer.AllocBufferObject(joints[2]->mat, PaddedJoints(data));
    for (int index = 1; index < 4; ++index)
      std::memcpy(joints[index], joints[0], jointBytes);
  }
  SetDualQuaternion(false);
  frameBounds = decl->referenceBounds;
  normalizedBounds = decl->referenceBounds;
  translatedBounds = decl->referenceBounds;
}

idJointAnimator::~idJointAnimator() {
  for (idJointMat*& joint : joints)
    Free(reinterpret_cast<void*&>(joint), ALIGN_128);
  for (float*& channel : userChannels)
    Free(reinterpret_cast<void*&>(channel), ALIGN_16);
  for (md6BlendOutputs_t*& output : blendOutputs) {
    if (output != nullptr)
      output->~md6BlendOutputs_t();
    Free(reinterpret_cast<void*&>(output), ALIGN_16);
  }
  Free(reinterpret_cast<void*&>(blendParms), ALIGN_16);
  Free(reinterpret_cast<void*&>(commands), ALIGN_16);
  for (idJointBuffer& buffer : jointBuffers) {
    if (buffer.IsMapped())
      buffer.UnmapBuffer();
    buffer.FreeBufferObject();
  }
}

bool idJointAnimator::IsValidJoint(idJointIndex_t jointIndex) const {
  const idMD6SkelData* data = SkeletonData(*this);
  return data != nullptr && jointIndex.value >= 0 &&
         jointIndex.value < data->numJoints;
}

void idJointAnimator::CalcFrameBoundsFromJoints() {
  const idMD6SkelData* data = SkeletonData(*this);
  if (data == nullptr || joints[1] == nullptr)
    return;
  if (BoundsVolume(frameBounds) <= 0.0f) {
    ClearBounds(frameBounds);
    for (int index = 0; index < data->numJoints; ++index) {
      if (index == skipJointForBounds.value)
        continue;
      const idVec3 point = JointOrigin(joints[1][index]);
      frameBounds[0].x = std::min(frameBounds[0].x, point.x);
      frameBounds[0].y = std::min(frameBounds[0].y, point.y);
      frameBounds[0].z = std::min(frameBounds[0].z, point.z);
      frameBounds[1].x = std::max(frameBounds[1].x, point.x);
      frameBounds[1].y = std::max(frameBounds[1].y, point.y);
      frameBounds[1].z = std::max(frameBounds[1].z, point.z);
    }
    frameBounds[0] -= idVec3{16.0f, 16.0f, 16.0f};
    frameBounds[1] += idVec3{16.0f, 16.0f, 16.0f};
  }
  ExpandModelBounds(frameBounds, decl->model);
}

void idJointAnimator::CalcFrameBoundsFromBlendOutput() {
  if (blendOutputs[1] == nullptr)
    return;
  std::memcpy(&frameBounds[0].x, blendOutputs[1]->jointBounds,
              sizeof(blendOutputs[1]->jointBounds));
  ExpandModelBounds(frameBounds, decl != nullptr ? decl->model : nullptr);
}

bool idJointAnimator::SyncJoints() {
  if ((animatorFlags & AF_DEFERRED_READY) == 0)
    return false;
  std::swap(joints[2], joints[0]);
  std::swap(joints[3], joints[1]);
  std::swap(userChannels[0], userChannels[1]);
  std::swap(blendOutputs[0], blendOutputs[1]);
  if (deferredJobJointBuffer >= 0 && deferredJobJointBuffer < NUM_JOINT_BUFFERS &&
      jointBuffers[deferredJobJointBuffer].IsMapped())
    jointBuffers[deferredJobJointBuffer].UnmapBuffer();
  if ((decl != nullptr && decl->calcRefBoundsFromJoints) ||
      (animatorFlags & AF_CALC_BOUNDS) != 0 || BoundsVolume(frameBounds) <= 0.0f)
    CalcFrameBoundsFromBlendOutput();
  nextRenderThreadJointBuffer = deferredJobJointBuffer;
  animatorFlags &= 0xbeu;
  if (blendShapesEval.Num() > 0)
    animatorFlags |= AF_BLEND_SHAPES;
  return true;
}

void idJointAnimator::Reference(idJointBuffer* jointBuffer, int jointRefOffset,
                                int numRefJoints) {
  if (jointBuffer == nullptr || nextRenderThreadJointBuffer < 0 ||
      nextRenderThreadJointBuffer >= NUM_JOINT_BUFFERS)
    return;
  renderThreadJointBuffer = nextRenderThreadJointBuffer;
  idJointBuffer& source = jointBuffers[renderThreadJointBuffer];
  if (source.IsMapped())
    source.UnmapBuffer();
  jointBuffer->Reference(source, jointRefOffset, numRefJoints);
}

void idJointAnimator::SetDualQuaternion(bool enable) {
  animatorFlags = static_cast<std::uint8_t>((animatorFlags & ~AF_DUAL_QUAT) |
                                            (enable ? AF_DUAL_QUAT : 0));
  for (idJointBuffer& buffer : jointBuffers)
    buffer.flags = enable ? 1 : 0;
}

float idJointAnimator::GetBlendedUserChannel(idUserChannelIndex index) const {
  const idMD6SkelData* data = SkeletonData(*this);
  if (data == nullptr || userChannels[1] == nullptr || index.value < 0 ||
      index.value >= data->numUserChannels)
    return 0.0f;
  return userChannels[1][index.value];
}

bool idJointAnimator::GetModelSpaceJointBindTransform(
    idJointIndex_t jointIndex, idVec3& origin, idMat3& axis) const {
  if (!IsValidJoint(jointIndex))
    return false;
  const idMD6SkelData* data = SkeletonData(*this);
  idJointMat joint;
  std::memcpy(&joint,
              reinterpret_cast<const std::uint8_t*>(data) +
                  data->inverseBasePoseOffset +
                  sizeof(idJointMat) * jointIndex.value,
              sizeof(joint));
  joint.Invert();
  origin = JointOrigin(joint);
  axis = JointAxis(joint);
  return true;
}

bool idJointAnimator::GetModelSpaceJointPosition(animationPose_t pose,
                                                 idJointIndex_t jointIndex,
                                                 idVec3& origin) const {
  if (!IsValidJoint(jointIndex) || joints[pose != ANIMATION_POSE_CURRENT] == nullptr)
    return false;
  origin = JointOrigin(joints[pose != ANIMATION_POSE_CURRENT][jointIndex.value]);
  return true;
}

bool idJointAnimator::GetModelSpaceJointAxis(animationPose_t pose,
                                             idJointIndex_t jointIndex,
                                             idMat3& axis) const {
  if (!IsValidJoint(jointIndex) || joints[pose != ANIMATION_POSE_CURRENT] == nullptr)
    return false;
  axis = JointAxis(joints[pose != ANIMATION_POSE_CURRENT][jointIndex.value]);
  return true;
}

bool idJointAnimator::GetModelSpaceJointTransform(animationPose_t pose,
                                                  idJointIndex_t jointIndex,
                                                  idVec3& origin,
                                                  idMat3& axis) const {
  return GetModelSpaceJointPosition(pose, jointIndex, origin) &&
         GetModelSpaceJointAxis(pose, jointIndex, axis);
}

bool idJointAnimator::GetModelSpaceAttachmentTransform(
    idJointIndex_t jointIndex, const idVec3& translation,
    const idQuat& rotation, idVec3& origin, idMat3& axis) const {
  if (!IsValidJoint(jointIndex) || joints[1] == nullptr)
    return false;
  const idJointMat& joint = joints[1][jointIndex.value];
  const idMat3 jointAxis = JointAxis(joint);
  origin = jointAxis * translation + JointOrigin(joint);
  axis = rotation.ToMat3() * jointAxis;
  return true;
}

bool idJointAnimator::GetModelSpaceAttachmentPosition(
    idJointIndex_t jointIndex, const idVec3& translation, const idQuat&,
    idVec3& origin) const {
  if (!IsValidJoint(jointIndex) || joints[1] == nullptr)
    return false;
  const idJointMat& joint = joints[1][jointIndex.value];
  origin = JointAxis(joint) * translation + JointOrigin(joint);
  return true;
}

bool idJointAnimator::GetModelSpaceAttachmentAxis(
    idJointIndex_t jointIndex, const idVec3&, const idQuat& rotation,
    idMat3& axis) const {
  if (!IsValidJoint(jointIndex) || joints[1] == nullptr)
    return false;
  axis = rotation.ToMat3() * JointAxis(joints[1][jointIndex.value]);
  return true;
}

bool idJointAnimator::GetModelSpaceJointFrameDeltas(
    const idJointIndex_t* indices, int numIndices, idVec3* translations,
    idMat3* rotations) const {
  if (numIndices < 0 || (numIndices > 0 &&
      (indices == nullptr || translations == nullptr || rotations == nullptr)) ||
      joints[1] == nullptr || joints[3] == nullptr)
    return false;
  for (int index = 0; index < numIndices; ++index) {
    if (!IsValidJoint(indices[index]))
      return false;
    const idJointMat& current = joints[1][indices[index].value];
    const idJointMat& previous = joints[3][indices[index].value];
    translations[index] = JointOrigin(current) - JointOrigin(previous);
    rotations[index] = JointAxis(current) * Transpose(JointAxis(previous));
  }
  return true;
}

bool idJointAnimator::GetModelSpaceReferenceJointFrameDeltas(
    const idJointIndex_t* indices, int numIndices, idVec3* translations,
    idMat3* rotations) const {
  if (numIndices < 0 || (numIndices > 0 &&
      (indices == nullptr || translations == nullptr || rotations == nullptr)) ||
      joints[0] == nullptr || joints[2] == nullptr)
    return false;
  for (int index = 0; index < numIndices; ++index) {
    if (!IsValidJoint(indices[index]))
      return false;
    const idJointMat& current = joints[0][indices[index].value];
    const idJointMat& previous = joints[2][indices[index].value];
    translations[index] = JointOrigin(current) - JointOrigin(previous);
    rotations[index] = JointAxis(current) * Transpose(JointAxis(previous));
  }
  return true;
}

bool idJointAnimator::GetModelSpaceAttachmentFrameDeltas(
    idJointIndex_t jointIndex, const idVec3& translation, const idQuat&,
    idVec3& translationDelta, idMat3& rotationDelta) const {
  if (!GetModelSpaceJointFrameDeltas(&jointIndex, 1, &translationDelta,
                                     &rotationDelta))
    return false;
  translationDelta += rotationDelta * translation - translation;
  return true;
}

void idJointAnimator::DrawJoints(
    idRenderDebug* renderDebug, animationPose_t pose, const idVec3& origin,
    const idMat3& axis, const idVec3& scale,
    const idList<idJointIndex_t, 5>* selectedJoints, bool showBones,
    bool showAxis, bool showBounds, bool showJointNames,
    const idColor* lineColor) const {
  if (jointAnimatorCallbacks.drawJoints != nullptr) {
    jointAnimatorCallbacks.drawJoints(*this, renderDebug, pose, origin, axis,
                                      scale, selectedJoints, showBones,
                                      showAxis, showBounds, showJointNames,
                                      lineColor);
    return;
  }
  if (renderDebug == nullptr ||
      (!showBones && !showAxis && !showBounds && !showJointNames) ||
      decl == nullptr || decl->model == nullptr ||
      decl->model->skeleton == nullptr)
    return;
  const idMD6Skel* skeleton = decl->model->skeleton;
  const idMD6SkelData* data = skeleton->data;
  if (data == nullptr || joints[pose != ANIMATION_POSE_CURRENT] == nullptr)
    return;
  const auto* parents = reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<const std::uint8_t*>(data) + data->parentTblOffset);
  const auto* handles = reinterpret_cast<const std::uint16_t*>(
      reinterpret_cast<const std::uint8_t*>(data) + data->jointHandleTblOffset);
  const char* exactName = r_jointName.GetString();
  const char* substring = r_jointSubString.GetString();
  const idVec4 defaultLineColor{1.0f, 1.0f, 1.0f, 1.0f};
  const idVec4& boneColor = lineColor != nullptr
                                ? static_cast<const idVec4&>(*lineColor)
                                : defaultLineColor;
  const idVec4 green{0.0f, 1.0f, 0.0f, 1.0f};
  const idVec4 red{1.0f, 0.0f, 0.0f, 1.0f};
  const idVec4 blue{0.0f, 0.0f, 1.0f, 1.0f};

  auto selected = [selectedJoints](int joint) {
    if (selectedJoints == nullptr)
      return true;
    for (int index = 0; index < selectedJoints->Num(); ++index) {
      if ((*selectedJoints)[index].value == joint)
        return true;
    }
    return false;
  };
  auto jointName = [skeleton](int joint) -> const char* {
    return joint >= 0 && joint < skeleton->NumJoints()
               ? skeleton->GetJointName(joint)
               : nullptr;
  };
  auto worldOrigin = [&origin, &axis, &scale](const idJointMat& joint) {
    idVec3 local = JointOrigin(joint);
    local.x *= scale.x;
    local.y *= scale.y;
    local.z *= scale.z;
    return axis * local + origin;
  };

  const idJointMat* poseJoints = joints[pose != ANIMATION_POSE_CURRENT];
  for (int joint = 0; joint < data->numJoints; ++joint) {
    if (handles[joint] == 0xffffu || !selected(joint))
      continue;
    const char* name = jointName(joint);
    if (exactName != nullptr && exactName[0] != '\0' &&
        (name == nullptr || idStr::Icmp(name, exactName) != 0))
      continue;
    if (substring != nullptr && substring[0] != '\0' &&
        (name == nullptr || std::strstr(name, substring) == nullptr))
      continue;
    const idVec3 position = worldOrigin(poseJoints[joint]);
    const idMat3 worldAxis = JointAxis(poseJoints[joint]) * axis;
    if (showBones && parents[joint] >= 0 &&
        (exactName == nullptr || exactName[0] == '\0')) {
      renderDebug->DebugLine(boneColor, position,
                             worldOrigin(poseJoints[parents[joint]]), 0, false);
    }
    if (showAxis) {
      const float axisScale = r_jointAxisScale.GetFloat();
      renderDebug->DebugLine(green, position,
                             position + worldAxis[0] * axisScale, 0, false);
      renderDebug->DebugLine(red, position,
                             position + worldAxis[1] * axisScale, 0, false);
      renderDebug->DebugLine(blue, position,
                             position + worldAxis[2] * axisScale, 0, false);
    }
    if (showJointNames) {
      char label[256];
      if (name != nullptr)
        std::snprintf(label, sizeof(label), "%s (%d)", name, joint);
      else
        std::snprintf(label, sizeof(label), "%d", joint);
      const idVec3 textOrigin =
          position + idVec3{0.0f, 0.0f, r_jointNameOffset.GetFloat()};
      renderDebug->DebugText(label, textOrigin, r_jointNameScale.GetFloat(),
                             defaultLineColor, 1, 0, false, false);
    }
  }
}

void idJointAnimator::SetFromAnim(
    const idMD6Anim* anim, int time, bool, bool checkForOriginTranslation,
    const idVec3* translationScale, const idMD6Blend::jointMod_t* mods,
    int numMods) {
  const idMD6SkelData* data = SkeletonData(*this);
  if (anim == nullptr || anim->animData == nullptr || data == nullptr ||
      joints[0] == nullptr || joints[1] == nullptr ||
      !anim->AnimIsOkForModel(decl->model))
    return;
  const bool retainOrigin =
      checkForOriginTranslation && (animatorFlags & AF_CLEAR_ORIGIN) != 0;
  alignas(128) char temporary[0x8000];
  static const float zeroOffset[4]{};
  const float* visualOffset = decl->config != nullptr
                                  ? decl->config->visualOffset
                                  : zeroOffset;
  if (translationScale != nullptr) {
    idMD6Blend::DecodeSingleFrameWithOriginDeltaScale(
        data, temporary, mods != nullptr ? 0x8000u : 0x4000u, anim->animData,
        time, g_md6UtilGameTimePerSecond,
        (animatorFlags & AF_CLEAR_ORIGIN) != 0, visualOffset, !retainOrigin,
        reinterpret_cast<__m128*>(joints[1]), userChannels[1],
        &translationScale->x);
  } else {
    idMD6Blend::DecodeSingleFrame(
        data, temporary, mods != nullptr ? 0x8000u : 0x4000u, anim->animData,
        time, g_md6UtilGameTimePerSecond,
        (animatorFlags & AF_CLEAR_ORIGIN) != 0, visualOffset, !retainOrigin,
        reinterpret_cast<__m128*>(joints[1]), userChannels[1], mods, numMods);
  }
  idMD6Blend::TransformModelMatrices(
      data, decl->model->jointRemap.list, joints[1]->mat,
      SkeletonFloats(data, data->inverseBasePoseOffset),
      SkeletonFloats(data, data->inverseBasePoseQuatOffset), joints[0]->mat,
      (animatorFlags & AF_DUAL_QUAT) != 0, nullptr);
  nextRenderThreadJointBuffer = (renderThreadJointBuffer + 1) % NUM_JOINT_BUFFERS;
  jointBuffers[nextRenderThreadJointBuffer].Update(joints[0]->mat,
                                                   PaddedJoints(data));
  std::memcpy(joints[0], joints[1],
              sizeof(idJointMat) * static_cast<std::size_t>(PaddedJoints(data)));

  ClearBounds(frameBounds);
  if (!decl->calcRefBoundsFromJoints &&
      (animatorFlags & AF_CALC_BOUNDS) == 0 &&
      (anim->animData->flags & 0x21u) == 0) {
    auto shortBounds = [](const idBoundsShort& source) {
      idBounds result;
      for (int side = 0; side < 2; ++side)
        for (int axis = 0; axis < 3; ++axis)
          result[side][axis] = static_cast<float>(source.b[side][axis]);
      return result;
    };
    translatedBounds = shortBounds(anim->translatedBounds);
    normalizedBounds = shortBounds(anim->normalizedBounds);
    frameBounds = ((animatorFlags & AF_CLEAR_ORIGIN) != 0 &&
                   (anim->animData->flags & 0x0eu) != 0)
                      ? normalizedBounds
                      : translatedBounds;
  }
  CalcFrameBoundsFromJoints();
}

bool idJointAnimator::GetLocalSpaceJointBindAxis(idJointIndex_t jointIndex,
                                                 idMat3& axis) const {
  if (!IsValidJoint(jointIndex))
    return false;
  const idMD6SkelData* data = SkeletonData(*this);
  const auto* inverseBase = reinterpret_cast<const idJointMat*>(
      reinterpret_cast<const std::uint8_t*>(data) + data->inverseBasePoseOffset);
  idJointMat joint = inverseBase[jointIndex.value];
  joint.Invert();
  axis = JointAxis(joint);
  const auto* parents = reinterpret_cast<const std::int16_t*>(
      reinterpret_cast<const std::uint8_t*>(data) + data->parentTblOffset);
  const int parent = parents[jointIndex.value];
  if (parent >= 0)
    axis = axis * JointAxis(inverseBase[parent]);
  return true;
}

bool idJointAnimator::GetModelSpaceTagTransform(const tagData_t& tag,
                                                idVec3& origin,
                                                idMat3& axis) const {
  return GetModelSpaceAttachmentTransform(
      JointIndexForHandle(SkeletonData(*this), tag.parentJoint.value), tag.trans,
      tag.rot, origin, axis);
}

bool idJointAnimator::GetModelSpaceTagPosition(const tagData_t& tag,
                                               idVec3& origin) const {
  return GetModelSpaceAttachmentPosition(
      JointIndexForHandle(SkeletonData(*this), tag.parentJoint.value), tag.trans,
      tag.rot, origin);
}

void idJointAnimator::ApplyOriginDeltas(idVec3& outOrigin, idMat3& outAxis,
                                        const idVec3& origin,
                                        const idMat3& axis,
                                        const idVec3& scale) const {
  if (blendOutputs[1] == nullptr) {
    outOrigin = origin;
    outAxis = axis;
    return;
  }
  const md6BlendOutputs_t& output = *blendOutputs[1];
  const idQuat inverseDelta = InverseUnitQuat(output.deltaRotation);
  outAxis = inverseDelta.ToMat3() * axis;
  OrthoNormalize(outAxis);
  idVec3 translation{scale.x * output.deltaTranslation[0],
                     scale.y * output.deltaTranslation[1],
                     scale.z * output.deltaTranslation[2]};
  const idQuat animRotation{output.animRotation[0], output.animRotation[1],
                            output.animRotation[2], output.animRotation[3]};
  translation = animRotation.ToMat3() * translation;
  outOrigin = outAxis * translation + origin;
}

void idJointAnimator::ApplyOriginDeltas(idVec3& outOrigin, idQuat& outAxis,
                                        const idVec3& origin,
                                        const idQuat& axis,
                                        const idVec3& scale) const {
  if (blendOutputs[1] == nullptr) {
    outOrigin = origin;
    outAxis = axis;
    return;
  }
  const md6BlendOutputs_t& output = *blendOutputs[1];
  outAxis = QuatMultiply(InverseUnitQuat(output.deltaRotation), axis);
  outAxis.Normalize();
  idVec3 translation{scale.x * output.deltaTranslation[0],
                     scale.y * output.deltaTranslation[1],
                     scale.z * output.deltaTranslation[2]};
  const idQuat animRotation{output.animRotation[0], output.animRotation[1],
                            output.animRotation[2], output.animRotation[3]};
  translation = animRotation.ToMat3() * translation;
  outOrigin = outAxis.ToMat3() * translation + origin;
}

void idJointAnimator::Save(idFile& file) const {
  if (jointAnimatorCallbacks.save != nullptr) {
    jointAnimatorCallbacks.save(*this, file);
    return;
  }
  const idMD6SkelData* data = SkeletonData(*this);
  const int matrixFloats = 12 * PaddedJoints(data);
  WriteBE32(file, static_cast<std::uint32_t>(lastBlendTime));
  const std::uint8_t bits[] = {
      static_cast<std::uint8_t>((animatorFlags & AF_DEFERRED_READY) != 0),
      static_cast<std::uint8_t>((animatorFlags & AF_CLEAR_ORIGIN) != 0),
      static_cast<std::uint8_t>((animatorFlags & AF_NORMALIZED_BOUNDS) != 0),
      static_cast<std::uint8_t>((animatorFlags & AF_CALC_BOUNDS) != 0)};
  file.Write(bits, sizeof(bits));
  WriteBE32(file, static_cast<std::uint32_t>(deferredJobJointBuffer));
  WriteBE32(file, static_cast<std::uint32_t>(matrixFloats * sizeof(float)));
  WriteBEFloats(file, joints[0]->mat, matrixFloats);
  WriteBEFloats(file, joints[1]->mat, matrixFloats);
  WriteBounds(file, frameBounds);
  WriteBounds(file, normalizedBounds);
  WriteBounds(file, translatedBounds);
  WriteBE16(file, static_cast<std::uint16_t>(skipJointForBounds.value));
}

bool idJointAnimator::Load(idFile& file) {
  if (jointAnimatorCallbacks.load != nullptr)
    return jointAnimatorCallbacks.load(*this, file);
  const idMD6SkelData* data = SkeletonData(*this);
  if (data == nullptr)
    return false;
  std::uint32_t value;
  if (!ReadBE32(file, value))
    return false;
  lastBlendTime = static_cast<int>(value);
  std::uint8_t bits[4];
  if (file.Read(bits, sizeof(bits)) != sizeof(bits))
    return false;
  animatorFlags = static_cast<std::uint8_t>(
      (animatorFlags & ~(AF_DEFERRED_READY | AF_CLEAR_ORIGIN |
                         AF_NORMALIZED_BOUNDS | AF_CALC_BOUNDS)) |
      (bits[0] ? AF_DEFERRED_READY : 0) | (bits[1] ? AF_CLEAR_ORIGIN : 0) |
      (bits[2] ? AF_NORMALIZED_BOUNDS : 0) |
      (bits[3] ? AF_CALC_BOUNDS : 0));
  if (!ReadBE32(file, value))
    return false;
  deferredJobJointBuffer = static_cast<int>(value);
  if (!ReadBE32(file, value))
    return false;
  const int matrixFloats = 12 * PaddedJoints(data);
  if (value != static_cast<std::uint32_t>(matrixFloats * sizeof(float)) ||
      !ReadBEFloats(file, joints[0]->mat, matrixFloats) ||
      !ReadBEFloats(file, joints[1]->mat, matrixFloats) ||
      !ReadBounds(file, frameBounds) || !ReadBounds(file, normalizedBounds) ||
      !ReadBounds(file, translatedBounds))
    return false;
  std::uint16_t skip;
  if (!ReadBE16(file, skip))
    return false;
  skipJointForBounds.value = static_cast<std::int16_t>(skip);
  idMD6Blend::TransformModelMatrices(
      data, decl->model->jointRemap.list, joints[1]->mat,
      SkeletonFloats(data, data->inverseBasePoseOffset),
      SkeletonFloats(data, data->inverseBasePoseQuatOffset), joints[3]->mat,
      (animatorFlags & AF_DUAL_QUAT) != 0, nullptr);
  for (idJointBuffer& buffer : jointBuffers)
    buffer.Update(joints[3]->mat, PaddedJoints(data));
  animatorFlags &= ~AF_DEFERRED_READY;
  const std::size_t jointBytes = sizeof(idJointMat) * PaddedJoints(data);
  std::memcpy(joints[2], joints[0], jointBytes);
  std::memcpy(joints[3], joints[1], jointBytes);
  return true;
}

void idJointAnimator::ClearJointMods() {
  jointMods[currentDeferred & 1].Clear();
}

void idJointAnimator::BlendTreeInternal(
    int currentTime, int previousTime, int gameMsPerFrame, int ticksPerSec,
    idMD6FusionBranch* tree, idParallelJobList* parallelJobList, float* localR,
    float* localS, float* localT, float* localU) {
  if ((tree == nullptr || !tree->IsValid()) &&
      jointMods[currentDeferred & 1].Num() == 0)
    return;
  const idMD6SkelData* data = SkeletonData(*this);
  if (data == nullptr || blendParms == nullptr || blendOutputs[0] == nullptr)
    return;
  lastBlendTime = currentTime;
  if (localR == nullptr) {
    while (!blendParms->blendOutputs->done)
      std::atomic_thread_fence(std::memory_order_acquire);
  }
  const bool dualQuat = r_forceDualQuatSkinning.GetBool();
  if (((animatorFlags & AF_DUAL_QUAT) != 0) != dualQuat)
    SetDualQuaternion(dualQuat);
  int bufferIndex = (nextRenderThreadJointBuffer + 1) % NUM_JOINT_BUFFERS;
  if (bufferIndex == deferredJobJointBuffer) {
    if (nextRenderThreadJointBuffer != renderThreadJointBuffer)
      jointBuffers[nextRenderThreadJointBuffer].Swap(
          jointBuffers[deferredJobJointBuffer]);
    else if (jointBuffers[bufferIndex].IsMapped()) {
      for (int offset = 1; offset < NUM_JOINT_BUFFERS; ++offset) {
        const int candidate = (bufferIndex + offset) % NUM_JOINT_BUFFERS;
        if (!jointBuffers[candidate].IsMapped() &&
            candidate != renderThreadJointBuffer) {
          bufferIndex = candidate;
          break;
        }
      }
      if (jointBuffers[bufferIndex].IsMapped())
        return;
    }
  }
  deferredJobJointBuffer = bufferIndex;
  const int elapsed = std::clamp(currentTime - previousTime, 0, gameMsPerFrame);
  ClearBounds(translatedBounds);
  ClearBounds(normalizedBounds);
  idBounds animationBounds;
  ClearBounds(animationBounds);
  std::uint16_t parseFlags = (animatorFlags & AF_CLEAR_ORIGIN) != 0 ? 1u : 0u;
  if ((animatorFlags & AF_NORMALIZED_BOUNDS) != 0)
    parseFlags |= 2u;
  if ((animatorFlags & AF_TRANSLATED_BOUNDS) != 0)
    parseFlags |= 4u;
  blendParms->skeleton = data;
  blendParms->config = decl->config;
  blendParms->cmds = commands;
  blendParms->numCmds = ParseTree(
      decl->model, currentTime - elapsed, currentTime, ticksPerSec, tree,
      commands, MAX_ANIM_COMMANDS, animationBounds, translatedBounds,
      normalizedBounds, blendOutputs[0], parseFlags);
  if (!decl->calcRefBoundsFromJoints &&
      (animatorFlags & AF_CALC_BOUNDS) == 0 && ValidBounds(animationBounds) &&
      !animationBounds.IsZero()) {
    frameBounds = animationBounds;
    CalcFrameBoundsFromJoints();
  }
  blendParms->mods = jointMods[currentDeferred & 1].list;
  blendParms->numMods = jointMods[currentDeferred & 1].num;
  blendParms->invertedBasePose =
      SkeletonFloats(data, data->inverseBasePoseOffset);
  blendParms->invertedBasePoseQuat =
      SkeletonFloats(data, data->inverseBasePoseQuatOffset);
  blendParms->jointRemap = decl->model->jointRemap.list;
  blendParms->referencePose = joints[2]->mat;
  blendParms->finalPose = joints[3]->mat;
  blendParms->renderPose =
      jointBuffers[deferredJobJointBuffer].MapBuffer(BM_WRITE_NOSYNC);
  blendParms->clearOriginTransform =
      (animatorFlags & AF_CLEAR_ORIGIN) != 0;
  blendParms->flags = (animatorFlags & AF_DUAL_QUAT) != 0;
  blendParms->blendOutputs = blendOutputs[0];
  blendParms->blendOutputs->time = currentTime;
  blendParms->blendOutputs->done = false;
  blendParms->userChannels = userChannels[0];
  blendParms->boundsSkipJoint = skipJointForBounds;
  blendParms->blendShapeEvals = blendShapesEval.list;
  blendParms->numBlendShapesEval = r_disableBlendShapes.GetBool()
                                        ? 0u
                                        : blendShapesEval.num;
  bool syncNow = false;
  if (localR != nullptr) {
    AnimationBlendJobToRSTU(blendParms, localR, localS, localT, localU);
    syncNow = true;
  } else if (parallelJobList != nullptr &&
             !r_forceNonDeferredAnimBlend.GetBool()) {
    if (r_useAnimBlendJobs.GetBool())
      parallelJobList->AddJob(AnimationBlendJobAdapter, blendParms);
    else
      AnimationBlendJob(blendParms);
  } else {
    AnimationBlendJob(blendParms);
    syncNow = true;
  }
  animatorFlags |= AF_DEFERRED_READY;
  if (syncNow)
    SyncJoints();
  currentDeferred ^= 1;
  ClearJointMods();
}

std::uint16_t idJointAnimator::JointModFlags(animationPose_t pose,
                                             jointModTransform_t type,
                                             std::uint16_t baseFlags) {
  if (type == JOINTMOD_MODEL || type == JOINTMOD_MODEL_OVERRIDE)
    baseFlags |= 1u;
  if (type == JOINTMOD_MODEL_OVERRIDE || type == JOINTMOD_LOCAL_OVERRIDE)
    baseFlags |= 0x20u;
  if (pose == ANIMATION_POSE_CURRENT)
    baseFlags |= 0x4000u;
  return baseFlags;
}

void idJointAnimator::SetJointPosAxisScale(
    animationPose_t pose, idJointIndex_t jointIndex, jointModTransform_t type,
    const idVec3& pos, const idMat3& axis, const idVec3& scale) {
  idMD6Blend::jointMod_t* mod = AllocJointMod();
  if (mod == nullptr)
    return;
  mod->mat[3] = pos.x;
  mod->mat[7] = pos.y;
  mod->mat[11] = pos.z;
  StoreAxis(*mod, axis);
  std::memcpy(mod->s, &scale, sizeof(scale));
  mod->flags = JointModFlags(pose, type, 0x800eu);
  mod->joint = jointIndex;
}

void idJointAnimator::SetJointMat3x4(animationPose_t pose,
                                     idJointIndex_t jointIndex,
                                     jointModTransform_t type,
                                     const idMat3x4& matrix,
                                     bool setPosition) {
  idMD6Blend::jointMod_t* mod = AllocJointMod();
  if (mod == nullptr)
    return;
  std::memcpy(mod->mat, matrix.mat, sizeof(matrix.mat));
  std::memset(mod->s, 0, sizeof(mod->s));
  mod->flags = JointModFlags(pose, type, setPosition ? 0x800au : 0x8002u);
  mod->joint = jointIndex;
}

void idJointAnimator::SetJointPos(animationPose_t pose,
                                  idJointIndex_t jointIndex,
                                  jointModTransform_t type,
                                  const idVec3& pos) {
  idMD6Blend::jointMod_t* mod = AllocJointMod();
  if (mod == nullptr)
    return;
  std::memset(mod, 0, sizeof(*mod));
  mod->mat[3] = pos.x;
  mod->mat[7] = pos.y;
  mod->mat[11] = pos.z;
  mod->flags = JointModFlags(pose, type, 0x8008u);
  mod->joint = jointIndex;
}

void idJointAnimator::SetJointAxis(animationPose_t pose,
                                   idJointIndex_t jointIndex,
                                   jointModTransform_t type,
                                   const idMat3& axis) {
  idMD6Blend::jointMod_t* mod = AllocJointMod();
  if (mod == nullptr)
    return;
  StoreAxis(*mod, axis);
  mod->flags = JointModFlags(pose, type, 0x8002u);
  mod->joint = jointIndex;
}

void idJointAnimator::SetJointScale(animationPose_t pose,
                                    idJointIndex_t jointIndex,
                                    jointModTransform_t type,
                                    const idVec3& scale) {
  idMD6Blend::jointMod_t* mod = AllocJointMod();
  if (mod == nullptr)
    return;
  std::memcpy(mod->s, &scale, sizeof(scale));
  mod->flags = JointModFlags(pose, type, 0x8004u);
  mod->joint = jointIndex;
}

idMD6Blend::jointMod_t* idJointAnimator::AllocJointMod() {
  return jointMods[currentDeferred & 1].Alloc();
}
