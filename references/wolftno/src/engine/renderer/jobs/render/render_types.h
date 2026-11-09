#pragma once

#include <cstddef>
#include <cstdint>

class idDeclRenderProg;
class idMaterial;
struct idRenderModelCommitted;
class idRenderModelSurface;

struct drawSurf_t {
  float sort;
  bool isWorld;
  bool skipDetailTriangles;
  std::uint8_t alignmentPadding[2];
  std::uint64_t depthKey;
  const idRenderModelCommitted* model;
  const idRenderModelSurface* modelSurface;
  const idMaterial* material;
  const idDeclRenderProg* prog;
};

struct renderPassDepthModels_t {
  int firstModel;
  int lastModel;
};

struct nodeWorldArea_t {
  std::int16_t areaNum;
  std::int16_t nodeOcclusionTestIndex;
};

struct viewWorldArea_t {
  int areaNum;
  std::int16_t numSurfs;
  bool skipDetailTriangles;
  std::uint8_t alignmentPadding;
};

struct renderPassSurfaces_t {
  int firstSurface;
  int lastSurface;
};

static_assert(offsetof(drawSurf_t, depthKey) == 0x08,
              "draw-surface depth key offset drift");
static_assert(offsetof(drawSurf_t, model) == 0x10,
              "draw-surface model offset drift");
static_assert(offsetof(drawSurf_t, prog) == 0x28,
              "draw-surface program offset drift");
static_assert(sizeof(drawSurf_t) == 0x30, "draw-surface ABI drift");
static_assert(sizeof(renderPassSurfaces_t) == 8,
              "render-pass surface range ABI drift");
static_assert(sizeof(nodeWorldArea_t) == 4,
              "node-world-area ABI drift");
static_assert(sizeof(viewWorldArea_t) == 8,
              "view-world-area ABI drift");
