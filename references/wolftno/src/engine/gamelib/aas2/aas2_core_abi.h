#pragma once

#include "engine/decls/decl.h"
#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/containers/blockalloc.h"
#include "shared/idlib/containers/index.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/plane.h"
#include "shared/idlib/math/vector.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>

enum invalidReachability_t { INVALID_REACHABILITY = -1 };
enum invalidAASAnimIndex_t { INVALID_AAS_ANIM = -1 };
enum invalidAASDependencyIndex_t { INVALID_AAS_DEPENDENCY = -1 };
enum invalidAASInteractionEntIndex_t { INVALID_AAS_INTERACTION_ENTITY = -1 };
enum invalidAASTraversalNameIndex_t { INVALID_AAS_TRAVERSAL_NAME = -1 };
enum invalidAASTree_t { INVALID_AAS_TREE = -1 };

using idAAS2ReachabilityIndex = idIndex<short, invalidReachability_t>;
using idAAS2AnimIndex = idIndex<short, invalidAASAnimIndex_t>;
using idAAS2DependencyIndex = idIndex<short, invalidAASDependencyIndex_t>;
using idAAS2InteractionEntIndex =
    idIndex<short, invalidAASInteractionEntIndex_t>;
using idAAS2TraversalNameIndex =
    idIndex<short, invalidAASTraversalNameIndex_t>;
using idAAS2TreeIndex = idIndex<int, invalidAASTree_t>;

struct aas2Trace_t;
struct aas2TraceHeight_t;
struct aas2TraceFloor_t;
class idAASClipLink;
class idClip;
class idClipModel;
class idMat3;
class idAngles;
class usercmd_t;
class idAAS2TravelSpeeds;
class idAAS2Callback;
struct idAAS2NearestReachable;
struct idAAS2Goal;
struct idAAS2Path;
struct idAAS2HopPathParms;
struct idAAS2PathPoint;
struct obstacleVertex_t;
struct obstacleEdge_t;
struct obstacleRoute_t;

struct aas2Name_t {
  char name[128];
  int index;
};

struct aas2AnimName_t {
  char name[128];
};

struct aas2DependencyName_t {
  char name[128];
};

struct aas2InteractionEntityName_t {
  char name[128];
};

struct aas2TraversalEntityName_t {
  char name[128];
};

struct aas2Cover_t {
  std::uint8_t recovered[0x38];
};

struct aas2ChokePoint_t {
  std::uint8_t recovered[0x1C];
};

struct aas2HintNode_t {
  std::uint8_t recovered[0x18];
};

struct aas2AreaBounds_t {
  std::uint8_t recovered[0x0C];
};

struct idQuantizedVec3 {
  std::int16_t x;
  std::int16_t y;
  std::int16_t z;
};

struct aas2Edge_t {
  int vertexNum[2];
  int flags;
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

struct aas2Node_t {
  unsigned int planeNum;
  unsigned int flags;
  int children[2];
};

struct aas2Portal_t {
  std::uint16_t areaNum;
  std::int16_t clusters[2];
  std::int16_t clusterAreaNum[2];
  std::uint16_t maxAreaTravelTime;
};

struct aas2Cluster_t {
  int numAreas;
  int numReachableAreas;
  int numPortals;
  int firstPortal;
};

class idAAS2Settings {
public:
  std::uint8_t recovered[0x130];
};

class idAAS2File : public idResource {
public:
  struct bspTree_t {
    idVec3 floorNormal;
    int headNode;
    int firstArea;
    int lastArea;
  };

  idAAS2File();
  ~idAAS2File() override;
  void LoadResource() override;
  bool ReloadIfStale() override;
  idResourceList* GetResourceList() const override;

  int PointAreaNum(idAAS2TreeIndex tree, const idVec3& origin) const;
  int PointAreaNumMaxheight(idAAS2TreeIndex tree, const idVec3& origin,
                            float maxHeight) const;
  int PointReachableAreaNum(idAAS2TreeIndex tree, const idVec3& origin,
                            int areaFlags, int excludeTravelFlags) const;
  int BoundsReachableAreaNum(idAAS2TreeIndex tree, const idBounds& bounds,
                             int areaFlags, int excludeTravelFlags) const;
  bool PushPointIntoNearestAreaNum(int& areaNum, idVec3& point) const;
  bool PushPointIntoAreaNum(int areaNum, idVec3& point) const;
  idVec3 AreaCenter(int areaNum) const;
  bool Trace(idAAS2TreeIndex tree, aas2Trace_t& trace, const idVec3& start,
             const idVec3& end) const;
  bool TraceHeight(idAAS2TreeIndex tree, aas2TraceHeight_t& trace,
                   const idVec3& start, const idVec3& end) const;
  bool TraceFloor(aas2TraceFloor_t& trace, const idVec3& start,
                  int startAreaNum, const idVec3& end, int endAreaNum,
                  int travelFlags, bool allowFloorNormalChange,
                  bool ignoreGravityDirectionDistance,
                  bool ignoreSameArea) const;
  bool GetAASAnim(idAAS2AnimIndex index,
                  const aas2AnimName_t*& name) const;
  idAAS2AnimIndex GetAASAnimIndexByName(const char* name) const;
  bool GetAASDependency(idAAS2DependencyIndex index,
                        const aas2DependencyName_t*& name) const;
  idAAS2DependencyIndex GetAASDependencyIndexByName(const char* name) const;
  bool GetAASInteractionEntity(
      idAAS2InteractionEntIndex index,
      const aas2InteractionEntityName_t*& name) const;
  idAAS2InteractionEntIndex
  GetAASInteractionEntityIndexByName(const char* name) const;
  bool GetAASTraversalNameIndex(
      idAAS2TraversalNameIndex index,
      const aas2TraversalEntityName_t*& name) const;
  idAAS2TraversalNameIndex
  GetAASTraversalNameIndexByName(const char* name) const;
  int GetAASTraversalIndexByNameIndex(
      idAAS2TraversalNameIndex index) const;
  bool SetTraversalFlag(int index, int flags);
  bool ClearTraversalFlag(int index, int flags);
  bool GetAASTraversalAreas(int index, int& startArea,
                            int& goalArea) const;
  void ClipGridToAreas(const idVec3& startOrigin, int startAreaNum,
                       int travelFlags, int cellSize, int cellsWide,
                       unsigned char* reachable) const;
  idAAS2TreeIndex GetTreeForFloorNormal(const idVec3& floorNormal) const;
  idAAS2TreeIndex GetTreeForArea(int areaNum) const;
  int GetNumAreasInTree(idAAS2TreeIndex treeNum) const;
  std::uint64_t MemorySize() const;
  void ResetCover();

  static idTypedResourceList<idAAS2File> resourceList;

  unsigned int crc;
  unsigned int timestamp;
  idList<int, 37> visitedAreas;
  idAAS2Settings settings;
  int majorVersion;
  int minorVersion;
  int firstFakeVertex;
  int firstFakeEdge;
  int firstFakeEdgeIndex;
  int firstFakeArea;
  bool needsReload;
  std::uint8_t recoveredBeforeTrees[7];
  idList<bspTree_t, 37> trees;
  idList<idPlane, 37> planes;
  idList<idVec3, 37> vertices;
  idList<aas2Edge_t, 37> edges;
  idList<int, 37> edgeIndex;
  idList<aas2Reachability_t, 37> reachabilities;
  idList<aas2Area_t, 37> areas;
  idList<aas2Node_t, 37> nodes;
  idList<aas2Portal_t, 37> portals;
  idList<int, 37> portalIndex;
  idList<aas2Cluster_t, 37> clusters;
  idList<unsigned char, 37> obstaclePVS;
  idList<aas2Name_t, 37> reachabilityNames;
  idList<aas2AnimName_t, 37> animNames;
  idList<aas2DependencyName_t, 37> dependencyNames;
  idList<aas2InteractionEntityName_t, 37> interactionEntityNames;
  idList<aas2TraversalEntityName_t, 37> traversalEntityNames;
  idList<aas2Cover_t, 37> cover;
  idList<int, 37> areaCoverIndex;
  idList<int, 37> touchingCoverIndex;
  idList<aas2ChokePoint_t, 37> chokePoints;
  idList<aas2Traversal_t, 37> traversalPoints;
  idList<aas2HintNode_t, 37> hintNodes;
  idList<aas2AreaBounds_t, 37> areaBounds;
};

class idAAS2 {
public:
  static idAAS2* Alloc();
  virtual ~idAAS2() = 0;
  virtual bool IsDefault() const = 0;
  virtual bool Init(const char* mapName, unsigned int mapFileCRC) = 0;
  virtual void Stats() const = 0;

  virtual bool Verify(idClip* clip) = 0;
  virtual void Test(const idVec3& origin, const idVec3& testOrigin,
                    const idMat3& axis, int startAreaNum, int goalAreaNum,
                    int travelFlags, int walkTravelFlags, int testMode,
                    idClip* clip, usercmd_t& command,
                    idAngles& viewAngles) = 0;
  virtual const idAAS2Settings* GetSettings() const = 0;
  virtual idBounds DefaultSearchBounds() const = 0;
  virtual int PointAreaNum(idAAS2TreeIndex tree,
                           const idVec3& origin) const = 0;
  virtual int PointAreaNumMaxheight(idAAS2TreeIndex tree,
                                    const idVec3& origin,
                                    float maxHeight) const = 0;
  virtual int PointReachableAreaNum(idAAS2TreeIndex tree,
                                    const idVec3& origin,
                                    const idBounds& searchBounds,
                                    int areaFlags,
                                    int excludeTravelFlags) const = 0;
  virtual int BoundsReachableAreaNum(idAAS2TreeIndex tree,
                                     const idBounds& bounds, int areaFlags,
                                     int excludeTravelFlags) const = 0;
  virtual int BoundsReachableAreaNum(idAAS2TreeIndex tree,
                                     const idVec3& origin,
                                     const idBounds& bounds, int areaFlags,
                                     int excludeTravelFlags) const = 0;
  virtual bool NearestPointReachable(
      idAAS2TreeIndex tree, const idVec3& source, int sourceAreaNum,
      const idVec3& destination, const idBounds& destinationBounds,
      int travelFlags, idAAS2NearestReachable& nearestInfo) const = 0;
  virtual bool PushPointIntoAreaNum(int areaNum, idVec3& point) const = 0;
  virtual bool PushPointIntoNearestAreaNum(int& areaNum,
                                           idVec3& point) const = 0;
  virtual idVec3 AreaCenter(int areaNum) const = 0;
  virtual bool Trace(idAAS2TreeIndex tree, aas2Trace_t& trace,
                     const idVec3& start, const idVec3& end) const = 0;
  virtual bool TraceHeight(idAAS2TreeIndex tree, aas2TraceHeight_t& trace,
                           const idVec3& start,
                           const idVec3& end) const = 0;
  virtual bool TraceFloor(aas2TraceFloor_t& trace, const idVec3& start,
                          int startAreaNum, const idVec3& end,
                          int travelFlags, bool allowFloorNormalChange,
                          bool ignoreGravityDirectionDistance,
                          bool ignoreSameArea) const = 0;
  virtual void GetEdgeVertexNumbers(int edgeNum, int* verts) const = 0;
  virtual void GetEdge(int edgeNum, idVec3& start, idVec3& end) const = 0;
  virtual int GetEdgeFlags(int edgeNum) const = 0;
  virtual int GetAreaFlags(int areaNum) const = 0;
  virtual bool ChangeAreaTravelFlags(const idBounds& bounds, int areaFlags,
                                     int travelFlags, bool set,
                                     const char* debugName) = 0;
  virtual bool ChangeReachabilityTravelFlags(const char* name,
                                             int travelFlag, bool set) = 0;
  virtual bool ChangeReachabilityTravelFlags(idAAS2ReachabilityIndex index,
                                             int travelFlag, bool set) = 0;
  virtual int TravelTimeToGoalArea(
      int startAreaNum, const idVec3& startOrigin, int goalAreaNum,
      const idVec3& goalOrigin, int travelFlags,
      const idAAS2TravelSpeeds& speeds) const = 0;
  virtual bool RouteToGoalArea(int startAreaNum, const idVec3& startOrigin,
                               int goalAreaNum, const idVec3& goalOrigin,
                               int travelFlags, int& travelTime,
                               idAAS2ReachabilityIndex& reachIndex) const = 0;
  virtual bool FindNearestGoal(idAAS2Goal& goal, int startAreaNum,
                               const idVec3& startOrigin, int travelFlags,
                               idAAS2Callback& callback) const = 0;
  virtual bool WalkPathToGoal(idAAS2Path& path, int startAreaNum,
                              const idVec3& startOrigin, int goalAreaNum,
                              const idVec3& goalOrigin, int travelFlags,
                              int walkTravelFlags) const = 0;
  virtual bool ExtendHopPathToGoal(
      idAAS2Path& path, int startAreaNum, const idVec3& startOrigin,
      int goalAreaNum, const idVec3& goalOrigin, int travelFlags,
      int walkTravelFlags, const idAAS2HopPathParms& parms) const = 0;
  virtual void ShowWalkPath(int startAreaNum, const idVec3& startOrigin,
                            int goalAreaNum, const idVec3& goalOrigin,
                            int travelFlags, int walkTravelFlags,
                            int stopEvent, int drawFlags,
                            int maxAreas) const = 0;
  virtual void ShowHopPath(int startAreaNum, const idVec3& startOrigin,
                           int goalAreaNum, const idVec3& goalOrigin,
                           int travelFlags, int walkTravelFlags,
                           const idAAS2HopPathParms& parms) const = 0;
  virtual void DrawEdge(int edgeNum, bool arrow, bool text,
                        bool highlight) const = 0;
  virtual void DrawReachability(idAAS2ReachabilityIndex reachIndex,
                                const aas2Reachability_t* reachability,
                                const char* text) const = 0;
  virtual void DrawArea(int areaNum, bool showNumbers, bool showReach,
                        bool showBounds, bool showClusters, bool showFlags,
                        bool showNormals) const = 0;
  virtual float GetGroundSpeedMultiplier() const = 0;
  virtual bool GetOptimizedWalkPath(
      int startAreaNum, const idVec3& startOrigin, int goalAreaNum,
      const idVec3& goalOrigin, int travelFlags, int walkTravelFlags,
      idList<idVec3, 5>& path) const = 0;
  virtual int GetNumAreas() const = 0;
  virtual const idPlane& GetPlane(int index) const = 0;
  virtual const aas2Area_t& GetArea(int areaNum) const = 0;
  virtual const aas2Reachability_t&
  GetReachability(idAAS2ReachabilityIndex reachNum) const = 0;
  virtual bool GetPath(const idVec3& startPos, const idVec3& goalPos,
                       int startAreaNum, int goalAreaNum, int travelFlags,
                       idAAS2PathPoint* pointList, int maxPoints,
                       int& numPoints) const = 0;
  virtual void ClipGridToAreas(const idVec3& startOrigin, int startAreaNum,
                               int travelFlags, int cellSize, int cellsWide,
                               unsigned char* reachable) const = 0;
  virtual int GetInvalidTravelFlags() const = 0;
  virtual int GetTraversalsForReachability(
      idAAS2ReachabilityIndex reachIndex,
      idList<int, 5>& traversalList) const = 0;
  virtual int GetTraversalsForInteractableEntity(
      idAAS2InteractionEntIndex interactionEntIndex,
      idList<int, 5>& traversalList) const = 0;
  virtual int FindBestTraversalForReachability(
      idAAS2ReachabilityIndex reachIndex, const idVec3& currentPoint,
      const idVec3& goalPoint, idVec3& traversalStart,
      idVec3& traversalEnd, idAAS2DependencyIndex& dependencyIndex,
      int travelFlags, bool useClosestPointOnFail) const = 0;
  virtual const aas2Traversal_t& GetTraversal(int traversalNum) const = 0;
  virtual bool GetAASAnim(idAAS2AnimIndex index,
                          const aas2AnimName_t*& name) const = 0;
  virtual idAAS2AnimIndex GetAASAnimIndexByName(const char* name) const = 0;
  virtual bool GetAASDependency(
      idAAS2DependencyIndex index,
      const aas2DependencyName_t*& name) const = 0;
  virtual idAAS2DependencyIndex
  GetAASDependencyIndexByName(const char* name) const = 0;
  virtual bool GetAASInteractionEntity(
      idAAS2InteractionEntIndex index,
      const aas2InteractionEntityName_t*& name) const = 0;
  virtual idAAS2InteractionEntIndex
  GetAASInteractionEntityIndexByName(const char* name) const = 0;
  virtual bool GetAASTraversalNameIndex(
      idAAS2TraversalNameIndex index,
      const aas2TraversalEntityName_t*& name) const = 0;
  virtual idAAS2TraversalNameIndex
  GetAASTraversalNameIndexByName(const char* name) const = 0;
  virtual int GetAASTraversalIndexByNameIndex(
      idAAS2TraversalNameIndex index) const = 0;
  virtual bool SetTraversalFlag(int index, int flags) = 0;
  virtual bool ClearTraversalFlag(int index, int flags) = 0;
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
  virtual int GetNumCoverPoints() const = 0;
  virtual const aas2Cover_t& GetCoverPoint(int coverIndex) const = 0;
  virtual int GetNumAreaCoverPoints(int areaNum) const = 0;
  virtual int GetAreaCoverPoints(int areaNum, int* coverPoints,
                                 int maxCover, int& numCover) const = 0;
  virtual int FindCoverPoints(int areaNum, const idVec3& origin,
                              float maxDist, int requiredFlags,
                              int excludeFlags, int travelFlags,
                              int* coverPoints, int maxCover,
                              int& numCover) const = 0;
  virtual int GetNumChokePoints() const = 0;
  virtual const aas2ChokePoint_t& GetChokePoint(int chokeIndex) const = 0;
  virtual int GetNumHintNodes() const = 0;
  virtual const aas2HintNode_t& GetHintNode(int hintIndex) const = 0;
  virtual bool GetAreaCells(int areaNum, int subSize, int& subX,
                            int& subY) const = 0;
  virtual bool WorldPosToCellPos(int areaNum, int subSize,
                                 const idVec3& origin, int& subX,
                                 int& subY) const = 0;
  virtual bool CellPosToWorldPos(int areaNum, int subSize, int subX,
                                 int subY, idVec3& worldPos,
                                 bool center) const = 0;
  virtual void DrawCoverPoint(const aas2Cover_t* cover,
                              int flags) const = 0;
  virtual int GetNumTrees() const = 0;
  virtual idAAS2TreeIndex GetTree(int index) const = 0;
  virtual idAAS2TreeIndex
  GetTreeForFloorNormal(const idVec3& floorNormal) const = 0;
  virtual const idVec3& GetFloorNormalForArea(int areaNum) const = 0;
  virtual const idVec3&
  GetFloorNormalForTree(idAAS2TreeIndex treeNum) const = 0;
  virtual int GetNumAreasInTree(idAAS2TreeIndex treeNum) const = 0;
protected:
  idAAS2() = default;
};

class idAASClipLink {
public:
  idAASClipLink* prevArea;
  idAASClipLink* nextArea;
  idAASClipLink* prevClip;
  idAASClipLink* nextClip;
  void* clipModel;
  int areaNum;
};

struct idObstacleAvoidance {
  idObstacleAvoidance();
  ~idObstacleAvoidance();

  void* buffers;
  idVec3 lastDir;
  idVec4 lastCorner;
  int lastUpdateTime;
  int firstUpdateTime;
};

struct idAAS2VerifyState {
  int currentIndex;
  int recoveredCount;
  void* invalidVertices;
  void* clipModel;
};

class idAASRoutingCache;
class idRoutingUpdate;

class idAAS2Local : public idAAS2 {
public:
  __declspec(noinline) idAAS2Local();
  __forceinline ~idAAS2Local() override { Shutdown(); }
  static __forceinline void operator delete(void* memory) noexcept {
    idMem::Singleton().Free(memory, ALIGN_16);
  }

  bool IsDefault() const override;
  bool Init(const char* mapName, unsigned int mapFileCRC) override;
  void Stats() const override;
  bool Verify(idClip* clip) override;
  void Test(const idVec3& origin, const idVec3& testOrigin,
            const idMat3& axis, int startAreaNum, int goalAreaNum,
            int travelFlags, int walkTravelFlags, int testMode,
            idClip* clip, usercmd_t& command,
            idAngles& viewAngles) override;
  const idAAS2Settings* GetSettings() const override;
  idBounds DefaultSearchBounds() const override;
  int PointAreaNum(idAAS2TreeIndex tree,
                   const idVec3& origin) const override;
  int PointAreaNumMaxheight(idAAS2TreeIndex tree, const idVec3& origin,
                            float maxHeight) const override;
  int PointReachableAreaNum(idAAS2TreeIndex tree, const idVec3& origin,
                            const idBounds& searchBounds, int areaFlags,
                            int excludeTravelFlags) const override;
  int BoundsReachableAreaNum(idAAS2TreeIndex tree, const idBounds& bounds,
                             int areaFlags,
                             int excludeTravelFlags) const override;
  int BoundsReachableAreaNum(idAAS2TreeIndex tree, const idVec3& origin,
                             const idBounds& bounds, int areaFlags,
                             int excludeTravelFlags) const override;
  bool NearestPointReachable(
      idAAS2TreeIndex tree, const idVec3& source, int sourceAreaNum,
      const idVec3& destination, const idBounds& destinationBounds,
      int travelFlags,
      idAAS2NearestReachable& nearestInfo) const override;
  bool PushPointIntoAreaNum(int areaNum, idVec3& point) const override;
  bool PushPointIntoNearestAreaNum(int& areaNum,
                                   idVec3& point) const override;
  idVec3 AreaCenter(int areaNum) const override;
  bool Trace(idAAS2TreeIndex tree, aas2Trace_t& trace, const idVec3& start,
             const idVec3& end) const override;
  bool TraceHeight(idAAS2TreeIndex tree, aas2TraceHeight_t& trace,
                   const idVec3& start, const idVec3& end) const override;
  bool TraceFloor(aas2TraceFloor_t& trace, const idVec3& start,
                  int startAreaNum, const idVec3& end, int travelFlags,
                  bool allowFloorNormalChange,
                  bool ignoreGravityDirectionDistance,
                  bool ignoreSameArea) const override;
  void GetEdgeVertexNumbers(int edgeNum, int* verts) const override;
  void GetEdge(int edgeNum, idVec3& start, idVec3& end) const override;
  int GetEdgeFlags(int edgeNum) const override;
  int GetAreaFlags(int areaNum) const override;
  bool ChangeAreaTravelFlags(const idBounds& bounds, int areaFlags,
                             int travelFlags, bool set,
                             const char* debugName) override;
  bool ChangeReachabilityTravelFlags(const char* name, int travelFlag,
                                     bool set) override;
  bool ChangeReachabilityTravelFlags(idAAS2ReachabilityIndex index,
                                     int travelFlag, bool set) override;
  int TravelTimeToGoalArea(
      int startAreaNum, const idVec3& startOrigin, int goalAreaNum,
      const idVec3& goalOrigin, int travelFlags,
      const idAAS2TravelSpeeds& speeds) const override;
  bool RouteToGoalArea(int startAreaNum, const idVec3& startOrigin,
                       int goalAreaNum, const idVec3& goalOrigin,
                       int travelFlags, int& travelTime,
                       idAAS2ReachabilityIndex& reachIndex) const override;
  bool FindNearestGoal(idAAS2Goal& goal, int startAreaNum,
                       const idVec3& startOrigin, int travelFlags,
                       idAAS2Callback& callback) const override;
  bool WalkPathToGoal(idAAS2Path& path, int startAreaNum,
                      const idVec3& startOrigin, int goalAreaNum,
                      const idVec3& goalOrigin, int travelFlags,
                      int walkTravelFlags) const override;
  bool ExtendHopPathToGoal(
      idAAS2Path& path, int startAreaNum, const idVec3& startOrigin,
      int goalAreaNum, const idVec3& goalOrigin, int travelFlags,
      int walkTravelFlags,
      const idAAS2HopPathParms& parms) const override;
  void ShowWalkPath(int startAreaNum, const idVec3& startOrigin,
                    int goalAreaNum, const idVec3& goalOrigin,
                    int travelFlags, int walkTravelFlags, int stopEvent,
                    int drawFlags, int maxAreas) const override;
  void ShowHopPath(int startAreaNum, const idVec3& startOrigin,
                   int goalAreaNum, const idVec3& goalOrigin,
                   int travelFlags, int walkTravelFlags,
                   const idAAS2HopPathParms& parms) const override;
  void DrawEdge(int edgeNum, bool arrow, bool text,
                bool highlight) const override;
  void DrawReachability(idAAS2ReachabilityIndex reachIndex,
                        const aas2Reachability_t* reachability,
                        const char* text) const override;
  void DrawArea(int areaNum, bool showNumbers, bool showReach,
                bool showBounds, bool showClusters, bool showFlags,
                bool showNormals) const override;
  float GetGroundSpeedMultiplier() const override;
  bool GetOptimizedWalkPath(
      int startAreaNum, const idVec3& startOrigin, int goalAreaNum,
      const idVec3& goalOrigin, int travelFlags, int walkTravelFlags,
      idList<idVec3, 5>& path) const override;
  int GetNumAreas() const override;
  const idPlane& GetPlane(int index) const override;
  const aas2Area_t& GetArea(int areaNum) const override;
  const aas2Reachability_t&
  GetReachability(idAAS2ReachabilityIndex reachNum) const override;
  bool GetPath(const idVec3& startPos, const idVec3& goalPos,
               int startAreaNum, int goalAreaNum, int travelFlags,
               idAAS2PathPoint* pointList, int maxPoints,
               int& numPoints) const override;
  void ClipGridToAreas(const idVec3& startOrigin, int startAreaNum,
                       int travelFlags, int cellSize, int cellsWide,
                       unsigned char* reachable) const override;
  int GetInvalidTravelFlags() const override;
  int GetTraversalsForReachability(
      idAAS2ReachabilityIndex reachIndex,
      idList<int, 5>& traversalList) const override;
  int GetTraversalsForInteractableEntity(
      idAAS2InteractionEntIndex interactionEntIndex,
      idList<int, 5>& traversalList) const override;
  int FindBestTraversalForReachability(
      idAAS2ReachabilityIndex reachIndex, const idVec3& currentPoint,
      const idVec3& goalPoint, idVec3& traversalStart,
      idVec3& traversalEnd, idAAS2DependencyIndex& dependencyIndex,
      int travelFlags, bool useClosestPointOnFail) const override;
  const aas2Traversal_t& GetTraversal(int traversalNum) const override;
  bool GetAASAnim(idAAS2AnimIndex index,
                  const aas2AnimName_t*& name) const override;
  idAAS2AnimIndex GetAASAnimIndexByName(const char* name) const override;
  bool GetAASDependency(idAAS2DependencyIndex index,
                        const aas2DependencyName_t*& name) const override;
  idAAS2DependencyIndex
  GetAASDependencyIndexByName(const char* name) const override;
  bool GetAASInteractionEntity(
      idAAS2InteractionEntIndex index,
      const aas2InteractionEntityName_t*& name) const override;
  idAAS2InteractionEntIndex
  GetAASInteractionEntityIndexByName(const char* name) const override;
  bool GetAASTraversalNameIndex(
      idAAS2TraversalNameIndex index,
      const aas2TraversalEntityName_t*& name) const override;
  idAAS2TraversalNameIndex
  GetAASTraversalNameIndexByName(const char* name) const override;
  int GetAASTraversalIndexByNameIndex(
      idAAS2TraversalNameIndex index) const override;
  bool SetTraversalFlag(int index, int flags) override;
  bool ClearTraversalFlag(int index, int flags) override;
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
  int GetNumCoverPoints() const override;
  const aas2Cover_t& GetCoverPoint(int coverIndex) const override;
  int GetNumAreaCoverPoints(int areaNum) const override;
  int GetAreaCoverPoints(int areaNum, int* coverPoints, int maxCover,
                         int& numCover) const override;
  int FindCoverPoints(int areaNum, const idVec3& origin, float maxDist,
                      int requiredFlags, int excludeFlags, int travelFlags,
                      int* coverPoints, int maxCover,
                      int& numCover) const override;
  int GetNumChokePoints() const override;
  const aas2ChokePoint_t& GetChokePoint(int chokeIndex) const override;
  int GetNumHintNodes() const override;
  const aas2HintNode_t& GetHintNode(int hintIndex) const override;
  bool GetAreaCells(int areaNum, int subSize, int& subX,
                    int& subY) const override;
  bool WorldPosToCellPos(int areaNum, int subSize, const idVec3& origin,
                         int& subX, int& subY) const override;
  bool CellPosToWorldPos(int areaNum, int subSize, int subX, int subY,
                         idVec3& worldPos, bool center) const override;
  void DrawCoverPoint(const aas2Cover_t* cover,
                      int flags) const override;
  int GetNumTrees() const override;
  idAAS2TreeIndex GetTree(int index) const override;
  idAAS2TreeIndex
  GetTreeForFloorNormal(const idVec3& floorNormal) const override;
  const idVec3& GetFloorNormalForArea(int areaNum) const override;
  const idVec3&
  GetFloorNormalForTree(idAAS2TreeIndex treeNum) const override;
  int GetNumAreasInTree(idAAS2TreeIndex treeNum) const override;

  __declspec(noinline) virtual void Shutdown();
  virtual void ShowOptimizedWalkPath(
      int startAreaNum, const idVec3& startOrigin, int goalAreaNum,
      const idVec3& goalOrigin, int travelFlags,
      int walkTravelFlags) const;

  idAAS2File* file;
  idStr name;
  bool defaulted;
  std::uint8_t recoveredBeforeVerify[7];
  idAAS2VerifyState verify;
  mutable idAASRoutingCache** areaCacheIndex;
  int areaCacheIndexSize;
  mutable idAASRoutingCache** portalCacheIndex;
  int portalCacheIndexSize;
  idRoutingUpdate* areaUpdate;
  idRoutingUpdate* portalUpdate;
  unsigned short* goalAreaTravelTimes;
  unsigned short* areaTravelTimes;
  int numAreaTravelTimes;
  mutable idAASRoutingCache* cacheListStart;
  mutable idAASRoutingCache* cacheListEnd;
  mutable int totalCacheMemory;
  float groundSpeedMultiplier;
  float waterSpeedMultiplier;
  int numObstaclePVSBytes;
  unsigned char* obstaclePVS;
  mutable int obstaclePVSAreaNum;
  std::uint8_t recoveredBeforeClipModels[4];
  idAASClipLink** areaClipModels;
  idBlockAlloc<idAASClipLink, 128, 37> clipModelLinkAllocator;
  idObstacleAvoidance obstacleAvoidance;

private:
  bool SetupRouting();
  void ShutdownRouting();
  void RemoveRoutingCacheUsingArea(int areaNum) const;
  void RoutingStats() const;
  void SetupObstaclePVS();
  void ShutdownObstaclePVS();
};

static_assert(sizeof(idAAS2Settings) == 0x130,
              "retail AAS settings ABI drift");
static_assert(sizeof(aas2Name_t) == 0x84,
              "retail AAS indexed-name ABI drift");
static_assert(sizeof(aas2AnimName_t) == 0x80,
              "retail AAS animation-name ABI drift");
static_assert(sizeof(aas2DependencyName_t) == 0x80,
              "retail AAS dependency-name ABI drift");
static_assert(sizeof(aas2InteractionEntityName_t) == 0x80,
              "retail AAS interaction-name ABI drift");
static_assert(sizeof(aas2TraversalEntityName_t) == 0x80,
              "retail AAS traversal-name ABI drift");
static_assert(sizeof(aas2Cover_t) == 0x38,
              "retail AAS cover ABI drift");
static_assert(sizeof(aas2ChokePoint_t) == 0x1C,
              "retail AAS choke-point ABI drift");
static_assert(sizeof(aas2HintNode_t) == 0x18,
              "retail AAS hint-node ABI drift");
static_assert(sizeof(aas2AreaBounds_t) == 0x0C,
              "retail AAS area-bounds ABI drift");
static_assert(sizeof(aas2Edge_t) == 0x0C, "retail AAS edge ABI drift");
static_assert(sizeof(aas2Reachability_t) == 0x20,
              "retail AAS reachability ABI drift");
static_assert(sizeof(aas2Area_t) == 0x28, "retail AAS area ABI drift");
static_assert(sizeof(aas2Traversal_t) == 0x38,
              "retail AAS traversal ABI drift");
static_assert(sizeof(idAAS2File::bspTree_t) == 0x18,
              "retail AAS tree ABI drift");
static_assert(offsetof(idAAS2File, settings) == 0x50,
              "retail AAS settings offset drift");
static_assert(offsetof(idAAS2File, trees) == 0x1A0,
              "retail AAS tree-list offset drift");
static_assert(offsetof(idAAS2File, planes) == 0x1B8,
              "retail AAS plane-list offset drift");
static_assert(offsetof(idAAS2File, vertices) == 0x1D0,
              "retail AAS vertex-list offset drift");
static_assert(offsetof(idAAS2File, edges) == 0x1E8,
              "retail AAS edge-list offset drift");
static_assert(offsetof(idAAS2File, reachabilities) == 0x218,
              "retail AAS reachability-list offset drift");
static_assert(offsetof(idAAS2File, areas) == 0x230,
              "retail AAS area-list offset drift");
static_assert(offsetof(idAAS2File, traversalPoints) == 0x398,
              "retail AAS traversal-list offset drift");
static_assert(sizeof(idAAS2File) == 0x3E0,
              "retail AAS file ABI drift");
static_assert(sizeof(idAASClipLink) == 0x30,
              "retail AAS clip-link ABI drift");
static_assert(sizeof(idObstacleAvoidance) == 0x30,
              "retail obstacle-avoidance ABI drift");
static_assert(sizeof(idAAS2VerifyState) == 0x18,
              "retail AAS verify state ABI drift");
static_assert(offsetof(idAAS2Local, file) == 0x08,
              "retail local AAS file offset drift");
static_assert(offsetof(idAAS2Local, verify) == 0x40,
              "retail local AAS verify offset drift");
static_assert(offsetof(idAAS2Local, areaCacheIndex) == 0x58,
              "retail local AAS cache offset drift");
static_assert(offsetof(idAAS2Local, groundSpeedMultiplier) == 0xB4,
              "retail local AAS ground-speed offset drift");
static_assert(offsetof(idAAS2Local, obstaclePVS) == 0xC0,
              "retail local AAS obstacle-PVS offset drift");
static_assert(offsetof(idAAS2Local, clipModelLinkAllocator) == 0xD8,
              "retail local AAS allocator offset drift");
static_assert(offsetof(idAAS2Local, obstacleAvoidance) == 0xF8,
              "retail local AAS obstacle-avoidance offset drift");
static_assert(sizeof(idAAS2Local) == 0x128,
              "retail local AAS ABI drift");
