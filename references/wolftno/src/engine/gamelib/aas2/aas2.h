#pragma once

#include "engine/aas2file/aas2file.h"
#include "engine/gamelib/aas2/aas2_interface.h"
#include "shared/idlib/containers/blockalloc.h"

#include <cstddef>
#include <cstdint>

class AAS2SpacialValidationAccess;
class AAS2PVSValidationAccess;
class idClipModel;

class idAASClipLink {
public:
  idAASClipLink* prevArea;
  idAASClipLink* nextArea;
  idAASClipLink* prevClip;
  idAASClipLink* nextClip;
  idClipModel* clipModel;
  int areaNum;
};

struct obstacleVertex_t {
  idVec3 position;
};

struct obstacleEdge_t {
  int flags;
  std::int16_t verts[2];
  int vertNums[2];
};

struct obstacleRoute_t {
  struct routeArea_t {
    int areaNum;
    idVec3 start;
    idVec3 end;
  };

  int numAreas;
  int pad[2];
  idVec3 endAlignDir;
  float endTurnRadius;
  routeArea_t areas[11];
};

class idAAS2Local : public idAAS2 {
public:
  char GetAreaCells(int areaNum, int subSize, int* subx, int* suby);
  char WorldPosToCellPos(int areaNum, int subSize, const idVec3* origin,
                         int* subx, int* suby);
  char CellPosToWorldPos(int areaNum, int subSize, int subx, int suby,
                         idVec3* worldPos);

  const unsigned char* GetObstaclePVS(int areaNum) const override;
  int GetObstaclePVSWallEdges(int areaNum, int edgeFlags, int* edges,
                              int maxEdges) const override;
  int GetObstaclePVSClipModels(int areaNum, idClipModel** clipModels,
                               int maxClipModels) const override;
  void GetObstaclePVSObstacles(
      int startAreaNum, int edgeFlags, obstacleVertex_t* vertices,
      int& maxVertices, obstacleEdge_t* edges, int& maxEdges,
      idClipModel** clipModels, int& maxClipModels,
      obstacleRoute_t* route) const override;
  void FreeClipLinks() override;
  idAASClipLink* LinkClipModel(idClipModel* clipModel) override;
  idAASClipLink* UnLinkClipModel(idAASClipLink* links) override;
  void ClipGridToAreas(const idVec3& origin, int areaNum, int travelFlags,
                       int cellSize, int dimension,
                       unsigned char* reachable) const override;

  int GetNumCoverPoints() const override;
  const aas2Cover_t* GetCoverPoint(int coverIndex) const override;
  int GetNumAreaCoverPoints(int areaNum) const override;
  int GetAreaCoverPoints(int areaNum, int* coverPoints, int maxCover,
                         int& numCover) const override;
  int FindCoverPoints(int areaNum, const idVec3& origin, float maxDist,
                      int requiredFlags, int excludeFlags, int travelFlags,
                      int* coverPoints, int maxCover,
                      int& numCover) const override;
  int GetNumChokePoints() const override;
  const aas2ChokePoint_t* GetChokePoint(int chokeIndex) const override;
  int GetNumHintNodes() const override;
  const aas2HintNode_t* GetHintNode(int hintIndex) const override;

  idAAS2File* file;

  std::uint8_t recoveredBeforeObstaclePVS[0xAC];
  int numObstaclePVSBytes;
  unsigned char* obstaclePVS;
  mutable int obstaclePVSAreaNum;
  std::uint8_t obstaclePVSPadding[4];
  idAASClipLink** areaClipModels;
  idBlockAlloc<idAASClipLink, 128, 37> clipModelLinkAllocator;
  std::uint8_t recoveredObstacleAvoidance[0x30];

private:
  void ShutdownObstaclePVS();
  void SetupObstaclePVS();
  void DecompressObstaclePVS(int areaNum, unsigned char* pvs,
                             int pvsSize) const;
  void GetObstaclePVSObstaclesInternal(
      int areaNum, int edgeFlags, obstacleVertex_t* vertices,
      int& maxVertices, obstacleEdge_t* edges, int& maxEdges,
      idClipModel** clipModels, int& maxClipModels,
      obstacleRoute_t* route) const;

  void FindCoverPoints_Internal(int startAreaNum, const idVec3& origin,
                                const idBounds& bounds, float maxDistSqr,
                                int requiredFlags, int excludeFlags,
                                int travelFlags, int* coverPoints,
                                int maxCover, int& numCover) const;

  friend class AAS2SpacialValidationAccess;
  friend class AAS2PVSValidationAccess;
};

static_assert(sizeof(idAASClipLink) == 0x30, "idAASClipLink ABI drift");
static_assert(offsetof(idAASClipLink, clipModel) == 0x20,
              "idAASClipLink clip-model offset drift");
static_assert(offsetof(idAASClipLink, areaNum) == 0x28,
              "idAASClipLink area-number offset drift");
static_assert(sizeof(obstacleVertex_t) == 0x0C,
              "obstacle vertex ABI drift");
static_assert(sizeof(obstacleEdge_t) == 0x10,
              "obstacle edge ABI drift");
static_assert(sizeof(obstacleRoute_t::routeArea_t) == 0x1C,
              "obstacle route-area ABI drift");
static_assert(offsetof(obstacleRoute_t, areas) == 0x1C,
              "obstacle route array offset drift");
static_assert(sizeof(obstacleRoute_t) == 0x150,
              "obstacle route ABI drift");
static_assert(offsetof(idAAS2Local, file) == 8, "idAAS2Local ABI drift");
static_assert(offsetof(idAAS2Local, numObstaclePVSBytes) == 0xBC,
              "idAAS2Local PVS-size offset drift");
static_assert(offsetof(idAAS2Local, obstaclePVS) == 0xC0,
              "idAAS2Local PVS pointer offset drift");
static_assert(offsetof(idAAS2Local, obstaclePVSAreaNum) == 0xC8,
              "idAAS2Local PVS area offset drift");
static_assert(offsetof(idAAS2Local, areaClipModels) == 0xD0,
              "idAAS2Local clip-list offset drift");
static_assert(offsetof(idAAS2Local, clipModelLinkAllocator) == 0xD8,
              "idAAS2Local clip allocator offset drift");
static_assert(sizeof(idAAS2Local) == 0x128, "idAAS2Local ABI drift");
