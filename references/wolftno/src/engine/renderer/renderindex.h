#pragma once

struct idRenderIndex {
  unsigned int index;
};

static_assert(sizeof(idRenderIndex) == 4, "idRenderIndex ABI drift");
