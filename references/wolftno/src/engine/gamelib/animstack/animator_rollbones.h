#pragma once

#include "engine/decls/declrollbones.h"
#include "engine/gamelib/animstack/animator_base.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>

class idAnimatorParms_RollBones : public idAnimatorParms_Base {
public:
  idAnimatorParms_RollBones(const idDeclRollBones* declaration,
                            idAnimStack* stack, const char* animatorName,
                            float animatorAlpha)
      : idAnimatorParms_Base(
            stack, animatorName, idMD6Blend::BOP_REF_LERP,
            idMD6Blend::ORIGINBLEND_BRANCH, MD6_WEIGHTGROUP_TORSO,
            MD6_WEIGHTGROUP_ALL, animatorAlpha),
        declRollBones(declaration) {}

  const idDeclRollBones* declRollBones;
};

class idAnimator_RollBones : public idAnimator_Base {
public:
  class idRollInstance {
  public:
    idMat3 localInv;
    idVec3 rollAxis;
    float rollAmount;
    idJointIndex_t rollSourceJoint;
    idJointIndex_t rollDestJoint;
  };

  idAnimator_RollBones();
  ~idAnimator_RollBones() override;

  priority_t GetStackPriority() const override {
    return static_cast<priority_t>(100);
  }
  void PreBlendSnapshot(idAnimStack*, int, int, float) override {}

  static bool HasCompatibleRollBones(const idDeclRollBones* rollBoneSetup,
                                     const idDeclMD6* declaration);

  static void operator delete(void* memory) noexcept {
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
  void InternalPostBlendTree(const idAnimStack*, int) override {}
  void InternalStart(
      const idAnimStack*, int,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalEnd(
      const idAnimStack*, int,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalBlend(
      const idAnimStack*, int, float,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  bool InternalIsContributing() const override { return enabled; }
  const idMD6Branch* InternalGetMergeBranch() const override {
    return &mergeBranch;
  }
  idMD6Branch* InternalGetMergeBranch() override { return &mergeBranch; }

public:
  bool enabled;
  std::uint8_t recoveredEnabledPadding[7];
  void* animator;
  idMD6LeafPause animLeaf;
  idMD6Branch mergeBranch;
  idList<idRollInstance, 5> rollBones;
};

template <>
bool idList<idAnimator_RollBones::idRollInstance, 5>::Resize(int newSize);
template <>
bool idList<idAnimator_RollBones::idRollInstance, 5>::SetNum(int newNum);

static_assert(sizeof(idAnimatorParms_RollBones) == 0x50,
              "roll-bones animator parameters ABI drift");
static_assert(offsetof(idAnimatorParms_RollBones, declRollBones) == 0x48,
              "roll-bones declaration parameter offset drift");
static_assert(sizeof(idAnimator_RollBones::idRollInstance) == 0x38,
              "roll-bones instance ABI drift");
static_assert(offsetof(idAnimator_RollBones::idRollInstance, localInv) == 0x00,
              "roll-bones local inverse offset drift");
static_assert(offsetof(idAnimator_RollBones::idRollInstance, rollAxis) == 0x24,
              "roll-bones axis offset drift");
static_assert(offsetof(idAnimator_RollBones::idRollInstance, rollAmount) == 0x30,
              "roll-bones amount offset drift");
static_assert(
    offsetof(idAnimator_RollBones::idRollInstance, rollSourceJoint) == 0x34,
    "roll-bones source-joint offset drift");
static_assert(
    offsetof(idAnimator_RollBones::idRollInstance, rollDestJoint) == 0x36,
    "roll-bones destination-joint offset drift");
static_assert(offsetof(idAnimator_RollBones, enabled) == 0x50,
              "roll-bones enabled offset drift");
static_assert(offsetof(idAnimator_RollBones, animator) == 0x58,
              "roll-bones animator offset drift");
static_assert(offsetof(idAnimator_RollBones, animLeaf) == 0x60,
              "roll-bones pause-leaf offset drift");
static_assert(offsetof(idAnimator_RollBones, mergeBranch) == 0xD0,
              "roll-bones merge-branch offset drift");
static_assert(offsetof(idAnimator_RollBones, rollBones) == 0x108,
              "roll-bones list offset drift");
static_assert(sizeof(idAnimator_RollBones) == 0x120,
              "roll-bones animator ABI drift");
