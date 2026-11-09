#pragma once

#include "engine/renderer/render.h"

#include <cstddef>

class idDeclRenderParm;
class idDeclRenderProg;
class idImage;
class idRenderDestination;
class idTriangles;

struct renderGlareParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  float xScale;
  float yScale;
  const idRenderDestination* renderDestDefault;
  const idRenderDestination* renderDestViewColor;
  const idRenderDestination* renderDestMip3;
  const idRenderDestination* renderDestMip3Alt;
  const idImage* imgBlack;
  const idDeclRenderParm* rpGlareMap;
  const idDeclRenderParm* rpGlareStep;
  const idDeclRenderProg* progGauss;
  const idDeclRenderProg* progGlareScale;
  const idTriangles* unitSquareTris;
};

struct renderAdapativeGlareParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  int frameNumber;
  float xScale;
  float yScale;
  float minThreshold;
  float maxThreshold;
  float avgBrightness;
  float decayRate;
  idVec4 color;
  float frameExposureMin;
  float frameExposureMax;
  float frameExposureDecay;
  float frameExposureAmount;
  float frameExposureSaturation;
  envBlend_t* envBlend;
  const idRenderDestination* renderDestDefault;
  const idRenderDestination* renderDestViewColor;
  const idRenderDestination* renderDestBloom;
  const idRenderDestination* renderDestAdaptiveGlare;
  const idRenderDestination* renderDestAdaptiveGlareAlt;
  const idRenderDestination* renderDestLuminosity;
  const idRenderDestination* renderDestLuminosityAlt;
  const idRenderDestination* renderDestFrameExposure;
  const idRenderDestination* renderDestFrameExposureAlt;
  const idDeclRenderParm* rpViewColorMap;
  const idDeclRenderParm* rpGlareMap;
  const idDeclRenderParm* rpAdaptiveGlareMap;
  const idDeclRenderParm* rpAdaptiveGlareMapAlt;
  const idDeclRenderParm* rpDownSample2x2Map;
  const idDeclRenderParm* rpLuminosityMap;
  const idDeclRenderParm* rpLuminosityMapAlt;
  const idDeclRenderParm* rpAdaptiveLuminance;
  const idDeclRenderParm* rpAdaptiveGlareBlurMap;
  const idDeclRenderParm* rpAdaptiveGlareBlurStep;
  const idDeclRenderParm* rpAdaptiveGlareParms;
  const idDeclRenderParm* rpAdaptiveGlareColor;
  const idDeclRenderParm* rpFrameExposureMap;
  const idDeclRenderParm* rpFrameExposureParms1;
  const idDeclRenderParm* rpFrameExposureParms2;
  const idDeclRenderProg* progLuminosity;
  const idDeclRenderProg* progBloomThreshold;
  const idDeclRenderProg* progAdaptiveGlareGauss;
  const idDeclRenderProg* progFrameExposure;
  const idImage* imgViewColor;
  const idImage* imgGlare;
  const idImage* imgLuminosity;
  const idImage* imgLuminosityAlt;
  const idImage* imgFrameExposure;
  const idImage* imgFrameExposureAlt;
  const idImage* imgBlack;
  const idTriangles* unitSquareTris;
};

void RenderGlare(const renderGlareParms_t* parameters);
void RenderAdaptiveGlare(const renderAdapativeGlareParms_t* parameters);

static_assert(offsetof(renderGlareParms_t, renderDestMip3) == 0x28,
              "glare mip destination offset drift");
static_assert(offsetof(renderGlareParms_t, progGauss) == 0x50,
              "glare program offset drift");
static_assert(sizeof(renderGlareParms_t) == 0x68,
              "glare parameters ABI drift");
static_assert(offsetof(renderAdapativeGlareParms_t, envBlend) == 0x50,
              "adaptive-glare environment offset drift");
static_assert(offsetof(renderAdapativeGlareParms_t, renderDestBloom) == 0x68,
              "adaptive-glare bloom destination offset drift");
static_assert(offsetof(renderAdapativeGlareParms_t, rpViewColorMap) == 0xA0,
              "adaptive-glare render parameter offset drift");
static_assert(offsetof(renderAdapativeGlareParms_t, progLuminosity) == 0x118,
              "adaptive-glare program offset drift");
static_assert(offsetof(renderAdapativeGlareParms_t, unitSquareTris) == 0x170,
              "adaptive-glare geometry offset drift");
static_assert(sizeof(renderAdapativeGlareParms_t) == 0x178,
              "adaptive-glare parameters ABI drift");
