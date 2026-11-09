#pragma once

#include "shared/idlib/math/vector.h"

#include <cstddef>

class idMaterial;

struct skinRemap_t {
  const idMaterial* mtr{};
  idVec2 texOffset{};
};

static_assert(sizeof(skinRemap_t) == 16, "skinRemap_t ABI drift");
static_assert(offsetof(skinRemap_t, texOffset) == 8,
              "skinRemap_t texture-offset ABI drift");
