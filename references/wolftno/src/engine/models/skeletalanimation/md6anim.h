#pragma once

#include "engine/animation/md6.h"
#include "engine/models/skeletalanimation/jointconversion.h"
#include "engine/models/skeletalanimation/md6parsetree.h"
#include "shared/idlib/containers/list.h"

#include <cstdint>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class idCVar;

struct idMD6AnimFileHeader {
  std::uint32_t timestamp{};
  std::uint32_t phaseTrackTimestamp{};
  std::uint32_t settingsTimestamp{};
  std::uint32_t skeletonTimestamp{};
  std::uint32_t blenddataMeshTimestamp{};
  std::uint32_t timeCode{};
  idBoundsShort translatedBounds{};
  idBoundsShort normalizedBounds{};
  std::uint16_t jointConversionChecksum{0xffffu};
  std::uint16_t pad{};
  idVec3 origWorldPos{};
  idAngles origWorldRot{};
  idVec3 origWorldScale{1.0f, 1.0f, 1.0f};
  std::uint16_t startFrameOffset{};
  std::uint16_t endFrameOffset{};
  std::uint32_t animDataSize{};

  void Swap();
};

using idMD6AnimLoadCallback = idMD6Anim* (*)(const char* filename,
                                              bool userChannelsOnly);
using idMD6AnimWaitCallback = void (*)(int milliseconds);
using idMD6AnimWarningCallback = void (*)(const char* message,
                                           const char* detail);
using idMD6AnimBoundsVerifier = bool (*)(const idMD6Anim* animation,
                                         const idDeclMD6* declaration,
                                         float epsilon,
                                         float jointRadius);
using idMD6AnimTextLoader = bool (*)(idMD6Anim* animation,
                                     const char* filename,
                                     idMD6Anim::animationSettings_t*
                                         settingsOverride);
using idMD6AnimParserLoader = bool (*)(
    idMD6Anim* animation, idParser* source,
    idMD6Anim::animationSettings_t* settingsOverride);
using idMD6AnimFacetrackLoader = bool (*)(
    idMD6Anim* animation, idMD6Anim::animationSettings_t& settings,
    const char* basePath, const idMD6Skel* skeleton,
    idList<idJointIndex_t, 5>& rotationMask,
    idList<idJointIndex_t, 5>& scaleMask,
    idList<idJointIndex_t, 5>& translationMask,
    idList<float, 5>& jointWeights);
using idMD6AnimRewriteCallback = bool (*)(const char* filename);
void SetMD6AnimLoadCallback(idMD6AnimLoadCallback callback);
void SetMD6AnimWaitCallback(idMD6AnimWaitCallback callback);
void SetMD6AnimWarningCallback(idMD6AnimWarningCallback callback);
void SetMD6AnimBoundsVerifier(idMD6AnimBoundsVerifier verifier);
void SetMD6AnimTextLoader(idMD6AnimTextLoader loader);
void SetMD6AnimParserLoader(idMD6AnimParserLoader loader);
void SetMD6AnimFacetrackLoader(idMD6AnimFacetrackLoader loader);
void SetMD6AnimRewriteCallback(idMD6AnimRewriteCallback callback);
void SetMD6AnimGameTimePerSecond(int ticksPerSecond);

std::uint32_t DecodeBitArray(const std::uint8_t* bits,
                             std::uint32_t numBits,
                             std::uint8_t* indices);

// The retail compressor implements these as functions on a method-local
// helper type.  Keeping them in a named namespace makes the recovered
// algorithms testable without changing their data contract.
namespace idMD6AnimCompression {
bool EqualR(const float* lhs, const float* rhs, float epsilon);
bool EqualS(const float* lhs, const float* rhs, float epsilon);
void CompressR(const float* input, std::int16_t* output);
float GetSetDistanceToRoot(std::int16_t jointIndex,
                           const std::int16_t* parentTable,
                           const float* baseTranslation,
                           idList<float, 5>& distances);
bool IsRWithinTolerance(const float* first, const float* second,
                        float endEffectorLength, float tolerance);
bool IsTWithinTolerance(const float* first, const float* second,
                        float tolerance,
                        float equalTolerance);
int GetDescendants(std::int16_t jointIndex,
                   const std::int16_t* parentTable, int numPaddedJoints,
                   idList<std::uint16_t, 5>& descendants);
std::int16_t GetSetFarthestEndEffector(
    std::int16_t jointIndex, const std::int16_t* parentTable,
    const idList<float, 5>& distanceToRoot,
    idList<std::int16_t, 5>& farthestEndEffectors);
int GenerateRLE(const std::uint8_t* bits, int numValues,
                const std::int16_t* targetMapping,
                std::uint8_t unmappedIndex,
                idList<std::uint8_t, 5>& rle);
int GenerateJointRLE(const idMD6Skel* sourceSkeleton,
                     const idMD6Skel* targetSkeleton,
                     const idList<std::uint8_t, 5>& bits,
                     idList<std::uint8_t, 5>& rle);
int GenerateUserChannelRLE(const idMD6Skel* sourceSkeleton,
                           const idMD6Skel* targetSkeleton,
                           int numPaddedUserChannels,
                           const idList<std::uint8_t, 5>& bits,
                           idList<std::uint8_t, 5>& rle);
void Reduce(const idList<float, 5>& keyFrames, int components,
            int numChannels,
            int leftFrame, int rightFrame, int channelIndex,
            float tolerance, bool quaternionChannel,
            idList<char, 5>& mask);
std::int64_t CalculateFrameSize(
    int frameStart, int frameRange, int numJoints,
    int numPaddedUserChannels, int numAnimR, int numAnimS, int numAnimT,
    int numAnimU, const idList<char, 5>& maskR,
    const idList<char, 5>& maskS, const idList<char, 5>& maskT,
    const idList<char, 5>& maskU, int& sizeR, int& sizeS, int& sizeT,
    int& sizeU);

struct idCompressionTolerances {
  idList<float, 5> rotation;
  idList<float, 5> scale;
  idList<float, 5> translation;
  idList<float, 5> userChannels;

  idCompressionTolerances(
      const idMD6Skel* skeleton,
      const idMD6Anim::idCompressionSettings& settings, int numFrames,
      float equalToleranceModifier, int numPaddedJoints,
      int numPaddedUserChannels, float jointTolerance,
      float userChannelTolerance);
};
} // namespace idMD6AnimCompression

class idMD6AnimResourceList {
public:
  ~idMD6AnimResourceList();
  idMD6Anim* Load(const char* name, bool makeDefault = true,
                  bool userChannelsOnly = false);
  idMD6Anim* FindExisting(const char* name) const;
  void FreeAll();
  int Num() const;

private:
  mutable std::mutex mutex;
  std::vector<std::unique_ptr<idMD6Anim>> resources;
};

using idMD6AnimModelLoader = idMD6Model* (*)(const char* modelName);
void SetMD6AnimModelLoader(idMD6AnimModelLoader loader);
namespace idMD6AnimRewriteHelpers {
idMD6Model* LoadModel(const char* meshPath, int numJoints,
                      int numPaddedUserChannels,
                      idCrcHandle_t parentTableCrc);
bool ValidateParenting(const char* filename, const idMD6Model* model,
                       int numJoints,
                       const idList<idStr, 5>& animationJointTable,
                       const idList<std::int16_t, 5>& animationParentTable,
                       idCrcHandle_t parentTableCrc);
} // namespace idMD6AnimRewriteHelpers

class idMD6AnimGenerator {
public:
  struct threadEntry_t {
    bool working = false;
    std::thread thread;
  };

  struct blendShapeExpressionEntry_t {
    std::string modelName;
    std::vector<std::string> expressions;
  };

  idMD6AnimGenerator();
  ~idMD6AnimGenerator();
  bool AddResource(idMD6Anim* animation);
  void Init(int numThreads = 1);
  void Finishup();
  blendShapeExpressionEntry_t* GetBlendShapeExpressionEntry(
      const char* modelName);
  static void WorkerThread(idMD6AnimGenerator* generator);

private:
  std::mutex mutex;
  std::condition_variable animAdded;
  std::condition_variable animReady;
  std::vector<idMD6Anim*> buffer;
  std::vector<threadEntry_t> threads;
  std::vector<blendShapeExpressionEntry_t> blendShapeExpressions;
  std::size_t head = 0;
  std::size_t tail = 0;
  std::size_t count = 0;
  bool valid = false;
  bool shuttingDown = false;
};

extern idMD6AnimGenerator animGenerator;
extern idCVar anim_allowDefaultedBinaryAnims;
extern idCVar anim_removeDefaultedAnimFiles;
extern idCVar anim_forceCompression;
extern idCVar anim_generatorNumThreads;

static_assert(sizeof(idMD6AnimFileHeader) == 96,
              "idMD6AnimFileHeader ABI drift");
