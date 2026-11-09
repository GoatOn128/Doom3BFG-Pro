#pragma once

#include "engine/renderer/jobs/render/render_types.h"

#include <cstddef>
#include <cstdint>

class idDeclRenderParm;
class idDeclRenderProg;
class idImage;
class idRenderModelSurface;
class idTriangles;
class idRenderDestination;
struct idRenderModelCommitted;
class idRenderView;
struct occlusionState_t;
struct renderSettings_t;

template <typename T> class idODSObject;

struct renderDepthParms_t {
  const renderSettings_t* settings;
  const idRenderView* renderView;
  const viewWorldArea_t* viewWorldAreas;
  int numViewWorldAreas;
  const idRenderModelSurface** viewWorldSurfaces;
  int numViewWorldSurfaces;
  const idRenderModelCommitted** viewModels;
  const renderPassDepthModels_t* renderViewModels;
  const bool* binaryModelState;
  const idRenderDestination* renderDestDepth;
  const idRenderDestination* renderDestDefault;
  const idTriangles* occlusionBaseTriangles;
  const idDeclRenderProg* progOccluderDepthOnly;
  const idDeclRenderProg* progPreZ;
  const idDeclRenderProg* progPreZAlpha;
  const idDeclRenderProg* progPreZSecondary;
  const idDeclRenderProg* progPreZWound;
  const idImage* imgBlack;
  const idDeclRenderParm* rpPostSpecularEnabled;
  std::uint32_t extraClears : 30;
  std::uint32_t skipDepthViewStencil : 1;
  std::uint32_t skipFirstPersonModels : 1;
  int renderExtraWorldPassDebug;
  float splitPlaneDistance;
  std::uint32_t alignmentBeforeOcclusionState;
  occlusionState_t* occlusionState;
  drawSurf_t* drawSurfs;
  int* numDrawSurfs;
};

std::uint64_t GetSurfaceSortKey(const renderDepthParms_t* parameters,
                                drawSurf_t* drawSurface, float* depthKeyMin,
                                std::uint16_t* surfaceSequence);
void DrawDepthSurface(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* model,
    const idODSObject<idRenderModelSurface>* surface, bool cullSurface,
    bool skipDetailTriangles, bool skipDepthViewStencil,
    bool skipPerforatedCullTest);
const idDeclRenderProg* BuildDrawSurf(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* model,
    const idODSObject<idRenderModelSurface>* surface, bool cullSurface,
    bool testOcclusion, bool skipDetailTriangles, float* sortOffset,
    float* depthKeyMin, std::uint16_t* surfaceSequence,
    bool skipSubSurfaceScattering, bool skipBuildDrawSurf);
void DrawOpaqueSurface(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* model,
    const idODSObject<idRenderModelSurface>* surface, bool cullSurface,
    bool testOcclusion, bool skipDetailTriangles, float* sortOffset,
    float* depthKeyMin, std::uint16_t* surfaceSequence,
    bool skipDepthViewStencil, bool skipSubSurfaceScattering,
    bool skipBuildDrawSurface);
bool GetSkipSubSurfaceScattering(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* model);
void BuildModelDrawsurfs(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* model, float* sortOffset,
    bool testOcclusion);
void DrawModelOpaqueSurfaces(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* model, float* sortOffset,
    bool testOcclusion, std::uint16_t skipBuildDrawSurf);
void DrawModelDepthSurfaces(
    const renderDepthParms_t* parameters,
    const idODSObject<idRenderModelCommitted>* model,
    bool skipPerforatedCullTest);
void RenderDepth(const renderDepthParms_t* parameters);
void RenderDepthFirstPerson(const renderDepthParms_t* parameters);
void RenderOpaque(const renderDepthParms_t* parameters);
void RenderOpaqueFirstPerson(const renderDepthParms_t* parameters);
void RenderOpaqueOcclusionTest(const renderDepthParms_t* parameters);

static_assert(sizeof(renderPassDepthModels_t) == 0x08,
              "render-depth model range ABI drift");
static_assert(offsetof(renderDepthParms_t, renderViewModels) == 0x38,
              "render-depth model ranges offset drift");
static_assert(offsetof(renderDepthParms_t, binaryModelState) == 0x40,
              "render-depth binary state offset drift");
static_assert(offsetof(renderDepthParms_t, rpPostSpecularEnabled) == 0x90,
              "render-depth post-specular parameter offset drift");
static_assert(offsetof(renderDepthParms_t, renderExtraWorldPassDebug) == 0x9C,
              "render-depth debug-pass offset drift");
static_assert(offsetof(renderDepthParms_t, splitPlaneDistance) == 0xA0,
              "render-depth split-plane offset drift");
static_assert(offsetof(renderDepthParms_t, occlusionState) == 0xA8,
              "render-depth occlusion-state offset drift");
static_assert(offsetof(renderDepthParms_t, numDrawSurfs) == 0xB8,
              "render-depth draw-surface count offset drift");
static_assert(sizeof(renderDepthParms_t) == 0xC0,
              "render-depth parameters ABI drift");
