#pragma once

#include "engine/renderer/jobs/render/rendercapture.h"

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
struct renderSettings_t;

struct renderSelfShadowsParms_t {
  std::int16_t* selfShadowModelSlaves;
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idRenderModelCommitted* const* committedRenderModels;
  int numCommittedRenderModels;
  std::uint32_t alignmentBeforeDimShadowModels;
  const idRenderModelCommitted* dimShadowModels;
  const idTriangles* extrudeBox;
  idRenderDestination* renderDestSelfShadow;
  const idRenderDestination* renderDestDefault;
  captureParms_t captureParms;
  const idDeclRenderParm* rpLightDepthCompareMap;
  const idDeclRenderParm* rpShadowBufferResolution;
  const idDeclRenderParm* rpSelfShadowMap;
  const idDeclRenderParm* rpSelfShadowFade;
  const idDeclRenderParm* rpSelfShadowEnable;
  const idDeclRenderParm* rpModelPosToShadowMapS[4];
  const idDeclRenderParm* rpModelPosToShadowMapT[4];
  const idDeclRenderParm* rpModelPosToShadowMapR[4];
  const idDeclRenderParm* rpModelPosToShadowMapQ[4];
  const idDeclRenderParm* rpInvShadowProjX[4];
  const idDeclRenderParm* rpInvShadowProjY[4];
  const idDeclRenderParm* rpInvShadowProjZ[4];
  const idDeclRenderParm* rpInvShadowProjW[4];
  const idDeclRenderParm* rpShadowProjectionDepth;
  const idDeclRenderProg* progSelfShadowCreate;
  const idDeclRenderProg* progSelfShadowCreateAlpha;
  const idDeclRenderProg* progSelfShadowCreateWound;
  const idDeclRenderProg* progSelfShadowCreateSecondary;
  const idDeclRenderProg* progSelfShadowSample;
  const idDeclRenderProg* progSelfShadowSampleAlpha;
  const idDeclRenderProg* progSelfShadowSampleSecondary;
  const idDeclRenderProg* progSelfShadowSampleWound;
  const idImage* imgBlack;
  int* numSelfShadows;
};

void SetupShadowProjection(const idVec3& localLightOrigin,
                           const idBounds& localModelBounds,
                           idRenderMatrix& shadowProjection,
                           idRenderMatrix& localShadowProjection,
                           float& depth);
void RenderSelfShadows(const renderSelfShadowsParms_t* parameters);

static_assert(offsetof(renderSelfShadowsParms_t, dimShadowModels) == 0x28,
              "self-shadow model-list offset drift");
static_assert(offsetof(renderSelfShadowsParms_t, captureParms) == 0x48,
              "self-shadow capture offset drift");
static_assert(offsetof(renderSelfShadowsParms_t, rpLightDepthCompareMap) ==
                  0x1D8,
              "self-shadow depth-map parameter offset drift");
static_assert(offsetof(renderSelfShadowsParms_t, rpModelPosToShadowMapS) ==
                  0x200,
              "self-shadow world-map S offset drift");
static_assert(offsetof(renderSelfShadowsParms_t, rpInvShadowProjX) == 0x280,
              "self-shadow inverse-projection X offset drift");
static_assert(offsetof(renderSelfShadowsParms_t, progSelfShadowCreate) ==
                  0x308,
              "self-shadow creation-program offset drift");
static_assert(offsetof(renderSelfShadowsParms_t, progSelfShadowSample) ==
                  0x328,
              "self-shadow sample-program offset drift");
static_assert(offsetof(renderSelfShadowsParms_t, numSelfShadows) == 0x350,
              "self-shadow count offset drift");
static_assert(sizeof(renderSelfShadowsParms_t) == 0x358,
              "self-shadow parameters ABI drift");
