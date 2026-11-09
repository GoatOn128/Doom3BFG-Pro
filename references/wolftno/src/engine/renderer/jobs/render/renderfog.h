#pragma once

#include "engine/renderer/render.h"

class idDeclRenderProg;
class idTriangles;

struct renderFogParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idDeclRenderProg* progGlobalFog;
  const idTriangles* unitSquareTris;
};

void RenderFog(const renderFogParms_t* parameters);

static_assert(sizeof(renderFogParms_t) == 0x20,
              "render-fog parameters ABI drift");

