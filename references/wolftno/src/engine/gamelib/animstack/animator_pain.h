#pragma once

#include "engine/gamelib/animstack/animator_channel.h"
#include "shared/idlib/sys/sys_alloc.h"

class idAnimator_Pain : public idAnimator_Channel {
public:
  idAnimator_Pain();
  ~idAnimator_Pain() override;

  serializeType_t GetSerializeType() override { return STYPE_PAIN; }
  void PreSerializeInit(idGameTimeManager* gameTimeManager) override;
  bool InternalPostInit(const idAnimatorParms_Base& parms) override;

  static void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }
};

class idAnimator_Shoot : public idAnimator_Channel {
public:
  idAnimator_Shoot();
  ~idAnimator_Shoot() override;

  serializeType_t GetSerializeType() override { return STYPE_SHOOT; }
  void PreSerializeInit(idGameTimeManager* gameTimeManager) override;
  bool InternalPostInit(const idAnimatorParms_Base& parms) override;

  static void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }
};

static_assert(sizeof(idAnimator_Pain) == 0xB8,
              "idAnimator_Pain ABI drift");
static_assert(sizeof(idAnimator_Shoot) == 0xB8,
              "idAnimator_Shoot ABI drift");
