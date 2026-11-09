#pragma once

#include "shared/idlib/geometry/rendermatrix.h"

#include <cstddef>
#include <cstdint>

struct shadowMapCPU_t {
  float ShadowSample(const idVec3& worldPoint) const;

  int width;
  std::uint16_t* map;
  idRenderMatrix mvp;
  float coordinateBias;
  float depthBias;
  bool useShadowMap;
};

static_assert(offsetof(shadowMapCPU_t, map) == 0x08,
              "global shadow map pointer offset drift");
static_assert(offsetof(shadowMapCPU_t, mvp) == 0x10,
              "global shadow MVP offset drift");
static_assert(offsetof(shadowMapCPU_t, coordinateBias) == 0x50,
              "global shadow coordinate-bias offset drift");
static_assert(offsetof(shadowMapCPU_t, depthBias) == 0x54,
              "global shadow depth-bias offset drift");
static_assert(offsetof(shadowMapCPU_t, useShadowMap) == 0x58,
              "global shadow enable offset drift");
static_assert(sizeof(shadowMapCPU_t) == 0x60,
              "global shadow map ABI drift");
