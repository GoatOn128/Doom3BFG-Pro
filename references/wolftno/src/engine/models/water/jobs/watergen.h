#pragma once

#include "shared/idlib/geometry/drawvert.h"
#include "shared/idlib/math/mathlib.h"

#include <cstddef>
#include <cstdint>

enum waterDisturbancType_t : std::int32_t {
  WATER_DISTURBANCE_CONSTANT = 0,
  WATER_DISTURBANCE_DROP = 1,
  WATER_DISTURBANCE_WAVE = 2,
  WATER_DISTURBANCE_GRID_DROP = 3,
};

struct waterDisturbanceParms_t {
  waterDisturbancType_t type;
  std::int32_t gridX;
  std::int32_t gridY;
  idVec2 loc;
  idVec4 parms;
  float radius;
  std::uint32_t pad[2];
};

struct deferredWaterBlockGenParms_t {
  float* currBlock;
  const float* currBlockL;
  const float* currBlockR;
  const float* currBlockU;
  const float* currBlockD;
  std::int16_t blockLocX;
  std::int16_t blockLocY;
  std::uint32_t pad44;
  const std::uint32_t* simulationParms;
  const std::uint8_t* vertGradient;
  const waterDisturbanceParms_t* disturbances;
  std::uint32_t numDisturbances;
  float spacing;
  float speed;
  float damping;
  float timeDelta;
  idVec3 waterOrg;
  std::uint16_t gridWidth;
  std::uint16_t gridHeight;
  std::uint32_t pad108;
  idDrawVert* verts;
  float* prevBlock;
};

void BlockToGridPos(int blockLocX, int blockLocY, int blockX, int blockY,
                    int* gridX, int* gridY);
void GridPosToBlockLoc(int gridX, int gridY, int numBlocksX,
                       int numBlocksY, int* blockPos);
void BlockPosToBlockLoc(int blockX, int blockY, int numBlocksX,
                        int* offset);
void ApplyDisturbances(const deferredWaterBlockGenParms_t* parms);
void GenWaterSurface(const deferredWaterBlockGenParms_t* parms,
                     const float* prevHM);

static_assert(sizeof(waterDisturbanceParms_t) == 48);
static_assert(offsetof(waterDisturbanceParms_t, loc) == 12);
static_assert(offsetof(waterDisturbanceParms_t, parms) == 20);
static_assert(offsetof(waterDisturbanceParms_t, radius) == 36);
static_assert(sizeof(deferredWaterBlockGenParms_t) == 128);
static_assert(offsetof(deferredWaterBlockGenParms_t, blockLocX) == 40);
static_assert(offsetof(deferredWaterBlockGenParms_t, simulationParms) == 48);
static_assert(offsetof(deferredWaterBlockGenParms_t, disturbances) == 64);
static_assert(offsetof(deferredWaterBlockGenParms_t, waterOrg) == 92);
static_assert(offsetof(deferredWaterBlockGenParms_t, verts) == 112);
static_assert(offsetof(deferredWaterBlockGenParms_t, prevBlock) == 120);

