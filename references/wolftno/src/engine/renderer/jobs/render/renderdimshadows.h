#pragma once

#include <cstddef>
#include <cstdint>

class idDeclRenderParm;
class idDeclRenderProg;
class idImage;
class idTriangles;
class idBounds;
class idRenderDestination;
struct idRenderMatrix;
struct idRenderModelCommitted;
class idRenderView;
class idVec3;
struct occlusionState_t;
struct renderSettings_t;

struct renderDimShadowsParms_t {
  std::int16_t* dimShadowModelSlaves;
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idRenderModelCommitted* const* committedRenderModels;
  int numCommittedRenderModels;
  std::uint32_t alignmentBeforeDimShadowModels;
  const idRenderModelCommitted* dimShadowModels;
  const idTriangles* extrudeBox;
  idRenderDestination* renderDestDimShadow;
  const idRenderDestination* renderDestDefault;
  const idImage* imgViewColor;
  const idDeclRenderParm* rpViewColorMap;
  const idDeclRenderParm* rpDimShadowDepthMap;
  const idDeclRenderParm* rpDimShadowFade;
  const idDeclRenderParm* rpDimShadowExtrude;
  const idDeclRenderParm* rpDimShadowClipPlane;
  const idDeclRenderParm* rpWindowPosToDimShadowBlurS;
  const idDeclRenderParm* rpWindowPosToDimShadowBlurT;
  const idDeclRenderParm* rpWindowPosToDimShadowBlurR;
  const idDeclRenderParm* rpWindowPosToDimShadowBlurQ;
  const idDeclRenderParm* rpVertexMorphScale;
  const idDeclRenderParm* rpDimShadowBufferResolution;
  const idDeclRenderProg* progDimShadowCreate;
  const idDeclRenderProg* progDimShadowCreateWound;
  const idDeclRenderProg* progDimShadowCreateAlpha;
  const idDeclRenderProg* progDimShadowCreateSecondary;
  const idDeclRenderProg* progDimShadowDraw;
  const idDeclRenderProg* progDimShadowDrawMidQuality;
  const idDeclRenderProg* progDimShadowDrawHighQuality;
  const idDeclRenderProg* progDimShadowStencil;
  const idDeclRenderProg* progDimShadowDebugDraw;
  occlusionState_t* occlusionState;
  int* numDimShadows;
  std::uint8_t* modelDimShadowState;
};

void CalculateLightMatrix(idRenderMatrix& lightMatrix,
                          const idVec3& primeLightDirection,
                          const idRenderMatrix& modelMatrix,
                          const idBounds& referenceBounds);
float DistanceToBox(const idBounds& box, const idVec3& point);
float ComputeShadowLODDistance(const idBounds& bounds,
                               const idVec3& camera,
                               const idVec3& lightDirection);
void RenderDimShadows(const renderDimShadowsParms_t* parameters);

static_assert(offsetof(renderDimShadowsParms_t, dimShadowModels) == 0x28,
              "dim-shadow model list offset drift");
static_assert(offsetof(renderDimShadowsParms_t, renderDestDimShadow) == 0x38,
              "dim-shadow destination offset drift");
static_assert(offsetof(renderDimShadowsParms_t, progDimShadowCreate) == 0xA8,
              "dim-shadow creation-program offset drift");
static_assert(offsetof(renderDimShadowsParms_t,
                       progDimShadowDrawMidQuality) == 0xD0,
              "dim-shadow mid-quality program offset drift");
static_assert(offsetof(renderDimShadowsParms_t, occlusionState) == 0xF0,
              "dim-shadow occlusion-state offset drift");
static_assert(offsetof(renderDimShadowsParms_t, modelDimShadowState) == 0x100,
              "dim-shadow state offset drift");
static_assert(sizeof(renderDimShadowsParms_t) == 0x108,
              "dim-shadow parameters ABI drift");
