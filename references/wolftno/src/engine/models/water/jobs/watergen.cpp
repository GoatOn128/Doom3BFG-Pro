#include "engine/models/water/jobs/watergen.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <immintrin.h>

namespace {

float Sin16ForWater(float angle) {
  if (angle < 0.0f || angle >= 6.2831855f)
    angle -= std::floor(angle * 0.15915494f) * 6.2831855f;
  if (angle >= 3.1415927f) {
    if (angle > 4.712389f)
      angle -= 6.2831855f;
    else
      angle = 3.1415927f - angle;
  } else if (angle > 1.5707964f) {
    angle = 3.1415927f - angle;
  }
  const float squared = angle * angle;
  return (((((0.0000027526f - squared * 0.0000000239f) * squared -
              0.000198409f) * squared + 0.0083333319f) * squared -
            0.16666667f) * squared + 1.0f) * angle;
}

std::uint8_t EncodeDirection(float value) {
  const int encoded = _mm_cvttss_si32(
      _mm_set_ss((value + 1.0f) * 127.5f));
  return static_cast<std::uint8_t>(std::clamp(
      encoded, 0, 255));
}

} // namespace

float idMath::Exp16(float f) {
  const float scaled = f * 1.442695f;
  std::int32_t scaledBits;
  std::memcpy(&scaledBits, &scaled, sizeof(scaledBits));
  const int sign = scaledBits >> 31;
  const int exponent =
      static_cast<int>((static_cast<std::uint32_t>(scaledBits) >> 23) &
                       0xffu) -
      127;
  const int mantissa =
      (scaledBits & 0x7fffff) | 0x800000;
  const int whole =
      sign ^ (~(exponent >> 31) & (mantissa >> (23 - exponent)));
  const std::uint32_t exponentBits =
      static_cast<std::uint32_t>(whole + 127) << 23;
  float powerOfTwo;
  std::memcpy(&powerOfTwo, &exponentBits, sizeof(powerOfTwo));
  float fraction = scaled - static_cast<float>(whole);
  if (fraction >= 0.5f) {
    powerOfTwo *= 1.4142135f;
    fraction -= 0.5f;
  }
  const float squared = fraction * fraction;
  const float numeratorTerm = squared + 20.818924f;
  const float fractionTerm = (squared * 0.057690073f + 7.2152891f) *
                             fraction;
  return (numeratorTerm + fractionTerm) * powerOfTwo /
         (numeratorTerm - fractionTerm);
}

void BlockToGridPos(int blockLocX, int blockLocY, int blockX, int blockY,
                    int* gridX, int* gridY) {
  *gridX = blockX + 16 * blockLocX;
  *gridY = blockY + 16 * blockLocY;
}

void GridPosToBlockLoc(int gridX, int gridY, int numBlocksX,
                       int numBlocksY, int* blockPos) {
  (void)numBlocksY;
  *blockPos = gridX % 16 +
              16 * (gridY % 16 +
                    16 * (gridX / 16 + numBlocksX * (gridY / 16)));
}

void BlockPosToBlockLoc(int blockX, int blockY, int numBlocksX,
                        int* offset) {
  *offset = (blockX + numBlocksX * blockY) << 8;
}

void ApplyDisturbances(const deferredWaterBlockGenParms_t* parms) {
  const float spacing = parms->spacing;
  const float inverseSpacing = 1.0f / spacing;
  for (int blockY = 0; blockY < 16; ++blockY) {
    for (int blockX = 0; blockX < 16; ++blockX) {
      const int index = blockX + blockY * 16;
      const int gridX = blockX + 16 * parms->blockLocX;
      const int gridY = blockY + 16 * parms->blockLocY;
      for (std::uint32_t disturbanceIndex = 0;
           disturbanceIndex < parms->numDisturbances; ++disturbanceIndex) {
        const waterDisturbanceParms_t& disturbance =
            parms->disturbances[disturbanceIndex];
        float amount = 0.0f;
        switch (disturbance.type) {
          case WATER_DISTURBANCE_CONSTANT:
            amount = disturbance.parms.x;
            break;
          case WATER_DISTURBANCE_DROP:
          case WATER_DISTURBANCE_GRID_DROP: {
            const int gridRadius = static_cast<int>(
                std::floor(disturbance.radius * inverseSpacing + 0.5f));
            if (gridY < disturbance.gridY - gridRadius ||
                gridY > disturbance.gridY + gridRadius ||
                gridX < disturbance.gridX - gridRadius ||
                gridX > disturbance.gridX + gridRadius) {
              continue;
            }
            const idVec2 location =
                disturbance.type == WATER_DISTURBANCE_GRID_DROP
                    ? idVec2{disturbance.gridX * spacing,
                             disturbance.gridY * spacing}
                    : disturbance.loc;
            const float dy = gridY * spacing - location.y;
            const float dx = gridX * spacing - location.x;
            const float distanceSquared = dy * dy + dx * dx;
            const float inside = disturbance.radius * disturbance.radius -
                                 distanceSquared;
            if (inside > 0.0f) {
              amount = disturbance.parms.x;
              if (disturbance.type != WATER_DISTURBANCE_GRID_DROP) {
                amount = (((1.0f / std::sqrt(inside)) * inside) /
                          disturbance.radius) *
                         amount;
              }
            }
            break;
          }
          case WATER_DISTURBANCE_WAVE: {
            const float dy = gridY * spacing - disturbance.loc.y;
            const float dx = gridX * spacing - disturbance.loc.x;
            const float distanceSquared = dy * dy + dx * dx;
            if (disturbance.radius * disturbance.radius - distanceSquared >
                0.0f) {
              const float distance = distanceSquared *
                                     (1.0f / std::sqrt(distanceSquared));
              const float remaining = disturbance.radius - distance;
              amount = idMath::Exp16(-distance * disturbance.parms.w -
                                     remaining * disturbance.parms.z) *
                       disturbance.parms.x *
                       Sin16ForWater(remaining * disturbance.parms.y);
            } else {
              continue;
            }
            break;
          }
          default:
            break;
        }
        const float simulationScale =
            static_cast<float>(parms->simulationParms[index] & 0xffu) *
            0.0039215689f;
        parms->currBlock[index] =
            simulationScale * (amount * parms->timeDelta) +
            parms->currBlock[index];
      }
    }
  }
}

void GenWaterSurface(const deferredWaterBlockGenParms_t* parms,
                     const float* prevHM) {
  if (parms->verts == nullptr)
    return;
  const float inverseSpacing = 1.0f / parms->spacing;
  const float inverseWidth =
      1.0f / ((static_cast<float>(parms->gridWidth) - 1.0f) *
              parms->spacing);
  const float inverseHeight =
      1.0f / ((static_cast<float>(parms->gridHeight) - 1.0f) *
              parms->spacing);
  const float damping = 1.0f - parms->timeDelta * parms->damping;
  const float wave =
      ((parms->timeDelta * parms->timeDelta) * 0.5f) *
      (parms->speed * parms->speed) *
      (1.0f / (parms->spacing * parms->spacing));

  for (int row = 0; row < 16; ++row) {
    idDrawVert localVerts[16]{};
    for (int column = 0; column < 16; ++column) {
      const int index = row * 16 + column;
      const float down = row == 15 ? parms->currBlockD[column]
                                   : parms->currBlock[index + 16];
      const float up = row == 0 ? parms->currBlockU[240 + column]
                                : parms->currBlock[index - 16];
      const float right = column == 15 ? parms->currBlockR[row * 16]
                                       : parms->currBlock[index + 1];
      const float left = column == 0 ? parms->currBlockL[row * 16 + 15]
                                     : parms->currBlock[index - 1];
      float height = (((up + down + right + left -
                        parms->currBlock[index] * 4.0f) * wave) +
                      (parms->currBlock[index] * 1.99f -
                       prevHM[index] * 0.99000001f)) * damping;
      if (std::fabs(height) < idMath::FLT_SMALLEST_NON_DENORMAL)
        height = 0.0f;
      const std::uint32_t simulation = parms->simulationParms[index];
      const float simulationScale =
          static_cast<float>(simulation & 0xffu) * 0.0039215689f;
      const float slopeY = (down - up) * inverseSpacing * 0.5f *
                           simulationScale;
      const float slopeX = (right - left) * inverseSpacing * 0.5f *
                           simulationScale;
      height = simulationScale * height;
      parms->prevBlock[index] = height;

      idDrawVert& vertex = localVerts[column];
      const float gridX = (column + 16 * parms->blockLocX) * parms->spacing;
      const float gridY = (row + 16 * parms->blockLocY) * parms->spacing;
      vertex.xyz = {gridX + parms->waterOrg.x,
                    gridY + parms->waterOrg.y,
                    height + parms->waterOrg.z};
      vertex.st = {gridX * inverseWidth, gridY * inverseHeight};
      const float inverseNormalLength =
          1.0f / std::sqrt(slopeX * slopeX + slopeY * slopeY + 1.0f);
      vertex.normal[0] = EncodeDirection(inverseNormalLength * slopeX);
      vertex.normal[1] = EncodeDirection(inverseNormalLength * slopeY);
      vertex.normal[2] = EncodeDirection(inverseNormalLength);
      vertex.normal[3] = 0;
      const float inverseTangentLength =
          1.0f / std::sqrt(slopeY * slopeY + 1.0f);
      vertex.tangent[0] = EncodeDirection(inverseTangentLength);
      vertex.tangent[1] = EncodeDirection(0.0f);
      vertex.tangent[2] =
          EncodeDirection(inverseTangentLength * slopeY);
      vertex.tangent[3] = 255;
      vertex.color[0] = parms->vertGradient[index];
      vertex.color[1] = static_cast<std::uint8_t>(simulation >> 8);
      vertex.color[2] = static_cast<std::uint8_t>(simulation >> 16);
      vertex.color[3] = static_cast<std::uint8_t>(simulation >> 24);
    }
    std::memcpy(parms->verts + row * 16, localVerts, sizeof(localVerts));
  }
}
