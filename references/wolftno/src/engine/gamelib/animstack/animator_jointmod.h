#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "engine/models/skeletalanimation/jointanimator.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

class idAnimator_JointMod : public idAnimator_Base {
public:
  struct animatorJointMod_t {
    idMat3 axis;
    idVec3 pos;
    idVec3 scale;
    animationPose_t pose;
    jointModTransform_t type;
    idJointIndex_t jointIndex;
    std::uint16_t driver;
  };

  idAnimator_JointMod();
  ~idAnimator_JointMod() override;

  void SetJointPos(int modIndex, const idVec3& value);
  void SetJointAxis(int modIndex, const idMat3& value);
  void SetJointAxis(animationPose_t pose, jointModTransform_t type,
                    idJointIndex_t index, const idMat3& value);
  void SetJointPos(animationPose_t pose, jointModTransform_t type,
                   idJointIndex_t index, const idVec3& value);
  void ClearJointMods();
  int GenerateBlendJointMods(idMD6Blend::jointMod_t* outMods,
                             int maxOutJointMods);

  void PreSerializeInit(idGameTimeManager* gameTimeManager) override;
  void PreBlendSnapshot(idAnimStack* stack, int curTime, int ticksPerSec,
                        float fraction) override;
  serializeType_t GetSerializeType() override { return STYPE_JOINTMOD; }

  void SetEnabled(bool value) { enabled = value; }
  void SetUsePersistentMods(bool value) { usePersistentMods = value; }

  __forceinline static void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }

protected:
  bool InternalInit(const idAnimatorParms_Base& parms) override;
  void InternalShutdown(idAnimStack*) override {}
  void InternalPreBlendTree(const idAnimStack* stack, int curTime,
                            int ticksPerSec) override;
  void InternalPostBlendTree(const idAnimStack* stack, int curTime) override;
  void InternalStart(
      const idAnimStack*, int,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalEnd(
      const idAnimStack*, int,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalBlend(
      const idAnimStack*, int, float,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  bool InternalIsContributing() const override { return false; }
  const idMD6Branch* InternalGetMergeBranch() const override {
    return nullptr;
  }
  idMD6Branch* InternalGetMergeBranch() override { return nullptr; }

public:
  bool enabled;
  bool usePersistentMods;
  std::uint8_t recoveredListAlignment[6];
  idList<animatorJointMod_t, 5> jointMods;
  int maxJointMods;
  std::uint8_t recoveredTail[4];
};

static_assert(sizeof(idAnimator_JointMod::animatorJointMod_t) == 0x48,
              "joint-mod record ABI drift");
static_assert(offsetof(idAnimator_JointMod::animatorJointMod_t, pos) == 0x24,
              "joint-mod position offset drift");
static_assert(offsetof(idAnimator_JointMod::animatorJointMod_t, scale) == 0x30,
              "joint-mod scale offset drift");
static_assert(offsetof(idAnimator_JointMod::animatorJointMod_t, pose) == 0x3C,
              "joint-mod pose offset drift");
static_assert(offsetof(idAnimator_JointMod::animatorJointMod_t, type) == 0x40,
              "joint-mod transform offset drift");
static_assert(
    offsetof(idAnimator_JointMod::animatorJointMod_t, jointIndex) == 0x44,
    "joint-mod joint offset drift");
static_assert(offsetof(idAnimator_JointMod::animatorJointMod_t, driver) == 0x46,
              "joint-mod driver offset drift");
static_assert(offsetof(idAnimator_JointMod, enabled) == 0x50,
              "joint-mod enabled offset drift");
static_assert(offsetof(idAnimator_JointMod, usePersistentMods) == 0x51,
              "joint-mod persistence offset drift");
static_assert(offsetof(idAnimator_JointMod, jointMods) == 0x58,
              "joint-mod list offset drift");
static_assert(offsetof(idAnimator_JointMod, maxJointMods) == 0x70,
              "joint-mod maximum offset drift");
static_assert(sizeof(idAnimator_JointMod) == 0x78,
              "joint-mod animator ABI drift");
