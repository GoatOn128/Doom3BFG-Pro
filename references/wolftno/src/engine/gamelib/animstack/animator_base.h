#pragma once

#include "engine/animation/md6.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

class idAnimStack;
class idAnimatorParms_Base;
class idClip;
class idGameTimeManager;
class idSerializer;
class idTreeAnimator;
class AnimatorPainValidationAccess;
class AnimatorChannelValidationAccess;

#ifndef ID_GAME_TIME_UNIQUE_T_DEFINED
#define ID_GAME_TIME_UNIQUE_T_DEFINED
enum gameTimeUnique_t : int;
#endif
enum serializeType_t {
  STYPE_GENERIC = 0,
  STYPE_AF = 1,
  STYPE_ANIMWEB_HANDS = 2,
  STYPE_WALK_IK = 3,
  STYPE_TORSO_TRACKER = 4,
  STYPE_PAIN = 5,
  STYPE_SHOOT = STYPE_PAIN,
  STYPE_JOINTMOD = 6,
  STYPE_REACH_IK = 7,
  STYPE_HAND_IK = STYPE_REACH_IK,
  STYPE_VEHICLECAR = STYPE_REACH_IK,
  STYPE_MAX = 8,
  SERIALIZE_TYPE_NONE = STYPE_GENERIC,
};

class idAnimatorParms_Base {
public:
  idAnimatorParms_Base(idAnimStack* stack, const char* animatorName,
                       idMD6Blend::blendOp_t operation,
                       idMD6Blend::originBlend_t originOperation,
                       md6WeightGroup_t animatorWeightGroup,
                       md6WeightGroup_t animatorFilterGroup, float animatorAlpha)
      : animStack(stack), name(animatorName), blendOp(operation),
        originBlend(originOperation), weightGroup(animatorWeightGroup),
        filterGroup(animatorFilterGroup), alpha(animatorAlpha) {}
  ~idAnimatorParms_Base() = default;

  idAnimStack* animStack;
  idStr name;
  idMD6Blend::blendOp_t blendOp;
  idMD6Blend::originBlend_t originBlend;
  md6WeightGroup_t weightGroup;
  md6WeightGroup_t filterGroup;
  float alpha;
  std::uint8_t padding[4];
};

class idGameTimeManagerPtr {
public:
  idGameTimeManager* gameTimeManager;
};

#ifndef ID_TYPESAFE_NUMBER_DEFINED
#define ID_TYPESAFE_NUMBER_DEFINED
template <typename T, typename UniqueTag> class idTypesafeNumber {
public:
  constexpr idTypesafeNumber() : value{} {}
  constexpr idTypesafeNumber(T input) : value(input) {}
  constexpr idTypesafeNumber(const idTypesafeNumber& other)
      : value(other.value) {}
  constexpr operator T() const { return value; }
  T value;
};
#endif

class idAnimator_Base {
public:
  enum priority_t {
    PRIORITY_WEB = 0,
    PRIORITY_AFTER_WEB = 1,
    PRIORITY_IK = 99,
    PRIORITY_AF = 100,
    PRIORITY_FACETRACKS = 101,
    PRIORITY_DEFAULT = PRIORITY_WEB,
  };

  struct serializeProps_t {
    idMD6Branch* serializedTrees[2];
    idTreeAnimator* treeAnimator;
    idAnimStack* animStack;
    idClip* clip;
    bool createdThroughSerialization;
    std::uint8_t padding[7];
  };

  idAnimator_Base();
  virtual ~idAnimator_Base()
#if defined(WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI)
      noexcept(false)
#endif
      ;

  bool Init(idGameTimeManager* gameTimeManager,
            const idAnimatorParms_Base& parms);
  void Shutdown(idAnimStack* stack);
  void Blend(const idAnimStack* stack, int curTime, float targetAlpha,
             int blendDurationMS, bool reset);
  void Start(const idAnimStack* stack, int curTime, int blendDurationMS,
             bool reset);
  void End(const idAnimStack* stack, int curTime, int blendDurationMS,
           bool reset);
  void Pause(const idAnimStack* stack,
             idTypesafeNumber<int, gameTimeUnique_t> curTime);
  void Unpause(const idAnimStack* stack,
               idTypesafeNumber<int, gameTimeUnique_t> curTime);
  void PreBlendTree(const idAnimStack* stack, int curTime,
                    int ticksPerSec) {
    idMD6Branch* const mergeBranch = InternalGetMergeBranch();
    if (mergeBranch != nullptr) {
      mergeBranch->left = nullptr;
    }
    InternalPreBlendTree(stack, curTime, ticksPerSec);
  }
  bool IsContributing() const;
  bool IsInitialized(idAnimStack* animStack) const;
  bool IsEnabled(idAnimStack* animStack) const;
  void SetEnabled(idAnimStack* animStack, bool enabled);
  md6WeightGroup_t GetFilterGroup() const;
  float GetAlpha() const;
  void SetAlpha(float alpha);
  void SetTargetAlpha(float alpha, float alphaRate);

  virtual priority_t GetStackPriority() const { return PRIORITY_WEB; }
  virtual serializeType_t GetSerializeType() { return STYPE_GENERIC; }
  virtual void SerializeSnapshot(idSerializer&) {}
  virtual void PreBlendSnapshot(idAnimStack*, int, int, float) {}
  virtual void PreSerializeInit(idGameTimeManager* gameTimeManager) {
    gametimeManager.gameTimeManager = gameTimeManager;
    serializeProps.createdThroughSerialization = true;
  }

  __forceinline static void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }

protected:
  virtual bool InternalInit(const idAnimatorParms_Base& parms) = 0;
  virtual bool InternalPostInit(const idAnimatorParms_Base& parms);
  virtual void InternalShutdown(idAnimStack* stack) = 0;
  virtual void InternalPreBlendTree(const idAnimStack* stack, int curTime,
                                    int ticksPerSec) = 0;
  virtual void InternalPostBlendTree(const idAnimStack* stack, int curTime) = 0;
  virtual void InternalStart(
      const idAnimStack* stack, int curTime,
      idTypesafeNumber<int, gameTimeUnique_t> startTime) = 0;
  virtual void InternalEnd(
      const idAnimStack* stack, int curTime,
      idTypesafeNumber<int, gameTimeUnique_t> endTime) = 0;
  virtual void InternalBlend(
      const idAnimStack* stack, int curTime, float blendWeight,
      idTypesafeNumber<int, gameTimeUnique_t> blendTime) = 0;
  virtual bool InternalIsContributing() const = 0;
  virtual const idMD6Branch* InternalGetMergeBranch() const = 0;
  virtual idMD6Branch* InternalGetMergeBranch() = 0;
  virtual void InternalPause(
      const idAnimStack*, idTypesafeNumber<int, gameTimeUnique_t>) {}
  virtual void InternalUnpause(
      const idAnimStack*, idTypesafeNumber<int, gameTimeUnique_t>) {}
  virtual const idMD6Branch* InternalGetEndBranch() const {
    return nullptr;
  }
  virtual idMD6Branch* InternalGetEndBranch() { return nullptr; }

protected:
  idGameTimeManagerPtr gametimeManager;
  md6WeightGroup_t filterGroup;
  md6WeightGroup_t weightGroup;
  serializeProps_t serializeProps;
  std::uint8_t initialized : 1;
  std::uint8_t shutdownCalled : 1;
  std::uint8_t flagsPadding : 6;
  std::uint8_t tailPadding[7];

  friend class idAnimStack;
  friend class AnimatorPainValidationAccess;
  friend class AnimatorChannelValidationAccess;
};

static_assert(sizeof(idTypesafeNumber<int, gameTimeUnique_t>) == 0x04,
              "idTypesafeNumber ABI drift");
static_assert(sizeof(idAnimatorParms_Base) == 0x48,
              "idAnimatorParms_Base ABI drift");
static_assert(offsetof(idAnimatorParms_Base, name) == 0x08,
              "idAnimatorParms_Base::name ABI drift");
static_assert(offsetof(idAnimatorParms_Base, blendOp) == 0x30,
              "idAnimatorParms_Base::blendOp ABI drift");
static_assert(offsetof(idAnimatorParms_Base, alpha) == 0x40,
              "idAnimatorParms_Base::alpha ABI drift");
static_assert(sizeof(idGameTimeManagerPtr) == 0x08,
              "idGameTimeManagerPtr ABI drift");
static_assert(sizeof(idAnimator_Base::serializeProps_t) == 0x30,
              "animator serialize properties ABI drift");
static_assert(offsetof(idAnimator_Base::serializeProps_t, animStack) == 0x18,
              "serialize anim-stack ABI drift");
static_assert(offsetof(idAnimator_Base::serializeProps_t,
                       createdThroughSerialization) == 0x28,
              "serialize creation flag ABI drift");
static_assert(sizeof(idAnimator_Base) == 0x50,
              "idAnimator_Base ABI drift");
