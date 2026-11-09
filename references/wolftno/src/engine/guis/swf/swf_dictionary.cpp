#include "engine/guis/swf/swf.h"

#include "shared/idlib/lib.h"

idSWFDictionaryEntry& idSWFDictionaryEntry::operator=(idSWFDictionaryEntry& other) {
  type = other.type;
  material = other.material;
  shape = other.shape;
  sprite = other.sprite;
  font = other.font;
  text = other.text;
  edittext = other.edittext;
  imageSize = other.imageSize;
  imageAtlasOffset = other.imageAtlasOffset;
  other.type = SWF_DICT_NULL;
  other.material = nullptr;
  other.shape = nullptr;
  other.sprite = nullptr;
  other.font = nullptr;
  other.text = nullptr;
  other.edittext = nullptr;
  return *this;
}

idSWFDictionaryEntry::idSWFDictionaryEntry()
    : type(SWF_DICT_NULL), material(nullptr), shape(nullptr), sprite(nullptr), font(nullptr),
      text(nullptr), edittext(nullptr), imageSize{0, 0}, imageAtlasOffset{0, 0},
      channelScale{1.0f, 1.0f, 1.0f, 1.0f} {}

idSWFDictionaryEntry* idSWF::FindDictionaryEntry(int characterID, swfDictType_t type) {
  if (dictionary.Num() < characterID + 1) {
    idLib::Warning("%s: Could not find character %d", filename.str, characterID);
    return nullptr;
  }
  idSWFDictionaryEntry* result = &dictionary[characterID];
  if (result->type != type) {
    idLib::Warning("%s: Character %d is the wrong type", filename.str, characterID);
    return nullptr;
  }
  return result;
}

idSWFDictionaryEntry* idSWF::FindDictionaryEntry(int characterID) {
  if (dictionary.Num() < characterID + 1) {
    idLib::Warning("%s: Could not find character %d", filename.str, characterID);
    return nullptr;
  }
  return &dictionary[characterID];
}

idSWFText::~idSWFText() = default;

idSWFText::idSWFText() : bounds(), matrix(), textRecords(), glyphs() {
  textRecords.memTag = 72;
  glyphs.memTag = 72;
}

idSWFFont::idSWFFont() : fontID(nullptr), ascent(0), descent(0), leading(0), glyphs() {
  glyphs.memTag = 72;
}

idSWFShape::~idSWFShape() = default;

idSWFShape::idSWFShape() : startBounds(), endBounds(), fillDraws(), lineDraws() {
  fillDraws.memTag = 72;
  lineDraws.memTag = 72;
}

idSWFDictionaryEntry::~idSWFDictionaryEntry() {
  delete shape;
  delete sprite;
  delete font;
  delete text;
  delete edittext;
}

idSWFDictionaryEntry* idSWF::AddDictionaryEntry(int characterID, swfDictType_t type) {
  if (dictionary.Num() < characterID + 1)
    dictionary.SetNum(characterID + 1);

  idSWFDictionaryEntry& entry = dictionary[characterID];
  if (entry.type != SWF_DICT_NULL) {
    idLib::Warning("%s: Duplicate character %d", filename.str, characterID);
    return nullptr;
  }

  entry.type = type;
  switch (type) {
  case SWF_DICT_SHAPE:
  case SWF_DICT_MORPH:
    entry.shape = new idSWFShape;
    break;
  case SWF_DICT_SPRITE:
    entry.sprite = new idSWFSprite;
    break;
  case SWF_DICT_FONT:
    entry.font = new idSWFFont;
    break;
  case SWF_DICT_TEXT:
    entry.text = new idSWFText;
    break;
  case SWF_DICT_EDITTEXT:
    entry.edittext = new idSWFEditText;
    break;
  default:
    break;
  }
  return &entry;
}

