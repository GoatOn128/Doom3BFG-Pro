#pragma once

#include "shared/idlib/math/vector.h"

#include <cstdint>

enum textureFilter_t : int {
  TF_LINEAR,
  TF_NEAREST,
  TF_NEAREST_MIPMAP_NEAREST,
  TF_LINEAR_MIPMAP_NEAREST,
  TF_TRILINEAR,
  TF_DEFAULT
};

enum textureRepeat_t : int {
  TR_REPEAT,
  TR_CLAMP,
  TR_CLAMP_S,
  TR_CLAMP_T,
  TR_CLAMP_TO_BORDER,
  TR_MIRROR
};

struct samplerOptions_t {
  textureFilter_t filter{};
  textureRepeat_t repeat{};
  idVec4 border{};
  float lodBias{};
  float lodMinClamp{};
  float lodMaxClamp{};
  float aniso{};
  bool forceBias{};
  bool depthCompareMode{};
  std::uint16_t structPad{};
};

static_assert(sizeof(samplerOptions_t) == 44,
              "sampler-options ABI drift");
