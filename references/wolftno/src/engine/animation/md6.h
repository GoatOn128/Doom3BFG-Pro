#pragma once

#include "engine/decls/decl.h"
#include "engine/models/skeletalanimation/md6morph.h"
#include "engine/models/sourcesurface.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/containers/array.h"
#include "shared/idlib/containers/index.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/bv/boundsshort.h"
#include "shared/idlib/text/atomicstring.h"
#include "shared/idlib/text/str.h"

#include <cstdint>
#include <memory>

template <typename First, typename Second> struct idPair {
  First first{};
  Second second{};
};
template <typename T, typename InvalidTag, int InvalidValue> class idHandle;
template <int MaxArgs> class idVarArgs;
class idAnimEvents;
class idDeclMD6;
class idParser;
class idMD6AnimResourceList;
struct idCachedJoint;
class idMD6AnimEvent;
#if defined(WOLFTNO_RETAIL_CHANNELWEIGHT_TICKS_ABI)
enum gameTimeUnique_t : int;
template <typename T, typename UniqueTag> class idTypesafeNumber;
#endif
class idMD6MorphSet;
class idMD6Skel;
class idRandom2;
class idDeclInfo;
namespace idMD6Blend { struct configInfo_t; }
struct idJointMat;

struct idDeclAnimWebTagGroup;
enum invalidUserChannelIndex_t { INVALID_USER_CHANNEL_INDEX };
enum invalidJointIndex_t { INVALID_JOINT_INDEX = -1 };
using idUserChannelIndex =
    idIndex<std::int16_t, invalidUserChannelIndex_t>;
using idJointIndex_t = idIndex<std::int16_t, invalidJointIndex_t>;
extern const idJointIndex_t NULL_JOINT_INDEX;

enum invalidAliasHandle_t : int;
template <>
class idHandle<unsigned short, invalidAliasHandle_t, 65535> {
public:
  unsigned short value = 0xffffu;
  idHandle() = default;
  explicit constexpr idHandle(unsigned short initial) : value(initial) {}
  bool operator==(const idHandle& other) const { return value == other.value; }
  bool operator!=(const idHandle& other) const { return value != other.value; }
  bool operator!() const { return value == 0xffffu; }
  bool IsValid() const { return value != 0xffffu; }
  void Release() { value = 0xffffu; }
  void Set(unsigned short next) { value = next; }
  unsigned short& Get() { return value; }
  const unsigned short& Get() const { return value; }
};
using idAliasHandle_t =
    idHandle<unsigned short, invalidAliasHandle_t, 65535>;
inline constexpr idAliasHandle_t NULL_ALIAS{};

class idAnimWebPath : public idStr {
public:
  static void SplitPath(const char* path, idStr& webName, idStr& subWebName,
                        idStr& stateName);
  void GetWebName(idStr& webName) const;
  void GetWebPathNames(idStr& webName, idStr& subWebName,
                       idStr& stateName) const;
  void SetPath(const char* webName, const char* subWebName,
               const char* stateName);
};

// NEEDS_WORK(animation): this is the recovered ABI boundary used by AnimWeb.
// The owning animation module will replace the resource loading implementation.
struct idMD6AnimData {
  std::uint32_t totalSize=0;
  std::uint16_t size=0;
  std::uint16_t flags=0;
  std::uint16_t numFrames=0;
  std::uint16_t frameRate=0;
  std::uint16_t numFrameSets=0;
  std::uint16_t frameSetTblOffset=0;
  std::uint16_t frameSetOffsetTblOffset=0;
  std::uint16_t numAnimMaps=0;
  std::uint16_t constROffset=0;
  std::uint16_t constSOffset=0;
  std::uint16_t constTOffset=0;
  std::uint16_t constUOffset=0;
  std::uint16_t nextSize=0;
  std::uint16_t jointWeightsOffset=0;
  float startDelta[12]{};
  float endDelta[12]{};
};
static_assert(sizeof(idMD6AnimData)==128);
class idMD6Anim {
public:
  idMD6Anim() = default;
  ~idMD6Anim() = default;
  struct compressionSetting_t {
    idAtomicString name;
    std::uint16_t flags = 0;
    bool enabled = true;
    std::uint8_t pad11 = 0;
    float tolerance = 1.0f;
    float equalTolerance = 0.0f;
    std::uint8_t recoveredRuleData[512]{};
    int weightGroup = 0;
    int compressionGroup = -1;
    bool userChannel = false;
    std::uint8_t pad541[3]{};
    void* userData = nullptr;
  };

  class idCompressionSettings {
  public:
    ~idCompressionSettings();
    compressionSetting_t* AllocSetting(const char* name);
    void SetupJointTolerance(const idMD6Skel* skeleton,
                             idList<float, 5>& tolerances,
                             int numPaddedJoints, int numFrames,
                             float baseTolerance) const;
    void SetupUserChannelTolerance(const idMD6Skel* skeleton,
                                   idList<float, 5>& tolerances,
                                   int numPaddedUserChannels, int numFrames,
                                   float baseTolerance) const;
    void Parse(idParser* source);

    idList<compressionSetting_t*, 30> compressionSettings;
  };

  struct animationSettings_t {
    animationSettings_t() = default;
    ~animationSettings_t();
    idCompressionSettings compressionSettings;
    float errorTolerance = 0.01f;
    float equalToleranceModifier = 1.0f;
    float minimumDistanceToEndEffector = 0.0f;
    std::uint16_t flags = 0;
    std::uint16_t frameRate = 30;
    int startFrame = 0;
    int endFrame = -1;
    bool useRotation = true;
    bool useScale = true;
    bool useTranslation = true;
    bool useUserChannels = true;
  };

  struct blendShapeChannel_t {
    idAtomicString name;
    idList<float, 5> samples;
  };

  struct sourceShapeExpr {
    idAtomicString source;
    idAtomicString expression;
  };

  bool WaitForValid() const;
  static idMD6AnimResourceList resourceList;
  idMD6AnimResourceList* GetResourceList() const;
  static idMD6Anim* LoadAnim_UserChannels(const char* filename);
  bool AnimIsOkForSkeleton(const class idMD6Skel* skeleton) const;
  bool AnimIsOkForModel(const class idMD6Model* model) const;
  void VerifyRLEData(
      const char* animationName,
      const idList<std::uint8_t, 5>& rotationRle,
      const idList<std::uint8_t, 5>& scaleRle,
      const idList<std::uint8_t, 5>& translationRle,
      const idList<std::uint8_t, 5>& userChannelRle,
      const idList<std::uint8_t, 5>& rotationBits,
      const idList<std::uint8_t, 5>& scaleBits,
      const idList<std::uint8_t, 5>& translationBits,
      const idList<std::uint8_t, 5>& userChannelBits) const;
  bool VerifyBoundingBoxes(const idDeclMD6* declaration, float epsilon,
                           float jointRadius) const;
  void GetAnimationDelta(idVec3* deltaTranslation,
                         idMat3* deltaAxis) const;
  int GetDurationInGameTime(int gameHz) const;
  int NumCompressedUserChannels() const;
  bool LoadBinary(const char* filename, bool allowDefaulted = false);
  bool WriteBinary(const char* filename) const;
  bool LoadText(char* filename,
                animationSettings_t* settingsOverride = nullptr);
  bool LoadTextFromParser(class idParser* source,
                          animationSettings_t* settingsOverride = nullptr);
  static bool LoadAnim_Settings(char* animationFilename,
                                idCompressionSettings& settings,
                                std::uint32_t* timestamp,
                                bool loadFromAnim,
                                bool updateNumFrames);
  static bool RewriteText(char* filename);
  void CompressAnim(
      const idMD6Skel* skeleton,
      const idList<const idMD6Skel*, 5>& mappedSkeletons,
      idList<float, 5>& rotation, idList<float, 5>& scale,
      idList<float, 5>& translation, idList<float, 5>& userChannels,
      const idList<idJointIndex_t, 5>& maskRJoints,
      const idList<idJointIndex_t, 5>& maskSJoints,
      const idList<idJointIndex_t, 5>& maskTJoints,
      const idList<idUserChannelIndex, 5>& maskUChannels,
      const idList<float, 5>& jointWeights, std::uint16_t flags,
      int numFrames, std::uint16_t frameRate,
      const animationSettings_t& settings);
  bool LoadFacetracksComposite(
      animationSettings_t& settings, char* basePath,
      const idMD6Skel* skeleton, idList<idJointIndex_t, 5>& maskRJoints,
      idList<idJointIndex_t, 5>& maskSJoints,
      idList<idJointIndex_t, 5>& maskTJoints,
      idList<float, 5>& jointWeights);
  void LoadResource();
  bool ReloadIfStale();
  void MakeDefault(const class idMD6Skel* skeleton);
  void FreeData();

  static void SwapFrameBits(std::uint8_t* bits, std::uint32_t numJoints,
                            std::uint32_t numFrames);
  static void SwapFrameSetFrameBits(const idMD6AnimData* data,
                                    struct frameSetData_t* frameSet);
  static void SwapAnimMap(idMD6AnimData* data);
  static void SwapHeader(idMD6AnimData* data);
  static void SwapFrameSetDataHeader(struct frameSetData_t* frameSet);
  static void SwapFrameSetData(const idMD6AnimData* data,
                               struct frameSetData_t* frameSet,
                               bool swapHeaderFirst);
  static void SwapAnimInfo(idMD6AnimData* data, bool swapHeaderFirst);

  void* vtable = nullptr;
  idAtomicString name;
  std::uint8_t resourceBase[32]{};
  idAtomicString skelName;
  idMD6AnimData* animData = nullptr;
  std::uint8_t phaseTrack[8]{};
  idBoundsShort translatedBounds{};
  idBoundsShort normalizedBounds{};
  std::uint16_t jointConversionHandle = 0xffffu;
  std::uint16_t jointConversionChecksum = 0xffffu;
  std::uint32_t timestamps[5]{0, 0xffffffffu, 0xffffffffu, 0xffffffffu,
                              0xffffffffu};
  std::uint32_t timeCode = 0;
  idVec3 origWorldPos{};
  idAngles origWorldRot{};
  idVec3 origWorldScale{1.0f, 1.0f, 1.0f};
  std::uint16_t startFrameOffset = 0;
  std::uint16_t endFrameOffset = 0;
  bool postedForGeneration = false;
  std::uint8_t tailPad[3]{};
};
static_assert(sizeof(idMD6Anim::compressionSetting_t) == 552);
class idMD6Model;
class idMD6Mesh;
class idJointGroupCollection;
class idPropsCollection;
class idDecl;
class idParser;
struct idMD6AliasEntry {
  idAtomicString animRef;
  const idMD6Anim* anim = nullptr;
};
using idMD6AliasMapping = idMD6AliasEntry;
class idMD6Alias {
public:
  idMD6Alias();
  bool IsInherited(const idMD6Alias* other) const;
  void LoadAnims(const class idDeclMD6* decl);
  void LoadAnimation(int index);
  void Parse(const idDecl* decl, idParser* source, int* loadErrors);

  idAtomicString name;
  std::uint16_t aliasHandle;
  std::uint16_t flags;
  std::uint32_t pad12;
  idList<idMD6AliasEntry, 19> mappings;
};
class idDeclMD6 {
public:
  struct aliasEntry_t {
    std::uint16_t handle = 0xffff;
    idMD6Alias alias;
  };

  idDeclMD6() = default;
  ~idDeclMD6() = default;

  const char* DefaultDefinition() const;
  idDeclInfo* GetDeclInfo() const;
  idAliasHandle_t StrongLoadAlias(const char* aliasName) const;
  bool GetJointsForAnimTime(idJointMat* joints, const idMD6Anim* anim,
                            int timeMS, const idVec3& offset,
                            bool removeOriginOffset) const;
  idAliasHandle_t FindAliasHandle(const char* aliasName) const;
  const char* FindAliasName(idAliasHandle_t aliasHandle) const;
#if defined(WOLFTNO_RETAIL_FACETRACKS_FIND_ALIAS_ABI) ||                       \
    defined(WOLFTNO_RETAIL_FIND_ALIAS_REFERENCE_ABI)
  const idMD6Alias* FindAlias(const idAliasHandle_t& aliasHandle,
#else
  const idMD6Alias* FindAlias(idAliasHandle_t aliasHandle,
#endif
                              bool includeInherited = true,
                              bool includeIncludes = true) const;
  const idMD6Alias* FindAlias(std::uint16_t handle) const {
    return FindAlias(idAliasHandle_t(handle));
  }
  const idMD6Alias* FindAlias(const char* aliasName,
                              bool includeInherited = true,
                              bool includeIncludes = true) const;
#if defined(WOLFTNO_RETAIL_CHANNELWEIGHT_ANIMFORALIAS_ABI)
  const idMD6Anim* AnimForAlias(const idAliasHandle_t& aliasHandle,
#else
  const idMD6Anim* AnimForAlias(idAliasHandle_t aliasHandle,
#endif
                                bool load) const;
  idAliasHandle_t AddAlias(const idMD6Alias& alias, bool overload);
  void GetAliases(idList<const idMD6Alias*, 5>& list,
                  bool includeParent = true,
                  bool includeIncludes = true) const;
  void IncludeAnimDataFrom(const idDeclMD6* includeDecl);
  bool IsChildOf(const idDeclMD6* candidate) const;

  int NumJoints() const;
  const char* GetJointName(idJointIndex_t index) const;
  void GetJointList(const char* jointNames,
                    idList<idJointIndex_t, 5>& jointList) const;
  int GetUserChannelWeightGroup() const;

  const idVarArgs<6>* FindUserProp(const char* propName) const;
  void ReadUserProp(idParser& source, idVarArgs<6>& prop);
  int GetHeadTrackGroupIndex(const char* groupName) const;
  bool GetRandomValidMeshKits(idAtomicString (&meshKits)[3],
                              idRandom2& random) const;
  int NumMeshKits(int component) const;
  const char* GetMeshKitName(int component, int kitIndex) const;
  const idList<int, 19>* GetMeshKitMeshes(int component,
                                          int kitIndex) const;
  void AddMeshKit(int component, const char* kitName,
                  const idList<int, 19>& meshIndices);
  const char* GetMeshKitDefaultOverride(int component) const;
  void SetMeshKitDefaultOverride(int component, const char* kitName);
  const char* GetDefaultSkinOverride() const;
  void SetDefaultSkinOverride(const char* skinName);
  int NumWrinkleMapRegionExpressions() const;
  const char* GetWrinkleMapRegionExpression(int index) const;
  void AddWrinkleMapRegionExpression(const char* expression);
  int NumSkipUseHintMeshes() const;
  int GetSkipUseHintMesh(int index) const;
  void AddSkipUseHintMesh(int meshIndex);
  idJointGroupCollection* GetJointGroupCollection() const;
  idPropsCollection* GetPropsCollection() const;

  void PinStreamedSound(int eventNum, int maxFrame) const;
  bool FindAnimEvents(const idMD6Anim* anim, int startFrame, int endFrame,
                      idList<const idMD6AnimEvent*, 5>& events) const;
  bool FindAnimEvents(const idMD6Anim* anim, int startFrame, int endFrame,
                      int eventNum,
                      idList<const idMD6AnimEvent*, 5>& events) const;
  bool GetCachedJoints(int eventNum, const idMD6Anim* anim,
                       idList<const idCachedJoint*, 5>& joints) const;

  int NumMorphSets() const;
  const idMD6MorphSet* GetMorphSet(int index) const;
  bool HasMorphSets() const;

  idAnimEvents& AnimEvents();
  const idAnimEvents& AnimEvents() const;
  void AddUserProp(const idVarArgs<6>& prop);
  void AddHeadTrackGroupName(const char* groupName);
  void AddValidMeshKitSet(const idList<idAtomicString, 5> (&kits)[3]);
  void SetMorphSets(const idList<idMD6MorphSet, 5>& sets);
  void SetUserChannelAliasOverride(idUserChannelIndex channel,
                                   idAliasHandle_t alias);
  idAliasHandle_t GetUserChannelAlias(idUserChannelIndex channel) const;
  void InitUserChannelToAnimationAliasMap();
  void InitHeadTrackGroups();
  void Parse(idParser& source);
  void FreeData();

#if defined(WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI)
  std::uint8_t recoveredVptr[8];
  idAtomicString name;
  std::uint8_t recovered10[0x50];
  idMD6Model* model;
  std::uint8_t recovered68[0x4E0];
#else
  idAtomicString name;
  idList<aliasEntry_t> aliases;
  idMD6Model* model = nullptr;
  const idDeclMD6* parentDecl = nullptr;
  idMD6Blend::configInfo_t* config = nullptr;
  bool calcRefBoundsFromJoints = false;
  std::uint8_t recoveredDeclPadding[3]{};
  idBounds referenceBounds{};
  int userChannelWeightGroupOverride = 8;
  std::shared_ptr<void> recoveredState;
#endif
};

#if defined(WOLFTNO_RETAIL_ANIMWEB_DECLMD6_LAYOUT_ABI)
static_assert(sizeof(idDeclMD6) == 0x548, "AnimWeb MD6 declaration ABI drift");
static_assert(offsetof(idDeclMD6, name) == 0x08,
              "AnimWeb MD6 declaration name offset drift");
static_assert(offsetof(idDeclMD6, model) == 0x60,
              "AnimWeb MD6 declaration model offset drift");
#endif

// NEEDS_WORK(animation): declarations below are the MD6 tree ABI consumed by
// AnimWeb. Their runtime algorithms belong to the animation folder.
namespace idMD6Blend {
enum blendOp_t {
  BOP_NO = 0,
  BOP_LERP = 1,
  BOP_REF_LERP = 2,
  BOP_ADD_LEFT = 3,
  BOP_ADD_RIGHT = 4,
  BOP_SUB_LEFT = 5,
  BOP_SUB_RIGHT = 6,
  BOP_BLEND = 7,
  BOP_BLENDA = 8,
  BOP_FUSION = 9,
  BOP_CLEARADDU = 10,
  BOP_MAX = 11,
};

enum originBlend_t {
  ORIGINBLEND_BRANCH = 0,
  ORIGINBLEND_LEFT = 1,
  ORIGINBLEND_RIGHT = 2,
  ORIGINBLEND_DEFAULT = ORIGINBLEND_BRANCH,
  ORIGINBLEND_SOURCE = ORIGINBLEND_LEFT,
  ORIGINBLEND_DEST = ORIGINBLEND_RIGHT,
  ORIGINBLEND_MAX = 3,
};

struct jointMod_t {
  float mat[12]{};
  float s[3]{};
  idJointIndex_t joint;
  std::uint16_t flags = 0;
};
}
enum md6WeightGroup_t {
  MD6_WEIGHTGROUP_ALL=0,
  MD6_WEIGHTGROUP_LEGS=1,
  MD6_WEIGHTGROUP_TORSO=2,
  MD6_WEIGHTGROUP_HEAD=3,
  MD6_WEIGHTGROUP_FACE=4,
  MD6_WEIGHTGROUP_MOUTH=5,
  MD6_WEIGHTGROUP_EYELIDS=6,
  MD6_WEIGHTGROUP_USER0=7,
  MD6_WEIGHTGROUP_MAX=8
};
class idMD6Skel;
class idMD6Model : public idResource {
public:
  static idTypedResourceList<idMD6Model> resourceList;

  idMD6Model();
  ~idMD6Model() noexcept(false) override;
  idResourceList* GetResourceList() const override;
  void LoadResource() override;
  bool ReloadIfStale() override;
  void Print() override;
  void List() const override;
  void Free();
  bool LoadBinary(const char* binaryFileName, bool explicitFile);
  bool LoadFileNoRemap(const char* modelFileName);
  void RemapSourceSurfaces();
  static bool RewriteText(char* fileName);
  bool GetJointsForAnimFrame(idJointMat* joints, const idMD6Anim* anim,
                             int frame, const idVec3* offset,
                             bool removeOriginOffset) const;
  bool GetJointsForAnimTime(idJointMat* joints, const idMD6Anim* anim,
                            int timeMS, const idVec3* offset,
                            bool removeOriginOffset) const;
  bool GetUserChannelsForAnimFrame(
#if defined(WOLFTNO_RETAIL_CHANNELWEIGHT_USERCHANNEL_RESTRICT_ABI)
                                   float* __restrict userChannels,
#else
                                   float* userChannels,
#endif
                                   unsigned int userChannelsSize,
                                   const idMD6Anim* anim, float frame) const;

  idStr filename;
  std::uint32_t timestamp = 0;
  std::uint32_t skelTimestamp = 0;
  idBounds defaultBounds{};
  bool remapForSkinning = false;
  std::uint8_t pad121[3]{};
  std::uint32_t skinChecksum = 0;
  idStr morphSkinName;
  std::uint32_t morphSkin = 0;
  std::int32_t numMeshesBeforeSplitting = 0;
  idList<idMD6Mesh*, 19> meshes;
  idList<std::uint8_t, 19> jointRemap;
  idMD6Morph morphSets;
  idList<sourceSurface_t, 5> sourceSurfaces;
  idList<const idMaterial*, 5> materials;
  idMD6Skel* skeleton = nullptr;
  idVec3 minBoundsExpansion{};
  idVec3 maxBoundsExpansion{};
};
class idMD6Node {
public:
  enum nodeType_t : std::uint8_t { NODE_BRANCH=0,NODE_LEAF_PAUSE=1,NODE_LEAF_PLAY=2,NODE_BLEND_BRANCH=3,NODE_BLENDA_BRANCH=4,NODE_FUSION_BRANCH=5,NODE_BEST_LEAF=6,NODE_TAG_FILTER=7,NODE_NONE=15 };
  bool IsValid() const;
  std::uint8_t type=NODE_NONE;
};

template <>
idMD6Model* idTypedResourceList<idMD6Model>::Alloc(
    const char* resourceName) const;
template <>
int idTypedResourceList<idMD6Model>::AllocSize() const;
class idMD6Leaf : public idMD6Node {
public:
  enum wrapMode_t : int {
    WRAP_CLAMP = 0,
    WRAP_LOOP = 1,
    WRAP_REPEAT = WRAP_LOOP,
    WRAP_INVALID = 2
  };

  explicit idMD6Leaf(idMD6Node::nodeType_t nodeType = NODE_NONE);
  int GetFrameRate() const;
  int GetLength(int ticksPerSec) const;
  const idMD6Anim*anim=nullptr;std::uint8_t weightGroup=0;std::uint8_t wrapMode=0;std::uint8_t initCounter=0;std::uint8_t pad=0;
};
class idMD6LeafPlay : public idMD6Leaf {
public:
#if defined(WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI)
  idMD6LeafPlay()
      : idMD6Leaf(NODE_LEAF_PLAY), startTime(0), rateScale(1.0f),
        syncGroup(0), syncEnabled(0) {}
#else
  idMD6LeafPlay() : idMD6Leaf(NODE_LEAF_PLAY) { Clear(); }
#endif
  void Clear();
#if defined(WOLFTNO_RETAIL_SYNCED_LEAF_INIT_ABI)
  void Init(const idMD6Anim* animation, int gameTime, int ticksPerSec,
            int frame, float animationRateScale,
            wrapMode_t animationWrapMode,
            md6WeightGroup_t animationWeightGroup);
#else
  void Init(const idMD6Anim* animation, int gameTime, int ticksPerSec,
            std::uint16_t frame, float animationRateScale,
            wrapMode_t animationWrapMode,
            md6WeightGroup_t animationWeightGroup);
#endif
  void Restart(int gameTime, int ticksPerSec,
#if defined(WOLFTNO_RETAIL_ANIMWEB_RESTART_FRAME_INT_ABI)
               int frame,
#else
               std::uint16_t frame,
#endif
               wrapMode_t animationWrapMode);
  void SetFrameTime(int gameTime, int targetTime);
  void SetFloatFrame(int gameTime, int ticksPerSec, float targetFrame);
  void SetFrame(int gameTime, int ticksPerSec, std::uint16_t targetFrame);
  int GetFrameTime(int gameTime) const;
#if defined(WOLFTNO_RETAIL_CHANNELWEIGHT_TICKS_ABI)
  idTypesafeNumber<int, gameTimeUnique_t> GetFrameTicks(int gameTime) const;
#else
  int GetFrameTicks(int gameTime) const;
#endif
  float GetNormalizedFrameTime(int gameTime, int ticksPerSec) const;
  float GetFloatFrame(int gameTime, int ticksPerSec) const;
  std::uint16_t GetFrame(int gameTime, int ticksPerSec) const;
  std::uint32_t GetLoopCount(int gameTime, int ticksPerSec) const;
  int GetAnimLength(int ticksPerSec, bool applyRateScale) const;
#if defined(WOLFTNO_RETAIL_CHANNELWEIGHT_TICKS_ABI)
  idTypesafeNumber<int, gameTimeUnique_t> GetAnimTicks(int ticksPerSec) const;
#else
  int GetAnimTicks(int ticksPerSec) const;
#endif
  int GetClampTime(int gameTime, int ticksPerSec) const;
  float GetRateScale() const { return rateScale; }
  void SetRateScale(int gameTime, float newRateScale);
  bool IsPlaying(int gameTime, int ticksPerSec) const;
  bool IsPlaying(int gameTime, int ticksPerSec,
                 bool clampIgnoreExtraFrame) const;
  bool IsPlaying(int gameTime, int ticksPerSec,
                 wrapMode_t animationWrapMode) const;
  int startTime=0;float rateScale=1.0f;std::uint8_t syncGroup=0;std::uint8_t syncEnabled=0;
};
class idMD6LeafPause : public idMD6Leaf {
public:
  idMD6LeafPause();
  ~idMD6LeafPause()
#if defined(WOLFTNO_RETAIL_TRACKER_LEAF_DTOR_UNWIND_ABI)
      noexcept(false)
#endif
      ;
  void Init(const idMD6Anim* animation, float animationFrame,
            idMD6Leaf::wrapMode_t animationWrapMode,
            md6WeightGroup_t animationWeightGroup);
  void Clear();
  void SetUseStackTop(bool set);
  void SetClearNonModWeights(bool set);
  void SetOverrideFrameBounds(bool set);
  void SetNumAnimMods(int count);

  int currentDeferred=0;
  std::uint8_t deferredPadding[4]{};
  idList<idMD6Blend::jointMod_t,30> animMods[2];
  std::int16_t flags=0;
  std::uint8_t flagsPadding[2]{};
  float frame=0.0f;
  idBounds bounds{};
};
class idMD6Branch : public idMD6Node {
public:
  enum blendType_t : int { BLEND_LINEAR=0,BLEND_EASEIN=1,BLEND_EASEOUT=2,BLEND_EASEIN_EASEOUT=3,BLEND_TYPE_COUNT=4 };
  idMD6Branch() : idMD6Node{NODE_BRANCH} {}
  void Clear();
  void Init(idMD6Node* leftNode, idMD6Node* rightNode,
            idMD6Blend::blendOp_t blendOp,
            idMD6Blend::originBlend_t animationOriginBlend,
            float startAlpha, float endAlpha, float blendRate,
            md6WeightGroup_t animationFilterGroup,
            blendType_t animationBlendType);
  bool IsBlendFinished() const;
  bool IsChildBlendedOut(const idMD6Node* child) const;
  bool IsBlendingOut() const;
  bool IsBlendingIn() const;
  void UpdateCurrentAlpha(int timeMSec,int lastTimeMSec,int ticksPerSec);
  idMD6Node*left=nullptr;idMD6Node*right=nullptr;int leftTimeOverride=-1;int rightTimeOverride=-1;std::uint8_t filterGroup=MD6_WEIGHTGROUP_MAX;std::uint8_t op=idMD6Blend::BOP_MAX;std::uint8_t originBlend=idMD6Blend::ORIGINBLEND_BRANCH;float currentAlpha=0.0f;float targetAlpha=0.0f;float alphaRate=0.0f;int blendType=BLEND_LINEAR;
};
class idMD6BlendBranch_Base : public idMD6Branch {
public:
  struct Coordinate_t {
    int size = 0;
    int padding = 0;
    const float* data = nullptr;
  };

  ~idMD6BlendBranch_Base();
  int GetNumAnimations() const { return animationList.num; }
  idMD6Node* GetAnimation(int index) const {
    return index >= 0 && index < animationList.num ? animationList.list[index]
                                                   : nullptr;
  }

  idList<float,30> coordinateList;
  idList<idMD6Node*,30> animationList;

  void InitCoordinateNodePairs(int numDimensions,
                               const idList<float, 5>& coordinates,
                               const idList<idMD6Node*, 5>& animations,
                               std::uint8_t flags);

protected:
  explicit idMD6BlendBranch_Base(idMD6Node::nodeType_t nodeType);
};
class idMD6BlendBranch : public idMD6BlendBranch_Base {
public:
  idMD6BlendBranch();
  ~idMD6BlendBranch();
  void Init(std::uint8_t dimensions,
            const idList<idUserChannelIndex, 30>& channels,
            const idList<float, 5>& coordinates,
            const idList<idMD6Node*, 5>& animations,
            const idList<float, 5>& coordinate,
            md6WeightGroup_t weightGroup, std::uint8_t flags);
  void Update(bool forceUpdate,bool isSorted);
  void FindAnimation(bool isSorted, idMD6Node** leftAnimation,
                     idMD6Node** rightAnimation, float* alpha);
  bool BuildPointList(idList<int, 5>& indices,
                      idArray<float, 6>& normal,
                      bool isFillerPoint) const;
  void FindBarycentric(idList<idPair<float, idMD6Node*>, 5>& leaves);
  void Solve_Gaussian(idArray<float, 6>& values,
                      idArray<float, 36>& matrix,
                      idArray<float, 6>& solution) const;
  void UpdateNormal(const idArray<float, 6>& point0,
                    const idArray<float, 6>& point1,
                    const idArray<float, 6>& inputNormal,
                    idArray<float, 6>& outputNormal) const;
  idList<float,30> currentCoordinate;
  idList<idUserChannelIndex,30> userChannelIndices;
  idList<float,30> activeCoordinate;
  idList<int,30> activeIndices;
  idList<idMD6Branch,30> branchList;
  std::uint8_t numDimensions=0;
  bool isAngleWrapped=false;

protected:
  explicit idMD6BlendBranch(idMD6Node::nodeType_t nodeType);
};
class idMD6BlendAdditiveBranch : public idMD6BlendBranch_Base {
public:
  idMD6BlendAdditiveBranch()
      : idMD6BlendBranch_Base(idMD6Node::NODE_BLENDA_BRANCH) {}
  void Init(idMD6Node* base, const idList<float, 5>& coordinates,
            const idList<idMD6Node*, 5>& animations, float coordinate,
            md6WeightGroup_t weightGroup);
  void Update(bool forceUpdate);
  void FindAnimation(idMD6Node** rightAnimation, float* alpha) const;
  float currentCoordinate=0.0f;
  float activeCoordinate=0.0f;
  idMD6Node*baseAnimation=nullptr;
};
class idMD6FusionBranch : public idMD6BlendBranch {
public:
  idMD6FusionBranch();
  ~idMD6FusionBranch();
  void Init(std::uint8_t dimensions,
            const idList<idUserChannelIndex, 30>& channels,
            const idList<float, 5>& coordinates,
            const idList<idMD6Node*, 5>& animations,
            const idList<float, 5>& coordinate,
            md6WeightGroup_t weightGroup, std::uint8_t flags);
  void Update(bool forceUpdate,int gameTime,int ticksPerSec,const idMD6Skel* skeleton);
  idList<std::uint16_t,30> phaseToFrameLookupData;
};
class idMD6Filter : public idMD6Node {};
class idMD6TagFilter : public idMD6Filter {
public:
  idMD6TagFilter();
  void Init(std::uint8_t inputTagBias, unsigned inputTagMask,
            std::uint8_t inputTagGroupIndex,
            const idDeclAnimWebTagGroup* inputTagGroup);
  const idDeclAnimWebTagGroup*tagGroup=nullptr;unsigned tagMask=0;std::uint8_t tagGroupIndex=0;std::uint8_t tagBias=0;
};
class idMD6BestLeaf : public idMD6Node {
public:
  idMD6BestLeaf();
  ~idMD6BestLeaf();
  void Init(std::uint8_t inputTagBias, unsigned inputTagMask,
            std::uint8_t inputTagGroupIndex,
            const idList<idMD6Node*, 30>& leaves,
            const idList<unsigned, 30>& tags,
            const idList<idMD6Filter*, 30>& filters,
            unsigned inputDefaultTag,
            const idDeclAnimWebTagGroup* inputTagGroup);
  void Update();
  idList<idMD6Node*,30> leafList;idList<unsigned,30>tagList;idList<idMD6Filter*,30>filterList;idMD6TagFilter tagGroupFilter;std::uint8_t debugFilterLogicHelper=0;std::uint8_t debugPadding[7]{};const idDeclAnimWebTagGroup*tagGroup=nullptr;unsigned desiredTag=0;unsigned activeTag=0;unsigned defaultTag=0;std::uint16_t bestLeafIndex=0;std::uint8_t tagBias=0;std::uint8_t tagGroupIndex=0;
};
static_assert(sizeof(idMD6Node)==1);
static_assert(sizeof(idMD6Blend::jointMod_t)==64);
static_assert(sizeof(idMD6Leaf)==24);
static_assert(sizeof(idMD6LeafPlay)==40);
static_assert(sizeof(idMD6LeafPause)==112);
static_assert(sizeof(idMD6Branch)==56);
static_assert(sizeof(idMD6BlendBranch_Base)==104);
static_assert(sizeof(idMD6BlendBranch)==232);
static_assert(sizeof(idMD6BlendAdditiveBranch)==120);
static_assert(sizeof(idMD6FusionBranch)==256);
static_assert(sizeof(idMD6TagFilter)==24);
static_assert(sizeof(idMD6BestLeaf)==136);
static_assert(sizeof(idMD6AliasEntry)==16);
static_assert(sizeof(idMD6Alias)==40);
static_assert(offsetof(idMD6Alias, mappings)==16);
static_assert(sizeof(idMD6Anim)==168);
static_assert(offsetof(idMD6Anim, name)==8);
static_assert(offsetof(idMD6Anim, animData)==56);
static_assert(offsetof(idMD6Anim, translatedBounds)==72);
static_assert(sizeof(idMD6Model)==328);
static_assert(offsetof(idMD6Model, name)==8);
static_assert(offsetof(idMD6Model, skeleton)==296);
static_assert(offsetof(idMD6Model, meshes)==176);
static_assert(offsetof(idMD6Model, jointRemap)==200);
class idMD6Allocator {
public:
  virtual ~idMD6Allocator()
#if defined(WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI)
      noexcept(false)
#endif
      = default;
  virtual idMD6Node* Alloc(idMD6Node::nodeType_t type)=0;
  virtual void FreeMem(idMD6Node*node)=0;
  virtual void Condense()=0;
  virtual std::size_t Size()const=0;
  virtual int NumNodes()const=0;
  virtual idMD6Node* NodeForIndex(int index)=0;
  virtual const idMD6Node* NodeForIndex(int index)const=0;

  template <typename NodeType> void Free(NodeType* node) {
    node->~NodeType();
    FreeMem(node);
  }
  static void FreeTyped(idMD6Allocator* allocator, const idMD6Node* node);
};
