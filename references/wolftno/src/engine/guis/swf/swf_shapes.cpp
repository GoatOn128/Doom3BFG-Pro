#include "engine/guis/swf/swf.h"
#include "engine/guis/swf/swf_bitstream.h"
#include "engine/guis/swf/swf_shapeparser.h"

idSWFShapeParser::idSWFShapeParser()
    : extendedCount(false), rgba(false), morph(false), lineStyle2(false), padding(0), verts(),
      fillDraws(), lineDraws() {}

idSWFShapeParser::~idSWFShapeParser() = default;

namespace {
void DefineShapeRecord(idSWF& swf, idSWFBitStream& bitstream, int recordType) {
  const std::uint16_t characterID = bitstream.ReadU16();
  idSWFDictionaryEntry* entry = swf.AddDictionaryEntry(characterID, SWF_DICT_SHAPE);
  if (entry == nullptr) {
    return;
  }
  idSWFShapeParser parser;
  parser.Parse(bitstream, *entry->shape, recordType);
}
}

void idSWF::DefineShape(idSWFBitStream& bitstream) { DefineShapeRecord(*this, bitstream, 1); }
void idSWF::DefineShape2(idSWFBitStream& bitstream) { DefineShapeRecord(*this, bitstream, 2); }
void idSWF::DefineShape3(idSWFBitStream& bitstream) { DefineShapeRecord(*this, bitstream, 3); }
void idSWF::DefineShape4(idSWFBitStream& bitstream) { DefineShapeRecord(*this, bitstream, 4); }

