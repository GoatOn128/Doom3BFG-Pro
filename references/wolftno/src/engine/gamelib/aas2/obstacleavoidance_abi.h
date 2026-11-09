#pragma once

#include "engine/gamelib/idspawnid_abi.h"
#if !defined(WOLFTNO_RETAIL_IDBOX_CLASS_ABI)
#define WOLFTNO_RETAIL_IDBOX_CLASS_ABI
#define WOLFTNO_OBSTACLEAVOIDANCE_OWNS_IDBOX_CLASS_ABI
#endif
#include "shared/idlib/bv/box.h"
#if defined(WOLFTNO_OBSTACLEAVOIDANCE_OWNS_IDBOX_CLASS_ABI)
#undef WOLFTNO_OBSTACLEAVOIDANCE_OWNS_IDBOX_CLASS_ABI
#undef WOLFTNO_RETAIL_IDBOX_CLASS_ABI
#endif
#include "shared/idlib/math/plane.h"

#include <cstddef>
#include <cstdint>

class idParallelJobList;
struct CellSpursJob128;

struct obstacleVertex_t {
  idVec3 position;
};

struct obstacleEdge_t {
  int flags;
  std::int16_t verts[2];
  int vertNums[2];
};

struct obstacleCorner_t {
  idVec3 corner;
  float radius;
};

struct obstacleTrace_t {
  idVec2 start;
  idVec2 end;
};

struct obstacleBox_t {
  idMat3 axis;
  idVec3 center;
  idVec3 extents;
  idSpawnId id;
  bool soft;
  std::uint8_t pad[15];
};

struct obstacleRoute_t {
  struct routeArea_t {
    int areaNum;
    idVec3 start;
    idVec3 end;
  };

  obstacleRoute_t& operator=(const obstacleRoute_t& other);

  int numAreas;
  int pad[2];
  idVec3 endAlignDir;
  float endTurnRadius;
  routeArea_t areas[11];
};

struct obstaclePath_t {
  struct radar_t {
    idVec3 direction;
    int obstacle;
  };

  obstaclePath_t();
  obstaclePath_t& operator=(const obstaclePath_t& other);

  idVec3 seekPos[31];
  int numSeekPos;
  idPlane seekPosPlane;
  idVec3 firstObstaclePosition;
  int firstObstacle;
  idVec3 startPosOutsideObstacles;
  int startPosObstacle;
  idVec3 seekPosOutsideObstacles;
  int seekPosObstacle;
  float targetDist;
  float pathLength;
  idVec4 nextCorner;
  float nextCornerAngle;
  idVec3 wallCorners[2];
  radar_t radar[7];
  bool useRadarForward;
  std::uint8_t radarPadding[3];
  idVec3 radarForward;
  bool hasValidPath;
  bool startPosValid;
  bool seekPosValid;
  bool done;
};

struct obstacleTraceSet_t {
  obstacleTrace_t traces[256];
  float traceFractions[256];
  int traceNumbers[256];
  int numTraces;
  int prevTraceNumbers[256];
  int prevNumTraces;
};

class idMat3x4 {
public:
  float mat[12];

  __forceinline void Identity() noexcept {
    mat[0] = 1.0f;
    mat[1] = 0.0f;
    mat[2] = 0.0f;
    mat[3] = 0.0f;
    mat[4] = 0.0f;
    mat[5] = 1.0f;
    mat[6] = 0.0f;
    mat[7] = 0.0f;
    mat[8] = 0.0f;
    mat[9] = 0.0f;
    mat[10] = 1.0f;
    mat[11] = 0.0f;
  }

  __forceinline void Transform(idVec3& output,
                               const idVec3& input) const noexcept {
    output = {
        ((mat[0] * input.x + mat[1] * input.y) + mat[2] * input.z) +
            mat[3],
        ((mat[4] * input.x + mat[5] * input.y) + mat[6] * input.z) +
            mat[7],
        ((mat[8] * input.x + mat[9] * input.y) + mat[10] * input.z) +
            mat[11],
    };
  }

  __forceinline void UnTransform(idVec3& output,
                                 const idVec3& input) const noexcept {
    const idVec3 translated{input.x - mat[3], input.y - mat[7],
                            input.z - mat[11]};
    output = {
        (mat[0] * translated.x + mat[4] * translated.y) +
            mat[8] * translated.z,
        (mat[1] * translated.x + mat[5] * translated.y) +
            mat[9] * translated.z,
        (mat[2] * translated.x + mat[6] * translated.y) +
            mat[10] * translated.z,
    };
  }
};

class idObstacleAvoidanceCore {
public:
  enum avoidColor_t : int {
    COLOR_INVALID,
    COLOR_WHITE,
    COLOR_LIGHT_GREY,
    COLOR_RED,
    COLOR_BLUE,
    COLOR_GREEN,
    COLOR_YELLOW,
    COLOR_ORANGE,
    COLOR_PURPLE,
    COLOR_CYAN,
    COLOR_MAGENTA,
  };

  idObstacleAvoidanceCore();
  ~idObstacleAvoidanceCore();

  void FindPathAroundObstacles(
      obstaclePath_t* path, struct avoidDebugLine_t* debugLines,
      int maxDebugLines, struct avoidDebugText_t* debugText,
      int maxDebugText, void* tempBuffer, int tempBufferSize,
      const idBounds& bbox, const idVec3& gravity, const idVec3& lastDir,
      const idVec4& lastCorner, float obstacleRadius, float frameMoveDist,
      float cornerCircleRadius, int flags, const obstacleRoute_t* route,
      const obstacleBox_t* obstacles, int numObstacles,
      const obstacleVertex_t* vertices, int numVertices,
      const obstacleEdge_t* edges, int numEdges,
      const obstacleCorner_t* corners, int numCorners,
      const obstacleTrace_t* traces, int numTraces, float* traceFractions);

private:
  struct bspNode_t {
    int axis;
    float dist;
    std::int16_t children[2];
    std::int16_t indexOffset;
    std::int16_t numIndices;
  };

  struct expandedObstacle_t {
    idVec2 bounds[2];
    idVec2* winding;
    int numPoints;
    int id;
    bool soft;
    std::uint8_t pad[3];
  };

  struct pathNode_t {
    int dir;
    idVec2 pos;
    idVec2 delta;
    float targetDistSqr;
    int obstacle;
    int edgeNum;
    int numNodes;
    std::uint32_t parentPadding;
    pathNode_t* parent;
    pathNode_t* children[2];
    int queueNext;
    std::uint32_t tailPadding;

    __forceinline void Init() noexcept {
      dir = 0;
      pos = {0.0f, 0.0f};
      delta = {0.0f, 0.0f};
      targetDistSqr = 0.0f;
      obstacle = -1;
      edgeNum = -1;
      numNodes = 0;
      parent = nullptr;
      children[0] = nullptr;
      children[1] = nullptr;
      queueNext = -1;
    }
  };

  static int BoxParallelProjectionSilhouetteVerts(
      const idMat3& axis, const idVec3& center, const idVec3& extents,
      const idVec3& projectionDir, idVec3* silVerts);
  static void GetCirclePoints(const idVec2& center, float radius,
                              idVec2* circlePoints);
  static bool Plane2DIntersection(const idVec3& plane1,
                                  const idVec3& plane2, idVec2& point);
  static bool PointInsideWinding(const idVec2* winding, int numPoints,
                                 const idVec2& point, float epsilon);
  static void ExpandWinding(const idVec2* winding, int numPoints, float d,
                            idVec2* expanded);
  static int ExpandWindingForAxialBox(idVec2* winding, int numPoints,
                                      const idVec2* bounds);
  static void GetWindingBounds(const idVec2* winding, int numPoints,
                               idVec2* bounds);
  static bool WindingRayIntersection(const idVec2* winding, int numPoints,
                                     const idVec2& start,
                                     const idVec2& dir, float& scale1,
                                     float& scale2, int* edgeNums);

  bool LineIntersectsPath(const idVec2& start, const idVec2& end,
                          const pathNode_t* node) const;
  bool LineIntersectsEdgeSequence(const idVec2& start, const idVec2& end,
                                  const obstacleVertex_t* vertices,
                                  const obstacleEdge_t* edges,
                                  const std::int16_t* firstEdge,
                                  const std::int16_t* lastEdge,
                                  int numEdges) const;
  int PointInsideObstacle(const idVec2& point) const;
  int PointInsideObstacleList(const int* obstacleList, int numObstacles,
                              const idVec2& point) const;
  bool LineIntersectsWall(const idVec2& start, const idVec2& end) const;
  bool LineIntersectsObstacle(const idVec2& start, const idVec2& end,
                              int obstacle, int skipEdge) const;
  bool GetFirstBlockingObstacle(const idVec2& start, const idVec2& end,
                                int skipObstacle, float& blockingScale,
                                int& blockingObstacle, int& blockingEdgeNum);
  bool GetPointOutsideObstacles(idVec2& point, int* obstacleList,
                                int* numObstacles, const idVec2& direction,
                                bool backAway);
  int GetBspNodesForLine_r(const idVec2& start, const idVec2& end,
                           int nodeNum, int maxNodes, int* nodes) const;
  std::int16_t SetupBspTree_r(const idVec2& start, const idVec2& end,
                              int depth, int parentNodeNum);
  void FilterObstacleIntoBspTree(std::int16_t nodeNum, int obstacleNum,
                                 std::int16_t* numObstacleIndices);
  void SetupWallOrLedgeWinding(expandedObstacle_t& obstacle, int firstEdge,
                               int numEdges, int edgeFlags,
                               const obstacleEdge_t* edges,
                               const obstacleVertex_t* vertices);
  bool SetupExpandedObstacles(
      const idBounds& bbox, const idVec3& gravity, const idVec3& startPos,
      const idVec3& seekPos, float obstacleRadius, bool outsideObstacles,
      const obstacleBox_t* obstacles, int numObstacles,
      const obstacleVertex_t* vertices, int numVertices,
      const obstacleEdge_t* edges, int numEdges,
      const obstacleCorner_t* corners, int numCorners, bool useBsp);
  bool GetPathNodeDelta(pathNode_t* node, const idVec2& seekPos,
                        const idBounds& bounds, bool blocked);
  pathNode_t* BuildPathTree(const idVec3& startPos, const idVec3& seekPos,
                            float maxPath, obstaclePath_t* path);
  void PrunePathTree(pathNode_t* root, const idVec2& seekPos);
  float OptimizePath(const pathNode_t* root, const pathNode_t* leaf,
                     const idVec2& startPos, const idVec2& seekPos,
                     idVec3* optimizedPath, int& numPathPoints) const;
  void FindOptimalPath(const pathNode_t* root, const idVec2& startPos,
                       const idVec2& seekPos, idVec3* optimizedPath,
                       int& numPathPoints, float& targetDist,
                       float& pathLength);
  void SubSamplePath(const idVec3& startPos, const idVec3& p1,
                     const idVec3& p2, idVec3& result);
  bool FindSeekPosForRoute(const obstacleRoute_t* route, float radius,
                           const idVec3& startPos, idVec3& seekPos,
                           idVec3* wallCorners, int& numWallCorners);
  void GetSeekPointsOnPath(const idVec2* path, int numPathPoints,
                           const idVec2& startPos, float maxSeekDistance,
                           float nextSeekDistance, idVec3* seekPos,
                           int& numSeekPos, bool skipFirstPoint);
  bool PathAroundCircle(const idVec2& startPos, const idVec2& seekPos,
                        const idVec2& circleOrigin,
                        const idVec2* circlePoints, bool clockwise,
                        idVec2* path, int& numPathPoints);
  void CircleCorners(obstaclePath_t* path, const idVec4& lastCorner,
                     const idBounds& bbox, float obstacleRadius,
                     float cornerCircleRadius);
  void CircleApproachDestination(obstaclePath_t* path,
                                 const obstacleRoute_t* route,
                                 float cornerCircleRadius);
  void UpdateRadar(obstaclePath_t* path, float obstacleRadius);
  void TransformInput(const idVec3& gravity, obstacleRoute_t* route,
                      idVec3& lastDir, idVec4& lastCorner,
                      obstacleBox_t* obstacles, int numObstacles,
                      obstacleVertex_t* vertices, int numVertices,
                      obstacleCorner_t* corners, int numCorners,
                      obstacleTrace_t* traces, int numTraces);
  void UnTransformOutput(obstaclePath_t* path);

  bspNode_t* bspNodes;
  int numBspNodes;
  std::uint32_t bspPadding;
  std::int16_t* obstacleIndices;
  expandedObstacle_t* expandedObstacles;
  int maxExpandedObstacles;
  int numExpandedObstacles;
  idVec2* windingPoints;
  int maxWindingPoints;
  int numWindingPoints;
  std::uint8_t* tempMemory;
  int tempMemorySize;
  std::uint32_t tempPadding;
  std::uint8_t* tempMemoryStart;
  std::uint8_t* tempMemoryEnd;
  pathNode_t* pathNodes;
  int maxPathNodes;
  int numPathNodes;
  struct avoidDebugLine_t* debugLines;
  int maxDebugLines;
  int numDebugLines;
  struct avoidDebugText_t* debugText;
  int maxDebugText;
  int numDebugText;
  bool inputTransformValid;
  std::uint8_t inputPadding[3];
  idMat3x4 inputTransform;
  int flags;
};

struct avoidDebugLine_t {
  idObstacleAvoidanceCore::avoidColor_t color;
  idVec3 start;
  idVec3 end;
};

struct avoidDebugText_t {
  char text[32];
  idVec3 pos;
  idObstacleAvoidanceCore::avoidColor_t color;
};

struct obstacleParms_t {
  idBounds bbox;
  idVec3 gravity;
  idVec3 lastDir;
  idVec4 lastCorner;
  float obstacleRadius;
  float frameMoveDist;
  float cornerCircleRadius;
  int flags;
  int numObstacles;
  int numVertices;
  int numEdges;
  int numCorners;
  int numTraces;
  int maxDebugLines;
  int maxDebugText;
  int pointerPadding;
  const obstacleRoute_t* route;
  const obstacleBox_t* obstacles;
  const obstacleVertex_t* vertices;
  const obstacleEdge_t* edges;
  const obstacleCorner_t* corners;
  const obstacleTrace_t* traces;
  float* traceFractions;
  obstaclePath_t* path;
  avoidDebugLine_t* debugLines;
  avoidDebugText_t* debugText;
  std::uint8_t pad[12];
};

struct idObstacleAvoidance;

struct idObstacleBuffers {
  idObstacleAvoidance* user;
  CellSpursJob128* job;
  obstacleParms_t* parms;
  obstacleRoute_t* route;
  obstacleBox_t* obstacles;
  obstacleVertex_t* vertices;
  obstacleEdge_t* edges;
  obstacleCorner_t* corners;
  obstacleTrace_t* traces;
  float* traceFractions;
  obstaclePath_t* path;
  int numObstacles;
  int numVertices;
  int numEdges;
  int numCorners;
  int numTraces;
  int frameCount;
};

struct idObstacleAvoidance {
  idObstacleAvoidance();
  ~idObstacleAvoidance();

  void AllocBuffers();
  void AddObstacle(const idBox& box, idSpawnId id, bool soft);
  void AddObstacle(const idBounds& bounds, const idVec3& origin,
                   const idMat3& axis, idSpawnId id, bool soft);
  void AddCorner(const idVec3& corner, float radius);
  obstacleVertex_t* GetVertexArray();
  int GetMaxVertices();
  void SetNumVertices(int num);
  obstacleEdge_t* GetEdgeArray();
  int GetMaxEdges();
  void SetNumEdges(int num);
  bool FindPathAroundObstacles(obstaclePath_t& path, int time,
                               int gameMsPerFrame, const idBounds& bbox,
                               const idVec3& gravity, float obstacleRadius,
                               float frameMoveDist, float cornerCircleRadius,
                               int flags, const obstacleRoute_t& route,
                               obstacleTraceSet_t* traceSet);
  void Restart(const idVec3& start, const idVec3& goal);
  bool SaveQuery(const char* fileName);
  bool TestQuery(const char* fileName);

  idObstacleBuffers* buffers;
  idVec3 lastDir;
  idVec4 lastCorner;
  int lastUpdateTime;
  int firstUpdateTime;

private:
  void ProjectTopDown(idVec3& point, const idVec3& startPos,
                      float scale) const;
  void DrawBox(const idVec3& startPos, float radius) const;
};

struct testQuery_t {
  __forceinline testQuery_t() noexcept;

  int time;
  int gameMsPerFrame;
  idBounds bbox;
  idVec3 gravity;
  float obstacleRadius;
  float frameMoveDist;
  float circleCornerRadius;
  int flags;
};

static_assert(sizeof(obstacleVertex_t) == 0x0C);
static_assert(sizeof(obstacleEdge_t) == 0x10);
static_assert(sizeof(obstacleCorner_t) == 0x10);
static_assert(sizeof(obstacleTrace_t) == 0x10);
static_assert(sizeof(obstacleBox_t) == 0x50);
static_assert(sizeof(obstacleRoute_t::routeArea_t) == 0x1C);
static_assert(sizeof(obstacleRoute_t) == 0x150);
static_assert(sizeof(obstaclePath_t::radar_t) == 0x10);
static_assert(sizeof(obstaclePath_t) == 0x270);
static_assert(offsetof(obstaclePath_t, numSeekPos) == 0x174);
static_assert(offsetof(obstaclePath_t, radar) == 0x1EC);
static_assert(offsetof(obstaclePath_t, useRadarForward) == 0x25C);
static_assert(offsetof(obstaclePath_t, hasValidPath) == 0x26C);
static_assert(sizeof(obstacleTraceSet_t) == 0x1C08);
static_assert(offsetof(obstacleTraceSet_t, numTraces) == 0x1800);
static_assert(offsetof(obstacleTraceSet_t, prevNumTraces) == 0x1C04);
static_assert(sizeof(idObstacleAvoidanceCore) == 0xC0);
static_assert(sizeof(idMat3x4) == 0x30);
static_assert(sizeof(avoidDebugLine_t) == 0x1C);
static_assert(sizeof(avoidDebugText_t) == 0x30);
static_assert(sizeof(obstacleParms_t) == 0xD0);
static_assert(offsetof(obstacleParms_t, route) == 0x70);
static_assert(offsetof(obstacleParms_t, path) == 0xA8);
static_assert(sizeof(idObstacleBuffers) == 0x70);
static_assert(offsetof(idObstacleBuffers, path) == 0x50);
static_assert(offsetof(idObstacleBuffers, numObstacles) == 0x58);
static_assert(sizeof(idObstacleAvoidance) == 0x30);
static_assert(sizeof(testQuery_t) == 0x3C);
