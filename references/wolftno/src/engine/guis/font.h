#pragma once

// Renderer-facing material loading remains at the renderer module boundary;
// the font ABI and this GUI compiland are recovered here.

#include "engine/decls/decl.h"

#include <cstdint>

class idMaterial;

struct glyphInfo_t {
  std::uint8_t width;
  std::uint8_t height;
  std::int8_t top;
  std::int8_t left;
  std::uint8_t xSkip;
  std::uint8_t padding;
  std::uint16_t s;
  std::uint16_t t;
  std::int16_t kerningIndex;
};

struct kerningpair_t {
  std::uint16_t leftIndex;
  std::uint16_t rightIndex;
  float kerning;
};

struct fontInfo_t {
  void AllocateGlyphs(int num);
  void AllocateKerning(int num);
  idMaterial* material;
  std::int16_t imageWidth;
  std::int16_t imageHeight;
  std::int16_t pointSize;
  std::int16_t ascender;
  std::int16_t descender;
  std::int16_t numGlyphs;
  std::uint32_t padding;
  glyphInfo_t* glyphData;
  std::uint32_t* charIndex;
  std::int16_t numKerning;
  std::uint8_t padding2[6];
  kerningpair_t* kerningPairs;
};

class idFont : public idResource {
public:
  idFont() : alias(nullptr), fontInfo(nullptr), timestamp(~0u) {}
  ~idFont() override;
  void LoadResource() override;
  bool ReloadIfStale() override;
  idResourceList* GetResourceList() const override;

  idMaterial* GetMaterial() const;
  std::uint32_t GetMaterialWidth() const;
  std::uint32_t GetMaterialHeight() const;
  std::uint32_t GetPointSize() const;
  int GetAscender() const;
  int GetDescender() const;
  glyphInfo_t& GetGlyph(std::uint32_t index) const;
  float GetKerning(std::uint32_t left, std::uint32_t right) const;
  static idFont* RemapFont(const char* baseName);

  static idTypedResourceList<idFont> resourceList;
  static const char* DEFAULT_FONT;

  idFont* alias;
  fontInfo_t* fontInfo;
  std::uint32_t timestamp;
  std::uint32_t padding;
};

static_assert(sizeof(glyphInfo_t) == 12, "glyphInfo_t ABI drift");
static_assert(sizeof(kerningpair_t) == 8, "kerningpair_t ABI drift");
static_assert(sizeof(fontInfo_t) == 56, "fontInfo_t ABI drift");
static_assert(sizeof(idFont) == 72, "idFont ABI drift");
