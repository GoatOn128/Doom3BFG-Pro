#pragma once

#include "shared/idlib/geometry/drawvert.h"

#include <cstddef>
#include <cstdint>

struct subdivPoint_t {
  idVec3 position;
  idVec3 tangent;
  float texcoord;
};

struct cableGenParms_t {
  const idVec4* points;
  const subdivPoint_t* subdivPoints;
  int numPoints;
  int diameterSubdivision;
  int lengthMaxSubdivision;
  float diameter;
  int maxVertices;
  int maxIndices;
  idDrawVert* vertices;
  std::uint16_t* indices;
  int* numVertices;
  int* numIndices;
  int* done;
};

void GenCable(const cableGenParms_t* parms);

static_assert(sizeof(subdivPoint_t) == 28);
static_assert(offsetof(subdivPoint_t, tangent) == 12);
static_assert(offsetof(subdivPoint_t, texcoord) == 24);
static_assert(sizeof(cableGenParms_t) == 80);
static_assert(offsetof(cableGenParms_t, numPoints) == 16);
static_assert(offsetof(cableGenParms_t, diameter) == 28);
static_assert(offsetof(cableGenParms_t, vertices) == 40);
static_assert(offsetof(cableGenParms_t, indices) == 48);
static_assert(offsetof(cableGenParms_t, numVertices) == 56);
static_assert(offsetof(cableGenParms_t, numIndices) == 64);
static_assert(offsetof(cableGenParms_t, done) == 72);
