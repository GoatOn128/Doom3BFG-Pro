#pragma once

#include <cstddef>

class idVec3;
class idClipModel;
class idAASClipLink;
struct aas2Area_t;
struct aas2ChokePoint_t;
struct aas2Cover_t;
struct aas2HintNode_t;
struct obstacleEdge_t;
struct obstacleRoute_t;
struct obstacleVertex_t;

class idAAS2 {
public:
#define ID_AAS2_RECOVERED_SLOT(slot) virtual int Slot##slot() = 0
  ID_AAS2_RECOVERED_SLOT(00);
  ID_AAS2_RECOVERED_SLOT(01);
  ID_AAS2_RECOVERED_SLOT(02);
  ID_AAS2_RECOVERED_SLOT(03);
  ID_AAS2_RECOVERED_SLOT(04);
  ID_AAS2_RECOVERED_SLOT(05);
  ID_AAS2_RECOVERED_SLOT(06);
  ID_AAS2_RECOVERED_SLOT(07);
  ID_AAS2_RECOVERED_SLOT(08);
  ID_AAS2_RECOVERED_SLOT(09);
  ID_AAS2_RECOVERED_SLOT(10);
  ID_AAS2_RECOVERED_SLOT(11);
  ID_AAS2_RECOVERED_SLOT(12);
  ID_AAS2_RECOVERED_SLOT(13);
  ID_AAS2_RECOVERED_SLOT(14);
  ID_AAS2_RECOVERED_SLOT(15);
  ID_AAS2_RECOVERED_SLOT(16);
  ID_AAS2_RECOVERED_SLOT(17);
  ID_AAS2_RECOVERED_SLOT(18);
  ID_AAS2_RECOVERED_SLOT(19);
  ID_AAS2_RECOVERED_SLOT(20);
  ID_AAS2_RECOVERED_SLOT(21);
  ID_AAS2_RECOVERED_SLOT(22);
  virtual int GetAreaFlags(int areaNum) const = 0;
  ID_AAS2_RECOVERED_SLOT(24);
  ID_AAS2_RECOVERED_SLOT(25);
  ID_AAS2_RECOVERED_SLOT(26);
  ID_AAS2_RECOVERED_SLOT(27);
  ID_AAS2_RECOVERED_SLOT(28);
  ID_AAS2_RECOVERED_SLOT(29);
  ID_AAS2_RECOVERED_SLOT(30);
  ID_AAS2_RECOVERED_SLOT(31);
  ID_AAS2_RECOVERED_SLOT(32);
  ID_AAS2_RECOVERED_SLOT(33);
  ID_AAS2_RECOVERED_SLOT(34);
  ID_AAS2_RECOVERED_SLOT(35);
  ID_AAS2_RECOVERED_SLOT(36);
  ID_AAS2_RECOVERED_SLOT(37);
  ID_AAS2_RECOVERED_SLOT(38);
  virtual int GetNumAreas() const = 0;
  ID_AAS2_RECOVERED_SLOT(40);
  virtual const aas2Area_t& GetArea(int areaNum) const = 0;
  ID_AAS2_RECOVERED_SLOT(42);
  ID_AAS2_RECOVERED_SLOT(43);
  virtual void ClipGridToAreas(const idVec3& origin, int areaNum,
                               int travelFlags, int cellSize, int dimension,
                               unsigned char* reachable) const = 0;
  ID_AAS2_RECOVERED_SLOT(45);
  ID_AAS2_RECOVERED_SLOT(46);
  ID_AAS2_RECOVERED_SLOT(47);
  ID_AAS2_RECOVERED_SLOT(48);
  ID_AAS2_RECOVERED_SLOT(49);
  ID_AAS2_RECOVERED_SLOT(50);
  ID_AAS2_RECOVERED_SLOT(51);
  ID_AAS2_RECOVERED_SLOT(52);
  ID_AAS2_RECOVERED_SLOT(53);
  ID_AAS2_RECOVERED_SLOT(54);
  ID_AAS2_RECOVERED_SLOT(55);
  ID_AAS2_RECOVERED_SLOT(56);
  ID_AAS2_RECOVERED_SLOT(57);
  ID_AAS2_RECOVERED_SLOT(58);
  ID_AAS2_RECOVERED_SLOT(59);
  ID_AAS2_RECOVERED_SLOT(60);
  virtual const unsigned char* GetObstaclePVS(int areaNum) const = 0;
  virtual int GetObstaclePVSWallEdges(int areaNum, int edgeFlags,
                                      int* edges, int maxEdges) const = 0;
  virtual int GetObstaclePVSClipModels(int areaNum,
                                       idClipModel** clipModels,
                                       int maxClipModels) const = 0;
  virtual void GetObstaclePVSObstacles(
      int startAreaNum, int edgeFlags, obstacleVertex_t* vertices,
      int& maxVertices, obstacleEdge_t* edges, int& maxEdges,
      idClipModel** clipModels, int& maxClipModels,
      obstacleRoute_t* route) const = 0;
  virtual void FreeClipLinks() = 0;
  virtual idAASClipLink* LinkClipModel(idClipModel* clipModel) = 0;
  virtual idAASClipLink* UnLinkClipModel(idAASClipLink* links) = 0;
#undef ID_AAS2_RECOVERED_SLOT

  virtual int GetNumCoverPoints() const = 0;
  virtual const aas2Cover_t* GetCoverPoint(int coverIndex) const = 0;
  virtual int GetNumAreaCoverPoints(int areaNum) const = 0;
  virtual int GetAreaCoverPoints(int areaNum, int* coverPoints, int maxCover,
                                 int& numCover) const = 0;
  virtual int FindCoverPoints(int areaNum, const idVec3& origin,
                              float maxDist, int requiredFlags,
                              int excludeFlags, int travelFlags,
                              int* coverPoints, int maxCover,
                              int& numCover) const = 0;
  virtual int GetNumChokePoints() const = 0;
  virtual const aas2ChokePoint_t* GetChokePoint(int chokeIndex) const = 0;
  virtual int GetNumHintNodes() const = 0;
  virtual const aas2HintNode_t* GetHintNode(int hintIndex) const = 0;
};

static_assert(sizeof(idAAS2) == 0x08, "idAAS2 ABI drift");
