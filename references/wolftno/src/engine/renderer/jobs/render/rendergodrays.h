#pragma once

#include "engine/renderer/render.h"

#include <cstddef>

class idDeclRenderParm;
class idDeclRenderProg;
class idImage;
class idRenderDestination;
struct idRenderModelCommitted;
class idTriangles;

struct renderGodRaysParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idDeclRenderProg* progCreateGodRays;
  const idDeclRenderProg* progDrawGodRays;
  const idRenderDestination* renderDestDefault;
  const idRenderDestination* renderDestOffScreen;
  const idImage* imgOffScreen;
  const idTriangles* unitSquareTris;
  const idRenderModelCommitted** viewGodrayModels;
  int numViewGodrayModels;
  const idDeclRenderParm* rpGodRayMap;
};

void RenderGodRays(const renderGodRaysParms_t* parameters);

static_assert(offsetof(renderGodRaysParms_t, numViewGodrayModels) == 0x48,
              "god-rays model count offset drift");
static_assert(offsetof(renderGodRaysParms_t, rpGodRayMap) == 0x50,
              "god-rays sampler parameter offset drift");
static_assert(sizeof(renderGodRaysParms_t) == 0x58,
              "god-rays parameters ABI drift");
