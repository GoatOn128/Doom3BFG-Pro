#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "engine/gamelib/animstack/animweb/animator_animweb_abi.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

class idDeclDragJoints;
class idTreeAnimator;
class idCVar;

extern idCVar anim_enableDragJoints;
extern idCVar anim_enableDragJointsConstraints;
extern idCVar anim_debugDragJoints;

using dragSubWebIndex_t =
    idIndex<std::int16_t, idDeclAnimWeb::invalidSubWebIndex_t>;
using dragStateIndex_t =
    idIndex<std::int16_t, idDeclAnimWeb::invalidStateIndex_t>;

class idAnimatorParms_DragJoints : public idAnimatorParms_Base {
public:
  idAnimatorParms_DragJoints(const idDeclDragJoints* declaration,
                             idAnimStack* stack, const char* animatorName,
                             float animatorAlpha,
                             idTreeAnimator* sourceAnimator)
      : idAnimatorParms_Base(
            stack, animatorName, idMD6Blend::BOP_REF_LERP,
            idMD6Blend::ORIGINBLEND_BRANCH, MD6_WEIGHTGROUP_TORSO,
            MD6_WEIGHTGROUP_ALL, animatorAlpha),
        declDragJoints(declaration), treeAnimator(sourceAnimator) {}

  const idDeclDragJoints* declDragJoints;
  idTreeAnimator* treeAnimator;
};

class idAnimator_DragJoints : public idAnimator_Base {
public:
  class idJoint {
  public:
    idJoint() : index() {}

    idJointIndex_t index;
    idVec3 position;
    float length;
    idMat3 originalRotation;
  };

  idAnimator_DragJoints();
  ~idAnimator_DragJoints() override;

  priority_t GetStackPriority() const override {
    return static_cast<priority_t>(100);
  }
  void PreBlendSnapshot(idAnimStack*, int, int, float) override {}
  void SetAnimWeb(idAnimator_AnimWeb* web) { animweb = web; }

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

private:
  void ReinitJoints();

public:
  bool enabled;
  std::uint8_t enabledPadding[3];
  int blendFrame;
  int numBlendFrames;
  std::uint32_t blendPadding;
  idAnimator_AnimWeb* animweb;
  idList<dragSubWebIndex_t, 5> subwebs;
  idList<dragStateIndex_t, 5> excludeStates;
  idJointIndex_t baseJoint;
  std::uint8_t baseJointPadding[6];
  idList<idJoint, 5> joints;
  float maxBend;
  float downHillPitchAdjust;
  idTreeAnimator* animator;
  idMD6LeafPause animLeaf;
  idMD6Branch* mergeBranch;

  friend struct AnimatorDragJointsValidationAccess;
};

template <>
bool idList<idAnimator_DragJoints::idJoint, 5>::Resize(int newSize);

static_assert(sizeof(idAnimatorParms_DragJoints) == 0x58,
              "drag-joints animator parameters ABI drift");
static_assert(offsetof(idAnimatorParms_DragJoints, declDragJoints) == 0x48,
              "drag-joints declaration parameter offset drift");
static_assert(offsetof(idAnimatorParms_DragJoints, treeAnimator) == 0x50,
              "drag-joints tree-animator parameter offset drift");
static_assert(sizeof(idAnimator_DragJoints::idJoint) == 0x38,
              "drag-joints joint record ABI drift");
static_assert(offsetof(idAnimator_DragJoints::idJoint, position) == 0x04,
              "drag-joints position offset drift");
static_assert(offsetof(idAnimator_DragJoints::idJoint, length) == 0x10,
              "drag-joints length offset drift");
static_assert(offsetof(idAnimator_DragJoints::idJoint, originalRotation) ==
                  0x14,
              "drag-joints original rotation offset drift");
static_assert(offsetof(idAnimator_DragJoints, enabled) == 0x50,
              "drag-joints enabled offset drift");
static_assert(offsetof(idAnimator_DragJoints, blendFrame) == 0x54,
              "drag-joints blend-frame offset drift");
static_assert(offsetof(idAnimator_DragJoints, animweb) == 0x60,
              "drag-joints animweb offset drift");
static_assert(offsetof(idAnimator_DragJoints, subwebs) == 0x68,
              "drag-joints subweb list offset drift");
static_assert(offsetof(idAnimator_DragJoints, excludeStates) == 0x80,
              "drag-joints excluded-state list offset drift");
static_assert(offsetof(idAnimator_DragJoints, baseJoint) == 0x98,
              "drag-joints base-joint offset drift");
static_assert(offsetof(idAnimator_DragJoints, joints) == 0xA0,
              "drag-joints joint-list offset drift");
static_assert(offsetof(idAnimator_DragJoints, maxBend) == 0xB8,
              "drag-joints max-bend offset drift");
static_assert(offsetof(idAnimator_DragJoints, animator) == 0xC0,
              "drag-joints tree-animator offset drift");
static_assert(offsetof(idAnimator_DragJoints, animLeaf) == 0xC8,
              "drag-joints pause-leaf offset drift");
static_assert(offsetof(idAnimator_DragJoints, mergeBranch) == 0x138,
              "drag-joints merge-branch offset drift");
static_assert(sizeof(idAnimator_DragJoints) == 0x140,
              "drag-joints animator ABI drift");
