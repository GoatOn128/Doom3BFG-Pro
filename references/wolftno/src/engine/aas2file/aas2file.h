#pragma once

#include "shared/idlib/bv/bounds.h"
#include "shared/idlib/containers/index.h"
#include "shared/idlib/math/plane.h"
#include "shared/idlib/text/str.h"

#include <cstddef>
#include <cstdint>
#include <vector>

class idFile;
class idLexer;

enum invalidAASTree_t : int { INVALID_AAS_TREE = -1 };
using idAASTreeIndex = idIndex<int, invalidAASTree_t>;

enum aas2AreaFlags_t : std::uint16_t {
  AAS2_AREA_REACHABLE = 1u << 0,
  AAS2_AREA_NO_PUSH = 1u << 1,
  AAS2_AREA_VISITED = 1u << 2,
};

struct aas2Name_t { idStr name; };
using aas2AnimName_t = aas2Name_t;
using aas2DependencyName_t = aas2Name_t;
using aas2InteractionEntityName_t = aas2Name_t;
using aas2TraversalEntityName_t = aas2Name_t;

struct aas2Edge_t {
  int vertex[2]{};
  int flags{};
};

struct aas2Reachability_t {
  int fromArea{};
  int toArea{};
  int travelFlags{};
  idStr name;
};

struct aas2Cover_t {
  aas2Cover_t();
  idVec3 origin{};
  idVec3 normal{};
  idVec3 dir{};
  int areaNum{-1};
  std::uint16_t flags{0};
  std::uint16_t reservedBy{0x1FFF};
  std::uint16_t numTouching{0};
  std::uint16_t firstTouching{0};
  float durationSec{0.0F};
  float minRange{0.0F};
  float maxRange{0.0F};
  int usableTime{-1};
  bool valid{};
};

struct aas2Traversal_t {
  aas2Traversal_t();
  void Clear();
  void CalcExtrusionPoint(idVec3& extrusionPoint, const idVec3& up) const;

  int startArea{-1};
  int goalArea{-1};
  int flags{};
  int nameIndex{-1};
  int interactionEntityIndex{-1};
  int reachabilityIndex{-1};
  idVec3 start{};
  idVec3 end{};
};

struct aas2HintNode_t {
  aas2HintNode_t();
  idBounds bounds{};
  int areaNum{-1};
};

struct aas2Area_t {
  aas2Area_t();
  idBounds bounds{};
  idVec3 center{};
  std::uint16_t flags{AAS2_AREA_REACHABLE};
  int travelFlags{};
  int treeNum{};
  std::vector<int> edges;
  std::vector<int> reachabilities;
};

struct aas2Node_t {
  idPlane plane{};
  int children[2]{};
  idBounds bounds{};
};

struct aas2Portal_t { int areas[2]{}; };
struct aas2Cluster_t { std::vector<int> areas; };

struct aas2Trace_t {
  float fraction{1.0f};
  idVec3 endPos{};
  int areaNum{-1};
  int lastAreaNum{-1};
  bool startSolid{};
};

struct aas2TraceHeight_t : aas2Trace_t { float height{}; };
struct aas2TraceFloor_t : aas2Trace_t { idVec3 floorNormal{0.0f, 0.0f, 1.0f}; };

class idAAS2Settings {
public:
  idAAS2Settings();
  bool ParseBool(idLexer* source, bool* value);
  bool ParseInt(idLexer* source, int* value);
  bool ParseFloat(idLexer* source, float* value);
  bool ParseBounds(idLexer* source, idBounds* value);
  bool ValidForBounds(const idBounds* value) const;
  bool ReadFromFile(idLexer* source);
  bool WriteToFileBinary(idFile* file) const;
  bool ReadFromFileBinary(idFile* file);

  idBounds boundingBox{{{-16.0f, -16.0f, 0.0f}, {16.0f, 16.0f, 72.0f}}};
  float maxStepHeight{18.0f};
  float maxBarrierHeight{32.0f};
  int maxFallHeight{128};
  bool usePatches{true};
};

class idAAS2File {
public:
  struct bspTree_t {
    int rootNode{};
    idVec3 floorNormal{0.0f, 0.0f, 1.0f};
    std::vector<int> areas;
  };
  struct floorEdgeSplitPoint_t { idVec3 point{}; float distance{}; int edgeNum{-1}; };
  struct bestReachableArea_t { int areaNum{-1}; idVec3 point{}; float distance{1.0e30f}; };

  idAAS2File();
  ~idAAS2File();

  static const std::vector<idAAS2File*>& GetResourceList();
  void Clear();
  void MakeDefault();
  std::size_t MemorySize() const;
  void EndianSwap();
  bool WriteBinary(const char* fileName, const char* binaryFileName,
                   unsigned mapFileCRC, unsigned sourceTimestamp) const;
  bool LoadBinary(const char* fileName, const char* binaryFileName,
                  unsigned mapFileCRC, unsigned sourceTimestamp);
  void LoadResource();
  bool LoadResource(const char* path);
  bool ReloadIfStale();

  bool ParseIndex(idLexer* source, std::vector<int>* indexes);
  bool ParseVertices(idLexer* source);
  bool ParseObstaclePVS(idLexer* source);
  bool ParseNames(idLexer* source, std::vector<aas2Name_t>* names);
  bool ParseAnimNames(idLexer* source, std::vector<aas2AnimName_t>* names);
  bool ParseDependencyNames(idLexer* source, std::vector<aas2DependencyName_t>* names);
  bool ParseInteractionEntityNames(idLexer* source,
                                   std::vector<aas2InteractionEntityName_t>* names);
  bool ParseTraversalEntityNames(idLexer* source,
                                 std::vector<aas2TraversalEntityName_t>* names);
  bool ParsePlanes(idLexer* source);
  bool ParseEdges(idLexer* source);
  bool ParseReachabilities(idLexer* source);
  bool ParseAreas(idLexer* source);
  bool ParseNodes(idLexer* source);
  bool ParsePortals(idLexer* source);
  bool ParseClusters(idLexer* source);
  bool ParseCover(idLexer* source);
  bool ParseTraversalPoints(idLexer* source);
  bool ParseHintNodes(idLexer* source);
  bool ParseTrees(idLexer* source);

  void CalculateAreaBounds();
  bool GetAASAnim(int index, const aas2AnimName_t** name) const;
  int GetAASAnimIndexByName(const char* name) const;
  bool GetAASDependency(int index, const aas2DependencyName_t** name) const;
  int GetAASDependencyIndexByName(const char* name) const;
  bool GetAASInteractionEntity(int index, const aas2InteractionEntityName_t** name) const;
  int GetAASInteractionEntityIndexByName(const char* name) const;
  bool GetAASTraversalNameIndex(int index, const aas2TraversalEntityName_t** name) const;
  int GetAASTraversalNameIndexByName(const char* name) const;
  int GetAASTraversalIndexByNameIndex(int index) const;
  bool SetTraversalFlag(int index, int flags);
  bool ClearTraversalFlag(int index, int flags);
  bool GetAASTraversalAreas(int index, int* startArea, int* goalArea) const;
  int FindReachabilityByName(const char* name) const;
  int GetTraversalsForReachability(int reachIndex, std::vector<int>* output) const;
  int GetTraversalsForInteractableEntity(int entityIndex, std::vector<int>* output) const;
  void FlagNoPushAreas();
  void ResetCover();
  int GetTreeForFloorNormal(const idVec3* floorNormal) const;
  int GetTreeForArea(int areaNum) const;
  int GetNumAreasInTree(int treeNum) const;

  idVec3 AreaCenter(int areaNum) const;
  idBounds EdgeBounds(int edgeNum) const;
  idBounds AreaBounds(int areaNum) const;
  int PointAreaNum(int tree, const idVec3* origin) const;
  int PointAreaNumMaxheight(int tree, const idVec3* origin, float maxHeight) const;
  int BoundsAreaNums(int tree, const idBounds* bounds, int* output, int maxAreas) const;
  int BoundsAreaNums(idAASTreeIndex tree, const idBounds& bounds,
                    int* output, int maxAreas) const;
  void FlagBoundsAreas(int tree, const idBounds* bounds, bool* areasTouched) const;
  int BoundsReachableAreaNum(int tree, const idBounds* bounds,
                             std::uint16_t areaFlags, int excludeTravelFlags) const;
  bool TraceHeight(int tree, aas2TraceHeight_t* trace, const idVec3* start,
                   const idVec3* end) const;
  bool GetFloorEdgeSplitPoints(floorEdgeSplitPoint_t* nearest,
                               floorEdgeSplitPoint_t* furthest, int areaNum,
                               const idPlane* pathPlane, const idPlane* nearPlane) const;
  void MaxTreeDepth_r(int nodeNum, int* depth, int* maxDepth) const;
  int MaxTreeDepth(int tree) const;
  bool Trace(int tree, aas2Trace_t* trace, const idVec3* start, const idVec3* end) const;
  int GetObstaclePVSWallEdges(int areaNum, int edgeFlags, int* output, int maxEdges) const;
  void ClipGridToAreas(const idVec3* startOrigin, int startAreaNum, int travelFlags,
                       int cellSize, unsigned dimension, std::uint8_t* reachable) const;
  float GetFloorDistance(int areaNum, const idPlane* floorPlane, const idVec3* point,
                         float bboxHeight, float maxEdgeDist) const;
  void PointBestReachableAreaNum(int tree, const idVec3* origin,
                                 bestReachableArea_t* bestArea) const;
  void BoundsBestReachableAreaNum(int tree, const idBounds* bounds, const idVec3* origin,
                                  bestReachableArea_t* bestArea) const;
  int PointReachableAreaNum(int tree, const idVec3* origin, int areaFlags,
                            float excludeTravelFlags) const;
  int PushPointIntoAreaNum(int areaNum, idVec3* point) const;
  bool PushPointIntoNearestAreaNum(int* areaNum, idVec3* point) const;
  void FloodClearVisitedAreas(int offset);
  bool TraceFloor(aas2TraceFloor_t* trace, const idVec3* start, int startAreaNum,
                  const idVec3* end, unsigned endAreaNum, int travelFlags,
                  bool allowFloorNormalChange, bool ignoreGravityDirectionDistance,
                  bool ignoreSameArea) const;

  idStr name;
  idStr sourcePath;
  unsigned mapCRC{};
  unsigned timestamp{};
  bool valid{};
  idAAS2Settings settings;
  std::vector<idVec3> vertices;
  std::vector<idPlane> planes;
  std::vector<aas2Edge_t> edges;
  std::vector<aas2Area_t> areas;
  std::vector<aas2Node_t> nodes;
  std::vector<aas2Reachability_t> reachabilities;
  std::vector<aas2Traversal_t> traversals;
  std::vector<aas2Cover_t> cover;
  std::vector<aas2HintNode_t> hintNodes;
  std::vector<aas2Portal_t> portals;
  std::vector<aas2Cluster_t> clusters;
  std::vector<bspTree_t> trees;
  std::vector<aas2AnimName_t> animNames;
  std::vector<aas2DependencyName_t> dependencyNames;
  std::vector<aas2InteractionEntityName_t> interactionEntityNames;
  std::vector<aas2TraversalEntityName_t> traversalEntityNames;
};

class idAAS2DebugAreaModelData {
public:
  struct debugAreaInfo_t { int firstVertex{}; int vertexCount{}; idBounds bounds{}; };

  idAAS2DebugAreaModelData();
  ~idAAS2DebugAreaModelData();
  static void BuildResourceNameFromAASName(const char* aasName, idStr* resourceName);
  void Draw(const std::vector<int>* visibleAreas = nullptr) const;
  void Free();
  bool LoadBinary(const char* binaryFileName, idStr* errorMsg, unsigned* timestamp);

  idStr sourceName;
  unsigned sourceTimestamp{};
  std::vector<idVec3> vertices;
  std::vector<debugAreaInfo_t> areas;
};

class idAAS2DebugAreaModelGenerator {
public:
  static bool WriteToBinary(const char* uniqueIdName, const char* fileName,
                            unsigned sourceTimestamp,
                            const idAAS2DebugAreaModelData* data);
};

class idAAS2DebugAreaModel {
public:
  static const std::vector<idAAS2DebugAreaModel*>& GetResourceList();
  bool LoadResource();
  bool LoadResource(const char* path);
  bool ReloadIfStale();
  bool WriteResourceFile();
  bool IsValidForAASFile(const idAAS2File* file) const;

  idStr resourcePath;
  idAAS2DebugAreaModelData data;
  bool valid{};
};
