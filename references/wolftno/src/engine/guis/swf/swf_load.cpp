#include "engine/guis/swf/swf.h"

#include "engine/guis/font.h"
#include "engine/guis/swf/swf_bitstream.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/lib.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <vector>

namespace {

constexpr std::uint32_t BSWF_VERSION = 20;
constexpr std::uint32_t BSWF_MAGIC = ('B' << 24) | ('S' << 16) | ('W' << 8) | BSWF_VERSION;

#pragma pack(push, 1)
struct swfHeader_t {
  char compression;
  char S;
  char W;
  std::uint8_t version;
  std::uint32_t fileLength;
};
#pragma pack(pop)
static_assert(sizeof(swfHeader_t) == 8, "SWF header ABI drift");

bool ReadBytes(idFile* file, void* value, std::size_t size) {
  return file != nullptr && file->Read(value, size) == size;
}

bool WriteBytes(idFile* file, const void* value, std::size_t size) {
  return file != nullptr && file->Write(value, size) == size;
}

bool ReadU8(idFile* file, std::uint8_t& value) { return ReadBytes(file, &value, 1); }
bool WriteU8(idFile* file, std::uint8_t value) { return WriteBytes(file, &value, 1); }

bool ReadBE16(idFile* file, std::uint16_t& value) {
  std::uint8_t bytes[2];
  if (!ReadBytes(file, bytes, sizeof(bytes))) return false;
  value = static_cast<std::uint16_t>((bytes[0] << 8) | bytes[1]);
  return true;
}

bool WriteBE16(idFile* file, std::uint16_t value) {
  const std::uint8_t bytes[2] = {static_cast<std::uint8_t>(value >> 8),
                                 static_cast<std::uint8_t>(value)};
  return WriteBytes(file, bytes, sizeof(bytes));
}

bool ReadBE32(idFile* file, std::uint32_t& value) {
  std::uint8_t bytes[4];
  if (!ReadBytes(file, bytes, sizeof(bytes))) return false;
  value = (static_cast<std::uint32_t>(bytes[0]) << 24) |
          (static_cast<std::uint32_t>(bytes[1]) << 16) |
          (static_cast<std::uint32_t>(bytes[2]) << 8) | bytes[3];
  return true;
}

bool WriteBE32(idFile* file, std::uint32_t value) {
  const std::uint8_t bytes[4] = {static_cast<std::uint8_t>(value >> 24),
                                 static_cast<std::uint8_t>(value >> 16),
                                 static_cast<std::uint8_t>(value >> 8),
                                 static_cast<std::uint8_t>(value)};
  return WriteBytes(file, bytes, sizeof(bytes));
}

bool ReadBE32(idFile* file, std::int32_t& value) {
  std::uint32_t bits = 0;
  if (!ReadBE32(file, bits)) return false;
  value = static_cast<std::int32_t>(bits);
  return true;
}

bool WriteBE32(idFile* file, std::int32_t value) {
  return WriteBE32(file, static_cast<std::uint32_t>(value));
}

bool ReadBEFloat(idFile* file, float& value) {
  std::uint32_t bits = 0;
  if (!ReadBE32(file, bits)) return false;
  std::memcpy(&value, &bits, sizeof(value));
  return true;
}

bool WriteBEFloat(idFile* file, float value) {
  std::uint32_t bits = 0;
  std::memcpy(&bits, &value, sizeof(bits));
  return WriteBE32(file, bits);
}

bool ReadBEVec2(idFile* file, idVec2& value) {
  return ReadBEFloat(file, value.x) && ReadBEFloat(file, value.y);
}

bool WriteBEVec2(idFile* file, const idVec2& value) {
  return WriteBEFloat(file, value.x) && WriteBEFloat(file, value.y);
}

template <typename T, typename Reader>
bool ReadArray(idFile* file, T* values, int count, Reader reader) {
  for (int i = 0; i < count; ++i)
    if (!reader(file, values[i])) return false;
  return true;
}

template <typename T, typename Writer>
bool WriteArray(idFile* file, const T* values, int count, Writer writer) {
  for (int i = 0; i < count; ++i)
    if (!writer(file, values[i])) return false;
  return true;
}

bool ReadString(idFile* file, idStr& value) {
  std::int32_t length = 0;
  if (!ReadBE32(file, length) || length < 0) return false;
  std::vector<char> text(static_cast<std::size_t>(length) + 1, '\0');
  if (length != 0 && !ReadBytes(file, text.data(), static_cast<std::size_t>(length))) return false;
  value = text.data();
  return true;
}

bool ReadString(idFile* file, idAtomicString& value) {
  idStr text;
  if (!ReadString(file, text)) return false;
  value.Set(text.data);
  return true;
}

bool WriteString(idFile* file, const char* value) {
  if (value == nullptr) value = "";
  const int length = idStr::Length(value);
  return WriteBE32(file, length) &&
         (length == 0 || WriteBytes(file, value, static_cast<std::size_t>(length)));
}

bool ReadCount(idFile* file, int& count) {
  std::int32_t value = 0;
  if (!ReadBE32(file, value) || value < 0) return false;
  count = value;
  return true;
}

bool JapaneseLanguageEnabled() {
  return cvarSystem != nullptr && cvarSystem->GetCVarBool("sys_langJapanese", false);
}

idStr AtlasFilename(const char* fullpath) {
  idStr result("generated/");
  result.Append(fullpath != nullptr ? fullpath : "");
  result.BackSlashesToSlashes();
  result.SetFileExtension("tga");
  return result;
}

} // namespace

void idSWF::WriteBinary(const char* bfilename) {
  std::unique_ptr<idFile> file(fileSystem != nullptr ? fileSystem->OpenFileWrite(bfilename) : nullptr);
  if (!file) return;

  WriteBE32(file.get(), BSWF_MAGIC);
  WriteBE32(file.get(), timestamp);
  WriteBEFloat(file.get(), frameWidth);
  WriteBEFloat(file.get(), frameHeight);
  WriteBE16(file.get(), frameRate);
  WriteBE32(file.get(), atlasWidth);
  WriteBE32(file.get(), atlasHeight);
  mainsprite->Write(file.get());

  WriteBE32(file.get(), dictionary.Num());
  for (int i = 0; i < dictionary.Num(); ++i) {
    idSWFDictionaryEntry& entry = dictionary[i];
    WriteBE32(file.get(), static_cast<std::int32_t>(entry.type));
    switch (entry.type) {
      case SWF_DICT_IMAGE:
        WriteString(file.get(), entry.material != nullptr ? entry.material->GetName() : ".");
        WriteBE32(file.get(), entry.imageSize.x);
        WriteBE32(file.get(), entry.imageAtlasOffset.x);
        WriteBE32(file.get(), entry.imageSize.y);
        WriteBE32(file.get(), entry.imageAtlasOffset.y);
        for (int channel = 0; channel < 4; ++channel)
          WriteBEFloat(file.get(), entry.channelScale[channel]);
        break;

      case SWF_DICT_SHAPE:
      case SWF_DICT_MORPH: {
        idSWFShape& shape = *entry.shape;
        WriteBEVec2(file.get(), shape.startBounds.tl);
        WriteBEVec2(file.get(), shape.startBounds.br);
        WriteBEVec2(file.get(), shape.endBounds.tl);
        WriteBEVec2(file.get(), shape.endBounds.br);
        WriteBE32(file.get(), shape.fillDraws.Num());
        for (int d = 0; d < shape.fillDraws.Num(); ++d) {
          idSWFShapeDrawFill& draw = shape.fillDraws[d];
          WriteU8(file.get(), draw.style.type);
          WriteU8(file.get(), draw.style.subType);
          WriteBytes(file.get(), &draw.style.startColor, 4);
          WriteBytes(file.get(), &draw.style.endColor, 4);
          const float* startMatrix = &draw.style.startMatrix.xx;
          const float* endMatrix = &draw.style.endMatrix.xx;
          WriteArray(file.get(), startMatrix, 6, WriteBEFloat);
          WriteArray(file.get(), endMatrix, 6, WriteBEFloat);
          WriteU8(file.get(), draw.style.gradient.numGradients);
          for (int g = 0; g < draw.style.gradient.numGradients; ++g) {
            const swfGradientRecord_t& record = draw.style.gradient.gradientRecords[g];
            WriteU8(file.get(), record.startRatio);
            WriteU8(file.get(), record.endRatio);
            WriteBytes(file.get(), &record.startColor, 4);
            WriteBytes(file.get(), &record.endColor, 4);
          }
          WriteBEFloat(file.get(), draw.style.focalPoint);
          WriteBE16(file.get(), draw.style.bitmapID);
          WriteBE32(file.get(), draw.startVerts.Num());
          WriteArray(file.get(), draw.startVerts.list, draw.startVerts.Num(), WriteBEVec2);
          WriteBE32(file.get(), draw.endVerts.Num());
          WriteArray(file.get(), draw.endVerts.list, draw.endVerts.Num(), WriteBEVec2);
          WriteBE32(file.get(), draw.indices.Num());
          WriteArray(file.get(), draw.indices.list, draw.indices.Num(), WriteBE16);
        }
        WriteBE32(file.get(), shape.lineDraws.Num());
        for (int d = 0; d < shape.lineDraws.Num(); ++d) {
          idSWFShapeDrawLine& draw = shape.lineDraws[d];
          WriteBE16(file.get(), draw.style.startWidth);
          WriteBE16(file.get(), draw.style.endWidth);
          WriteBytes(file.get(), &draw.style.startColor, 4);
          WriteBytes(file.get(), &draw.style.endColor, 4);
          WriteBE32(file.get(), draw.startVerts.Num());
          WriteArray(file.get(), draw.startVerts.list, draw.startVerts.Num(), WriteBEVec2);
          WriteBE32(file.get(), draw.endVerts.Num());
          WriteArray(file.get(), draw.endVerts.list, draw.endVerts.Num(), WriteBEVec2);
          WriteBE32(file.get(), draw.indices.Num());
          WriteArray(file.get(), draw.indices.list, draw.indices.Num(), WriteBE16);
        }
        break;
      }

      case SWF_DICT_SPRITE:
        entry.sprite->Write(file.get());
        break;

      case SWF_DICT_FONT: {
        idSWFFont& font = *entry.font;
        WriteString(file.get(), font.fontID != nullptr ? font.fontID->GetName() : "");
        WriteBE16(file.get(), static_cast<std::uint16_t>(font.ascent));
        WriteBE16(file.get(), static_cast<std::uint16_t>(font.descent));
        WriteBE16(file.get(), static_cast<std::uint16_t>(font.leading));
        WriteBE32(file.get(), font.glyphs.Num());
        for (int g = 0; g < font.glyphs.Num(); ++g) {
          idSWFFontGlyph& glyph = font.glyphs[g];
          WriteBE16(file.get(), glyph.code);
          WriteBE16(file.get(), static_cast<std::uint16_t>(glyph.advance));
          WriteBE32(file.get(), glyph.verts.Num());
          WriteArray(file.get(), glyph.verts.list, glyph.verts.Num(), WriteBEVec2);
          WriteBE32(file.get(), glyph.indices.Num());
          WriteArray(file.get(), glyph.indices.list, glyph.indices.Num(), WriteBE16);
        }
        break;
      }

      case SWF_DICT_TEXT: {
        idSWFText& text = *entry.text;
        WriteBEVec2(file.get(), text.bounds.tl);
        WriteBEVec2(file.get(), text.bounds.br);
        WriteArray(file.get(), &text.matrix.xx, 6, WriteBEFloat);
        WriteBE32(file.get(), text.textRecords.Num());
        for (int t = 0; t < text.textRecords.Num(); ++t) {
          idSWFTextRecord& record = text.textRecords[t];
          WriteBE16(file.get(), record.fontID);
          WriteBytes(file.get(), &record.color, 4);
          WriteBE16(file.get(), static_cast<std::uint16_t>(record.xOffset));
          WriteBE16(file.get(), static_cast<std::uint16_t>(record.yOffset));
          WriteBE16(file.get(), record.textHeight);
          WriteBE16(file.get(), record.firstGlyph);
          WriteU8(file.get(), record.numGlyphs);
        }
        WriteBE32(file.get(), text.glyphs.Num());
        for (int g = 0; g < text.glyphs.Num(); ++g) {
          WriteBE32(file.get(), text.glyphs[g].index);
          WriteBE32(file.get(), text.glyphs[g].advance);
        }
        break;
      }

      case SWF_DICT_EDITTEXT: {
        idSWFEditText& edit = *entry.edittext;
        WriteBEVec2(file.get(), edit.bounds.tl);
        WriteBEVec2(file.get(), edit.bounds.br);
        WriteBE32(file.get(), static_cast<std::int32_t>(edit.flags));
        WriteBE16(file.get(), edit.fontID);
        WriteBE16(file.get(), edit.fontHeight);
        WriteBytes(file.get(), &edit.color, 4);
        WriteBE16(file.get(), edit.maxLength);
        WriteBE32(file.get(), static_cast<std::int32_t>(edit.align));
        WriteBE16(file.get(), edit.leftMargin);
        WriteBE16(file.get(), edit.rightMargin);
        WriteBE16(file.get(), edit.indent);
        WriteBE16(file.get(), static_cast<std::uint16_t>(edit.leading));
        WriteString(file.get(), edit.variable.str);
        WriteString(file.get(), edit.initialText.str);
        break;
      }

      default:
        break;
    }
  }
}

bool idSWF::LoadSWF(const char* fullpath, bool) {
  std::unique_ptr<idFile> rawfile(fileSystem != nullptr ? fileSystem->OpenFileRead(fullpath) : nullptr);
  if (!rawfile) {
    idLib::Printf("SWF File not found %s\n", fullpath);
    return false;
  }

  swfHeader_t header{};
  if (!ReadBytes(rawfile.get(), &header, sizeof(header)) || header.W != 'W' || header.S != 'S') {
    idLib::Warning("Wrong signature bytes");
    return false;
  }
  if (header.version > 9) {
    idLib::Warning("Unsupported version %d", header.version);
    return false;
  }
  const bool compressed = header.compression == 'C';
  if (!compressed && header.compression != 'F') {
    idLib::Warning("Unsupported compression type %c", header.compression);
    return false;
  }
  if (header.fileLength < sizeof(header)) return false;

  const std::uint32_t payloadLength = header.fileLength - sizeof(header);
  std::vector<std::uint8_t> source(payloadLength);
  const std::size_t sourceSize = rawfile->Read(source.data(), source.size());
  rawfile.reset();
  std::vector<std::uint8_t> inflated;
  const std::uint8_t* fileData = source.data();
  if (compressed) {
    inflated.resize(payloadLength);
    if (!Inflate(source.data(), static_cast<unsigned int>(sourceSize), inflated.data(), payloadLength)) {
      idLib::Warning("Inflate error");
      return false;
    }
    fileData = inflated.data();
  }

  idSWFBitStream bitstream(fileData, payloadLength, false);
  swfRect_t frameSize;
  bitstream.ReadRect(frameSize);
  if (frameSize.tl.x != 0.0f || frameSize.tl.y != 0.0f) {
    idLib::Warning("Invalid frameSize top left");
    return false;
  }
  frameWidth = frameSize.br.x;
  frameHeight = frameSize.br.y;
  frameRate = bitstream.ReadU16();
  mainsprite->Load(bitstream, this);
  if (!JapaneseLanguageEnabled()) dictionary.SetNum(dictionary.Num() + 2);
  const idStr atlasName = AtlasFilename(fullpath);
  WriteSwfImageAtlas(atlasName.data);
  return true;
}

bool idSWF::LoadBinary(const char* bfilename) {
  std::unique_ptr<idFile> file(fileSystem != nullptr ? fileSystem->OpenFileRead(bfilename) : nullptr);
  if (!file || file->Length() <= 0) return false;

  std::uint32_t magic = 0;
  std::uint32_t binaryTimestamp = 0;
  if (!ReadBE32(file.get(), magic) || !ReadBE32(file.get(), binaryTimestamp)) return false;
  if (idLib::production == PROD_DEVELOPMENT && timestamp != binaryTimestamp) return false;
  if (magic != BSWF_MAGIC) return false;
  if (!ReadBEFloat(file.get(), frameWidth) || !ReadBEFloat(file.get(), frameHeight) ||
      !ReadBE16(file.get(), frameRate) || !ReadBE32(file.get(), atlasWidth) ||
      !ReadBE32(file.get(), atlasHeight)) return false;

  if (mouseX == -1) mouseX = static_cast<int>(frameWidth * 0.5f);
  if (mouseY == -1) mouseY = static_cast<int>(frameHeight * 0.5f);
  mainsprite->Read(file.get());

  int storedDictionaryCount = 0;
  if (!ReadCount(file.get(), storedDictionaryCount)) return false;
  dictionary.SetNum(storedDictionaryCount);
  if (!JapaneseLanguageEnabled()) dictionary.SetNum(storedDictionaryCount + 2);

  for (int i = 0; i < dictionary.Num(); ++i) {
    std::int32_t type = 0;
    if (!ReadBE32(file.get(), type)) return false;
    idSWFDictionaryEntry& entry = dictionary[i];
    entry.type = static_cast<swfDictType_t>(type);
    switch (entry.type) {
      case SWF_DICT_IMAGE: {
        idStr materialName;
        if (!ReadString(file.get(), materialName)) return false;
        entry.material = materialName[0] == '.' ? nullptr
            : const_cast<idMaterial*>(FindMaterialOrAutoMaterial(materialName.data));
        if (!ReadBE32(file.get(), entry.imageSize.x) ||
            !ReadBE32(file.get(), entry.imageAtlasOffset.x) ||
            !ReadBE32(file.get(), entry.imageSize.y) ||
            !ReadBE32(file.get(), entry.imageAtlasOffset.y)) return false;
        for (int channel = 0; channel < 4; ++channel)
          if (!ReadBEFloat(file.get(), entry.channelScale[channel])) return false;
        break;
      }

      case SWF_DICT_SHAPE:
      case SWF_DICT_MORPH: {
        entry.shape = new idSWFShape;
        idSWFShape& shape = *entry.shape;
        if (!ReadBEVec2(file.get(), shape.startBounds.tl) ||
            !ReadBEVec2(file.get(), shape.startBounds.br) ||
            !ReadBEVec2(file.get(), shape.endBounds.tl) ||
            !ReadBEVec2(file.get(), shape.endBounds.br)) return false;
        int count = 0;
        if (!ReadCount(file.get(), count)) return false;
        shape.fillDraws.SetNum(count);
        for (int d = 0; d < shape.fillDraws.Num(); ++d) {
          idSWFShapeDrawFill& draw = shape.fillDraws[d];
          if (!ReadU8(file.get(), draw.style.type) || !ReadU8(file.get(), draw.style.subType) ||
              !ReadBytes(file.get(), &draw.style.startColor, 4) ||
              !ReadBytes(file.get(), &draw.style.endColor, 4) ||
              !ReadArray(file.get(), &draw.style.startMatrix.xx, 6, ReadBEFloat) ||
              !ReadArray(file.get(), &draw.style.endMatrix.xx, 6, ReadBEFloat) ||
              !ReadU8(file.get(), draw.style.gradient.numGradients)) return false;
          for (int g = 0; g < draw.style.gradient.numGradients; ++g) {
            swfGradientRecord_t& record = draw.style.gradient.gradientRecords[g];
            if (!ReadU8(file.get(), record.startRatio) || !ReadU8(file.get(), record.endRatio) ||
                !ReadBytes(file.get(), &record.startColor, 4) ||
                !ReadBytes(file.get(), &record.endColor, 4)) return false;
          }
          if (!ReadBEFloat(file.get(), draw.style.focalPoint) ||
              !ReadBE16(file.get(), draw.style.bitmapID) || !ReadCount(file.get(), count)) return false;
          draw.startVerts.SetNum(count);
          if (!ReadArray(file.get(), draw.startVerts.list, count, ReadBEVec2) ||
              !ReadCount(file.get(), count)) return false;
          draw.endVerts.SetNum(count);
          if (!ReadArray(file.get(), draw.endVerts.list, count, ReadBEVec2) ||
              !ReadCount(file.get(), count)) return false;
          draw.indices.SetNum(count);
          if (!ReadArray(file.get(), draw.indices.list, count, ReadBE16)) return false;
        }
        if (!ReadCount(file.get(), count)) return false;
        shape.lineDraws.SetNum(count);
        for (int d = 0; d < shape.lineDraws.Num(); ++d) {
          idSWFShapeDrawLine& draw = shape.lineDraws[d];
          if (!ReadBE16(file.get(), draw.style.startWidth) ||
              !ReadBE16(file.get(), draw.style.endWidth) ||
              !ReadBytes(file.get(), &draw.style.startColor, 4) ||
              !ReadBytes(file.get(), &draw.style.endColor, 4) || !ReadCount(file.get(), count)) return false;
          draw.startVerts.SetNum(count);
          if (!ReadArray(file.get(), draw.startVerts.list, count, ReadBEVec2) ||
              !ReadCount(file.get(), count)) return false;
          draw.endVerts.SetNum(count);
          if (!ReadArray(file.get(), draw.endVerts.list, count, ReadBEVec2) ||
              !ReadCount(file.get(), count)) return false;
          draw.indices.SetNum(count);
          if (!ReadArray(file.get(), draw.indices.list, count, ReadBE16)) return false;
        }
        break;
      }

      case SWF_DICT_SPRITE:
        entry.sprite = new idSWFSprite;
        entry.sprite->Read(file.get());
        break;

      case SWF_DICT_FONT: {
        entry.font = new idSWFFont;
        idStr fontName;
        if (!ReadString(file.get(), fontName)) return false;
        entry.font->fontID = static_cast<idFont*>(idFont::resourceList.Load(fontName.data, true, false));
        std::uint16_t value = 0;
        if (!ReadBE16(file.get(), value)) return false;
        entry.font->ascent = static_cast<std::int16_t>(value);
        if (!ReadBE16(file.get(), value)) return false;
        entry.font->descent = static_cast<std::int16_t>(value);
        if (!ReadBE16(file.get(), value)) return false;
        entry.font->leading = static_cast<std::int16_t>(value);
        int count = 0;
        if (!ReadCount(file.get(), count)) return false;
        entry.font->glyphs.SetNum(count);
        for (int g = 0; g < entry.font->glyphs.Num(); ++g) {
          idSWFFontGlyph& glyph = entry.font->glyphs[g];
          if (!ReadBE16(file.get(), glyph.code) || !ReadBE16(file.get(), value)) return false;
          glyph.advance = static_cast<std::int16_t>(value);
          if (!ReadCount(file.get(), count)) return false;
          glyph.verts.SetNum(count);
          if (!ReadArray(file.get(), glyph.verts.list, count, ReadBEVec2) ||
              !ReadCount(file.get(), count)) return false;
          glyph.indices.SetNum(count);
          if (!ReadArray(file.get(), glyph.indices.list, count, ReadBE16)) return false;
        }
        break;
      }

      case SWF_DICT_TEXT: {
        entry.text = new idSWFText;
        idSWFText& text = *entry.text;
        if (!ReadBEVec2(file.get(), text.bounds.tl) || !ReadBEVec2(file.get(), text.bounds.br) ||
            !ReadArray(file.get(), &text.matrix.xx, 6, ReadBEFloat)) return false;
        int count = 0;
        if (!ReadCount(file.get(), count)) return false;
        text.textRecords.SetNum(count);
        for (int t = 0; t < text.textRecords.Num(); ++t) {
          idSWFTextRecord& record = text.textRecords[t];
          std::uint16_t value = 0;
          if (!ReadBE16(file.get(), record.fontID) || !ReadBytes(file.get(), &record.color, 4) ||
              !ReadBE16(file.get(), value)) return false;
          record.xOffset = static_cast<std::int16_t>(value);
          if (!ReadBE16(file.get(), value)) return false;
          record.yOffset = static_cast<std::int16_t>(value);
          if (!ReadBE16(file.get(), record.textHeight) || !ReadBE16(file.get(), record.firstGlyph) ||
              !ReadU8(file.get(), record.numGlyphs)) return false;
        }
        if (!ReadCount(file.get(), count)) return false;
        text.glyphs.SetNum(count);
        for (int g = 0; g < count; ++g)
          if (!ReadBE32(file.get(), text.glyphs[g].index) ||
              !ReadBE32(file.get(), text.glyphs[g].advance)) return false;
        break;
      }

      case SWF_DICT_EDITTEXT: {
        entry.edittext = new idSWFEditText;
        idSWFEditText& edit = *entry.edittext;
        std::int32_t intValue = 0;
        std::uint16_t shortValue = 0;
        if (!ReadBEVec2(file.get(), edit.bounds.tl) || !ReadBEVec2(file.get(), edit.bounds.br) ||
            !ReadBE32(file.get(), intValue)) return false;
        edit.flags = static_cast<std::uint32_t>(intValue);
        if (!ReadBE16(file.get(), edit.fontID) || !ReadBE16(file.get(), edit.fontHeight) ||
            !ReadBytes(file.get(), &edit.color, 4) || !ReadBE16(file.get(), edit.maxLength) ||
            !ReadBE32(file.get(), intValue)) return false;
        edit.align = static_cast<swfEditTextAlign_t>(intValue);
        if (!ReadBE16(file.get(), edit.leftMargin) || !ReadBE16(file.get(), edit.rightMargin) ||
            !ReadBE16(file.get(), edit.indent) || !ReadBE16(file.get(), shortValue)) return false;
        edit.leading = static_cast<std::int16_t>(shortValue);
        if (!ReadString(file.get(), edit.variable) || !ReadString(file.get(), edit.initialText)) return false;
        break;
      }

      default:
        break;
    }
  }
  return true;
}
