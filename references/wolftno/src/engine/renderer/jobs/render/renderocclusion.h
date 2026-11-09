#pragma once

#include <cstddef>
#include <cstdint>

class idDeclRenderProg;
class idTriangles;
class idBounds;
class idRenderDestination;
struct idRenderLightCommitted;
struct idRenderModelCommitted;
class idRenderView;
struct renderSettings_t;

struct alignas(8) occlusionState_t {
  std::int64_t nodeOcclusionQuery[64];
  std::int64_t worldAreaOcclusionQuery[8192];
  std::int64_t modelOcclusionQuery[8192];
  std::int64_t lightOcclusionQuery[8192];
  std::int64_t shadowOcclusionQuery[8192];
  int worldAreaVisibleFrameNumber[8192];
  int modelVisibleFrameNumber[8192];
  int modelVisibleFrameCount[8192];
  int worldAreaOcclusionQueryResult[8192];
  int modelOcclusionQueryResult[8192];
  int lightOcclusionQueryResult[8192];
  int shadowOcclusionQueryResult[8192];
  int renderFrameNumber;
};

struct renderOcclusionParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const int* occlusionTestNodes;
  int numOcclusionTestNodes;
  const int* occlusionTestWorldAreas;
  int numOcclusionTestWorldAreas;
  const idRenderModelCommitted** occlusionTestModels;
  int numOcclusionTestModels;
  const idRenderLightCommitted** occlusionTestLights;
  int numOcclusionTestLights;
  const int* areaNodeOcclusionIndex;
  const idBounds* areaNodeBounds;
  const idBounds* worldAreaGeometryBounds;
  int numWorldAreas;
  const idRenderDestination* renderDestDefault;
  const idDeclRenderProg* progDepthOnly;
  const idTriangles* unitCubeTris;
  const idTriangles* zeroOneCubeTris;
  occlusionState_t* occlusionState;
};

void RenderOcclusion(const renderOcclusionParms_t* parameters);

static_assert(offsetof(occlusionState_t, worldAreaOcclusionQuery) == 0x200,
              "world-area occlusion query offset drift");
static_assert(offsetof(occlusionState_t, modelOcclusionQuery) == 0x10200,
              "model occlusion query offset drift");
static_assert(offsetof(occlusionState_t, lightOcclusionQuery) == 0x20200,
              "light occlusion query offset drift");
static_assert(sizeof(occlusionState_t) == 0x78208,
              "occlusion state ABI drift");
static_assert(offsetof(renderOcclusionParms_t, occlusionTestWorldAreas) == 0x20,
              "occlusion world-area list offset drift");
static_assert(offsetof(renderOcclusionParms_t, occlusionTestModels) == 0x30,
              "occlusion model list offset drift");
static_assert(offsetof(renderOcclusionParms_t, occlusionTestLights) == 0x40,
              "occlusion light list offset drift");
static_assert(offsetof(renderOcclusionParms_t, renderDestDefault) == 0x70,
              "occlusion destination offset drift");
static_assert(offsetof(renderOcclusionParms_t, occlusionState) == 0x90,
              "occlusion state pointer offset drift");
static_assert(sizeof(renderOcclusionParms_t) == 0x98,
              "occlusion parameters ABI drift");
