#pragma once

#include "shared/idlib/containers/list.h"

#include <cstdint>

void StripIndexesToTriangleIndexes(const std::uint16_t* stripIndexes,
                                   int numStripIndexes,
                                   idList<std::uint16_t, 5>& triIndexes);
inline void StripIndexesToTriangleIndexes(
    const std::uint16_t* stripIndexes, int numStripIndexes,
    idList<std::uint16_t, 5>* triIndexes) {
  StripIndexesToTriangleIndexes(stripIndexes, numStripIndexes, *triIndexes);
}
void InOrderTriangleStripper(const std::uint16_t* triIndexes,
                             int numTriIndexes,
                             idList<std::uint16_t, 5>* stripIndexes);
void TriangleStripper(const std::uint16_t* indexes, unsigned int numIndexes,
                      idList<std::uint16_t, 5>* stripIndexes);
