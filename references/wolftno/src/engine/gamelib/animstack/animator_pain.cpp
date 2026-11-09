#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/animstack/animator_pain.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include "engine/gamelib/animstack/animstack.h"

idAnimator_Pain::idAnimator_Pain() = default;

idAnimator_Pain::~idAnimator_Pain() = default;

bool idAnimator_Pain::InternalPostInit(const idAnimatorParms_Base& parms) {
  if (parms.animStack == nullptr) {
    return false;
  }
  parms.animStack->ClearAnimatorFlag(
      this, idAnimStack::ANIMATORFLAG_SERIALIZE);
  return true;
}

idAnimator_Shoot::idAnimator_Shoot() = default;

idAnimator_Shoot::~idAnimator_Shoot() = default;

void idAnimator_Pain::PreSerializeInit(
    idGameTimeManager* gameTimeManager) {
  gametimeManager.gameTimeManager = gameTimeManager;
  serializeProps.createdThroughSerialization = true;

  idAnimatorParms_Base painParms(
      serializeProps.animStack, "pain", idMD6Blend::BOP_ADD_RIGHT,
      idMD6Blend::ORIGINBLEND_BRANCH, MD6_WEIGHTGROUP_ALL,
      MD6_WEIGHTGROUP_ALL, 1.0f);
  Init(gameTimeManager, painParms);
  serializeProps.animStack->ClearAnimatorFlag(
      this, idAnimStack::ANIMATORFLAG_SERIALIZE);
}

void idAnimator_Shoot::PreSerializeInit(
    idGameTimeManager* gameTimeManager) {
  gametimeManager.gameTimeManager = gameTimeManager;
  serializeProps.createdThroughSerialization = true;

  idAnimatorParms_Base shootParms(
      serializeProps.animStack, "fire", idMD6Blend::BOP_ADD_RIGHT,
      idMD6Blend::ORIGINBLEND_BRANCH, MD6_WEIGHTGROUP_ALL,
      MD6_WEIGHTGROUP_ALL, 1.0f);
  Init(gameTimeManager, shootParms);
  serializeProps.animStack->ClearAnimatorFlag(
      this, idAnimStack::ANIMATORFLAG_SERIALIZE);
}

bool idAnimator_Shoot::InternalPostInit(const idAnimatorParms_Base& parms) {
  forceFree = true;
  if (parms.animStack == nullptr) {
    return false;
  }
  parms.animStack->ClearAnimatorFlag(
      this, idAnimStack::ANIMATORFLAG_SERIALIZE);
  return true;
}
