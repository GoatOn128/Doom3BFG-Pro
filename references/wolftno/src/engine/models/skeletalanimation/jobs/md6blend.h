#pragma once

#include "engine/models/skeletalanimation/jointconversion.h"
#include "engine/models/skeletalanimation/md6parsetree.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>
#include <immintrin.h>

class idUserChannelExpression;

// Serialized MD6 skeleton header consumed directly by the blend job.  Every
// offset is relative to the beginning of this header.
struct idMD6SkelData {
  static constexpr int MAX_WEIGHT_SETS = 8;
  static constexpr int SIMD_PAD_SIZE = 8;
  static constexpr int DUMMY_JOINT_PAD_SIZE = 8;
  static constexpr int DUMMY_USER_CHANNEL_PAD_SIZE = 8;

  std::uint16_t size;
  std::uint16_t numJoints;
  std::uint16_t numUserChannels;
  idCrcHandle_t parentTblCrc;
  std::uint16_t basePoseOffset;
  std::uint16_t inverseBasePoseOffset;
  std::uint16_t inverseBasePoseQuatOffset;
  std::uint16_t parentTblOffset;
  std::uint16_t jointHandleTblOffset;
  std::uint16_t userChannelHandleTblOffset;
  std::uint16_t jointWeightOffsets[MAX_WEIGHT_SETS];
  std::uint16_t userWeightOffsets[MAX_WEIGHT_SETS];
  std::uint8_t pad[12];
};

namespace quat_t {
void ToMat3(const float* q, idMat3* out);
}

void BlendShapeFirst(__m128i* destMap, const __m128i* sourceData,
                     int numVerts, float blendValue);
void BlendShapeAdd(__m128i* destMap, const __m128i* source2Data,
                   const __m128i* sourceData, int numVerts,
                   std::uint32_t blendValue);

namespace idMD6Blend {

static constexpr int MAX_FRAMESET_SIZE = 256;
static constexpr int MIN_BLEND_STACK_SIZE = 0x2000;
static constexpr int MIN_BASEPOSE_TEMP_SIZE = 0x2000;
static constexpr int MIN_SINGLEFRAME_TEMP_SIZE = 0x2000;
static constexpr int MAX_BLENDSHAPES = 32;

struct configInfo_t { float visualOffset[4]; };

struct frameOffsets_t {
  std::uint8_t leftFrameOffset[MAX_FRAMESET_SIZE];
  std::uint8_t rightFrameOffset[MAX_FRAMESET_SIZE];
  std::uint16_t leftKeyOffset[MAX_FRAMESET_SIZE];
  std::uint16_t rightKeyOffset[MAX_FRAMESET_SIZE];
};

struct doubleStack_t {
  char* start;
  char* end;
  char* top;
  char* bot;
};

struct blendShapeEval_t {
  idUserChannelExpression* expressions[MAX_BLENDSHAPES];
  const std::uint8_t* sourceDatas[MAX_BLENDSHAPES];
  std::uint8_t* destData;
  int numVerts;
  std::uint8_t numBlendShapes;
  std::uint8_t pad[3];
};

struct blendParms_t {
  const idMD6SkelData* skeleton;
  const configInfo_t* config;
  const md6AnimCommand_t* cmds;
  const jointMod_t* mods;
  const float* invertedBasePose;
  const float* invertedBasePoseQuat;
  const std::uint8_t* jointRemap;
  std::uint32_t numCmds;
  std::uint32_t numMods;
  std::uint32_t clearOriginTransform;
  std::uint32_t flags;
  std::uint32_t numBlendShapesEval;
  idJointIndex_t boundsSkipJoint;
  float* referencePose;
  float* finalPose;
  float* renderPose;
  float* userChannels;
  md6BlendOutputs_t* blendOutputs;
  blendShapeEval_t* blendShapeEvals;
};

void DecodeBasePoseRST(std::uint32_t numJoints, const float* inR,
                       const float* inS, const float* inT, float* outR,
                       float* outS, float* outT);
void DecodeBasePoseU(std::uint32_t numUserChannels, const float* inU,
                     float* outU);
void DecodeConstantR(const std::uint8_t* jointsR, std::uint32_t numJointsR,
                     const char* inR, float* outR);
void DecodeConstantU(const std::uint8_t* channelsU,
                     std::uint32_t numUserChannels, const float* inU,
                     float* outU);
std::int64_t DecodeFrameOffsets(int numJoints, std::uint32_t frame,
                                std::uint32_t numFrames,
                                std::uint32_t stride, const void* firstKeys,
                                const void* rangeKeys, const void* nextKeys,
                                const std::uint8_t* frameBits,
                                frameOffsets_t* offsets, std::uint32_t pad);
void DecodeFrameSetR(const std::uint8_t* joints, int numJoints,
                     std::uint32_t frame, std::uint32_t numFrames,
                     std::uint32_t fraction, const std::int16_t* firstKeys,
                     const std::int16_t* rangeKeys,
                     const std::int16_t* nextKeys,
                     const std::uint8_t* frameBits, float* out);
void DecodeFrameSetST(const std::uint8_t* joints, int numJoints,
                      std::uint32_t frame, std::uint32_t numFrames,
                      std::uint32_t fraction, const float* firstKeys,
                      const float* rangeKeys, const float* nextKeys,
                      const std::uint8_t* frameBits, float* out);
void DecodeFrameSetU(const std::uint8_t* channelsU, int numUserChannels,
                     std::uint32_t frame, std::uint32_t numFrames,
                     std::uint32_t fraction, const float* firstKeys,
                     const float* rangeKeys, const float* nextKeys,
                     const std::uint8_t* frameBits, float* out);

void LerpRST(std::uint32_t numJoints, float alpha,
             const std::uint8_t* filter, const float* aR, const float* aS,
             const float* aT, const std::uint8_t* aW, const float* bR,
             const float* bS, const float* bT, std::uint32_t* bW,
             float* outR, float* outS, float* outT, std::uint8_t* outW);
void AddRST(std::uint32_t numJoints, float alpha,
            const std::uint8_t* filter, const float* aR, const float* aS,
            const float* aT, const std::uint8_t* aW, const float* bR,
            const float* bS, const float* bT, const std::uint8_t* bW,
            float* outR, float* outS, float* outT, std::uint8_t* outW);
void SubRST(std::uint32_t numJoints, float alpha,
            const std::uint8_t* filter, const float* aR, const float* aS,
            const float* aT, const std::uint8_t* aW, const float* bR,
            const float* bS, const float* bT, const std::uint8_t* bW,
            float* outR, float* outS, float* outT, std::uint8_t* outW);
void AddSubU(std::uint32_t numUserChannels, float alpha,
             const std::uint8_t* filter, const float* aU,
             const std::uint8_t* aW, const float* bU,
             const std::uint8_t* bW, float* outU, std::uint8_t* outW);
void LerpU(int numUserChannels, float alpha, const std::uint8_t* filter,
           const float* aU, const std::uint8_t* aW, const float* bU,
           const std::uint8_t* bW, float* outU, std::uint8_t* outW);

void RetargetAdditiveRST(const idMD6SkelData* skeleton, float* outR,
                         float* outS, float* outT,
                         std::uint32_t numJoints);
void RetargetAdditiveU(const idMD6SkelData* skeleton, float* outU,
                       int numUserChannels);
void DecodeSkeletonFrame(const idMD6SkelData* skeleton, float* stackR,
                         float* stackS, float* stackT, float* stackU);
void TransformLocalJointToModelJoint(const std::int16_t* parentTable,
                                     int joint, const float* inR,
                                     const float* inS, const float* inT,
                                     float* outR, float* outS, float* outT);

int GetAnimMapIndex(const idMD6AnimData* animData,
                    idCrcHandle_t targetParentTableCRC);
int DecodeRLE(const std::uint8_t* rle, std::uint16_t sentinel,
              std::uint8_t* indices);
void DecodeAnimFrame(const idMD6SkelData* skeleton,
                     std::uint16_t numPaddedJoints,
                     const idMD6AnimData* animData,
                     const frameSetData_t* frameSetData, int frame,
                     std::uint32_t frameFraction, float* stackR,
                     float* stackS, float* stackT, float* stackU);
void DecodeAnimFrameU(const idMD6SkelData* skeleton,
                      const idMD6AnimData* animData,
                      const frameSetData_t* frameSetData,
                      const md6FrameInfo_t* frameInfo, float* stackU);
void DecodeUserChannelsForAnimFrame(const idMD6SkelData* skeleton,
                                    const idMD6AnimData* animData,
                                    float frame, float* outUserChannels);
void ApplyAnimMods(const idMD6SkelData* skeleton, const jointMod_t* animMods,
                   std::uint32_t numAnimMods, float* R, float* S, float* T,
                   float* U);
void CalculatePoseBounds(const idMD6SkelData* skeleton, __m128* finalPose,
                         idJointIndex_t boundsSkipJoint,
                         md6BlendOutputs_t* blendOutputs);
void TransformModelMatrices(const idMD6SkelData* skeleton,
                            const std::uint8_t* remap,
                            const float* inMatrices1,
                            const float* inMatrices2,
                            const float* invertedRefPoseQuat,
                            float* outMatrices, bool useDualQuat,
                            float* quatTemp);
void TransformLocalJointsToModelMatrices(
    const idMD6SkelData* skeleton, float* temp, std::uint32_t tempSize,
    const jointMod_t* mods, std::uint32_t numMods, std::uint32_t flags,
    const float* originOffset, bool addOffset, const float* inR,
    __m128i* inS, const float* inT, __m128* outMatrices, float* quatTemp,
    const float* refPose);
void DecodeBasePose(const idMD6SkelData* skeleton, float* temp,
                    std::uint32_t tempSize, const float* originOffset,
                    bool offsetAdd, __m128* outPose);
void DecodeSingleFrame(const idMD6SkelData* skeleton, char* temp,
                       std::uint32_t tempSize,
                       const idMD6AnimData* animData, int timeMSec,
                       int ticksPerSec, bool clearOriginTransform,
                       const float* originOffset, bool offsetAdd,
                       __m128* outPose, float* outUserChannels,
                       const jointMod_t* jointMods, int numJointMods);
void DecodeSingleFrameWithOriginDeltaScale(
    const idMD6SkelData* skeleton, char* temp, std::uint32_t tempSize,
    const idMD6AnimData* animData, int timeMSec, int ticksPerSec,
    bool clearOriginTransform, const float* originOffset, bool offsetAdd,
    __m128* outPose, float* outUserChannels,
    const float* originDeltaScale);
void ExecuteCommandsInternal(
    const idMD6SkelData* skeleton, char* temp, doubleStack_t* stack,
    const md6AnimCommand_t* cmds, std::uint32_t numCmds,
    bool clearOriginTransform, idJointIndex_t boundsSkipJoint,
    const float* originOffset, bool addOffset, const jointMod_t* jointMods,
    std::uint32_t numJointMods, const std::uint8_t* jointRemap,
    bool* generatedReferencePose, float* refPose,
    md6BlendOutputs_t* blendOutputs, float* userChannels,
    blendShapeEval_t* blendShapeEvals, std::uint32_t numBlendShapeEvals);
void ExecuteCommands(
    const idMD6SkelData* skeleton, char* temp, std::uint32_t tempSize,
    const md6AnimCommand_t* cmds, std::uint32_t numCmds,
    bool clearOriginTransform, idJointIndex_t boundsSkipJoint,
    bool useDualQuat, const float* originOffset, bool addOffset,
    const jointMod_t* jointMods, std::uint32_t numJointMods,
    const float* invertedBasePose, const float* invertedBasePoseQuat,
    const std::uint8_t* jointRemap, __m128* refPose, __m128* finalPose,
    float* renderPose, md6BlendOutputs_t* blendOutputs,
    float* userChannels, blendShapeEval_t* blendShapeEvals,
    std::uint32_t numBlendShapeEvals);
void ExecuteCommandsToRSTU(
    const idMD6SkelData* skeleton, char* temp, std::uint32_t tempSize,
    const md6AnimCommand_t* cmds, std::uint32_t numCmds,
    bool clearOriginTransform, idJointIndex_t boundsSkipJoint,
    const float* originOffset, bool addOffset, const jointMod_t* jointMods,
    std::uint32_t numJointMods, const std::uint8_t* jointRemap,
    __m128* finalPose, float* localR, float* localS, float* localT,
    md6BlendOutputs_t* blendOutputs, float* userChannels,
    blendShapeEval_t* blendShapeEvals, std::uint32_t numBlendShapeEvals);

} // namespace idMD6Blend

static_assert(sizeof(idMD6SkelData) == 0x40);
static_assert(offsetof(idMD6SkelData, parentTblCrc) == 0x06);
static_assert(offsetof(idMD6SkelData, jointWeightOffsets) == 0x14);
static_assert(sizeof(idMD6Blend::configInfo_t) == 0x10);
static_assert(sizeof(idMD6Blend::frameOffsets_t) == 0x600);
static_assert(sizeof(idMD6Blend::doubleStack_t) == 0x20);
static_assert(sizeof(idMD6Blend::blendShapeEval_t) == 0x210);
static_assert(sizeof(idMD6Blend::blendParms_t) == 0x80);
