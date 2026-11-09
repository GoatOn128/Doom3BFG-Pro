#pragma once

#include "engine/models/water/jobs/oceanjob.h"

#include <cstddef>
#include <cstdint>

struct Ocean2TessParams {
  idVec4 m_TessBase;
  idVec4 m_TessU;
  idVec4 m_TessV;
  float m_Near;
  float m_Far;
  float m_NearWidth;
  float m_FarWidth;
  float m_NearPersp;
  float m_FarPersp;
};

struct idRenderModelOcean2_TileJobParams {
  COceanVertex* m_pVB;
  idVec2i m_VBDim;
  idVec2i m_SubRectOfs;
  idVec2i m_SubRectSize;
  Ocean2TessParams m_Tess;
  idRenderModelOcean_Timers m_Timers;
  void* m_pParams;
};

void EvalVertices(const idVec4& position, const idVec4& step,
                  COceanVertex* destination, unsigned int vertexCount);
void Ocean2GenJob(idRenderModelOcean2_TileJobParams* params);

static_assert(sizeof(COceanVertex) == 16);
static_assert(offsetof(COceanVertex, s0) == 12);
static_assert(sizeof(Ocean2TessParams) == 72);
static_assert(offsetof(Ocean2TessParams, m_Near) == 48);
static_assert(offsetof(Ocean2TessParams, m_NearPersp) == 64);
static_assert(sizeof(idRenderModelOcean_Timers) == 64);
static_assert(sizeof(idRenderModelOcean2_TileJobParams) == 176);
static_assert(offsetof(idRenderModelOcean2_TileJobParams, m_Tess) == 32);
static_assert(offsetof(idRenderModelOcean2_TileJobParams, m_Timers) == 104);
static_assert(offsetof(idRenderModelOcean2_TileJobParams, m_pParams) == 168);
