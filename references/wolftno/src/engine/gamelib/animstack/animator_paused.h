#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>

class idAnimatorParms_Pause : public idAnimatorParms_Base {
public:
  idAnimatorParms_Pause(
      const idMD6Anim* animation, float animationStartFrame,
      float animationNormalizedStartFrame, idAnimStack* stack,
      const char* animatorName, idMD6Blend::blendOp_t operation,
      idMD6Blend::originBlend_t originOperation,
      md6WeightGroup_t animatorWeightGroup,
      md6WeightGroup_t animatorFilterGroup, float animatorAlpha)
      : idAnimatorParms_Base(stack, animatorName, operation, originOperation,
                             animatorWeightGroup, animatorFilterGroup,
                             animatorAlpha),
        anim(animation), startFrame(animationStartFrame),
        normalizedStartFrame(animationNormalizedStartFrame) {}

  const idMD6Anim* anim;
  float startFrame;
  float normalizedStartFrame;
};

class idAnimator_Paused : public idAnimator_Base {
public:
  idAnimator_Paused();
  ~idAnimator_Paused() override;

  void SetAnim(const idMD6Anim* animation);
  const idMD6Anim* GetAnim() const;
  void SetFrame(float frame);
  float GetFrame() const;
  void SetNormalizedFrame(float normalizedFrame);

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
  void InternalPreBlendTree(const idAnimStack*, int, int) override {}
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
  bool InternalIsContributing() const override;
  const idMD6Branch* InternalGetMergeBranch() const override;
  idMD6Branch* InternalGetMergeBranch() override;

public:
  idMD6LeafPause leaf;
  idMD6Branch mergeBranch;
  const idMD6Anim* anim;

  friend struct AnimatorPausedValidationAccess;
};

static_assert(sizeof(idAnimatorParms_Pause) == 0x58,
              "paused animator parameters ABI drift");
static_assert(offsetof(idAnimatorParms_Pause, anim) == 0x48,
              "paused animation parameter offset drift");
static_assert(offsetof(idAnimatorParms_Pause, startFrame) == 0x50,
              "paused start-frame parameter offset drift");
static_assert(offsetof(idAnimatorParms_Pause, normalizedStartFrame) == 0x54,
              "paused normalized-frame parameter offset drift");
static_assert(offsetof(idAnimator_Paused, leaf) == 0x50,
              "paused leaf offset drift");
static_assert(offsetof(idAnimator_Paused, mergeBranch) == 0xC0,
              "paused merge-branch offset drift");
static_assert(offsetof(idAnimator_Paused, anim) == 0xF8,
              "paused animation offset drift");
static_assert(sizeof(idAnimator_Paused) == 0x100,
              "paused animator ABI drift");
