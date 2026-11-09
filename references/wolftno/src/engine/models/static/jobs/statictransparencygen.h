#pragma once

#include "engine/models/renderworld.h"
#include "shared/idlib/geometry/drawvert.h"

#include <cstddef>
#include <cstdint>

class idTransparencyVert {
public:
  idVec3 xyz;
  std::int16_t st[2];
  std::int16_t st1[2];
  std::uint8_t normal[4];
  std::uint8_t tangent[4];
  std::uint8_t color[4];
};

struct staticTransparencyGenParms_t {
  const idDrawVert* inputVerts;
  int numInputVerts;
  float fadeStartDist;
  float fadeMaxDist;
  idVec3 origin;
  idMat3 axis;
  idVec3 viewOrg;
  idVec3 viewDir;
  idVec4 transSortScaleBias;
  approximateLighting_t lighting;
  idTransparencyVert* verts;
  float* quadDepth;
  int* quadsUsed;
};

void StaticTransparencyGenJob(const staticTransparencyGenParms_t* parms);

static_assert(sizeof(idTransparencyVert) == 32);
static_assert(offsetof(idTransparencyVert, st) == 12);
static_assert(offsetof(idTransparencyVert, st1) == 16);
static_assert(offsetof(idTransparencyVert, normal) == 20);
static_assert(offsetof(idTransparencyVert, tangent) == 24);
static_assert(offsetof(idTransparencyVert, color) == 28);
static_assert(sizeof(staticTransparencyGenParms_t) == 360);
static_assert(offsetof(staticTransparencyGenParms_t, numInputVerts) == 8);
static_assert(offsetof(staticTransparencyGenParms_t, origin) == 20);
static_assert(offsetof(staticTransparencyGenParms_t, axis) == 32);
static_assert(offsetof(staticTransparencyGenParms_t, lighting) == 108);
static_assert(offsetof(staticTransparencyGenParms_t, verts) == 336);
static_assert(offsetof(staticTransparencyGenParms_t, quadDepth) == 344);
static_assert(offsetof(staticTransparencyGenParms_t, quadsUsed) == 352);
