#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>

class idAnimator_AnimWeb;

class idAnimator_Proxy final : public idAnimator_Base {
public:
  idAnimator_Proxy();
  ~idAnimator_Proxy() override;

  void SetTree(idAnimator_AnimWeb* web);

  static void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }

protected:
  bool InternalInit(const idAnimatorParms_Base&) override { return true; }
  void InternalShutdown(idAnimStack*) override {}
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
  // Exposed for recovered layout/type-info parity.
  idMD6Branch mergeBranch;

  friend class AnimatorProxyValidationAccess;
};

static_assert(offsetof(idAnimator_Proxy, mergeBranch) == 0x50,
              "animator proxy merge-branch offset drift");
static_assert(sizeof(idAnimator_Proxy) == 0x88,
              "idAnimator_Proxy ABI drift");
