#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "engine/decls/animwebs/blendparms.h"
#include "shared/idlib/math/random.h"

#include <cstddef>
#include <cstdint>

class idJointAnimator;

class idAnimator_Channel : public idAnimator_Base {
public:
  idAnimator_Channel();
  ~idAnimator_Channel()
#if defined(WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI)
      noexcept(false)
#endif
      override;

  bool PlayAnim(const idAnimStack* stack, const idMD6Anim* animation,
                int curTime, float rateScale, const blendParms_t& blendParms,
                int blendOutDurationMS, idMD6LeafPlay** leafStarted,
                float targetAlpha);
  bool PlayAnim(const idAnimStack* stack, const idAliasHandle_t& alias,
                int curTime, float rateScale, const blendParms_t& blendParms,
                int blendOutDurationMS, idMD6LeafPlay** leafStarted,
                float targetAlpha);
  bool CycleAnim(const idAnimStack* stack, const idMD6Anim* animation,
                 int curTime, float rateScale, const blendParms_t& blendParms,
                 idMD6LeafPlay** leafStarted, float targetAlpha);
  bool CycleAnim(const idAnimStack* stack, const idAliasHandle_t& alias,
                 int curTime, float rateScale, const blendParms_t& blendParms,
                 idMD6LeafPlay** leafStarted, float targetAlpha);
  bool IsAnimPlaying(const idAnimStack* stack, const idAliasHandle_t& alias,
                     int curTime) const;
  bool IsDone(int curTime) const;
  bool IsDone(int curTime, bool clampIgnoreExtraFrame) const;
  idMD6LeafPlay* GetLastPlayedLeaf();
  void ClearAnimator();

  void SetForceFree(bool value) { forceFree = value; }
  void SetOmitFreeLeafWarning(bool value) { omitFreeLeafWarning = value; }

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
      const idAnimStack* stack, int curTime,
      idTypesafeNumber<int, gameTimeUnique_t> endTime) override;
  void InternalBlend(
      const idAnimStack*, int, float,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  bool InternalIsContributing() const override { return true; }
  const idMD6Branch* InternalGetMergeBranch() const override {
    return mergeBranch;
  }
  idMD6Branch* InternalGetMergeBranch() override { return mergeBranch; }

  idMD6LeafPlay* leaves[3];
  idMD6Branch* branches[2];
  idMD6Branch* mergeBranch;
  idMD6LeafPlay* freeLeaves[3];
  idMD6Branch* freeBranches[2];
  idRandom random;
  std::int16_t numFreeLeaves;
  std::int16_t numFreeBranches;
  std::int16_t blendOutDurationMS;
  std::uint8_t forceFree : 1;
  std::uint8_t omitFreeLeafWarning : 1;
  std::uint8_t channelFlagPadding : 6;
  std::uint8_t channelTailPadding[5];

  friend class AnimatorPainValidationAccess;
  friend class AnimatorChannelValidationAccess;

private:
  bool StartNode(idJointAnimator* animator, idMD6Node* node, int curTime,
                 idMD6Blend::blendOp_t blendOp,
                 idMD6Leaf::wrapMode_t wrap,
                 const blendParms_t& blendParms, int blendOutDurationMS,
                 float targetAlpha);
  bool StartLeaf(idJointAnimator* animator, const idMD6Anim* animation,
                 int curTime, float rateScale,
                 idMD6Blend::blendOp_t blendOp,
                 idMD6Leaf::wrapMode_t wrap,
                 const blendParms_t& blendParms, int blendOutDurationMS,
                 idMD6LeafPlay** leafStarted, float targetAlpha);
  bool StartLeaf(idJointAnimator* animator, const idAliasHandle_t& alias,
                 int curTime, float rateScale,
                 idMD6Blend::blendOp_t blendOp,
                 idMD6Leaf::wrapMode_t wrap,
                 const blendParms_t& blendParms, int blendOutDurationMS,
                 idMD6LeafPlay** leafStarted, float targetAlpha);
  void ForceFreeLeaf();
  void FreeTree(idMD6Node* node);
};

static_assert(sizeof(idAnimator_Channel) == 0xB8,
              "idAnimator_Channel ABI drift");
