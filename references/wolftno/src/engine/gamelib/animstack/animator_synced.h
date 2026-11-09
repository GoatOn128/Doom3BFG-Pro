#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>

class idAnimator_Synced : public idAnimator_Base {
public:
  static constexpr int MAX_SYNCED_ANIMS = 8;
  static constexpr int MAX_LEAVES = 8;
  static constexpr int MAX_BRANCHES = 7;

  idAnimator_Synced();
  __declspec(noinline) ~idAnimator_Synced()
#if defined(WOLFTNO_RETAIL_SYNCED_DTOR_UNWIND_ABI)
      noexcept(false)
#endif
      override;

  int NumAnims() const { return anims.num; }
  const idMD6Anim* GetAnim(int index) const { return anims.list[index]; }
  idAliasHandle_t GetAliasHandle(int index) const {
    return aliasHandles.list[index];
  }
  float GetAnimWeight(int index) const { return aliasWeights.list[index]; }

  void SetAnimHandle(const idDeclMD6* declaration, int curTime, int index,
                     const idAliasHandle_t& aliasHandle);
  void SetAnimHandlesAndWeights(const idDeclMD6* declaration, int curTime,
                                const idAliasHandle_t* handles, int numAnims,
                                const float* weights, int numWeights);
  bool IsDone(int curTime);

  __forceinline static void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }

protected:
  bool InternalInit(const idAnimatorParms_Base&) override { return true; }
  void InternalShutdown(idAnimStack*) override {}
  void InternalPreBlendTree(const idAnimStack* stack, int curTime,
                            int ticksPerSec) override;
  void InternalPostBlendTree(const idAnimStack*, int) override {}
  void InternalStart(
      const idAnimStack*, int,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  void InternalEnd(
      const idAnimStack* stack, int curTime,
      idTypesafeNumber<int, gameTimeUnique_t> blendDurationTicks) override;
  void InternalBlend(
      const idAnimStack*, int, float,
      idTypesafeNumber<int, gameTimeUnique_t>) override {}
  bool InternalIsContributing() const override;
  const idMD6Branch* InternalGetMergeBranch() const override;
  idMD6Branch* InternalGetMergeBranch() override;

  void CalcAlphaValuesFromWeights();

public:
  idMD6LeafPlay leaves[MAX_LEAVES];
  idMD6Branch branches[MAX_BRANCHES];
  idMD6Branch mergeBranch;
  idStaticList<const idMD6Anim*, MAX_SYNCED_ANIMS> anims;
  idStaticList<idAliasHandle_t, MAX_SYNCED_ANIMS> aliasHandles;
  idStaticList<float, MAX_SYNCED_ANIMS> aliasWeights;
  idStaticList<float, MAX_SYNCED_ANIMS> alphaValues;
  idMD6Leaf::wrapMode_t wrapMode;
  float rateScale;

};

static_assert(offsetof(idAnimator_Synced, leaves) == 0x50,
              "synced leaves offset drift");
static_assert(offsetof(idAnimator_Synced, branches) == 0x190,
              "synced branches offset drift");
static_assert(offsetof(idAnimator_Synced, mergeBranch) == 0x318,
              "synced merge branch offset drift");
static_assert(offsetof(idAnimator_Synced, anims) == 0x350,
              "synced animation list offset drift");
static_assert(offsetof(idAnimator_Synced, aliasHandles) == 0x3A8,
              "synced alias list offset drift");
static_assert(offsetof(idAnimator_Synced, aliasWeights) == 0x3D0,
              "synced weight list offset drift");
static_assert(offsetof(idAnimator_Synced, alphaValues) == 0x408,
              "synced alpha list offset drift");
static_assert(offsetof(idAnimator_Synced, wrapMode) == 0x440,
              "synced wrap mode offset drift");
static_assert(offsetof(idAnimator_Synced, rateScale) == 0x444,
              "synced rate scale offset drift");
static_assert(sizeof(idAnimator_Synced) == 0x448,
              "synced animator ABI drift");
