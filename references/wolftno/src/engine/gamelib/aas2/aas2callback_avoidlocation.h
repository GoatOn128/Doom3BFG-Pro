#pragma once

#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/sys/sys_alloc.h"

#include <cstddef>

class idAAS2;

#if defined(_MSC_VER)
extern "C" void* idAAS2CallbackVftableAlias[];
#pragma comment(linker,                                                        \
                "/alternatename:idAAS2CallbackVftableAlias="                 \
                "??_7idAAS2Callback@@6B@")
#endif

struct idAAS2Obstacle {
  idBounds absBounds;
  idBounds expAbsBounds;
};

class idAAS2Callback {
public:
  virtual ~idAAS2Callback() {
#if defined(_MSC_VER)
    *reinterpret_cast<void***>(this) = idAAS2CallbackVftableAlias;
#endif
  }

  virtual bool PathValid(const idAAS2*, const idVec3&, const idVec3&) {
    return true;
  }
  virtual int AdditionalTravelTimeForPath(const idAAS2*, const idVec3&,
                                          const idVec3&) {
    return 0;
  }
  virtual bool AreaIsGoal(const idAAS2*, int, const idVec3&) = 0;

  static void operator delete(void* memory) noexcept {
#if defined(WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI)
    idMem::Singleton().Free(memory, ALIGN_16);
#else
    idMem::Singleton()->Free(memory, ALIGN_16);
#endif
  }
};

class idAAS2Callback_AvoidLocation : public idAAS2Callback {
public:
  idAAS2Callback_AvoidLocation();
  ~idAAS2Callback_AvoidLocation() override;

  bool PathValid(const idAAS2* aas, const idVec3& start,
                 const idVec3& end) override;
  int AdditionalTravelTimeForPath(const idAAS2* aas, const idVec3& start,
                                  const idVec3& end) override;

  idVec3 avoidLocation;
  float avoidDist;
  const idAAS2Obstacle* obstacles;
  int numObstacles;
};

static_assert(sizeof(idAAS2Callback) == 0x08,
              "idAAS2Callback ABI drift");
static_assert(sizeof(idAAS2Obstacle) == 0x30,
              "idAAS2Obstacle ABI drift");
static_assert(offsetof(idAAS2Callback_AvoidLocation, avoidLocation) == 0x08,
              "avoid-location vector offset drift");
static_assert(offsetof(idAAS2Callback_AvoidLocation, avoidDist) == 0x14,
              "avoid distance offset drift");
static_assert(offsetof(idAAS2Callback_AvoidLocation, obstacles) == 0x18,
              "avoid obstacles offset drift");
static_assert(offsetof(idAAS2Callback_AvoidLocation, numObstacles) == 0x20,
              "avoid obstacle-count offset drift");
static_assert(sizeof(idAAS2Callback_AvoidLocation) == 0x28,
              "idAAS2Callback_AvoidLocation ABI drift");
