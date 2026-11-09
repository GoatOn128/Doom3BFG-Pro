#pragma once

#include "engine/renderer/render.h"

#include <cstddef>

class idDeclRenderParm;
class idDeclRenderProg;
class idImage;
class idRenderDestination;
class idTriangles;

struct renderHazeFlareParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idTriangles* unitSquareTris;
  const idDeclRenderProg* progHazeFlareBlurPass1;
  const idDeclRenderProg* progHazeFlareBlurPass2;
  const idDeclRenderProg* progHazeFlare;
  const idDeclRenderParm* rpHazeFlareMap;
  const idDeclRenderParm* rpMipMapSampler;
  const idDeclRenderParm* rpGlareMap;
  const idImage* imgViewColor;
  const idImage* imgGlare;
  const idImage* imgHazeFlare[2];
  const idRenderDestination* renderDestHazeFlare[2];
  const idRenderDestination* renderDestDefault;
};

void RenderHazeFlare(const renderHazeFlareParms_t* parameters);

static_assert(offsetof(renderHazeFlareParms_t, renderDestHazeFlare) == 0x68,
              "haze-flare intermediate destinations offset drift");
static_assert(sizeof(renderHazeFlareParms_t) == 0x80,
              "haze-flare parameters ABI drift");
