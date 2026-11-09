#pragma once

#include "shared/idlib/bv/bounds2d.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/math/vector.h"

struct ContourNode_t {
  ContourNode_t* next;
  idVec2i pos;
};

struct ContourEdge_t {
  ContourEdge_t* next;
  ContourNode_t* node;
};

struct Contour {
  Contour* next;
  ContourEdge_t* edges;
  idBounds2D bound;
  int numEdges;

  static bool SlopesEqual(const idVec2i& v1, const idVec2i& v2,
                          const idVec2i& v3, const idVec2i& v4);
  static bool SlopesEqual(const idVec2i& v1, const idVec2i& v2,
                          const idVec2i& v3);
  static bool SlopesEqual(const idList<idVec2i>& points);

  ContourEdge_t* FindEdge(const idVec2i& v0, const idVec2i& v1);
  static ContourNode_t* FindNode(ContourEdge_t* edge, const idVec2i& v0,
                                 const idVec2i& v1);
  bool ContainsPoint(const idVec2i& pos) const;
};

static_assert(sizeof(ContourNode_t) == 16, "ContourNode_t ABI drift");
static_assert(sizeof(ContourEdge_t) == 16, "ContourEdge_t ABI drift");
static_assert(sizeof(Contour) == 40, "Contour ABI drift");
