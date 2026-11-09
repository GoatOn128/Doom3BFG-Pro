#pragma once

#include "engine/renderer/render.h"

#include <cstddef>

class idDeclRenderParm;
class idDeclRenderProg;
class idImage;
class idRenderDestination;
class idTriangles;

struct renderDynamicColorCorrectionParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idDeclRenderParm* rpUserColorCorrection;
  const idDeclRenderParm* rpColor;
  const idRenderDestination* renderDestDynamicColorCorrection;
  const idImage* imgDynamicColorCorrection;
  const idImage* imgUserColorCorrection;
  const idTriangles* unitSquareTris;
  const idDeclRenderProg* progBlend;
  envBlend_t* envBlend;
  bool forceBlend;
  const idDeclRenderParm* rpGammaForBrightness;
  float gammaForBrightness;
};

void RenderDynamicColorCorrection(
    const renderDynamicColorCorrectionParms_t* parameters);

static_assert(offsetof(renderDynamicColorCorrectionParms_t, envBlend) == 0x48,
              "dynamic color-correction environment offset drift");
static_assert(offsetof(renderDynamicColorCorrectionParms_t, forceBlend) == 0x50,
              "dynamic color-correction force flag offset drift");
static_assert(sizeof(renderDynamicColorCorrectionParms_t) == 0x68,
              "dynamic color-correction parameters ABI drift");
