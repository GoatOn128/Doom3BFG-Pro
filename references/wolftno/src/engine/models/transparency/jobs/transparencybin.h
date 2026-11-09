#pragma once

#include "engine/models/static/jobs/statictransparencygen.h"
#include "shared/idlib/math/plane.h"

#include <cstddef>
#include <cstdint>

struct polygon_t {
  std::uint16_t points[15];
  std::uint16_t numPoints;
};

struct bin_t {
  std::uint16_t indices[126];
  int numIndices;
};

struct node_t {
  idPlane plane;
  std::int16_t children[2];
};

struct deferredTransparencyBinParms_t {
  const idTransparencyVert* verts;
  const std::uint16_t* indices;
  int numVerts;
  int numIndices;
  idVec3 viewOrigin;
  idMat3 viewAxis;
  float fovX;
  float fovY;
  bin_t* bins;
  int numBins;
  node_t* nodes;
  int numNodes;
  idTransparencyVert* tempBinVerts;
  int* vertexRemap;
  idTransparencyVert* binVerts;
  int numBinVerts;
  int maxBinVerts;
  std::uint16_t* binIndices;
  int numBinIndices;
  int maxBinIndices;
};

int SplitPolygon(const polygon_t& polygon, const idPlane& plane,
                 float epsilon, idTransparencyVert* binVerts,
                 int& numBinVerts, polygon_t& front, polygon_t& back);
void BinPolygon_r(deferredTransparencyBinParms_t* parms, int nodeNum,
                  const polygon_t& polygon);
int BuildBinTree_r(deferredTransparencyBinParms_t* parms,
                   const idVec3* screenVerts, int offsetX, int partitionsX,
                   int offsetY, int partitionsY);
void BinQuads(deferredTransparencyBinParms_t* parms);

static_assert(sizeof(polygon_t) == 32);
static_assert(offsetof(polygon_t, numPoints) == 30);
static_assert(sizeof(bin_t) == 256);
static_assert(offsetof(bin_t, numIndices) == 252);
static_assert(sizeof(node_t) == 20);
static_assert(offsetof(node_t, children) == 16);
static_assert(sizeof(deferredTransparencyBinParms_t) == 160);
static_assert(offsetof(deferredTransparencyBinParms_t, viewOrigin) == 24);
static_assert(offsetof(deferredTransparencyBinParms_t, bins) == 80);
static_assert(offsetof(deferredTransparencyBinParms_t, nodes) == 96);
static_assert(offsetof(deferredTransparencyBinParms_t, tempBinVerts) == 112);
static_assert(offsetof(deferredTransparencyBinParms_t, binIndices) == 144);
