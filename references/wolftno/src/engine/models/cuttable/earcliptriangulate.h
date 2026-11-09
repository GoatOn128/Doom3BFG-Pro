#pragma once

#include "engine/models/cuttable/contourinfo.h"
#include "shared/idlib/containers/hashindex.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/plane.h"

#include <cstdint>

class idEarClipTriangulate {
public:
  idEarClipTriangulate();
  ~idEarClipTriangulate();

  void BuildGeometry(const idPlane& plane, idList<idVec2>& positions,
                     idList<std::uint16_t>& indices);
  void SetOuterFromPoints(const idList<idVec2>& points);
  void AddInnerFromPoints(const idList<idVec2>& points);
  void SetOuterFromContour(const Contour* outer);
  void SetOuterFromPoints(const idList<idVec2i>& points);
  void AddInnerFromContour(const Contour* inner);
  void AddInnerFromPoints(const idList<idVec2i>& points);
  bool Triangulate();

private:
  friend struct EarClipValidationAccess;
  friend class idRenderModelCuttableResource;

  struct vertex_t {
    std::int16_t index;
    std::int16_t prevVertex;
    std::int16_t nextVertex;
    std::int16_t prevEar;
    std::int16_t nextEar;
    union {
      std::int16_t convex;
      std::int16_t reflex;
      std::int16_t value;
    } prevShared;
    union {
      std::int16_t convex;
      std::int16_t reflex;
      std::int16_t value;
    } nextShared;
    std::int16_t isConvex : 1;
    std::int16_t isEar : 1;
    std::int16_t __free : 14;
  };

  struct contour_t {
    idList<std::int16_t> indices;
    float valueMax;
    std::int16_t indexMax;
  };

  struct IndexMap {
    idHashIndex hashIndex;
    idList<std::int16_t> indices;

    IndexMap();
    ~IndexMap();
  };

  static_assert(sizeof(vertex_t) == 16, "idEarClipTriangulate::vertex_t ABI drift");
  static_assert(sizeof(contour_t) == 32, "idEarClipTriangulate::contour_t ABI drift");
  static_assert(sizeof(IndexMap) == 64, "idEarClipTriangulate::IndexMap ABI drift");

  static bool IsClockwise(const idList<idVec2>& points);
  void RemapIndices();
  bool RemoveReflex(int index);
  void Reset();
  int TriangleQuery(const idVec2& point, const idVec2& v0, const idVec2& v1,
                    const idVec2& v2) const;
  bool UpdateEar(int vertexIndex);
  void CreateVertices();
  bool Process();
  bool CombineContours(const contour_t* contour,
                       const idList<std::int16_t>& outerIndices,
                       idList<std::int16_t>& combined);
  bool CreateSimplePolygon();

  idList<idVec2> positions;
  idList<vertex_t> vertices;
  idList<contour_t*> contours;
  idList<std::int16_t> indices;
  idList<std::int16_t> triangles;
  IndexMap indexMap;
  int headConvex;
  int tailConvex;
  int headReflex;
  int tailReflex;
  int headEar;
  int tailEar;
  int maxIndex;
};

static_assert(sizeof(idEarClipTriangulate) == 216,
              "idEarClipTriangulate ABI drift");
