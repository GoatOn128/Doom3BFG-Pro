#pragma once

#include "engine/renderer/render.h"

#include <cstddef>

class idDeclRenderParm;
class idDeclRenderProg;
class idImage;
class idRenderDestination;
class idTriangles;

struct renderScreenSpaceReflectionsParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idTriangles* unitSquareTris;
  const idDeclRenderProg* progScreenSpaceReflections;
  const idDeclRenderProg* progScreenSpaceReflectionsApply;
  const idDeclRenderParm* rpSSRMap;
  const idDeclRenderParm* rpSSRParm0;
  const idDeclRenderParm* rpSSRParm1;
  const idDeclRenderParm* rpViewColorMap;
  const idImage* imgViewColor;
  const idRenderDestination* renderDestReflections;
  const idRenderDestination* renderDestDefault;
  float xScale;
  float yScale;
};

void RenderScreenSpaceReflections(
    const renderScreenSpaceReflectionsParms_t* parameters, int quality);

static_assert(offsetof(renderScreenSpaceReflectionsParms_t,
                       progScreenSpaceReflections) == 0x18,
              "screen-space-reflections program offset drift");
static_assert(offsetof(renderScreenSpaceReflectionsParms_t, rpSSRMap) == 0x28,
              "screen-space-reflections sampler parameter offset drift");
static_assert(offsetof(renderScreenSpaceReflectionsParms_t,
                       renderDestReflections) == 0x50,
              "screen-space-reflections destination offset drift");
static_assert(offsetof(renderScreenSpaceReflectionsParms_t, xScale) == 0x60,
              "screen-space-reflections scale offset drift");
static_assert(sizeof(renderScreenSpaceReflectionsParms_t) == 0x68,
              "screen-space-reflections parameters ABI drift");
