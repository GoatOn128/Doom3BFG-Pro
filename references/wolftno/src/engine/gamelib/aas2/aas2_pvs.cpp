#define WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI
#include "engine/gamelib/aas2/aas2_pvs_abi.h"
#undef WOLFTNO_RETAIL_IDMEM_SINGLETON_ABI

#include "engine/gamelib/physics/clip.h"

#include <cstdint>
#include <cstring>

using aas2PVSABI::area_t;
using aas2PVSABI::edge_t;
using aas2PVSABI::list_t;

namespace {

int Add32(int left, int right) {
  return static_cast<int>(static_cast<std::uint32_t>(left) +
                          static_cast<std::uint32_t>(right));
}

int Sub32(int left, int right) {
  return static_cast<int>(static_cast<std::uint32_t>(left) -
                          static_cast<std::uint32_t>(right));
}

int Abs32(int value) {
  const std::uint32_t bits = static_cast<std::uint32_t>(value);
  return value < 0 ? static_cast<int>(0u - bits) : value;
}

const area_t& VirtualArea(const idAAS2Local& aas, int areaNum) {
  return reinterpret_cast<const area_t&>(aas.GetArea(areaNum));
}

idAAS2File* LiveFile(const idAAS2Local& aas) {
  return *reinterpret_cast<idAAS2File* const volatile*>(&aas.file);
}

bool PVSContains(const unsigned char* pvs, int areaNum) {
  const std::int64_t byteIndex = static_cast<std::int64_t>(areaNum) >> 3;
  const unsigned char mask =
      static_cast<unsigned char>(1u << (areaNum & 7));
  return (pvs[byteIndex] & mask) != 0;
}

bool HasVisibleLowerArea(const idAASClipLink* link,
                         const unsigned char* pvs) {
  for (const idAASClipLink* lower = link->prevArea; lower != nullptr;
       lower = lower->prevArea) {
    if (lower->areaNum < link->areaNum &&
        PVSContains(pvs, lower->areaNum)) {
      return true;
    }
  }
  return false;
}

void WalkHigherAreas(const idAASClipLink* link) {
  const idAASClipLink* higher = link->nextArea;
  while (higher != nullptr) {
    higher = *reinterpret_cast<idAASClipLink* const volatile*>(
        &higher->nextArea);
  }
}

void StorePVSBit(unsigned char* pvs, int areaNum, int value) {
  const std::int64_t byteIndex = static_cast<std::int64_t>(areaNum) >> 3;
  const unsigned int shift = static_cast<unsigned int>(areaNum) & 7u;
  pvs[byteIndex] |= static_cast<unsigned char>((value & 1) << shift);
}

} // namespace

template <>
__declspec(noinline) void
idBlockAlloc<idAASClipLink, 128, 37>::AllocNewBlock() {
  idBlock* const block = static_cast<idBlock*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\shared\\idlib\\BlockAlloc.h(207) : "
          "_memTag_",
          sizeof(idBlock), static_cast<memTag_t>(37), false, ALIGN_16,
          HEAP_DEFAULTHEAP));
  block->next = blocks;
  blocks = block;
  for (int index = 0; index < 128; ++index) {
    block->elements[index].next = free;
    free = &block->elements[index];
  }
  total += 128;
}

void idAAS2Local::ShutdownObstaclePVS() {
  idMem::Singleton().Free(obstaclePVS, ALIGN_16);
  idMem::Singleton().Free(areaClipModels, ALIGN_16);
  numObstaclePVSBytes = 0;
  obstaclePVS = nullptr;
  areaClipModels = nullptr;
  obstaclePVSAreaNum = 0;
}

int idAAS2Local::GetObstaclePVSWallEdges(int areaNum, int edgeFlags,
                                         int* edges, int maxEdges) const {
  if (file == nullptr) {
    return 0;
  }
  return file->GetObstaclePVSWallEdges(areaNum, edgeFlags, edges, maxEdges);
}

void idAAS2Local::SetupObstaclePVS() {
  const int rounded = Add32(aas2PVSABI::List<area_t>(
                                file, aas2PVSABI::AREAS_OFFSET)
                                .num,
                            7);
  numObstaclePVSBytes =
      Add32(rounded, rounded < 0 ? 7 : 0) >> 3;

  obstaclePVS = static_cast<unsigned char*>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\engine\\gamelib\\aas2\\AAS2_pvs.cpp(26) "
          ": TAG_AAS",
          static_cast<std::size_t>(
              static_cast<std::int64_t>(numObstaclePVSBytes)),
          static_cast<memTag_t>(37), false, ALIGN_16, HEAP_DEFAULTHEAP));

  const int numAreas =
      aas2PVSABI::List<area_t>(file, aas2PVSABI::AREAS_OFFSET).num;
  areaClipModels = static_cast<idAASClipLink**>(
      idMem::Singleton().AllocWithLocation(
          "w:\\build\\tech5\\engine\\gamelib\\aas2\\AAS2_pvs.cpp(27) "
          ": TAG_AAS",
          static_cast<std::size_t>(static_cast<std::int64_t>(numAreas) * 8),
          static_cast<memTag_t>(37), true, ALIGN_16, HEAP_DEFAULTHEAP));
}

const unsigned char* idAAS2Local::GetObstaclePVS(int areaNum) const {
  if (areaNum == obstaclePVSAreaNum) {
    return obstaclePVS;
  }

  std::memset(obstaclePVS, 0,
              static_cast<std::size_t>(
                  static_cast<std::int64_t>(numObstaclePVSBytes)));
  const list_t<area_t>& areas =
      aas2PVSABI::List<area_t>(file, aas2PVSABI::AREAS_OFFSET);
  int outputArea = 0;
  const int numAreas = areas.num;
  std::int64_t inputOffset = areas.list[static_cast<std::int64_t>(areaNum)]
                                 .obstaclePVSOffset;

  if (numAreas > 0) {
    do {
      ++inputOffset;
      const unsigned char* const compressed =
          aas2PVSABI::List<unsigned char>(
              LiveFile(*this), aas2PVSABI::OBSTACLE_PVS_OFFSET)
              .list;
      const int value = compressed[inputOffset - 1];
      if ((value & 0x80) == 0) {
        for (int bit = 0; bit < 7; ++bit) {
          StorePVSBit(obstaclePVS, Add32(outputArea, bit), value >> bit);
        }
        outputArea = Add32(outputArea, 7);
      } else {
        int skip = value & 0x3F;
        if ((value & 0x40) != 0) {
          skip |= compressed[inputOffset++] << 6;
        }
        outputArea = Add32(outputArea, Add32(skip, 1));
      }
    } while (outputArea < numAreas);
  }

  obstaclePVSAreaNum = areaNum;
  return obstaclePVS;
}

void idAAS2Local::FreeClipLinks() {
  clipModelLinkAllocator.Shutdown();
  const int numAreas =
      aas2PVSABI::List<area_t>(file, aas2PVSABI::AREAS_OFFSET).num;
  for (int areaNum = 0; areaNum < numAreas; ++areaNum) {
    areaClipModels[areaNum] = nullptr;
  }
}

idAASClipLink* idAAS2Local::LinkClipModel(idClipModel* clipModel) {
  idBounds bounds = clipModel->absBounds;
  idAASClipLink* links = nullptr;
  idAASTreeIndex tree(0);
  idAAS2File* currentFile = file;
  const idBounds& boundingBox = aas2PVSABI::BoundingBox(currentFile);
  bounds[0].x -= boundingBox[1].x;
  bounds[0].y -= boundingBox[1].y;
  bounds[1].x -= boundingBox[0].x;
  bounds[1].y -= boundingBox[0].y;

  if (aas2PVSABI::List<unsigned char>(currentFile,
                                     aas2PVSABI::TREES_OFFSET)
          .num <= 0) {
    return links;
  }

  do {
    int areas[128];
    const int count =
        currentFile->BoundsAreaNums(tree, bounds, areas, 128);

    for (int pass = count - 1; pass > 0; --pass) {
      for (int index = 0; index < pass; ++index) {
        if (areas[index] < areas[index + 1]) {
          const int temporary = areas[index];
          areas[index] = areas[index + 1];
          areas[index + 1] = temporary;
        }
      }
    }

    for (int index = 0; index < count; ++index) {
      idAASClipLink* const link = clipModelLinkAllocator.Alloc();
      const int areaNum = areas[index];
      link->clipModel = clipModel;
      link->nextArea = links;
      link->prevArea = nullptr;
      link->areaNum = areaNum;
      if (links != nullptr) {
        links->prevArea = link;
      }
      links = link;

      idAASClipLink* const oldAreaHead = areaClipModels[areaNum];
      link->prevClip = nullptr;
      link->nextClip = oldAreaHead;
      idAASClipLink** const currentAreaClipModels = areaClipModels;
      if (currentAreaClipModels[areaNum] != nullptr) {
        currentAreaClipModels[areaNum]->prevClip = link;
      }
      areaClipModels[areaNum] = link;
    }

    currentFile = LiveFile(*this);
    ++tree.value;
  } while (tree.value <
           aas2PVSABI::List<unsigned char>(currentFile,
                                          aas2PVSABI::TREES_OFFSET)
               .num);

  return links;
}

idAASClipLink* idAAS2Local::UnLinkClipModel(idAASClipLink* links) {
  idAASClipLink* link = links;
  while (link != nullptr) {
    idAASClipLink* const nextArea = link->nextArea;
    if (link->prevClip != nullptr) {
      link->prevClip->nextClip = link->nextClip;
    } else {
      areaClipModels[link->areaNum] = link->nextClip;
    }
    if (link->nextClip != nullptr) {
      link->nextClip->prevClip = link->prevClip;
    }

    link->prevArea = reinterpret_cast<idAASClipLink*>(
        clipModelLinkAllocator.free);
    --clipModelLinkAllocator.active;
    clipModelLinkAllocator.free =
        reinterpret_cast<decltype(clipModelLinkAllocator.free)>(link);
    link = nextArea;
  }
  return nullptr;
}

int idAAS2Local::GetObstaclePVSClipModels(
    int areaNum, idClipModel** clipModels, int maxClipModels) const {
  int numClipModels = 0;
  int writtenClipModels = 0;
  const area_t& startArea = VirtualArea(*this, areaNum);
  std::memset(obstaclePVS, 0,
              static_cast<std::size_t>(
                  static_cast<std::int64_t>(numObstaclePVSBytes)));

  std::int64_t inputOffset = startArea.obstaclePVSOffset;
  int outputArea = 0;
  const int numAreas =
      aas2PVSABI::List<area_t>(file, aas2PVSABI::AREAS_OFFSET).num;
  if (numAreas <= 0) {
    obstaclePVSAreaNum = areaNum;
    return 0;
  }

  for (;;) {
    ++inputOffset;
    const unsigned char* const compressed =
        aas2PVSABI::List<unsigned char>(
            LiveFile(*this), aas2PVSABI::OBSTACLE_PVS_OFFSET)
            .list;
    const int value = compressed[inputOffset - 1];
    if ((value & 0x80) != 0) {
      int skip = value & 0x3F;
      if ((value & 0x40) != 0) {
        skip |= compressed[inputOffset++] << 6;
      }
      outputArea = Add32(outputArea, Add32(skip, 1));
    } else {
      for (int bit = 0; bit < 7; ++bit) {
        const int currentArea = outputArea;
        StorePVSBit(obstaclePVS, currentArea, value >> bit);
        outputArea = Add32(outputArea, 1);

        if (((1 << bit) & value) != 0) {
          idAASClipLink* link = areaClipModels[currentArea];
          while (link != nullptr) {
            if (writtenClipModels >= maxClipModels) {
              return numClipModels;
            }
            const bool duplicate =
                HasVisibleLowerArea(link, obstaclePVS);
            WalkHigherAreas(link);
            if (!duplicate) {
              ++numClipModels;
              clipModels[writtenClipModels++] = link->clipModel;
            }
            link = link->nextClip;
          }
        }
      }
    }

    if (outputArea >= numAreas) {
      obstaclePVSAreaNum = areaNum;
      return numClipModels;
    }
  }
}

void idAAS2Local::GetObstaclePVSObstaclesInternal(
    int areaNum, int edgeFlags, obstacleVertex_t* vertices,
    int& maxVertices, obstacleEdge_t* edges, int& maxEdges,
    idClipModel** clipModels, int& maxClipModels,
    obstacleRoute_t* route) const {
  const int vertexCapacity = maxVertices;
  const int edgeCapacity = maxEdges;
  const int clipModelCapacity = maxClipModels;
  int numVertices = 0;
  int numEdges = 0;
  int numClipModels = 0;

  const area_t& startArea = VirtualArea(*this, areaNum);
  std::memset(obstaclePVS, 0,
              static_cast<std::size_t>(
                  static_cast<std::int64_t>(numObstaclePVSBytes)));
  std::int64_t inputOffset = startArea.obstaclePVSOffset;
  const int totalAreas =
      aas2PVSABI::List<area_t>(file, aas2PVSABI::AREAS_OFFSET).num;
  int outputArea = 0;

  if (totalAreas > 0) {
    do {
      ++inputOffset;
      const unsigned char* const compressed =
          aas2PVSABI::List<unsigned char>(
              LiveFile(*this), aas2PVSABI::OBSTACLE_PVS_OFFSET)
              .list;
      const int value = compressed[inputOffset - 1];
      if ((value & 0x80) != 0) {
        int skip = value & 0x3F;
        if ((value & 0x40) != 0) {
          skip |= compressed[inputOffset++] << 6;
        }
        outputArea = Add32(outputArea, Add32(skip, 1));
        continue;
      }

      for (int bit = 0; bit < 7 && outputArea < totalAreas; ++bit) {
        const int currentArea = outputArea;
        StorePVSBit(obstaclePVS, currentArea, value >> bit);
        outputArea = Add32(outputArea, 1);
        if (((1 << bit) & value) == 0) {
          continue;
        }

        idAASClipLink* link = areaClipModels[currentArea];
        while (link != nullptr && numClipModels < clipModelCapacity) {
          const bool duplicate = HasVisibleLowerArea(link, obstaclePVS);
          WalkHigherAreas(link);
          if (!duplicate) {
            clipModels[numClipModels++] = link->clipModel;
          }
          link = link->nextClip;
        }

        const area_t& currentAreaData = VirtualArea(*this, currentArea);
        int lastVertex = -1;
        for (int areaEdge = 0; areaEdge < currentAreaData.numEdges;
             ++areaEdge) {
          if (numEdges >= edgeCapacity ||
              numVertices >= Sub32(vertexCapacity, 1)) {
            break;
          }

          idAAS2File* const currentFile = LiveFile(*this);
          const list_t<int>& edgeIndexes =
              aas2PVSABI::List<int>(currentFile,
                                    aas2PVSABI::EDGE_INDEX_OFFSET);
          const int edgeIndex =
              edgeIndexes.list[Add32(currentAreaData.firstEdge, areaEdge)];
          const edge_t& edge =
              aas2PVSABI::List<edge_t>(currentFile,
                                       aas2PVSABI::EDGES_OFFSET)
                  .list[Abs32(edgeIndex)];
          if ((edgeFlags & edge.flags) == 0) {
            lastVertex = -1;
            continue;
          }

          const int destinationVertex = edge.vertexNum[edgeIndex >= 0 ? 1 : 0];
          const int sourceVertex = edge.vertexNum[edgeIndex < 0 ? 1 : 0];
          if (lastVertex == -1) {
            const idVec3* const sourceVertices =
                aas2PVSABI::List<idVec3>(
                    currentFile, aas2PVSABI::VERTICES_OFFSET)
                    .list;
            vertices[numVertices].position.x = sourceVertices[sourceVertex].x;
            vertices[numVertices].position.y = sourceVertices[sourceVertex].y;
            vertices[numVertices].position.z = sourceVertices[sourceVertex].z;
            lastVertex = numVertices++;
          }

          ++numEdges;
          const int newVertex = numVertices++;
          const idVec3* const destinationVertices =
              aas2PVSABI::List<idVec3>(
                  LiveFile(*this), aas2PVSABI::VERTICES_OFFSET)
                  .list;
          vertices[newVertex].position.x =
              destinationVertices[destinationVertex].x;
          vertices[newVertex].position.y =
              destinationVertices[destinationVertex].y;
          vertices[newVertex].position.z =
              destinationVertices[destinationVertex].z;

          obstacleEdge_t& outputEdge = edges[numEdges - 1];
          outputEdge.flags = edge.flags;
          outputEdge.verts[0] = static_cast<std::int16_t>(lastVertex);
          outputEdge.vertNums[0] = sourceVertex;
          outputEdge.vertNums[1] = destinationVertex;
          outputEdge.verts[1] = static_cast<std::int16_t>(newVertex);
          lastVertex = newVertex;
        }
      }
    } while (outputArea < totalAreas);
  }

  maxVertices = numVertices;
  maxEdges = numEdges;
  maxClipModels = numClipModels;
  obstaclePVSAreaNum = areaNum;

  if (route != nullptr && route->numAreas > 0) {
    int visibleRouteAreas = 0;
    while (visibleRouteAreas < route->numAreas &&
           PVSContains(obstaclePVS,
                       route->areas[visibleRouteAreas].areaNum)) {
      ++visibleRouteAreas;
    }
    if (visibleRouteAreas < route->numAreas) {
      route->numAreas = visibleRouteAreas;
    }
  }
}

void idAAS2Local::DecompressObstaclePVS(int areaNum, unsigned char* pvs,
                                        int pvsSize) const {
  const area_t& startArea = VirtualArea(*this, areaNum);
  std::memset(pvs, 0, static_cast<std::size_t>(
                          static_cast<std::int64_t>(pvsSize)));
  std::int64_t inputOffset = startArea.obstaclePVSOffset;
  const int numAreas =
      aas2PVSABI::List<area_t>(file, aas2PVSABI::AREAS_OFFSET).num;
  int outputArea = 0;

  if (numAreas > 0) {
    for (;;) {
      ++inputOffset;
      const unsigned char* const compressed =
          aas2PVSABI::List<unsigned char>(
              LiveFile(*this), aas2PVSABI::OBSTACLE_PVS_OFFSET)
              .list;
      const int value = compressed[inputOffset - 1];
      if ((value & 0x80) != 0) {
        int skip = value & 0x3F;
        if ((value & 0x40) != 0) {
          skip |= compressed[inputOffset++] << 6;
        }
        outputArea = Add32(outputArea, Add32(skip, 1));
      } else {
        for (int bit = 0; bit < 7 && outputArea < numAreas; ++bit) {
          StorePVSBit(pvs, outputArea, value >> bit);
          outputArea = Add32(outputArea, 1);
        }
      }

      if (outputArea >= numAreas) {
        return;
      }
    }
  }
}

void idAAS2Local::GetObstaclePVSObstacles(
    int startAreaNum, int edgeFlags, obstacleVertex_t* vertices,
    int& maxVertices, obstacleEdge_t* edges, int& maxEdges,
    idClipModel** clipModels, int& maxClipModels,
    obstacleRoute_t* route) const {
  if (route == nullptr || route->numAreas == 0) {
    GetObstaclePVSObstaclesInternal(
        startAreaNum, edgeFlags, vertices, maxVertices, edges, maxEdges,
        clipModels, maxClipModels, route);
    return;
  }

  const std::int64_t routeCount = route->numAreas;
  const int endAreaNum =
      reinterpret_cast<const int*>(route)[routeCount * 7];
  if (routeCount > 0) {
    int routeIndex = 0;
    obstacleRoute_t::routeArea_t* routeArea = route->areas;
    const unsigned char startMask =
        static_cast<unsigned char>(1u << (startAreaNum & 7));
    do {
      const int routeAreaNum = routeArea->areaNum;
      DecompressObstaclePVS(routeAreaNum, obstaclePVS,
                            numObstaclePVSBytes);
      if ((obstaclePVS[static_cast<std::int64_t>(startAreaNum) >> 3] &
           startMask) != 0 &&
          PVSContains(obstaclePVS, endAreaNum)) {
        GetObstaclePVSObstaclesInternal(
            routeAreaNum, edgeFlags, vertices, maxVertices, edges,
            maxEdges, clipModels, maxClipModels, route);
        if (routeAreaNum >= 0) {
          return;
        }
        break;
      }
      ++routeIndex;
      ++routeArea;
    } while (routeIndex < route->numAreas);
  }

  GetObstaclePVSObstaclesInternal(
      startAreaNum, edgeFlags, vertices, maxVertices, edges, maxEdges,
      clipModels, maxClipModels, route);
}
