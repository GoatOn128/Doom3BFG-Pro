#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#define WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#include "engine/gamelib/animstack/animator_base.h"

#include "engine/framework/gametimemanager.h"
#include "engine/gamelib/animstack/animstack.h"
#include "shared/idlib/lib.h"
#undef WOLFTNO_RETAIL_IDLIB_ERROR_RETURNS_ABI
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include <cmath>
#include <intrin.h>

idAnimator_Base::~idAnimator_Base() {}

bool idAnimator_Base::InternalPostInit(const idAnimatorParms_Base& parms) {
  if (parms.animStack == nullptr) {
    return false;
  }
  parms.animStack->SetAnimatorFlag(this,
                                   idAnimStack::ANIMATORFLAG_SERIALIZE);
  return true;
}

void idAnimator_Base::Shutdown(idAnimStack* stack) {
  shutdownCalled = true;
  InternalShutdown(stack);
  if (stack != nullptr) {
    stack->ClearAnimatorFlag(this, idAnimStack::ANIMATORFLAG_INITIALIZED);
    stack->RemoveAnimator(this);
  }
}

bool idAnimator_Base::IsInitialized(idAnimStack* animStack) const {
  return animStack != nullptr &&
         animStack->IsAnimatorFlagSet(
             this, idAnimStack::ANIMATORFLAG_INITIALIZED);
}

bool idAnimator_Base::IsEnabled(idAnimStack* animStack) const {
  return animStack != nullptr &&
         animStack->IsAnimatorFlagSet(this,
                                      idAnimStack::ANIMATORFLAG_ENABLED);
}

void idAnimator_Base::SetEnabled(idAnimStack* animStack, bool enabled) {
  if (enabled) {
    animStack->SetAnimatorFlag(this, idAnimStack::ANIMATORFLAG_ENABLED);
  } else {
    animStack->ClearAnimatorFlag(this, idAnimStack::ANIMATORFLAG_ENABLED);
  }
}

idAnimator_Base::idAnimator_Base() {
  gametimeManager.gameTimeManager = nullptr;
  filterGroup = MD6_WEIGHTGROUP_ALL;
  weightGroup = MD6_WEIGHTGROUP_ALL;
  serializeProps.serializedTrees[0] = nullptr;
  serializeProps.serializedTrees[1] = nullptr;
  serializeProps.treeAnimator = nullptr;
  serializeProps.animStack = nullptr;
  serializeProps.clip = nullptr;
  serializeProps.createdThroughSerialization = false;
  *reinterpret_cast<std::uint8_t*>(
      reinterpret_cast<std::uint8_t*>(this) + 0x48) &= 0xFCu;
}

bool idAnimator_Base::Init(idGameTimeManager* gameTimeManager,
                           const idAnimatorParms_Base& parms) {
  if (parms.animStack->GetAnimatorIndex(this) >= 0 && initialized) {
    idLib::Error("Attempted to add idAnimator %s twice.",
                 parms.name.c_str());
    __debugbreak();
  }

  gametimeManager.gameTimeManager = gameTimeManager;
  weightGroup = parms.weightGroup;
  filterGroup = parms.filterGroup;
  if (!InternalInit(parms)) {
    return false;
  }

  using MutableMergeBranchFn = idMD6Branch*(__fastcall*)(idAnimator_Base*);
  MutableMergeBranchFn* const mergeVtable =
      *reinterpret_cast<MutableMergeBranchFn**>(this);
  idMD6Branch* const mergeBranch = mergeVtable[16](this);
  if (mergeBranch != nullptr) {
    mergeBranch->op = static_cast<std::uint8_t>(parms.blendOp);
    mergeBranch->originBlend =
        static_cast<std::uint8_t>(parms.originBlend);
    mergeBranch->currentAlpha = parms.alpha;
    mergeBranch->alphaRate = 0.0f;
    mergeBranch->targetAlpha = parms.alpha;
    mergeBranch->filterGroup =
        static_cast<std::uint8_t>(parms.filterGroup);
    const va mergeName("%s_merge", parms.name.c_str());
    (void)mergeName;
  }

  parms.animStack->AddAnimator(this);
  parms.animStack->SetAnimatorFlag(
      this, idAnimStack::ANIMATORFLAG_INITIALIZED);
  parms.animStack->SetAnimatorFlag(this,
                                   idAnimStack::ANIMATORFLAG_ENABLED);
  volatile std::uint8_t& flags = *reinterpret_cast<std::uint8_t*>(
      reinterpret_cast<std::uint8_t*>(this) + 0x48);
  flags = static_cast<std::uint8_t>((flags & 0xFCu) | 1u);
  return InternalPostInit(parms);
}

void idAnimator_Base::Blend(const idAnimStack* stack, int curTime,
                            float targetAlpha, int blendDurationMS,
                            bool reset) {
  using MutableMergeBranchFn = idMD6Branch*(__fastcall*)(idAnimator_Base*);
  MutableMergeBranchFn* const mergeVtable =
      *reinterpret_cast<MutableMergeBranchFn**>(this);
  idMD6Branch* const mergeBranch = mergeVtable[16](this);
  if (mergeBranch == nullptr) {
    return;
  }

  mergeBranch->targetAlpha = targetAlpha;
  const idTypesafeNumber<int, gameTimeUnique_t> blendTicks =
      gametimeManager.gameTimeManager->ConvertTimeRealToGame(
          idTypesafeNumber<int, millisecondUnique_t>(blendDurationMS));
  const bool alreadyBlending =
      targetAlpha <= mergeBranch->currentAlpha
          ? mergeBranch->IsBlendingOut()
          : mergeBranch->IsBlendingIn();
  if (blendTicks.value <= 0) {
    mergeBranch->currentAlpha = targetAlpha;
    mergeBranch->alphaRate = 0.0f;
  } else if (reset || !alreadyBlending) {
    const float difference = targetAlpha - mergeBranch->currentAlpha;
    volatile double widenedDifference = static_cast<double>(difference);
    widenedDifference = std::fabs(widenedDifference);
    const float absoluteDifference =
        static_cast<float>(widenedDifference);
    mergeBranch->alphaRate =
        absoluteDifference *
        (1000.0f / static_cast<float>(blendTicks.value));
  }
  InternalBlend(stack, curTime, targetAlpha, blendTicks);
}

bool idAnimator_Base::IsContributing() const {
  using ConstMergeBranchFn =
      const idMD6Branch*(__fastcall*)(const idAnimator_Base*);
  ConstMergeBranchFn* const mergeVtable =
      *reinterpret_cast<ConstMergeBranchFn**>(
          const_cast<idAnimator_Base*>(this));
  const idMD6Branch* const mergeBranch = mergeVtable[15](this);
  return mergeBranch != nullptr && mergeBranch->right != nullptr &&
         mergeBranch->right->IsValid() &&
         (mergeBranch->currentAlpha > 0.0f ||
          !mergeBranch->IsBlendFinished()) &&
         InternalIsContributing();
}

md6WeightGroup_t idAnimator_Base::GetFilterGroup() const {
  using ConstMergeBranchFn =
      const idMD6Branch*(__fastcall*)(const idAnimator_Base*);
  ConstMergeBranchFn* const mergeVtable =
      *reinterpret_cast<ConstMergeBranchFn**>(
          const_cast<idAnimator_Base*>(this));
  const idMD6Branch* const mergeBranch = mergeVtable[15](this);
  return mergeBranch != nullptr
             ? static_cast<md6WeightGroup_t>(mergeBranch->filterGroup)
             : MD6_WEIGHTGROUP_ALL;
}

float idAnimator_Base::GetAlpha() const {
  using ConstMergeBranchFn =
      const idMD6Branch*(__fastcall*)(const idAnimator_Base*);
  ConstMergeBranchFn* const mergeVtable =
      *reinterpret_cast<ConstMergeBranchFn**>(
          const_cast<idAnimator_Base*>(this));
  const idMD6Branch* const mergeBranch = mergeVtable[15](this);
  return mergeBranch != nullptr ? mergeBranch->currentAlpha : 1.0f;
}

void idAnimator_Base::SetAlpha(float alpha) {
  using MutableMergeBranchFn = idMD6Branch*(__fastcall*)(idAnimator_Base*);
  MutableMergeBranchFn* const mergeVtable =
      *reinterpret_cast<MutableMergeBranchFn**>(this);
  idMD6Branch* const mergeBranch = mergeVtable[16](this);
  if (mergeBranch != nullptr) {
    mergeBranch->currentAlpha = alpha;
  }
}

void idAnimator_Base::SetTargetAlpha(float alpha, float alphaRate) {
  using MutableMergeBranchFn = idMD6Branch*(__fastcall*)(idAnimator_Base*);
  MutableMergeBranchFn* const mergeVtable =
      *reinterpret_cast<MutableMergeBranchFn**>(this);
  idMD6Branch* const mergeBranch = mergeVtable[16](this);
  if (mergeBranch == nullptr) {
    return;
  }
  const bool hasPositiveRate = alphaRate > 0.0f;
  _ReadWriteBarrier();
  mergeBranch->targetAlpha = alpha;
  if (hasPositiveRate) {
    mergeBranch->alphaRate = alphaRate;
  } else {
    mergeBranch->currentAlpha = alpha;
    mergeBranch->alphaRate = 0.0f;
  }
}

void idAnimator_Base::Pause(
    const idAnimStack* stack,
    idTypesafeNumber<int, gameTimeUnique_t> curTime) {
  InternalPause(stack, curTime);
}

void idAnimator_Base::Unpause(
    const idAnimStack* stack,
    idTypesafeNumber<int, gameTimeUnique_t> curTime) {
  InternalUnpause(stack, curTime);
}

void idAnimator_Base::Start(const idAnimStack* stack, int curTime,
                            int blendDurationMS, bool reset) {
  using MutableMergeBranchFn = idMD6Branch*(__fastcall*)(idAnimator_Base*);
  MutableMergeBranchFn* const mergeVtable =
      *reinterpret_cast<MutableMergeBranchFn**>(this);
  idMD6Branch* const mergeBranch = mergeVtable[16](this);
  if (mergeBranch == nullptr) {
    return;
  }

  const idTypesafeNumber<int, gameTimeUnique_t> blendTicks =
      gametimeManager.gameTimeManager->ConvertTimeRealToGame(
          idTypesafeNumber<int, millisecondUnique_t>(blendDurationMS));
  if (blendTicks.value <= 0) {
    mergeBranch->currentAlpha = 1.0f;
    mergeBranch->targetAlpha = 1.0f;
    mergeBranch->alphaRate = 0.0f;
  } else if (reset || !mergeBranch->IsBlendingIn()) {
    mergeBranch->targetAlpha = 1.0f;
    mergeBranch->alphaRate =
        (1.0f - mergeBranch->currentAlpha) *
        (static_cast<float>(timeManager.gameTimePerSecond.value) /
         static_cast<float>(blendTicks.value));
  }
  InternalStart(stack, curTime, blendTicks);
}

void idAnimator_Base::End(const idAnimStack* stack, int curTime,
                          int blendDurationMS, bool reset) {
  if (!initialized) {
    return;
  }
  using MutableMergeBranchFn = idMD6Branch*(__fastcall*)(idAnimator_Base*);
  MutableMergeBranchFn* const mergeVtable =
      *reinterpret_cast<MutableMergeBranchFn**>(this);
  idMD6Branch* const mergeBranch = mergeVtable[16](this);
  if (mergeBranch == nullptr) {
    return;
  }

  const idTypesafeNumber<int, gameTimeUnique_t> blendTicks =
      gametimeManager.gameTimeManager->ConvertTimeRealToGame(
          idTypesafeNumber<int, millisecondUnique_t>(blendDurationMS));
  if (blendTicks.value <= 0) {
    mergeBranch->targetAlpha = 0.0f;
    mergeBranch->alphaRate = 0.0f;
    mergeBranch->currentAlpha = 0.0f;
  } else if (reset || !mergeBranch->IsBlendingOut()) {
    mergeBranch->targetAlpha = 0.0f;
    mergeBranch->alphaRate =
        (static_cast<float>(timeManager.gameTimePerSecond.value) /
         static_cast<float>(blendTicks.value)) *
        mergeBranch->currentAlpha;
  }
  InternalEnd(stack, curTime, blendTicks);
}
