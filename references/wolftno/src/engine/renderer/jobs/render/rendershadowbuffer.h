#pragma once

#include "shared/idlib/geometry/rendermatrix.h"

#include <cstddef>
#include <cstdint>

class idDeclRenderParm;
class idRenderView;
class idVec3;
class idBounds;
class idRenderDestination;
struct idRenderLightParms;
struct idRenderLightCommitted;
class idRenderModelSurface;
struct idRenderModelCommitted;
struct renderSettings_t;

struct shadowBufferOptions_t {
  int shadowBufferSize;
  int occluderFacing;
  int singleSide;
  float sliceStep;
  float polyOfsFactor;
  float polyOfsUnits;
  float parallelPolyOfsFactor;
  float parallelPolyOfsUnits;
  bool skipShadows;
  bool skipShadowModelSort;
  bool skipShadowOccluders;
  bool skipShadowModelCPUCulling;
  bool skipShadowSurfaceCPUCulling;
  std::uint8_t alignmentTail[3];
};

struct renderShadowBufferParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const idBounds* worldAreaSubspaceBounds;
  const idBounds* worldAreaGeometryBounds;
  const std::uint8_t* worldAreaSubspaceBoundsCulled;
  const idRenderModelCommitted* worldModel;
  const bool* binaryModelState;
  const idRenderModelCommitted* const* committedRenderModels;
  int numCommittedRenderModels;
  std::uint32_t alignmentBeforeRenderParm;
  const idDeclRenderParm* rpShadowBufferResolution;
  const idRenderDestination* renderDestDefault;
  const idRenderDestination* renderDestShadow;
  shadowBufferOptions_t options;
  std::uint8_t* modelConsideredAsOccluder;
};

struct lightShadow_t {
  idRenderMatrix shadowProjection;
  idRenderMatrix worldPosToShadowMap;
};

struct shadowCaster_t {
  const idRenderModelCommitted* model;
  const idBounds* bounds;
  const idRenderModelSurface* surfaces;
  int numSurfaces;
  std::uint32_t alignmentTail;
};

void GetPointLightViewMatrix(int side, idRenderMatrix& lightViewMatrix);
void GetSpotLightViewMatrix(const idVec3& lightTarget,
                            const idVec3& lightRight,
                            const idVec3& lightUp,
                            idRenderMatrix& lightViewMatrix);
void GetParallelLightViewMatrix(const idVec3& direction,
                                idRenderMatrix& lightViewMatrix);

void GetSpotShadowProjectionMatrix(float zNear, float zFar, float tanFovX,
                                   float tanFovY,
                                   idRenderMatrix& projectionMatrix);
void SetupWorldPosToShadowMap(const idRenderMatrix& shadowProjection,
                              float scale, float sBias, float tBias,
                              idRenderMatrix& worldPosToShadowMap);
int SetupShadowBufferMatrices(const idRenderLightParms& lightParms,
                              const idBounds& lightBounds,
                              const idVec3& viewOrigin,
                              const shadowBufferOptions_t& options,
                              idRenderMatrix* shadowViewProjection,
                              lightShadow_t& lightShadow);
void ShadowCasterQuickSort(shadowCaster_t* casters,
                           std::uint32_t* distances, std::int64_t count);

void RenderShadowBuffer(const renderShadowBufferParms_t* parameters,
                        const idRenderLightCommitted* light,
                        lightShadow_t* lightShadow);

static_assert(sizeof(shadowBufferOptions_t) == 0x28,
              "shadow-buffer options ABI drift");
static_assert(offsetof(renderShadowBufferParms_t, committedRenderModels) == 0x38,
              "shadow-buffer committed-model offset drift");
static_assert(offsetof(renderShadowBufferParms_t, rpShadowBufferResolution) == 0x48,
              "shadow-buffer render-parameter offset drift");
static_assert(offsetof(renderShadowBufferParms_t, options) == 0x60,
              "shadow-buffer options offset drift");
static_assert(offsetof(renderShadowBufferParms_t, modelConsideredAsOccluder) == 0x88,
              "shadow-buffer considered-model offset drift");
static_assert(sizeof(renderShadowBufferParms_t) == 0x90,
              "shadow-buffer parameters ABI drift");
static_assert(sizeof(lightShadow_t) == 0x80,
              "light-shadow transform ABI drift");
static_assert(sizeof(shadowCaster_t) == 0x20,
              "shadow-caster ABI drift");
