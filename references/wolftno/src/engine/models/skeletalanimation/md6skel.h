#pragma once

#include "engine/decls/decl.h"
#include "engine/models/skeletalanimation/jointconversion.h"
#include "engine/models/skeletalanimation/jobs/md6blend.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

class idLexer;
class idParser;
class idMD6Anim;
class idMD6Model;
class idDeclMD6;
struct idJointMat;

struct idJointQuat {
  void FromMat4(const idMat4& matrix);
  float jointQuat[8]{};
};

class idMD6Skel : public idResource {
public:
  idMD6Skel();
  ~idMD6Skel() noexcept(false) override;

  idResourceList* GetResourceList() const override;
  void LoadResource() override;
  bool ReloadIfStale() override;

  idUserChannelHandle_t GetUserChannelHandle(int userChannelIndex) const;
  idJointIndex_t GetJointIndex(idJointHandle_t handle) const;
  idJointIndex_t GetJointIndex(const char* jointName) const;
  idJointHandle_t GetJointHandle(int jointIndex) const;
  idUserChannelIndex GetUserChannelIndex(
      idUserChannelHandle_t handle) const;
  idUserChannelIndex GetUserChannelIndex(const char* channelName) const;
  bool IsJointChildOf(idJointIndex_t joint, idJointIndex_t parent) const;
  bool GetJointsForAnimFrame(idJointMat* joints, const idMD6Anim* anim,
                             int frame, const idVec3* offset,
                             bool removeOriginOffset) const;
  bool GetJointsForAnimTime(idJointMat* joints, const idMD6Anim* anim,
                            int timeMS, const idVec3* offset,
                            bool removeOriginOffset) const;
  bool GetUserChannelsForAnimFrame(float* userChannels,
                                   unsigned int userChannelsSize,
                                   const idMD6Anim* anim, float frame) const;
  int NumJoints() const;
  const char* GetJointName(int jointIndex) const;

  static void SwapData(idMD6SkelData* skeletonData);
  static void SwapHeader(idMD6SkelData* skeletonData);
  bool LoadBinary(const char* fileName, bool directBinary = false);
  bool WriteBinary(const char* fileName);
  bool MakeSkeletonData(unsigned int numJoints, float* basePose,
                        const float* secondaryBasePose,
                        idJointHandle_t* jointHandleTable,
                        std::int16_t* parentTable, int numUserChannels,
                        float* userChannels,
                        idUserChannelHandle_t* userChannelHandleTable,
                        std::uint8_t** jointWeights,
                        std::uint8_t** userWeights);
  void GetJointList(const idMD6Model* model, const char* jointNames,
                    idList<idJointIndex_t, 5>& jointList) const;
  bool Parse(idParser& parser);
  bool LoadText(const char* fileName);

  const idMD6Anim* defaultAnim{};
  idMD6SkelData* data{};
  idAtomicString userChannelFilename;
  std::uint32_t timestamp{0xffffffffu};
  std::uint32_t userChannelTimestamp{0xffffffffu};
  idJointConversionHandle_t jointConversion;
  idCrcHandle_t jointConversionChecksum;

  static idTypedResourceList<idMD6Skel> resourceList;
};

void SubChildren_r(const idMD6Skel* skeleton, idJointIndex_t parent,
                   idList<idJointIndex_t, 5>& jointList);
void AddChildren_r(const idMD6Skel* skeleton, idJointIndex_t parent,
                   idList<idJointIndex_t, 5>& jointList);

static_assert(sizeof(idJointQuat) == 32, "idJointQuat ABI drift");
static_assert(sizeof(idMD6Skel) == 88, "idMD6Skel ABI drift");
static_assert(offsetof(idMD6Skel, defaultAnim) == 48,
              "idMD6Skel default-animation offset drift");
static_assert(offsetof(idMD6Skel, data) == 56, "idMD6Skel data offset drift");
static_assert(offsetof(idMD6Skel, userChannelFilename) == 64,
              "idMD6Skel channel-file offset drift");
static_assert(offsetof(idMD6Skel, timestamp) == 72,
              "idMD6Skel timestamp offset drift");
static_assert(offsetof(idMD6Skel, jointConversion) == 80,
              "idMD6Skel conversion offset drift");

class idMD6SkeletonConfig : public idResource {
public:
  struct instance_t {
    const idMD6Skel* skeleton{};
    std::uint32_t timestamp{0xffffffffu};
    std::uint32_t recoveredPadding{};
  };

  idMD6SkeletonConfig();
  ~idMD6SkeletonConfig() noexcept(false) override;

  idResourceList* GetResourceList() const override;
  const char* GetResourceTypeName() const;
  void LoadResource() override;
  bool ReloadIfStale() override;
  bool WriteSkeletonConfig_Binary(const char* filename);
  bool ReadSkeletonConfig_Binary(const char* filename,
                                 bool directBinary = false);
  bool LoadSkeletonConfig(const char* basePath);

  idList<instance_t, 5> instances;
  const idDeclMD6* boundsDecl{};

  static idTypedResourceList<idMD6SkeletonConfig> resourceList;
};

using idMD6SkeletonConfigDeclLoader = const idDeclMD6* (*)(const char* name);
void SetMD6SkeletonConfigDeclLoader(
    idMD6SkeletonConfigDeclLoader loader);

static_assert(sizeof(idMD6SkeletonConfig::instance_t) == 16,
              "skeleton-config instance ABI drift");
static_assert(sizeof(idMD6SkeletonConfig) == 80,
              "skeleton-config ABI drift");
static_assert(offsetof(idMD6SkeletonConfig, instances) == 48,
              "skeleton-config instances offset drift");
static_assert(offsetof(idMD6SkeletonConfig, boundsDecl) == 72,
              "skeleton-config bounds offset drift");
