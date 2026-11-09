#pragma once

#include <cstdint>

unsigned int GetTriangleCost(std::uint16_t* l1Cache, std::uint16_t* l2Cache,
                             std::uint16_t* indexes);
void RenderTriangle(std::uint16_t*& l1Cache, std::uint16_t*& l2Cache,
                    std::uint16_t* indexes);
unsigned int GetBestRenderOrder_r(unsigned int inCost, unsigned int inBestCost,
                                  unsigned int* renderList, unsigned int renderListSize,
                                  unsigned int* triOrder, std::uint16_t* renderOrder,
                                  std::uint16_t* l1Cache, std::uint16_t* l2Cache,
                                  std::uint16_t* indexes);
void MeshOptimizer(std::uint16_t* indexes, unsigned int numIndexes,
                   std::uint16_t* outputIndexes, float k1, float k2, float k3);
