#pragma once

#include "engine/guis/swf/swf_enums.h"
#include "engine/guis/swf/swf_types.h"
#include "shared/idlib/containers/list.h"
#include "shared/idlib/text/atomicstring.h"

#include <cstdint>

class idFont;
class idFile;
class idSWF;
class idSWFBitStream;
class idSWFSpriteInstance;

enum swfDictType_t {
  SWF_DICT_NULL = 0,
  SWF_DICT_IMAGE = 1,
  SWF_DICT_SHAPE = 2,
  SWF_DICT_MORPH = 3,
  SWF_DICT_SPRITE = 4,
  SWF_DICT_FONT = 5,
  SWF_DICT_TEXT = 6,
  SWF_DICT_EDITTEXT = 7,
  NUM_DICT_TYPES = 8,
};

class idSWFShapeDrawFill {
public:
  swfFillStyle_t style;
  idList<idVec2, 72> startVerts;
  idList<idVec2, 72> endVerts;
  idList<std::uint16_t, 72> indices;
};

class idSWFShapeDrawLine {
public:
  swfLineStyle_t style;
  idList<idVec2, 72> startVerts;
  idList<idVec2, 72> endVerts;
  idList<std::uint16_t, 72> indices;
};

class idSWFShape {
public:
  idSWFShape();
  ~idSWFShape();

  swfRect_t startBounds;
  swfRect_t endBounds;
  idList<idSWFShapeDrawFill, 72> fillDraws;
  idList<idSWFShapeDrawLine, 72> lineDraws;
};

class idSWFFontGlyph {
public:
  idSWFFontGlyph() : code(0), advance(0), verts(), indices() {}
  std::uint16_t code;
  std::int16_t advance;
  idList<idVec2, 72> verts;
  idList<std::uint16_t, 72> indices;
};

class idSWFFont {
public:
  idSWFFont();
  idFont* fontID;
  std::int16_t ascent;
  std::int16_t descent;
  std::int16_t leading;
  idList<idSWFFontGlyph, 72> glyphs;
};

class idSWFTextRecord {
public:
  idSWFTextRecord()
      : fontID(0), color(), xOffset(0), yOffset(0), textHeight(0), firstGlyph(0), numGlyphs(0) {}
  std::uint16_t fontID;
  swfColorRGBA_t color;
  std::int16_t xOffset;
  std::int16_t yOffset;
  std::uint16_t textHeight;
  std::uint16_t firstGlyph;
  std::uint8_t numGlyphs;
};

struct swfGlyphEntry_t {
  swfGlyphEntry_t() : index(0), advance(0) {}
  std::uint32_t index;
  std::int32_t advance;
};

class idSWFText {
public:
  idSWFText();
  ~idSWFText();
  swfRect_t bounds;
  swfMatrix_t matrix;
  idList<idSWFTextRecord, 72> textRecords;
  idList<swfGlyphEntry_t, 72> glyphs;
};

enum swfEditTextFlags_t {
  SWF_ET_NONE = 0,
  SWF_ET_WORDWRAP = 1,
  SWF_ET_MULTILINE = 2,
  SWF_ET_PASSWORD = 4,
  SWF_ET_READONLY = 8,
  SWF_ET_AUTOSIZE = 16,
  SWF_ET_BORDER = 32,
};

enum swfEditTextAlign_t {
  SWF_ET_ALIGN_LEFT = 0,
  SWF_ET_ALIGN_RIGHT = 1,
  SWF_ET_ALIGN_CENTER = 2,
  SWF_ET_ALIGN_JUSTIFY = 3,
};

class idSWFEditText {
public:
  idSWFEditText()
      : bounds(), flags(0), fontID(0), fontHeight(24), color(), maxLength(0xffff),
        align(SWF_ET_ALIGN_LEFT), leftMargin(0), rightMargin(0), indent(0), leading(0), variable(),
        initialText() {}
  swfRect_t bounds;
  std::uint32_t flags;
  std::uint16_t fontID;
  std::uint16_t fontHeight;
  swfColorRGBA_t color;
  std::uint16_t maxLength;
  swfEditTextAlign_t align;
  std::uint16_t leftMargin;
  std::uint16_t rightMargin;
  std::uint16_t indent;
  std::int16_t leading;
  idAtomicString variable;
  idAtomicString initialText;
};

struct swfSpriteCommand_t {
  swfSpriteCommand_t() : tag(static_cast<swfTag_t>(0)), data(nullptr), dataLength(0) {}
  swfTag_t tag;
  std::uint8_t* data;
  int dataLength;
};

class idSWFSprite {
public:
  struct swfFrameLabel_t {
    idAtomicString frameLabel;
    std::uint32_t frameNum;
  };

  idSWFSprite();
  virtual ~idSWFSprite();
  int FindFrameByLabel(const char* label) const;
  void RunFrameCommands(const idSWFSpriteInstance* spriteInstance, int desiredCurrentFrame,
                        int desiredTargetFrame);
  void Write(idFile* file);
  void Read(idFile* file);
  void Load(idSWFBitStream& bitstream, idSWF* swf);

  std::uint16_t frameCount;
  idList<std::uint32_t, 72> frameOffsets;
  idList<swfFrameLabel_t, 72> frameLabels;
  idList<swfSpriteCommand_t, 72> commands;
  idList<swfSpriteCommand_t, 72> doInitActions;
};

static_assert(sizeof(idSWFShapeDrawFill) == 304, "idSWFShapeDrawFill ABI drift");
static_assert(sizeof(idSWFShapeDrawLine) == 88, "idSWFShapeDrawLine ABI drift");
static_assert(sizeof(idSWFShape) == 80, "idSWFShape ABI drift");
static_assert(sizeof(idSWFFontGlyph) == 56, "idSWFFontGlyph ABI drift");
static_assert(sizeof(idSWFFont) == 40, "idSWFFont ABI drift");
static_assert(sizeof(idSWFTextRecord) == 16, "idSWFTextRecord ABI drift");
static_assert(sizeof(swfGlyphEntry_t) == 8, "swfGlyphEntry_t ABI drift");
static_assert(sizeof(idSWFText) == 88, "idSWFText ABI drift");
static_assert(sizeof(idSWFEditText) == 64, "idSWFEditText ABI drift");
static_assert(sizeof(swfSpriteCommand_t) == 24, "swfSpriteCommand_t ABI drift");
static_assert(sizeof(idSWFSprite::swfFrameLabel_t) == 16, "swfFrameLabel_t ABI drift");
static_assert(sizeof(idSWFSprite) == 112, "idSWFSprite ABI drift");
