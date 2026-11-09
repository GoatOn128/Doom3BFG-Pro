#pragma once

#include "engine/gamelib/aas2/aas2.h"

#include <cstddef>
#include <cstdint>

namespace aas2PVSABI {

template <typename Type> struct list_t {
  Type* list;
  int num;
  int size;
  std::int16_t granularity;
  std::uint8_t memTag;
  std::uint8_t listStatic;
  std::uint8_t padding[4];
};

struct area_t {
  int travelFlags;
  std::uint16_t flags;
  std::int16_t numEdges;
  int firstEdge;
  std::int16_t cluster;
  std::int16_t clusterAreaNum;
  int obstaclePVSOffset;
  std::int16_t reach;
  std::int16_t reverseReach;
  std::uint16_t firstChokePoint;
  std::uint16_t numChokePoints;
  std::uint16_t firstCover;
  std::uint16_t numCover;
  std::uint16_t firstTraversal;
  std::uint16_t numTraversals;
  std::uint16_t firstHintNode;
  std::uint16_t numHintNodes;
};

struct edge_t {
  int vertexNum[2];
  int flags;
};

constexpr std::size_t BOUNDING_BOX_OFFSET = 0xD0;
constexpr std::size_t TREES_OFFSET = 0x1A0;
constexpr std::size_t VERTICES_OFFSET = 0x1D0;
constexpr std::size_t EDGES_OFFSET = 0x1E8;
constexpr std::size_t EDGE_INDEX_OFFSET = 0x200;
constexpr std::size_t AREAS_OFFSET = 0x230;
constexpr std::size_t OBSTACLE_PVS_OFFSET = 0x2A8;

inline const std::uint8_t* Bytes(const idAAS2File* file) {
  return reinterpret_cast<const std::uint8_t*>(file);
}

template <typename Type>
inline const list_t<Type>& List(const idAAS2File* file,
                                std::size_t offset) {
  return *reinterpret_cast<const list_t<Type>*>(Bytes(file) + offset);
}

inline const idBounds& BoundingBox(const idAAS2File* file) {
  return *reinterpret_cast<const idBounds*>(Bytes(file) +
                                            BOUNDING_BOX_OFFSET);
}

} // namespace aas2PVSABI

static_assert(sizeof(aas2PVSABI::list_t<int>) == 0x18,
              "retail AAS list ABI drift");
static_assert(sizeof(aas2PVSABI::area_t) == 0x28,
              "retail AAS area ABI drift");
static_assert(offsetof(aas2PVSABI::area_t, obstaclePVSOffset) == 0x10,
              "retail AAS obstacle-PVS offset drift");
static_assert(sizeof(aas2PVSABI::edge_t) == 0x0C,
              "retail AAS edge ABI drift");
