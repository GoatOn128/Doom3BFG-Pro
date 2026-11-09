#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>
#include <cstdint>

class idAnimator_FaceTracks : public idAnimator_Base {
public:
  idAnimator_FaceTracks();
  ~idAnimator_FaceTracks() override;

  priority_t GetStackPriority() const override {
    return static_cast<priority_t>(101);
  }

  static bool HasFaceTracks(const idDeclMD6* declaration);
  void AddUserChannelMod(idUserChannelIndex userChannel, float value,
                         int curTime);

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
  bool InternalIsContributing() const override { return true; }
  const idMD6Branch* InternalGetMergeBranch() const override;
  idMD6Branch* InternalGetMergeBranch() override;
  const idMD6Branch* InternalGetEndBranch() const override;
  idMD6Branch* InternalGetEndBranch() override;

public:
  idList<idMD6LeafPause*, 5> leaves;
  idList<idMD6Branch*, 5> branches;
  idList<int, 5> leafAnimModsTime;
  idMD6Branch mergeBranch;
  idMD6Branch* endBranch;
  idAliasHandle_t faceSetupAnimAlias;
  std::uint8_t recoveredAliasPadding[2];
  int animModsTime;

};

template <>
bool idList<idMD6Anim*, 5>::Resize(int newSize);
template <>
bool idList<int, 5>::Resize(int newSize);

static_assert(offsetof(idAnimator_FaceTracks, leaves) == 0x50,
              "face-tracks leaves offset drift");
static_assert(offsetof(idAnimator_FaceTracks, branches) == 0x68,
              "face-tracks branches offset drift");
static_assert(offsetof(idAnimator_FaceTracks, leafAnimModsTime) == 0x80,
              "face-tracks modification-time offset drift");
static_assert(offsetof(idAnimator_FaceTracks, mergeBranch) == 0x98,
              "face-tracks merge-branch offset drift");
static_assert(offsetof(idAnimator_FaceTracks, endBranch) == 0xD0,
              "face-tracks end-branch offset drift");
static_assert(offsetof(idAnimator_FaceTracks, faceSetupAnimAlias) == 0xD8,
              "face-tracks alias offset drift");
static_assert(offsetof(idAnimator_FaceTracks, animModsTime) == 0xDC,
              "face-tracks modification deadline offset drift");
static_assert(sizeof(idAnimator_FaceTracks) == 0xE0,
              "face-tracks animator ABI drift");
