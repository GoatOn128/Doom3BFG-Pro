#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

class idDeclFreezeJoints;
class idTreeAnimator;
class idCVar;

extern idCVar anim_enableFreezeJoints;

class idAnimatorParms_FreezeJoints : public idAnimatorParms_Base {
public:
  idAnimatorParms_FreezeJoints(const idDeclFreezeJoints* declaration,
                               idAnimStack* stack, const char* animatorName,
                               float animatorAlpha,
                               idTreeAnimator* sourceAnimator)
      : idAnimatorParms_Base(
            stack, animatorName, idMD6Blend::BOP_REF_LERP,
            idMD6Blend::ORIGINBLEND_BRANCH, MD6_WEIGHTGROUP_TORSO,
            MD6_WEIGHTGROUP_ALL, animatorAlpha),
        decl(declaration), treeAnimator(sourceAnimator) {}

  const idDeclFreezeJoints* decl;
  idTreeAnimator* treeAnimator;
};

class idAnimator_FreezeJoints : public idAnimator_Base {
public:
  class idJoint {
  public:
    idJoint() : index() {}

    idJointIndex_t index;
    idVec3 originalPosition;
    idMat3 originalRotation;
  };

  idAnimator_FreezeJoints();
  ~idAnimator_FreezeJoints() override;

  priority_t GetStackPriority() const override {
    return static_cast<priority_t>(100);
  }
  void PreBlendSnapshot(idAnimStack*, int, int, float) override {}

  static void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }

protected:
  bool InternalInit(const idAnimatorParms_Base& parms) override;
  void InternalShutdown(idAnimStack* stack) override;
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
    return mergeBranch;
  }
  idMD6Branch* InternalGetMergeBranch() override { return mergeBranch; }

public:
  bool enabled;
  idList<idJoint, 5> joints;
  idTreeAnimator* animator;
  idMD6LeafPause animLeaf;
  idMD6Branch* mergeBranch;

  friend struct AnimatorFreezeJointsValidationAccess;
};

template <>
bool idList<idAnimator_FreezeJoints::idJoint, 5>::Resize(int newSize);

static_assert(sizeof(idAnimatorParms_FreezeJoints) == 0x58,
              "freeze-joints animator parameters ABI drift");
static_assert(offsetof(idAnimatorParms_FreezeJoints, decl) == 0x48,
              "freeze-joints declaration parameter offset drift");
static_assert(offsetof(idAnimatorParms_FreezeJoints, treeAnimator) == 0x50,
              "freeze-joints tree-animator parameter offset drift");
static_assert(sizeof(idAnimator_FreezeJoints::idJoint) == 0x34,
              "freeze-joints joint record ABI drift");
static_assert(offsetof(idAnimator_FreezeJoints::idJoint, index) == 0x00,
              "freeze-joints joint index offset drift");
static_assert(
    offsetof(idAnimator_FreezeJoints::idJoint, originalPosition) == 0x04,
    "freeze-joints original-position offset drift");
static_assert(
    offsetof(idAnimator_FreezeJoints::idJoint, originalRotation) == 0x10,
    "freeze-joints original-rotation offset drift");
static_assert(offsetof(idAnimator_FreezeJoints, enabled) == 0x50,
              "freeze-joints enabled offset drift");
static_assert(offsetof(idAnimator_FreezeJoints, joints) == 0x58,
              "freeze-joints joint-list offset drift");
static_assert(offsetof(idAnimator_FreezeJoints, animator) == 0x70,
              "freeze-joints animator offset drift");
static_assert(offsetof(idAnimator_FreezeJoints, animLeaf) == 0x78,
              "freeze-joints pause-leaf offset drift");
static_assert(offsetof(idAnimator_FreezeJoints, mergeBranch) == 0xE8,
              "freeze-joints merge-branch offset drift");
static_assert(sizeof(idAnimator_FreezeJoints) == 0xF0,
              "freeze-joints animator ABI drift");
