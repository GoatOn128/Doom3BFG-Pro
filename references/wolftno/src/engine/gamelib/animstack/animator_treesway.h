#pragma once

#include "engine/gamelib/animstack/animator_base.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>

class idAnimator_TreeSway final : public idAnimator_Base {
public:
  idAnimator_TreeSway();
  ~idAnimator_TreeSway() override;

  void SetTranslateScale(const idVec3& scale) { translateScale = scale; }
  void SetRotationScale(const idVec3& scale) { rotationScale = scale; }
  void SetRotationOffset(const idQuat& offset) { rotationOffset = offset; }
  void SetTimeDifference(float difference) { timeDiff = difference; }
  void SetTimeScale(float scale) { timeScale = scale; }

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
  const idMD6Branch* InternalGetMergeBranch() const override {
    return nullptr;
  }
  idMD6Branch* InternalGetMergeBranch() override { return nullptr; }

private:
  float timeDiff;
  float timeScale;
  idQuat rotationOffset;
  idVec3 rotationScale;
  idVec3 translateScale;
};

static_assert(sizeof(idAnimator_TreeSway) == 0x80,
              "idAnimator_TreeSway ABI drift");
