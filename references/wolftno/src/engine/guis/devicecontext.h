#pragma once

#include "shared/idlib/geometry/drawvert.h"
#include "shared/idlib/math/vector.h"

#include <cstdint>

class idFont;
class idMaterial;
class idRenderModelGui;

struct idWinding2D {
  int numPoints;
  idVec2 p[32];
  idVec2 st[32];
};

class alignas(16) idDeviceContext {
public:
  enum alignment_t {
    ALIGN_LEFT = 0,
    ALIGN_RIGHT = 1,
    ALIGN_CENTER = 2,
  };

  explicit idDeviceContext(idRenderModelGui* guiModel);

  void SetFont(const idFont* font);
  void DrawStretchPic(float x, float y, float width, float height,
                      float s1, float t1, float s2, float t2,
                      const idMaterial* shader, float angle = 0.0f,
                      alignment_t align = ALIGN_LEFT);
  int TextWidth(float scale, const char* text, int limit = 0) const;
  int TextHeight(float scale) const;
  void DrawWindingMaterial(idWinding2D* winding, const idMaterial* material,
                           const idVec4& color);
  void DrawFilledRect(float x, float y, float width, float height,
                      const idVec4& color);
  void DrawRect(float x, float y, float width, float height, float size,
                const idVec4& color);
  int DrawTextA(float x, float y, float scale, const idVec4& color,
                const char* text);
  int DrawShadowText(float x, float y, float scale, const idVec4& color,
                     const char* text);
  void DrawLine(const idVec2& start, const idVec2& end, float width,
                const idVec4& color);

  idDrawVert stretchPicVerts[32];
  std::uint16_t stretchPicIndices[90];
  idRenderModelGui* gui;
  const idMaterial* whiteMaterial;
  const idFont* activeFont;
  idVec2 uiScale;
  idVec2 uiOffset;
  idMat3 axis;
  idVec3 origin;
  bool identityMatrix;
};

static_assert(sizeof(idWinding2D) == 516, "idWinding2D ABI drift");
static_assert(sizeof(idDeviceContext) == 1312, "idDeviceContext ABI drift");
