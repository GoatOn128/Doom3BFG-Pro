#include "engine/guis/devicecontext.h"

#include "engine/guis/font.h"
#include "engine/renderer/declmaterial.h"
#include "engine/renderer/rendermodelgui.h"
#include "shared/idlib/color.h"
#include "shared/idlib/text/str.h"

#include <cmath>
#include <cstring>

namespace {

void SetPacked(std::uint8_t (&bytes)[4], std::uint32_t value) {
  std::memcpy(bytes, &value, sizeof(value));
}

} // namespace

void idDeviceContext::SetFont(const idFont* font) { activeFont = font; }

void idDeviceContext::DrawStretchPic(float x, float y, float width, float height,
                                     float s1, float t1, float s2, float t2,
                                     const idMaterial* shader, float,
                                     alignment_t align) {
  if (align == ALIGN_RIGHT)
    x -= width;
  else if (align == ALIGN_CENTER)
    x -= width * 0.5f;

  if (identityMatrix) {
    gui->DrawStretchPic(uiScale.x * x + uiOffset.x, uiScale.y * y + uiOffset.y,
                        0.0f, uiScale.x * width, uiScale.y * height,
                        s1, t1, s2, t2, shader);
    return;
  }

  const idVec2 positions[4] = {
      {x, y}, {x + width, y}, {x + width, y + height}, {x, y + height}};
  const idVec2 texCoords[4] = {{s1, t1}, {s2, t1}, {s2, t2}, {s1, t2}};
  for (int i = 0; i < 4; ++i) {
    idDrawVert& vertex = stretchPicVerts[i];
    vertex.xyz = {positions[i].x, positions[i].y, 0.0f};
    vertex.st = texCoords[i];
    SetPacked(vertex.color, gui->currentVertexColor);
    SetPacked(vertex.tangent, 0x7F7F7F7Fu);

    const idVec3 local = vertex.xyz - origin;
    vertex.xyz = axis * local + origin;
    vertex.xyz.x = vertex.xyz.x * uiScale.x + uiOffset.x;
    vertex.xyz.y = vertex.xyz.y * uiScale.y + uiOffset.y;
  }
  gui->DrawTris(stretchPicVerts, 4, stretchPicIndices, 6, shader);
}

int idDeviceContext::TextWidth(float scale, const char* text, int limit) const {
  if (text == nullptr)
    return 0;
  const float fontScale = scale * 48.0f / static_cast<float>(activeFont->GetPointSize());
  if (limit <= 0)
    limit = 999999999;

  float width = 0.0f;
  int index = 0;
  while (text[index] != '\0' && index < limit) {
    if (idStr::IsColor(text + index)) {
      ++index;
    } else {
      const std::uint32_t codePoint =
          idStr::UTF8Char(reinterpret_cast<const std::uint8_t*>(text), &index);
      width += activeFont->GetGlyph(codePoint).xSkip * fontScale;
    }
  }
  return static_cast<int>(width + 0.5f);
}

int idDeviceContext::TextHeight(float scale) const {
  const float fontScale = scale * 48.0f / static_cast<float>(activeFont->GetPointSize());
  return static_cast<int>((activeFont->GetAscender() - activeFont->GetDescender()) * fontScale);
}

idDeviceContext::idDeviceContext(idRenderModelGui* guiModel)
    : gui(guiModel),
      whiteMaterial(static_cast<const idMaterial*>(
          idMaterial::resourceList.FindWithInheritance("_white", true, true))),
      activeFont(static_cast<const idFont*>(
          idFont::resourceList.Load(idFont::DEFAULT_FONT, true, false))),
      uiScale{1.0f, 1.0f}, uiOffset{0.0f, 0.0f}, axis(mat3_identity),
      origin{0.0f, 0.0f, 0.0f}, identityMatrix(true) {
  std::memset(stretchPicVerts, 0, sizeof(stretchPicVerts));
  for (idDrawVert& vertex : stretchPicVerts)
    SetPacked(vertex.color, 0xFFFFFFFFu);
  for (int vertex = 2, index = 0; vertex < 32; ++vertex) {
    stretchPicIndices[index++] = 0;
    stretchPicIndices[index++] = static_cast<std::uint16_t>(vertex - 1);
    stretchPicIndices[index++] = static_cast<std::uint16_t>(vertex);
  }
}

void idDeviceContext::DrawWindingMaterial(idWinding2D* winding,
                                          const idMaterial* material,
                                          const idVec4& color) {
  gui->currentVertexColor = idColor::PackColor(color);
  for (int i = 0; i < winding->numPoints; ++i) {
    idDrawVert& vertex = stretchPicVerts[i];
    vertex.xyz.x = winding->p[i].x * uiScale.x + uiOffset.x;
    vertex.xyz.y = winding->p[i].y * uiScale.y + uiOffset.y;
    vertex.st = winding->st[i];
    SetPacked(vertex.tangent, 0x7F7F7F7Fu);
    SetPacked(vertex.color, gui->currentVertexColor);
  }
  gui->DrawTris(stretchPicVerts, winding->numPoints, stretchPicIndices,
                 3 * winding->numPoints - 6, material);
}

void idDeviceContext::DrawFilledRect(float x, float y, float width, float height,
                                     const idVec4& color) {
  if (color.w == 0.0f)
    return;
  gui->currentVertexColor = idColor::PackColor(color);
  DrawStretchPic(x, y, width, height, 0.0f, 0.0f, 0.0f, 0.0f,
                 whiteMaterial, 0.0f, ALIGN_LEFT);
}

void idDeviceContext::DrawRect(float x, float y, float width, float height,
                               float size, const idVec4& color) {
  if (color.w == 0.0f || size == 0.0f)
    return;
  gui->currentVertexColor = idColor::PackColor(color);
  DrawStretchPic(x, y + size, size, height - size * 2.0f,
                 0, 0, 0, 0, whiteMaterial);
  DrawStretchPic(x + width - size, y + size, size, height - size * 2.0f,
                 0, 0, 0, 0, whiteMaterial);
  DrawStretchPic(x, y, width, size, 0, 0, 0, 0, whiteMaterial);
  DrawStretchPic(x, y + height - size, width, size, 0, 0, 0, 0, whiteMaterial);
}

int idDeviceContext::DrawTextA(float x, float y, float scale, const idVec4& color,
                               const char* text) {
  if (text == nullptr || color.w == 0.0f)
    return 0;
  const float fontScale = scale * 48.0f / static_cast<float>(activeFont->GetPointSize());
  const float inverseWidth = 1.0f / static_cast<float>(activeFont->GetMaterialWidth());
  const float inverseHeight = 1.0f / static_cast<float>(activeFont->GetMaterialHeight());
  gui->currentVertexColor = idColor::PackColor(color);

  const int length = idStr::Length(text);
  int index = 0;
  while (index < length) {
    const std::uint32_t codePoint =
        idStr::UTF8Char(reinterpret_cast<const std::uint8_t*>(text), &index);
    const glyphInfo_t& glyph = activeFont->GetGlyph(codePoint);
    if (idStr::IsColor(text + index)) {
      idVec4 newColor = color;
      if (text[index + 1] != '0') {
        newColor = idStr::ColorForIndex(text[index + 1]);
        newColor.w = color.w;
      }
      gui->currentVertexColor = idColor::PackColor(newColor);
    } else {
      DrawStretchPic(x + glyph.left * fontScale, y - glyph.top * fontScale,
                     (glyph.width + 1.0f) * fontScale,
                     (glyph.height + 1.0f) * fontScale,
                     (glyph.s - 0.5f) * inverseWidth,
                     (glyph.t - 0.5f) * inverseHeight,
                     (glyph.s + glyph.width + 0.5f) * inverseWidth,
                     (glyph.t + glyph.height + 0.5f) * inverseHeight,
                     activeFont->GetMaterial());
      x += glyph.xSkip * fontScale;
    }
  }
  return length;
}

int idDeviceContext::DrawShadowText(float x, float y, float scale,
                                    const idVec4& color, const char* text) {
  const idVec4 black{0.0f, 0.0f, 0.0f, color.w};
  const int count = DrawTextA(x, y, scale, black, text);
  DrawTextA(x - scale * 1.5f, y - scale * 1.5f, scale, color, text);
  return count;
}

void idDeviceContext::DrawLine(const idVec2& start, const idVec2& end,
                               float width, const idVec4& color) {
  const idVec2 delta{end.x - start.x, end.y - start.y};
  const float inverseLength = 1.0f / std::sqrt(
      delta.x * delta.x + delta.y * delta.y < 1.17549435e-38f
          ? 1.17549435e-38f
          : delta.x * delta.x + delta.y * delta.y);
  const idVec2 perpendicular{delta.y * inverseLength * width,
                             -delta.x * inverseLength * width};
  idWinding2D line{};
  line.numPoints = 4;
  line.p[0] = {start.x - perpendicular.x, start.y - perpendicular.y};
  line.p[1] = {start.x + perpendicular.x, start.y + perpendicular.y};
  line.p[2] = {end.x + perpendicular.x, end.y + perpendicular.y};
  line.p[3] = {end.x - perpendicular.x, end.y - perpendicular.y};
  line.st[0] = {1.0f, 0.0f};
  line.st[1] = {0.0f, 0.0f};
  line.st[2] = {0.0f, 1.0f};
  line.st[3] = {1.0f, 1.0f};
  DrawWindingMaterial(&line, whiteMaterial, color);
}
