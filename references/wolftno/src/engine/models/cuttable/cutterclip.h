#pragma once

#include "engine/models/cuttable/contourinfo.h"
#include "shared/idlib/containers/list.h"

#include <cstddef>
#include <cstdint>

enum polygonType_t : std::int32_t {
  POLYTYPE_CLIP = 0,
  POLYTYPE_SUBJECT = 1,
};

enum clipMode_t : std::int32_t {
  CLIPMODE_INTERSECT = 0,
  CLIPMODE_UNION = 1,
  CLIPMODE_DIFFERENCE = 2,
  CLIPMODE_XOR = 3,
};

enum fillType_t : std::int32_t {
  FILLTYPE_EVEN_ODD = 0,
  FILLTYPE_NON_ZERO = 1,
};

class idCutterClip {
public:
  enum horizontalDirection_t : std::int32_t {
    HORIZONTALDIRECTION_NONE = 0,
    HORIZONTALDIRECTION_LEFTTORIGHT = 1,
    HORIZONTALDIRECTION_RIGHTTOLEFT = 2,
  };

  enum intersectProtects_t : std::int32_t {
    INTERSECTPROTECTS_NONE = 0,
    INTERSECTPROTECTS_LEFT = 1,
    INTERSECTPROTECTS_RIGHT = 2,
    INTERSECTPROTECTS_BOTH = 3,
  };

  enum addEdge_t : std::int32_t {
    ADDEDGE_NONE = 0,
    ADDEDGE_LEFT = 1,
    ADDEDGE_RIGHT = 2,
    ADDEDGE_BOTH = 3,
  };

  enum edgeSide_t : std::int32_t {
    EDGESIDE_LEFT = 0,
    EDGESIDE_RIGHT = 1,
  };

  enum intersectState_t : std::int32_t {
    INTERSECTSTATE_NONE = 0,
    INTERSECTSTATE_LEFT = 1,
    INTERSECTSTATE_RIGHT = 2,
    INTERSECTSTATE_BOTH = 3,
  };

  struct Edge_t {
    Edge_t* prev;
    Edge_t* next;
    Edge_t* prevLML;
    Edge_t* nextLML;
    Edge_t* prevAEL;
    Edge_t* nextAEL;
    Edge_t* prevSEL;
    Edge_t* nextSEL;
    idVec2i top;
    idVec2i bottom;
    idVec2i current;
    float deltaX;
    int tmpX;
    std::int16_t windingCount;
    std::int16_t windingCount2;
    std::int16_t outIndex;
    std::int16_t windingDelta;
    std::uint16_t side : 1;
    std::uint16_t polyType : 1;
    std::uint16_t horizontal : 1;
    std::uint16_t free : 12;
    int ID;
  };

  struct Scanbeam_t {
    Scanbeam_t* next;
    int y;
  };

  struct PolygonPoint_t {
    PolygonPoint_t* next;
    PolygonPoint_t* prev;
    idVec2i pos;
    int index;
  };

#pragma pack(push, 4)
  struct Polygon_t {
    Polygon_t* next;
    Polygon_t* firstLeft;
    Polygon_t* appendLink;
    PolygonPoint_t* points;
    PolygonPoint_t* bottomPoint;
    Edge_t* bottomEdge1;
    Edge_t* bottomEdge2;
    int index;
    bool connected;
    bool hole;
  };
#pragma pack(pop)

  struct NestedPolygon_t {
    NestedPolygon_t* next;
    Polygon_t* outer;
    Polygon_t* inner;
  };

  struct LocalMinima_t {
    LocalMinima_t* next;
    Edge_t* leftBound;
    Edge_t* rightBound;
    int y;
  };

  struct IntersectNode_t {
    IntersectNode_t* next;
    Edge_t* edge1;
    Edge_t* edge2;
    idVec2i pos;
  };

  struct JoinRecord_t {
    JoinRecord_t* next;
    idVec2i point1a;
    idVec2i point1b;
    idVec2i point2a;
    idVec2i point2b;
    std::int16_t polyIndex1;
    std::int16_t polyIndex2;
  };

  struct HorizontalJoinRecord_t {
    HorizontalJoinRecord_t* next;
    Edge_t* edge;
    std::int16_t savedIndex;
  };

  idCutterClip();
  ~idCutterClip();

  void Prepare();
  void PreAllocate();
  void Reset();
  void AddContour(const Contour* contours, polygonType_t polyType);
  bool Clip(clipMode_t clipMode, fillType_t fillType);

  int GetNumPolygons() const;
  void GetPolygon(int index, idList<idVec2i>& list);
  void GetInnerPolygon(idList<idVec2i>& list);
  void GetConnectedPolygon(int index, idList<idVec2i>& list);
  bool HasInnerPolygon() const;

  static bool LineLineIntersection(const idVec2i& p1a, const idVec2i& p1b,
                                   const idVec2i& p2a, const idVec2i& p2b,
                                   idVec2i* pos);

private:
#if defined(WOLF_CUTTERCLIP_TEST_ACCESS)
  friend struct idCutterClipTestAccess;
#endif
  class idSort_Polygons;
  static void GetPolygon(const Polygon_t* polygon, idList<idVec2i>& list);
  static bool IsClockwise(const PolygonPoint_t* points);
  static bool PointInPolygon(const idVec2i& pos, const PolygonPoint_t* point);
  static bool EdgeEdgeIntersection(Edge_t* edge1, Edge_t* edge2, idVec2i* pos);
  static bool IsSegmentOverlapping(const idVec2i& p1a, const idVec2i& p1b,
                                   const idVec2i& p2a, const idVec2i& p2b,
                                   idVec2i* overlap1, idVec2i* overlap2);
  static bool SlopesEqual(Edge_t* edge1, Edge_t* edge2);
  static bool IsContributing(clipMode_t clipMode, Edge_t* edge);
  static const Polygon_t* FindBottomPolygon(const Polygon_t* polygon1,
                                            const Polygon_t* polygon2);
  static PolygonPoint_t* FindBottom(PolygonPoint_t* point);
  static void ReverseLinks(PolygonPoint_t* point);

  void SwapEdgeInAEL(Edge_t* edge1, Edge_t* edge2);
  void SwapEdgeInSEL(Edge_t* edge1, Edge_t* edge2);
  void RemoveEdgeFromAEL(Edge_t* edge);
  bool FixupIntersections();
  bool FixHoleLinkage_r(Polygon_t* polygon);
  void AddEdgeToAEL(Edge_t* edge);
  void AppendPolygon(Edge_t* edge1, Edge_t* edge2);
  void SetWindingCount(Edge_t* edge);
  void ClearHorizontalJoinRecords();
  void ClearJoinRecords();
  void ClearNestedPolygons();
  void ClearPolygons();
  void ClearPolygonPoints();
  void FixupPolygon(Polygon_t* polygon);

  Edge_t* AllocEdge();
  LocalMinima_t* AllocLocalMinima();
  Scanbeam_t* AllocScanbeam();
  Polygon_t* AllocPolygon();
  PolygonPoint_t* AllocPolygonPoint();
  NestedPolygon_t* AllocNestedPolygon();
  JoinRecord_t* AllocJoinRecord();
  HorizontalJoinRecord_t* AllocHorizontalJoinRecord();
  IntersectNode_t* AllocIntersectNode();
  void BuildResult();
  Edge_t* AddBoundToLML(Edge_t* edge);
  void AddScanbeam(int y);
  void AddJoinRecord(Edge_t* edge1, Edge_t* edge2,
                     std::int16_t outIndexEdge1, std::int16_t outIndexEdge2);
  PolygonPoint_t* AddPolygonPoint(Edge_t* edge1, Edge_t* edge2,
                                  const idVec2i& pos);
  void AddLocalMinPolygon(Edge_t* edge1, Edge_t* edge2, const idVec2i& pos);
  void AddLocalMaxPolygon(Edge_t* edge1, Edge_t* edge2, const idVec2i& pos);
  void AddEdgePolygon(addEdge_t addFlags, Edge_t* edge1, Edge_t* edge2,
                      const idVec2i& pos);
  void IntersectEdges(Edge_t* edge1, Edge_t* edge2, const idVec2i& pos,
                      intersectProtects_t protects);
  Edge_t* UpdateEdgeInAEL(Edge_t* edge);
  void ProcessIntersectList();
  void BuildIntersectList(int yBottom, int yTop);
  bool DoMaxima(Edge_t* edge, Edge_t* edgeMaxPair, int y);
  void AddLocalMinimaToAEL(int yBottom);
  bool ProcessHorizontal(Edge_t* edge);
  void JoinEdges();
  bool ProcessEdgesAtTopOfScanbeam(int y);

#pragma pack(push, 4)
  static int precisionModifier;

  int memoryPeek = 0;
  int memoryCurrent = 0;
  int numEdges = 0;
  int maxEdges = 0;
  int numLocalMinima = 0;
  int maxLocalMinima = 0;
  int numScanbeams = 0;
  int maxScanbeams = 0;
  int numPolygons = 0;
  int maxPolygons = 0;
  int numPolygonPoints = 0;
  int maxPolygonPoints = 0;
  int numNestedPolygons = 0;
  int maxNestedPolygons = 0;
  int numJoinRecords = 0;
  int maxJoinRecords = 0;
  int numHorizontalJoinRecords = 0;
  int maxHorizontalJoinRecords = 0;
  int numIntersectNode = 0;
  int maxIntersectNode = 0;

  Edge_t* edgesPool = nullptr;
  Scanbeam_t* scanbeamsPool = nullptr;
  Polygon_t* polygonsPool = nullptr;
  PolygonPoint_t* polygonPointsPool = nullptr;
  NestedPolygon_t* nestedPolygonsPool = nullptr;
  LocalMinima_t* localMinimasPool = nullptr;
  IntersectNode_t* intersectNodesPool = nullptr;
  JoinRecord_t* joinRecordsPool = nullptr;
  HorizontalJoinRecord_t* horizontalJoinRecordsPool = nullptr;
  LocalMinima_t* currentLocalMinima = nullptr;
  LocalMinima_t* localMinimaList = nullptr;
  Scanbeam_t* scanbeams = nullptr;
  Edge_t* activeEdges = nullptr;
  Edge_t* sortedEdges = nullptr;
  IntersectNode_t* intersectNodes = nullptr;
  NestedPolygon_t* currentNestedPolygon = nullptr;
  Polygon_t* currentInnerPolygon = nullptr;

  idList<Edge_t*> edges;
  idList<PolygonPoint_t*> polygonPoints;
  idList<Polygon_t*> polygons;
  idList<NestedPolygon_t*> nestedPolygons;
  idList<JoinRecord_t*> joins;
  idList<HorizontalJoinRecord_t*> horizontalJoins;
  idList<void*> poolAllocations;

  fillType_t fillTypeSubject;
  fillType_t fillTypeClip;
  clipMode_t clipMode;
  bool running : 1;

};
#pragma pack(pop)

static_assert(sizeof(idCutterClip::Edge_t) == 112, "Edge_t ABI drift");
static_assert(offsetof(idCutterClip::Edge_t, top) == 64, "Edge_t::top ABI drift");
static_assert(offsetof(idCutterClip::Edge_t, bottom) == 72,
              "Edge_t::bottom ABI drift");
static_assert(offsetof(idCutterClip::Edge_t, current) == 80,
              "Edge_t::current ABI drift");
static_assert(offsetof(idCutterClip::Edge_t, deltaX) == 88,
              "Edge_t::deltaX ABI drift");
static_assert(offsetof(idCutterClip::Edge_t, windingCount) == 96,
              "Edge_t::windingCount ABI drift");
static_assert(offsetof(idCutterClip::Edge_t, outIndex) == 100,
              "Edge_t::outIndex ABI drift");
static_assert(offsetof(idCutterClip::Edge_t, ID) == 108, "Edge_t::ID ABI drift");
static_assert(sizeof(idCutterClip::Scanbeam_t) == 16,
              "Scanbeam_t ABI drift");
static_assert(sizeof(idCutterClip::PolygonPoint_t) == 32,
              "PolygonPoint_t ABI drift");
static_assert(sizeof(idCutterClip::Polygon_t) == 64,
              "Polygon_t ABI drift");
static_assert(alignof(idCutterClip::Polygon_t) == 4,
              "Polygon_t alignment drift");
static_assert(offsetof(idCutterClip::Polygon_t, points) == 24,
              "Polygon_t::points ABI drift");
static_assert(offsetof(idCutterClip::Polygon_t, index) == 56,
              "Polygon_t::index ABI drift");
static_assert(sizeof(idCutterClip::NestedPolygon_t) == 24,
              "NestedPolygon_t ABI drift");
static_assert(sizeof(idCutterClip::LocalMinima_t) == 32,
              "LocalMinima_t ABI drift");
static_assert(sizeof(idCutterClip::IntersectNode_t) == 32,
              "IntersectNode_t ABI drift");
static_assert(sizeof(idCutterClip::JoinRecord_t) == 48,
              "JoinRecord_t ABI drift");
static_assert(sizeof(idCutterClip::HorizontalJoinRecord_t) == 24,
              "HorizontalJoinRecord_t ABI drift");
static_assert(sizeof(idCutterClip) == 400, "idCutterClip ABI drift");
static_assert(alignof(idCutterClip) == 4, "idCutterClip alignment drift");
