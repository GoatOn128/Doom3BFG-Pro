#include "engine/guis/swf/swf.h"

#include "engine/guis/font.h"
#include "engine/guis/swf/swf_bitstream.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/str.h"

#include <cstring>

void idSWF::DefineEditText(idSWFBitStream& bitstream) {
  idSWFDictionaryEntry* entry = AddDictionaryEntry(bitstream.ReadU16(), SWF_DICT_EDITTEXT);
  if (entry == nullptr) return;
  idSWFEditText& edit = *entry->edittext;
  bitstream.ReadRect(edit.bounds);
  bitstream.ResetBits();
  const bool hasText = bitstream.ReadBool();
  const bool wordWrap = bitstream.ReadBool();
  const bool multiline = bitstream.ReadBool();
  const bool password = bitstream.ReadBool();
  const bool readOnly = bitstream.ReadBool();
  const bool hasTextColor = bitstream.ReadBool();
  const bool hasMaxLength = bitstream.ReadBool();
  const bool hasFont = bitstream.ReadBool();
  const bool hasFontClass = bitstream.ReadBool();
  const bool autoSize = bitstream.ReadBool();
  const bool hasLayout = bitstream.ReadBool();
  bitstream.ReadBool();
  const bool border = bitstream.ReadBool();
  bitstream.ReadBool();
  bitstream.ReadBool();
  bitstream.ReadBool();
  if (hasFont) {
    edit.fontID = bitstream.ReadU16();
    edit.fontHeight = bitstream.ReadU16();
  }
  if (hasFontClass) {
    idStr ignored(bitstream.ReadString());
  }
  if (hasTextColor) bitstream.ReadColorRGBA(edit.color);
  if (hasMaxLength) edit.maxLength = bitstream.ReadU16();
  if (hasLayout) {
    edit.align = static_cast<swfEditTextAlign_t>(bitstream.ReadU8());
    edit.leftMargin = bitstream.ReadU16();
    edit.rightMargin = bitstream.ReadU16();
    edit.indent = bitstream.ReadU16();
    edit.leading = bitstream.ReadS16();
  }
  edit.variable.Set(bitstream.ReadString());
  if (hasText) {
    const char* text = bitstream.ReadString();
    idStr converted;
    for (int i = 0; text[i] != '\0'; ++i) {
      if (text[i] == '<') {
        if (i != 0 && text[i + 1] == 'p') converted.Append('\n');
        while (text[i] != '\0' && text[i] != '>') ++i;
        continue;
      }
      char output = text[i];
      if (output == '&') {
        idStr entity;
        for (++i; text[i] != '\0' && text[i] != ';'; ++i) entity.Append(text[i]);
        if (entity.Icmp("amp") == 0) output = '&';
        else if (entity.Icmp("apos") == 0) output = '\'';
        else if (entity.Icmp("lt") == 0) output = '<';
        else if (entity.Icmp("gt") == 0) output = '>';
        else if (entity.Icmp("quot") == 0) output = '"';
      }
      converted.Append(output);
    }
    edit.initialText.Set(converted.data);
  }
  edit.flags |= wordWrap ? SWF_ET_WORDWRAP : 0;
  edit.flags |= multiline ? SWF_ET_MULTILINE : 0;
  edit.flags |= password ? SWF_ET_PASSWORD : 0;
  edit.flags |= readOnly ? SWF_ET_READONLY : 0;
  edit.flags |= autoSize ? SWF_ET_AUTOSIZE : 0;
  edit.flags |= border ? SWF_ET_BORDER : 0;
}

void idSWF::DefineTextX(idSWFBitStream& bitstream, bool rgba) {
  idSWFDictionaryEntry* entry = AddDictionaryEntry(bitstream.ReadU16(), SWF_DICT_TEXT);
  if (entry == nullptr) return;
  idSWFText& text = *entry->text;
  bitstream.ReadRect(text.bounds);
  bitstream.ReadMatrix(text.matrix);
  const std::uint8_t glyphBits = bitstream.ReadU8();
  const std::uint8_t advanceBits = bitstream.ReadU8();
  for (std::uint8_t flags = bitstream.ReadU8(); flags != 0; flags = bitstream.ReadU8()) {
    idSWFTextRecord& record = *text.textRecords.Alloc();
    if ((flags & 8) != 0) record.fontID = bitstream.ReadU16();
    if ((flags & 4) != 0) {
      if (rgba) bitstream.ReadColorRGBA(record.color);
      else bitstream.ReadColorRGB(record.color);
    }
    if ((flags & 1) != 0) record.xOffset = bitstream.ReadS16();
    if ((flags & 2) != 0) record.yOffset = bitstream.ReadS16();
    if ((flags & 8) != 0) record.textHeight = bitstream.ReadU16();
    record.firstGlyph = static_cast<std::uint16_t>(text.glyphs.Num());
    record.numGlyphs = bitstream.ReadU8();
    for (int i = 0; i < record.numGlyphs; ++i) {
      swfGlyphEntry_t& glyph = *text.glyphs.Alloc();
      glyph.index = bitstream.ReadU(glyphBits);
      glyph.advance = bitstream.ReadS(advanceBits);
    }
  }
}

void idSWF::DefineText(idSWFBitStream& bitstream) { DefineTextX(bitstream, false); }
void idSWF::DefineText2(idSWFBitStream& bitstream) { DefineTextX(bitstream, true); }

void idSWF::DefineFont2(idSWFBitStream& bitstream) {
  idSWFDictionaryEntry* entry = AddDictionaryEntry(bitstream.ReadU16(), SWF_DICT_FONT);
  if (entry == nullptr) return;
  const std::uint8_t flags = bitstream.ReadU8();
  bitstream.ReadU8();
  char fontName[257];
  const std::uint8_t nameLength = bitstream.ReadU8();
  std::memcpy(fontName, bitstream.ReadData(nameLength), nameLength);
  fontName[nameLength] = '\0';
  entry->font->fontID = static_cast<idFont*>(idFont::resourceList.Load(fontName, true, false));
  const std::uint16_t numGlyphs = bitstream.ReadU16();
  entry->font->glyphs.SetNum(numGlyphs);
  if (numGlyphs == 0) {
    idLib::Warning("idSWF::DefineFont2: numglyphs = 0 exiting");
    return;
  }
  if ((flags & 8) != 0) {
    const std::uint32_t tableSize = (numGlyphs + 1u) * 4u;
    idSWFBitStream offsets(bitstream.ReadData(static_cast<int>(tableSize)), tableSize, false);
    if (offsets.ReadU32() != tableSize) {
      idLib::Warning("idSWF::DefineFont2: first glyph offset != offsetTableSize");
      return;
    }
    std::uint32_t previous = tableSize;
    for (int i = 0; i < numGlyphs; ++i) {
      const std::uint32_t next = offsets.ReadU32();
      bitstream.ReadData(static_cast<int>(next - previous));
      previous = next;
    }
  } else {
    const std::uint16_t tableSize = static_cast<std::uint16_t>((numGlyphs + 1u) * 2u);
    idSWFBitStream offsets(bitstream.ReadData(tableSize), tableSize, false);
    if (offsets.ReadU16() != tableSize) {
      idLib::Warning("idSWF::DefineFont2: first glyph offset != offsetTableSize");
      return;
    }
    std::uint16_t previous = tableSize;
    for (int i = 0; i < numGlyphs; ++i) {
      const std::uint16_t next = offsets.ReadU16();
      bitstream.ReadData(static_cast<std::uint16_t>(next - previous));
      previous = next;
    }
  }
  for (int i = 0; i < numGlyphs; ++i) {
    entry->font->glyphs[i].code = (flags & 4) != 0 ? bitstream.ReadU16() : bitstream.ReadU8();
  }
  if ((flags & 0x80) != 0) {
    entry->font->ascent = bitstream.ReadS16();
    entry->font->descent = bitstream.ReadS16();
    entry->font->leading = bitstream.ReadS16();
    for (int i = 0; i < numGlyphs; ++i) entry->font->glyphs[i].advance = bitstream.ReadS16();
    for (int i = 0; i < numGlyphs; ++i) {
      swfRect_t ignored;
      bitstream.ReadRect(ignored);
    }
    const std::uint16_t kerningCount = bitstream.ReadU16();
    for (int i = 0; i < kerningCount; ++i) {
      if ((flags & 4) != 0) {
        bitstream.ReadU16();
        bitstream.ReadU16();
      } else {
        bitstream.ReadU8();
        bitstream.ReadU8();
      }
      bitstream.ReadS16();
    }
  }
}

void idSWF::DefineFont3(idSWFBitStream& bitstream) { DefineFont2(bitstream); }
