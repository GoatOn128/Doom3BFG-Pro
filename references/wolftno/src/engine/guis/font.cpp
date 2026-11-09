#include "engine/guis/font.h"

#include "engine/framework/resourcemanager.h"
#include "engine/renderer/declmaterial.h"
#include "shared/idlib/csystems/cvarsystemlocal.h"
#include "shared/idlib/filesystem/filesystemlocal.h"
#include "shared/idlib/langdict.h"
#include "shared/idlib/lib.h"
#include "shared/idlib/text/str.h"

#include <algorithm>

namespace {
glyphInfo_t defaultGlyph{};

int BinaryFind(const std::uint32_t* values, int count, std::uint32_t value) {
  if (values == nullptr || count <= 0) return -1;
  const std::uint32_t* found = std::lower_bound(values, values + count, value);
  return found != values + count && *found == value ? static_cast<int>(found - values) : -1;
}

template <typename T> bool ReadBig(idFile& file, T& value) {
  if (file.Read(&value, sizeof(value)) != sizeof(value)) return false;
  auto* bytes = reinterpret_cast<std::uint8_t*>(&value);
  std::reverse(bytes, bytes + sizeof(value));
  return true;
}

void FreeFontInfo(fontInfo_t*& info) {
  if (info == nullptr) return;
  delete[] info->glyphData;
  delete[] info->charIndex;
  delete[] info->kerningPairs;
  delete info;
  info = nullptr;
}
}

idTypedResourceList<idFont> idFont::resourceList("font");
const char* idFont::DEFAULT_FONT = "courier";

void fontInfo_t::AllocateGlyphs(int num) {
  delete[] glyphData;
  delete[] charIndex;
  numGlyphs = static_cast<std::int16_t>(num);
  charIndex = num > 0 ? new std::uint32_t[num] : nullptr;
  glyphData = num > 0 ? new glyphInfo_t[num] : nullptr;
}

void fontInfo_t::AllocateKerning(int num) {
  delete[] kerningPairs;
  numKerning = static_cast<std::int16_t>(num);
  kerningPairs = num > 0 ? new kerningpair_t[num] : nullptr;
}

idMaterial* idFont::GetMaterial() const {
  const idFont* font = this;
  while (font->alias != nullptr) font = font->alias;
  return font->fontInfo != nullptr ? font->fontInfo->material : nullptr;
}

std::uint32_t idFont::GetMaterialWidth() const {
  const idFont* font = this;
  while (font->alias != nullptr) font = font->alias;
  return font->fontInfo != nullptr ? static_cast<std::uint32_t>(font->fontInfo->imageWidth) : 1u;
}

std::uint32_t idFont::GetMaterialHeight() const {
  const idFont* font = this;
  while (font->alias != nullptr) font = font->alias;
  return font->fontInfo != nullptr ? static_cast<std::uint32_t>(font->fontInfo->imageHeight) : 1u;
}

std::uint32_t idFont::GetPointSize() const {
  const idFont* font = this;
  while (font->alias != nullptr) font = font->alias;
  return font->fontInfo != nullptr ? static_cast<std::uint32_t>(font->fontInfo->pointSize) : 0u;
}

int idFont::GetAscender() const {
  const idFont* font = this;
  while (font->alias != nullptr) font = font->alias;
  return font->fontInfo != nullptr ? font->fontInfo->ascender : 0;
}

int idFont::GetDescender() const {
  const idFont* font = this;
  while (font->alias != nullptr) font = font->alias;
  return font->fontInfo != nullptr ? font->fontInfo->descender : 0;
}

idFont* idFont::RemapFont(const char* baseName) {
  idStr cleanName(baseName != nullptr ? baseName : "");
  const bool changedDash = cleanName.ReplaceChar('-', '_');
  const bool changed = cleanName.ReplaceChar(' ', '_') || changedDash;
  if (idStr::Icmp(cleanName.data, DEFAULT_FONT) == 0) return nullptr;
  const char* language = cvarSystem != nullptr ? cvarSystem->GetCVarString("sys_lang", "english") : "english";
  if (idStr::Icmp(language, "russian") == 0 &&
      idStr::Icmp(cleanName.data, "japanese/futura_com_medium_jpn90") == 0) return nullptr;
  idStr key("#font_");
  key.Append(cleanName);
  const char* remapped = idLocalization::languageDict.FindString(key.data);
  if (remapped != nullptr) return static_cast<idFont*>(resourceList.Load(remapped, true, false));
  const char* wildcard = idLocalization::languageDict.FindString("#font_*");
  if (wildcard != nullptr && idStr::Icmp(cleanName.data, wildcard) != 0) {
    return static_cast<idFont*>(resourceList.Load(wildcard, true, false));
  }
  return changed ? static_cast<idFont*>(resourceList.Load(cleanName.data, true, false)) : nullptr;
}

bool idFont::ReloadIfStale() {
  if (alias != RemapFont(name.str)) {
    LoadResource();
    return true;
  }
  idStr fontName;
  fontName.Format("fonts/%s/48.dat", name.str);
  if (cvarSystem != nullptr && cvarSystem->GetCVarInteger("r_useDistanceFont", 0) > 0)
    fontName.Format("fonts/%s/64_df.dat", name.str);
  if (timestamp == ~0u || fileSystem == nullptr || timestamp != fileSystem->GetTimestamp(fontName.data)) {
    LoadResource();
    return true;
  }
  return false;
}

glyphInfo_t& idFont::GetGlyph(std::uint32_t index) const {
  if (alias != nullptr) return alias->GetGlyph(index);
  if (fontInfo != nullptr && fontInfo->glyphData != nullptr && fontInfo->charIndex != nullptr &&
      fontInfo->numGlyphs != 0) {
    int found = BinaryFind(fontInfo->charIndex, fontInfo->numGlyphs, index);
    if (found >= 0) return fontInfo->glyphData[found];
    found = BinaryFind(fontInfo->charIndex, fontInfo->numGlyphs, 42u);
    if (found >= 0) return fontInfo->glyphData[found];
  }
  return defaultGlyph;
}

float idFont::GetKerning(std::uint32_t left, std::uint32_t right) const {
  if (left == ~0u || right == ~0u) return 0.0f;
  if (alias != nullptr) return alias->GetKerning(left, right);
  glyphInfo_t& glyph = GetGlyph(left);
  if (glyph.kerningIndex == -1 || fontInfo == nullptr || fontInfo->kerningPairs == nullptr) return 0.0f;
  const int leftIndex = BinaryFind(fontInfo->charIndex, fontInfo->numGlyphs, left);
  const int rightIndex = BinaryFind(fontInfo->charIndex, fontInfo->numGlyphs, right);
  for (int i = glyph.kerningIndex; i < fontInfo->numKerning; ++i) {
    const kerningpair_t& pair = fontInfo->kerningPairs[i];
    if (pair.leftIndex != leftIndex) break;
    if (pair.rightIndex == rightIndex) return pair.kerning;
  }
  return 0.0f;
}

void idFont::LoadResource() {
  timestamp = ~0u;
  FreeFontInfo(fontInfo);
  alias = RemapFont(name.str);
  if (alias != nullptr) {
    loadedResource = 0;
    return;
  }
  idStr fontName;
  fontName.Format("fonts/%s/48.dat", name.str);
  if (cvarSystem != nullptr && cvarSystem->GetCVarInteger("r_useDistanceFont", 0) > 0)
    fontName.Format("fonts/%s/64_df.dat", name.str);
  if (idLib::production >= PROD_PRODUCTION && resourceManager != nullptr &&
      resourceManager->ExcludeFont(fontName.data)) {
    return;
  }
  idFile* file = fileSystem != nullptr ? fileSystem->OpenFileRead(fontName.data) : nullptr;
  if (file == nullptr) {
    fontName.Format("fonts/%s/12.dat", name.str);
    file = fileSystem != nullptr ? fileSystem->OpenFileRead(fontName.data) : nullptr;
  }
  if (file == nullptr) {
    SetResourceError("Couldn't open '%s'", fontName.data);
    if (idStr::Icmp(name.str, DEFAULT_FONT) == 0) {
      idLib::Error("Couldn't load default font");
    } else {
      alias = static_cast<idFont*>(resourceList.Load(DEFAULT_FONT, true, false));
    }
    return;
  }
  std::uint32_t magic = 0;
  if (!ReadBig(*file, magic) || (magic & 0xffffff00u) != 0x69646600u || (magic & 0xffu) < 42u) {
    SetResourceError("Wrong Version for idFont '%s'", name.str);
    if (idStr::Icmp(name.str, DEFAULT_FONT) == 0) {
      idLib::Error("Couldn't load default font");
    } else {
      alias = static_cast<idFont*>(resourceList.Load(DEFAULT_FONT, true, false));
    }
    delete file;
    return;
  }
  const std::uint8_t version = static_cast<std::uint8_t>(magic);
  timestamp = file->Timestamp();
  fontInfo = new fontInfo_t{};
  ReadBig(*file, fontInfo->pointSize);
  ReadBig(*file, fontInfo->ascender);
  ReadBig(*file, fontInfo->descender);
  ReadBig(*file, fontInfo->numGlyphs);
  fontInfo->AllocateGlyphs(fontInfo->numGlyphs);
  if (version == 42) {
    for (int i = 0; i < fontInfo->numGlyphs; ++i) {
      file->Read(&fontInfo->glyphData[i], 10);
      fontInfo->glyphData[i].kerningIndex = -1;
    }
  } else {
    file->Read(fontInfo->glyphData, sizeof(glyphInfo_t) * fontInfo->numGlyphs);
  }
  file->Read(fontInfo->charIndex, sizeof(std::uint32_t) * fontInfo->numGlyphs);
  if (version >= 43) {
    ReadBig(*file, fontInfo->numKerning);
    fontInfo->AllocateKerning(fontInfo->numKerning);
    file->Read(fontInfo->kerningPairs, sizeof(kerningpair_t) * fontInfo->numKerning);
  }
  delete file;
  idStr textureName(fontName);
  textureName.SetFileExtension("tga");
  char remappedMaterial[512];
  const char* materialName =
      idMaterial::resourceList.RemapResourceName(remappedMaterial, sizeof(remappedMaterial),
                                                 textureName.data);
  fontInfo->material =
      static_cast<idMaterial*>(idMaterial::resourceList.Load(materialName, true, false));
  if (fontInfo->material != nullptr) {
    if (idLib::production < PROD_PRODUCTION) fontInfo->material->ReloadImages(false);
    fontInfo->imageWidth = static_cast<std::int16_t>(fontInfo->material->GetWidth());
    fontInfo->imageHeight = static_cast<std::int16_t>(fontInfo->material->GetHeight());
  }
}

idResourceList* idFont::GetResourceList() const { return &resourceList; }
idFont::~idFont() { FreeFontInfo(fontInfo); }
