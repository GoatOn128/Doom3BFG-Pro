#pragma once

#include "engine/models/cuttable/contourinfo.h"
#include "engine/models/cuttable/cuttercontour.h"
#include "shared/idlib/color.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/containers/staticlist.h"
#include "shared/idlib/filesystem/file.h"
#include "shared/idlib/math/plane.h"
#include "shared/idlib/text/str.h"

#include <cstdint>

class idRenderWorld;

struct graphBranch_t {
  std::int16_t index = 0;
  std::int16_t num = 0;
};

struct graphTree_t {
  graphTree_t();
  ~graphTree_t();

  idStaticList<idVec3, 2048> positions;
  idStaticList<idVec3, 1024> intersections;
  idStaticList<idVec3, 1024> endpoints;
  idStaticList<graphBranch_t, 256> contours;
  idStaticList<graphBranch_t, 256> branches;
};

static_assert(sizeof(graphTree_t) == 51320, "graphTree_t ABI drift");

class idCutterGraphManager {
public:
  enum : std::int32_t {
    NUM_POOLNODES = 1024,
    NUM_POOLLINKS = 1024,
  };

  struct Link_t;
  struct Edge_t;

  struct Node_t {
    Node_t* next;
    Link_t* link;
    idVec2i pos;
    std::int32_t id : 31;
    std::int32_t allocated : 1;
  };

  struct Link_t {
    Link_t* next;
    Edge_t* edge;
    Node_t* node;
    std::uint32_t allocated : 1;
    int invalid;
  };

  struct Edge_t {
    Edge_t* next;
    Node_t* node1;
    Node_t* node2;
    Contour* contour;
    ContourEdge_t* contourEdge;
    std::uint32_t visited : 1;
    std::uint32_t allocated : 1;
  };

  struct nodeHandle_t { Node_t* data = nullptr; };
  struct edgeHandle_t { Edge_t* data = nullptr; };
  struct linkHandle_t { Link_t* data = nullptr; };

  struct savedContour_t {
    idList<idVec3> contourPoints;
    idColor color{};
    idStr text;
  };

  struct savedPathPos_t {
    idVec3 pos{};
    idVec2 localPos{};
    idColor color{};
  };

  struct savedRemovedLinks_t {
    idVec2i nodePos{};
    idVec2i edgePos1{};
    idVec2i edgePos2{};
  };

  class Graph;

  struct IDHeap {
    idList<std::uint8_t> buffer;
    std::uint32_t index : 31;
    std::uint32_t isStatic : 1;
    int numBits;

    IDHeap();
    ~IDHeap();
    int Alloc();
    void Clear();
    void Write(idFile* file) const;
    void Read(idFile* file);
  };

  struct graph_t { Graph* graph = nullptr; };

  class Graph {
  public:
    Graph();
    ~Graph();

    int FindNodeIndex(const idVec2i& pos, int threshold) const;
    idVec2i& GetLocal2D(idVec2i& result, const idVec3& point) const;
    idVec3& GetLocal3D(idVec3& result, const idVec2& point2d) const;
    idVec3& GetPosition3D(idVec3& result, const idVec2i& point2d) const;
    void SetPosition(const idVec3& position, const idMat3& orientation);
    void DebugDraw(idRenderWorld* renderWorld, int lifeTime) const;
    int FindClosestNode(const Edge_t* edge, const idVec2i& hitPos, int threshold) const;
    Edge_t* FindSplitEdge(const Node_t* srcNode, const idVec2i& dst, idVec2i& pos) const;
    void Init(const idStr& name, Contour* contour);
    void AddNode(Node_t* node);
    bool PathFind(idList<idVec2i>& points);
    void BuildGraphTree(graphTree_t* graphTree) const;

    Node_t* lastNode;
    Node_t* prevNode;
    idList<nodeHandle_t> nodes;
    idList<edgeHandle_t> edges;
    idList<linkHandle_t> links;
    Contour* contoursOuter;
    Contour* contoursInner;
    IDHeap idheap;
    idStr name;
    idMat3 inverse;
    idMat3 orientation;
    idVec3 position;
    idPlane plane;
    idVec3 left;
    idVec3 down;
  };

  idCutterGraphManager();
  virtual ~idCutterGraphManager();

  void MergeContours(Contour* contour);
  void FreeContour(Contour* contour);
  void PreAcquire();
  Contour* CreateContour(const idList<idVec2i>& points);
  idVec2i& GetLocal2D(idVec2i& result, const idVec3& point) const;
  idVec3& GetLocal3D(idVec3& result, const idVec2& point2d) const;
  idVec3& GetPosition3D(idVec3& result, const idVec2i& point2d) const;
  bool IsGraphValid(int id) const;
  void SetPosition(int id, const idVec3& position, const idMat3& orientation);
  void GetNormalVectors(int id, idVec3& normal, idVec3& left, idVec3& up) const;
  Contour* GetOuterContour(int id) const;
  Contour* GetInnerContour(int id) const;
  void DebugDraw(int id, idRenderWorld* renderWorld, int lifeTime) const;
  void PreAllocatePools();
  Node_t* AllocateNode();
  Edge_t* AllocateEdge();
  void FreeLink(Link_t* link);
  void FreeNode(Node_t* node);
  void FreeEdge(Edge_t* edge);
  void ProcessLinks(Contour* contour, Node_t* node);
  int AllocateGraph(const idStr& name, const idVec3& position,
                    const idMat3& orientation, const idPlane& plane,
                    const idList<idVec3>& points);
  bool PathFind(idList<idVec2i>& points);
  void ReleaseGraph(int id);
  void FreeGraph(int id);
  void BuildGraphTree(int id, graphTree_t* graphTree) const;
  void AddLink(Node_t* srcNode, Node_t* dstNode, Edge_t* edge);
  void RemoveLink(Node_t* srcNode, Node_t* dstNode);
  void Connect(Node_t* srcNode, Node_t* dstNode, Contour* contour);
  bool BuildEdge(const idVec2i& dst, int maxConnectDistance);
  void ApplyGraph(Graph* graph);
  void Acquire(int id);
  bool AddContour(Contour* contours, Contour* mergedContours);
  Contour* IntersectContours(Contour* contour);
  void Write(idFile* file, int graphHandle) const;
  void Read(idFile* file);

  idList<savedContour_t> savedContours;
  idList<savedPathPos_t> savedNodes;
  idList<savedRemovedLinks_t> savedRemovedLinks;
  idList<Node_t*> invalidNodes;
  idList<graph_t> graphs;
  idList<Link_t*> linksPoolBlock;
  idList<Node_t*> nodesPoolBlock;
  idList<Edge_t*> edgesPoolBlock;
  idList<Link_t*> linksPool;
  idList<Node_t*> nodesPool;
  idList<Edge_t*> edgesPool;
  idContourManager* contourManager;
  Graph* graph;
};

static_assert(sizeof(idCutterGraphManager::Node_t) == 32, "Node_t ABI drift");
static_assert(sizeof(idCutterGraphManager::Link_t) == 32, "Link_t ABI drift");
static_assert(sizeof(idCutterGraphManager::Edge_t) == 48, "Edge_t ABI drift");
static_assert(sizeof(idCutterGraphManager::IDHeap) == 32, "IDHeap ABI drift");
static_assert(sizeof(idCutterGraphManager::Graph) == 304, "Graph ABI drift");
static_assert(sizeof(idCutterGraphManager) == 288, "idCutterGraphManager ABI drift");
