#pragma once

#include "engine/models/cuttable/contourinfo.h"

class idContourManager {
public:
  idContourManager();
  ~idContourManager();

  void Free(Contour* contour);
  ContourEdge_t* AllocEdge();
  Contour* AllocContour();
  ContourNode_t* AllocNode();
  Contour* Create(const idList<idVec2i>& points);
  void Merge(Contour* contours);
  void SplitEdge(ContourEdge_t* edge, const idVec2i& v0, const idVec2i& v1,
                 const idVec2i& pos);

private:
#if defined(WOLF_CUTTERCONTOUR_TEST_ACCESS)
  friend struct idContourManagerTestAccess;
#endif
  Contour* contoursPool = nullptr;
  ContourEdge_t* edgesPool = nullptr;
  ContourNode_t* nodesPool = nullptr;
};

static_assert(sizeof(idContourManager) == 24,
              "idContourManager ABI drift");
