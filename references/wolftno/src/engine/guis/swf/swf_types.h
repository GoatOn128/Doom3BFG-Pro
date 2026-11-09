#pragma once

#include "shared/idlib/math/vector.h"

#include <cstdint>

struct swfRect_t {
  swfRect_t() : tl{0.0f, 0.0f}, br{0.0f, 0.0f} {}
  idVec2 tl;
  idVec2 br;
};
struct swfMatrix_t {
  swfMatrix_t() : xx(1), yy(1), xy(0), yx(0), tx(0), ty(0) {}
  float xx, yy, xy, yx, tx, ty;
  idVec2 Scale(const idVec2& in) const {
    return {(in.x * xx) + (in.y * xy), (in.y * yy) + (in.x * yx)};
  }
  idVec2 Transform(const idVec2& in) const {
    const idVec2 scaled = Scale(in);
    return {scaled.x + tx, scaled.y + ty};
  }
  swfMatrix_t Multiply(const swfMatrix_t& other) const {
    swfMatrix_t result;
    result.xx = xx * other.xx + yx * other.xy;
    result.yx = xx * other.yx + yx * other.yy;
    result.xy = xy * other.xx + yy * other.xy;
    result.yy = xy * other.yx + yy * other.yy;
    result.tx = tx * other.xx + ty * other.xy + other.tx;
    result.ty = tx * other.yx + ty * other.yy + other.ty;
    return result;
  }
  swfMatrix_t Inverse() const;
};
struct swfColorRGB_t {
  swfColorRGB_t() : r(255), g(255), b(255) {}
  std::uint8_t r, g, b;
};
struct swfColorRGBA_t : public swfColorRGB_t {
  swfColorRGBA_t() : a(255) {}
  std::uint8_t a;
};
struct swfGradientRecord_t {
  swfGradientRecord_t() : startRatio(0), endRatio(0) {}
  std::uint8_t startRatio;
  std::uint8_t endRatio;
  swfColorRGBA_t startColor;
  swfColorRGBA_t endColor;
};
struct swfGradient_t {
  swfGradient_t() : numGradients(0) {}
  std::uint8_t numGradients;
  swfGradientRecord_t gradientRecords[16];
};
struct swfColorXform_t {
  swfColorXform_t() : mul{1,1,1,1}, add{0,0,0,0} {}
  idVec4 mul;
  idVec4 add;
  swfColorXform_t Multiply(const swfColorXform_t& other) const;
};

class idMaterial;
struct swfRenderState_t {
  swfRenderState_t()
      : matrix(), cxf(), material(nullptr), materialWidth(0), materialHeight(0), activeMasks(0),
        remainderScaleX(0.0f), remainderScaleY(0.0f), z(0.0f), blendMode(0), ratio(0.0f),
        useBlackbars(false) {}
  swfMatrix_t matrix;
  swfColorXform_t cxf;
  const idMaterial* material;
  int materialWidth;
  int materialHeight;
  int activeMasks;
  float remainderScaleX;
  float remainderScaleY;
  float z;
  std::uint8_t blendMode;
  float ratio;
  bool useBlackbars;
};

class idSWFSpriteInstance;
class idSWFTextInstance;

struct swfDisplayEntry_t {
  swfDisplayEntry_t()
      : characterID(0), depth(0), clipDepth(0), blendMode(0), matrix(), cxf(), ratio(0.0f),
        spriteInstance(nullptr), textInstance(nullptr) {}
  std::uint16_t characterID;
  std::uint16_t depth;
  std::uint16_t clipDepth;
  std::uint16_t blendMode;
  swfMatrix_t matrix;
  swfColorXform_t cxf;
  float ratio;
  idSWFSpriteInstance* spriteInstance;
  idSWFTextInstance* textInstance;
};

struct swfLineStyle_t {
  swfLineStyle_t() : startWidth(0), endWidth(0), startColor(), endColor() {}
  std::uint16_t startWidth;
  std::uint16_t endWidth;
  swfColorRGBA_t startColor;
  swfColorRGBA_t endColor;
};

struct swfFillStyle_t {
  swfFillStyle_t()
      : type(0), subType(0), startColor(), endColor(), startMatrix(), endMatrix(), gradient(),
        focalPoint(0.0f), bitmapID(0) {}
  std::uint8_t type;
  std::uint8_t subType;
  swfColorRGBA_t startColor;
  swfColorRGBA_t endColor;
  swfMatrix_t startMatrix;
  swfMatrix_t endMatrix;
  swfGradient_t gradient;
  float focalPoint;
  std::uint16_t bitmapID;
};

static_assert(sizeof(swfRect_t) == 16, "swfRect_t ABI drift");
static_assert(sizeof(swfMatrix_t) == 24, "swfMatrix_t ABI drift");
static_assert(sizeof(swfColorRGB_t) == 3, "swfColorRGB_t ABI drift");
static_assert(sizeof(swfColorRGBA_t) == 4, "swfColorRGBA_t ABI drift");
static_assert(sizeof(swfGradientRecord_t) == 10, "swfGradientRecord_t ABI drift");
static_assert(sizeof(swfGradient_t) == 161, "swfGradient_t ABI drift");
static_assert(sizeof(swfColorXform_t) == 32, "swfColorXform_t ABI drift");
static_assert(sizeof(swfRenderState_t) == 104, "swfRenderState_t ABI drift");
static_assert(sizeof(swfDisplayEntry_t) == 88, "swfDisplayEntry_t ABI drift");
static_assert(sizeof(swfLineStyle_t) == 12, "swfLineStyle_t ABI drift");
static_assert(sizeof(swfFillStyle_t) == 232, "swfFillStyle_t ABI drift");
