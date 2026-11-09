#pragma once

#include <cstddef>

class idDeclRenderParm;
class idDeclRenderProg;
class idImage;
class idRenderView;
class idTriangles;
class idRenderDestination;
struct renderSettings_t;

struct renderPostProcessParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idDeclRenderParm* rpViewColorMap;
  const idDeclRenderParm* rpGlareMap;
  const idDeclRenderParm* rpAdaptiveGlareMap;
  const idDeclRenderParm* rpAdaptiveGlareLuminance;
  const idDeclRenderParm* rpDistortionMap;
  const idDeclRenderProg* progPostProcess;
  const idImage* imgViewColor;
  const idImage* imgGlare;
  const idImage* imgAdaptiveGlare;
  const idImage* imgAdaptiveGlareLuminance;
  const idImage* imgBlack;
  const idImage* imgDistortion;
  const idTriangles* unitSquareTris;
  const idDeclRenderParm* rpDofMipScale;
  const idDeclRenderProg* progSlowDepthOfField;
  const idDeclRenderProg* progOverlappedDownSampleMaxAlpha;
  const idDeclRenderParm* rpGlobalBlur;
  const idDeclRenderParm* rpDofOffset;
  const idDeclRenderProg* progCalcDepthOfFieldValues;
  const idDeclRenderProg* progCalcDepthOfFieldMaxKernel;
  const idDeclRenderProg* progDepthFilterPass;
  const idDeclRenderProg* progDepthFilterDoublePass;
  const idDeclRenderParm* rpDepthFilterParams;
  const idRenderDestination* renderDestViewColor;
  const idRenderDestination* renderDestViewPostScratch;
  const idRenderDestination* renderDestDefault;
  const idImage* imgViewPostScratch;
  const idRenderDestination* renderDestMip;
  const idDeclRenderParm* rpMipMapSampler;
  const idDeclRenderProg* progOverlappedDownSample;
};

void RegenerateColorMips(const renderPostProcessParms_t* parameters,
                         const idRenderDestination* texture, bool maxAlpha);
void RenderPostProcess_SlowDepthBlur(
    const renderPostProcessParms_t* parameters);
void RenderPostProcess_SeparatedDepthBlur(
    const renderPostProcessParms_t* parameters, bool boxBlur,
    bool sampleMipMaps);
void RenderPostProcess(const renderPostProcessParms_t* parameters);

static_assert(offsetof(renderPostProcessParms_t, progPostProcess) == 0x38,
              "post-process program offset drift");
static_assert(offsetof(renderPostProcessParms_t, unitSquareTris) == 0x70,
              "post-process geometry offset drift");
static_assert(offsetof(renderPostProcessParms_t, rpDepthFilterParams) == 0xC0,
              "post-process filter-parameter offset drift");
static_assert(offsetof(renderPostProcessParms_t, renderDestViewColor) == 0xC8,
              "post-process color destination offset drift");
static_assert(offsetof(renderPostProcessParms_t, progOverlappedDownSample) ==
                  0xF8,
              "post-process downsample program offset drift");
static_assert(sizeof(renderPostProcessParms_t) == 0x100,
              "post-process parameters ABI drift");
