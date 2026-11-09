#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>

class idAnimatorParms_EndAdditiveChannels : public idAnimatorParms_Base {
public:
  idAnimatorParms_EndAdditiveChannels(
      idAnimStack* stack, const char* animatorName, int channelCount,
      md6WeightGroup_t animatorWeightGroup,
      md6WeightGroup_t animatorFilterGroup,
      idMD6Blend::blendOp_t initialBlendOp)
      : idAnimatorParms_Base(
            stack, animatorName, idMD6Blend::BOP_ADD_RIGHT,
            idMD6Blend::ORIGINBLEND_BRANCH, animatorWeightGroup,
            animatorFilterGroup, 1.0f),
        maxChannels(channelCount), firstBlendOp(initialBlendOp) {}

  int maxChannels;
  idMD6Blend::blendOp_t firstBlendOp;
};

class idAnimator_EndAdditiveChannels : public idAnimator_Base {
public:
  idAnimator_EndAdditiveChannels();
  __declspec(noinline) ~idAnimator_EndAdditiveChannels() override;

  void SetAnimHandlesAndWeights(int curTime, const idAliasHandle_t* handles,
                                int numAnims, const float* weights,
                                int numWeights);
  void SetExternalBlend(float blendTarget, int blendMS);
  void SetWrapMode(idMD6Leaf::wrapMode_t mode) { wrapMode = mode; }

  __forceinline static void operator delete(void* memory) noexcept {
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
  bool InternalIsContributing() const override;
  const idMD6Branch* InternalGetMergeBranch() const override;
  idMD6Branch* InternalGetMergeBranch() override;
  const idMD6Branch* InternalGetEndBranch() const override;
  idMD6Branch* InternalGetEndBranch() override;

public:
  idList<idMD6LeafPlay*, 5> leaves;
  idList<idMD6Branch*, 5> branches;
  idList<float, 5> aliasWeights;
  idMD6Branch mergeBranch;
  idList<idAliasHandle_t, 5> animAlias;
  int numAddedChannels;
  int numCurrentChannels;
  idMD6Leaf::wrapMode_t wrapMode;
  idMD6Blend::blendOp_t firstBlendOp;
  float currentExternalAlpha;
  float targetExternalAlpha;
  float externalAlphaRate;
};

static_assert(sizeof(idAnimatorParms_EndAdditiveChannels) == 0x50,
              "end-additive parameter ABI drift");
static_assert(offsetof(idAnimatorParms_EndAdditiveChannels, maxChannels) ==
                  0x48,
              "end-additive channel-count parameter offset drift");
static_assert(offsetof(idAnimatorParms_EndAdditiveChannels, firstBlendOp) ==
                  0x4C,
              "end-additive blend-op parameter offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, leaves) == 0x50,
              "end-additive leaves offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, branches) == 0x68,
              "end-additive branches offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, aliasWeights) == 0x80,
              "end-additive weights offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, mergeBranch) == 0x98,
              "end-additive merge-branch offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, animAlias) == 0xD0,
              "end-additive aliases offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, numAddedChannels) ==
                  0xE8,
              "end-additive active-count offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, numCurrentChannels) ==
                  0xEC,
              "end-additive current-count offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, wrapMode) == 0xF0,
              "end-additive wrap-mode offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, firstBlendOp) == 0xF4,
              "end-additive first blend-op offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, currentExternalAlpha) ==
                  0xF8,
              "end-additive current external-alpha offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, targetExternalAlpha) ==
                  0xFC,
              "end-additive target external-alpha offset drift");
static_assert(offsetof(idAnimator_EndAdditiveChannels, externalAlphaRate) ==
                  0x100,
              "end-additive external-alpha rate offset drift");
static_assert(sizeof(idAnimator_EndAdditiveChannels) == 0x108,
              "end-additive animator ABI drift");
