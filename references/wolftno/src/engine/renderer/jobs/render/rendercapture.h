#pragma once

#include "shared/idlib/containers/heaparray.h"

#include <cstddef>

enum renderCapture_t : int {
  CAPTURE_VIEW_COLOR,
  CAPTURE_VIEW_DEPTH,
  CAPTURE_VIEW_NORMAL,
  CAPTURE_FEEDBACK,
  CAPTURE_GLARE_MAP,
  CAPTURE_GUI_IMAGE,
};

class idDeclRenderParm;
class idDeclRenderProg;
class idImage;
class idPixelPackBuffer;
class idRenderDestination;
class idTextureSampler;
class idTriangles;
struct renderSettings_t;
struct captureParms_t {
  renderCapture_t capture;
  const renderSettings_t* settings;
  int renderWidth;
  int renderHeight;
  int windowWidth;
  int windowHeight;
  bool forceFullVirtualTextureLoad;
  int viewIndex;
  const idRenderDestination* renderDestDefault;
  const idImage* imgViewDepth;
  const idRenderDestination* renderDestViewColor;
  const idRenderDestination* renderDestViewDepth;
  const idRenderDestination* renderDestViewNormal;
  idRenderDestination* renderDestFeedback;
  idHeapArray<unsigned char>* feedbackBuffer;
  idPixelPackBuffer* feedbackBufferObject;
  const idDeclRenderParm* rpFeedbackColor;
  const idRenderDestination* renderDestGui;
  bool createMipMaps;
  const idRenderDestination* renderDestMip;
  const idRenderDestination* renderDestMip3;
  const idDeclRenderParm* rpGlareMap;
  const idDeclRenderProg* progGlareScale;
  const idDeclRenderProg* progOverlappedDownSample;
  const idDeclRenderProg* progFeedbackDownSample;
  const idDeclRenderProg* progCaptureDepth;
  const idDeclRenderProg* progCopyToMip01;
  const idDeclRenderProg* progCopyToMip012;
  const idDeclRenderParm* rpViewColorMap;
  const idDeclRenderParm* rpMipMapSampler;
  const idTextureSampler* mipMapSamplers[16];
  const idDeclRenderParm* rpTargetDepth;
  const idDeclRenderParm* rpMipGenSrc;
  const idDeclRenderParm* rpMipGenDst0;
  const idDeclRenderParm* rpMipGenDst1;
  const idDeclRenderParm* rpMipGenDst2;
  const idTriangles* unitSquareTris;
  const idImage* imgBlack;
};

void CaptureViewColor(const captureParms_t* parameters);
void CaptureViewDepth(const captureParms_t* parameters);
void CaptureFeedback(const captureParms_t* parameters);
void CaptureGlareMap(const captureParms_t* parameters);
void CaptureGuiImage(const captureParms_t* parameters);
void CreateColorMips(const captureParms_t* parameters,
                     const idRenderDestination* texture);
void RenderCapture(captureParms_t* parameters);

static_assert(offsetof(captureParms_t, progCopyToMip01) == 0xB8,
              "capture copy-to-mip program offset drift");
static_assert(offsetof(captureParms_t, rpViewColorMap) == 0xC8,
              "capture view-color parameter offset drift");
static_assert(offsetof(captureParms_t, rpTargetDepth) == 0x158,
              "capture target-depth parameter offset drift");
static_assert(offsetof(captureParms_t, rpMipGenSrc) == 0x160,
              "capture mip-generation source offset drift");
static_assert(offsetof(captureParms_t, unitSquareTris) == 0x180,
              "capture unit-square offset drift");
static_assert(sizeof(captureParms_t) == 0x190,
              "capture parameters ABI drift");
