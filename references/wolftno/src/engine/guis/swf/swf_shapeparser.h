#pragma once

#include "engine/guis/swf/swf_resources.h"
#include "shared/idlib/containers/list.h"

#include <cstdint>

class idSWFBitStream;

class idSWFShapeParser {
public:
  idSWFShapeParser();
  ~idSWFShapeParser();

  void Parse(idSWFBitStream& bitstream, idSWFShape& shape, int recordType);

private:
  struct swfSPEdge_t {
    std::uint16_t v0;
    std::uint16_t v1;
    std::uint16_t cp;
  };

  struct swfSPMorphEdge_t {
    swfSPEdge_t start;
    swfSPEdge_t end;
  };

  struct swfSPLineLoop_t {
    bool hole;
    std::uint8_t padding[7];
    idList<std::uint16_t, 72> vindex1;
    idList<std::uint16_t, 72> vindex2;
  };

  struct swfSPDrawFill_t {
    swfFillStyle_t style;
    idList<swfSPMorphEdge_t, 72> edges;
    idList<swfSPLineLoop_t, 72> loops;
  };

  struct swfSPDrawLine_t {
    swfLineStyle_t style;
    std::uint32_t padding;
    idList<swfSPMorphEdge_t, 72> edges;
  };

  void ParseShapes(idSWFBitStream& bitstream1, idSWFBitStream* bitstream2, bool swap);
  void ReadFillStyle(idSWFBitStream& bitstream);
  void ParseEdge(idSWFBitStream& bitstream, int& penX, int& penY, swfSPEdge_t& edge);
  void MakeLoops();
  void TriangulateSoup(idSWFShape& shape);
  int FindEarVert(swfSPLineLoop_t& loop);
  void AddUniqueVert(idSWFShapeDrawFill& drawFill, idVec2& start, idVec2& end);

  bool extendedCount;
  bool rgba;
  bool morph;
  bool lineStyle2;
  std::uint32_t padding;
  idList<idVec2, 72> verts;
  idList<swfSPDrawFill_t, 72> fillDraws;
  idList<swfSPDrawLine_t, 72> lineDraws;
};

static_assert(sizeof(idSWFShapeParser) == 80, "idSWFShapeParser ABI drift");

