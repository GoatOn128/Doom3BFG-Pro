#pragma once

#include "shared/idlib/containers/index.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"

#include <cstddef>
#include <cstdint>

enum invalidReachability_t { INVALID_REACHABILITY = -1 };
enum invalidAASDependencyIndex_t { INVALID_AAS_DEPENDENCY = -1 };
enum invalidAASInteractionEntIndex_t { INVALID_AAS_INTERACTION_ENTITY = -1 };
enum invalidAASTree_t { INVALID_AAS_TREE = -1 };

using idAAS2ReachabilityIndex = idIndex<short, invalidReachability_t>;
using idAAS2DependencyIndex = idIndex<short, invalidAASDependencyIndex_t>;
using idAAS2InteractionEntIndex =
    idIndex<short, invalidAASInteractionEntIndex_t>;
using idAAS2TreeIndex = idIndex<int, invalidAASTree_t>;

struct idQuantizedVec3 {
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
};

struct aas2EdgeCrossed_t {
  int toAreaNum;
  int edgeNum;
  idVec3 edgePoint;
};

struct aas2TraceFloor_t {
  float fraction;
  idVec3 endPos;
  int lastAreaNum;
  aas2EdgeCrossed_t firstEdge;
  aas2EdgeCrossed_t lastEdge;
  int maxAreas;
  int numAreas;
  int maxReachIndices;
  int numReachIndices;
  std::uint32_t pointerPadding;
  int* areas;
  idAAS2ReachabilityIndex* reachIndices;
};

struct aas2TraceHeight_t {
  int maxPoints;
  int numPoints;
  idVec3* points;
};

struct aas2Reachability_t {
  int travelFlags;
  std::uint16_t travelTime;
  std::uint16_t fromAreaNum;
  std::uint16_t toAreaNum;
  std::uint16_t padding;
  idQuantizedVec3 start;
  idQuantizedVec3 end;
  int areaTravelTimeOffsetAndNumber;
  idAAS2ReachabilityIndex next;
  idAAS2ReachabilityIndex reverseNext;
};

struct aas2Area_t {
  int travelFlags;
  std::uint16_t flags;
  std::int16_t numEdges;
  int firstEdge;
  std::int16_t cluster;
  std::int16_t clusterAreaNum;
  int obstaclePVSOffset;
  idAAS2ReachabilityIndex reach;
  idAAS2ReachabilityIndex reverseReach;
  std::uint16_t firstChokePoint;
  std::uint16_t numChokePoints;
  std::uint16_t firstCover;
  std::uint16_t numCover;
  std::uint16_t firstTraversal;
  std::uint16_t numTraversals;
  std::uint16_t firstHintNode;
  std::uint16_t numHintNodes;
};

struct aas2Traversal_t {
  void CalcExtrusionPoint(idVec3& extrusionPoint, const idVec3& up) const;

  idVec3 startPoint;
  idVec3 endPoint;
  idQuantizedVec3 orientationForward;
  idQuantizedVec3 extrusionForward;
  std::int16_t animIndex;
  idAAS2ReachabilityIndex reachabilityIndex;
  idAAS2DependencyIndex dependencyIndex;
  idAAS2InteractionEntIndex interactionEntIndex;
  std::int16_t extrusionDistance;
  std::uint16_t startAreaNum;
  std::uint16_t endAreaNum;
  std::int16_t traversalNameIndex;
  int flags;
};

struct obstacleRoute_t {
  struct routeArea_t {
    int areaNum;
    idVec3 start;
    idVec3 end;
  };

  int numAreas;
  int padding[2];
  idVec3 endAlignDir;
  float endTurnRadius;
  routeArea_t areas[11];
};

struct idAAS2Path {
  int type;
  idVec3 moveGoal;
  int moveAreaNum;
  aas2EdgeCrossed_t firstEdge;
  idAAS2ReachabilityIndex reachIndex;
  std::uint16_t reachIndexPadding;
  int travelTime;
  int traversalIndex;
  idVec3 traversalStart;
  idVec3 traversalEnd;
  idAAS2DependencyIndex dependencyIndex;
  std::uint16_t dependencyIndexPadding;
  idVec3 obstacleGoal;
  int obstacleAreaNum;
  float pathMaxHeight;
  obstacleRoute_t obstacleRoute;
};

struct idAAS2HopPathParms {
  float maxDistance;
  float maxHeight;
  float minHeight;
  float maxSlope;
};

struct idAAS2PathPoint {
  int areaNum;
  idVec3 origin;
  idAAS2ReachabilityIndex nextReachIndex;
  std::uint16_t nextReachIndexPadding;
  int travelTime;
};

struct idAreaTrail {
  int areaTrail[4];
  int trailIndex;
};

class idAAS2File {
public:
  struct bspTree_t {
    idVec3 floorNormal;
    int headNode;
    int firstArea;
    int lastArea;
  };

  int GetTraversalsForReachability(
      idAAS2ReachabilityIndex reachIndex,
      idList<int, 5>& traversalList) const;
  int GetTraversalsForInteractableEntity(
      idAAS2InteractionEntIndex interactionEntIndex,
      idList<int, 5>& traversalList) const;
  bool TraceFloor(aas2TraceFloor_t& trace, const idVec3& start,
                  int startAreaNum, const idVec3& end, int endAreaNum,
                  int travelFlags, bool allowFloorNormalChange,
                  bool ignoreGravityDirectionDistance,
                  bool ignoreSameArea) const;
  bool TraceHeight(idAAS2TreeIndex tree, aas2TraceHeight_t& trace,
                   const idVec3& start, const idVec3& end) const;
  idAAS2TreeIndex GetTreeForArea(int areaNum) const;
  bool PushPointIntoAreaNum(int areaNum, idVec3& point) const;

  std::uint8_t recoveredPrefix[0x130];
  float obstaclePVSRadius;
  std::uint8_t recoveredBeforeTrees[0x6C];
  idList<bspTree_t, 5> trees;
  std::uint8_t recoveredBeforeReachabilities[0x60];
  idList<aas2Reachability_t, 5> reachabilities;
  idList<aas2Area_t, 5> areas;
  std::uint8_t recoveredBeforeTraversalPoints[0x150];
  idList<aas2Traversal_t, 5> traversalPoints;
  std::uint8_t recoveredTail[0x30];
};

class idAAS2Local {
public:
#define ID_AAS2_PATHING_SLOT(slot) virtual int PathingSlot##slot() const = 0
  ID_AAS2_PATHING_SLOT(00);
  ID_AAS2_PATHING_SLOT(01);
  ID_AAS2_PATHING_SLOT(02);
  ID_AAS2_PATHING_SLOT(03);
  ID_AAS2_PATHING_SLOT(04);
  ID_AAS2_PATHING_SLOT(05);
  ID_AAS2_PATHING_SLOT(06);
  ID_AAS2_PATHING_SLOT(07);
  ID_AAS2_PATHING_SLOT(08);
  ID_AAS2_PATHING_SLOT(09);
  ID_AAS2_PATHING_SLOT(10);
  ID_AAS2_PATHING_SLOT(11);
  ID_AAS2_PATHING_SLOT(12);
  ID_AAS2_PATHING_SLOT(13);
  ID_AAS2_PATHING_SLOT(14);
  ID_AAS2_PATHING_SLOT(15);
  ID_AAS2_PATHING_SLOT(16);
  ID_AAS2_PATHING_SLOT(17);
  ID_AAS2_PATHING_SLOT(18);
  ID_AAS2_PATHING_SLOT(19);
  ID_AAS2_PATHING_SLOT(20);
  ID_AAS2_PATHING_SLOT(21);
  ID_AAS2_PATHING_SLOT(22);
  ID_AAS2_PATHING_SLOT(23);
  ID_AAS2_PATHING_SLOT(24);
  ID_AAS2_PATHING_SLOT(25);
  ID_AAS2_PATHING_SLOT(26);
  ID_AAS2_PATHING_SLOT(27);
  virtual bool RouteToGoalArea(
      int areaNum, const idVec3& origin, int goalAreaNum,
      const idVec3& goalOrigin, int travelFlags, int& travelTime,
      idAAS2ReachabilityIndex& reachIndex) const = 0;
  ID_AAS2_PATHING_SLOT(29);
  virtual bool WalkPathToGoal(idAAS2Path& path, int startAreaNum,
                              const idVec3& startOrigin, int goalAreaNum,
                              const idVec3& goalOrigin, int travelFlags,
                              int walkTravelFlags) const;
  virtual bool ExtendHopPathToGoal(
      idAAS2Path& path, int startAreaNum, const idVec3& startOrigin,
      int goalAreaNum, const idVec3& goalOrigin, int travelFlags,
      int walkTravelFlags, const idAAS2HopPathParms& parms) const;
  ID_AAS2_PATHING_SLOT(32);
  ID_AAS2_PATHING_SLOT(33);
  ID_AAS2_PATHING_SLOT(34);
  ID_AAS2_PATHING_SLOT(35);
  ID_AAS2_PATHING_SLOT(36);
  ID_AAS2_PATHING_SLOT(37);
  virtual bool GetOptimizedWalkPath(int startAreaNum,
                                    const idVec3& startOrigin,
                                    int goalAreaNum,
                                    const idVec3& goalOrigin,
                                    int travelFlags, int walkTravelFlags,
                                    idList<idVec3, 5>& path) const;
  ID_AAS2_PATHING_SLOT(39);
  ID_AAS2_PATHING_SLOT(40);
  ID_AAS2_PATHING_SLOT(41);
  ID_AAS2_PATHING_SLOT(42);
  virtual bool GetPath(const idVec3& startPos, const idVec3& goalPos,
                       int startAreaNum, int goalAreaNum, int travelFlags,
                       idAAS2PathPoint* pointList, int maxPoints,
                       int& numPoints) const;
  ID_AAS2_PATHING_SLOT(44);
  ID_AAS2_PATHING_SLOT(45);
  virtual int GetTraversalsForReachability(
      idAAS2ReachabilityIndex reachIndex,
      idList<int, 5>& traversalList) const;
  virtual int GetTraversalsForInteractableEntity(
      idAAS2InteractionEntIndex interactionEntIndex,
      idList<int, 5>& traversalList) const;
  virtual int FindBestTraversalForReachability(
      idAAS2ReachabilityIndex reachIndex, const idVec3& curPoint,
      const idVec3& goalPoint, idVec3& outTraversalStart,
      idVec3& outTraversalEnd, idAAS2DependencyIndex& dependencyIndex,
      int travelFlags, bool useClosestPointOnFail) const;
  ID_AAS2_PATHING_SLOT(49);
  ID_AAS2_PATHING_SLOT(50);
  ID_AAS2_PATHING_SLOT(51);
  ID_AAS2_PATHING_SLOT(52);
  ID_AAS2_PATHING_SLOT(53);
  ID_AAS2_PATHING_SLOT(54);
  ID_AAS2_PATHING_SLOT(55);
  ID_AAS2_PATHING_SLOT(56);
  ID_AAS2_PATHING_SLOT(57);
  ID_AAS2_PATHING_SLOT(58);
  ID_AAS2_PATHING_SLOT(59);
  ID_AAS2_PATHING_SLOT(60);
  ID_AAS2_PATHING_SLOT(61);
  ID_AAS2_PATHING_SLOT(62);
  ID_AAS2_PATHING_SLOT(63);
  ID_AAS2_PATHING_SLOT(64);
  ID_AAS2_PATHING_SLOT(65);
  ID_AAS2_PATHING_SLOT(66);
  ID_AAS2_PATHING_SLOT(67);
  ID_AAS2_PATHING_SLOT(68);
  ID_AAS2_PATHING_SLOT(69);
  ID_AAS2_PATHING_SLOT(70);
  ID_AAS2_PATHING_SLOT(71);
  ID_AAS2_PATHING_SLOT(72);
  ID_AAS2_PATHING_SLOT(73);
  ID_AAS2_PATHING_SLOT(74);
  ID_AAS2_PATHING_SLOT(75);
  ID_AAS2_PATHING_SLOT(76);
  ID_AAS2_PATHING_SLOT(77);
  ID_AAS2_PATHING_SLOT(78);
  ID_AAS2_PATHING_SLOT(79);
  ID_AAS2_PATHING_SLOT(80);
  ID_AAS2_PATHING_SLOT(81);
  ID_AAS2_PATHING_SLOT(82);
  ID_AAS2_PATHING_SLOT(83);
  virtual const idVec3& GetFloorNormalForArea(int areaNum) const = 0;
#undef ID_AAS2_PATHING_SLOT

  idAAS2File* file;
  std::uint8_t recoveredTail[0x118];

private:
  unsigned short AreaTravelTime(int areaNum, const idVec3& start,
                                const idVec3& end) const;
  bool WalkPathIsValid(int startAreaNum, const idVec3& startOrigin,
                       int goalAreaNum, const idVec3& goalOrigin,
                       int travelFlags, int& endAreaNum,
                       aas2EdgeCrossed_t& firstEdgeCrossed) const;
  void SubSampleWalkPath(int startAreaNum, const idVec3& startOrigin,
                         int pathAreaNum, const idVec3& pathStart,
                         const idVec3& pathEnd, int travelFlags,
                         idVec3& endPos, int& endAreaNum,
                         aas2EdgeCrossed_t& firstEdgeCrossed) const;
  bool HopPathIsValid(int startAreaNum, const idVec3& startOrigin,
                      int goalAreaNum, const idVec3& goalOrigin,
                      const idAAS2HopPathParms& parms) const;
};

class idCVar;
extern idCVar aas_debugTraversals;
extern idCVar aas_simplifyPath;

static_assert(sizeof(idQuantizedVec3) == 0x06,
              "retail quantized-vector ABI drift");
static_assert(sizeof(aas2EdgeCrossed_t) == 0x14,
              "retail crossed-edge ABI drift");
static_assert(sizeof(aas2TraceFloor_t) == 0x60,
              "retail trace-floor ABI drift");
static_assert(offsetof(aas2TraceFloor_t, firstEdge) == 0x14,
              "retail first-edge offset drift");
static_assert(offsetof(aas2TraceFloor_t, areas) == 0x50,
              "retail trace-area pointer offset drift");
static_assert(sizeof(aas2TraceHeight_t) == 0x10,
              "retail trace-height ABI drift");
static_assert(sizeof(aas2Reachability_t) == 0x20,
              "retail reachability ABI drift");
static_assert(sizeof(aas2Area_t) == 0x28, "retail area ABI drift");
static_assert(sizeof(aas2Traversal_t) == 0x38,
              "retail traversal ABI drift");
static_assert(sizeof(obstacleRoute_t) == 0x150,
              "retail obstacle-route ABI drift");
static_assert(sizeof(idAAS2Path) == 0x1B4, "retail path ABI drift");
static_assert(offsetof(idAAS2Path, obstacleRoute) == 0x64,
              "retail path route offset drift");
static_assert(sizeof(idAAS2PathPoint) == 0x18,
              "retail path-point ABI drift");
static_assert(sizeof(idAreaTrail) == 0x14, "retail area-trail ABI drift");
static_assert(sizeof(idAAS2File::bspTree_t) == 0x18,
              "retail BSP-tree ABI drift");
static_assert(offsetof(idAAS2File, obstaclePVSRadius) == 0x130,
              "retail obstacle radius offset drift");
static_assert(offsetof(idAAS2File, trees) == 0x1A0,
              "retail tree-list offset drift");
static_assert(offsetof(idAAS2File, reachabilities) == 0x218,
              "retail reachability-list offset drift");
static_assert(offsetof(idAAS2File, areas) == 0x230,
              "retail area-list offset drift");
static_assert(offsetof(idAAS2File, traversalPoints) == 0x398,
              "retail traversal-list offset drift");
static_assert(sizeof(idAAS2File) == 0x3E0, "retail AAS file ABI drift");
static_assert(offsetof(idAAS2Local, file) == 0x08,
              "retail local AAS file offset drift");
static_assert(sizeof(idAAS2Local) == 0x128,
              "retail local AAS ABI drift");
