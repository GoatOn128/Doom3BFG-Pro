#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

class idAnimator_ChannelWeight : public idAnimator_Base {
public:
  static constexpr std::uint16_t InvalidLeafIndex = 0xFFFF;

  class leafAnimation_t {
  public:
    using idListAllocReturnsReferenceMarker = void;

    leafAnimation_t()
        : animation(nullptr), leaf(nullptr), branch(nullptr) {}

    const idMD6Anim* animation;
    idMD6LeafPause* leaf;
    idMD6Branch* branch;
  };

  class baseAnimation_t {
  public:
    const idMD6Anim* animation;
    idMD6LeafPause* leaf;
  };

  idAnimator_ChannelWeight();
  __declspec(noinline) ~idAnimator_ChannelWeight()
#if defined(WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI)
      noexcept(false)
#endif
      override
      ;

  void SetControlAnimation(const idMD6LeafPlay* animation, void* lockOwner_);
  void SetEntityNumber(int number) { entityNumber = number; }

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
  idMD6Branch* InternalGetMergeBranch() override { return mergeBranch; }

public:
  idList<std::uint16_t, 5> userChannelToLeafMap;
  idList<leafAnimation_t, 5> leaves;
  baseAnimation_t baseAnimation;
  idMD6Branch* mergeBranch;
  const idMD6LeafPlay* controlAnimation;
  int entityNumber;
  std::uint8_t recoveredPadding[4];
  void* lockOwner;
};

static_assert(sizeof(idAnimator_ChannelWeight::leafAnimation_t) == 0x18,
              "channel-weight leaf ABI drift");
static_assert(offsetof(idAnimator_ChannelWeight::leafAnimation_t, branch) ==
                  0x10,
              "channel-weight leaf branch offset drift");
static_assert(sizeof(idAnimator_ChannelWeight::baseAnimation_t) == 0x10,
              "channel-weight base-animation ABI drift");
static_assert(offsetof(idAnimator_ChannelWeight, userChannelToLeafMap) == 0x50,
              "channel-weight map offset drift");
static_assert(offsetof(idAnimator_ChannelWeight, leaves) == 0x68,
              "channel-weight leaves offset drift");
static_assert(offsetof(idAnimator_ChannelWeight, baseAnimation) == 0x80,
              "channel-weight base animation offset drift");
static_assert(offsetof(idAnimator_ChannelWeight, mergeBranch) == 0x90,
              "channel-weight merge offset drift");
static_assert(offsetof(idAnimator_ChannelWeight, controlAnimation) == 0x98,
              "channel-weight control offset drift");
static_assert(offsetof(idAnimator_ChannelWeight, entityNumber) == 0xA0,
              "channel-weight entity offset drift");
static_assert(offsetof(idAnimator_ChannelWeight, lockOwner) == 0xA8,
              "channel-weight lock offset drift");
static_assert(sizeof(idAnimator_ChannelWeight) == 0xB0,
              "channel-weight animator ABI drift");
